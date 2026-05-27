#include "n32_usbhs_keyboard.h"

#include "usb_driver.h"
#include "usbhs_bsp.h"
#include "usbd_desc.h"
#include "usbd_req.h"

static uint8_t n32_usbhs_keyboard_init(void *pdev, uint8_t cfgidx);
static uint8_t n32_usbhs_keyboard_deinit(void *pdev, uint8_t cfgidx);
static uint8_t n32_usbhs_keyboard_setup(void *pdev, USB_SETUP_REQ *req);
static uint8_t *n32_usbhs_keyboard_get_config(uint8_t speed, uint16_t *length);
static uint8_t n32_usbhs_keyboard_datain(void *pdev, uint8_t epnum);
static uint8_t n32_usbhs_keyboard_dataout(void *pdev, uint8_t epnum);

USBD_Class_cb_TypeDef n32_usbhs_keyboard_cb = {
    n32_usbhs_keyboard_init,
    n32_usbhs_keyboard_deinit,
    n32_usbhs_keyboard_setup,
    NULL,
    NULL,
    n32_usbhs_keyboard_datain,
    n32_usbhs_keyboard_dataout,
    NULL,
    NULL,
    NULL,
    n32_usbhs_keyboard_get_config,
    n32_usbhs_keyboard_get_config,
};

__ALIGN_BEGIN static uint32_t keyboard_alt_set __ALIGN_END;
__ALIGN_BEGIN static uint32_t keyboard_protocol __ALIGN_END;
__ALIGN_BEGIN static uint32_t keyboard_idle_state __ALIGN_END;
__ALIGN_BEGIN static uint8_t keyboard_led_report[HID_OUT_PACKET] __ALIGN_END;

__ALIGN_BEGIN static uint8_t keyboard_config_desc[N32_USBHS_KEYBOARD_CFGDESC_SIZE] __ALIGN_END = {
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    N32_USBHS_KEYBOARD_CFGDESC_SIZE,
    0x00,
    0x01,
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
    0x01,
    0x01,
    0x00,
    0x09,
    N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE,
    0x11,
    0x01,
    0x00,
    0x01,
    N32_USBHS_KEYBOARD_REPORT_DESC,
    N32_USBHS_KEYBOARD_REPORT_DESC_SIZE,
    0x00,
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBHS_HID_IN_EP,
    0x03,
    HID_IN_PACKET,
    0x00,
    N32_USBHS_POLLING_INTERVAL,
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBHS_HID_OUT_EP,
    0x03,
    HID_OUT_PACKET,
    0x00,
    N32_USBHS_POLLING_INTERVAL,
};

__ALIGN_BEGIN static uint8_t keyboard_report_desc[N32_USBHS_KEYBOARD_REPORT_DESC_SIZE] __ALIGN_END = {
    0x05, 0x01,
    0x09, 0x06,
    0xA1, 0x01,
    0x05, 0x07,
    0x19, 0xE0,
    0x29, 0xE7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x01,
    0x95, 0x05,
    0x75, 0x01,
    0x05, 0x08,
    0x19, 0x01,
    0x29, 0x05,
    0x91, 0x02,
    0x95, 0x01,
    0x75, 0x03,
    0x91, 0x01,
    0x95, 0x06,
    0x75, 0x08,
    0x15, 0x00,
    0x25, 0x65,
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0x65,
    0x81, 0x00,
    0xC0,
};

static uint8_t n32_usbhs_keyboard_init(void *pdev, uint8_t cfgidx) {
    (void)cfgidx;
    USBDEV_EP_Open(pdev, N32_USBHS_HID_IN_EP, HID_IN_PACKET, USB_EP_INT);
    USBDEV_EP_Open(pdev, N32_USBHS_HID_OUT_EP, HID_OUT_PACKET, USB_EP_INT);
    USBDEV_EP_PrepareRx(pdev, N32_USBHS_HID_OUT_EP, keyboard_led_report, HID_OUT_PACKET);
    USBDEV_SetEPStatus(pdev, N32_USBHS_HID_OUT_EP, USB_EP_RX_VALID);
    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_deinit(void *pdev, uint8_t cfgidx) {
    (void)cfgidx;
    USBDEV_EP_Close(pdev, N32_USBHS_HID_IN_EP);
    USBDEV_EP_Close(pdev, N32_USBHS_HID_OUT_EP);
    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_setup(void *pdev, USB_SETUP_REQ *req) {
    uint16_t len = 0;
    uint8_t *pbuf = NULL;
    uint8_t status = USBD_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS:
            switch (req->bRequest) {
                case N32_USBHS_KEYBOARD_REQ_SET_PROTOCOL:
                    keyboard_protocol = (uint8_t)req->wValue;
                    break;
                case N32_USBHS_KEYBOARD_REQ_GET_PROTOCOL:
                    USBD_CtrlSendData(pdev, (uint8_t *)&keyboard_protocol, 1);
                    break;
                case N32_USBHS_KEYBOARD_REQ_SET_IDLE:
                    keyboard_idle_state = (uint8_t)(req->wValue >> 8);
                    break;
                case N32_USBHS_KEYBOARD_REQ_GET_IDLE:
                    USBD_CtrlSendData(pdev, (uint8_t *)&keyboard_idle_state, 1);
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
                        len = MIN(N32_USBHS_KEYBOARD_REPORT_DESC_SIZE, req->wLength);
                        pbuf = keyboard_report_desc;
                    } else if ((req->wValue >> 8) == N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE) {
                        len = MIN(N32_USBHS_KEYBOARD_DESC_SIZE, req->wLength);
                        pbuf = keyboard_config_desc + 18;
                    }
                    USBD_CtrlSendData(pdev, pbuf, len);
                    break;
                case USB_REQ_GET_INTERFACE:
                    USBD_CtrlSendData(pdev, (uint8_t *)&keyboard_alt_set, 1);
                    break;
                case USB_REQ_SET_INTERFACE:
                    keyboard_alt_set = (uint8_t)req->wValue;
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

static uint8_t *n32_usbhs_keyboard_get_config(uint8_t speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(keyboard_config_desc);
    return keyboard_config_desc;
}

static uint8_t n32_usbhs_keyboard_datain(void *pdev, uint8_t epnum) {
    (void)epnum;
    USBDEV_EP_Flush(pdev, N32_USBHS_HID_IN_EP);
    n32_usbhs_keyboard_tx_done();
    return USBD_OK;
}

static uint8_t n32_usbhs_keyboard_dataout(void *pdev, uint8_t epnum) {
    if (epnum == N32_USBHS_HID_OUT_EP || epnum == (N32_USBHS_HID_OUT_EP & 0x0F)) {
        n32_usb_set_keyboard_leds(keyboard_led_report[0]);
    }

    USBDEV_EP_PrepareRx(pdev, N32_USBHS_HID_OUT_EP, keyboard_led_report, HID_OUT_PACKET);
    USBDEV_SetEPStatus(pdev, N32_USBHS_HID_OUT_EP, USB_EP_RX_VALID);
    return USBD_OK;
}

void n32_usbhs_user_init(void) {}
void n32_usbhs_user_device_reset(uint8_t speed) { (void)speed; }
void n32_usbhs_user_device_configured(void) {}
void n32_usbhs_user_device_suspended(void) {}
void n32_usbhs_user_device_resumed(void) {}
void n32_usbhs_user_device_connected(void) {}
void n32_usbhs_user_device_disconnected(void) {}
