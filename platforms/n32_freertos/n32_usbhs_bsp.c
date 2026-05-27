#include "usbhs_bsp.h"

#include "misc.h"
#include "n32h47x_48x_exti.h"
#include "n32h47x_48x_gpio.h"
#include "n32h47x_48x_rcc.h"

void USB_BSP_Init(void) {
    GPIO_InitType gpio;

    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA | RCC_AHB_PERIPHEN_GPIOB | RCC_AHB_PERIPHEN_GPIOC, ENABLE);

    GPIO_InitStruct(&gpio);

    gpio.Pin = GPIO_PIN_13;
    gpio.GPIO_Mode = GPIO_MODE_INPUT;
    GPIO_InitPeripheral(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_12;
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    gpio.GPIO_Alternate = 14;
    GPIO_InitPeripheral(GPIOB, &gpio);
}

void USB_BSP_EnableInterrupt(USB_CORE_MODULE *USBx) {
    (void)USBx;

    EXTI_InitType exti;

    NVIC_SetPriorityGrouping(3);
    NVIC_SetPriority(USB_HS_WKUP_IRQn, NVIC_EncodePriority(3, 1, 0));
    NVIC_EnableIRQ(USB_HS_WKUP_IRQn);
    NVIC_SetPriority(USB_HS_IRQn, NVIC_EncodePriority(3, 2, 0));
    NVIC_EnableIRQ(USB_HS_IRQn);

    EXTI_ClrITPendBit(EXTI_LINE23);
    exti.EXTI_Line = EXTI_LINE23;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&exti);
}

void USB_BSP_uDelay(const uint32_t usec) {
    const uint32_t cycles = ((240000000UL + 1000000UL - 1UL) / 1000000UL) * usec;
    for (volatile uint32_t i = 0; i < cycles; ++i) {
        __NOP();
    }
}

void USB_BSP_mDelay(const uint32_t msec) {
    USB_BSP_uDelay(msec * 1000U);
}
