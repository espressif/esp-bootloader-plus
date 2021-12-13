# esp bootloader plus

## Overview

* [中文版](./README_CN.md)

The [esp bootloader plus](https://github.com/espressif/esp-bootloader-plus) is an enhanced bootloader based on [ESP-IDF](https://github.com/espressif/esp-idf) [custom bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader). It supports "decompression" or "decompression + patch" of the firmware with "compression" or "diff + compression" in the bootloader stage to upgrade the original firmware.  

By using esp bootloader plus, you can easily use the following three OTA upgrade methods:  

- Full upgrade: OTA server sends the `new_app.bin` to ESP device, and the ESP device applies the `new_app.bin` directly to complete the firmware upgrade.  
- Compression upgrade: OTA server sends the `compressed_new_app.bin` to ESP device. The ESP device decompresses the received `compressed_new_app.bin` in bootloader, and then apply the decompressed firmware to complete the firmware upgrade.  
- Diff compression upgrade (aka incremental upgrade or delta upgrade): calculates the difference between the new firmware and the original firmware, and generates a `patch.bin`. ESP device downloads the `patch.bin` from OTA server, then applies the `patch.bin` and the local original firmware to get the new firmware, completes the firmware upgrade.  

Note: Each of the above upgrade methods is downward compatible. Compression upgrade is compatible with full upgrade; Diff compression upgrade is compatible with compression upgrade and full upgrade.  

## Advantages

The advantages of compression upgrade and diff compression upgrade include:  

- Save flash space. The size of compressed firmware is smaller, therefore it requires less storage space.  
- Save transmission time and improve upgrade success rate. With small firmware size, compression upgrade and differential compression upgrade can shorten the time required to transmit firmware. In this case, it can also improve the upgrade success rate for devices with limited bandwidth or poor network environment.   
- Save OTA power consumption. Our test shows that during an OTA process, the power consumption used to transmit firmware accounts for 80% of the entire power consumption for OTA. Therefore, the smaller firmware size, the less power consumption.  
- Save transmission traffic. The size of compressed firmware or patch file is smaller than the original firmware, so the traffic required for transmission is smaller. If your product is charged by data traffic, then this method can save your maintenance costs.  

## How to Use esp bootloader plus

1. To get esp bootloader plus, you need to navigate to your project directory and clone the repository with `git clone`:  

```
git clone https://github.com/espressif/esp-bootloader-plus.git bootloader_components
```
Example: After cloning the esp bootloader plus in the `hello_world` project, the contents of the `hello_world` directory are as follows:  

```
bootloader_components        — Bootloader directory      
  + esp-xz                   - Esp xz decompress lib
  + main                     - Main source files of the bootloader
main 				         — Main source files of the application 
server_certs                 — The directory used to store certificate
Makefile/CMakeLists.txt    — Makefiles of the application project
```
2. Run the command `idf.py menuconfig `to configure the project.  
3. Run the command `idf.py flash monitor ` to program the target and monitor the output.  

## How to Generate Compressed Firmware and Patch Files

The `custom_ota_gen.py` in the tools directory can be used to generate compressed firmware or patch files.  

The following packages need to be installed before using `custom_ota_gen.py`. You can use the following command to install the required packages:  

```
python -m pip install pycrypto
```

Example 1：  

```
custom_ota_gen.py -i build/hello-world.bin
```
Run the above command in your project directory will generate the `custom_ota_binaries/hello-world.bin.xz.packed` in the current directory. It is the new compressed firmware after compressing `hello-world.bin`. The suffix `. xz` indicates that it uses `esp-xz` compression algorithm. The suffix `.packed` indicates that it has added a compressed header.  

Example 2:  
```
custom_ota_gen.py -i hello-world.bin --sign_key secure_boot_signing_key.pem
```
Run the above command in your project directory will generate the  `custom_ota_binaries/hello-world.bin.xz.packed.signed` in the current directory. It is the compressed firmware and signed after compression. If [Secure Boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) is used, you can use `--sign_key` option specifies the signature key to sign the compressed firmware, otherwise the ESP device will consider the compressed firmware as illegal data, resulting in upgrade failure.  

Note: To append the image signature to the existing binary, `espsecure.py` needs to be installed. You can refer to [ESP-IDF-Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html) to install the tool.   

## How to Test
You can use  [hello_world](https://github.com/espressif/esp-idf/tree/master/examples/get-started/hello_world) project to test the compression upgrade:  

1）Navigate to `hello_world` directory and run command `https://github.com/espressif/esp-bootloader-plus.git bootloader_components` to clone esp bootloader plus.  
2）Create a new partition file `partitions_example.csv` in the `hello_world` directory as the following table:  

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1216k,
storage,  data, 0x22,      ,        640k,
```
3）Execute command `idf.py set-target esp32c3` in the `hello_world` directory.  

4）Execute command `idf.py menuconfig` in the `hello_world` directory, and set the configuration of partition table to be the new created `partitions_example.csv` in step 2.   

5）Run command `idf.py flash monitor` to compile and flash the new firmware. The log will be as:  

```
I (978) cpu_start: Starting scheduler.
Hello world!
This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
Minimum free heap size: 329596 bytes
Restarting in 10 seconds...
Restarting in 9 seconds...
```

6）Modify the `printf("Hello world!\n")` in the `app_main()` function of `hello_world` to be `printf("Hello world, Hello esp32!\n")`, and then run command `idf.py build` to build a new firmware.   

7）Run command `custom_ota_gen.py -i build/hello_world.bin`, it will create a new sub-directory `custom_ota_binaries` and generate the new compressed firmware `hello_world.bin.xz.packed` in it.   

8）Run command `esptool.py -p PORT write_flash 0x150000 custom_ota_binaries/hello_world.bin.xz.packed` to burn the new compressed firmware into the ESP device's storage partition. Please note that the parameter `PORT` should be replaced with the actual port of your device.    

9）Run command `idf.py monitor` to check device log, it should be as:   

```
I (285) cpu_start: Starting scheduler.
Hello world, Hello esp32!
This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
Minimum free heap size: 329596 bytes
Restarting in 10 seconds...
Restarting in 9 seconds...
```

## Examples

ToDo 

## Note
1. If your Flash partition has only two partitions, one for storing apps and one for storing compressed firmware, then you cannot rollback to the old version if using compression upgrade. In this case, please ensure the availability and correctness of your compressed firmware before upgrading to it.    

## Release Note  

1. Only the compression upgrade is supported for now, the diff compression upgrade will be released soon.  

2. `esp bootloader plus` is supported with ESP-IDF v4.4 or later versions.  

3. Only ESP32-C3 supports `esp bootloader plus` for now.  


If you have any question in use, please feel free to contact us.

