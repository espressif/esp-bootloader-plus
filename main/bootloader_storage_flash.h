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

#include "bootloader_custom_ota.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

int bootloader_storage_flash_init(bootloader_custom_ota_params_t *params);

int bootloader_storage_flash_input(void *buf, int size);

esp_err_t bootloader_storage_flash_read(size_t src_addr, void *dest, size_t size, bool allow_decrypt);

esp_err_t bootloader_storage_flash_write(size_t dest_addr, void *src, size_t size, bool allow_decrypt);

#ifdef __cplusplus
}
#endif