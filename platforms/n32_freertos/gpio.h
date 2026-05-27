#pragma once

#include <stdbool.h>
#include "pin_defs.h"

void n32_gpio_set_input(pin_t pin, bool pull_up, bool pull_down);
void n32_gpio_set_output(pin_t pin, bool open_drain);
void n32_gpio_write(pin_t pin, bool level);
bool n32_gpio_read(pin_t pin);
void n32_gpio_toggle(pin_t pin);

#define gpio_set_pin_input(pin) n32_gpio_set_input((pin), false, false)
#define gpio_set_pin_input_high(pin) n32_gpio_set_input((pin), true, false)
#define gpio_set_pin_input_low(pin) n32_gpio_set_input((pin), false, true)
#define gpio_set_pin_output_push_pull(pin) n32_gpio_set_output((pin), false)
#define gpio_set_pin_output_open_drain(pin) n32_gpio_set_output((pin), true)
#define gpio_set_pin_output(pin) gpio_set_pin_output_push_pull(pin)

#define gpio_write_pin_high(pin) n32_gpio_write((pin), true)
#define gpio_write_pin_low(pin) n32_gpio_write((pin), false)
#define gpio_write_pin(pin, level) n32_gpio_write((pin), (level))
#define gpio_read_pin(pin) n32_gpio_read((pin))
#define gpio_toggle_pin(pin) n32_gpio_toggle((pin))
