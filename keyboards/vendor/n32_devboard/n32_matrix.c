// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"
#include "matrix.h"
#include "n32_perf.h"
#include "platform_deps.h"
#ifdef N32_TRACE_MATRIX
#    include "SEGGER_RTT.h"
#    include "timer.h"
#    include <stdio.h>
#endif
#include "wait.h"

#if DIODE_DIRECTION != COL2ROW && DIODE_DIRECTION != ROW2COL
#    error "n32_matrix.c supports COL2ROW and ROW2COL only."
#endif

static const pin_t n32_row_pins[MATRIX_ROWS_PER_HAND] = MATRIX_ROW_PINS;
static const pin_t n32_col_pins[MATRIX_COLS]          = MATRIX_COL_PINS;

#ifndef N32_MATRIX_SELECT_SETTLE_CYCLES
#    define N32_MATRIX_SELECT_SETTLE_CYCLES 48
#endif

#ifndef N32_MATRIX_UNSELECT_SETTLE_CYCLES
#    define N32_MATRIX_UNSELECT_SETTLE_CYCLES 0
#endif

#ifndef N32_MATRIX_ACTIVE_UNSELECT_SETTLE_CYCLES
#    define N32_MATRIX_ACTIVE_UNSELECT_SETTLE_CYCLES N32_MATRIX_UNSELECT_SETTLE_CYCLES
#endif

#ifndef N32_MATRIX_ADJACENT_RECHECK_CYCLES
#    define N32_MATRIX_ADJACENT_RECHECK_CYCLES 0
#endif

typedef struct {
    GPIO_Module *port;
    uint16_t     mask;
    uint8_t      index;
} n32_matrix_gpio_t;

typedef struct {
    GPIO_Module *port;
    uint8_t      count;
    uint8_t      col_index[MATRIX_COLS];
    uint16_t     col_mask[MATRIX_COLS];
} n32_matrix_col_group_t;

typedef struct {
    GPIO_Module *port;
    uint8_t      count;
    uint8_t      row_index[MATRIX_ROWS_PER_HAND];
    uint16_t     row_mask[MATRIX_ROWS_PER_HAND];
} n32_matrix_row_group_t;

static n32_matrix_gpio_t      n32_rows[MATRIX_ROWS_PER_HAND];
static n32_matrix_gpio_t      n32_cols[MATRIX_COLS];
static n32_matrix_row_group_t n32_row_groups[8];
static n32_matrix_col_group_t n32_col_groups[8];
static uint8_t                n32_row_group_count;
static uint8_t                n32_col_group_count;
static uint32_t               n32_previous_col_pressed_rows;
#ifdef N32_TRACE_MATRIX
static matrix_row_t n32_trace_rows[MATRIX_ROWS_PER_HAND];
static uint32_t     n32_trace_pids[MATRIX_ROWS_PER_HAND][8];
static uint32_t     n32_trace_timer;
static bool         n32_trace_dirty;
static uint32_t     n32_trace_adjacent_recheck_count;
static uint32_t     n32_trace_event_adjacent_recheck_count;
#endif

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

static n32_matrix_gpio_t n32_matrix_gpio(pin_t pin) {
    n32_matrix_gpio_t gpio = {0};

    if (pin != NO_PIN) {
        gpio.port  = n32_matrix_port(pin);
        gpio.mask  = n32_matrix_mask(pin);
        gpio.index = N32_PIN_INDEX(pin);
    }

    return gpio;
}

static void n32_matrix_gpio_input_pullup(pin_t pin) {
    if (pin == NO_PIN) {
        return;
    }

    GPIO_InitType init;
    RCC_EnableAHB1PeriphClk(n32_matrix_clock(pin), ENABLE);
    GPIO_InitStruct(&init);
    init.Pin       = n32_matrix_mask(pin);
    init.GPIO_Mode = GPIO_MODE_INPUT;
    init.GPIO_Pull = GPIO_PULL_UP;
    GPIO_InitPeripheral(n32_matrix_port(pin), &init);
}

static void n32_matrix_add_col_group(uint8_t col, n32_matrix_gpio_t gpio) {
    for (uint8_t group = 0; group < n32_col_group_count; ++group) {
        if (n32_col_groups[group].port == gpio.port) {
            uint8_t index                          = n32_col_groups[group].count++;
            n32_col_groups[group].col_index[index] = col;
            n32_col_groups[group].col_mask[index]  = gpio.mask;
            return;
        }
    }

    if (n32_col_group_count < 8U) {
        uint8_t group                      = n32_col_group_count++;
        n32_col_groups[group].port         = gpio.port;
        n32_col_groups[group].count        = 1;
        n32_col_groups[group].col_index[0] = col;
        n32_col_groups[group].col_mask[0]  = gpio.mask;
    }
}

