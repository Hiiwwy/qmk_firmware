// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ws2812.h"

#include "n32_perf.h"
#include "platform_deps.h"
#include "wait.h"

#include "misc.h"
#include "n32h47x_48x_dma.h"
#include "n32h47x_48x_tim.h"

#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_RGB)
#    include "SEGGER_RTT.h"
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef WS2812_DI_PIN
#    define WS2812_DI_PIN A8
#endif

#ifndef N32_WS2812_DI_PIN_NAME
#    define N32_WS2812_DI_PIN_NAME "WS2812_DI_PIN"
#endif

#ifndef N32_WS2812_TIMING_FALLBACK_HZ
#    define N32_WS2812_TIMING_FALLBACK_HZ 240000000UL
#endif

#if WS2812_DI_PIN != A8
#    error "N32 WS2812 pwm_dma currently supports PA8/ATIM1_CH1 only. Use N32_WS2812_DRIVER=bitbang for other pins."
#endif

#define N32_WS2812_RESET_SLOTS (((WS2812_TRST_US * 1000U) + WS2812_TIMING - 1U) / WS2812_TIMING + 1U)
#ifdef WS2812_RGBW
#    define N32_WS2812_BYTES_PER_LED 4U
#else
#    define N32_WS2812_BYTES_PER_LED 3U
#endif
#define N32_WS2812_BITS_PER_LED (N32_WS2812_BYTES_PER_LED * 8U)
#define N32_WS2812_PWM_BUFFER_LEN ((WS2812_LED_COUNT * N32_WS2812_BITS_PER_LED) + N32_WS2812_RESET_SLOTS)

static uint16_t      ws2812_pwm_buffer[N32_WS2812_PWM_BUFFER_LEN];
static volatile bool ws2812_dma_busy;

ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

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

static uint32_t n32_ws2812_cycles_from_ns(uint32_t ns) {
    uint32_t hz = SystemCoreClock ? SystemCoreClock : N32_WS2812_TIMING_FALLBACK_HZ;

    return (uint32_t)((((uint64_t)hz * ns) + 999999999ULL) / 1000000000ULL);
}

static void n32_ws2812_gpio_init(void) {
    GPIO_InitType init;

    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA, ENABLE);
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_AFIO, ENABLE);

    GPIO_InitStruct(&init);
    init.Pin            = GPIO_PIN_8;
    init.GPIO_Mode      = GPIO_MODE_AF_PP;
    init.GPIO_Slew_Rate = GPIO_SLEW_RATE_FAST;
    init.GPIO_Current   = GPIO_DC_8mA;
    init.GPIO_Pull      = GPIO_NO_PULL;
    init.GPIO_Alternate = GPIO_AF_ATIM1_CH1_PA8;
    GPIO_InitPeripheral(GPIOA, &init);
}

static void n32_ws2812_timer_init(uint16_t period, uint16_t first_pulse) {
    TIM_TimeBaseInitType time_base;
    OCInitType           oc;
    TIM_BDTRInitType     bdtr;

    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPHEN_ATIM1, ENABLE);

    TIM_Enable(ATIM1, DISABLE);
    TIM_EnableDma(ATIM1, TIM_DMA_UPDATE, DISABLE);
    TIM_DeInit(ATIM1);

    TIM_InitTimBaseStruct(&time_base);
    time_base.Prescaler   = 0;
    time_base.CounterMode = TIM_CNT_MODE_UP;
    time_base.Period      = period;
    time_base.ClkDiv      = TIM_CLK_DIV1;
    time_base.RepetCnt    = 0;
    TIM_InitTimeBase(ATIM1, &time_base);

    TIM_InitOcStruct(&oc);
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.OutputState  = TIM_OUTPUT_STATE_ENABLE;
    oc.OutputNState = TIM_OUTPUT_NSTATE_DISABLE;
    oc.Pulse        = first_pulse;
    oc.OCPolarity   = TIM_OC_POLARITY_HIGH;
    oc.OCNPolarity  = TIM_OCN_POLARITY_HIGH;
    oc.OCIdleState  = TIM_OC_IDLE_STATE_RESET;
    oc.OCNIdleState = TIM_OCN_IDLE_STATE_RESET;
    TIM_InitOc1(ATIM1, &oc);
    TIM_ConfigOc1Preload(ATIM1, TIM_OC_PRE_LOAD_DISABLE);
    TIM_ConfigArPreload(ATIM1, DISABLE);

    TIM_InitBkdtStruct(&bdtr);
    bdtr.OSSRState       = TIM_OSSR_STATE_ENABLE;
    bdtr.OSSIState       = TIM_OSSI_STATE_ENABLE;
    bdtr.LOCKLevel       = TIM_LOCK_LEVEL_OFF;
    bdtr.DeadTime        = 0;
    bdtr.Break           = TIM_BREAK_IN_DISABLE;
    bdtr.BreakPolarity   = TIM_BREAK_POLARITY_HIGH;
    bdtr.AutomaticOutput = TIM_AUTO_OUTPUT_DISABLE;
    TIM_ConfigBkdt(ATIM1, &bdtr);

    TIM_EnableCtrlPwmOutputs(ATIM1, ENABLE);
}

static void n32_ws2812_dma_init(void) {
    NVIC_InitType nvic;

    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPHEN_DMA1, ENABLE);

    DMA_EnableChannel(DMA1_CH1, DISABLE);
    DMA_DeInit(DMA1_CH1);
    DMA_ClrIntPendingBit(DMA_INT_GLB1, DMA1);

    DMA_RequestRemap(DMA_REMAP_ATIM1_UP, DMA1_CH1, ENABLE);
    DMA_ConfigInt(DMA1_CH1, DMA_INT_TXC | DMA_INT_ERR, ENABLE);

    nvic.NVIC_IRQChannel                   = DMA1_Channel1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 5;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

