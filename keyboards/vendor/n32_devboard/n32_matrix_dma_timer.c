// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "matrix.h"
#include "n32_perf.h"
#include "platform_deps.h"

#include "misc.h"
#include "n32h47x_48x_dma.h"
#include "n32h47x_48x_tim.h"

#if defined(N32_TRACE_MATRIX) || defined(N32_TRACE_MATRIX_EVENT)
#    include "SEGGER_RTT.h"
#    include "timer.h"
#    include <stdio.h>
#endif

#if DIODE_DIRECTION != ROW2COL
#    error "n32_matrix_dma_timer.c currently supports ROW2COL only."
#endif

#if MATRIX_ROWS != 5 || MATRIX_COLS != 6
#    error "n32_matrix_dma_timer.c is specialized for the current N32 devboard 5x6 matrix."
#endif

#ifndef N32_MATRIX_DMA_COLUMN_US
#    define N32_MATRIX_DMA_COLUMN_US 10U
#endif

#ifndef N32_MATRIX_DMA_SWITCH_DEAD_CYCLES
#    define N32_MATRIX_DMA_SWITCH_DEAD_CYCLES 240U
#endif

#ifndef N32_MATRIX_DMA_SAMPLE_PERCENT
#    define N32_MATRIX_DMA_SAMPLE_PERCENT 90U
#endif

#ifndef N32_MATRIX_DMA_SAMPLE_PHASE
#    define N32_MATRIX_DMA_SAMPLE_PHASE 0
#endif

#ifndef N32_MATRIX_DMA_TIMER_FALLBACK_HZ
#    define N32_MATRIX_DMA_TIMER_FALLBACK_HZ 240000000UL
#endif

#ifndef N32_MATRIX_DMA_PRESS_CONFIRM_FRAMES
#    define N32_MATRIX_DMA_PRESS_CONFIRM_FRAMES 2U
#endif

#ifndef N32_MATRIX_DMA_RELEASE_CONFIRM_FRAMES
#    define N32_MATRIX_DMA_RELEASE_CONFIRM_FRAMES 8U
#endif

#ifndef N32_MATRIX_DMA_HW_COLUMNS
#    define N32_MATRIX_DMA_HW_COLUMNS 0
#endif

#define N32_MATRIX_DMA_COLS MATRIX_COLS
#define N32_MATRIX_DMA_ROWS MATRIX_ROWS
#define N32_MATRIX_GPIO_PORT_COUNT 6U
#define N32_MATRIX_DMA_HW_COLUMNS_DUAL_PORT (N32_MATRIX_DMA_HW_COLUMNS == 1)
#define N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT (N32_MATRIX_DMA_HW_COLUMNS == 2)
#define N32_MATRIX_DMA_HW_COLUMNS_ENABLED (N32_MATRIX_DMA_HW_COLUMNS != 0)

static const pin_t        n32_row_pins[N32_MATRIX_DMA_ROWS]          = MATRIX_ROW_PINS;
static const pin_t        n32_col_pins[N32_MATRIX_DMA_COLS]          = MATRIX_COL_PINS;
static GPIO_Module *const n32_gpio_ports[N32_MATRIX_GPIO_PORT_COUNT] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF};

uint8_t        n32_get_matrix_dma_press_confirm_frames(void);
uint8_t        n32_get_matrix_dma_release_confirm_frames(void);
static uint8_t n32_matrix_press_confirm_frames(void);
static uint8_t n32_matrix_release_confirm_frames(void);

#if !N32_MATRIX_DMA_HW_COLUMNS_ENABLED
static volatile uint32_t n32_dma_gpioa;
static volatile uint32_t n32_dma_gpiob;
static volatile uint32_t n32_dma_gpioc;
static volatile uint32_t n32_dma_gpiod;
#endif
static volatile uint32_t     n32_sample_gpioa[N32_MATRIX_DMA_COLS];
static volatile uint32_t     n32_sample_gpiob[N32_MATRIX_DMA_COLS];
static volatile uint32_t     n32_sample_gpioc[N32_MATRIX_DMA_COLS];
static volatile uint32_t     n32_sample_gpiod[N32_MATRIX_DMA_COLS];
#if N32_MATRIX_DMA_HW_COLUMNS_ENABLED
static volatile uint32_t n32_frame_gpioa[N32_MATRIX_DMA_COLS];
static volatile uint32_t n32_frame_gpiob[N32_MATRIX_DMA_COLS];
static volatile uint32_t n32_frame_gpioc[N32_MATRIX_DMA_COLS];
static volatile uint32_t n32_frame_gpiod[N32_MATRIX_DMA_COLS];
#endif
static uint8_t               n32_dma_press_counts[N32_MATRIX_DMA_ROWS][N32_MATRIX_DMA_COLS];
static uint8_t               n32_dma_release_counts[N32_MATRIX_DMA_ROWS][N32_MATRIX_DMA_COLS];
static volatile matrix_row_t n32_dma_rows[N32_MATRIX_DMA_ROWS];
static matrix_row_t          n32_qmk_rows[N32_MATRIX_DMA_ROWS];
static volatile uint8_t      n32_dma_samples[N32_MATRIX_DMA_COLS];
static volatile uint32_t     n32_dma_frame_generation;
static volatile uint32_t     n32_dma_frame_count;
static volatile uint8_t      n32_dma_active_col;
static GPIO_Module          *n32_col_ports[N32_MATRIX_DMA_COLS];
static uint16_t              n32_col_masks[N32_MATRIX_DMA_COLS];
static uint16_t              n32_col_release_masks[N32_MATRIX_GPIO_PORT_COUNT];
static uint32_t              n32_col_pmode_clear_masks[N32_MATRIX_GPIO_PORT_COUNT];
static uint32_t              n32_col_pmode_output_bits[N32_MATRIX_GPIO_PORT_COUNT];
#if N32_MATRIX_DMA_HW_COLUMNS_DUAL_PORT
static uint32_t n32_col_dma_gpioa[N32_MATRIX_DMA_COLS];
static uint32_t n32_col_dma_gpioc[N32_MATRIX_DMA_COLS];
#elif N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT
static uint32_t     n32_col_dma_gpio[N32_MATRIX_DMA_COLS];
static GPIO_Module *n32_col_dma_port;
static uint8_t      n32_col_dma_port_index;
static bool         n32_col_dma_valid;
#endif

