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

static volatile uint8_t hs_shared_prev_xfer_complete = 1;
static volatile uint8_t hs_raw_prev_xfer_complete    = 1;
static uint8_t          hs_shared_report[HID_IN_PACKET] __attribute__((aligned(4)));
static uint8_t          hs_raw_report[RAW_IN_PACKET] __attribute__((aligned(4)));
static uint16_t         n32_usb_report_rate_hz = 8000;
static uint32_t         n32_usb_report_interval_cycles;
static uint32_t         n32_usb_last_shared_tx_cycle;
static uint32_t         n32_usb_dropped_shared_reports;
static uint32_t         n32_usb_report_slot_timeouts;
static bool             n32_usb_cycle_counter_ready;

static USBD_User_cb_TypeDef n32_usb_user_cb = {
    .Init               = n32_usbhs_user_init,
    .DeviceReset        = n32_usbhs_user_device_reset,
    .DeviceConfigured   = n32_usbhs_user_device_configured,
    .DeviceSuspended    = n32_usbhs_user_device_suspended,
    .DeviceResumed      = n32_usbhs_user_device_resumed,
    .DeviceConnected    = n32_usbhs_user_device_connected,
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

static void n32_usbhs_enable_cycle_counter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    n32_usb_cycle_counter_ready = (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0;
}

static void n32_usbhs_update_report_interval(void) {
    uint32_t cycles_per_second = SystemCoreClock ? SystemCoreClock : 240000000U;

    n32_usb_report_interval_cycles = cycles_per_second / n32_usb_report_rate_hz;
}

static void n32_usbhs_wait_report_slot(void) {
    if (n32_usb_report_interval_cycles == 0 || !n32_usb_cycle_counter_ready || (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0) {
        return;
    }

    uint32_t now = DWT->CYCCNT;
    uint32_t guard = n32_usb_report_interval_cycles * 4U;

    while ((uint32_t)(now - n32_usb_last_shared_tx_cycle) < n32_usb_report_interval_cycles) {
        now = DWT->CYCCNT;
        if (guard-- == 0) {
            n32_usb_report_slot_timeouts++;
            break;
        }
    }

    n32_usb_last_shared_tx_cycle = now;
}

static bool n32_usb_shared_endpoint_ready(void) {
    if (USB_dev.dev.device_status != USB_CONFIGURED || !hs_shared_prev_xfer_complete) {
        n32_usb_dropped_shared_reports++;
        return false;
    }

    return true;
}

void n32_usb_init(void) {
    n32_usbhs_config_clk();
    n32_usbhs_enable_cycle_counter();
    n32_usbhs_update_report_interval();
    USBD_Init(&USB_dev, &USBD_desc, &n32_usbhs_keyboard_cb, &n32_usb_user_cb);
}

void n32_usb_task(void) {}

#ifdef RAW_ENABLE
void raw_hid_task(void) {
    n32_usbhs_raw_hid_task();
}
#endif

uint8_t n32_usb_keyboard_leds(void) {
    return keyboard_led_state;
}

void n32_usb_set_report_rate_hz(uint16_t hz) {
    switch (hz) {
        case 1000:
        case 2000:
        case 4000:
        case 8000:
            n32_usb_report_rate_hz = hz;
            break;
        default:
            n32_usb_report_rate_hz = 8000;
            break;
    }

    n32_usbhs_update_report_interval();
}

uint16_t n32_usb_get_report_rate_hz(void) {
    return n32_usb_report_rate_hz;
}

uint32_t n32_usb_get_dropped_shared_reports(void) {
    return n32_usb_dropped_shared_reports;
}

uint32_t n32_usb_get_report_slot_timeouts(void) {
    return n32_usb_report_slot_timeouts;
}

void n32_usb_send_keyboard(report_keyboard_t *report) {
    if (!n32_usb_shared_endpoint_ready()) {
        return;
    }

    n32_usbhs_wait_report_slot();
    memcpy(hs_shared_report, report, sizeof(*report));
    hs_shared_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_shared_report, sizeof(*report));
}

void n32_usb_send_nkro(report_nkro_t *report) {
    if (!n32_usb_shared_endpoint_ready()) {
        return;
    }

    n32_usbhs_wait_report_slot();
    memcpy(hs_shared_report, report, sizeof(*report));
    hs_shared_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_shared_report, sizeof(*report));
}

void n32_usb_send_mouse(report_mouse_t *report) {
    if (!n32_usb_shared_endpoint_ready()) {
        return;
    }

    n32_usbhs_wait_report_slot();
    memcpy(hs_shared_report, report, sizeof(*report));
    hs_shared_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_shared_report, sizeof(*report));
}

void n32_usb_send_extra(report_extra_t *report) {
    if (!n32_usb_shared_endpoint_ready()) {
        return;
    }

    n32_usbhs_wait_report_slot();
    memcpy(hs_shared_report, report, sizeof(*report));
    hs_shared_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_shared_report, sizeof(*report));
}

#ifdef RAW_ENABLE
void n32_usb_send_raw_hid(uint8_t *data, uint8_t length) {
    if (length != RAW_IN_PACKET || USB_dev.dev.device_status != USB_CONFIGURED || !hs_raw_prev_xfer_complete) {
        return;
    }

    memcpy(hs_raw_report, data, RAW_IN_PACKET);
    hs_raw_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_RAW_IN_EP, hs_raw_report, RAW_IN_PACKET);
}
#endif

void n32_usb_set_keyboard_leds(uint8_t leds) {
    keyboard_led_state = leds;
}

void n32_usbhs_shared_tx_done(void) {
    hs_shared_prev_xfer_complete = 1;
}

#ifdef RAW_ENABLE
void n32_usbhs_raw_tx_done(void) {
    hs_raw_prev_xfer_complete = 1;
}
#endif
