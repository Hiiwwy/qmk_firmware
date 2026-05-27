#include "_wait.h"

void wait_ms(uint32_t ms) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1));
        return;
    }

    for (volatile uint32_t i = 0; i < (ms ? ms : 1) * 12000U; ++i) {
        __NOP();
    }
}

void wait_us(uint16_t us) {
    for (volatile uint32_t i = 0; i < (uint32_t)(us ? us : 1) * 12U; ++i) {
        __NOP();
    }
}
