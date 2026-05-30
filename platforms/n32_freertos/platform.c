#include "keyboard.h"
#include "keycode_config.h"
#include "matrix.h"
#include "host.h"
#include "led.h"
#include "platform_deps.h"
#include "timer.h"
#include "sync_timer.h"
#include "eeconfig.h"
#ifdef VIA_ENABLE
#    include "via.h"
#endif
#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h"
#endif
#ifdef CONSOLE_ENABLE
#    include "print.h"
#    include "debug.h"
#    include "SEGGER_RTT.h"
#endif

#ifndef N32_QMK_TASK_DELAY_MS
#    define N32_QMK_TASK_DELAY_MS 1
#endif

#ifndef N32_QMK_TASK_STACK_WORDS
#    define N32_QMK_TASK_STACK_WORDS 4096
#endif

#ifdef CONSOLE_ENABLE
#    define N32_RTT_TRACE(msg) SEGGER_RTT_Write(0, msg, sizeof(msg) - 1)
#else
#    define N32_RTT_TRACE(msg) ((void)0)
#endif

#if defined(CONSOLE_ENABLE) && defined(N32_QMK_DEBUG_ALL)
static void n32_enable_qmk_debug(void) {
    debug_config.enable   = true;
    debug_config.matrix   = true;
    debug_config.keyboard = true;
    debug_config.mouse    = true;
    N32_RTT_TRACE("N32 QMK debug flags enabled\n");
}
#else
static void n32_enable_qmk_debug(void) {}
#endif

#ifdef N32_TRACE_KEYBOARD_INIT
void quantum_init(void);
void keyboard_post_init_quantum(void);

#ifdef RGB_MATRIX_ENABLE
void eeconfig_debug_rgb_matrix(void);

static void n32_rgb_matrix_init_traced(void) {
#    ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
    rgb_matrix_init();
#    else
    N32_RTT_TRACE("N32 rgb_matrix_init: driver_init\n");
    rgb_matrix_driver.init();
    N32_RTT_TRACE("N32 rgb_matrix_init: driver_init ok\n");

    N32_RTT_TRACE("N32 rgb_matrix_init: eeconfig_read\n");
    eeconfig_read_rgb_matrix(&rgb_matrix_config);
    N32_RTT_TRACE("N32 rgb_matrix_init: eeconfig_read ok\n");

    if (!rgb_matrix_config.mode || rgb_matrix_config.mode >= RGB_MATRIX_EFFECT_MAX) {
        N32_RTT_TRACE("N32 rgb_matrix_init: default_set\n");
        rgb_matrix_config.enable = RGB_MATRIX_DEFAULT_ON;
        rgb_matrix_config.mode   = RGB_MATRIX_DEFAULT_MODE;
        rgb_matrix_config.hsv    = (hsv_t){RGB_MATRIX_DEFAULT_HUE, RGB_MATRIX_DEFAULT_SAT, RGB_MATRIX_DEFAULT_VAL};
        rgb_matrix_config.speed  = RGB_MATRIX_DEFAULT_SPD;
        rgb_matrix_config.flags  = RGB_MATRIX_DEFAULT_FLAGS;
        N32_RTT_TRACE("N32 rgb_matrix_init: default_set ok\n");

        N32_RTT_TRACE("N32 rgb_matrix_init: default_write\n");
        eeconfig_update_rgb_matrix(&rgb_matrix_config);
        N32_RTT_TRACE("N32 rgb_matrix_init: default_write ok\n");
    }

    N32_RTT_TRACE("N32 rgb_matrix_init: eeconfig_debug skipped\n");
#    endif
}
#endif

