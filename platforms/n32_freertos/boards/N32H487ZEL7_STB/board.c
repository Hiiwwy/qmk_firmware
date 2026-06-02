#include "board.h"
#include "gpio.h"
#include "platform_deps.h"

void n32_board_early_init(void) {
    NVIC_SetPriorityGrouping(3);
}

void n32_board_init(void) {
    gpio_set_pin_output(N32_DEVBOARD_LED_PIN);
    gpio_write_pin_low(N32_DEVBOARD_LED_PIN);
}
