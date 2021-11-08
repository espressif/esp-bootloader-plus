/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"

#include "bootloader_decompressor_common.h"

#include "bootloader_flash_priv.h"

#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
#include "hal/wdt_hal.h"
static wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};
static bool rtc_wdt_ctx_enabled = false;
#endif

static bootloader_custom_ota_config_t *custom_ota_config;
static bootloader_custom_ota_header_t *custom_ota_header;
static bootloader_custom_ota_engine_t **custom_ota_engines;

static const char *TAG = "boot_com";

int bootloader_decompressor_read(void *buf, unsigned int size)
{
    uint32_t len = (custom_ota_header->length - custom_ota_config->src_offset > size) ? \
                   size : custom_ota_header->length - custom_ota_config->src_offset;

    if (len > 0) {
        uint32_t src_addr = custom_ota_config->src_addr + sizeof(bootloader_custom_ota_header_t) + custom_ota_config->src_offset;
        ESP_LOGD(TAG, "read from %x, len %d", src_addr, len);
        bootloader_flash_read(src_addr, buf, len, true);
        custom_ota_config->src_offset += len;
    }

#ifdef CONFIG_BOOTLOADER_WDT_ENABLE
    rtc_wdt_ctx_enabled = wdt_hal_is_enabled(&rtc_wdt_ctx);
    if (true == rtc_wdt_ctx_enabled) {
        wdt_hal_write_protect_disable(&rtc_wdt_ctx);
        wdt_hal_feed(&rtc_wdt_ctx);
        wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    }
#endif

    return len;
}

int bootloader_decompressor_write(void *buf, unsigned int size)
{
    // write to buffer if necessary
    // Now we should try to run next engine
    ESP_LOGD(TAG, "write buffer %x, len %d", buf, size);

    return custom_ota_engines[DECOMPRESS_ENGINE + 1]->input(buf, size);
}

int bootloader_decompressor_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_config = params->config;
    custom_ota_header = params->header;
    custom_ota_engines = (bootloader_custom_ota_engine_t **)params->engines;

    return 0;
}