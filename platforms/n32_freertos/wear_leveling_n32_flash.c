// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "n32_flash_ops.h"
#include "nvm_eeconfig.h"
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

static void n32_eeprom_migrate_raw_format(void);

bool backing_store_init(void) {
    backing_store_is_unlocked = false;
    backing_store_lock();
    n32_eeprom_migrate_raw_format();
    return true;
}

bool backing_store_unlock(void) {
    if (!n32_flash_begin()) {
        return false;
    }
    backing_store_is_unlocked = true;
    return true;
}

bool backing_store_lock(void) {
    if (backing_store_is_unlocked) {
        n32_flash_end();
    }
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

#ifdef N32_EEPROM_MIGRATE_RAW_FORMAT
static void n32_eeprom_migrate_raw_format(void) {
    uint16_t raw_magic;

    memcpy(&raw_magic, (const void *)N32_EEPROM_FLASH_BASE, sizeof(raw_magic));
    if (raw_magic == EECONFIG_MAGIC_NUMBER) {
        backing_store_erase();
    }
}
#else
static void n32_eeprom_migrate_raw_format(void) {}
#endif

static backing_store_int_t n32_flash_to_wear_leveling(backing_store_int_t value) {
    return ~value;
}

static backing_store_int_t n32_wear_leveling_to_flash(backing_store_int_t value) {
    return ~value;
}

bool backing_store_erase(void) {
    bool status = true;
    bool locked_here;

    locked_here = backing_store_unlock_if_needed();
    for (uint32_t offset = 0; offset < WEAR_LEVELING_BACKING_SIZE; offset += N32_FLASH_PAGE_SIZE) {
        status = n32_flash_erase_page(N32_EEPROM_FLASH_BASE + offset);
        if (!status) {
            break;
        }
    }
    if (locked_here) {
        backing_store_lock();
    }

    return status;
}

bool backing_store_read(uint32_t address, backing_store_int_t *value) {
    backing_store_int_t stored_value;

    if (address + sizeof(*value) > WEAR_LEVELING_BACKING_SIZE) {
        return false;
    }

    memcpy(&stored_value, (const void *)(N32_EEPROM_FLASH_BASE + address), sizeof(stored_value));
    *value = n32_flash_to_wear_leveling(stored_value);
    return true;
}

bool backing_store_read_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    size_t length = item_count * sizeof(*values);

    if (address + length > WEAR_LEVELING_BACKING_SIZE) {
        return false;
    }

    memcpy(values, (const void *)(N32_EEPROM_FLASH_BASE + address), length);
    for (size_t i = 0; i < item_count; ++i) {
        values[i] = n32_flash_to_wear_leveling(values[i]);
    }
    return true;
}

bool backing_store_write(uint32_t address, backing_store_int_t value) {
    return backing_store_write_bulk(address, &value, 1);
}

bool backing_store_write_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    bool   status = true;
    size_t length = item_count * sizeof(*values);
    bool   locked_here;

    if (address + length > WEAR_LEVELING_BACKING_SIZE) {
        return false;
    }

    locked_here = backing_store_unlock_if_needed();
    for (size_t i = 0; i < item_count; ++i) {
        backing_store_int_t stored_value = n32_wear_leveling_to_flash(values[i]);
        uint32_t            data0        = 0xFFFFFFFFU;
        uint32_t            data1        = 0xFFFFFFFFU;

        if (stored_value == (backing_store_int_t) ~(backing_store_int_t)0) {
            continue;
        }

        memcpy(&data0, &stored_value, sizeof(data0));
        memcpy(&data1, ((const uint8_t *)&stored_value) + sizeof(data0), sizeof(data1));

        status = n32_flash_program_doubleword(N32_EEPROM_FLASH_BASE + address + (i * sizeof(*values)), data0, data1);
        if (!status) {
            break;
        }
    }
    if (locked_here) {
        backing_store_lock();
    }

    return status;
}
