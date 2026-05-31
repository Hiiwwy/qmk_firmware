// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ws2812.h"

#include "gpio.h"
#include "n32_perf.h"
#include "platform_deps.h"
#include "wait.h"
#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_RGB)
#    include "SEGGER_RTT.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef WS2812_DI_PIN
#    define WS2812_DI_PIN B7
#endif

#ifndef N32_WS2812_DI_PIN_NAME
#    define N32_WS2812_DI_PIN_NAME "WS2812_DI_PIN"
#endif

#ifndef N32_WS2812_TIMING_FALLBACK_HZ
#    define N32_WS2812_TIMING_FALLBACK_HZ 240000000UL
#endif

#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_RGB)
#    define N32_WS2812_TRACE(msg) SEGGER_RTT_Write(0, msg, sizeof(msg) - 1)
#else
#    define N32_WS2812_TRACE(msg) ((void)0)
#endif

static void n32_ws2812_tracef(const char *format, ...) {
#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_RGB)
    char    buffer[160];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length > 0) {
        SEGGER_RTT_Write(0, buffer, length < (int)sizeof(buffer) ? (unsigned)length : sizeof(buffer) - 1U);
    }
#else
    (void)format;
#endif
}

static GPIO_Module *n32_ws2812_port(void) {
    switch (N32_PIN_PORT(WS2812_DI_PIN)) {
        case N32_PORT_A:
            return GPIOA;
        case N32_PORT_B:
            return GPIOB;
        case N32_PORT_C:
            return GPIOC;
        case N32_PORT_D:
            return GPIOD;
        case N32_PORT_E:
            return GPIOE;
        case N32_PORT_F:
            return GPIOF;
        case N32_PORT_G:
            return GPIOG;
        case N32_PORT_H:
            return GPIOH;
        default:
            return GPIOA;
    }
}

static inline uint32_t n32_ws2812_cycles_from_ns(uint32_t ns) {
    uint32_t hz = SystemCoreClock ? SystemCoreClock : N32_WS2812_TIMING_FALLBACK_HZ;

    return (uint32_t)((((uint64_t)hz * ns) + 999999999ULL) / 1000000000ULL);
}

static inline void n32_ws2812_enable_cycle_counter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void n32_ws2812_wait_until(uint32_t start, uint32_t cycles) {
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static void ws2812_send_byte(GPIO_Module *port, uint32_t mask, uint8_t byte, uint32_t t0h, uint32_t t1h, uint32_t total) {
    uint32_t reset_mask = mask << 16;

    for (uint8_t bit = 0; bit < 8; ++bit) {
        uint32_t high_cycles = (byte & (0x80 >> bit)) ? t1h : t0h;
        uint32_t start;

        port->PBSC = mask;
        start      = DWT->CYCCNT;
        n32_ws2812_wait_until(start, high_cycles);
        port->PBSC = reset_mask;
        n32_ws2812_wait_until(start, total);
    }
}

ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

void ws2812_init(void) {
    n32_ws2812_enable_cycle_counter();
    n32_ws2812_tracef("RGB ws2812 init: pin=%s count=%u clock=%lu\n", N32_WS2812_DI_PIN_NAME, (unsigned)WS2812_LED_COUNT, (unsigned long)(SystemCoreClock ? SystemCoreClock : N32_WS2812_TIMING_FALLBACK_HZ));
    gpio_set_pin_output_push_pull(WS2812_DI_PIN);
    gpio_write_pin_low(WS2812_DI_PIN);
    N32_WS2812_TRACE("RGB ws2812 init: output low ok\n");
}

void ws2812_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    ws2812_leds[index].r = red;
    ws2812_leds[index].g = green;
    ws2812_leds[index].b = blue;
#if defined(WS2812_RGBW)
    ws2812_rgb_to_rgbw(&ws2812_leds[index]);
#endif
}

void ws2812_set_color_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (int i = 0; i < WS2812_LED_COUNT; ++i) {
        ws2812_set_color(i, red, green, blue);
    }
}

void ws2812_flush(void) {
    static uint32_t flush_count;
    uint32_t        perf_start = n32_perf_cycles();
    GPIO_Module    *port       = n32_ws2812_port();
    uint32_t        mask       = 1UL << N32_PIN_INDEX(WS2812_DI_PIN);
    uint32_t        t0h        = n32_ws2812_cycles_from_ns(WS2812_T0H);
    uint32_t        t1h        = n32_ws2812_cycles_from_ns(WS2812_T1H);
    uint32_t        total      = n32_ws2812_cycles_from_ns(WS2812_TIMING);

    flush_count++;
    if (flush_count <= 16U || (flush_count % 128U) == 0U) {
#if WS2812_LED_COUNT >= 2
        n32_ws2812_tracef("RGB flush #%lu: led0=(%u,%u,%u) led1=(%u,%u,%u) cyc=(%lu,%lu,%lu)\n", (unsigned long)flush_count, ws2812_leds[0].r, ws2812_leds[0].g, ws2812_leds[0].b, ws2812_leds[1].r, ws2812_leds[1].g, ws2812_leds[1].b, (unsigned long)t0h, (unsigned long)t1h, (unsigned long)total);
#else
        n32_ws2812_tracef("RGB flush #%lu: led0=(%u,%u,%u) cyc=(%lu,%lu,%lu)\n", (unsigned long)flush_count, ws2812_leds[0].r, ws2812_leds[0].g, ws2812_leds[0].b, (unsigned long)t0h, (unsigned long)t1h, (unsigned long)total);
#endif
    }

    n32_ws2812_enable_cycle_counter();
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    for (int i = 0; i < WS2812_LED_COUNT; ++i) {
#if (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_GRB)
        ws2812_send_byte(port, mask, ws2812_leds[i].g, t0h, t1h, total);
        ws2812_send_byte(port, mask, ws2812_leds[i].r, t0h, t1h, total);
        ws2812_send_byte(port, mask, ws2812_leds[i].b, t0h, t1h, total);
#elif (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_RGB)
        ws2812_send_byte(port, mask, ws2812_leds[i].r, t0h, t1h, total);
        ws2812_send_byte(port, mask, ws2812_leds[i].g, t0h, t1h, total);
        ws2812_send_byte(port, mask, ws2812_leds[i].b, t0h, t1h, total);
#elif (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_BGR)
        ws2812_send_byte(port, mask, ws2812_leds[i].b, t0h, t1h, total);
        ws2812_send_byte(port, mask, ws2812_leds[i].g, t0h, t1h, total);
        ws2812_send_byte(port, mask, ws2812_leds[i].r, t0h, t1h, total);
#endif

#ifdef WS2812_RGBW
        ws2812_send_byte(port, mask, ws2812_leds[i].w, t0h, t1h, total);
#endif
    }
    port->PBSC = mask << 16;
    if (primask == 0U) {
        __enable_irq();
    }

    wait_us(WS2812_TRST_US);
    n32_perf_record(N32_PERF_RGB_FLUSH, perf_start);
}
