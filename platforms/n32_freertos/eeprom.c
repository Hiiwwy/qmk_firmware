#include <string.h>
#include <stdbool.h>

#include "n32h47x_48x_flash.h"
#include "eeprom_driver.h"
#include "eeprom.h"

#ifndef N32_EEPROM_SIZE
#    define N32_EEPROM_SIZE 1024
#endif

#ifndef N32_EEPROM_FLASH_BASE
#    define N32_EEPROM_FLASH_BASE 0x0807E000U
#endif

static uint8_t eeprom_shadow[N32_EEPROM_SIZE];
static bool    eeprom_dirty;

static void eeprom_load_shadow(void) {
    memcpy(eeprom_shadow, (const void *)N32_EEPROM_FLASH_BASE, sizeof(eeprom_shadow));
}

static void eeprom_flush_shadow(void) {
    FLASH_STS status;

    FLASH_Unlock();
    status = FLASH_EraseOnePage(N32_EEPROM_FLASH_BASE);
    if (status == FLASH_EOP) {
        for (size_t i = 0; i < sizeof(eeprom_shadow); i += 8) {
            uint32_t data0 = 0xFFFFFFFFU;
            uint32_t data1 = 0xFFFFFFFFU;

            memcpy(&data0, &eeprom_shadow[i], sizeof(data0));
            if (i + sizeof(data0) < sizeof(eeprom_shadow)) {
                memcpy(&data1, &eeprom_shadow[i + sizeof(data0)], sizeof(data1));
            }

            status = FLASH_ProgramdoubleWord(N32_EEPROM_FLASH_BASE + i, data0, data1);
            if (status != FLASH_EOP) {
                break;
            }
        }
    }
    FLASH_Lock();

    if (status == FLASH_EOP) {
        eeprom_dirty = false;
    }
}

void eeprom_driver_init(void) {
    eeprom_load_shadow();
}

void eeprom_driver_format(bool erase) {
    if (erase) {
        eeprom_driver_erase();
    }
}

void eeprom_driver_erase(void) {
    memset(eeprom_shadow, 0xFF, sizeof(eeprom_shadow));
    eeprom_dirty = true;
    eeprom_flush_shadow();
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
    if (memcmp(&eeprom_shadow[offset], buf, len) == 0) {
        return;
    }
    memcpy(&eeprom_shadow[offset], buf, len);
    eeprom_dirty = true;
    eeprom_flush_shadow();
}
