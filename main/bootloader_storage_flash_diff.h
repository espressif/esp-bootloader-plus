/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bootloader_custom_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

int bootloader_storage_flash_diff_init(bootloader_custom_ota_params_t *params);

int bootloader_storage_flash_diff_input(void *buf, int size);

#ifdef __cplusplus
}
#endif
