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

/* ddelta_apply.c - A sane reimplementation of bspatch
 *
 * Copyright (C) 2017 Julian Andres Klode <jak@debian.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include "esp_log.h"
#include "esp_flash_encrypt.h"

#include "bootloader_diff_ddelta.h"
#include "bootloader_storage_flash.h"
#include "bootloader_custom_malloc.h" // Note, this header is just used to provide malloc() and free() support.

#ifdef CONFIG_BOOTLOADER_DIFF_DDELTA

#define CONFIG_DDELTA_DEBUG_ON (1)
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifdef __GNUC__
typedef unsigned char uchar_vector __attribute__((vector_size(16)));
#else
typedef unsigned char uchar_vector;
#endif

typedef enum {
    ESP_DDELTA_HAVE_NOT_INIT = 0,
    ESP_DDELTA_HAVE_NOT_GET_HEADER,                     // Have not get patch file header
    ESP_DDELTA_HAVE_NOT_GET_ENTRY, 	                    // Have not get entry
    ESP_DDELTA_IN_DIFF_PROCESS,	                        // In diff process
    ESP_DDELTA_IN_COPY_PROCESS,                         // In copy process
    ESP_DDELTA_IN_SKIP_PROCESS,                         // In skip process
    ESP_DDELTA_MAX_STATUS
} esp_ddelta_status;

typedef struct ddelta_handler {
    esp_ddelta_status current_stage;
    struct ddelta_header d_header;
    struct ddelta_entry_header entry;
    uint32_t bytes_written;
    uint32_t current_processed_data_count;                   // this counter used to count the data that has been processed in diff or copy stage.
    uchar_vector *diff_old_buf;
    uchar_vector *diff_patch_buf;
    char *copy_buf;
    int (*read_old)(void *buf, size_t size);  // read data from old file
    int (*seek_old)(int32_t skip_value);                     // skip skip_value bytes from current read position
    int (*write_new)(void *buf, size_t size); // write data to new file
    uint32_t(*get_patch_valid_len)(void);                    // get the current patch data valid len
    int (*read_patch)(uint8_t *buffer, uint32_t len);        // read len bytes patch data to the buffer
} esp_ddelta_handler;

typedef struct {
    uint8_t *current_buf_addr;
    unsigned int current_valid_len; // size of current data can be used in flush buffer
    unsigned int dec_read_pos; // current read position
} esp_decompress_flush_buf;

static bootloader_custom_ota_engine_t **custom_ota_engines;
static esp_ddelta_handler *s_ddelta_handler = NULL;
static esp_decompress_flush_buf flush_buf;
static bootloader_custom_ota_config_t *custom_ota_config;
static const char *TAG = "ddelta";

static uint64_t ddelta_be64toh(uint64_t be64)
{
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(be64);
#elif defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return be64;
#else
    unsigned char *buf = (unsigned char *)&be64;

    return (uint64_t)buf[0] << 56 |
        (uint64_t)buf[1] << 48 |
        (uint64_t)buf[2] << 40 |
        (uint64_t)buf[3] << 32 |
        (uint64_t)buf[4] << 24 |
        (uint64_t)buf[5] << 16 |
        (uint64_t)buf[6] << 8 |
        (uint64_t)buf[7] << 0;
#endif
}

static int64_t ddelta_from_unsigned(uint64_t u)
{
    return u & 0x80000000 ? -(int64_t)~(u - 1) : (int64_t)u;
}

/**
 * @brief Initialize ddelta decoder. Apply for resources and register the required callback functions
 *
 * @return
 *         -  0 on success,
 *         - -1 on init error
 *
 */
