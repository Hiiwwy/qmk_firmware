#include "action.h"
#include "suspend.h"
#include "usb_driver.h"
#include "platform_deps.h"
#include "print.h"
#include "timer.h"

#include <string.h>

#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_USB_HS)
#    define n32_usbhs_trace(...) xprintf(__VA_ARGS__)
#else
#    define n32_usbhs_trace(...) ((void)0)
#endif

#ifdef N32_USB_SPEED_FS
#    include "n32_usbfsd_keyboard.h"
#else
#    ifdef MIN
#        undef MIN
#    endif
#    include "n32_usbhs_keyboard.h"
#    include "usbhs_core.h"
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
static uint8_t          hs_shared_pending_report[HID_IN_PACKET] __attribute__((aligned(4)));
static uint8_t          hs_shared_pending_length;
static volatile uint8_t hs_shared_report_pending;
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
    n32_usbhs_trace("USB HS clk begin hse=%lu sys=%lu ctrl1=%08lX cfg3=%08lX\n", (unsigned long)HSE_VALUE, (unsigned long)SystemCoreClock, (unsigned long)RCC->USBHSCTRL1, (unsigned long)RCC->CFG3);

#    if HSE_VALUE == 8000000U
    RCC_ConfigUSBPLLPresClk(RCC_USBPLLCLK_SRC_PLL, RCC_USBPLLCLK_DIV15);
    RCC_ConfigUSBHSClk(RCC_USBHS_CLKSRC_PLLPRES);
    n32_usbhs_trace("USB HS clk pll/15 cfg3=%08lX\n", (unsigned long)RCC->CFG3);
#    else
    RCC_ConfigUSBHSClk(RCC_USBHS_CLKSRC_HSE);
#    endif

    RCC_ConfigUSBHSBandwidth(RCC_USBHS_BW_16M);
    RCC_ConfigUSBHSFrequency(RCC_USBHS_FREQ_16_OR_32M);
    RCC_EnableAHBPeriphReset(RCC_AHBPRST_USBHSPHYRST);
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPHEN_USBHS, ENABLE);

    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0xF << 4))) | (0x5 << 4);
    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0x3 << 12))) | (0x3 << 12);
    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0x3 << 14))) | (0x3 << 14);
    RCC->USBHSCTRL2 = (RCC->USBHSCTRL2 & (~(0x3 << 16))) | (0x3 << 16);
    n32_usbhs_trace("USB HS clk done ctrl1=%08lX ctrl2=%08lX ahb=%08lX\n", (unsigned long)RCC->USBHSCTRL1, (unsigned long)RCC->USBHSCTRL2, (unsigned long)RCC->AHB1PCLKEN);
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
static void n32_usb_hs_start_shared_tx(const void *report, uint8_t length) {
    n32_usb_wait_report_slot();
    memcpy(hs_shared_report, report, length);
    hs_shared_prev_xfer_complete = 0;
    USBDEV_EP_Tx(&USB_dev, N32_USBHS_HID_IN_EP, hs_shared_report, length);
}

static void n32_usb_hs_queue_shared_report(const void *report, uint8_t length) {
    if (USB_dev.dev.device_status != USB_CONFIGURED) {
        return;
    }

    if (hs_shared_report_pending) {
        n32_usb_dropped_shared_reports++;
    }
    memcpy(hs_shared_pending_report, report, length);
    hs_shared_pending_length  = length;
    hs_shared_report_pending = 1;
}

static void n32_usb_hs_flush_shared_report(void) {
    if (USB_dev.dev.device_status != USB_CONFIGURED || !hs_shared_prev_xfer_complete || !hs_shared_report_pending) {
        return;
    }

    uint8_t length = hs_shared_pending_length;
    hs_shared_report_pending = 0;
    n32_usb_hs_start_shared_tx(hs_shared_pending_report, length);
}
#endif

