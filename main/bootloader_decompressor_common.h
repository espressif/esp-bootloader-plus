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

int bootloader_decompressor_read(void *buf, unsigned int size);

int bootloader_decompressor_write(void *buf, unsigned int size);

int bootloader_decompressor_init(bootloader_custom_ota_params_t *params);

#ifdef __cplusplus
}
#endif