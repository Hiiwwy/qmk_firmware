#include "n32_usbhs_keyboard.h"

#include "report.h"
#ifdef RAW_ENABLE
#    include "raw_hid.h"
#endif
#include "usb_driver.h"
#include "usbhs_bsp.h"
#include "usbhs_core.h"
#include "usbd_desc.h"
#include "usbd_req.h"

#include <string.h>

static uint8_t  n32_usbhs_keyboard_init(void *pdev, uint8_t cfgidx);
static uint8_t  n32_usbhs_keyboard_deinit(void *pdev, uint8_t cfgidx);
static uint8_t  n32_usbhs_keyboard_setup(void *pdev, USB_SETUP_REQ *req);
static uint8_t  n32_usbhs_keyboard_ep0_rx_ready(void *pdev);
static uint8_t *n32_usbhs_keyboard_get_config(uint8_t speed, uint16_t *length);
static uint8_t  n32_usbhs_keyboard_datain(void *pdev, uint8_t epnum);
static uint8_t  n32_usbhs_keyboard_dataout(void *pdev, uint8_t epnum);

USBD_Class_cb_TypeDef n32_usbhs_keyboard_cb = {
    n32_usbhs_keyboard_init, n32_usbhs_keyboard_deinit, n32_usbhs_keyboard_setup, NULL, n32_usbhs_keyboard_ep0_rx_ready, n32_usbhs_keyboard_datain, n32_usbhs_keyboard_dataout, NULL, NULL, NULL, n32_usbhs_keyboard_get_config, n32_usbhs_keyboard_get_config,
};

