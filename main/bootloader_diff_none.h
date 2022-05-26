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

#ifdef __cplusplus
extern "C" {
#endif

int bootloader_diff_none_init(bootloader_custom_ota_params_t *params);

int bootloader_diff_none_input(void *buf, int size);

#ifdef __cplusplus
}
#endif