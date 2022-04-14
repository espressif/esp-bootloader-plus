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

#include "esp_flash_partitions.h"
#include "bootloader_utility.h"

#ifdef __cplusplus
extern "C" {
#endif

bool bootloader_custom_utility_search_partition_pos(uint8_t type, uint8_t subtype, esp_partition_pos_t *pos);

esp_err_t bootloader_custom_utility_updata_ota_data(const bootloader_state_t *bs, int boot_index);

#ifdef __cplusplus
}
#endif