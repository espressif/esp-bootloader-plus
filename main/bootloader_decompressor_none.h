/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bootloader_custom_ota.h"

int bootloader_decompressor_none_init(bootloader_custom_ota_params_t *params);

int bootloader_decompressor_none_input(void *addr, int size);

#ifdef __cplusplus
}
#endif