int esp_ddelta_init(int (*read_old)(void *buf, size_t size),
    int (*seek_old)(int32_t skip_value),
    int (*write_new)(void *buf, size_t size),
    uint32_t(*get_patch_valid_len)(void),
    int (*read_patch)(uint8_t *buffer, uint32_t len))
{
    if(s_ddelta_handler) {
        return -1;
    }

    s_ddelta_handler = (esp_ddelta_handler *)bootloader_custom_malloc(sizeof(esp_ddelta_handler));
    if (!s_ddelta_handler) {
        ESP_LOGE(TAG, "malloc ddelta_handler err");
        return -1;
    }
    memset(s_ddelta_handler, 0, sizeof(esp_ddelta_handler));

    s_ddelta_handler->diff_old_buf = (uchar_vector *)bootloader_custom_malloc(DDELTA_BLOCK_SIZE);
    if (s_ddelta_handler->diff_old_buf == NULL) {
        goto err;
    }
    s_ddelta_handler->diff_patch_buf = (uchar_vector *)bootloader_custom_malloc(DDELTA_BLOCK_SIZE);
    if (s_ddelta_handler->diff_patch_buf == NULL) {
        goto err;
    }
    s_ddelta_handler->copy_buf = (char *)bootloader_custom_malloc(DDELTA_BLOCK_SIZE);
    if (s_ddelta_handler->copy_buf == NULL) {
        goto err;
    }

    s_ddelta_handler->read_old = read_old;
    s_ddelta_handler->seek_old = seek_old;
    s_ddelta_handler->write_new = write_new;
    s_ddelta_handler->get_patch_valid_len = get_patch_valid_len;
    s_ddelta_handler->read_patch = read_patch;
    s_ddelta_handler->current_stage = ESP_DDELTA_HAVE_NOT_GET_HEADER;

#ifdef CONFIG_DDELTA_DEBUG_ON
    ESP_LOGI(TAG, "ddelta malloc total size:%d", sizeof(esp_ddelta_handler) + DDELTA_BLOCK_SIZE * 3);
#endif
    return 0;
err:
    if (s_ddelta_handler->diff_old_buf != NULL) {
        bootloader_custom_free(s_ddelta_handler->diff_old_buf);
        s_ddelta_handler->diff_old_buf = NULL;
    }
    if (s_ddelta_handler->diff_patch_buf != NULL) {
        bootloader_custom_free(s_ddelta_handler->diff_patch_buf);
        s_ddelta_handler->diff_patch_buf = NULL;
    }
    ESP_LOGE(TAG, "malloc err");
    return -1;
}

/**
 * @brief Free resources requested by ddelta decoder.
 *
 * @return
 *
 */
void esp_ddelta_deinit(void)
{
    if (s_ddelta_handler) {
        if (s_ddelta_handler->diff_old_buf != NULL) {
            bootloader_custom_free(s_ddelta_handler->diff_old_buf);
            s_ddelta_handler->diff_old_buf = NULL;
        }
        if (s_ddelta_handler->diff_patch_buf != NULL) {
            bootloader_custom_free(s_ddelta_handler->diff_patch_buf);
            s_ddelta_handler->diff_patch_buf = NULL;
        }
        if (s_ddelta_handler->copy_buf != NULL) {
            bootloader_custom_free(s_ddelta_handler->copy_buf);
            s_ddelta_handler->copy_buf = NULL;
        }
        bootloader_custom_free(s_ddelta_handler);
        s_ddelta_handler = NULL;
    }
}

static int get_ddelta_header(struct ddelta_header *header)
{
    if (s_ddelta_handler->read_patch((uint8_t *)header, sizeof(struct ddelta_header)) != sizeof(struct ddelta_header)) {
        ESP_LOGE(TAG, "read ddelta header err");
        return -DDELTA_EPATCHIO;
    }

    if (memcmp(DDELTA_MAGIC, header->magic, sizeof(header->magic)) != 0) {
        ESP_LOGE(TAG, "ddelta magic err");
        return -DDELTA_EMAGIC;
    }

    header->new_file_size = ddelta_be64toh(header->new_file_size);

    ESP_LOGD(TAG, "new size: %llu", header->new_file_size);

    return 0;
}

/**
 * @brief
 *
 * @param entry
 * @param current_used_data_len    Data need be used in current flush buf
 * @param current_processed_data_count    Data has been stored to entry object
 * @return
 *         -  0 on success
 */
static int get_entry_header(struct ddelta_entry_header *entry, uint32_t current_used_data_len, uint32_t current_processed_data_count)
{
    ESP_LOGD(TAG, "used data is %u, processed data is %u", current_used_data_len, current_processed_data_count);
    if (s_ddelta_handler->read_patch((uint8_t *)entry + current_processed_data_count, current_used_data_len) != current_used_data_len) {
        return -DDELTA_EPATCHIO;
    }

    return 0;
}

static int esp_apply_diff(uint32_t current_used_data_len)
{
    int diff_old_buffer_size = DDELTA_BLOCK_SIZE;
    unsigned int i;
    uint32_t toread = 0;
    uint32_t items_to_add = 0;
    /* Apply the diff */
    while (current_used_data_len > 0) {
        toread = MIN(diff_old_buffer_size, current_used_data_len);
        items_to_add = MIN(sizeof(uchar_vector) + toread,
            diff_old_buffer_size) /
            sizeof(uchar_vector);

        if (s_ddelta_handler->read_patch((uint8_t *)s_ddelta_handler->diff_patch_buf, toread) < toread) {
            ESP_LOGE(TAG, "read diff err");
            return -DDELTA_EPATCHIO;
        }

        if (s_ddelta_handler->read_old((uint8_t *)s_ddelta_handler->diff_old_buf, toread) < toread) {
            ESP_LOGE(TAG, "read old err");
            return -DDELTA_EOLDIO;
        }

        ESP_LOGD(TAG, "to_read is %u, items_to_read is %u\n", toread, items_to_add);

        for (i = 0; i < items_to_add; i++)
            s_ddelta_handler->diff_old_buf[i] += s_ddelta_handler->diff_patch_buf[i];

        if (s_ddelta_handler->write_new(s_ddelta_handler->diff_old_buf, toread) < toread) {
            ESP_LOGE(TAG, "write new err");
            return -DDELTA_ENEWIO;
        }

        current_used_data_len -= toread;
    }

    return 0;
}