#ifdef N32_TRACE_MATRIX
static uint32_t n32_dma_trace_timer;
static uint32_t n32_dma_trace_generation;
#endif

#ifdef N32_TRACE_MATRIX_EVENT
static matrix_row_t          n32_dma_event_rows[N32_MATRIX_DMA_ROWS];
static volatile bool         n32_dma_event_pending;
static volatile matrix_row_t n32_dma_event_pending_rows[N32_MATRIX_DMA_ROWS];
static volatile uint8_t      n32_dma_event_pending_samples[N32_MATRIX_DMA_COLS];
static volatile uint8_t      n32_dma_event_pending_active_col;
#endif

static void n32_matrix_trace_literal(const char *text) {
#ifdef N32_TRACE_MATRIX
    unsigned length = 0;
    while (text[length] != '\0') {
        length++;
    }
    SEGGER_RTT_Write(0, text, length);
#else
    (void)text;
#endif
}

static uint8_t n32_matrix_sample_bits_from(uint8_t col, const volatile uint32_t gpioa[N32_MATRIX_DMA_COLS], const volatile uint32_t gpiob[N32_MATRIX_DMA_COLS], const volatile uint32_t gpioc[N32_MATRIX_DMA_COLS], const volatile uint32_t gpiod[N32_MATRIX_DMA_COLS]) {
    uint8_t bits = 0;

    if ((gpiob[col] & (1UL << 0)) == 0) {
        bits |= (1U << 0);
    }
    if ((gpiob[col] & (1UL << 2)) == 0) {
        bits |= (1U << 1);
    }
    if ((gpiod[col] & (1UL << 4)) == 0) {
        bits |= (1U << 2);
    }
    if ((gpioa[col] & (1UL << 6)) == 0) {
        bits |= (1U << 3);
    }
    if ((gpioc[col] & (1UL << 4)) == 0) {
        bits |= (1U << 4);
    }

    return bits;
}

static uint8_t n32_matrix_sample_bits(uint8_t col) {
#if N32_MATRIX_DMA_HW_COLUMNS_ENABLED
    return n32_matrix_sample_bits_from(col, n32_frame_gpioa, n32_frame_gpiob, n32_frame_gpioc, n32_frame_gpiod);
#else
    return n32_matrix_sample_bits_from(col, n32_sample_gpioa, n32_sample_gpiob, n32_sample_gpioc, n32_sample_gpiod);
#endif
}

static void n32_matrix_trace_event_capture(const matrix_row_t rows[N32_MATRIX_DMA_ROWS]) {
#ifdef N32_TRACE_MATRIX_EVENT
    bool changed = false;

    for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
        if (n32_dma_event_rows[row] != rows[row]) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return;
    }
    for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
        n32_dma_event_rows[row]         = rows[row];
        n32_dma_event_pending_rows[row] = rows[row];
    }
    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col) {
        n32_dma_event_pending_samples[col] = n32_dma_samples[col];
    }
    n32_dma_event_pending_active_col = n32_dma_active_col;
    n32_dma_event_pending            = true;
#endif
}

