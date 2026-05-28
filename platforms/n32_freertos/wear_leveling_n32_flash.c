// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "n32h47x_48x_flash.h"
#include "wear_leveling/wear_leveling_internal.h"

#ifndef N32_EEPROM_FLASH_BASE
#    define N32_EEPROM_FLASH_BASE 0x0807E000U
#endif

#ifndef N32_FLASH_PAGE_SIZE
#    define N32_FLASH_PAGE_SIZE 0x2000U
#endif

#if (WEAR_LEVELING_BACKING_SIZE % N32_FLASH_PAGE_SIZE) != 0
#    error WEAR_LEVELING_BACKING_SIZE must be aligned to the N32 Flash page size.
#endif

static bool backing_store_is_unlocked;

bool backing_store_init(void) {
    backing_store_is_unlocked = false;
    backing_store_lock();
    return true;
}

bool backing_store_unlock(void) {
    FLASH_Unlock();
    backing_store_is_unlocked = true;
    return true;
}

bool backing_store_lock(void) {
    FLASH_Lock();
    backing_store_is_unlocked = false;
    return true;
}

static bool backing_store_unlock_if_needed(void) {
    if (backing_store_is_unlocked) {
        return false;
    }

    backing_store_unlock();
    return true;
}

bool backing_store_erase(void) {
    FLASH_STS status = FLASH_EOP;
    bool      locked_here;

    locked_here = backing_store_unlock_if_needed();
    for (uint32_t offset = 0; offset < WEAR_LEVELING_BACKING_SIZE; offset += N32_FLASH_PAGE_SIZE) {
        status = FLASH_EraseOnePage(N32_EEPROM_FLASH_BASE + offset);
        if (status != FLASH_EOP) {
            break;
        }
    }
    if (locked_here) {
        backing_store_lock();
    }

    return status == FLASH_EOP;
}

bool backing_store_read(uint32_t address, backing_store_int_t *value) {
    if (address + sizeof(*value) > WEAR_LEVELING_BACKING_SIZE) {
        return false;
    }

    memcpy(value, (const void *)(N32_EEPROM_FLASH_BASE + address), sizeof(*value));
    return true;
}

bool backing_store_read_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    size_t length = item_count * sizeof(*values);

    if (address + length > WEAR_LEVELING_BACKING_SIZE) {
        return false;
    }

    memcpy(values, (const void *)(N32_EEPROM_FLASH_BASE + address), length);
    return true;
}

bool backing_store_write(uint32_t address, backing_store_int_t value) {
    return backing_store_write_bulk(address, &value, 1);
}

bool backing_store_write_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    FLASH_STS status = FLASH_EOP;
    size_t    length = item_count * sizeof(*values);
    bool      locked_here;

    if (address + length > WEAR_LEVELING_BACKING_SIZE) {
        return false;
    }

    locked_here = backing_store_unlock_if_needed();
    for (size_t i = 0; i < item_count; ++i) {
        uint32_t data0 = 0xFFFFFFFFU;
        uint32_t data1 = 0xFFFFFFFFU;

        memcpy(&data0, &values[i], sizeof(data0));
        memcpy(&data1, ((const uint8_t *)&values[i]) + sizeof(data0), sizeof(data1));

        status = FLASH_ProgramdoubleWord(N32_EEPROM_FLASH_BASE + address + (i * sizeof(*values)), data0, data1);
        if (status != FLASH_EOP) {
            break;
        }
    }
    if (locked_here) {
        backing_store_lock();
    }

    return status == FLASH_EOP;
}
