/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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