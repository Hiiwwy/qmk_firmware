#include "usb_driver.h"
#include "platform_deps.h"

#include <string.h>

#ifdef MIN
#    undef MIN
#endif

#include "n32_usbhs_keyboard.h"
#include "usbhs_dcd_int.h"
#include "usbd_core.h"
#include "usbd_desc.h"

static uint8_t keyboard_led_state;

__ALIGN_BEGIN USB_CORE_MODULE USB_dev __ALIGN_END;

static volatile uint8_t hs_prev_xfer_complete = 1;
static uint8_t hs_keyboard_report[8] __attribute__((aligned(4)));

static USBD_User_cb_TypeDef n32_usb_user_cb = {
    .Init = n32_usbhs_user_init,
    .DeviceReset = n32_usbhs_user_device_reset,
    .DeviceConfigured = n32_usbhs_user_device_configured,
    .DeviceSuspended = n32_usbhs_user_device_suspended,
    .DeviceResumed = n32_usbhs_user_device_resumed,
    .DeviceConnected = n32_usbhs_user_device_connected,
    .DeviceDisconnected = n32_usbhs_user_device_disconnected,
};

static void n32_usbhs_config_clk(void) {
    RCC_ConfigUSBHSBandwidth(RCC_USBHS_BW_16M);
    RCC_ConfigUSBHSFrequency(RCC_USBHS_FREQ_16_OR_32M);
    RCC_ConfigUSBHSClk(RCC_USBHS_CLKSRC_HSE);
    RCC_EnableAHBPeriphReset(RCC_AHBPRST_USBHSPHYRST);
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPHEN_USBHS, ENABLE);

    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0xF << 4))) | (0x5 << 4);
    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0x3 << 12))) | (0x3 << 12);
    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0x3 << 14))) | (0x3 << 14);
    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0x3 << 16))) | (0x3 << 16);
}

void n32_usb_init(void) {
    n32_usbhs_config_clk();
    USBD_Init(&USB_dev, &USBD_desc, &n32_usbhs_keyboard_cb, &n32_usb_user_cb);
}

void n32_usb_task(void) {}

uint8_t n32_usb_keyboard_leds(void) {
    return keyboard_led_state;
}

void n32_usb_send_keyboard(report_keyboard_t *report) {
    if (USB_dev.dev.device_status != USB_CONFIGURED || !hs_prev_xfer_complete) {
        return;
    }

    memcpy(hs_keyboard_report, report, sizeof(hs_keyboard_report));
    hs_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_keyboard_report, sizeof(hs_keyboard_report));
}

void n32_usb_send_nkro(report_nkro_t *report) {
    (void)report;
}

void n32_usb_send_mouse(report_mouse_t *report) {
    (void)report;
}

void n32_usb_send_extra(report_extra_t *report) {
    (void)report;
}

void n32_usb_set_keyboard_leds(uint8_t leds) {
    keyboard_led_state = leds;
}

void n32_usbhs_keyboard_tx_done(void) {
    hs_prev_xfer_complete = 1;
}
