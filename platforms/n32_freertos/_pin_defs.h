#pragma once

#include <stdint.h>

typedef uint16_t pin_t;

#define N32_PORT_A 0
#define N32_PORT_B 1
#define N32_PORT_C 2
#define N32_PORT_D 3
#define N32_PORT_E 4
#define N32_PORT_F 5
#define N32_PORT_G 6
#define N32_PORT_H 7

#define N32_PIN(port, pin) ((pin_t)((((port) & 0x0F) << 4) | ((pin) & 0x0F)))
#define N32_PIN_PORT(pin) (((pin) >> 4) & 0x0F)
#define N32_PIN_INDEX(pin) ((pin) & 0x0F)

#define A0 N32_PIN(N32_PORT_A, 0)
#define A1 N32_PIN(N32_PORT_A, 1)
#define A2 N32_PIN(N32_PORT_A, 2)
#define A3 N32_PIN(N32_PORT_A, 3)
#define A4 N32_PIN(N32_PORT_A, 4)
#define A5 N32_PIN(N32_PORT_A, 5)
#define A6 N32_PIN(N32_PORT_A, 6)
#define A7 N32_PIN(N32_PORT_A, 7)
#define A8 N32_PIN(N32_PORT_A, 8)
#define A9 N32_PIN(N32_PORT_A, 9)
#define A10 N32_PIN(N32_PORT_A, 10)
#define A11 N32_PIN(N32_PORT_A, 11)
#define A12 N32_PIN(N32_PORT_A, 12)
#define A13 N32_PIN(N32_PORT_A, 13)
#define A14 N32_PIN(N32_PORT_A, 14)
#define A15 N32_PIN(N32_PORT_A, 15)

#define B0 N32_PIN(N32_PORT_B, 0)
#define B1 N32_PIN(N32_PORT_B, 1)
#define B2 N32_PIN(N32_PORT_B, 2)
#define B3 N32_PIN(N32_PORT_B, 3)
#define B4 N32_PIN(N32_PORT_B, 4)
#define B5 N32_PIN(N32_PORT_B, 5)
#define B6 N32_PIN(N32_PORT_B, 6)
#define B7 N32_PIN(N32_PORT_B, 7)
#define B8 N32_PIN(N32_PORT_B, 8)
#define B9 N32_PIN(N32_PORT_B, 9)
#define B10 N32_PIN(N32_PORT_B, 10)
#define B11 N32_PIN(N32_PORT_B, 11)
#define B12 N32_PIN(N32_PORT_B, 12)
#define B13 N32_PIN(N32_PORT_B, 13)
#define B14 N32_PIN(N32_PORT_B, 14)
#define B15 N32_PIN(N32_PORT_B, 15)

#define C0 N32_PIN(N32_PORT_C, 0)
#define C1 N32_PIN(N32_PORT_C, 1)
#define C2 N32_PIN(N32_PORT_C, 2)
#define C3 N32_PIN(N32_PORT_C, 3)
#define C4 N32_PIN(N32_PORT_C, 4)
#define C5 N32_PIN(N32_PORT_C, 5)
#define C6 N32_PIN(N32_PORT_C, 6)
#define C7 N32_PIN(N32_PORT_C, 7)
#define C8 N32_PIN(N32_PORT_C, 8)
#define C9 N32_PIN(N32_PORT_C, 9)
#define C10 N32_PIN(N32_PORT_C, 10)
#define C11 N32_PIN(N32_PORT_C, 11)
#define C12 N32_PIN(N32_PORT_C, 12)
#define C13 N32_PIN(N32_PORT_C, 13)
#define C14 N32_PIN(N32_PORT_C, 14)
#define C15 N32_PIN(N32_PORT_C, 15)