static void n32_matrix_trace_event_flush(void) {
#ifdef N32_TRACE_MATRIX_EVENT
    if (!n32_dma_event_pending) {
        return;
    }

    matrix_row_t rows[N32_MATRIX_DMA_ROWS];
    uint8_t      samples[N32_MATRIX_DMA_COLS];
    uint8_t      active_col;

    __disable_irq();
    for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
        rows[row] = n32_dma_event_pending_rows[row];
    }
    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col) {
        samples[col] = n32_dma_event_pending_samples[col];
    }
    active_col            = n32_dma_event_pending_active_col;
    n32_dma_event_pending = false;
    __enable_irq();

    char buffer[256];
    int  offset = snprintf(buffer, sizeof(buffer), "MATRIX event rows=%03lX/%03lX/%03lX/%03lX/%03lX active=%u s=", (unsigned long)rows[0], (unsigned long)rows[1], (unsigned long)rows[2], (unsigned long)rows[3], (unsigned long)rows[4], (unsigned)active_col);

    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS && offset > 0 && offset < (int)sizeof(buffer); ++col) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%02X", (unsigned)samples[col]);
        if (col + 1U < N32_MATRIX_DMA_COLS && offset > 0 && offset < (int)sizeof(buffer)) {
            buffer[offset++] = ',';
        }
    }

    if (offset > 0 && offset < (int)sizeof(buffer)) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " cu=%u dc=%u sp=%u ph=%d pf=%u rf=%u hc=%u pod=%04lX/%04lX/%04lX\n", (unsigned)N32_MATRIX_DMA_COLUMN_US, (unsigned)N32_MATRIX_DMA_SWITCH_DEAD_CYCLES, (unsigned)N32_MATRIX_DMA_SAMPLE_PERCENT, (int)N32_MATRIX_DMA_SAMPLE_PHASE, (unsigned)n32_matrix_press_confirm_frames(), (unsigned)n32_matrix_release_confirm_frames(), (unsigned)N32_MATRIX_DMA_HW_COLUMNS, (unsigned long)(GPIOA->POD & 0xFFFFU), (unsigned long)(GPIOB->POD & 0xFFFFU), (unsigned long)(GPIOC->POD & 0xFFFFU));
    }
    if (offset > 0) {
        SEGGER_RTT_Write(0, buffer, offset < (int)sizeof(buffer) ? (unsigned)offset : sizeof(buffer) - 1U);
    }
#endif
}

