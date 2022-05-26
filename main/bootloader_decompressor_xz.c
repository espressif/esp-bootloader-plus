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

#include "esp_xz_decompressor.h"

#include "bootloader_flash_priv.h"

#include "bootloader_decompressor_xz.h"

#include "bootloader_decompressor_common.h"

static const char *TAG = "boot_xz";

static void bootloader_decompressor_xz_error(char *msg)
{
    ESP_LOGE(TAG, "%s", msg);
}

int bootloader_decompressor_xz_init(bootloader_custom_ota_params_t *params)
{
    return bootloader_decompressor_init(params);
}

int bootloader_decompressor_xz_input(void *addr, int size)
{
    int ret = -1;
    int in_used = 0;

    ret = esp_xz_decompress(NULL, 0, &bootloader_decompressor_read, &bootloader_decompressor_write, NULL, &in_used, &bootloader_decompressor_xz_error);

    if((ret != 0) && (in_used != 0)) {
        return -2;
    }

    return ret;
}
