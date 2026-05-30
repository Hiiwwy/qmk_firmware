// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "platform_deps.h"

#ifdef CONSOLE_ENABLE
#    include "SEGGER_RTT.h"

static void n32_fault_trace(const char *text) {
    unsigned len = 0;
    while (text[len] != '\0') {
        len++;
    }
    SEGGER_RTT_Write(0, text, len);
}

static void n32_fault_trace_hex(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char              text[11];

    text[0] = '0';
    text[1] = 'x';
    for (uint8_t i = 0; i < 8; ++i) {
        text[2 + i] = hex[(value >> (28 - (i * 4))) & 0x0F];
    }
    text[10] = '\n';

    SEGGER_RTT_Write(0, text, sizeof(text));
}

static void n32_fault_dump(const char *name) {
    n32_fault_trace(name);
    n32_fault_trace(" HFSR=");
    n32_fault_trace_hex(SCB->HFSR);
    n32_fault_trace(" CFSR=");
    n32_fault_trace_hex(SCB->CFSR);
    n32_fault_trace(" BFAR=");
    n32_fault_trace_hex(SCB->BFAR);
    n32_fault_trace(" MMFAR=");
    n32_fault_trace_hex(SCB->MMFAR);
}
#else
static void n32_fault_dump(const char *name) {
    (void)name;
}
#endif

void HardFault_Handler(void) {
    n32_fault_dump("N32 HardFault");
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void MemManage_Handler(void) {
    n32_fault_dump("N32 MemManage");
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void BusFault_Handler(void) {
    n32_fault_dump("N32 BusFault");
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void UsageFault_Handler(void) {
    n32_fault_dump("N32 UsageFault");
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}