static GPIO_Module *n32_matrix_port(pin_t pin) {
    switch (N32_PIN_PORT(pin)) {
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

static uint32_t n32_matrix_clock(pin_t pin) {
    switch (N32_PIN_PORT(pin)) {
        case N32_PORT_A:
            return RCC_AHB_PERIPHEN_GPIOA;
        case N32_PORT_B:
            return RCC_AHB_PERIPHEN_GPIOB;
        case N32_PORT_C:
            return RCC_AHB_PERIPHEN_GPIOC;
        case N32_PORT_D:
            return RCC_AHB_PERIPHEN_GPIOD;
        case N32_PORT_E:
            return RCC_AHB_PERIPHEN_GPIOE;
        case N32_PORT_F:
            return RCC_AHB_PERIPHEN_GPIOF;
        case N32_PORT_G:
            return RCC_AHB_PERIPHEN_GPIOG;
        case N32_PORT_H:
            return RCC_AHB_PERIPHEN_GPIOH;
        default:
            return RCC_AHB_PERIPHEN_GPIOA;
    }
}

static uint16_t n32_matrix_mask(pin_t pin) {
    return (uint16_t)(1U << N32_PIN_INDEX(pin));
}

static uint8_t n32_matrix_port_index(pin_t pin) {
    switch (N32_PIN_PORT(pin)) {
        case N32_PORT_A:
            return 0;
        case N32_PORT_B:
            return 1;
        case N32_PORT_C:
            return 2;
        case N32_PORT_D:
            return 3;
        case N32_PORT_E:
            return 4;
        case N32_PORT_F:
            return 5;
        default:
            return 0;
    }
}

static inline void n32_matrix_wait_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static void n32_matrix_gpio_input_pullup(pin_t pin) {
    GPIO_InitType init;

    RCC_EnableAHB1PeriphClk(n32_matrix_clock(pin), ENABLE);
    GPIO_InitStruct(&init);
    init.Pin       = n32_matrix_mask(pin);
    init.GPIO_Mode = GPIO_MODE_INPUT;
    init.GPIO_Pull = GPIO_PULL_UP;
    GPIO_InitPeripheral(n32_matrix_port(pin), &init);
}

static void n32_matrix_prepare_col_fastio(void) {
    for (uint8_t port = 0; port < N32_MATRIX_GPIO_PORT_COUNT; ++port) {
        n32_col_release_masks[port]     = 0;
        n32_col_pmode_clear_masks[port] = 0;
        n32_col_pmode_output_bits[port] = 0;
    }

    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col) {
        uint8_t  port_index = n32_matrix_port_index(n32_col_pins[col]);
        uint16_t mask       = n32_matrix_mask(n32_col_pins[col]);
        uint32_t shift      = (uint32_t)N32_PIN_INDEX(n32_col_pins[col]) * 2U;

        n32_col_ports[col] = n32_gpio_ports[port_index];
        n32_col_masks[col] = mask;
        n32_col_release_masks[port_index] |= mask;
        n32_col_pmode_clear_masks[port_index] |= (GPIO_PMODE_BIT_MASK << shift);
        n32_col_pmode_output_bits[port_index] |= (GPIO_PMODE_OUTPUT << shift);
    }

#if N32_MATRIX_DMA_HW_COLUMNS_DUAL_PORT
    for (uint8_t step = 0; step < N32_MATRIX_DMA_COLS; ++step) {
        uint8_t  col        = step + 1U;
        uint32_t gpioa_pbsc = n32_col_release_masks[0];
        uint32_t gpioc_pbsc = n32_col_release_masks[2];

        if (col >= N32_MATRIX_DMA_COLS) {
            col = 0;
        }

        if (n32_col_ports[col] == GPIOA) {
            gpioa_pbsc |= (uint32_t)n32_col_masks[col] << 16;
        } else if (n32_col_ports[col] == GPIOC) {
            gpioc_pbsc |= (uint32_t)n32_col_masks[col] << 16;
        }

        n32_col_dma_gpioa[step] = gpioa_pbsc;
        n32_col_dma_gpioc[step] = gpioc_pbsc;
    }
#elif N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT
    n32_col_dma_valid      = true;
    n32_col_dma_port_index = n32_matrix_port_index(n32_col_pins[0]);
    n32_col_dma_port       = n32_gpio_ports[n32_col_dma_port_index];

    for (uint8_t col = 1; col < N32_MATRIX_DMA_COLS; ++col) {
        if (n32_matrix_port_index(n32_col_pins[col]) != n32_col_dma_port_index) {
            n32_col_dma_valid = false;
        }
    }

    if (n32_col_dma_valid) {
        uint16_t all_cols_mask = n32_col_release_masks[n32_col_dma_port_index];

        for (uint8_t step = 0; step < N32_MATRIX_DMA_COLS; ++step) {
            uint8_t col = step + 1U;
            if (col >= N32_MATRIX_DMA_COLS) {
                col = 0;
            }
            uint16_t selected = n32_col_masks[col];
            n32_col_dma_gpio[step] = (uint32_t)(all_cols_mask & (uint16_t)~selected) | ((uint32_t)selected << 16);
        }
    } else {
        for (uint8_t step = 0; step < N32_MATRIX_DMA_COLS; ++step) {
            n32_col_dma_gpio[step] = n32_col_release_masks[n32_col_dma_port_index];
        }
    }
#endif
}

static void n32_matrix_release_all_cols(void) {
    for (uint8_t port_index = 0; port_index < N32_MATRIX_GPIO_PORT_COUNT; ++port_index) {
        uint16_t mask = n32_col_release_masks[port_index];

        if (mask != 0) {
            GPIO_Module *port = n32_gpio_ports[port_index];

            port->PBSC  = mask;
            port->PMODE = (port->PMODE & ~n32_col_pmode_clear_masks[port_index]) | n32_col_pmode_output_bits[port_index];
        }
    }
}

static void n32_matrix_apply_col(uint8_t col) {
    n32_matrix_release_all_cols();
    n32_matrix_wait_cycles(N32_MATRIX_DMA_SWITCH_DEAD_CYCLES);

    if (col < N32_MATRIX_DMA_COLS) {
        n32_col_ports[col]->PBC = n32_col_masks[col];
    }
}

static uint8_t n32_matrix_press_confirm_frames(void) {
    uint8_t frames = n32_get_matrix_dma_press_confirm_frames();
    return frames == 0 ? 1U : frames;
}

static uint8_t n32_matrix_release_confirm_frames(void) {
    uint8_t frames = n32_get_matrix_dma_release_confirm_frames();
    return frames == 0 ? 1U : frames;
}

static void n32_matrix_decode_frame(void) {
    matrix_row_t rows[N32_MATRIX_DMA_ROWS]          = {0};
    matrix_row_t filtered_rows[N32_MATRIX_DMA_ROWS] = {0};
    uint8_t      press_confirm_frames               = n32_matrix_press_confirm_frames();
    uint8_t      release_confirm_frames             = n32_matrix_release_confirm_frames();

    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col) {
        matrix_row_t col_bit = (matrix_row_t)1U << col;

        uint8_t sample       = n32_matrix_sample_bits(col);
        n32_dma_samples[col] = sample;

        if ((sample & (1U << 0)) != 0) {
            rows[0] |= col_bit;
        }
        if ((sample & (1U << 1)) != 0) {
            rows[1] |= col_bit;
        }
        if ((sample & (1U << 2)) != 0) {
            rows[2] |= col_bit;
        }
        if ((sample & (1U << 3)) != 0) {
            rows[3] |= col_bit;
        }
        if ((sample & (1U << 4)) != 0) {
            rows[4] |= col_bit;
        }
    }

    for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
        matrix_row_t mask = MATRIX_ROW_SHIFTER;

        for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col, mask <<= 1) {
            bool raw_pressed      = (rows[row] & mask) != 0;
            bool filtered_pressed = (n32_dma_rows[row] & mask) != 0;

            if (raw_pressed) {
                n32_dma_release_counts[row][col] = 0;
                if (filtered_pressed) {
                    n32_dma_press_counts[row][col] = press_confirm_frames;
                    filtered_rows[row] |= mask;
                } else {
                    if (n32_dma_press_counts[row][col] < press_confirm_frames) {
                        n32_dma_press_counts[row][col]++;
                    }
                    if (n32_dma_press_counts[row][col] >= press_confirm_frames) {
                        filtered_rows[row] |= mask;
                    }
                }
                if (press_confirm_frames <= 1U) {
                    n32_dma_press_counts[row][col] = 1U;
                    filtered_rows[row] |= mask;
                }
            } else {
                n32_dma_press_counts[row][col] = 0;
                if (!filtered_pressed) {
                    n32_dma_release_counts[row][col] = 0;
                    continue;
                }
                if (release_confirm_frames <= 1U) {
                    n32_dma_release_counts[row][col] = 0;
                } else {
                    if (n32_dma_release_counts[row][col] < release_confirm_frames) {
                        n32_dma_release_counts[row][col]++;
                    }
                    if (n32_dma_release_counts[row][col] < release_confirm_frames) {
                        filtered_rows[row] |= mask;
                    }
                }
            }
        }
        n32_dma_rows[row] = filtered_rows[row];
    }
    n32_dma_frame_count++;
    n32_dma_frame_generation++;
    n32_matrix_trace_event_capture(filtered_rows);
}

