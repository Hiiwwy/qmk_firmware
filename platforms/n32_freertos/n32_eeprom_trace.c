// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <stdint.h>

#include "SEGGER_RTT.h"

void __real_eeprom_read_block(void *buf, const void *addr, size_t len);
void __real_eeprom_write_block(const void *buf, void *addr, size_t len);

static void n32_eeprom_trace(const char *text) {
    unsigned len = 0;
    while (text[len] != '\0') {
        len++;
    }
    SEGGER_RTT_Write(0, text, len);
}

static void n32_eeprom_trace_hex(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char              text[10];

    text[0] = '0';
    text[1] = 'x';
    for (uint8_t i = 0; i < 8; ++i) {
        text[2 + i] = hex[(value >> (28 - (i * 4))) & 0x0F];
    }

    SEGGER_RTT_Write(0, text, sizeof(text));
}

void __wrap_eeprom_read_block(void *buf, const void *addr, size_t len) {
    n32_eeprom_trace("N32 eeprom read addr=");
    n32_eeprom_trace_hex((uint32_t)(uintptr_t)addr);
    n32_eeprom_trace(" len=");
    n32_eeprom_trace_hex((uint32_t)len);
    n32_eeprom_trace("\n");
    __real_eeprom_read_block(buf, addr, len);
    n32_eeprom_trace("N32 eeprom read ok\n");
}

void __wrap_eeprom_write_block(const void *buf, void *addr, size_t len) {
    n32_eeprom_trace("N32 eeprom write addr=");
    n32_eeprom_trace_hex((uint32_t)(uintptr_t)addr);
    n32_eeprom_trace(" len=");
    n32_eeprom_trace_hex((uint32_t)len);
    n32_eeprom_trace("\n");
    __real_eeprom_write_block(buf, addr, len);
    n32_eeprom_trace("N32 eeprom write ok\n");
}
