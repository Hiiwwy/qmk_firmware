#pragma once

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

static inline uint8_t __n32_interrupt_disable__(void) {
    taskENTER_CRITICAL();
    return 1;
}

static inline void __n32_interrupt_enable__(const uint8_t *state) {
    (void)state;
    taskEXIT_CRITICAL();
    __asm__ volatile("" ::: "memory");
}

#define ATOMIC_BLOCK(type) for (type, __ToDo = 1; __ToDo; __ToDo = 0)
#define ATOMIC_FORCEON uint8_t status_save __attribute__((__cleanup__(__n32_interrupt_enable__))) = __n32_interrupt_disable__()
#define ATOMIC_RESTORESTATE ATOMIC_FORCEON

#define ATOMIC_BLOCK_RESTORESTATE ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#define ATOMIC_BLOCK_FORCEON ATOMIC_BLOCK(ATOMIC_FORCEON)
