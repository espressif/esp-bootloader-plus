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

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_rom_md5.h"
#include "esp_rom_crc.h"

#include "bootloader_flash_priv.h"

#include "bootloader_custom_ota.h"

#include "bootloader_custom_utility.h"

#include "bootloader_decompressor_none.h"
#include "bootloader_diff_none.h"

#ifdef CONFIG_BOOTLOADER_DECOMPRESSOR_XZ
#include "bootloader_decompressor_xz.h"
#endif

#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA 
#include "bootloader_diff_ddelta.h"
#endif

#include "bootloader_storage_flash.h"

#ifdef CONFIG_SECURE_BOOT_V2_ENABLED
#include "esp_secure_boot.h"
#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#endif

static const char *TAG = "bootloader_custom";

static const bootloader_custom_ota_engine_t decompressor_engines[] = {
    {COMPRESS_NONE, bootloader_decompressor_none_init,   bootloader_decompressor_none_input},
#ifdef CONFIG_BOOTLOADER_DECOMPRESSOR_XZ
    {COMPRESS_XZ, bootloader_decompressor_xz_init,     bootloader_decompressor_xz_input},
#endif
};

static const bootloader_custom_ota_engine_t diff_engines[] = {
    {DIFF_NONE, bootloader_diff_none_init,     bootloader_diff_none_input},
#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA
    {DIFF_DDELTA, bootloader_diff_ddelta_init,   bootloader_diff_ddelta_input},
#endif
};

static const bootloader_custom_ota_engine_t storage_engines[] = {
    {STORAGE_FLASH, bootloader_storage_flash_init, bootloader_storage_flash_input},
};

static bootloader_custom_ota_config_t custom_ota_config;
static bootloader_custom_ota_header_t custom_ota_header;
static const bootloader_custom_ota_engine_t * custom_ota_engines[MAX_ENGINE];

static bootloader_custom_ota_params_t custom_ota_params;

uint32_t bootloader_custom_ota_get_header_size(bootloader_custom_ota_header_t *header)
{
    uint32_t header_size = 0;
    switch (header->header.version) {
        case 1:
        header_size = sizeof(bootloader_custom_ota_header_v1_t);
        break;
        case 2:
        header_size = sizeof(bootloader_custom_ota_header_v2_t);
        break;
        default:
        break;
    }

    return header_size;
}

uint32_t bootloader_custom_ota_get_header_crc(bootloader_custom_ota_header_t *header)
{
    uint32_t header_crc = 0;
    switch (header->header.version) {
        case 1:
        header_crc = header->header_v1.crc32;
        break;
        case 2:
        header_crc = header->header_v2.crc32;
        break;
        default:
        break;
    }

    return header_crc;
}

static const bootloader_custom_ota_engine_t *bootloader_custom_ota_engine_get(bootloader_custom_ota_engine_type_t engine_type,
    const bootloader_custom_ota_engine_t *engines, uint8_t engines_num)
{
    const bootloader_custom_ota_engine_t *engine = NULL;

    for (uint32_t loop = 0; loop < engines_num; loop++) {
        if (engine_type == engines[loop].type) {
            engine = &engines[loop];
            break;
        }
    }

    return engine;
}

static bool bootloader_custom_ota_engines_init(bootloader_custom_ota_params_t *params)
{
    // Prepare engines
    custom_ota_engines[DECOMPRESS_ENGINE] = bootloader_custom_ota_engine_get(custom_ota_header.header.compress_type,
        decompressor_engines, sizeof(decompressor_engines) / sizeof(decompressor_engines[0]));

    if (!custom_ota_engines[DECOMPRESS_ENGINE]) {
        ESP_LOGE(TAG, "No supported decompressor %d type!", custom_ota_header.header.compress_type);
        return false;
    }

    custom_ota_engines[DIFF_ENGINE] = bootloader_custom_ota_engine_get(custom_ota_header.header.delta_type,
        diff_engines, sizeof(diff_engines) / sizeof(diff_engines[0]));

    if (!custom_ota_engines[DIFF_ENGINE]) {
        ESP_LOGE(TAG, "No supported diff %d type!", custom_ota_header.header.delta_type);
        return false;
    }

    // Read compressed ota header, if it's diff compressed ota, storage_engin is STORAGE_FLASH_DIFF. 
    custom_ota_engines[STORAGE_ENGINE] = &storage_engines[STORAGE_FLASH];

    for (uint32_t loop = 0; loop < MAX_ENGINE; loop++) {
        if (custom_ota_engines[loop]->init(params) != 0) {
            ESP_LOGE(TAG, "engin %u init fail!", loop);
            return false;
        }
    }

    return true;
}

