#include "usbd_desc.h"

#include "config.h"
#include "usbd_req.h"

#ifndef VENDOR_ID
#    define VENDOR_ID 0x1FC9
#endif

#ifndef PRODUCT_ID
#    define PRODUCT_ID 0x0487
#endif

#ifndef DEVICE_VER
#    define DEVICE_VER 0x0001
#endif

#ifndef MANUFACTURER
#    define MANUFACTURER "Nations"
#endif

#ifndef PRODUCT
#    define PRODUCT "N32 Devboard"
#endif

#define USBD_LANGID_STRING 0x0409

static uint8_t *n32_usbhs_string(const char *string, uint16_t *length) {
    USBD_GetString((uint8_t *)string, USBD_StrDesc, length);
    return USBD_StrDesc;
}

USBD_DEVICE_DESC USBD_desc = {
    USBD_USER_DeviceDescriptor,
    USBD_USER_LangIDStrDescriptor,
    USBD_USER_ManufacturerStrDescriptor,
    USBD_USER_ProductStrDescriptor,
    USBD_USER_SerialStrDescriptor,
    USBD_USER_ConfigStrDescriptor,
    USBD_USER_InterfaceStrDescriptor,
};

__ALIGN_BEGIN uint8_t USBD_DeviceDesc[USB_SIZ_DEVICE_DESC] __ALIGN_END = {
    0x12,
    USB_DEVICE_DESCRIPTOR_TYPE,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    USB_MAX_EP0_SIZE,
    LOBYTE(VENDOR_ID),
    HIBYTE(VENDOR_ID),
    LOBYTE(PRODUCT_ID),
    HIBYTE(PRODUCT_ID),
    LOBYTE(DEVICE_VER),
    HIBYTE(DEVICE_VER),
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_CFG_MAX_NUM,
};

__ALIGN_BEGIN uint8_t USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
};

__ALIGN_BEGIN uint8_t USBD_LangIDDesc[USB_SIZ_STRING_LANGID] __ALIGN_END = {
    USB_SIZ_STRING_LANGID,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

__ALIGN_BEGIN uint8_t USBD_StrDesc[USB_MAX_STR_DESC_SIZ] __ALIGN_END;

uint8_t *USBD_USER_DeviceDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

uint8_t *USBD_USER_LangIDStrDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

uint8_t *USBD_USER_ManufacturerStrDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    return n32_usbhs_string(MANUFACTURER, length);
}

uint8_t *USBD_USER_ProductStrDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    return n32_usbhs_string(PRODUCT, length);
}

uint8_t *USBD_USER_SerialStrDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    return n32_usbhs_string("N32H487000001", length);
}

uint8_t *USBD_USER_ConfigStrDescriptor(uint8_t speed, uint16_t *length) {
    return n32_usbhs_string(speed == USB_SPEED_HIGH ? "QMK HS Keyboard" : "QMK FS Keyboard", length);
}

uint8_t *USBD_USER_InterfaceStrDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    return n32_usbhs_string("QMK HID Keyboard", length);
}
