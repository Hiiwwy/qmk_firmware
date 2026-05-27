// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "debounce.h"
#include "timer.h"

#include <string.h>

uint8_t n32_get_debounce_ms(void);

void debounce_init(void) {}

bool debounce(matrix_row_t raw[], matrix_row_t cooked[], bool changed) {
    static fast_timer_t debouncing_time;
    static bool         debouncing;
    bool                cooked_changed = false;
    uint8_t             debounce_ms    = n32_get_debounce_ms();

    if (debounce_ms == 0) {
        size_t matrix_size = MATRIX_ROWS_PER_HAND * sizeof(matrix_row_t);
        if (changed && memcmp(cooked, raw, matrix_size) != 0) {
            memcpy(cooked, raw, matrix_size);
            cooked_changed = true;
        }
        debouncing = false;
        return cooked_changed;
    }

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

    return cooked_changed;
}
