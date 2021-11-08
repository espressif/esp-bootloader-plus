/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bootloader_flash_priv.h"

static const char *TAG = "boot_flash";

static bootloader_custom_ota_config_t *custom_ota_config;

/* TODO: We may need to merge the methods of storing flash data. */
int bootloader_storage_flash_diff_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_config = params->config;

    return 0;
}

int bootloader_storage_flash_diff_input(void *buf, int size)
{
    return size;
}