static int bootloader_custom_ota_engines_start()
{
    /*
    * Decompress engine return 0 if no errors; When an error occurs, it returns -1 if the original firmware is not damaged,
    * and returns -2 if the original firmware is damaged.
    * TODO: Unify the parameters used by the function.
    */
    return custom_ota_engines[DECOMPRESS_ENGINE]->input(NULL, 0);
}

static bool bootloader_custom_ota_header_check()
{
    uint32_t header_crc = 0;
    uint32_t bootloader_custom_ota_header_length = 0;
    /*
    * Read sizeof(compress_header) bytes data to check whether it is a custom OTA app.bin(include diff compressed OTA or compressed only OTA).
    * The custom OTA app.bin will have a custom header, where the magic string is BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC.
    */
    if (bootloader_flash_read(custom_ota_config.src_addr, &custom_ota_header, sizeof(custom_ota_header), true) != ESP_OK) {
        ESP_LOGD(TAG, "read ota header err!");
        return false;
    }

    if (strcmp((char *)custom_ota_header.header.magic, BOOTLOADER_CUSTOM_OTA_HEADER_MAGIC)) {
        return false;
    }

    // check OTA version
    if (custom_ota_header.header.version > BOOTLOADER_CUSTOM_OTA_HEADER_VERSION) {
        ESP_LOGW(TAG, "custom OTA version check error!");
        return false;
    }

    header_crc = bootloader_custom_ota_get_header_crc(&custom_ota_header);

    bootloader_custom_ota_header_length = bootloader_custom_ota_get_header_size(&custom_ota_header);
    
    // check CRC32
    if (esp_rom_crc32_le(0, (const uint8_t *)&custom_ota_header, bootloader_custom_ota_header_length - sizeof(custom_ota_header.header_v2.crc32)) != header_crc) {
        ESP_LOGW(TAG, "custom OTA CRC32 check error!");
        return false;
    }
    
    return true;
}

#if CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME && CONFIG_SECURE_BOOT_V2_ENABLED
static bool bootloader_custom_ota_verify_signature()
{
    uint32_t bootloader_custom_ota_header_length = bootloader_custom_ota_get_header_size(&custom_ota_header);
    uint32_t packed_image_len = ALIGN_UP((custom_ota_header.header.length + bootloader_custom_ota_header_length), FLASH_SECTOR_SIZE);
#ifdef	CONFIG_BOOTLOADER_CUSTOM_DEBUG_ON
    ESP_LOGI(TAG, "sign_length is %0xu, src_addr is %0xu, packed_image_len is %0xu", custom_ota_header.header.length, custom_ota_config.src_addr, packed_image_len);
#endif
    //check signature
    esp_err_t verify_err = esp_secure_boot_verify_signature(custom_ota_config.src_addr, packed_image_len);
    if (verify_err != ESP_OK) {
        ESP_LOGE(TAG, "verify signature fail, err:%0xu", verify_err);
        return false;
    }

    ESP_LOGI(TAG, "verify signature success when decompressor");
    return true;
}
#endif

static bool bootloader_custom_ota_md5_check()
{
#define MD5_BUF_SIZE    (1024 * 4)

    // check MD5
    md5_context_t md5_context;
    uint8_t digest[16];
    uint8_t data[MD5_BUF_SIZE];
    uint32_t len = 0;
    uint32_t bootloader_custom_ota_header_length = bootloader_custom_ota_get_header_size(&custom_ota_header);

    esp_rom_md5_init(&md5_context);
    for (uint32_t loop = 0; loop < custom_ota_header.header.length;) {
        len = (custom_ota_header.header.length - loop > MD5_BUF_SIZE) ? MD5_BUF_SIZE : (custom_ota_header.header.length - loop);
        bootloader_flash_read(custom_ota_config.src_addr + bootloader_custom_ota_header_length + loop, data, len, true);
        esp_rom_md5_update(&md5_context, data, len);
        loop += len;
    }
    esp_rom_md5_final(digest, &md5_context);

    if (memcmp(custom_ota_header.header.md5, digest, sizeof(digest))) {
        ESP_LOGE(TAG, "custom OTA MD5 check error!");
        return false;
    }

    return true;
}