static void n32_matrix_add_row_group(uint8_t row, n32_matrix_gpio_t gpio) {
    for (uint8_t group = 0; group < n32_row_group_count; ++group) {
        if (n32_row_groups[group].port == gpio.port) {
            uint8_t index                          = n32_row_groups[group].count++;
            n32_row_groups[group].row_index[index] = row;
            n32_row_groups[group].row_mask[index]  = gpio.mask;
            return;
        }
    }

    if (n32_row_group_count < 8U) {
        uint8_t group                      = n32_row_group_count++;
        n32_row_groups[group].port         = gpio.port;
        n32_row_groups[group].count        = 1;
        n32_row_groups[group].row_index[0] = row;
        n32_row_groups[group].row_mask[0]  = gpio.mask;
    }
}

static inline void n32_matrix_enable_cycle_counter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void n32_matrix_wait_cycles(uint32_t cycles) {
    if (cycles == 0U) {
        return;
    }

    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static inline void n32_matrix_select_row(n32_matrix_gpio_t row) {
    uint32_t shift = (uint32_t)row.index * 2U;

    row.port->PMODE = (row.port->PMODE & ~(GPIO_PMODE_BIT_MASK << shift)) | (GPIO_PMODE_OUTPUT << shift);
    row.port->PBC   = row.mask;
}

static inline void n32_matrix_unselect_row(n32_matrix_gpio_t row) {
    uint32_t shift = (uint32_t)row.index * 2U;

    row.port->PBSC = row.mask;
    row.port->PMODE &= ~(GPIO_PMODE_BIT_MASK << shift);
}

static inline void n32_matrix_select_col(n32_matrix_gpio_t col) {
    uint32_t shift = (uint32_t)col.index * 2U;

    col.port->PMODE = (col.port->PMODE & ~(GPIO_PMODE_BIT_MASK << shift)) | (GPIO_PMODE_OUTPUT << shift);
    col.port->PBC   = col.mask;
}

static inline void n32_matrix_unselect_col(n32_matrix_gpio_t col) {
    uint32_t shift = (uint32_t)col.index * 2U;

    col.port->PBSC = col.mask;
    col.port->PMODE &= ~(GPIO_PMODE_BIT_MASK << shift);
}

static void n32_matrix_trace_init(void) {
#ifdef N32_TRACE_MATRIX
    char buffer[96];
    int  length = snprintf(buffer, sizeof(buffer), "MATRIX init rows=%u cols=%u col_groups=%u\n", (unsigned)MATRIX_ROWS_PER_HAND, (unsigned)MATRIX_COLS, (unsigned)n32_col_group_count);

    if (length > 0) {
        SEGGER_RTT_Write(0, buffer, length < (int)sizeof(buffer) ? (unsigned)length : sizeof(buffer) - 1U);
    }
    n32_trace_timer = timer_read32();
#endif
}

static void n32_matrix_trace_row(uint8_t row, uint8_t group, uint32_t pid) {
#ifdef N32_TRACE_MATRIX
    if (row < MATRIX_ROWS_PER_HAND && group < 8U) {
        n32_trace_pids[row][group] = pid;
    }
#else
    (void)row;
    (void)group;
    (void)pid;
#endif
}

static void n32_matrix_trace_raw(uint8_t row, matrix_row_t value) {
#ifdef N32_TRACE_MATRIX
    if (row < MATRIX_ROWS_PER_HAND) {
        if (n32_trace_rows[row] != value) {
            n32_trace_dirty = true;
            n32_trace_event_adjacent_recheck_count = value != 0 ? n32_trace_adjacent_recheck_count : 0;
            n32_trace_adjacent_recheck_count       = 0;
        }
        n32_trace_rows[row] = value;
    }
#else
    (void)row;
    (void)value;
#endif
}

static void n32_matrix_trace_report(void) {
#ifdef N32_TRACE_MATRIX
    uint32_t elapsed = timer_elapsed32(n32_trace_timer);

    if (!n32_trace_dirty && elapsed < 1000U) {
        return;
    }
    if (n32_trace_dirty && elapsed < 10U) {
        return;
    }

    char buffer[224];
    int  offset = snprintf(buffer, sizeof(buffer), "MATRIX raw");

    for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND && offset > 0 && offset < (int)sizeof(buffer); ++row) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " r%u=%08lX", (unsigned)row, (unsigned long)n32_trace_rows[row]);
    }
    if (n32_trace_event_adjacent_recheck_count > 0U && offset > 0 && offset < (int)sizeof(buffer)) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, " adj=%lu", (unsigned long)n32_trace_event_adjacent_recheck_count);
    }
    for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND && offset > 0 && offset < (int)sizeof(buffer); ++row) {
        for (uint8_t group = 0; group < n32_col_group_count && offset > 0 && offset < (int)sizeof(buffer); ++group) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, " p%u.%u=%04lX", (unsigned)row, (unsigned)group, (unsigned long)(n32_trace_pids[row][group] & 0xFFFFU));
        }
    }
    if (offset > 0 && offset < (int)sizeof(buffer)) {
        buffer[offset++] = '\n';
    }
    if (offset > 0) {
        SEGGER_RTT_Write(0, buffer, offset < (int)sizeof(buffer) ? (unsigned)offset : sizeof(buffer) - 1U);
    }
    n32_trace_timer = timer_read32();
    n32_trace_dirty = false;
    n32_trace_event_adjacent_recheck_count = 0;
