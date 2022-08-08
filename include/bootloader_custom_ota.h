// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef BOOTLOADER_BUILD
#include "bootloader_utility.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC 		"ESP"

#define BOOTLOADER_CUSTOM_OTA_HEADER_VERSION    2

#define BOOTLOADER_CUSTOM_OTA_PARTITION_SUBTYPE 0x22

typedef enum {
    DECOMPRESS_ENGINE,
    DIFF_ENGINE,
    STORAGE_ENGINE,
    MAX_ENGINE
} bootloader_custom_ota_engine_type_t;

typedef enum {
    COMPRESS_NONE,
    COMPRESS_XZ,
} bootloader_custom_ota_compress_type_t;

typedef enum {
    DIFF_NONE,
#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA    
    DIFF_DDELTA,
#endif
} bootloader_custom_ota_diff_type_t;

typedef enum {
    STORAGE_FLASH,
} bootloader_custom_ota_storage_type_t;

typedef struct {
    uint8_t  magic[4];             /*!< The magic num of compressed firmware, default is BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC */
    uint8_t  version;              /*!< Custom ota header version */
    uint8_t  compress_type:4;      /*!< The compression algorithm used by the compression firmware */
    uint8_t  delta_type:4;         /*!< The diff algorithm used to generate original patch file */
    uint8_t  encryption_type;      /*!< The Encryption algorithm for pre-encrypted compressed firmware(or compressed patch) */
    uint8_t  reserved;             /*!< Reserved */
    uint8_t  firmware_version[32]; /*!< The compressed firmware version, it can be same as the new firmware */
    uint32_t length;               /*!< The length of compressed firmware(or compressed patch) */
    uint8_t  md5[32];              /*!< The MD5 of compressed firmware(or compressed patch) */
} __attribute__((packed)) bootloader_custom_ota_header_common_t;

typedef struct {
    bootloader_custom_ota_header_common_t header_common;
    uint32_t crc32;                /*!< The CRC32 of the header */
} bootloader_custom_ota_header_v1_t;

typedef struct {
    bootloader_custom_ota_header_common_t header_common;
    uint32_t base_len_for_crc32;   /*!< The length of data used for CRC check in base_app(have to be 4-byte aligned.) */
    uint32_t base_crc32;           /*!< The CRC32 of the base_app[0~base_len_for_crc32] */
    uint32_t crc32;                /*!< The CRC32 of the header */
} bootloader_custom_ota_header_v2_t;

typedef union {
    bootloader_custom_ota_header_common_t header;
    bootloader_custom_ota_header_v1_t header_v1;
    bootloader_custom_ota_header_v2_t header_v2;
} bootloader_custom_ota_header_t;

typedef struct {
    uint32_t src_addr;
    uint32_t src_size;
    uint32_t src_offset;

    uint32_t dst_addr;
    uint32_t dst_size;
    uint32_t dst_offset;
#ifdef CONFIG_BOOTLOADER_DIFF_ENABLED
    uint32_t base_addr;
    uint32_t base_size;
    uint32_t base_offset;
#endif
} bootloader_custom_ota_config_t;

typedef struct {
    bootloader_custom_ota_config_t *config;
    bootloader_custom_ota_header_t *header;
    void                           **engines;
} bootloader_custom_ota_params_t;

typedef struct {
    int type;
    int (*init)(bootloader_custom_ota_params_t *params);                  // init OK return 0, others return -1, -2, ...
    int (*input)(void *addr, int size); // the input function just recv data and use data, and then put data to next engine; return 0 if no errors.
} bootloader_custom_ota_engine_t;

#ifdef BOOTLOADER_BUILD
int bootloader_custom_ota_main(bootloader_state_t *boot_status, int boot_index);
#endif

// get the custom ota header size. Note: the header size is different in different version.
uint32_t bootloader_custom_ota_get_header_size(bootloader_custom_ota_header_t *header);

uint32_t bootloader_custom_ota_get_header_crc(bootloader_custom_ota_header_t *header);

#ifdef __cplusplus
}
#endif