#if !N32_MATRIX_DMA_HW_COLUMNS_ENABLED
static void n32_matrix_store_active_sample(void) {
    int8_t col = (int8_t)n32_dma_active_col + (int8_t)N32_MATRIX_DMA_SAMPLE_PHASE;

    while (col < 0) {
        col += N32_MATRIX_DMA_COLS;
    }
    while (col >= N32_MATRIX_DMA_COLS) {
        col -= N32_MATRIX_DMA_COLS;
    }

    if (col >= N32_MATRIX_DMA_COLS) {
        return;
    }

    uint8_t sample_col = (uint8_t)col;

    n32_sample_gpioa[sample_col] = n32_dma_gpioa;
    n32_sample_gpiob[sample_col] = n32_dma_gpiob;
    n32_sample_gpioc[sample_col] = n32_dma_gpioc;
    n32_sample_gpiod[sample_col] = n32_dma_gpiod;
}
#endif

#if N32_MATRIX_DMA_HW_COLUMNS_ENABLED
static void n32_matrix_capture_frame(void) {
    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col) {
        n32_frame_gpioa[col] = n32_sample_gpioa[col];
        n32_frame_gpiob[col] = n32_sample_gpiob[col];
        n32_frame_gpioc[col] = n32_sample_gpioc[col];
        n32_frame_gpiod[col] = n32_sample_gpiod[col];
    }
}
#endif

static void n32_matrix_snapshot_rows(void) {
    __disable_irq();
    for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
        n32_qmk_rows[row] = n32_dma_rows[row];
    }
    __enable_irq();
}

static uint32_t n32_matrix_timer_ticks(void) {
    uint32_t hz    = SystemCoreClock ? SystemCoreClock : N32_MATRIX_DMA_TIMER_FALLBACK_HZ;
    uint32_t ticks = (uint32_t)((((uint64_t)hz * N32_MATRIX_DMA_COLUMN_US) + 999999ULL) / 1000000ULL);

    if (ticks < 16U) {
        ticks = 16U;
    }
    if (ticks > 0xFFFFU) {
        ticks = 0xFFFFU;
    }

    return ticks;
}

static void n32_matrix_dma_init_channel(DMA_ChannelType *channel, uint32_t remap, volatile uint32_t *periph, volatile uint32_t *memory, uint32_t direction, uint16_t buf_size, uint32_t memory_inc) {
    DMA_InitType dma;

    DMA_EnableChannel(channel, DISABLE);
    DMA_DeInit(channel);
    DMA_StructInit(&dma);
    dma.PeriphAddr     = (uint32_t)periph;
    dma.MemAddr        = (uint32_t)memory;
    dma.Direction      = direction;
    dma.BufSize        = buf_size;
    dma.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    dma.MemoryInc      = memory_inc;
    dma.PeriphDataSize = DMA_PERIPH_DATA_WIDTH_WORD;
    dma.MemDataSize    = DMA_MEM_DATA_WIDTH_WORD;
    dma.CircularMode   = DMA_MODE_CIRCULAR;
    dma.Priority       = DMA_PRIORITY_HIGH;
    dma.Mem2Mem        = DMA_M2M_DISABLE;
    dma.BURST_BYPASS   = DMA_BURST_BYPASS_ENABLE;
    dma.BURST_MODE     = DMA_BURST_MODE_ALMOST;
    dma.BURST_LEN      = DMA_BURST_LEN_SINGLE;
    DMA_Init(channel, &dma);
    DMA_RequestRemap(remap, channel, ENABLE);
}