__ALIGN_BEGIN static uint32_t hid_alt_set[N32_USBHS_INTERFACE_COUNT] __ALIGN_END;
__ALIGN_BEGIN static uint32_t hid_protocol[N32_USBHS_INTERFACE_COUNT] __ALIGN_END;
__ALIGN_BEGIN static uint32_t hid_idle_state[N32_USBHS_INTERFACE_COUNT] __ALIGN_END;
__ALIGN_BEGIN static uint8_t  hid_out_report[HID_OUT_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t  ep0_out_report[HID_OUT_PACKET] __ALIGN_END;
static uint8_t                ep0_out_interface;
static n32_usbhs_debug_counters_t n32_usbhs_debug_counters;

extern USB_CORE_MODULE USB_dev;

#ifdef RAW_ENABLE
#    ifndef RAW_REPORT_PACKET
#        define RAW_REPORT_PACKET 32U
#    endif
__ALIGN_BEGIN static uint8_t raw_out_report[RAW_OUT_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t raw_ep0_report[RAW_OUT_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t raw_pending_report[RAW_REPORT_PACKET] __ALIGN_END;
static volatile uint8_t      raw_report_pending;
static uint16_t              raw_ep0_report_length;
#endif

__ALIGN_BEGIN static const uint8_t shared_report_desc[] __ALIGN_END = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, REPORT_ID_KEYBOARD, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x05, 0x07, 0x19, 0x00, 0x2A, 0xFF, 0x00, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, 0x06, 0x75, 0x08, 0x81, 0x00, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0xC0,

    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, REPORT_ID_MOUSE,    0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x95, 0x02, 0x75, 0x08, 0x81, 0x06, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x95, 0x01, 0x75, 0x08, 0x81, 0x06, 0x05, 0x0C, 0x0A, 0x38, 0x02, 0x15, 0x81, 0x25, 0x7F, 0x95, 0x01, 0x75, 0x08, 0x81, 0x06, 0xC0, 0xC0,

    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x85, REPORT_ID_SYSTEM,   0x19, 0x01, 0x2A, 0xB7, 0x00, 0x15, 0x01, 0x26, 0xB7, 0x00, 0x95, 0x01, 0x75, 0x10, 0x81, 0x00, 0xC0,

    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, REPORT_ID_CONSUMER, 0x19, 0x01, 0x2A, 0xA0, 0x02, 0x15, 0x01, 0x26, 0xA0, 0x02, 0x95, 0x01, 0x75, 0x10, 0x81, 0x00, 0xC0,

    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, REPORT_ID_NKRO,     0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x05, 0x07, 0x19, 0x00, 0x29, 0xEF, 0x15, 0x00, 0x25, 0x01, 0x95, 0xF0, 0x75, 0x01, 0x81, 0x02, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05, 0x75, 0x01, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0xC0,
};

#ifdef RAW_ENABLE
__ALIGN_BEGIN static const uint8_t raw_report_desc[] __ALIGN_END = {
    0x06, 0x60, 0xFF, 0x09, 0x61, 0xA1, 0x01, 0x09, 0x62, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, RAW_REPORT_PACKET, 0x75, 0x08, 0x81, 0x02, 0x09, 0x63, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, RAW_REPORT_PACKET, 0x75, 0x08, 0x91, 0x02, 0xC0,
};
#endif

__ALIGN_BEGIN static uint8_t keyboard_config_desc[N32_USBHS_KEYBOARD_CFGDESC_SIZE] __ALIGN_END = {
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    LOBYTE(N32_USBHS_KEYBOARD_CFGDESC_SIZE),
    HIBYTE(N32_USBHS_KEYBOARD_CFGDESC_SIZE),
    N32_USBHS_INTERFACE_COUNT,
    0x01,
    0x00,
    0xA0,
    0x32,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    0x02,
    0x03,
    0x00,
    0x00,
    0x00,
    0x09,
    N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE,
    0x11,
    0x01,
    0x00,
    0x01,
    N32_USBHS_KEYBOARD_REPORT_DESC,
    LOBYTE(sizeof(shared_report_desc)),
    HIBYTE(sizeof(shared_report_desc)),
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBHS_HID_IN_EP,
    0x03,
    LOBYTE(HID_IN_PACKET),
    HIBYTE(HID_IN_PACKET),
    N32_USBHS_POLLING_INTERVAL,
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBHS_HID_OUT_EP,
    0x03,
    LOBYTE(HID_OUT_PACKET),
    HIBYTE(HID_OUT_PACKET),
    N32_USBHS_POLLING_INTERVAL,

#ifdef RAW_ENABLE
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
    0x00,
    0x02,
    0x03,
    0x00,
    0x00,
    0x05,
    0x09,
    N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE,
    0x11,
    0x01,
    0x00,
    0x01,
    N32_USBHS_KEYBOARD_REPORT_DESC,
    LOBYTE(sizeof(raw_report_desc)),
    HIBYTE(sizeof(raw_report_desc)),
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBHS_RAW_IN_EP,
    0x03,
    LOBYTE(RAW_IN_PACKET),
    HIBYTE(RAW_IN_PACKET),
    N32_USBHS_POLLING_INTERVAL,
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBHS_RAW_OUT_EP,
    0x03,
    LOBYTE(RAW_OUT_PACKET),
    HIBYTE(RAW_OUT_PACKET),
    N32_USBHS_POLLING_INTERVAL,
#endif
};

static void n32_usbhs_handle_output_report(const uint8_t *report) {
    if (report[0] == REPORT_ID_KEYBOARD || report[0] == REPORT_ID_NKRO) {
        n32_usb_set_keyboard_leds(report[1]);
    } else {
        n32_usb_set_keyboard_leds(report[0]);
    }
}

#ifdef RAW_ENABLE
static void n32_usbhs_queue_raw_report(const uint8_t *report, uint32_t length) {
    const uint8_t *payload = report;

    if (length == (RAW_REPORT_PACKET + 1U) && report[0] == 0) {
        payload = report + 1;
    } else if (length < RAW_REPORT_PACKET) {
        n32_usbhs_debug_counters.raw_drop_count++;
        return;
    }

    if (!raw_report_pending) {
        memcpy(raw_pending_report, payload, RAW_REPORT_PACKET);
        raw_report_pending = 1;
    } else {
        n32_usbhs_debug_counters.raw_drop_count++;
    }
}
#endif

void n32_usbhs_debug_snapshot(n32_usbhs_debug_counters_t *snapshot) {
    *snapshot = n32_usbhs_debug_counters;
}

static uint8_t n32_usbhs_keyboard_init(void *pdev, uint8_t cfgidx) {
    USB_CORE_MODULE *usb = (USB_CORE_MODULE *)pdev;

    n32_usbhs_debug_counters.class_init_count++;
    n32_usbhs_debug_counters.last_cfgidx = cfgidx;
    (void)cfgidx;
    USBDEV_EP_Open(pdev, N32_USBHS_HID_IN_EP, HID_IN_PACKET, USB_EP_INT);
    USBDEV_EP_Open(pdev, N32_USBHS_HID_OUT_EP, HID_OUT_PACKET, USB_EP_INT);
    USBDEV_EP_PrepareRx(pdev, N32_USBHS_HID_OUT_EP, hid_out_report, HID_OUT_PACKET);
    USBDEV_SetEPStatus(pdev, N32_USBHS_HID_OUT_EP, USB_EP_RX_VALID);

#ifdef RAW_ENABLE
    USBDEV_EP_Open(pdev, N32_USBHS_RAW_IN_EP, RAW_IN_PACKET, USB_EP_INT);
    USBDEV_EP_Open(pdev, N32_USBHS_RAW_OUT_EP, RAW_OUT_PACKET, USB_EP_INT);
    USBDEV_EP_PrepareRx(pdev, N32_USBHS_RAW_OUT_EP, raw_out_report, RAW_OUT_PACKET);
    USBDEV_SetEPStatus(pdev, N32_USBHS_RAW_OUT_EP, USB_EP_RX_VALID);
#endif

#ifdef USB_DEDICATED_EP_ENABLED
    uint32_t in_mask  = USBHS_DINEPINTEN_TXCIEN | USBHS_DINEPINTEN_TOIEN | USBHS_DINEPINTEN_EPDIEN;
    uint32_t out_mask = USBHS_DOUTEPINTEN_STUPDNEIEN | USBHS_DOUTEPINTEN_TXCIEN | USBHS_DOUTEPINTEN_EPDIEN;
    USB_WRITE_REG32(&usb->regs.DCSR->DINEPXINTEN[N32_USBHS_HID_IN_EP & 0x0FU], in_mask);
    USB_WRITE_REG32(&usb->regs.DCSR->DOUTEPXINTEN[N32_USBHS_HID_OUT_EP & 0x0FU], out_mask);
#    ifdef RAW_ENABLE
    USB_WRITE_REG32(&usb->regs.DCSR->DINEPXINTEN[N32_USBHS_RAW_IN_EP & 0x0FU], in_mask);
    USB_WRITE_REG32(&usb->regs.DCSR->DOUTEPXINTEN[N32_USBHS_RAW_OUT_EP & 0x0FU], out_mask);
#    endif
#else
    uint32_t daepinten = USB_READ_REG32(&usb->regs.DCSR->DAEPINTEN);
    daepinten |= (1UL << (N32_USBHS_HID_IN_EP & 0x0FU));
    daepinten |= (1UL << (16U + (N32_USBHS_HID_OUT_EP & 0x0FU)));
#    ifdef RAW_ENABLE
    daepinten |= (1UL << (N32_USBHS_RAW_IN_EP & 0x0FU));
    daepinten |= (1UL << (16U + (N32_USBHS_RAW_OUT_EP & 0x0FU)));
#    endif
    USB_WRITE_REG32(&usb->regs.DCSR->DAEPINTEN, daepinten);
    USB_SET_REG32_BIT(&usb->regs.DCSR->DOUTEPINTEN, USBHS_DOUTEPINTEN_TXCIEN | USBHS_DOUTEPINTEN_EPDIEN);
#endif

    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_deinit(void *pdev, uint8_t cfgidx) {
    n32_usbhs_debug_counters.class_deinit_count++;
    n32_usbhs_debug_counters.last_cfgidx = cfgidx;
    (void)cfgidx;
    USBDEV_EP_Close(pdev, N32_USBHS_HID_IN_EP);
    USBDEV_EP_Close(pdev, N32_USBHS_HID_OUT_EP);
#ifdef RAW_ENABLE
    USBDEV_EP_Close(pdev, N32_USBHS_RAW_IN_EP);
    USBDEV_EP_Close(pdev, N32_USBHS_RAW_OUT_EP);
#endif
    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_setup(void *pdev, USB_SETUP_REQ *req) {
    uint16_t len       = 0;
    uint8_t *pbuf      = NULL;
    uint8_t  status    = USBD_OK;
    uint8_t  interface = (uint8_t)(req->wIndex & 0xFF);

    n32_usbhs_debug_counters.setup_count++;
    n32_usbhs_debug_counters.last_request_type = req->bmRequest;
    n32_usbhs_debug_counters.last_request      = req->bRequest;
    n32_usbhs_debug_counters.last_value        = req->wValue;
    n32_usbhs_debug_counters.last_index        = req->wIndex;
    n32_usbhs_debug_counters.last_length       = req->wLength;

    if (interface >= N32_USBHS_INTERFACE_COUNT) {
        interface = 0;
    }

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS:
            switch (req->bRequest) {
                case N32_USBHS_KEYBOARD_REQ_SET_PROTOCOL:
                    hid_protocol[interface] = (uint8_t)req->wValue;
                    break;
                case N32_USBHS_KEYBOARD_REQ_GET_PROTOCOL:
                    USBD_CtrlSendData(pdev, (uint8_t *)&hid_protocol[interface], 1);
                    break;
                case N32_USBHS_KEYBOARD_REQ_SET_IDLE:
                    hid_idle_state[interface] = (uint8_t)(req->wValue >> 8);
                    break;
                case N32_USBHS_KEYBOARD_REQ_GET_IDLE:
                    USBD_CtrlSendData(pdev, (uint8_t *)&hid_idle_state[interface], 1);
                    break;
                case N32_USBHS_KEYBOARD_REQ_SET_REPORT:
                    ep0_out_interface = interface;
#ifdef RAW_ENABLE
                    if (interface == 1) {
                        raw_ep0_report_length = MIN(req->wLength, sizeof(raw_ep0_report));
                        USBD_CtrlPrepareRx(pdev, raw_ep0_report, raw_ep0_report_length);
                    } else
#endif
                    {
                        USBD_CtrlPrepareRx(pdev, ep0_out_report, MIN(req->wLength, sizeof(ep0_out_report)));
                    }
                    break;
                default:
                    USBD_CtrlError(pdev, req);
                    status = USBD_FAIL;
                    break;
            }
            break;
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest) {
                case USB_REQ_GET_DESCRIPTOR:
                    if ((req->wValue >> 8) == N32_USBHS_KEYBOARD_REPORT_DESC) {
#ifdef RAW_ENABLE
                        if (interface == 1) {
                            len  = MIN(sizeof(raw_report_desc), req->wLength);
                            pbuf = (uint8_t *)raw_report_desc;
                        } else
#endif
                        {
                            len  = MIN(sizeof(shared_report_desc), req->wLength);
                            pbuf = (uint8_t *)shared_report_desc;
                        }
                    } else if ((req->wValue >> 8) == N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE) {
#ifdef RAW_ENABLE
                        pbuf = keyboard_config_desc + (interface == 1 ? 50 : 18);
#else
                        pbuf = keyboard_config_desc + 18;
#endif
                        len = MIN(N32_USBHS_KEYBOARD_DESC_SIZE, req->wLength);
                    }

                    if (pbuf != NULL) {
                        USBD_CtrlSendData(pdev, pbuf, len);
                    } else {
                        USBD_CtrlError(pdev, req);
                        status = USBD_FAIL;
                    }
                    break;
                case USB_REQ_GET_INTERFACE:
                    USBD_CtrlSendData(pdev, (uint8_t *)&hid_alt_set[interface], 1);
                    break;
                case USB_REQ_SET_INTERFACE:
                    hid_alt_set[interface] = (uint8_t)req->wValue;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return status;
}

static uint8_t n32_usbhs_keyboard_ep0_rx_ready(void *pdev) {
    (void)pdev;

    n32_usbhs_debug_counters.ep0_rx_count++;

#ifdef RAW_ENABLE
    if (ep0_out_interface == 1) {
        n32_usbhs_debug_counters.raw_ep0_out_count++;
        n32_usbhs_debug_counters.last_raw_ep0_length = raw_ep0_report_length;
        n32_usbhs_queue_raw_report(raw_ep0_report, raw_ep0_report_length);
        return USBD_OK;
    }
#endif

    if (ep0_out_interface == 0) {
        n32_usbhs_handle_output_report(ep0_out_report);
    }

    return USBD_OK;
}

static uint8_t *n32_usbhs_keyboard_get_config(uint8_t speed, uint16_t *length) {
    n32_usbhs_debug_counters.get_config_count++;
    n32_usbhs_debug_counters.last_speed = speed;
    *length = sizeof(keyboard_config_desc);
    return keyboard_config_desc;
}

static uint8_t n32_usbhs_keyboard_datain(void *pdev, uint8_t epnum) {
    (void)pdev;

    n32_usbhs_debug_counters.datain_count++;
    n32_usbhs_debug_counters.last_ep = epnum;

    if (epnum == N32_USBHS_HID_IN_EP || epnum == (N32_USBHS_HID_IN_EP & 0x0F)) {
        n32_usbhs_shared_tx_done();
    }
#ifdef RAW_ENABLE
    if (epnum == N32_USBHS_RAW_IN_EP || epnum == (N32_USBHS_RAW_IN_EP & 0x0F)) {
        n32_usbhs_raw_tx_done();
    }
#endif

    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_dataout(void *pdev, uint8_t epnum) {
    n32_usbhs_debug_counters.dataout_count++;
    n32_usbhs_debug_counters.last_ep = epnum;

    if (epnum == N32_USBHS_HID_OUT_EP || epnum == (N32_USBHS_HID_OUT_EP & 0x0F)) {
        n32_usbhs_handle_output_report(hid_out_report);
        USBDEV_EP_PrepareRx(pdev, N32_USBHS_HID_OUT_EP, hid_out_report, HID_OUT_PACKET);
        USBDEV_SetEPStatus(pdev, N32_USBHS_HID_OUT_EP, USB_EP_RX_VALID);
    }

#ifdef RAW_ENABLE
    if (epnum == N32_USBHS_RAW_OUT_EP || epnum == (N32_USBHS_RAW_OUT_EP & 0x0F)) {
        USB_CORE_MODULE *usb = (USB_CORE_MODULE *)pdev;
        uint8_t          raw_out_ep = N32_USBHS_RAW_OUT_EP & 0x0F;
        uint32_t         rx_length  = usb->dev.out_ep[raw_out_ep].xfer_count;

        n32_usbhs_debug_counters.raw_out_count++;
        n32_usbhs_debug_counters.last_raw_out_length = (uint16_t)rx_length;
        if (rx_length > sizeof(raw_out_report)) {
            rx_length = sizeof(raw_out_report);
        }
        n32_usbhs_queue_raw_report(raw_out_report, rx_length);
        USBDEV_EP_PrepareRx(pdev, N32_USBHS_RAW_OUT_EP, raw_out_report, RAW_OUT_PACKET);
        USBDEV_SetEPStatus(pdev, N32_USBHS_RAW_OUT_EP, USB_EP_RX_VALID);
    }
#endif

    return USBD_OK;
}

#ifdef RAW_ENABLE
void n32_usbhs_raw_hid_task(void) {
    uint8_t data[RAW_REPORT_PACKET];

    if (!raw_report_pending) {
        return;
    }

    memcpy(data, raw_pending_report, sizeof(data));
    raw_report_pending = 0;
    n32_usbhs_debug_counters.raw_task_count++;
    raw_hid_receive(data, sizeof(data));
}
#endif

void n32_usbhs_user_init(void) {
}
void n32_usbhs_user_device_reset(uint8_t speed) {
    n32_usbhs_debug_counters.reset_count++;
    n32_usbhs_debug_counters.last_speed = speed;
    USB_EP0_OutStart(&USB_dev);
}
void n32_usbhs_user_device_configured(void) {
    n32_usbhs_debug_counters.configured_count++;
}
void n32_usbhs_user_device_suspended(void) {
    n32_usbhs_debug_counters.suspended_count++;
}
void n32_usbhs_user_device_resumed(void) {
    n32_usbhs_debug_counters.resumed_count++;
}
void n32_usbhs_user_device_connected(void) {
    n32_usbhs_debug_counters.connected_count++;
}
void n32_usbhs_user_device_disconnected(void) {
    n32_usbhs_debug_counters.disconnected_count++;
}