#ifdef CONFIG_BOOTLOADER_DIFF_ENABLED
static bool bootloader_custom_ota_base_app_check()
{
#define BASE_APP_CHECK_DATA_SIZE    (1024 * 4)
    uint8_t data[BASE_APP_CHECK_DATA_SIZE];
    uint32_t crc32_ret = 0;
    size_t used_len = 0;
    bootloader_custom_ota_header_v2_t* header = &custom_ota_header.header_v2;

    for (uint32_t loop = 0; loop < header->base_len_for_crc32;) {
        used_len = (header->base_len_for_crc32 - loop > BASE_APP_CHECK_DATA_SIZE) ? BASE_APP_CHECK_DATA_SIZE : (header->base_len_for_crc32 - loop);
        bootloader_flash_read(custom_ota_config.base_addr+loop, data, used_len, true);
        crc32_ret = esp_rom_crc32_le(crc32_ret, (const uint8_t *)&data[0], used_len);
        loop += used_len;
    }

    if (crc32_ret != header->base_crc32) {
        ESP_LOGW(TAG, "base app check error!");
        return false;
    }
    return true;
}
#endif

static esp_err_t bootloader_custom_ota_clear_storage_header()
{
#define SEC_SIZE    (1024 * 4)
    return bootloader_flash_erase_range(custom_ota_config.src_addr, SEC_SIZE);
}

int bootloader_custom_ota_main(bootloader_state_t *boot_status, int boot_index)
{
    int ota_index;
    esp_partition_pos_t pos;

    if (boot_index <= FACTORY_INDEX) {
        ota_index = 0;
    } else {
        ota_index = (boot_index + 1) == boot_status->app_count ? 0 : (boot_index + 1);
    }

    if (!bootloader_custom_utility_search_partition_pos(PART_TYPE_DATA, BOOTLOADER_CUSTOM_OTA_PARTITION_SUBTYPE, &pos)) {
        ESP_LOGE(TAG, "no custom OTA storage partition!");
        return boot_index;
    }

    custom_ota_config.src_addr = pos.offset;
    custom_ota_config.src_size = pos.size;
    custom_ota_config.src_offset = 0;

    custom_ota_config.dst_addr = boot_status->ota[ota_index].offset;
    custom_ota_config.dst_size = boot_status->ota[ota_index].size;
    custom_ota_config.dst_offset = 0;
#ifdef CONFIG_BOOTLOADER_DIFF_ENABLED
    custom_ota_config.base_addr = boot_status->ota[boot_index].offset;
    custom_ota_config.base_size = boot_status->ota[boot_index].size;
    custom_ota_config.base_offset = 0;
    ESP_LOGD(TAG, "base_addr = %08x, base_size: %08x", custom_ota_config.base_addr, custom_ota_config.base_size);
#endif

    if (!bootloader_custom_ota_header_check()) {
        return boot_index;
    }

    if (!bootloader_custom_ota_md5_check()) {
        goto exit;
    }

#ifdef CONFIG_BOOTLOADER_DIFF_ENABLED
    // when diff ota is used, check the base app, because if base app is error, we can not get the correct new app
    if (custom_ota_header.header_v2.header_common.delta_type && (!bootloader_custom_ota_base_app_check())) {
        goto exit;
    } 
#endif

#if CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME && CONFIG_SECURE_BOOT_V2_ENABLED
    if (!bootloader_custom_ota_verify_signature()) {
        goto exit;
    }
#endif
    ESP_LOGI(TAG, "boot from slot %d, OTA to slot: %d", boot_index, ota_index);

    custom_ota_params.config = &custom_ota_config;
    custom_ota_params.header = &custom_ota_header;
    custom_ota_params.engines = (void **)custom_ota_engines;

    if (!bootloader_custom_ota_engines_init(&custom_ota_params)) {
        return boot_index;
    }

    ESP_LOGI(TAG, "start to erase OTA slot %d", ota_index);
    bootloader_flash_erase_range(custom_ota_config.dst_addr, custom_ota_config.dst_size);

    int ret = bootloader_custom_ota_engines_start();

    if (ret < 0) {
        ESP_LOGE(TAG, "OTA failed!");
        return boot_index;
    }

    bootloader_custom_utility_updata_ota_data(boot_status, ota_index);
    boot_index = ota_index;

exit:
    // Erase custom OTA binary header to avoid doing OTA again in the next reboot.
    if(bootloader_custom_ota_clear_storage_header() != ESP_OK) {
        ESP_LOGE(TAG,"erase compressed OTA header error!");
    }
    return boot_index;
}