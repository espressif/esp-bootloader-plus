/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"

#include "esp_flash_encrypt.h"

#include "bootloader_storage_flash.h"

#include "bootloader_flash_priv.h"

static const char *TAG = "boot_flash";

static bootloader_custom_ota_config_t *custom_ota_config;

int bootloader_storage_flash_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_config = params->config;

    return 0;
}

int bootloader_storage_flash_input(void *buf, int size)
{
    bool encryption = false;
    if (custom_ota_config->dst_offset + size < custom_ota_config->dst_size) {
        uint32_t dst_addr = custom_ota_config->dst_addr + custom_ota_config->dst_offset;
        ESP_LOGD(TAG, "write buffer %x to %x, len %d", buf, dst_addr, size);
        if (esp_flash_encryption_enabled()) {
            encryption = true;
        }
        bootloader_flash_write(custom_ota_config->dst_addr + custom_ota_config->dst_offset, buf, size, encryption);
        custom_ota_config->dst_offset += size;
    }

    return size;
}
