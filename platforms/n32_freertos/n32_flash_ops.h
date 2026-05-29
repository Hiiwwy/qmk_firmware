// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool n32_flash_begin(void);
void n32_flash_end(void);
bool n32_flash_erase_page(uint32_t address);
bool n32_flash_program_doubleword(uint32_t address, uint32_t data0, uint32_t data1);
