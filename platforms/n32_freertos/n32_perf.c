// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "n32_perf.h"

#ifdef N32_TRACE_PERF

#    include <stdio.h>

#    include "SEGGER_RTT.h"
#    include "keyboard.h"
#    include "platform_deps.h"
#    include "timer.h"

typedef struct {
    uint32_t cycles;
    uint32_t max_cycles;
    uint32_t count;
} n32_perf_counter_t;

static n32_perf_counter_t n32_perf_counters[N32_PERF_COUNT];
static uint32_t           n32_perf_report_timer;

static uint32_t n32_perf_cycles_to_us(uint32_t cycles, uint32_t count) {
    uint32_t hz = SystemCoreClock ? SystemCoreClock : 240000000UL;

    if (count == 0U || hz == 0U) {
        return 0;
    }

    return (uint32_t)((((uint64_t)cycles * 1000000ULL) / hz) / count);
}

static uint32_t n32_perf_max_to_us(n32_perf_section_t section) {
    uint32_t hz = SystemCoreClock ? SystemCoreClock : 240000000UL;

    if (hz == 0U) {
        return 0;
    }

    return (uint32_t)(((uint64_t)n32_perf_counters[section].max_cycles * 1000000ULL) / hz);
}

static uint32_t n32_perf_avg_us(n32_perf_section_t section) {
    return n32_perf_cycles_to_us(n32_perf_counters[section].cycles, n32_perf_counters[section].count);
}

void n32_perf_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    n32_perf_report_timer = timer_read32();
}

uint32_t n32_perf_cycles(void) {
    return DWT->CYCCNT;
}

void n32_perf_record(n32_perf_section_t section, uint32_t start_cycles) {
    if (section >= N32_PERF_COUNT) {
        return;
    }

    uint32_t elapsed = DWT->CYCCNT - start_cycles;

    n32_perf_counters[section].cycles += elapsed;
    n32_perf_counters[section].count++;
    if (elapsed > n32_perf_counters[section].max_cycles) {
        n32_perf_counters[section].max_cycles = elapsed;
    }
}

void n32_perf_report_if_due(void) {
    if (timer_elapsed32(n32_perf_report_timer) < 1000U) {
        return;
    }

    char buffer[256];
    int  length = snprintf(buffer, sizeof(buffer), "PERF hz=%lu loop=%lu/%luus pre=%lu kb=%lu/%lu post=%lu raw=%lu house=%lu matrix=%lu/%lu debounce=%lu rgb=%lu/%lu calls=%lu/%lu/%lu\n", (unsigned long)get_matrix_scan_rate(), (unsigned long)n32_perf_avg_us(N32_PERF_LOOP), (unsigned long)n32_perf_max_to_us(N32_PERF_LOOP), (unsigned long)n32_perf_avg_us(N32_PERF_PROTOCOL_PRE), (unsigned long)n32_perf_avg_us(N32_PERF_KEYBOARD), (unsigned long)n32_perf_max_to_us(N32_PERF_KEYBOARD), (unsigned long)n32_perf_avg_us(N32_PERF_PROTOCOL_POST), (unsigned long)n32_perf_avg_us(N32_PERF_RAW_HID), (unsigned long)n32_perf_avg_us(N32_PERF_HOUSEKEEPING), (unsigned long)n32_perf_avg_us(N32_PERF_MATRIX_SCAN), (unsigned long)n32_perf_max_to_us(N32_PERF_MATRIX_SCAN), (unsigned long)n32_perf_avg_us(N32_PERF_DEBOUNCE), (unsigned long)n32_perf_avg_us(N32_PERF_RGB_FLUSH), (unsigned long)n32_perf_max_to_us(N32_PERF_RGB_FLUSH), (unsigned long)n32_perf_counters[N32_PERF_LOOP].count,
                           (unsigned long)n32_perf_counters[N32_PERF_MATRIX_SCAN].count, (unsigned long)n32_perf_counters[N32_PERF_RGB_FLUSH].count);

    if (length > 0) {
        SEGGER_RTT_Write(0, buffer, length < (int)sizeof(buffer) ? (unsigned)length : sizeof(buffer) - 1U);
    }

    for (uint8_t i = 0; i < N32_PERF_COUNT; ++i) {
        n32_perf_counters[i] = (n32_perf_counter_t){0};
    }
    n32_perf_report_timer = timer_read32();
}

#endif
