// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ws2812.h"

#include "gpio.h"
#include "platform_deps.h"

#ifndef WS2812_DI_PIN
#    define WS2812_DI_PIN B7
#endif

#ifndef N32_WS2812_NOP_FUDGE
#    define N32_WS2812_NOP_FUDGE 2
#endif

#define N32_WS2812_NS_TO_LOOPS(ns) (((ns) * N32_WS2812_NOP_FUDGE + 99) / 100)

static inline void ws2812_wait_ns(uint32_t ns) {
    for (volatile uint32_t i = 0; i < N32_WS2812_NS_TO_LOOPS(ns); ++i) {
        __NOP();
    }
}

static void ws2812_send_byte(uint8_t byte) {
    taskENTER_CRITICAL();

    for (uint8_t bit = 0; bit < 8; ++bit) {
        if (byte & (0x80 >> bit)) {
            gpio_write_pin_high(WS2812_DI_PIN);
            ws2812_wait_ns(WS2812_T1H);
            gpio_write_pin_low(WS2812_DI_PIN);
            ws2812_wait_ns(WS2812_T1L);
        } else {
            gpio_write_pin_high(WS2812_DI_PIN);
            ws2812_wait_ns(WS2812_T0H);
            gpio_write_pin_low(WS2812_DI_PIN);
            ws2812_wait_ns(WS2812_T0L);
        }
    }

    taskEXIT_CRITICAL();
}

ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

void ws2812_init(void) {
    gpio_set_pin_output_push_pull(WS2812_DI_PIN);
    gpio_write_pin_low(WS2812_DI_PIN);
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
    for (int i = 0; i < WS2812_LED_COUNT; ++i) {
#if (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_GRB)
        ws2812_send_byte(ws2812_leds[i].g);
        ws2812_send_byte(ws2812_leds[i].r);
        ws2812_send_byte(ws2812_leds[i].b);
#elif (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_RGB)
        ws2812_send_byte(ws2812_leds[i].r);
        ws2812_send_byte(ws2812_leds[i].g);
        ws2812_send_byte(ws2812_leds[i].b);
#elif (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_BGR)
        ws2812_send_byte(ws2812_leds[i].b);
        ws2812_send_byte(ws2812_leds[i].g);
        ws2812_send_byte(ws2812_leds[i].r);
#endif

#ifdef WS2812_RGBW
        ws2812_send_byte(ws2812_leds[i].w);
#endif
    }

    gpio_write_pin_low(WS2812_DI_PIN);
    ws2812_wait_ns(WS2812_TRST_US * 1000UL);
}