void n32_usb_init(void) {
    n32_usbhs_trace("USB init begin speed=hs hse=%lu\n", (unsigned long)HSE_VALUE);
    n32_usb_enable_cycle_counter();
    n32_usb_update_report_interval();
#ifdef N32_USB_SPEED_FS
    n32_usbfsd_init();
#else
    n32_usbhs_config_clk();
    n32_usbhs_trace("USB HS usbd init begin\n");
    USBD_Init(&USB_dev, &USBD_desc, &n32_usbhs_keyboard_cb, &n32_usb_user_cb);
    n32_usbhs_trace("USB HS usbd init done status=%lu speed=%lu\n", (unsigned long)USB_dev.dev.device_status, (unsigned long)USB_dev.cfg.speed);
#endif
}

#ifdef N32_USB_SPEED_FS
extern __IO uint32_t bDeviceState;
#endif

void n32_usb_task(void) {
    if (n32_usb_suspended()) {
        while (n32_usb_suspended()) {
            suspend_power_down();

            if (suspend_wakeup_condition() && n32_usb_remote_wakeup_enabled()) {
                n32_usb_remote_wakeup();
                clear_keyboard();
            }
        }

        suspend_wakeup_init();
    }

#ifndef N32_USB_SPEED_FS
    n32_usb_hs_flush_shared_report();
#endif
#ifdef N32_TRACE_USB_RUNTIME
    static uint32_t last_log_tick;
    static uint32_t prev_usb_state = 0xFFFFFFFF;
#    ifdef N32_USB_SPEED_FS
    static n32_usbfsd_debug_counters_t prev_fs_debug;
#    else
    static n32_usbhs_debug_counters_t prev_hs_debug;
#    endif
    uint32_t now = timer_read32();

    if ((now - last_log_tick) >= 1000) {
        last_log_tick = now;
#    ifdef N32_USB_SPEED_FS
        uint32_t                    state = bDeviceState;
        n32_usbfsd_debug_counters_t fs_debug;
#    else
        uint32_t state = USB_dev.dev.device_status;
#    endif
        if (state != prev_usb_state) {
            xprintf("USB state: %lu\n", (unsigned long)state);
            prev_usb_state = state;
        }
#    ifndef N32_USB_SPEED_FS
        n32_usbhs_debug_counters_t hs_debug;
        n32_usbhs_debug_snapshot(&hs_debug);
        if (memcmp(&hs_debug, &prev_hs_debug, sizeof(hs_debug)) != 0 || state != USB_CONFIGURED) {
            uint8_t raw_in_ep  = N32_USBHS_RAW_IN_EP & 0x0FU;
            uint8_t raw_out_ep = N32_USBHS_RAW_OUT_EP & 0x0FU;
            xprintf("USB hs dbg: status=%lu speed=%lu cfg=%lu addr=%lu rst=%lu conf=%lu susp=%lu res=%lu conn=%lu disc=%lu init=%lu deinit=%lu setup=%lu getcfg=%lu ep0=%lu in=%lu out=%lu raw=%lu/%lu/%lu/%lu rawlen=%u/%u last=%02X/%02X/%04X/%04X/%04X spd=%u cfgidx=%u ep=%u drop=%lu slot_to=%lu ctrl1=%08lX ctrl2=%08lX cfg3=%08lX ahb=%08lX gint=%08lX/%08lX gahb=%08lX gcfg=%08lX dctl=%08lX dcfg=%08lX dsts=%08lX daep=%08lX/%08lX ep0in=%08lX/%08lX ep0out=%08lX/%08lX/%08lX rawin%u=%08lX/%08lX/%08lX rawout%u=%08lX/%08lX/%08lX\n", (unsigned long)USB_dev.dev.device_status, (unsigned long)USB_dev.cfg.speed, (unsigned long)USB_dev.dev.device_config, (unsigned long)USB_dev.dev.device_address, (unsigned long)hs_debug.reset_count, (unsigned long)hs_debug.configured_count, (unsigned long)hs_debug.suspended_count, (unsigned long)hs_debug.resumed_count, (unsigned long)hs_debug.connected_count, (unsigned long)hs_debug.disconnected_count, (unsigned long)hs_debug.class_init_count, (unsigned long)hs_debug.class_deinit_count, (unsigned long)hs_debug.setup_count, (unsigned long)hs_debug.get_config_count, (unsigned long)hs_debug.ep0_rx_count, (unsigned long)hs_debug.datain_count, (unsigned long)hs_debug.dataout_count, (unsigned long)hs_debug.raw_ep0_out_count, (unsigned long)hs_debug.raw_out_count, (unsigned long)hs_debug.raw_task_count, (unsigned long)hs_debug.raw_drop_count, hs_debug.last_raw_ep0_length, hs_debug.last_raw_out_length, hs_debug.last_request_type, hs_debug.last_request, hs_debug.last_value, hs_debug.last_index, hs_debug.last_length, hs_debug.last_speed, hs_debug.last_cfgidx, hs_debug.last_ep, (unsigned long)n32_usb_dropped_shared_reports, (unsigned long)n32_usb_report_slot_timeouts, (unsigned long)RCC->USBHSCTRL1, (unsigned long)RCC->USBHSCTRL2, (unsigned long)RCC->CFG3, (unsigned long)RCC->AHB1PCLKEN, (unsigned long)USB_READ_REG32(&USB_dev.regs.GCSR->GINTSTS), (unsigned long)USB_READ_REG32(&USB_dev.regs.GCSR->GINTEN), (unsigned long)USB_READ_REG32(&USB_dev.regs.GCSR->GAHBCFG), (unsigned long)USB_READ_REG32(&USB_dev.regs.GCSR->GCFG), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DCFG), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DSTS), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DAEPINTSTS), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DAEPINTEN), (unsigned long)USB_READ_REG32(&USB_dev.regs.INEPCSR[0]->DINEPCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.INEPCSR[0]->DINEPTXSIZ), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[0]->DOUTEPCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[0]->DOUTEPTXSIZ), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[0]->DOUTEPINTSTS), raw_in_ep, (unsigned long)USB_READ_REG32(&USB_dev.regs.INEPCSR[raw_in_ep]->DINEPCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.INEPCSR[raw_in_ep]->DINEPTXSIZ), (unsigned long)USB_READ_REG32(&USB_dev.regs.INEPCSR[raw_in_ep]->DINEPINTSTS), raw_out_ep, (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[raw_out_ep]->DOUTEPCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[raw_out_ep]->DOUTEPTXSIZ), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[raw_out_ep]->DOUTEPINTSTS));
            xprintf("USB hs epmask: daep=%08lX/%08lX dinepm=%08lX doutepm=%08lX hidout1=%08lX/%08lX/%08lX rawout%u=%08lX/%08lX/%08lX\n", (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DAEPINTSTS), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DAEPINTEN), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DINEPINTEN), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DOUTEPINTEN), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[N32_USBHS_HID_OUT_EP & 0x0FU]->DOUTEPCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[N32_USBHS_HID_OUT_EP & 0x0FU]->DOUTEPTXSIZ), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[N32_USBHS_HID_OUT_EP & 0x0FU]->DOUTEPINTSTS), raw_out_ep, (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[raw_out_ep]->DOUTEPCTRL), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[raw_out_ep]->DOUTEPTXSIZ), (unsigned long)USB_READ_REG32(&USB_dev.regs.OUTEPCSR[raw_out_ep]->DOUTEPINTSTS));
            xprintf("USB hs deepmask: deep=%08lX/%08lX dinex%u=%08lX doutx%u=%08lX rawinx%u=%08lX rawoutx%u=%08lX\n", (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DEEPINTSTS), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DEEPINTEN), (N32_USBHS_HID_IN_EP & 0x0FU), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DINEPXINTEN[N32_USBHS_HID_IN_EP & 0x0FU]), (N32_USBHS_HID_OUT_EP & 0x0FU), (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DOUTEPXINTEN[N32_USBHS_HID_OUT_EP & 0x0FU]), raw_in_ep, (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DINEPXINTEN[raw_in_ep]), raw_out_ep, (unsigned long)USB_READ_REG32(&USB_dev.regs.DCSR->DOUTEPXINTEN[raw_out_ep]));
            prev_hs_debug = hs_debug;
        }
