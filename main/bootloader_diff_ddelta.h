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
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fork of BSDIFF that does not compress ctrl, diff, extra blocks */
#define DDELTA_MAGIC "DDELTA40"

/**
 * A ddelta file has the following format:
 *
 * * the header
 * * a list of entries
 */
struct ddelta_header {
    char magic[8];
    uint64_t new_file_size;
};

/**
 * An entry consists of this header, followed by
 *
 * 1. 'diff' bytes of diff data
 * 2. 'extra' bytes of extra data
 */
struct ddelta_entry_header {
    uint64_t diff;
    uint64_t extra;
    union {
        int64_t value;
        uint64_t raw;
    } seek;
};

/* Static assertions that the headers have the correct size. */
typedef int ddelta_assert_header_size[sizeof(struct ddelta_header) == 16 ? 1 : -1];
typedef int ddelta_assert_entry_header_size[sizeof(struct ddelta_entry_header) == 24 ? 1 : -1];

/**
 * Error codes to be returned by ddelta functions.
 *
 * Each function returns a negated variant of these error code on success, and for the
 * I/O errors, more information is available in errno.
 */
enum ddelta_error {
    /** The patch file has an invalid magic or header could not be read */
    DDELTA_EMAGIC = 1,
    /** An unknown algorithm error occured */
    DDELTA_EALGO,
    /** An I/O error occured while reading from (apply) or writing to (generate) the patch file */
    DDELTA_EPATCHIO,
    /** An I/O error occured while reading from the old file */
    DDELTA_EOLDIO,
    /** An I/O error occured while reading from (generate) or writing to (apply) the new file */
    DDELTA_ENEWIO,
    /** Patch ended before target file was fully written */
    DDELTA_EPATCHSHORT
};

/* Size of blocks to work on at once */
#ifndef DDELTA_BLOCK_SIZE
#define DDELTA_BLOCK_SIZE (4 * 1024) // The length must be flash sector size align
#endif

int bootloader_diff_ddelta_init(bootloader_custom_ota_params_t *params);

int bootloader_diff_ddelta_input(void *buf, int size);

#ifdef __cplusplus
}
#endif