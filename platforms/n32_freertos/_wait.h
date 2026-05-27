#pragma once

#include <stdint.h>
#include "platform_deps.h"

void wait_ms(uint32_t ms);
void wait_us(uint16_t us);

#ifndef GPIO_INPUT_PIN_DELAY
#    define GPIO_INPUT_PIN_DELAY 64
#endif

#define waitInputPinDelay() wait_us(1)
