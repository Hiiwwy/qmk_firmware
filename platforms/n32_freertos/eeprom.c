#include <string.h>
#include "eeprom_driver.h"
#include "eeprom.h"

#ifndef N32_EEPROM_SIZE
#    define N32_EEPROM_SIZE 1024
#endif

static uint8_t eeprom_shadow[N32_EEPROM_SIZE];

void eeprom_driver_init(void) {}

void eeprom_driver_format(bool erase) {
    if (erase) {
        eeprom_driver_erase();
    }
}

void eeprom_driver_erase(void) {
    memset(eeprom_shadow, 0xFF, sizeof(eeprom_shadow));
}

void eeprom_read_block(void *buf, const void *addr, size_t len) {
    uintptr_t offset = (uintptr_t)addr;
    if (offset + len > sizeof(eeprom_shadow)) {
        memset(buf, 0xFF, len);
        return;
    }
    memcpy(buf, &eeprom_shadow[offset], len);
}

void eeprom_write_block(const void *buf, void *addr, size_t len) {
    uintptr_t offset = (uintptr_t)addr;
    if (offset + len > sizeof(eeprom_shadow)) {
        return;
    }
    memcpy(&eeprom_shadow[offset], buf, len);
}
