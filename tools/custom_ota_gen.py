#!/usr/bin/env python
#
# Copyright 2022 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import sys
import struct
import argparse
import binascii
import hashlib
import os
import shutil
from Crypto.Cipher import AES
import subprocess
import json
import lzma

# src_file = 'hello-world.bin'
# compressed_file = 'hello-world.bin.xz'
# pcaked_compressed_file = 'hello-world.bin.xz.packed'
# siged_pcaked_compressed_file = 'hello-world.bin.xz.packed.signed'

binary_compress_type = {'none': 0, 'xz':1}
binary_delta_type = {'none': 0, 'ddelta':1}
binary_encryption_type = {'none': 0, 'aes128':1}
header_version = {'v1': 1, 'v2':2}

AES_BLOCK_SIZE = AES.block_size     # AES data block length
AES128_KEY_SIZE = 16
# when diff ota is used, add crc32_checksum to the header to ensure the old app(or called 'base app') is the one matches the ptach. 
# At present, we calculate the checksum of the first 4KB data of the old app.                
OLD_APP_CHECK_DATA_SIZE = 4096      

# |------|---------|---------|----------|------------|------------|-----------|----------|---------------|------------|--------------|--------------|--------------|------------|
# |      | Magic   | header  | Compress | delta      | Encryption | Reserved  | Firmware | The length of | The MD5 of | old app check| The CRC32 for| The CRC32 for| compressed |
# |      | number  | version | type     | type       | type       |           | version  | compressed bin|    bin     | data len     | old app data | the header   | firmware   |
# |------|---------|---------|----------|------------|------------|-----------|----------|---------------|------------|--------------|--------------|--------------|------------|
# | Size | 4 bytes | 1 byte  | 4 bits	| 4 bits     | 1 bytes	  | 1 bytes   | 32 bytes |  4 bytes	     | 32 bytes	  |  4 bytes     |  4 bytes     |  4 bytes     |            |
# |------|---------|---------|----------|------------|------------|-----------|----------|---------------|------------|--------------|--------------|--------------|------------|
# | Data | String  | little- | little-  | little-    | little-    |           | String   | little-endian |   String   |little-endian | little-endian| little-endian|            |
# | type | ended   | endian  | endian   | endian     | endian     |           | ended    |   integer     |   ended    |integer       |    integer   |    integer   |            |
# |      |with ‘\0’| integer | integer	| integer    | integer 	  |           | with ‘\0’|               |   with ‘\0’|              |              |              | Binary data|
# |------|---------|---------|----------|------------|------------|-----------|----------|---------------|------------|--------------|--------------|--------------|------------|
# | Data | “ESP”   |    2    |   0/1/2  | 0/1/2      |	0/1       |           |          |               |            |              |              |              |            |
# |------|---------|---------|----------|------------|------------|-----------|----------|---------------|------------|--------------|--------------|--------------|------------|

def xz_compress(store_directory, in_file):
    compressed_file = ''.join([in_file,'.xz'])
    if(os.path.exists(compressed_file)):
        os.remove(compressed_file)

    xz_compressor_filter = [
        {"id": lzma.FILTER_LZMA2, "preset": 6, "dict_size": 64*1024},
    ]
    with open(in_file, 'rb') as src_f:
        data = src_f.read()
        with lzma.open(compressed_file, "wb", format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=xz_compressor_filter) as f:
            f.write(data)
            f.close()
    
    if not os.path.exists(os.path.join(store_directory, os.path.split(compressed_file)[1])):
        shutil.copy(compressed_file, store_directory)
        print('copy xz file done')

def secure_boot_sign(sign_key, in_file, out_file):
    ret = os.system('espsecure.py sign_data --version 2 --keyfile {} --output {} {}'.format(sign_key, out_file, in_file))
    if ret:
        raise Exception('sign failed')