#    endif
#    ifdef N32_USB_SPEED_FS
        n32_usbfsd_debug_snapshot(&fs_debug);
        if (memcmp(&fs_debug, &prev_fs_debug, sizeof(fs_debug)) != 0) {
            xprintf("USB fs dbg: rst=%lu susp=%lu err=%lu ctr=%lu sof=%lu esof=%lu frst=%lu addr=%lu cfg=%lu dev=%lu conf=%lu str=%lu hid=%lu rep=%lu cls=%lu nod=%lu uns=%lu last=%02X/%02X/%04X/%04X/%04X reg=%04X/%04X/%04X/%04X/%04X/%04X/%04X rcc=%08lX/%08lX/%08lX\n", (unsigned long)fs_debug.reset_count, (unsigned long)fs_debug.suspend_count, (unsigned long)fs_debug.error_count, (unsigned long)fs_debug.ctr_count, (unsigned long)fs_debug.sof_count, (unsigned long)fs_debug.esof_count, (unsigned long)fs_debug.esof_forced_reset_count, (unsigned long)fs_debug.set_address_count, (unsigned long)fs_debug.set_configuration_count, (unsigned long)fs_debug.device_desc_count, (unsigned long)fs_debug.config_desc_count, (unsigned long)fs_debug.string_desc_count, (unsigned long)fs_debug.hid_desc_count, (unsigned long)fs_debug.report_desc_count, (unsigned long)fs_debug.class_data_count, (unsigned long)fs_debug.class_nodata_count, (unsigned long)fs_debug.unsupported_count, fs_debug.last_request,
                    fs_debug.last_type, fs_debug.last_value, fs_debug.last_index, fs_debug.last_length, fs_debug.cntr, fs_debug.istr, fs_debug.fnr, fs_debug.daddr, fs_debug.ep0r, fs_debug.ep1r, fs_debug.interrupt_mask, (unsigned long)fs_debug.rcc_cfg3, (unsigned long)fs_debug.rcc_apb1pclken, (unsigned long)fs_debug.rcc_ahb1pclken);
            prev_fs_debug = fs_debug;
        }
