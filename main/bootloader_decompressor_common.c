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

#include "esp_log.h"

#include "bootloader_decompressor_common.h"

#include "bootloader_flash_priv.h"

static bootloader_custom_ota_config_t *custom_ota_config;
static bootloader_custom_ota_header_t *custom_ota_header;
static bootloader_custom_ota_engine_t **custom_ota_engines;

static const char *TAG = "boot_com";

int bootloader_decompressor_read(void *buf, unsigned int size)
{
    uint32_t len = (custom_ota_header->header.length - custom_ota_config->src_offset > size) ? \
                   size : custom_ota_header->header.length - custom_ota_config->src_offset;
    uint32_t header_len = bootloader_custom_ota_get_header_size(custom_ota_header);

    if (len > 0) {
        uint32_t src_addr = custom_ota_config->src_addr + header_len + custom_ota_config->src_offset;
        ESP_LOGD(TAG, "read from %x, len %d", src_addr, len);
        bootloader_flash_read(src_addr, buf, len, true);
        custom_ota_config->src_offset += len;
    }

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
    custom_ota_header = (bootloader_custom_ota_header_t *)params->header;
    custom_ota_engines = (bootloader_custom_ota_engine_t **)params->engines;

    return 0;
}