# PKCS5padding
def aes_data_pad(bytes):
    block_size = AES.block_size
    pad_size = block_size - len(bytes) % block_size
    if pad_size == 0:
        pad_size = block_size
    pad = chr(pad_size)
    print('pad_size: {}, pad char: {}'.format(pad_size, pad.encode()))
    while pad_size != 0:     
        bytes += pad.encode()
        pad_size -= 1

    return bytes

def get_aes_key_from_file(keyfile):
    key = None
    with open(keyfile, 'rb') as pubkey_f:
        bytes = pubkey_f.read()
        print('keyfile_length：{}'.format(len(bytes)))
        key = bytes[:16]                               # AES128 key len is 16 bytes
        print('aes128 pubkey：{}'.format(len(key)))
    
    return key

def aes_encrypt(key_str, bytes, out_file):
    myCipher = AES.new(key_str, AES.MODE_ECB)
    encryptData = myCipher.encrypt(bytes)
    print('encryed data len：{}'.format(len(encryptData)))
    with open(out_file, 'wb') as f:
        write_bytes = f.write(encryptData)
        print('encryed file len：{}'.format(write_bytes))
        f.close()

    return write_bytes

def get_app_name():
    with open(os.path.join('flasher_args.json')) as f:
        try:
            flasher_args = json.load(f)
            return flasher_args['app']['file']
        except Exception as e:
            print(e)

    return ''

