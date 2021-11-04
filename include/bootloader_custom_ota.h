/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef BOOTLOADER_BUILD
#include "bootloader_utility.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC 		"ESP"

#define BOOTLOADER_CUSTOM_OTA_HEADER_VERSION    1

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
#ifdef CONFIG_BOOTLOADER_DIFF_ENABLED
    STORAGE_FLASH_DIFF,
#endif
} bootloader_custom_ota_storage_type_t;

typedef struct {
    uint8_t  magic[4];
    uint8_t  version;
    uint8_t  compress_type:4;
    uint8_t  delta_type:4;
    uint8_t  encryption_type;
    uint8_t  reserved;
    uint8_t  firmware_version[32];
    uint32_t length;
    uint8_t  md5[32];
    uint32_t crc32;
} bootloader_custom_ota_header_t;

typedef struct {
    uint32_t src_addr;
    uint32_t src_size;
    uint32_t src_offset;

    uint32_t dst_addr;
    uint32_t dst_size;
    uint32_t dst_offset;
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

#ifdef __cplusplus
}
#endif