#    endif
    }
#endif
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

bool n32_usb_suspended(void) {
#ifdef N32_USB_SPEED_FS
    return false;
#else
    return USB_dev.dev.device_status == USB_SUSPENDED;
#endif
}

bool n32_usb_remote_wakeup_enabled(void) {
#ifdef N32_USB_SPEED_FS
    return false;
#else
    return USB_dev.dev.DevRemoteWakeup != 0;
#endif
}

void n32_usb_remote_wakeup(void) {
#ifndef N32_USB_SPEED_FS
    USB_ActiveRemoteWakeup(&USB_dev);
#endif
}

static void n32_usb_send_shared_report(const void *report, uint8_t length) {
#ifdef N32_USB_SPEED_FS
    n32_usb_wait_report_slot();
    if (!n32_usbfsd_send_shared(report, length)) {
        n32_usb_dropped_shared_reports++;
    }
#else
    if (USB_dev.dev.device_status != USB_CONFIGURED) {
        return;
    }

    if (!hs_shared_prev_xfer_complete || hs_shared_report_pending) {
        n32_usb_hs_queue_shared_report(report, length);
        return;
    }

    n32_usb_hs_start_shared_tx(report, length);
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
    if (length != RAW_IN_PACKET || USB_dev.dev.device_status != USB_CONFIGURED) {
        return;
    }

    uint32_t timeout = (SystemCoreClock ? SystemCoreClock : 240000000U) / 1000U;
    while (!hs_raw_prev_xfer_complete && timeout-- > 0) {
        __NOP();
    }

    if (!hs_raw_prev_xfer_complete) {
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
