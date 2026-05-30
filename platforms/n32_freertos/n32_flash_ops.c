// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "n32_flash_ops.h"

#include "FreeRTOS.h"
#include "n32h47x_48x_flash.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t n32_flash_mutex;
static volatile bool     n32_flash_pre_scheduler_locked;

static bool n32_flash_scheduler_started(void) {
    return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
}

static void n32_flash_init_mutex(void) {
    if (n32_flash_mutex == NULL) {
        n32_flash_mutex = xSemaphoreCreateRecursiveMutex();
    }
}

bool n32_flash_begin(void) {
    if (n32_flash_scheduler_started()) {
        n32_flash_init_mutex();
        if (n32_flash_mutex == NULL || xSemaphoreTakeRecursive(n32_flash_mutex, portMAX_DELAY) != pdTRUE) {
            return false;
        }
    } else {
        while (n32_flash_pre_scheduler_locked) {
        }
        n32_flash_pre_scheduler_locked = true;
    }

    FLASH_Unlock();
    return true;
}

void n32_flash_end(void) {
    FLASH_Lock();

    if (n32_flash_scheduler_started()) {
        if (n32_flash_mutex != NULL) {
            xSemaphoreGiveRecursive(n32_flash_mutex);
        }
    } else {
        n32_flash_pre_scheduler_locked = false;
    }
}

bool n32_flash_erase_page(uint32_t address) {
    return FLASH_EraseOnePage(address) == FLASH_EOP;
}

bool n32_flash_program_doubleword(uint32_t address, uint32_t data0, uint32_t data1) {
    return FLASH_ProgramdoubleWord(address, data0, data1) == FLASH_EOP;
}
