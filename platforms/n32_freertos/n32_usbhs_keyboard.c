#include "n32_usbhs_keyboard.h"

#include "report.h"
#ifdef RAW_ENABLE
#    include "raw_hid.h"
#endif
#include "usb_driver.h"
#include "usbhs_bsp.h"
#include "usbd_desc.h"
#include "usbd_req.h"

#include <string.h>

static uint8_t n32_usbhs_keyboard_init(void *pdev, uint8_t cfgidx);
static uint8_t n32_usbhs_keyboard_deinit(void *pdev, uint8_t cfgidx);
static uint8_t n32_usbhs_keyboard_setup(void *pdev, USB_SETUP_REQ *req);
static uint8_t n32_usbhs_keyboard_ep0_rx_ready(void *pdev);
static uint8_t *n32_usbhs_keyboard_get_config(uint8_t speed, uint16_t *length);
static uint8_t n32_usbhs_keyboard_datain(void *pdev, uint8_t epnum);
static uint8_t n32_usbhs_keyboard_dataout(void *pdev, uint8_t epnum);

USBD_Class_cb_TypeDef n32_usbhs_keyboard_cb = {
    n32_usbhs_keyboard_init,
    n32_usbhs_keyboard_deinit,
    n32_usbhs_keyboard_setup,
    NULL,
    n32_usbhs_keyboard_ep0_rx_ready,
    n32_usbhs_keyboard_datain,
    n32_usbhs_keyboard_dataout,
    NULL,
    NULL,
    NULL,
    n32_usbhs_keyboard_get_config,
    n32_usbhs_keyboard_get_config,
};

__ALIGN_BEGIN static uint32_t hid_alt_set[2] __ALIGN_END;
__ALIGN_BEGIN static uint32_t hid_protocol[2] __ALIGN_END;
__ALIGN_BEGIN static uint32_t hid_idle_state[2] __ALIGN_END;
__ALIGN_BEGIN static uint8_t hid_out_report[HID_OUT_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t ep0_out_report[HID_OUT_PACKET] __ALIGN_END;
static uint8_t ep0_out_interface;

#ifdef RAW_ENABLE
__ALIGN_BEGIN static uint8_t raw_out_report[RAW_OUT_PACKET] __ALIGN_END;
__ALIGN_BEGIN static uint8_t raw_pending_report[RAW_OUT_PACKET] __ALIGN_END;
static volatile uint8_t raw_report_pending;
#endif

__ALIGN_BEGIN static const uint8_t shared_report_desc[] __ALIGN_END = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, REPORT_ID_KEYBOARD,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08,
    0x81, 0x01, 0x05, 0x07, 0x19, 0x00, 0x2A, 0xFF, 0x00, 0x15,
    0x00, 0x26, 0xFF, 0x00, 0x95, 0x06, 0x75, 0x08, 0x81, 0x00,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03,
    0x91, 0x01, 0xC0,

    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, REPORT_ID_MOUSE,
    0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x08,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F,
    0x95, 0x02, 0x75, 0x08, 0x81, 0x06, 0x09, 0x38, 0x15, 0x81,
    0x25, 0x7F, 0x95, 0x01, 0x75, 0x08, 0x81, 0x06, 0x05, 0x0C,
    0x0A, 0x38, 0x02, 0x15, 0x81, 0x25, 0x7F, 0x95, 0x01, 0x75,
    0x08, 0x81, 0x06, 0xC0, 0xC0,

    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x85, REPORT_ID_SYSTEM,
    0x19, 0x01, 0x2A, 0xB7, 0x00, 0x15, 0x01, 0x26, 0xB7, 0x00,
    0x95, 0x01, 0x75, 0x10, 0x81, 0x00, 0xC0,

    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, REPORT_ID_CONSUMER,
    0x19, 0x01, 0x2A, 0xA0, 0x02, 0x15, 0x01, 0x26, 0xA0, 0x02,
    0x95, 0x01, 0x75, 0x10, 0x81, 0x00, 0xC0,

    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, REPORT_ID_NKRO,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x05, 0x07, 0x19, 0x00,
    0x29, 0xEF, 0x15, 0x00, 0x25, 0x01, 0x95, 0xF0, 0x75, 0x01,
    0x81, 0x02, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05,
    0x75, 0x01, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0xC0,
};

#ifdef RAW_ENABLE
__ALIGN_BEGIN static const uint8_t raw_report_desc[] __ALIGN_END = {
    0x06, 0x60, 0xFF, 0x09, 0x61, 0xA1, 0x01,
    0x09, 0x62, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, RAW_IN_PACKET,
    0x75, 0x08, 0x81, 0x02,
    0x09, 0x63, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, RAW_OUT_PACKET,
    0x75, 0x08, 0x91, 0x02, 0xC0,
};
#endif

