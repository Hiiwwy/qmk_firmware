#include "usbd_desc.h"

#include "config.h"
#include "n32h47x_48x.h"
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
#define N32_USB_SERIAL_CHARS (UID_LENGTH * 2U)

#ifndef N32_UID_ALT_BASE
#    define N32_UID_ALT_BASE 0x1FFFF270U
#endif

static char n32_usb_serial[N32_USB_SERIAL_CHARS + 1];

static uint8_t *n32_usbhs_string(const char *string, uint16_t *length) {
    USBD_GetString((uint8_t *)string, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static void n32_usbhs_read_uid(uint8_t *uid) {
    const volatile uint32_t *uid_addr = *(const volatile uint32_t *)N32_UID_ALT_BASE == 0xFFFFFFFFU ? (const volatile uint32_t *)UID_BASE : (const volatile uint32_t *)N32_UID_ALT_BASE;

    for (uint32_t i = 0; i < UID_LENGTH; i += 4) {
        uint32_t word = *uid_addr++;

        uid[i + 0] = (uint8_t)word;
        uid[i + 1] = (uint8_t)(word >> 8);
        uid[i + 2] = (uint8_t)(word >> 16);
        uid[i + 3] = (uint8_t)(word >> 24);
    }
}

static const char *n32_usbhs_serial_string(void) {
    static const char hex[] = "0123456789ABCDEF";
    uint8_t           uid[UID_LENGTH];

    n32_usbhs_read_uid(uid);
    for (uint32_t i = 0; i < UID_LENGTH; ++i) {
        n32_usb_serial[(i * 2U) + 0U] = hex[(uid[i] >> 4) & 0x0F];
        n32_usb_serial[(i * 2U) + 1U] = hex[uid[i] & 0x0F];
    }
    n32_usb_serial[N32_USB_SERIAL_CHARS] = '\0';

    return n32_usb_serial;
}

USBD_DEVICE_DESC USBD_desc = {
    USBD_USER_DeviceDescriptor, USBD_USER_LangIDStrDescriptor, USBD_USER_ManufacturerStrDescriptor, USBD_USER_ProductStrDescriptor, USBD_USER_SerialStrDescriptor, USBD_USER_ConfigStrDescriptor, USBD_USER_InterfaceStrDescriptor,
};

__ALIGN_BEGIN uint8_t USBD_DeviceDesc[USB_SIZ_DEVICE_DESC] __ALIGN_END = {
    0x12, USB_DEVICE_DESCRIPTOR_TYPE, 0x00, 0x02, 0x00, 0x00, 0x00, USB_MAX_EP0_SIZE, LOBYTE(VENDOR_ID), HIBYTE(VENDOR_ID), LOBYTE(PRODUCT_ID), HIBYTE(PRODUCT_ID), LOBYTE(DEVICE_VER), HIBYTE(DEVICE_VER), USBD_IDX_MFC_STR, USBD_IDX_PRODUCT_STR, USBD_IDX_SERIAL_STR, USBD_CFG_MAX_NUM,
};

__ALIGN_BEGIN uint8_t USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00,
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
    return n32_usbhs_string(n32_usbhs_serial_string(), length);
}

uint8_t *USBD_USER_ConfigStrDescriptor(uint8_t speed, uint16_t *length) {
    return n32_usbhs_string(speed == USB_SPEED_HIGH ? "QMK HS Keyboard" : "QMK FS Keyboard", length);
}

uint8_t *USBD_USER_InterfaceStrDescriptor(uint8_t speed, uint16_t *length) {
    (void)speed;
    return n32_usbhs_string("QMK HID Keyboard", length);
}