def update_ddelta_library_path():
    os.environ['LD_LIBRARY_PATH'] = "bootloader_components/tools/lib"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-hv', '--header_ver', nargs='?', choices = ['v1', 'v2'], 
            default='v1', help='the version of the packed file header [default:v1]')
    parser.add_argument('-c', '--compress_type', nargs= '?', choices = ['none', 'xz'], 
            default='xz', help='firmware compressed type [default:xz]')
    parser.add_argument('-d', '--delta_type', nargs= '?', choices = ['none', 'ddelta'], 
            default='none', help='firmware delta type [default:none]')
    parser.add_argument('-e', '--encryption_type', nargs= '?', choices = ['none', 'aes128'], 
            default='none', help='the encryption type for the compressed firmware [default:none]')
    parser.add_argument('-b', '--base_file', nargs = '?', 
            default='', help='the base firmware(only needed for delta compress type)')
    parser.add_argument('-i', '--in_file', nargs = '?', 
            default='', help='the new firmware')
    parser.add_argument('--sign_key', nargs = '?', 
            default='', help='the sign key used for secure boot')
    parser.add_argument('--encry_key', nargs = '?', 
            default='', help='the encryption key used for encrypting file')
    parser.add_argument('-v', '--ver', nargs='?', default='', help='the version of the compressed firmware')
    
    args = parser.parse_args()

    compress_type = args.compress_type
    delta_type = args.delta_type
    encryption_type = args.encryption_type
    firmware_ver = args.ver
    src_file = args.in_file
    sign_key = args.sign_key
    encry_key = args.encry_key
    base_file = args.base_file
    header_ver = args.header_ver

    if src_file == '':
        origin_app_name = get_app_name()
        if(origin_app_name == ''):
            print('get origin app name fail')
            return
        
        if os.path.exists(os.path.join(origin_app_name)):
            src_file = os.path.join(origin_app_name)
        else:
            print('origin app.bin not found')
            return
    
    print('src file is: {}'.format(src_file))

    # rebuild the cpmpressed_app directroy
    cpmoressed_app_directory = os.path.join('custom_ota_binaries')
    if os.path.exists(cpmoressed_app_directory):
        shutil.rmtree(cpmoressed_app_directory)
    
    os.mkdir(cpmoressed_app_directory)
    print('The compressed file will store in {}'.format(cpmoressed_app_directory))

    if header_ver == 'v2' :
        # if the compress type is 'ddelta', we need to generate the uncompressed patch file, and then to compress the uncompressed patch file
        if delta_type == 'ddelta':
            update_ddelta_library_path()
            uncompressed_patch = os.path.join(cpmoressed_app_directory, 'patch')
            ret = subprocess.call('bootloader_components/tools/ddelta_generate {0} {1} {2}'.format(base_file, src_file, uncompressed_patch), shell = True)
            # print('xz compress cmd return: {}'.format(ret))
            if ret:
                raise Exception("ddelta_gen cmd failed")
            src_file = uncompressed_patch
            print('to gen compressed patch')

    #step1: compress
    if compress_type == 'xz':
        xz_compress(cpmoressed_app_directory, os.path.abspath(src_file))

        origin_app_name = os.path.split(src_file)[1]
        compressed_file_name = ''.join([origin_app_name, '.xz'])
        compressed_file = os.path.join(cpmoressed_app_directory, compressed_file_name)
    else:
        compressed_file = ''.join(src_file)
    
    print('compressed file is: {}'.format(compressed_file))

    #step2: packet the compressed image header
    packed_file = ''.join([compressed_file,'.packed'])
    with open(compressed_file, 'rb') as src_f:
        data = src_f.read()
        f_len = src_f.tell()
        # magic number
        bin_data = struct.pack('4s', b'ESP')
        # header version
        bin_data += struct.pack('B', header_version[header_ver])
        # Compress type
        bin_data += struct.pack('B', (binary_delta_type[delta_type] << 4) | binary_compress_type[compress_type])
        print('compressed type: {}'.format((binary_delta_type[delta_type] << 4) | binary_compress_type[compress_type]))
        # Encryption type
        if encry_key != '':
            bin_data += struct.pack('B', binary_encryption_type[encryption_type])
        else:
            bin_data += struct.pack('B', binary_encryption_type['none'])

        print('encryption type: {}'.format(binary_encryption_type[encryption_type]))
        # Reserved
        bin_data += struct.pack('?', 0)
        # Firmware version
        bin_data += struct.pack('32s', firmware_ver.encode())
        # The length of the compressed binary
        bin_data += struct.pack('<I', f_len)
        print('compressed app len: {}'.format(f_len))
        # The MD5 for the compressed binary
        bin_data += struct.pack('32s', hashlib.md5(data).digest())
        if header_ver == 'v2' :
            if delta_type != 'none' :
                # The length of the old app data used to generate crc checksum
                bin_data += struct.pack('<I', OLD_APP_CHECK_DATA_SIZE)
                # The crc checksum of the old app data which length is OLD_APP_CHECK_DATA_SIZE
                with open(base_file, 'rb') as base_f:
                    old_app_check_data = base_f.read(OLD_APP_CHECK_DATA_SIZE)
                    bin_data += struct.pack('<I', binascii.crc32(old_app_check_data, 0x0))
                    print('add crc32 checksum of old app to the header')
            else:
                # Now it is not diff ota
                bin_data += struct.pack('<I', 0)
                bin_data += struct.pack('<I', 0)
        # The CRC32 for the header
        bin_data += struct.pack('<I', binascii.crc32(bin_data, 0x0))
        
        # compressed firmware
        bin_data += data
        with open(packed_file, 'wb') as dst_f:
            dst_f.write(bin_data)
    
    print('packed file is: {}'.format(packed_file))

    #step3: if need sign, then sign the packed image
    if sign_key != '':
        signed_file = ''.join([packed_file,'.signed'])
        secure_boot_sign(sign_key, packed_file, signed_file)
        print('signed_file is: {}'.format(signed_file))
    else:
        signed_file = ''.join(packed_file)
    
    #step4: if need encryptin, then aes encryption
    if encry_key != '':
        encryed_file = ''.join([signed_file, '.encryed'])
        key_str = get_aes_key_from_file(encry_key)
        if key_str is not None:
            with open(signed_file, 'rb') as encry_f:
                origin_bytes = encry_f.read()
                print('signed_file_length：{}'.format(len(origin_bytes)))
                pad_bytes = aes_data_pad(origin_bytes)
                print('padded_file_length：{}'.format(len(pad_bytes)))
                if(binary_encryption_type[encryption_type] == 1):
                    aes_encrypt(key_str, pad_bytes, encryed_file)
                print('encryed file is: {}'.format(encryed_file))

if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(e)
        sys.exit(2)