static void n32_matrix_dma_init(void) {
    n32_matrix_trace_literal("MATRIX dma_timer dma init\n");

    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPHEN_DMA2, ENABLE);
    DMA_ClrIntPendingBit(DMA_INT_GLB1 | DMA_INT_GLB2 | DMA_INT_GLB3 | DMA_INT_GLB4 | DMA_INT_GLB5 | DMA_INT_GLB6, DMA2);

#if N32_MATRIX_DMA_HW_COLUMNS_DUAL_PORT || N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT
    n32_matrix_dma_init_channel(DMA2_CH1, DMA_REMAP_ATIM2_CH1, &GPIOA->PID, &n32_sample_gpioa[0], DMA_DIR_PERIPH_SRC, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
    n32_matrix_dma_init_channel(DMA2_CH2, DMA_REMAP_ATIM2_CH2, &GPIOB->PID, &n32_sample_gpiob[0], DMA_DIR_PERIPH_SRC, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
    n32_matrix_dma_init_channel(DMA2_CH3, DMA_REMAP_ATIM2_CH3, &GPIOC->PID, &n32_sample_gpioc[0], DMA_DIR_PERIPH_SRC, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
    n32_matrix_dma_init_channel(DMA2_CH4, DMA_REMAP_ATIM2_CH4, &GPIOD->PID, &n32_sample_gpiod[0], DMA_DIR_PERIPH_SRC, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
#    if N32_MATRIX_DMA_HW_COLUMNS_DUAL_PORT
    n32_matrix_dma_init_channel(DMA2_CH5, DMA_REMAP_ATIM2_UP, &GPIOA->PBSC, &n32_col_dma_gpioa[0], DMA_DIR_PERIPH_DST, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
    n32_matrix_dma_init_channel(DMA2_CH6, DMA_REMAP_ATIM2_UP, &GPIOC->PBSC, &n32_col_dma_gpioc[0], DMA_DIR_PERIPH_DST, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
#    elif N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT
    if (n32_col_dma_valid) {
        n32_matrix_dma_init_channel(DMA2_CH5, DMA_REMAP_ATIM2_UP, &n32_col_dma_port->PBSC, &n32_col_dma_gpio[0], DMA_DIR_PERIPH_DST, N32_MATRIX_DMA_COLS, DMA_MEM_INC_ENABLE);
    }
#    endif
    DMA_ConfigInt(DMA2_CH4, DMA_INT_TXC | DMA_INT_ERR, ENABLE);
#else
    n32_matrix_dma_init_channel(DMA2_CH1, DMA_REMAP_ATIM2_CH1, &GPIOA->PID, &n32_dma_gpioa, DMA_DIR_PERIPH_SRC, 1, DMA_MEM_INC_DISABLE);
    n32_matrix_dma_init_channel(DMA2_CH2, DMA_REMAP_ATIM2_CH2, &GPIOB->PID, &n32_dma_gpiob, DMA_DIR_PERIPH_SRC, 1, DMA_MEM_INC_DISABLE);
    n32_matrix_dma_init_channel(DMA2_CH3, DMA_REMAP_ATIM2_CH3, &GPIOC->PID, &n32_dma_gpioc, DMA_DIR_PERIPH_SRC, 1, DMA_MEM_INC_DISABLE);
    n32_matrix_dma_init_channel(DMA2_CH4, DMA_REMAP_ATIM2_CH4, &GPIOD->PID, &n32_dma_gpiod, DMA_DIR_PERIPH_SRC, 1, DMA_MEM_INC_DISABLE);
#endif
    DMA_EnableChannel(DMA2_CH1, ENABLE);
    DMA_EnableChannel(DMA2_CH2, ENABLE);
    DMA_EnableChannel(DMA2_CH3, ENABLE);
    DMA_EnableChannel(DMA2_CH4, ENABLE);
#if N32_MATRIX_DMA_HW_COLUMNS_DUAL_PORT
    DMA_EnableChannel(DMA2_CH5, ENABLE);
    DMA_EnableChannel(DMA2_CH6, ENABLE);
#elif N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT
    if (n32_col_dma_valid) {
        DMA_EnableChannel(DMA2_CH5, ENABLE);
    }
#endif

    n32_matrix_trace_literal("MATRIX dma_timer dma enabled\n");
}

static void n32_matrix_timer_init(void) {
    n32_matrix_trace_literal("MATRIX dma_timer timer init\n");

    TIM_TimeBaseInitType time_base;
    NVIC_InitType        nvic;
    uint32_t             ticks        = n32_matrix_timer_ticks();
    uint32_t             compare_tick = (ticks * N32_MATRIX_DMA_SAMPLE_PERCENT) / 100U;

    if (compare_tick < 2U) {
        compare_tick = 2U;
    }
    if ((compare_tick + 3U) >= ticks) {
        compare_tick = ticks - 4U;
    }

    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPHEN_ATIM2, ENABLE);

    TIM_Enable(ATIM2, DISABLE);
    TIM_EnableDma(ATIM2, TIM_DMA_CC1 | TIM_DMA_CC2 | TIM_DMA_CC3 | TIM_DMA_CC4 | TIM_DMA_UPDATE, DISABLE);
    TIM_ConfigInt(ATIM2, TIM_INT_UPDATE, DISABLE);
    TIM_DeInit(ATIM2);

    TIM_InitTimBaseStruct(&time_base);
    time_base.Prescaler   = 0;
    time_base.CounterMode = TIM_CNT_MODE_UP;
    time_base.Period      = ticks - 1U;
    time_base.ClkDiv      = TIM_CLK_DIV1;
    time_base.RepetCnt    = 0;
    TIM_InitTimeBase(ATIM2, &time_base);
    TIM_SetCmp1(ATIM2, (uint16_t)compare_tick);
    TIM_SetCmp2(ATIM2, (uint16_t)(compare_tick + 1U));
    TIM_SetCmp3(ATIM2, (uint16_t)(compare_tick + 2U));
    TIM_SetCmp4(ATIM2, (uint16_t)(compare_tick + 3U));
    TIM_SetCnt(ATIM2, 0);
    TIM_ClrIntPendingBit(ATIM2, TIM_INT_UPDATE | TIM_INT_CC1 | TIM_INT_CC2 | TIM_INT_CC3 | TIM_INT_CC4);

#if N32_MATRIX_DMA_HW_COLUMNS_ENABLED
    nvic.NVIC_IRQChannel = DMA2_Channel4_IRQn;
#else
    nvic.NVIC_IRQChannel = ATIM2_UP_IRQn;
#endif
    nvic.NVIC_IRQChannelPreemptionPriority = 5;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

#if N32_MATRIX_DMA_HW_COLUMNS_ENABLED
    TIM_ConfigInt(ATIM2, TIM_INT_UPDATE, DISABLE);
    TIM_EnableDma(ATIM2, TIM_DMA_CC1 | TIM_DMA_CC2 | TIM_DMA_CC3 | TIM_DMA_CC4 | TIM_DMA_UPDATE, ENABLE);
#else
    TIM_ConfigInt(ATIM2, TIM_INT_UPDATE, ENABLE);
    TIM_EnableDma(ATIM2, TIM_DMA_CC1 | TIM_DMA_CC2 | TIM_DMA_CC3 | TIM_DMA_CC4, ENABLE);
#endif

    n32_matrix_trace_literal("MATRIX dma_timer timer enable\n");
    TIM_Enable(ATIM2, ENABLE);
}

static void n32_matrix_trace_init(void) {
#ifdef N32_TRACE_MATRIX
    char buffer[128];
    int  length = snprintf(buffer, sizeof(buffer), "MATRIX dma_timer init rows=%u cols=%u col_us=%u pf=%u rf=%u dma=DMA2_CH1-6 timer=ATIM2 hwcol=%u\n", (unsigned)N32_MATRIX_DMA_ROWS, (unsigned)N32_MATRIX_DMA_COLS, (unsigned)N32_MATRIX_DMA_COLUMN_US, (unsigned)n32_matrix_press_confirm_frames(), (unsigned)n32_matrix_release_confirm_frames(), (unsigned)N32_MATRIX_DMA_HW_COLUMNS);

    if (length > 0) {
        SEGGER_RTT_Write(0, buffer, length < (int)sizeof(buffer) ? (unsigned)length : sizeof(buffer) - 1U);
    }
    n32_dma_trace_timer = timer_read32();
#endif
}

static void n32_matrix_trace_report(void) {
#ifdef N32_TRACE_MATRIX
    uint32_t elapsed = timer_elapsed32(n32_dma_trace_timer);

    if (elapsed < 1000U || n32_dma_trace_generation == n32_dma_frame_generation) {
        return;
    }

    char buffer[320];
    int  length = snprintf(buffer, sizeof(buffer), "MATRIX dma_timer frames=%lu gen=%lu active_col=%u rows=%08lX/%08lX/%08lX/%08lX/%08lX s=", (unsigned long)n32_dma_frame_count, (unsigned long)n32_dma_frame_generation, (unsigned)n32_dma_active_col, (unsigned long)n32_dma_rows[0], (unsigned long)n32_dma_rows[1], (unsigned long)n32_dma_rows[2], (unsigned long)n32_dma_rows[3], (unsigned long)n32_dma_rows[4]);

    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS && length > 0 && length < (int)sizeof(buffer); ++col) {
        length += snprintf(buffer + length, sizeof(buffer) - length, "%02X", (unsigned)n32_dma_samples[col]);
        if (col + 1U < N32_MATRIX_DMA_COLS && length > 0 && length < (int)sizeof(buffer)) {
            buffer[length++] = ',';
        }
    }
    if (length > 0 && length < (int)sizeof(buffer)) {
        length += snprintf(buffer + length, sizeof(buffer) - length, " p0=%04lX/%04lX/%04lX/%04lX p1=%04lX/%04lX/%04lX/%04lX p2=%04lX/%04lX/%04lX/%04lX\n", (unsigned long)(n32_sample_gpioa[0] & 0xFFFFU), (unsigned long)(n32_sample_gpiob[0] & 0xFFFFU), (unsigned long)(n32_sample_gpioc[0] & 0xFFFFU), (unsigned long)(n32_sample_gpiod[0] & 0xFFFFU), (unsigned long)(n32_sample_gpioa[1] & 0xFFFFU), (unsigned long)(n32_sample_gpiob[1] & 0xFFFFU), (unsigned long)(n32_sample_gpioc[1] & 0xFFFFU), (unsigned long)(n32_sample_gpiod[1] & 0xFFFFU), (unsigned long)(n32_sample_gpioa[2] & 0xFFFFU), (unsigned long)(n32_sample_gpiob[2] & 0xFFFFU), (unsigned long)(n32_sample_gpioc[2] & 0xFFFFU), (unsigned long)(n32_sample_gpiod[2] & 0xFFFFU));
    }

    if (length > 0) {
        SEGGER_RTT_Write(0, buffer, length < (int)sizeof(buffer) ? (unsigned)length : sizeof(buffer) - 1U);
    }
    n32_dma_trace_generation = n32_dma_frame_generation;
    n32_dma_trace_timer      = timer_read32();
#endif
}

void matrix_init_pins(void) {
    n32_matrix_trace_literal("MATRIX dma_timer pins init\n");

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
        gpio_set_pin_input_high(n32_row_pins[row]);
    }

    for (uint8_t col = 0; col < N32_MATRIX_DMA_COLS; ++col) {
        n32_matrix_gpio_input_pullup(n32_col_pins[col]);
    }
    n32_matrix_prepare_col_fastio();

    n32_dma_active_col = 0;
#if N32_MATRIX_DMA_HW_COLUMNS_SINGLE_PORT
    if (n32_col_dma_valid) {
        n32_matrix_apply_col(0);
    } else {
        n32_matrix_apply_col(0);
    }
#else
    n32_matrix_apply_col(0);
#endif
    n32_matrix_dma_init();
    n32_matrix_trace_init();
    n32_matrix_timer_init();
}

void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {
    uint32_t perf_start = n32_perf_cycles();

    if (current_row == 0) {
        n32_matrix_snapshot_rows();
    }
    if (current_row < N32_MATRIX_DMA_ROWS) {
        current_matrix[current_row] = n32_qmk_rows[current_row];
    }

    n32_matrix_trace_report();
    n32_matrix_trace_event_flush();
    n32_perf_record(N32_PERF_MATRIX_SCAN, perf_start);
}

void matrix_read_rows_on_col(matrix_row_t current_matrix[], uint8_t current_col, matrix_row_t row_shifter) {
    uint32_t perf_start = n32_perf_cycles();

    if (current_col == 0) {
        n32_matrix_snapshot_rows();
    }
    if (current_col < N32_MATRIX_DMA_COLS) {
        for (uint8_t row = 0; row < N32_MATRIX_DMA_ROWS; ++row) {
            if ((n32_qmk_rows[row] & row_shifter) != 0) {
                current_matrix[row] |= row_shifter;
            } else {
                current_matrix[row] &= ~row_shifter;
            }
        }
    }

    n32_matrix_trace_report();
    n32_matrix_trace_event_flush();
    n32_perf_record(N32_PERF_MATRIX_SCAN, perf_start);
}

void matrix_output_select_delay(void) {}

void matrix_output_unselect_delay(uint8_t line, bool key_pressed) {
    (void)line;
    (void)key_pressed;
}

#if N32_MATRIX_DMA_HW_COLUMNS_ENABLED
void DMA2_Channel4_IRQHandler(void) {
    bool transfer_complete = DMA_GetIntStatus(DMA_INT_TXC4, DMA2) != RESET;

    if (!transfer_complete && DMA_GetIntStatus(DMA_INT_ERR4, DMA2) == RESET) {
        return;
    }

    DMA_ClrIntPendingBit(DMA_INT_GLB4, DMA2);

    if (transfer_complete) {
        n32_dma_active_col = 0;
        n32_matrix_capture_frame();
        n32_matrix_decode_frame();
    }
}
#else
void ATIM2_UP_IRQHandler(void) {
    if (TIM_GetIntStatus(ATIM2, TIM_INT_UPDATE) == RESET) {
        return;
    }

    TIM_ClrIntPendingBit(ATIM2, TIM_INT_UPDATE);

    n32_matrix_store_active_sample();

    uint8_t next_col = n32_dma_active_col + 1U;
    bool    wrapped  = false;
    if (next_col >= N32_MATRIX_DMA_COLS) {
        next_col = 0;
        wrapped  = true;
    }

    n32_dma_active_col = next_col;
    n32_matrix_apply_col(next_col);

    if (wrapped) {
        n32_matrix_decode_frame();
    }
}
#endif
