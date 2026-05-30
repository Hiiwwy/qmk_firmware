/*
 * Copyright 2025 N32 QMK Contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * sendchar backend using SEGGER RTT for N32 FreeRTOS platform.
 * Redirects QMK print/dprintf/uprintf output to J-Link RTT Viewer.
 */

#include "SEGGER_RTT.h"
#include "sendchar.h"

int8_t sendchar(uint8_t c) {
    SEGGER_RTT_PutChar(0, (char)c);
    return 0;
}