static void n32_keyboard_init_traced(void) {
    N32_RTT_TRACE("N32 keyboard_init: timer_init\n");
    timer_init();
    N32_RTT_TRACE("N32 keyboard_init: sync_timer_init\n");
    sync_timer_init();

#ifdef VIA_ENABLE
    N32_RTT_TRACE("N32 keyboard_init: via_init\n");
    via_init();
    N32_RTT_TRACE("N32 keyboard_init: via_init ok\n");
#endif

    N32_RTT_TRACE("N32 keyboard_init: matrix_init\n");
    matrix_init();
    N32_RTT_TRACE("N32 keyboard_init: matrix_init ok\n");

    N32_RTT_TRACE("N32 keyboard_init: quantum_init\n");
    quantum_init();
    N32_RTT_TRACE("N32 keyboard_init: quantum_init ok\n");
    n32_enable_qmk_debug();

    N32_RTT_TRACE("N32 keyboard_init: host_init\n");
    host_init();
    N32_RTT_TRACE("N32 keyboard_init: led_init_ports\n");
    led_init_ports();

#ifdef RGB_MATRIX_ENABLE
    N32_RTT_TRACE("N32 keyboard_init: rgb_matrix_init\n");
    n32_rgb_matrix_init_traced();
    N32_RTT_TRACE("N32 keyboard_init: rgb_matrix_init ok\n");
#endif

#if defined(NKRO_ENABLE) && defined(FORCE_NKRO)
    keymap_config.nkro = 1;
    eeconfig_update_keymap(&keymap_config);
#endif

    N32_RTT_TRACE("N32 keyboard_init: keyboard_post_init_quantum\n");
    keyboard_post_init_quantum();
    N32_RTT_TRACE("N32 keyboard_init: keyboard_post_init_quantum ok\n");
}
#endif

void n32_board_early_init(void);
void n32_board_init(void);
void n32_usb_init(void);
void n32_usb_task(void);
void protocol_setup(void);
void protocol_pre_init(void);
void protocol_post_init(void);
void protocol_pre_task(void);
void protocol_post_task(void);

void protocol_keyboard_task(void) __attribute__((weak));
void protocol_keyboard_task(void) {
    keyboard_task();
}

static void qmk_freertos_task(void *arg) {
    (void)arg;

    N32_RTT_TRACE("N32 QMK task entered\n");

    protocol_setup();
    N32_RTT_TRACE("N32 protocol_setup ok\n");

    keyboard_setup();
    N32_RTT_TRACE("N32 keyboard_setup ok\n");
    n32_enable_qmk_debug();

    protocol_pre_init();
#ifdef N32_TRACE_KEYBOARD_INIT
    n32_keyboard_init_traced();
#else
    keyboard_init();
#endif
    N32_RTT_TRACE("N32 keyboard_init ok\n");
    n32_enable_qmk_debug();

    protocol_post_init();
    N32_RTT_TRACE("N32 protocol_post_init ok\n");

#ifdef CONSOLE_ENABLE
    N32_RTT_TRACE("N32 QMK started\n");
#endif

    bool main_loop_logged = false;
    while (true) {
        if (!main_loop_logged) {
            N32_RTT_TRACE("N32 main loop entered\n");
            main_loop_logged = true;
        }
        protocol_pre_task();
        protocol_keyboard_task();
        protocol_post_task();
#ifdef RAW_ENABLE
        void raw_hid_task(void);
        raw_hid_task();
#endif
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
#ifdef CONSOLE_ENABLE
    SEGGER_RTT_Init();
    N32_RTT_TRACE("N32 RTT boot\n");
#endif

    platform_setup();
    N32_RTT_TRACE("N32 platform_setup ok\n");

    n32_usb_init();
    N32_RTT_TRACE("N32 usb_init ok\n");

    BaseType_t task_created = xTaskCreate(qmk_freertos_task, "qmk", N32_QMK_TASK_STACK_WORDS, NULL, tskIDLE_PRIORITY + 2, NULL);
    if (task_created == pdPASS) {
        N32_RTT_TRACE("N32 qmk task create ok\n");
    } else {
        N32_RTT_TRACE("N32 qmk task create failed\n");
    }

    vTaskStartScheduler();
    N32_RTT_TRACE("N32 scheduler returned\n");

    while (true) {
    }
}

#ifdef CONSOLE_ENABLE
void console_task(void) {}
#endif
