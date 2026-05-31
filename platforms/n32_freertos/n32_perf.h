// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>

typedef enum { N32_PERF_LOOP, N32_PERF_PROTOCOL_PRE, N32_PERF_KEYBOARD, N32_PERF_PROTOCOL_POST, N32_PERF_RAW_HID, N32_PERF_HOUSEKEEPING, N32_PERF_MATRIX_SCAN, N32_PERF_DEBOUNCE, N32_PERF_RGB_FLUSH, N32_PERF_COUNT } n32_perf_section_t;

#ifdef N32_TRACE_PERF
void     n32_perf_init(void);
uint32_t n32_perf_cycles(void);
void     n32_perf_record(n32_perf_section_t section, uint32_t start_cycles);
void     n32_perf_report_if_due(void);
#else
static inline void     n32_perf_init(void) {}
static inline uint32_t n32_perf_cycles(void) {
    return 0;
}
static inline void n32_perf_record(n32_perf_section_t section, uint32_t start_cycles) {
    (void)section;
    (void)start_cycles;
}
static inline void n32_perf_report_if_due(void) {}
#endif
