/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bootloader_decompressor_none.h"

int bootloader_decompressor_none_init(bootloader_custom_ota_params_t *params)
{
    return 0;
}

int bootloader_decompressor_none_input(void *addr, int size)
{
    return size;
}