#endif
}

void matrix_init_pins(void) {
    n32_matrix_enable_cycle_counter();
    n32_row_group_count = 0;
    n32_col_group_count = 0;
    n32_previous_col_pressed_rows = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND; ++row) {
        n32_rows[row] = n32_matrix_gpio(n32_row_pins[row]);
        n32_matrix_gpio_input_pullup(n32_row_pins[row]);
        if (n32_rows[row].port != NULL) {
            n32_matrix_add_row_group(row, n32_rows[row]);
        }
    }

    for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
        n32_cols[col] = n32_matrix_gpio(n32_col_pins[col]);
        n32_matrix_gpio_input_pullup(n32_col_pins[col]);
        if (n32_cols[col].port != NULL) {
            n32_matrix_add_col_group(col, n32_cols[col]);
        }
    }
    n32_matrix_trace_init();
}

void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {
    uint32_t     perf_start        = n32_perf_cycles();
    matrix_row_t current_row_value = 0;

    if (current_row >= MATRIX_ROWS_PER_HAND) {
        return;
    }

    if (n32_rows[current_row].port == NULL) {
        current_matrix[current_row] = 0;
        return;
    }

    n32_matrix_select_row(n32_rows[current_row]);
    matrix_output_select_delay();

    for (uint8_t group = 0; group < n32_col_group_count; ++group) {
        uint32_t pid = n32_col_groups[group].port->PID;

        n32_matrix_trace_row(current_row, group, pid);

        for (uint8_t index = 0; index < n32_col_groups[group].count; ++index) {
            if ((pid & n32_col_groups[group].col_mask[index]) == 0) {
                current_row_value |= (MATRIX_ROW_SHIFTER << n32_col_groups[group].col_index[index]);
            }
        }
    }

    n32_matrix_unselect_row(n32_rows[current_row]);
    matrix_output_unselect_delay(current_row, current_row_value != 0);

    current_matrix[current_row] = current_row_value;
    n32_matrix_trace_raw(current_row, current_row_value);
    n32_matrix_trace_report();
    n32_perf_record(N32_PERF_MATRIX_SCAN, perf_start);
}

void matrix_read_rows_on_col(matrix_row_t current_matrix[], uint8_t current_col, matrix_row_t row_shifter) {
    uint32_t perf_start               = n32_perf_cycles();
    bool     key_pressed              = false;
    uint32_t current_col_pressed_rows = 0;

    if (current_col >= MATRIX_COLS) {
        return;
    }

    if (current_col == 0) {
        n32_previous_col_pressed_rows = 0;
    }

    if (n32_cols[current_col].port == NULL) {
        for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND; ++row) {
            current_matrix[row] &= ~row_shifter;
        }
        n32_previous_col_pressed_rows = 0;
        return;
    }

    n32_matrix_select_col(n32_cols[current_col]);
    matrix_output_select_delay();

    for (uint8_t group = 0; group < n32_row_group_count; ++group) {
        uint32_t pid             = n32_row_groups[group].port->PID;
        bool     group_rechecked = false;

        for (uint8_t index = 0; index < n32_row_groups[group].count; ++index) {
            uint8_t row = n32_row_groups[group].row_index[index];
            uint32_t row_bit = 1UL << row;
            bool     pressed = (pid & n32_row_groups[group].row_mask[index]) == 0;

            if (pressed && (n32_previous_col_pressed_rows & row_bit) != 0U) {
                if (!group_rechecked) {
#ifdef N32_TRACE_MATRIX
                    n32_trace_adjacent_recheck_count++;
#endif
                    n32_matrix_wait_cycles(N32_MATRIX_ADJACENT_RECHECK_CYCLES);
                    pid             = n32_row_groups[group].port->PID;
                    group_rechecked = true;
                }
                pressed = (pid & n32_row_groups[group].row_mask[index]) == 0;
            }

            if (pressed) {
                current_matrix[row] |= row_shifter;
                current_col_pressed_rows |= row_bit;
                key_pressed = true;
            } else {
                current_matrix[row] &= ~row_shifter;
            }
            n32_matrix_trace_raw(row, current_matrix[row]);
        }
    }

    n32_matrix_unselect_col(n32_cols[current_col]);
    matrix_output_unselect_delay(current_col, key_pressed);
    n32_previous_col_pressed_rows = current_col_pressed_rows;

    n32_matrix_trace_report();
    n32_perf_record(N32_PERF_MATRIX_SCAN, perf_start);
}

void matrix_output_select_delay(void) {
    n32_matrix_wait_cycles(N32_MATRIX_SELECT_SETTLE_CYCLES);
}

void matrix_output_unselect_delay(uint8_t line, bool key_pressed) {
    (void)line;
    n32_matrix_wait_cycles(key_pressed ? N32_MATRIX_ACTIVE_UNSELECT_SETTLE_CYCLES : N32_MATRIX_UNSELECT_SETTLE_CYCLES);
}