static uint16_t n32_ws2812_pulse_from_ns(uint32_t ns) {
    uint32_t cycles = n32_ws2812_cycles_from_ns(ns);

    if (cycles > 0xFFFFU) {
        cycles = 0xFFFFU;
    }
    return (uint16_t)cycles;
}

static void n32_ws2812_encode_byte(uint16_t **cursor, uint8_t value, uint16_t t0h, uint16_t t1h) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        *(*cursor)++ = (value & (0x80U >> bit)) ? t1h : t0h;
    }
}

static uint16_t n32_ws2812_build_pwm_buffer(void) {
    uint16_t *cursor = ws2812_pwm_buffer;
    uint16_t  t0h    = n32_ws2812_pulse_from_ns(WS2812_T0H);
    uint16_t  t1h    = n32_ws2812_pulse_from_ns(WS2812_T1H);

    for (int i = 0; i < WS2812_LED_COUNT; ++i) {
#if (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_GRB)
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].g, t0h, t1h);
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].r, t0h, t1h);
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].b, t0h, t1h);
#elif (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_RGB)
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].r, t0h, t1h);
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].g, t0h, t1h);
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].b, t0h, t1h);
#elif (WS2812_BYTE_ORDER == WS2812_BYTE_ORDER_BGR)
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].b, t0h, t1h);
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].g, t0h, t1h);
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].r, t0h, t1h);
#endif
#ifdef WS2812_RGBW
        n32_ws2812_encode_byte(&cursor, ws2812_leds[i].w, t0h, t1h);
#endif
    }

    for (uint16_t i = 0; i < N32_WS2812_RESET_SLOTS; ++i) {
        *cursor++ = 0;
    }

    return (uint16_t)(cursor - ws2812_pwm_buffer);
}

static void n32_ws2812_start_dma(uint16_t length) {
    DMA_InitType dma;

    TIM_Enable(ATIM1, DISABLE);
    TIM_EnableDma(ATIM1, TIM_DMA_UPDATE, DISABLE);
    DMA_EnableChannel(DMA1_CH1, DISABLE);
    DMA_ClrIntPendingBit(DMA_INT_GLB1, DMA1);

    TIM_SetCnt(ATIM1, 0);
    TIM_SetCmp1(ATIM1, ws2812_pwm_buffer[0]);

    DMA_StructInit(&dma);
    dma.PeriphAddr     = (uint32_t)&ATIM1->CCDAT1;
    dma.MemAddr        = (uint32_t)&ws2812_pwm_buffer[1];
    dma.Direction      = DMA_DIR_PERIPH_DST;
    dma.BufSize        = length - 1U;
    dma.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    dma.MemoryInc      = DMA_MEM_INC_ENABLE;
    dma.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_HALFWORD;
    dma.MemDataSize    = DMA_MEM_DATA_WIDTH_HALFWORD;
    dma.CircularMode   = DMA_MODE_NORMAL;
    dma.Priority       = DMA_PRIORITY_VERY_HIGH;
    dma.Mem2Mem        = DMA_M2M_DISABLE;
    dma.BURST_BYPASS   = DMA_BURST_BYPASS_ENABLE;
    dma.BURST_MODE     = DMA_BURST_MODE_ALMOST;
    dma.BURST_LEN      = DMA_BURST_LEN_SINGLE;
    DMA_Init(DMA1_CH1, &dma);
    DMA_RequestRemap(DMA_REMAP_ATIM1_UP, DMA1_CH1, ENABLE);
    DMA_ConfigInt(DMA1_CH1, DMA_INT_TXC | DMA_INT_ERR, ENABLE);

    ws2812_dma_busy = true;
    DMA_EnableChannel(DMA1_CH1, ENABLE);
    TIM_EnableDma(ATIM1, TIM_DMA_UPDATE, ENABLE);
    TIM_Enable(ATIM1, ENABLE);
}

void ws2812_init(void) {
    uint16_t period = n32_ws2812_pulse_from_ns(WS2812_TIMING);
    uint16_t t0h    = n32_ws2812_pulse_from_ns(WS2812_T0H);

    if (period > 0) {
        period--;
    }

    n32_ws2812_gpio_init();
    n32_ws2812_timer_init(period, 0);
    n32_ws2812_dma_init();
    n32_ws2812_tracef("RGB ws2812 pwm_dma init: pin=%s count=%u period=%u t0=%u t1=%u reset_slots=%u\n", N32_WS2812_DI_PIN_NAME, (unsigned)WS2812_LED_COUNT, (unsigned)period, (unsigned)t0h, (unsigned)n32_ws2812_pulse_from_ns(WS2812_T1H), (unsigned)N32_WS2812_RESET_SLOTS);
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
    uint32_t perf_start = n32_perf_cycles();

    if (!ws2812_dma_busy) {
        uint16_t length = n32_ws2812_build_pwm_buffer();
        n32_ws2812_start_dma(length);
    }

    n32_perf_record(N32_PERF_RGB_FLUSH, perf_start);
}

void DMA1_Channel1_IRQHandler(void) {
    if (DMA_GetIntStatus(DMA_INT_TXC1, DMA1) || DMA_GetIntStatus(DMA_INT_ERR1, DMA1)) {
        DMA_ClrIntPendingBit(DMA_INT_GLB1, DMA1);
        DMA_EnableChannel(DMA1_CH1, DISABLE);
        TIM_EnableDma(ATIM1, TIM_DMA_UPDATE, DISABLE);
        TIM_SetCmp1(ATIM1, 0);
        ws2812_dma_busy = false;
    }
}
