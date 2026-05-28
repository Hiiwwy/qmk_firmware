#include "keyboard.h"
#include "platform_deps.h"
#include "timer.h"

#ifndef N32_QMK_TASK_DELAY_MS
#    define N32_QMK_TASK_DELAY_MS 1
#endif

#ifndef N32_QMK_TASK_STACK_WORDS
#    define N32_QMK_TASK_STACK_WORDS 2048
#endif

void n32_board_early_init(void);
void n32_board_init(void);
void n32_usb_init(void);
void n32_usb_task(void);

static void qmk_freertos_task(void *arg) {
    (void)arg;

    keyboard_setup();
    keyboard_init();

    while (true) {
        keyboard_task();
        housekeeping_task();
#if N32_QMK_TASK_DELAY_MS > 0
        vTaskDelay(pdMS_TO_TICKS(1));
#else
        taskYIELD();
#endif
    }
}

void platform_setup(void) {
    n32_board_early_init();
    timer_init();
    n32_board_init();
}

int main(void) {
    platform_setup();
    n32_usb_init();

    xTaskCreate(qmk_freertos_task, "qmk", N32_QMK_TASK_STACK_WORDS, NULL, tskIDLE_PRIORITY + 2, NULL);
    vTaskStartScheduler();

    while (true) {
    }
}
