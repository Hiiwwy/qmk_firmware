#include "usb_driver.h"
#include "platform_deps.h"
#include "print.h"
#include "timer.h"

#include <string.h>

#ifdef N32_USB_SPEED_FS
#    include "n32_usbfsd_keyboard.h"
#else
#    ifdef MIN
#        undef MIN
#    endif
#    include "n32_usbhs_keyboard.h"
#    include "usbhs_dcd_int.h"
#    include "usbd_core.h"
#    include "usbd_desc.h"
#    include <string.h>
#endif

#ifndef N32_USB_REPORT_RATE_DEFAULT
#    ifdef N32_USB_SPEED_FS
#        define N32_USB_REPORT_RATE_DEFAULT 1000
#    else
#        define N32_USB_REPORT_RATE_DEFAULT 8000
#    endif
#endif

static uint8_t keyboard_led_state;

#ifndef N32_USB_SPEED_FS
__ALIGN_BEGIN USB_CORE_MODULE USB_dev __ALIGN_END;

static volatile uint8_t hs_shared_prev_xfer_complete = 1;
static volatile uint8_t hs_raw_prev_xfer_complete    = 1;
static uint8_t          hs_shared_report[HID_IN_PACKET] __attribute__((aligned(4)));
static uint8_t          hs_raw_report[RAW_IN_PACKET] __attribute__((aligned(4)));

static USBD_User_cb_TypeDef n32_usb_user_cb = {
    .Init               = n32_usbhs_user_init,
    .DeviceReset        = n32_usbhs_user_device_reset,
    .DeviceConfigured   = n32_usbhs_user_device_configured,
    .DeviceSuspended    = n32_usbhs_user_device_suspended,
    .DeviceResumed      = n32_usbhs_user_device_resumed,
    .DeviceConnected    = n32_usbhs_user_device_connected,
    .DeviceDisconnected = n32_usbhs_user_device_disconnected,
};
#endif

static uint16_t n32_usb_report_rate_hz = N32_USB_REPORT_RATE_DEFAULT;
static uint32_t n32_usb_report_interval_cycles;
static uint32_t n32_usb_last_shared_tx_cycle;
static uint32_t n32_usb_dropped_shared_reports;
static uint32_t n32_usb_report_slot_timeouts;
static bool     n32_usb_cycle_counter_ready;

#ifndef N32_USB_SPEED_FS
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
#endif

static void n32_usb_enable_cycle_counter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    n32_usb_cycle_counter_ready = (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0;
}

static void n32_usb_update_report_interval(void) {
    uint32_t cycles_per_second = SystemCoreClock ? SystemCoreClock : 240000000U;

    n32_usb_report_interval_cycles = cycles_per_second / n32_usb_report_rate_hz;
}

