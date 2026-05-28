#include "gpio.h"
#include "platform_deps.h"

static GPIO_Module *n32_gpio_port(pin_t pin) {
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

static uint32_t n32_gpio_clock(pin_t pin) {
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

static uint16_t n32_gpio_pin_mask(pin_t pin) {
    return (uint16_t)(1U << N32_PIN_INDEX(pin));
}

void n32_gpio_set_input(pin_t pin, bool pull_up, bool pull_down) {
    GPIO_InitType init;
    RCC_EnableAHB1PeriphClk(n32_gpio_clock(pin), ENABLE);
    GPIO_InitStruct(&init);
    init.Pin       = n32_gpio_pin_mask(pin);
    init.GPIO_Mode = GPIO_MODE_INPUT;
    init.GPIO_Pull = pull_up ? GPIO_PULL_UP : (pull_down ? GPIO_PULL_DOWN : GPIO_NO_PULL);
    GPIO_InitPeripheral(n32_gpio_port(pin), &init);
}

void n32_gpio_set_output(pin_t pin, bool open_drain) {
    GPIO_InitType init;
    RCC_EnableAHB1PeriphClk(n32_gpio_clock(pin), ENABLE);
    GPIO_InitStruct(&init);
    init.Pin       = n32_gpio_pin_mask(pin);
    init.GPIO_Mode = open_drain ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT_PP;
    init.GPIO_Pull = GPIO_NO_PULL;
    GPIO_InitPeripheral(n32_gpio_port(pin), &init);
}

void n32_gpio_write(pin_t pin, bool level) {
    GPIO_WriteBits(n32_gpio_port(pin), n32_gpio_pin_mask(pin), level ? Bit_SET : Bit_RESET);
}

bool n32_gpio_read(pin_t pin) {
    return GPIO_ReadInputDataBit(n32_gpio_port(pin), n32_gpio_pin_mask(pin)) != RESET;
}

void n32_gpio_toggle(pin_t pin) {
    n32_gpio_write(pin, !n32_gpio_read(pin));
}
