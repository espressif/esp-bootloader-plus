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

#include <string.h>

#include "esp_log.h"

#include "esp_flash_encrypt.h"

#include "bootloader_common.h"

#include "bootloader_flash_priv.h"

#include "bootloader_custom_utility.h"

#define FLASH_SECTOR_SIZE                   0x1000

static const char *TAG = "boot_c_uti";

// Read ota_info partition and fill array from two otadata structures.
static esp_err_t read_otadata(const esp_partition_pos_t *ota_info, esp_ota_select_entry_t *two_otadata)
{
    const esp_ota_select_entry_t *ota_select_map;
    if (ota_info->offset == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // partition table has OTA data partition
    if (ota_info->size < 2 * SPI_SEC_SIZE) {
        ESP_LOGE(TAG, "ota_info partition size %d is too small (minimum %d bytes)", ota_info->size, (2 * SPI_SEC_SIZE));
        return ESP_FAIL; // can't proceed
    }

    ESP_LOGD(TAG, "OTA data offset 0x%x", ota_info->offset);
    ota_select_map = bootloader_mmap(ota_info->offset, ota_info->size);
    if (!ota_select_map) {
        ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", ota_info->offset, ota_info->size);
        return ESP_FAIL; // can't proceed
    }

    memcpy(&two_otadata[0], ota_select_map, sizeof(esp_ota_select_entry_t));
    memcpy(&two_otadata[1], (uint8_t *)ota_select_map + SPI_SEC_SIZE, sizeof(esp_ota_select_entry_t));
    bootloader_munmap(ota_select_map);

    return ESP_OK;
}

static esp_err_t write_otadata(esp_ota_select_entry_t *otadata, uint32_t offset, bool write_encrypted)
{
    esp_err_t err = bootloader_flash_erase_sector(offset / FLASH_SECTOR_SIZE);
    if (err == ESP_OK) {
        err = bootloader_flash_write(offset, otadata, sizeof(esp_ota_select_entry_t), write_encrypted);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error in write_otadata operation. err = 0x%x", err);
    }
    return err;
}

static esp_ota_img_states_t set_new_state_otadata(void)
{
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    ESP_LOGD(TAG, "Monitoring the first boot of the app is enabled.");
    return ESP_OTA_IMG_NEW;
#else
    return ESP_OTA_IMG_UNDEFINED;
#endif
}

esp_err_t bootloader_custom_utility_updata_ota_data(const bootloader_state_t *bs, int boot_index)
{
    esp_ota_select_entry_t otadata[2];
    if (read_otadata(&bs->ota_info, otadata) != ESP_OK) {
        return ESP_FAIL;
    }

    uint8_t ota_app_count = bs->app_count;
    if (boot_index >= ota_app_count) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "otadata[0] ota_seq: %u, otadata[1] ota_seq: %u, ota_app_count is %u", otadata[0].ota_seq, otadata[1].ota_seq, ota_app_count);

    //esp32_idf use two sector for store information about which partition is running
    //it defined the two sector as ota data partition,two structure esp_ota_select_entry_t is saved in the two sector
    //named data in first sector as otadata[0], second sector data as otadata[1]
    //e.g.
    //if otadata[0].ota_seq == otadata[1].ota_seq == 0xFFFFFFFF,means ota info partition is in init status
    //so it will boot factory application(if there is),if there's no factory application,it will boot ota[0] application
    //if otadata[0].ota_seq != 0 and otadata[1].ota_seq != 0,it will choose a max seq ,and get value of max_seq%max_ota_app_number
    //and boot a subtype (mask 0x0F) value is (max_seq - 1)%max_ota_app_number,so if want switch to run ota[x],can use next formulas.
    //for example, if otadata[0].ota_seq = 4, otadata[1].ota_seq = 5, and there are 8 ota application,
    //current running is (5-1)%8 = 4,running ota[4],so if we want to switch to run ota[7],
    //we should add otadata[0].ota_seq (is 4) to 4 ,(8-1)%8=7,then it will boot ota[7]
    //if      A=(B - C)%D
    //then    B=(A + C)%D + D*n ,n= (0,1,2...)
    //so current ota app sub type id is x , dest bin subtype is y,total ota app count is n
    //seq will add (x + n*1 + 1 - seq)%n

    int active_otadata = bootloader_common_get_active_otadata(otadata);
    if (active_otadata != -1) {
        uint32_t seq = otadata[active_otadata].ota_seq;
        ESP_LOGD(TAG, "active otadata[%d].ota_seq is %u", active_otadata, otadata[active_otadata].ota_seq);
        uint32_t i = 0;
        while (seq > (boot_index + 1) % ota_app_count + i * ota_app_count) {
            i++;
        }
        int next_otadata = (~active_otadata) & 1; // if 0 -> will be next 1. and if 1 -> will be next 0.
        otadata[next_otadata].ota_seq = (boot_index + 1) % ota_app_count + i * ota_app_count;
        otadata[next_otadata].ota_state = set_new_state_otadata();
        otadata[next_otadata].crc = bootloader_common_ota_select_crc(&otadata[next_otadata]);
        bool write_encrypted = esp_flash_encryption_enabled();
        ESP_LOGD(TAG, "next_otadata is %d and i=%u and rewrite seq is %u", next_otadata, i, otadata[next_otadata].ota_seq);
        return write_otadata(&otadata[next_otadata], bs->ota_info.offset + FLASH_SECTOR_SIZE * next_otadata, write_encrypted);
    } else {
        /* Both OTA slots are invalid, probably because unformatted... */
        int next_otadata = 0;
        otadata[next_otadata].ota_seq = boot_index + 1;
        otadata[next_otadata].ota_state = set_new_state_otadata();
        otadata[next_otadata].crc = bootloader_common_ota_select_crc(&otadata[next_otadata]);
        bool write_encrypted = esp_flash_encryption_enabled();
        ESP_LOGD(TAG, "next_otadata is %d and rewrite seq is %u", next_otadata, otadata[next_otadata].ota_seq);
        return write_otadata(&otadata[next_otadata], bs->ota_info.offset + FLASH_SECTOR_SIZE * next_otadata, write_encrypted);
    }
}

bool bootloader_custom_utility_search_partition_pos(uint8_t type, uint8_t subtype, esp_partition_pos_t *pos)
{
    const esp_partition_info_t *partitions;
    esp_err_t err;
    int num_partitions;
    bool ret = false;

    partitions = bootloader_mmap(ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
    if (!partitions) {
        ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
        return ret;
    }
    ESP_LOGD(TAG, "mapped partition table 0x%x at 0x%x", ESP_PARTITION_TABLE_OFFSET, (intptr_t)partitions);

    err = esp_partition_table_verify(partitions, true, &num_partitions);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to verify partition table");
        return ret;
    }

    for (int i = 0; i < num_partitions; i++) {
        const esp_partition_info_t *partition = &partitions[i];

        if (type == partition->type && subtype == partition->subtype) {
            ESP_LOGD(TAG, "load partition table entry 0x%x", (intptr_t)partition);
            ESP_LOGD(TAG, "find type=%x subtype=%x", partition->type, partition->subtype);
            *pos = partition->pos;
            ret = true;
            break;
        }
    }

    bootloader_munmap(partitions);

    return ret;
}