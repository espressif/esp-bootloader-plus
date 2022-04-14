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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOOTLOADER_BUILD
#include <stdlib.h>
#define bootloader_custom_malloc(size) malloc(size)
#define bootloader_custom_free(ptr) free(ptr)
#else
void* bootloader_custom_malloc(size_t size);
void bootloader_custom_free(void *ptr);
#endif

#ifdef __cplusplus
}
#endif