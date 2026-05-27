#include "platform_deps.h"

#include "usbhs_dcd_int.h"

#include "n32h47x_48x_exti.h"

void n32_qmk_tick_1ms(void);
void xPortSysTickHandler(void);

extern USB_CORE_MODULE USB_dev;

void SysTick_Handler(void) {
    n32_qmk_tick_1ms();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

void USB_HS_IRQHandler(void) {
    USBD_ISTR_Handler(&USB_dev);
}

void USB_HS_WKUP_IRQHandler(void) {
    USBD_ISTR_WKUP_handler(&USB_dev);
    EXTI_ClrITPendBit(EXTI_LINE23);
}

#ifdef USB_DEDICATED_EP_ENABLED
void USB_HS_EPx_OUT_IRQHandler(void) {
    USBD_EP_OUT_ISTR_Handler(&USB_dev);
}

void USB_HS_EPx_IN_IRQHandler(void) {
    USBD_EP_IN_ISTR_Handler(&USB_dev);
}
#endif
