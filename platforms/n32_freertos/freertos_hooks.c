#include "platform_deps.h"

#ifdef CONSOLE_ENABLE
#    include "SEGGER_RTT.h"

static unsigned n32_strlen(const char *text) {
    unsigned len = 0;
    while (text && text[len] != '\0') {
        len++;
    }
    return len;
}

static void n32_hook_trace(const char *text) {
    SEGGER_RTT_Write(0, text, n32_strlen(text));
}
#else
static void n32_hook_trace(const char *text) {
    (void)text;
}
#endif

void vApplicationMallocFailedHook(void) {
    n32_hook_trace("N32 FreeRTOS malloc failed\n");
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    n32_hook_trace("N32 FreeRTOS stack overflow: ");
    n32_hook_trace(name);
    n32_hook_trace("\n");
    taskDISABLE_INTERRUPTS();
    while (true) {
    }
}

void vApplicationIdleHook(void) {}
void vApplicationTickHook(void) {}