__ALIGN_BEGIN static uint8_t keyboard_config_desc[N32_USBHS_KEYBOARD_CFGDESC_SIZE] __ALIGN_END = {
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    LOBYTE(N32_USBHS_KEYBOARD_CFGDESC_SIZE),
    HIBYTE(N32_USBHS_KEYBOARD_CFGDESC_SIZE),
    0x02,
    0x01,
    0x00,
    0xE0,
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

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
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
#ifdef RAW_ENABLE
    LOBYTE(sizeof(raw_report_desc)),
    HIBYTE(sizeof(raw_report_desc)),
#else
    0x00,
    0x00,
#endif
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
};

static void n32_usbhs_handle_output_report(const uint8_t *report) {
    if (report[0] == REPORT_ID_KEYBOARD || report[0] == REPORT_ID_NKRO) {
        n32_usb_set_keyboard_leds(report[1]);
    } else {
        n32_usb_set_keyboard_leds(report[0]);
    }
}

static uint8_t n32_usbhs_keyboard_init(void *pdev, uint8_t cfgidx) {
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

    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_deinit(void *pdev, uint8_t cfgidx) {
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
    uint16_t len = 0;
    uint8_t *pbuf = NULL;
    uint8_t status = USBD_OK;
    uint8_t interface = (uint8_t)(req->wIndex & 0xFF);

    if (interface >= 2) {
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
                    USBD_CtrlPrepareRx(pdev, ep0_out_report, MIN(req->wLength, sizeof(ep0_out_report)));
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
                        if (interface == 1) {
#ifdef RAW_ENABLE
                            len = MIN(sizeof(raw_report_desc), req->wLength);
                            pbuf = (uint8_t *)raw_report_desc;
#endif
                        } else {
                            len = MIN(sizeof(shared_report_desc), req->wLength);
                            pbuf = (uint8_t *)shared_report_desc;
                        }
                    } else if ((req->wValue >> 8) == N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE) {
                        pbuf = keyboard_config_desc + (interface == 1 ? 50 : 18);
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

    if (ep0_out_interface == 0) {
        n32_usbhs_handle_output_report(ep0_out_report);
    }

    return USBD_OK;
}

static uint8_t *n32_usbhs_keyboard_get_config(uint8_t speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(keyboard_config_desc);
    return keyboard_config_desc;
}

static uint8_t n32_usbhs_keyboard_datain(void *pdev, uint8_t epnum) {
    (void)pdev;

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
    if (epnum == N32_USBHS_HID_OUT_EP || epnum == (N32_USBHS_HID_OUT_EP & 0x0F)) {
        n32_usbhs_handle_output_report(hid_out_report);
        USBDEV_EP_PrepareRx(pdev, N32_USBHS_HID_OUT_EP, hid_out_report, HID_OUT_PACKET);
        USBDEV_SetEPStatus(pdev, N32_USBHS_HID_OUT_EP, USB_EP_RX_VALID);
    }

#ifdef RAW_ENABLE
    if (epnum == N32_USBHS_RAW_OUT_EP || epnum == (N32_USBHS_RAW_OUT_EP & 0x0F)) {
        if (!raw_report_pending) {
            memcpy(raw_pending_report, raw_out_report, RAW_OUT_PACKET);
            raw_report_pending = 1;
        }
        USBDEV_EP_PrepareRx(pdev, N32_USBHS_RAW_OUT_EP, raw_out_report, RAW_OUT_PACKET);
        USBDEV_SetEPStatus(pdev, N32_USBHS_RAW_OUT_EP, USB_EP_RX_VALID);
    }
#endif

    return USBD_OK;
}

#ifdef RAW_ENABLE
void n32_usbhs_raw_hid_task(void) {
    uint8_t data[RAW_OUT_PACKET];

    if (!raw_report_pending) {
        return;
    }

    memcpy(data, raw_pending_report, sizeof(data));
    raw_report_pending = 0;
    raw_hid_receive(data, sizeof(data));
}
#endif

void n32_usbhs_user_init(void) {}
void n32_usbhs_user_device_reset(uint8_t speed) { (void)speed; }
void n32_usbhs_user_device_configured(void) {}
void n32_usbhs_user_device_suspended(void) {}
void n32_usbhs_user_device_resumed(void) {}
void n32_usbhs_user_device_connected(void) {}
void n32_usbhs_user_device_disconnected(void) {}