static void n32_usb_wait_report_slot(void) {
    if (n32_usb_report_interval_cycles == 0 || !n32_usb_cycle_counter_ready || (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0) {
        return;
    }

    uint32_t now   = DWT->CYCCNT;
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

#ifndef N32_USB_SPEED_FS
static bool n32_usb_hs_shared_endpoint_ready(void) {
    if (USB_dev.dev.device_status != USB_CONFIGURED || !hs_shared_prev_xfer_complete) {
        n32_usb_dropped_shared_reports++;
        return false;
    }

    return true;
}
#endif

void n32_usb_init(void) {
    n32_usb_enable_cycle_counter();
    n32_usb_update_report_interval();
#ifdef N32_USB_SPEED_FS
    n32_usbfsd_init();
#else
    n32_usbhs_config_clk();
    USBD_Init(&USB_dev, &USBD_desc, &n32_usbhs_keyboard_cb, &n32_usb_user_cb);
#endif
}

#ifdef N32_USB_SPEED_FS
extern __IO uint32_t bDeviceState;
#endif

void n32_usb_task(void) {
    static uint32_t last_log_tick;
    static uint32_t prev_usb_state = 0xFFFFFFFF;
#ifdef N32_USB_SPEED_FS
    static n32_usbfsd_debug_counters_t prev_fs_debug;
#endif
    uint32_t now = timer_read32();

    if ((now - last_log_tick) >= 1000) {
        last_log_tick = now;
#ifdef N32_USB_SPEED_FS
        uint32_t                    state = bDeviceState;
        n32_usbfsd_debug_counters_t fs_debug;
#else
        uint32_t state = USB_dev.dev.device_status;
#endif
        if (state != prev_usb_state) {
            xprintf("USB state: %lu\n", (unsigned long)state);
            prev_usb_state = state;
        }
#ifdef N32_USB_SPEED_FS
        n32_usbfsd_debug_snapshot(&fs_debug);
        if (memcmp(&fs_debug, &prev_fs_debug, sizeof(fs_debug)) != 0) {
            xprintf("USB fs dbg: rst=%lu susp=%lu err=%lu ctr=%lu sof=%lu esof=%lu frst=%lu addr=%lu cfg=%lu dev=%lu conf=%lu str=%lu hid=%lu rep=%lu cls=%lu nod=%lu uns=%lu last=%02X/%02X/%04X/%04X/%04X reg=%04X/%04X/%04X/%04X/%04X/%04X/%04X rcc=%08lX/%08lX/%08lX\n", (unsigned long)fs_debug.reset_count, (unsigned long)fs_debug.suspend_count, (unsigned long)fs_debug.error_count, (unsigned long)fs_debug.ctr_count, (unsigned long)fs_debug.sof_count, (unsigned long)fs_debug.esof_count, (unsigned long)fs_debug.esof_forced_reset_count, (unsigned long)fs_debug.set_address_count, (unsigned long)fs_debug.set_configuration_count, (unsigned long)fs_debug.device_desc_count, (unsigned long)fs_debug.config_desc_count, (unsigned long)fs_debug.string_desc_count, (unsigned long)fs_debug.hid_desc_count, (unsigned long)fs_debug.report_desc_count, (unsigned long)fs_debug.class_data_count, (unsigned long)fs_debug.class_nodata_count, (unsigned long)fs_debug.unsupported_count, fs_debug.last_request,
                    fs_debug.last_type, fs_debug.last_value, fs_debug.last_index, fs_debug.last_length, fs_debug.cntr, fs_debug.istr, fs_debug.fnr, fs_debug.daddr, fs_debug.ep0r, fs_debug.ep1r, fs_debug.interrupt_mask, (unsigned long)fs_debug.rcc_cfg3, (unsigned long)fs_debug.rcc_apb1pclken, (unsigned long)fs_debug.rcc_ahb1pclken);
            prev_fs_debug = fs_debug;
        }
#endif
    }
}

#ifdef RAW_ENABLE
void raw_hid_task(void) {
#    ifdef N32_USB_SPEED_FS
    n32_usbfsd_raw_hid_task();
#    else
    n32_usbhs_raw_hid_task();
#    endif
}
#endif

uint8_t n32_usb_keyboard_leds(void) {
    return keyboard_led_state;
}

void n32_usb_set_report_rate_hz(uint16_t hz) {
#ifdef N32_USB_SPEED_FS
    (void)hz;
    n32_usb_report_rate_hz = 1000;
#else
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
#endif

    n32_usb_update_report_interval();
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

static void n32_usb_send_shared_report(const void *report, uint8_t length) {
#ifdef N32_USB_SPEED_FS
    n32_usb_wait_report_slot();
    if (!n32_usbfsd_send_shared(report, length)) {
        n32_usb_dropped_shared_reports++;
    }
#else
    if (!n32_usb_hs_shared_endpoint_ready()) {
        return;
    }

    n32_usb_wait_report_slot();
    memcpy(hs_shared_report, report, length);
    hs_shared_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_shared_report, length);
#endif
}

void n32_usb_send_keyboard(report_keyboard_t *report) {
#if defined(N32_USB_SPEED_FS) && defined(N32_USBFS_MINIMAL_KEYBOARD) && defined(KEYBOARD_SHARED_EP)
    n32_usb_send_shared_report((const uint8_t *)report + 1, sizeof(*report) - 1);
#else
    n32_usb_send_shared_report(report, sizeof(*report));
#endif
}

void n32_usb_send_nkro(report_nkro_t *report) {
#if defined(N32_USB_SPEED_FS) && defined(N32_USBFS_MINIMAL_KEYBOARD)
    (void)report;
#else
    n32_usb_send_shared_report(report, sizeof(*report));
#endif
}

void n32_usb_send_mouse(report_mouse_t *report) {
#if defined(N32_USB_SPEED_FS) && defined(N32_USBFS_MINIMAL_KEYBOARD)
    (void)report;
#else
    n32_usb_send_shared_report(report, sizeof(*report));
#endif
}

void n32_usb_send_extra(report_extra_t *report) {
#if defined(N32_USB_SPEED_FS) && defined(N32_USBFS_MINIMAL_KEYBOARD)
    (void)report;
#else
    n32_usb_send_shared_report(report, sizeof(*report));
#endif
}

#ifdef RAW_ENABLE
void n32_usb_send_raw_hid(uint8_t *data, uint8_t length) {
#    ifdef N32_USB_SPEED_FS
    if (!n32_usbfsd_send_raw_hid(data, length)) {
        return;
    }
#    else
    if (length != RAW_IN_PACKET || USB_dev.dev.device_status != USB_CONFIGURED || !hs_raw_prev_xfer_complete) {
        return;
    }

    memcpy(hs_raw_report, data, RAW_IN_PACKET);
    hs_raw_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_RAW_IN_EP, hs_raw_report, RAW_IN_PACKET);
#    endif
}
#endif

void n32_usb_set_keyboard_leds(uint8_t leds) {
    keyboard_led_state = leds;
}

#ifndef N32_USB_SPEED_FS
void n32_usbhs_shared_tx_done(void) {
    hs_shared_prev_xfer_complete = 1;
}

#    ifdef RAW_ENABLE
void n32_usbhs_raw_tx_done(void) {
    hs_raw_prev_xfer_complete = 1;
}
#    endif
#endif
