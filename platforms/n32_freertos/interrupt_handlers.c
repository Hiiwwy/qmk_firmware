#include "platform_deps.h"

#include "n32h47x_48x_exti.h"

#ifdef N32_USB_SPEED_FS
#    include "usbfsd_istr.h"
#else
#    include "usbhs_dcd_int.h"
#endif

void n32_qmk_tick_1ms(void);
void xPortSysTickHandler(void);

#ifndef N32_USB_SPEED_FS
extern USB_CORE_MODULE USB_dev;
#endif

void SysTick_Handler(void) {
    n32_qmk_tick_1ms();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

#ifdef N32_USB_SPEED_FS
void USB_FS_LP_IRQHandler(void) {
    USB_Istr();
}

void USB_FS_WKUP_IRQHandler(void) {
    EXTI_ClrITPendBit(EXTI_LINE18);
    USB_Istr();
}
#else
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
#endif
