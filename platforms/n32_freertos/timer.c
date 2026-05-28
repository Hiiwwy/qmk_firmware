#include "platform_deps.h"
#include "timer.h"

volatile uint32_t timer_count;
static uint32_t   saved_ms;

void timer_init(void) {
    timer_count = 0;
}

void timer_clear(void) {
    taskENTER_CRITICAL();
    timer_count = 0;
    taskEXIT_CRITICAL();
}

void timer_save(void) {
    saved_ms = timer_read32();
}

void timer_restore(void) {
    taskENTER_CRITICAL();
    timer_count = saved_ms;
    taskEXIT_CRITICAL();
}

uint16_t timer_read(void) {
    return (uint16_t)timer_read32();
}

uint32_t timer_read32(void) {
    uint32_t value;
    taskENTER_CRITICAL();
    value = timer_count;
    taskEXIT_CRITICAL();
    return value;
}

void n32_qmk_tick_1ms(void) {
    timer_count++;
}
