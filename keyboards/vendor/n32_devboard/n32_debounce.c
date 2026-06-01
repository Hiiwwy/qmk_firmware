// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "debounce.h"
#include "n32_perf.h"
#include "timer.h"

#include <string.h>

uint8_t n32_get_debounce_ms(void);

void debounce_init(void) {}

static bool n32_debounce_copy(matrix_row_t raw[], matrix_row_t cooked[], bool changed, uint32_t perf_start) {
    bool cooked_changed = false;

    if (changed) {
        size_t matrix_size = MATRIX_ROWS_PER_HAND * sizeof(matrix_row_t);
        if (memcmp(cooked, raw, matrix_size) != 0) {
            memcpy(cooked, raw, matrix_size);
            cooked_changed = true;
        }
    }
    n32_perf_record(N32_PERF_DEBOUNCE, perf_start);
    return cooked_changed;
}

#if defined(N32_DEBOUNCE_MODE_DEFER_G)
static bool n32_debounce_defer_g(matrix_row_t raw[], matrix_row_t cooked[], bool changed, uint8_t debounce_ms, uint32_t perf_start) {
    static fast_timer_t debouncing_time;
    static bool         debouncing;
    bool                cooked_changed = false;

    if (changed) {
        debouncing      = true;
        debouncing_time = timer_read_fast();
    } else if (debouncing && timer_elapsed_fast(debouncing_time) >= debounce_ms) {
        size_t matrix_size = MATRIX_ROWS_PER_HAND * sizeof(matrix_row_t);
        if (memcmp(cooked, raw, matrix_size) != 0) {
            memcpy(cooked, raw, matrix_size);
            cooked_changed = true;
        }
        debouncing = false;
    }

    n32_perf_record(N32_PERF_DEBOUNCE, perf_start);
    return cooked_changed;
}
#else
static bool n32_debounce_eager_pk(matrix_row_t raw[], matrix_row_t cooked[], uint8_t debounce_ms, uint32_t perf_start) {
    static matrix_row_t pending_release[MATRIX_ROWS_PER_HAND];
    static fast_timer_t release_time[MATRIX_ROWS_PER_HAND][MATRIX_COLS];
    bool                cooked_changed = false;

    for (uint8_t row = 0; row < MATRIX_ROWS_PER_HAND; ++row) {
        matrix_row_t mask = MATRIX_ROW_SHIFTER;

        for (uint8_t col = 0; col < MATRIX_COLS; ++col, mask <<= 1) {
            bool raw_pressed    = (raw[row] & mask) != 0;
            bool cooked_pressed = (cooked[row] & mask) != 0;

            if (raw_pressed) {
                pending_release[row] &= ~mask;
                if (!cooked_pressed) {
                    cooked[row] |= mask;
                    cooked_changed = true;
                }
            } else if (cooked_pressed) {
                if ((pending_release[row] & mask) == 0) {
                    pending_release[row] |= mask;
                    release_time[row][col] = timer_read_fast();
                } else if (timer_elapsed_fast(release_time[row][col]) >= debounce_ms) {
                    pending_release[row] &= ~mask;
                    cooked[row] &= ~mask;
                    cooked_changed = true;
                }
            } else {
                pending_release[row] &= ~mask;
            }
        }
    }

    n32_perf_record(N32_PERF_DEBOUNCE, perf_start);
    return cooked_changed;
}
#endif

bool debounce(matrix_row_t raw[], matrix_row_t cooked[], bool changed) {
    uint32_t perf_start = n32_perf_cycles();
#ifdef N32_MATRIX_DMA_TIMER
    uint8_t debounce_ms = 0;
#else
    uint8_t debounce_ms = n32_get_debounce_ms();
#endif

    if (debounce_ms == 0) {
        return n32_debounce_copy(raw, cooked, changed, perf_start);
    }

#if defined(N32_DEBOUNCE_MODE_DEFER_G)
    return n32_debounce_defer_g(raw, cooked, changed, debounce_ms, perf_start);
#else
    return n32_debounce_eager_pk(raw, cooked, debounce_ms, perf_start);
#endif
}