static int esp_apply_copy(uint32_t current_used_data_len)
{
    uint32_t toread = 0;
    while (current_used_data_len > 0) {
        toread = MIN(DDELTA_BLOCK_SIZE, current_used_data_len);

        if (s_ddelta_handler->read_patch((uint8_t *)s_ddelta_handler->copy_buf, toread) < toread) {
            ESP_LOGE(TAG, "read copy data err");
            return -DDELTA_EPATCHIO;
        }

        if (s_ddelta_handler->write_new(s_ddelta_handler->copy_buf, toread) < toread) {
            ESP_LOGE(TAG, "write new err");
            return -DDELTA_ENEWIO;
        }

        current_used_data_len -= toread;
    }
    return 0;
}

/**
 * @brief The main function of ddelta decoder. It will read old(or called 'base') app data and patch data to genetate new app data.
 *
 * @return
 *         -  0 on success, ddetla finish
 *         - -1 on init error
 *         - -2 on need more data
 *         - -3 on fatal error
 *
 */
int esp_ddelta_apply(void)
{
    uint32_t current_valid_data_len; // the data len can be used in buffer
    uint32_t current_used_data_len; // the data len need to be used for current step
    while (1) {
        switch (s_ddelta_handler->current_stage) {
        case ESP_DDELTA_HAVE_NOT_GET_HEADER:
            ESP_LOGD(TAG, "IN GET_HEADER");
            if (s_ddelta_handler->get_patch_valid_len() > sizeof(struct ddelta_header)) {
                if (get_ddelta_header(&s_ddelta_handler->d_header) != 0) {
                    goto err_handler;
                }
            } else {
                return -2;
            }
            s_ddelta_handler->current_stage = ESP_DDELTA_HAVE_NOT_GET_ENTRY;
            s_ddelta_handler->current_processed_data_count = 0;
            break;
        case ESP_DDELTA_HAVE_NOT_GET_ENTRY:
            ESP_LOGD(TAG, "IN GET_ENTRY");
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len, (sizeof(struct ddelta_entry_header) - s_ddelta_handler->current_processed_data_count));

            if (get_entry_header(&s_ddelta_handler->entry, current_used_data_len, s_ddelta_handler->current_processed_data_count) != 0) {
                ESP_LOGE(TAG, "get entry error");
                goto err_handler;
            }

            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count == sizeof(struct ddelta_entry_header)) {
                s_ddelta_handler->entry.diff = ddelta_be64toh(s_ddelta_handler->entry.diff);
                s_ddelta_handler->entry.extra = ddelta_be64toh(s_ddelta_handler->entry.extra);
                s_ddelta_handler->entry.seek.value = ddelta_from_unsigned(ddelta_be64toh(s_ddelta_handler->entry.seek.value));

                // check whether ddelta is finished or not
                if (s_ddelta_handler->entry.diff == 0 && s_ddelta_handler->entry.extra == 0 && s_ddelta_handler->entry.seek.value == 0) {
                    if (s_ddelta_handler->bytes_written == s_ddelta_handler->d_header.new_file_size) {
                        ESP_LOGI(TAG, "ddelta OK");
                        // fflush(newfd);
                        return 0;
                    } else {
                        ESP_LOGE(TAG, "ddelta patch short, bytes_written: %u, new_file_size: %llu", s_ddelta_handler->bytes_written, s_ddelta_handler->d_header.new_file_size);
                        goto err_handler;
                    }
                }

                s_ddelta_handler->current_stage = ESP_DDELTA_IN_DIFF_PROCESS;
                s_ddelta_handler->current_processed_data_count = 0;
            } else {
                return -2;
            }

            break;
        case ESP_DDELTA_IN_DIFF_PROCESS:
            ESP_LOGD(TAG, "IN DIFF_PROCESS");
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len, (s_ddelta_handler->entry.diff - s_ddelta_handler->current_processed_data_count));

            if (esp_apply_diff(current_used_data_len) != 0) {
                ESP_LOGE(TAG, "apply diff error");
                goto err_handler;
            }

            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count != s_ddelta_handler->entry.diff) {
                return -2;
            } else {
                s_ddelta_handler->current_stage = ESP_DDELTA_IN_COPY_PROCESS;
                s_ddelta_handler->current_processed_data_count = 0;
            }
            break;
        case ESP_DDELTA_IN_COPY_PROCESS:
            ESP_LOGD(TAG, "IN COPY_PROCESS");
            current_valid_data_len = s_ddelta_handler->get_patch_valid_len();
            if (current_valid_data_len == 0) {
                return -2;
            }
            current_used_data_len = MIN(current_valid_data_len, (s_ddelta_handler->entry.extra - s_ddelta_handler->current_processed_data_count));

            if (esp_apply_copy(current_used_data_len) != 0) {
                ESP_LOGE(TAG, "apply copy error");
                goto err_handler;
            }

            s_ddelta_handler->current_processed_data_count += current_used_data_len;
            if (s_ddelta_handler->current_processed_data_count != s_ddelta_handler->entry.extra) {
                return -2;
            } else {
                s_ddelta_handler->current_stage = ESP_DDELTA_IN_SKIP_PROCESS;
                s_ddelta_handler->current_processed_data_count = 0;
            }
            break;
        case ESP_DDELTA_IN_SKIP_PROCESS:
            /* Skip remaining bytes */
            ESP_LOGD(TAG, "IN SKIP_PROCESS");
            if (s_ddelta_handler->seek_old(s_ddelta_handler->entry.seek.value) < 0) {
                ESP_LOGE(TAG, "seek err");
                goto err_handler;
            }

            s_ddelta_handler->bytes_written += s_ddelta_handler->entry.diff + s_ddelta_handler->entry.extra;
            s_ddelta_handler->current_stage = ESP_DDELTA_HAVE_NOT_GET_ENTRY;
            break;

        default:
            goto err_handler;
            break;
        }
    }

