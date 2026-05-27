#include "platform_deps.h"

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    (void)name;
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void vApplicationIdleHook(void) {}
void vApplicationTickHook(void) {}
