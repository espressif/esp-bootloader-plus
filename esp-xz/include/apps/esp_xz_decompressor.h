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

#ifndef ESP_XZ_DECOMPRESS_H
#define ESP_XZ_DECOMPRESS_H
#include <stdint.h>

/**
 * @brief   This function implements the API defined in <linux/decompress/generic.h>.
 *
 * This wrapper will automatically choose single-call or multi-call mode
 * of the native XZ decoder API. The single-call mode can be used only when
 * both input and output buffers are available as a single chunk, i.e. when
 * fill() and flush() won't be used.
 * 
 * @param[in]  in pointer to the input buffer to be decompressed
 * @param[in]  in_size size of the input buffer
 * @param[in]  fill pointer to the function used to read compressed data
 * @param[in]  flush pointer to the function used to write decompressed data
 * @param[out] out pointer to the out buffer to store the decompresssed data
 * @param[out] in_used length of the data has been used to decompressed
 * @param[in]  error pointer to the function to output the error message
 * @return
 *         - 0 on success
 *         - -1 on error
 *
 */
int esp_xz_decompress(unsigned char *in, int in_size,
	int (*fill)(void *dest, unsigned int size),
	int (*flush)(void *src, unsigned int size),
	unsigned char *out, int *in_used,
	void (*error)(char *x));

#endif