err_handler:
    esp_ddelta_deinit();
    return -3;
}

// Define some register func() for esp_ddelta begin
// read the patch file in byte stream mode, if addr is NULL, it is similar to seek
static int esp_ddelta_read_patch(uint8_t *buffer, uint32_t len)
{
    if (flush_buf.current_buf_addr == NULL) {
        return -1;
    }
    
    if (!buffer) {
        return -1;
    }
    
    if (flush_buf.current_valid_len == 0) {
        return 0;
    }
    if (len > flush_buf.current_valid_len) {
        len = flush_buf.current_valid_len;
    }
    memcpy(buffer, flush_buf.current_buf_addr + flush_buf.dec_read_pos, len);
    flush_buf.dec_read_pos += len;
    flush_buf.current_valid_len -= len;

    return len;
}

/**
* This is a help function for updating buffer(used to store decompressed patch data) info  for ddelta decoder.
* The processing here is just like shared memory,
* We don't really write data to another buffer, just update the flush buffer's status.
*/
static void update_patch_flush_buf(uint8_t *buffer, uint32_t len)
{
    flush_buf.current_buf_addr = buffer;
    flush_buf.dec_read_pos = 0;
    flush_buf.current_valid_len = len;
}

/**
 * The help function for ddelta decoder to get patch data len can be used in flush buffer.
 */
static uint32_t get_patch_flush_buf_valid_len(void)
{
    return flush_buf.current_valid_len;
}

// read the old file in byte stream mode, if addr is NULL, it is similar to seek
static int esp_ddelta_read_old(void *buf, size_t size)
{
    esp_err_t ret = bootloader_storage_flash_read(custom_ota_config->base_addr + custom_ota_config->base_offset, buf, size, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read old err");
    }

    custom_ota_config->base_offset += size;
    return size;
}

static int esp_ddelta_seek_old(int32_t skip_value)
{
    custom_ota_config->base_offset += skip_value;
    return 0;
}

// write the new file in byte stream mode, if addr is NULL, it is similar to seek
static int esp_ddelta_write_new(void *buf, size_t size)
{
    // write into flash
    return custom_ota_engines[DIFF_ENGINE + 1]->input(buf, size);
}
// Define some register func() for esp_ddelta end

int bootloader_diff_ddelta_init(bootloader_custom_ota_params_t *params)
{
    custom_ota_engines = (bootloader_custom_ota_engine_t **)params->engines;
    custom_ota_config = params->config;
    if (esp_ddelta_init(&esp_ddelta_read_old, &esp_ddelta_seek_old, &esp_ddelta_write_new, &get_patch_flush_buf_valid_len, &esp_ddelta_read_patch) != 0) {
        ESP_LOGE(TAG, "ddelta init err");
        return -1;
    }

    return 0;
}

int bootloader_diff_ddelta_input(void *buf, int size)
{
    // ToDo:
    // ESP_LOGW(TAG, "flush size is %u", size);
    update_patch_flush_buf((uint8_t *)buf, size);
    int ret = esp_ddelta_apply();

    if (ret != -2) {
        if (ret == 0) {
            return size;
        }
        ESP_LOGE(TAG, "patch err");
        return 0;
    }

    return size;
}
#endif // end CONFIG_BOOTLOADER_DIFF_DDELTA