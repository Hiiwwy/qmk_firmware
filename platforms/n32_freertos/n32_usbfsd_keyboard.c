#include "n32_usbfsd_keyboard.h"

#include "config.h"
#include "n32h47x_48x_exti.h"
#include "n32h47x_48x_gpio.h"
#include "n32h47x_48x_rcc.h"
#include "report.h"
#ifdef RAW_ENABLE
#    include "raw_hid.h"
#endif
#include "usb_driver.h"
#include "usbfsd_istr.h"
#include "usbfsd_lib.h"
#include "usbfsd_pwr.h"
#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_USB_SETUP)
#    include "SEGGER_RTT.h"
#endif

#include <string.h>

#ifndef VENDOR_ID
#    define VENDOR_ID 0x1FC9
#endif

#ifndef PRODUCT_ID
#    define PRODUCT_ID 0x0487
#endif

#ifndef DEVICE_VER
#    define DEVICE_VER 0x0001
#endif

#ifdef N32_USBFS_SDK_IDENTITY
#    define N32_USBFSD_VENDOR_ID 0x19F5
#    define N32_USBFSD_PRODUCT_ID 0x5560
#    define N32_USBFSD_DEVICE_VER 0x0200
#else
#    define N32_USBFSD_VENDOR_ID VENDOR_ID
#    define N32_USBFSD_PRODUCT_ID PRODUCT_ID
#    define N32_USBFSD_DEVICE_VER DEVICE_VER
#endif

#ifndef N32_UID_ALT_BASE
#    define N32_UID_ALT_BASE 0x1FFFF270U
#endif

#define LOBYTE(x) ((uint8_t)((x) & 0xFFU))
#define HIBYTE(x) ((uint8_t)(((x) >> 8) & 0xFFU))
#define USB_DEVICE_DESCRIPTOR_TYPE 0x01U
#define USB_CONFIGURATION_DESCRIPTOR_TYPE 0x02U
#define USB_STRING_DESCRIPTOR_TYPE 0x03U
#define USB_INTERFACE_DESCRIPTOR_TYPE 0x04U
#define USB_ENDPOINT_DESCRIPTOR_TYPE 0x05U
#define N32_USBFSD_DEVICE_DESC_SIZE 18U
#define N32_USBFSD_STRING_LANGID_SIZE 4U
#define N32_USBFSD_STRING_SERIAL_SIZE ((UID_LENGTH * 4U) + 2U)
#define N32_USBFSD_HID_DESC_SIZE 9U
#define N32_USBFSD_HID_DESC_OFFSET 18U
#define N32_USBFSD_RAW_HID_DESC_OFFSET 43U
#define N32_USBFSD_HID_DESCRIPTOR_TYPE 0x21U
#define N32_USBFSD_REPORT_DESCRIPTOR_TYPE 0x22U
#ifdef N32_USBFS_MINIMAL_KEYBOARD
#    define N32_USBFSD_SHARED_EP_SIZE 8U
#    define N32_USBFSD_SHARED_OUT_EP_SIZE 2U
#    define N32_USBFSD_SHARED_ENDPOINT_COUNT 2U
#    define N32_USBFSD_SHARED_INTERFACE_SUBCLASS 0x01U
#    define N32_USBFSD_SHARED_INTERFACE_PROTOCOL 0x01U
#    define N32_USBFSD_HID_BCD_LO 0x10U
#    define N32_USBFSD_HID_BCD_HI 0x01U
#    define N32_USBFSD_CONFIG_ATTRIBUTES 0xE0U
#    define N32_USBFSD_SHARED_POLLING_INTERVAL 0x20U
#else
#    define N32_USBFSD_SHARED_EP_SIZE N32_USBFSD_PACKET_SIZE
#    define N32_USBFSD_SHARED_OUT_EP_SIZE N32_USBFSD_PACKET_SIZE
#    define N32_USBFSD_SHARED_ENDPOINT_COUNT 1U
#    define N32_USBFSD_SHARED_INTERFACE_SUBCLASS 0x00U
#    define N32_USBFSD_SHARED_INTERFACE_PROTOCOL 0x00U
#    define N32_USBFSD_HID_BCD_LO 0x11U
#    define N32_USBFSD_HID_BCD_HI 0x01U
#    define N32_USBFSD_CONFIG_ATTRIBUTES 0xA0U
#    define N32_USBFSD_SHARED_POLLING_INTERVAL N32_USBFSD_POLLING_INTERVAL
#endif
#if defined(RAW_ENABLE) && !defined(N32_USBFS_SINGLE_INTERFACE)
#    define N32_USBFSD_HAS_RAW_INTERFACE
#endif
#ifdef N32_USBFSD_HAS_RAW_INTERFACE
#    define N32_USBFSD_INTERFACE_COUNT 2U
#    define N32_USBFSD_CONFIG_DESC_SIZE 66U
#else
#    define N32_USBFSD_INTERFACE_COUNT 1U
#    ifdef N32_USBFS_MINIMAL_KEYBOARD
#        define N32_USBFSD_CONFIG_DESC_SIZE 41U
#    else
#        define N32_USBFSD_CONFIG_DESC_SIZE 34U
#    endif
#endif

enum {
    N32_USBFSD_GET_REPORT = 1,
    N32_USBFSD_GET_IDLE,
    N32_USBFSD_GET_PROTOCOL,
    N32_USBFSD_SET_REPORT = 9,
    N32_USBFSD_SET_IDLE,
    N32_USBFSD_SET_PROTOCOL,
};

static void       n32_usbfsd_class_init(void);
static void       n32_usbfsd_reset(void);
static void       n32_usbfsd_set_configuration(void);
static void       n32_usbfsd_set_address(void);
static void       n32_usbfsd_status_in(void);
static void       n32_usbfsd_status_out(void);
static USB_Result n32_usbfsd_data_setup(uint8_t request);
static USB_Result n32_usbfsd_no_data_setup(uint8_t request);
static USB_Result n32_usbfsd_get_interface_setting(uint8_t interface, uint8_t alternate);
static uint8_t   *n32_usbfsd_get_device_descriptor(uint16_t length);
static uint8_t   *n32_usbfsd_get_config_descriptor(uint16_t length);
static uint8_t   *n32_usbfsd_get_string_descriptor(uint16_t length);
static uint8_t   *n32_usbfsd_get_shared_report_descriptor(uint16_t length);
#ifdef N32_USBFSD_HAS_RAW_INTERFACE
static uint8_t *n32_usbfsd_get_raw_report_descriptor(uint16_t length);
#endif
static uint8_t *n32_usbfsd_get_hid_descriptor(uint16_t length);
static uint8_t *n32_usbfsd_set_report(uint16_t length);
static uint8_t *n32_usbfsd_get_protocol(uint16_t length);
static uint8_t *n32_usbfsd_get_idle(uint16_t length);

static volatile uint8_t fs_shared_prev_xfer_complete = 1;
#ifdef RAW_ENABLE
static volatile uint8_t fs_raw_prev_xfer_complete = 1;
static volatile uint8_t fs_raw_report_pending;
static uint8_t          fs_raw_pending_report[N32_USBFSD_PACKET_SIZE];
#endif
static uint8_t fs_ep0_report[N32_USBFSD_PACKET_SIZE];
static uint8_t fs_shared_out_report[N32_USBFSD_PACKET_SIZE];
#ifdef N32_USBFSD_HAS_RAW_INTERFACE
static uint8_t fs_raw_out_report[N32_USBFSD_PACKET_SIZE];
#endif
static uint8_t fs_protocol[N32_USBFSD_INTERFACE_COUNT];
static uint8_t fs_idle[N32_USBFSD_INTERFACE_COUNT];
static uint8_t fs_ep0_interface;
static uint8_t fs_ep0_report_pending;
static uint8_t fs_string_desc[64];

n32_usbfsd_debug_counters_t n32_usbfsd_debug_counters;

USB_Device Device_Table = {EP_NUM, 1};

DEVICE_PROP Device_Property = {
    n32_usbfsd_class_init, n32_usbfsd_reset, n32_usbfsd_status_in, n32_usbfsd_status_out, n32_usbfsd_data_setup, n32_usbfsd_no_data_setup, n32_usbfsd_get_interface_setting, n32_usbfsd_get_device_descriptor, n32_usbfsd_get_config_descriptor, n32_usbfsd_get_string_descriptor, 0, N32_USBFSD_EP0_SIZE,
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
    USB_ProcessNop, n32_usbfsd_set_configuration, USB_ProcessNop, USB_ProcessNop, USB_ProcessNop, USB_ProcessNop, USB_ProcessNop, USB_ProcessNop, n32_usbfsd_set_address,
};

__IO uint32_t bDeviceState    = UNCONNECTED;
__IO bool     fSuspendEnabled = true;

static const uint8_t n32_usbfsd_device_desc[N32_USBFSD_DEVICE_DESC_SIZE] = {
    0x12, USB_DEVICE_DESCRIPTOR_TYPE, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, LOBYTE(N32_USBFSD_VENDOR_ID), HIBYTE(N32_USBFSD_VENDOR_ID), LOBYTE(N32_USBFSD_PRODUCT_ID), HIBYTE(N32_USBFSD_PRODUCT_ID), LOBYTE(N32_USBFSD_DEVICE_VER), HIBYTE(N32_USBFSD_DEVICE_VER), 1, 2, 3, 1,
};

static const uint8_t n32_usbfsd_langid_desc[N32_USBFSD_STRING_LANGID_SIZE] = {
    N32_USBFSD_STRING_LANGID_SIZE,
    USB_STRING_DESCRIPTOR_TYPE,
    0x09,
    0x04,
};

static uint8_t n32_usbfsd_serial_desc[N32_USBFSD_STRING_SERIAL_SIZE];

#ifdef N32_USBFS_MINIMAL_KEYBOARD
static const uint8_t shared_report_desc[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03, 0xC0,
};
#else
static const uint8_t shared_report_desc[] = {
    0x05,
    0x01,
    0x09,
    0x06,
    0xA1,
    0x01,
    0x85,
    REPORT_ID_KEYBOARD,
    0x05,
    0x07,
    0x19,
    0xE0,
    0x29,
    0xE7,
    0x15,
    0x00,
    0x25,
    0x01,
    0x75,
    0x01,
    0x95,
    0x08,
    0x81,
    0x02,
    0x95,
    0x01,
    0x75,
    0x08,
    0x81,
    0x01,
    0x05,
    0x07,
    0x19,
    0x00,
    0x2A,
    0xFF,
    0x00,
    0x15,
    0x00,
    0x26,
    0xFF,
    0x00,
    0x95,
    0x06,
    0x75,
    0x08,
    0x81,
    0x00,
    0x05,
    0x08,
    0x19,
    0x01,
    0x29,
    0x05,
    0x15,
    0x00,
    0x25,
    0x01,
    0x95,
    0x05,
    0x75,
    0x01,
    0x91,
    0x02,
    0x95,
    0x01,
    0x75,
    0x03,
    0x91,
    0x01,
    0xC0,
    0x05,
    0x01,
    0x09,
    0x02,
    0xA1,
    0x01,
    0x85,
    REPORT_ID_MOUSE,
    0x09,
    0x01,
    0xA1,
    0x00,
    0x05,
    0x09,
    0x19,
    0x01,
    0x29,
    0x08,
    0x15,
    0x00,
    0x25,
    0x01,
    0x95,
    0x08,
    0x75,
    0x01,
    0x81,
    0x02,
    0x05,
    0x01,
    0x09,
    0x30,
    0x09,
    0x31,
    0x15,
    0x81,
    0x25,
    0x7F,
    0x95,
    0x02,
    0x75,
    0x08,
    0x81,
    0x06,
    0x09,
    0x38,
    0x15,
    0x81,
    0x25,
    0x7F,
    0x95,
    0x01,
    0x75,
    0x08,
    0x81,
    0x06,
    0x05,
    0x0C,
    0x0A,
    0x38,
    0x02,
    0x15,
    0x81,
    0x25,
    0x7F,
    0x95,
    0x01,
    0x75,
    0x08,
    0x81,
    0x06,
    0xC0,
    0xC0,
    0x05,
    0x01,
    0x09,
    0x80,
    0xA1,
    0x01,
    0x85,
    REPORT_ID_SYSTEM,
    0x19,
    0x01,
    0x2A,
    0xB7,
    0x00,
    0x15,
    0x01,
    0x26,
    0xB7,
    0x00,
    0x95,
    0x01,
    0x75,
    0x10,
    0x81,
    0x00,
    0xC0,
    0x05,
    0x0C,
    0x09,
    0x01,
    0xA1,
    0x01,
    0x85,
    REPORT_ID_CONSUMER,
    0x19,
    0x01,
    0x2A,
    0xA0,
    0x02,
    0x15,
    0x01,
    0x26,
    0xA0,
    0x02,
    0x95,
    0x01,
    0x75,
    0x10,
    0x81,
    0x00,
    0xC0,
    0x05,
    0x01,
    0x09,
    0x06,
    0xA1,
    0x01,
    0x85,
    REPORT_ID_NKRO,
    0x05,
    0x07,
    0x19,
    0xE0,
    0x29,
    0xE7,
    0x15,
    0x00,
    0x25,
    0x01,
    0x95,
    0x08,
    0x75,
    0x01,
    0x81,
    0x02,
    0x05,
    0x07,
    0x19,
    0x00,
    0x29,
    0xEF,
    0x15,
    0x00,
    0x25,
    0x01,
    0x95,
    0xF0,
    0x75,
    0x01,
    0x81,
    0x02,
    0x05,
    0x08,
    0x19,
    0x01,
    0x29,
    0x05,
    0x95,
    0x05,
    0x75,
    0x01,
    0x91,
    0x02,
    0x95,
    0x01,
    0x75,
    0x03,
    0x91,
    0x01,
    0xC0,
};
#endif

#ifdef N32_USBFSD_HAS_RAW_INTERFACE
static const uint8_t raw_report_desc[] = {
    0x06, 0x60, 0xFF, 0x09, 0x61, 0xA1, 0x01, 0x09, 0x62, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, N32_USBFSD_PACKET_SIZE, 0x75, 0x08, 0x81, 0x02, 0x09, 0x63, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, N32_USBFSD_PACKET_SIZE, 0x75, 0x08, 0x91, 0x02, 0xC0,
};
#endif

static const uint8_t n32_usbfsd_config_desc[N32_USBFSD_CONFIG_DESC_SIZE] = {
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    LOBYTE(N32_USBFSD_CONFIG_DESC_SIZE),
    HIBYTE(N32_USBFSD_CONFIG_DESC_SIZE),
    N32_USBFSD_INTERFACE_COUNT,
    0x01,
    0x00,
    N32_USBFSD_CONFIG_ATTRIBUTES,
    0x32,
    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    N32_USBFSD_SHARED_ENDPOINT_COUNT,
    0x03,
    N32_USBFSD_SHARED_INTERFACE_SUBCLASS,
    N32_USBFSD_SHARED_INTERFACE_PROTOCOL,
    0x00,
    0x09,
    N32_USBFSD_HID_DESCRIPTOR_TYPE,
    N32_USBFSD_HID_BCD_LO,
    N32_USBFSD_HID_BCD_HI,
    0x00,
    0x01,
    N32_USBFSD_REPORT_DESCRIPTOR_TYPE,
    LOBYTE(sizeof(shared_report_desc)),
    HIBYTE(sizeof(shared_report_desc)),
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBFSD_SHARED_IN_EP,
    0x03,
    LOBYTE(N32_USBFSD_SHARED_EP_SIZE),
    HIBYTE(N32_USBFSD_SHARED_EP_SIZE),
    N32_USBFSD_SHARED_POLLING_INTERVAL,
#ifdef N32_USBFS_MINIMAL_KEYBOARD
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBFSD_SHARED_OUT_EP,
    0x03,
    LOBYTE(N32_USBFSD_SHARED_OUT_EP_SIZE),
    HIBYTE(N32_USBFSD_SHARED_OUT_EP_SIZE),
    N32_USBFSD_SHARED_POLLING_INTERVAL,
#endif
#ifdef N32_USBFSD_HAS_RAW_INTERFACE
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
    N32_USBFSD_HID_DESCRIPTOR_TYPE,
    0x11,
    0x01,
    0x00,
    0x01,
    N32_USBFSD_REPORT_DESCRIPTOR_TYPE,
    LOBYTE(sizeof(raw_report_desc)),
    HIBYTE(sizeof(raw_report_desc)),
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBFSD_RAW_IN_EP,
    0x03,
    LOBYTE(N32_USBFSD_PACKET_SIZE),
    HIBYTE(N32_USBFSD_PACKET_SIZE),
    N32_USBFSD_POLLING_INTERVAL,
    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    N32_USBFSD_RAW_OUT_EP,
    0x03,
    LOBYTE(N32_USBFSD_PACKET_SIZE),
    HIBYTE(N32_USBFSD_PACKET_SIZE),
    N32_USBFSD_POLLING_INTERVAL,
#endif
};

static USB_OneDescriptor device_descriptor        = {(uint8_t *)n32_usbfsd_device_desc, sizeof(n32_usbfsd_device_desc)};
static USB_OneDescriptor config_descriptor        = {(uint8_t *)n32_usbfsd_config_desc, sizeof(n32_usbfsd_config_desc)};
static USB_OneDescriptor shared_report_descriptor = {(uint8_t *)shared_report_desc, sizeof(shared_report_desc)};
#ifdef N32_USBFSD_HAS_RAW_INTERFACE
static USB_OneDescriptor raw_report_descriptor = {(uint8_t *)raw_report_desc, sizeof(raw_report_desc)};
#endif
static USB_OneDescriptor langid_descriptor = {(uint8_t *)n32_usbfsd_langid_desc, sizeof(n32_usbfsd_langid_desc)};
static USB_OneDescriptor serial_descriptor = {n32_usbfsd_serial_desc, sizeof(n32_usbfsd_serial_desc)};
static USB_OneDescriptor hid_descriptor    = {(uint8_t *)n32_usbfsd_config_desc + N32_USBFSD_HID_DESC_OFFSET, N32_USBFSD_HID_DESC_SIZE};

#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_USB_SETUP)
static void n32_usbfsd_trace_write(const char *text) {
    SEGGER_RTT_Write(0, text, strlen(text));
}

static void n32_usbfsd_trace_hex8(uint8_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char              text[2];

    text[0] = hex[(value >> 4) & 0x0F];
    text[1] = hex[value & 0x0F];
    SEGGER_RTT_Write(0, text, sizeof(text));
}

static void n32_usbfsd_trace_hex16(uint16_t value) {
    n32_usbfsd_trace_hex8((uint8_t)(value >> 8));
    n32_usbfsd_trace_hex8((uint8_t)value);
}

static void n32_usbfsd_trace_setup(const char *tag, uint8_t request) {
    n32_usbfsd_trace_write("N32 usbfsd ");
    n32_usbfsd_trace_write(tag);
    n32_usbfsd_trace_write(" type=0x");
    n32_usbfsd_trace_hex8((uint8_t)Type_Recipient);
    n32_usbfsd_trace_write(" req=0x");
    n32_usbfsd_trace_hex8(request);
    n32_usbfsd_trace_write(" val=0x");
    n32_usbfsd_trace_hex16(pInformation->USBwValue);
    n32_usbfsd_trace_write(" idx=0x");
    n32_usbfsd_trace_hex16(pInformation->USBwIndex);
    n32_usbfsd_trace_write(" len=0x");
    n32_usbfsd_trace_hex16(pInformation->USBwLength);
    n32_usbfsd_trace_write("\n");
}

static void n32_usbfsd_trace_state(const char *tag, uint8_t value) {
    n32_usbfsd_trace_write("N32 usbfsd ");
    n32_usbfsd_trace_write(tag);
    n32_usbfsd_trace_write(" 0x");
    n32_usbfsd_trace_hex8(value);
    n32_usbfsd_trace_write("\n");
}
#else
#    define n32_usbfsd_trace_write(text) ((void)0)
#    define n32_usbfsd_trace_setup(tag, request) ((void)0)
#    define n32_usbfsd_trace_state(tag, value) ((void)0)
#endif

static void n32_usbfsd_debug_record_request(uint8_t request) {
    n32_usbfsd_debug_counters.last_request = request;
    n32_usbfsd_debug_counters.last_type    = (uint8_t)Type_Recipient;
    n32_usbfsd_debug_counters.last_value   = pInformation->USBwValue;
    n32_usbfsd_debug_counters.last_index   = pInformation->USBwIndex;
    n32_usbfsd_debug_counters.last_length  = pInformation->USBwLength;
}

void n32_usbfsd_debug_snapshot(n32_usbfsd_debug_counters_t *snapshot) {
    *snapshot                = n32_usbfsd_debug_counters;
    snapshot->cntr           = _GetCNTR();
    snapshot->istr           = _GetISTR();
    snapshot->fnr            = _GetFNR();
    snapshot->daddr          = _GetDADDR();
    snapshot->ep0r           = _GetENDPOINT(ENDP0);
    snapshot->ep1r           = _GetENDPOINT(ENDP1);
    snapshot->interrupt_mask = wInterrupt_Mask;
    snapshot->rcc_cfg3       = RCC->CFG3;
    snapshot->rcc_apb1pclken = RCC->APB1PCLKEN;
    snapshot->rcc_ahb1pclken = RCC->AHB1PCLKEN;
}

static uint8_t *n32_usbfsd_make_string(const char *string, uint16_t *length) {
    uint8_t i = 0;

    fs_string_desc[1] = USB_STRING_DESCRIPTOR_TYPE;
    while (string[i] != '\0' && (uint16_t)(2U + (i * 2U) + 1U) < sizeof(fs_string_desc)) {
        fs_string_desc[2U + (i * 2U)]      = (uint8_t)string[i];
        fs_string_desc[2U + (i * 2U) + 1U] = 0;
        i++;
    }
    fs_string_desc[0] = 2U + (i * 2U);
    *length           = fs_string_desc[0];
    return fs_string_desc;
}

static void n32_usbfsd_update_serial(void) {
    static const char        hex[]    = "0123456789ABCDEF";
    const volatile uint32_t *uid_addr = *(const volatile uint32_t *)N32_UID_ALT_BASE == 0xFFFFFFFFU ? (const volatile uint32_t *)UID_BASE : (const volatile uint32_t *)N32_UID_ALT_BASE;
    uint8_t                  uid[UID_LENGTH];

    n32_usbfsd_serial_desc[0] = sizeof(n32_usbfsd_serial_desc);
    n32_usbfsd_serial_desc[1] = USB_STRING_DESCRIPTOR_TYPE;

    for (uint32_t i = 0; i < UID_LENGTH; i += 4) {
        uint32_t word = *uid_addr++;

        uid[i + 0] = (uint8_t)word;
        uid[i + 1] = (uint8_t)(word >> 8);
        uid[i + 2] = (uint8_t)(word >> 16);
        uid[i + 3] = (uint8_t)(word >> 24);
    }

    for (uint32_t i = 0; i < UID_LENGTH; ++i) {
        n32_usbfsd_serial_desc[2U + (i * 4U)]      = (uint8_t)hex[(uid[i] >> 4) & 0x0F];
        n32_usbfsd_serial_desc[2U + (i * 4U) + 1U] = 0;
        n32_usbfsd_serial_desc[2U + (i * 4U) + 2U] = (uint8_t)hex[uid[i] & 0x0F];
        n32_usbfsd_serial_desc[2U + (i * 4U) + 3U] = 0;
    }
}

static void n32_usbfsd_configure_io(void) {
    GPIO_InitType gpio;

    RCC_EnableAHB1PeriphClk(RCC_AHB_PERIPHEN_GPIOA, ENABLE);
    GPIO_InitStruct(&gpio);
    gpio.GPIO_Mode      = GPIO_MODE_AF_PP;
    gpio.GPIO_Alternate = GPIO_AF10;
    gpio.Pin            = GPIO_PIN_11;
    GPIO_InitPeripheral(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_12;
    GPIO_InitPeripheral(GPIOA, &gpio);
}

static void n32_usbfsd_configure_clock(void) {
    RCC_ConfigUSBFSClk(RCC_USBFS_CLKSRC_PLLPRES);
    RCC_ConfigUSBPLLPresClk(RCC_USBPLLCLK_SRC_PLL, RCC_USBPLLCLK_DIV5);
    RCC->CFG3 |= RCC_CFG3_USBFSTM;
    RCC_EnableAPB1PeriphReset(RCC_APB1_PERIPH_USBFS);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USBFS, ENABLE);
}

static void n32_usbfsd_configure_interrupts(void) {
    EXTI_InitType exti;

    NVIC_SetPriorityGrouping(3);
    NVIC_SetPriority(USB_FS_LP_IRQn, NVIC_EncodePriority(3, 2, 0));
    NVIC_EnableIRQ(USB_FS_LP_IRQn);
    NVIC_SetPriority(USB_FS_WKUP_IRQn, NVIC_EncodePriority(3, 1, 0));
    NVIC_EnableIRQ(USB_FS_WKUP_IRQn);

    EXTI_ClrITPendBit(EXTI_LINE18);
    exti.EXTI_Line    = EXTI_LINE18;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&exti);
}

void n32_usbfsd_init(void) {
    n32_usbfsd_update_serial();
    n32_usbfsd_configure_io();
    n32_usbfsd_configure_clock();
    n32_usbfsd_configure_interrupts();
    USB_Init();
}

static void n32_usbfsd_class_init(void) {
    pInformation->CurrentConfiguration = 0;
    PowerOn();
    USB_SilInit();
    _EnPortPullup();
    bDeviceState = UNCONNECTED;
}

static void n32_usbfsd_reset(void) {
    n32_usbfsd_trace_write("N32 usbfsd reset\n");

    pInformation->CurrentConfiguration = 0;
    pInformation->CurrentInterface     = 0;
    pInformation->CurrentFeature       = n32_usbfsd_config_desc[7];

    USB_SetBuftab(BTABLE_ADDRESS);

    USB_SetEpType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_STALL);
    USB_SetEpRxAddr(ENDP0, ENDP0_RXADDR);
    USB_SetEpTxAddr(ENDP0, ENDP0_TXADDR);
    USB_ClrStsOut(ENDP0);
    USB_SetEpRxCnt(ENDP0, Device_Property.MaxPacketSize);
    USB_SetEpRxValid(ENDP0);

    USB_SetEpType(ENDP1, EP_INTERRUPT);
    USB_SetEpTxAddr(ENDP1, ENDP1_TXADDR);
    USB_SetEpRxAddr(ENDP1, ENDP1_RXADDR);
#ifdef N32_USBFS_MINIMAL_KEYBOARD
    USB_SetEpTxCnt(ENDP1, N32_USBFSD_SHARED_EP_SIZE);
    USB_SetEpRxCnt(ENDP1, N32_USBFSD_SHARED_EP_SIZE);
    SetEPRxStatus(ENDP1, EP_RX_VALID);
    SetEPTxStatus(ENDP1, EP_TX_VALID);
#else
    USB_SetEpTxCnt(ENDP1, 0);
    USB_SetEpRxCnt(ENDP1, N32_USBFSD_SHARED_OUT_EP_SIZE);
    SetEPRxStatus(ENDP1, EP_RX_VALID);
    SetEPTxStatus(ENDP1, EP_TX_NAK);
#endif
    fs_shared_prev_xfer_complete = 1;

#ifdef N32_USBFSD_HAS_RAW_INTERFACE
    USB_SetEpType(ENDP2, EP_INTERRUPT);
    USB_SetEpTxAddr(ENDP2, ENDP2_TXADDR);
    USB_SetEpTxCnt(ENDP2, 0);
    SetEPTxStatus(ENDP2, EP_TX_NAK);

    USB_SetEpType(ENDP3, EP_INTERRUPT);
    USB_SetEpRxAddr(ENDP3, ENDP3_RXADDR);
    USB_SetEpRxCnt(ENDP3, N32_USBFSD_PACKET_SIZE);
    SetEPRxStatus(ENDP3, EP_RX_VALID);
    SetEPTxStatus(ENDP3, EP_TX_NAK);

    fs_raw_prev_xfer_complete = 1;
#endif

    USB_SetDeviceAddress(0);
    bDeviceState = ATTACHED;
}

static void n32_usbfsd_set_configuration(void) {
    n32_usbfsd_trace_state("set_config", pInformation->CurrentConfiguration);
    n32_usbfsd_debug_record_request(SET_CONFIGURATION);
    n32_usbfsd_debug_counters.set_configuration_count++;

    if (pInformation->CurrentConfiguration != 0) {
        bDeviceState = CONFIGURED;
    }
}

static void n32_usbfsd_set_address(void) {
    n32_usbfsd_trace_state("set_address", pInformation->USBwValue0);
    n32_usbfsd_debug_record_request(SET_ADDRESS);
    n32_usbfsd_debug_counters.set_address_count++;
    bDeviceState = ADDRESSED;
}

static void n32_usbfsd_status_in(void) {
    if (fs_ep0_report_pending) {
        if (fs_ep0_interface == 0) {
            uint8_t led_state = fs_ep0_report[0] == REPORT_ID_KEYBOARD || fs_ep0_report[0] == REPORT_ID_NKRO ? fs_ep0_report[1] : fs_ep0_report[0];
            n32_usb_set_keyboard_leds(led_state);
        }
        fs_ep0_report_pending = 0;
    }
}

static void n32_usbfsd_status_out(void) {}

static USB_Result n32_usbfsd_get_interface_setting(uint8_t interface, uint8_t alternate) {
    return interface < N32_USBFSD_INTERFACE_COUNT && alternate == 0 ? Success : UnSupport;
}

static USB_Result n32_usbfsd_data_setup(uint8_t request) {
    uint8_t *(*copy_routine)(uint16_t) = NULL;
    uint8_t interface                  = pInformation->USBwIndex0;

    n32_usbfsd_debug_record_request(request);

    if (interface >= N32_USBFSD_INTERFACE_COUNT) {
        n32_usbfsd_trace_setup("data_bad_interface", request);
        n32_usbfsd_debug_counters.unsupported_count++;
        return UnSupport;
    }

    if (request == GET_DESCRIPTOR && Type_Recipient == (STANDARD_REQUEST | INTERFACE_RECIPIENT)) {
        if (pInformation->USBwValue1 == N32_USBFSD_REPORT_DESCRIPTOR_TYPE) {
            n32_usbfsd_trace_setup("get_report_desc", request);
            n32_usbfsd_debug_counters.report_desc_count++;
#ifdef N32_USBFSD_HAS_RAW_INTERFACE
            copy_routine = interface == 1 ? n32_usbfsd_get_raw_report_descriptor : n32_usbfsd_get_shared_report_descriptor;
#else
            copy_routine = n32_usbfsd_get_shared_report_descriptor;
#endif
        } else if (pInformation->USBwValue1 == N32_USBFSD_HID_DESCRIPTOR_TYPE) {
            n32_usbfsd_trace_setup("get_hid_desc", request);
            n32_usbfsd_debug_counters.hid_desc_count++;
            hid_descriptor.Descriptor = (uint8_t *)n32_usbfsd_config_desc + (interface == 1 ? N32_USBFSD_RAW_HID_DESC_OFFSET : N32_USBFSD_HID_DESC_OFFSET);
            copy_routine              = n32_usbfsd_get_hid_descriptor;
        }
    } else if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
        n32_usbfsd_trace_setup("class_data", request);
        n32_usbfsd_debug_counters.class_data_count++;
        switch (request) {
            case N32_USBFSD_GET_PROTOCOL:
                copy_routine = n32_usbfsd_get_protocol;
                break;
            case N32_USBFSD_GET_IDLE:
                copy_routine = n32_usbfsd_get_idle;
                break;
            case N32_USBFSD_SET_REPORT:
                fs_ep0_interface      = interface;
                fs_ep0_report_pending = 1;
                copy_routine          = n32_usbfsd_set_report;
                break;
            default:
                break;
        }
    }

    if (copy_routine == NULL) {
        n32_usbfsd_trace_setup("data_unsupported", request);
        n32_usbfsd_debug_counters.unsupported_count++;
        return UnSupport;
    }

    pInformation->Ctrl_Info.CopyData    = copy_routine;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    copy_routine(0);
    return Success;
}

static USB_Result n32_usbfsd_no_data_setup(uint8_t request) {
    uint8_t interface = pInformation->USBwIndex0;

    n32_usbfsd_debug_record_request(request);

    if (interface >= N32_USBFSD_INTERFACE_COUNT || Type_Recipient != (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
        n32_usbfsd_trace_setup("nodata_unsupported", request);
        n32_usbfsd_debug_counters.unsupported_count++;
        return UnSupport;
    }

    n32_usbfsd_trace_setup("class_nodata", request);
    n32_usbfsd_debug_counters.class_nodata_count++;

    switch (request) {
        case N32_USBFSD_SET_PROTOCOL:
            fs_protocol[interface] = pInformation->USBwValue0;
            return Success;
        case N32_USBFSD_SET_IDLE:
            fs_idle[interface] = pInformation->USBwValue1;
            return Success;
        default:
            return UnSupport;
    }
}

static uint8_t *n32_usbfsd_get_device_descriptor(uint16_t length) {
    if (length == 0) {
        n32_usbfsd_trace_setup("get_device_desc", pInformation->bRequest);
        n32_usbfsd_debug_record_request(pInformation->bRequest);
        n32_usbfsd_debug_counters.device_desc_count++;
    }
    return Standard_GetDescriptorData(length, &device_descriptor);
}

static uint8_t *n32_usbfsd_get_config_descriptor(uint16_t length) {
    if (length == 0) {
        n32_usbfsd_trace_setup("get_config_desc", pInformation->bRequest);
        n32_usbfsd_debug_record_request(pInformation->bRequest);
        n32_usbfsd_debug_counters.config_desc_count++;
    }
    return Standard_GetDescriptorData(length, &config_descriptor);
}

static uint8_t *n32_usbfsd_get_string_descriptor(uint16_t length) {
    uint8_t index = pInformation->USBwValue0;

    if (length == 0) {
        n32_usbfsd_trace_setup("get_string_desc", pInformation->bRequest);
        n32_usbfsd_debug_record_request(pInformation->bRequest);
        n32_usbfsd_debug_counters.string_desc_count++;
    }

    if (index == 0) {
        return Standard_GetDescriptorData(length, &langid_descriptor);
    }
    if (index == 1) {
        uint16_t string_length;
        n32_usbfsd_make_string(MANUFACTURER, &string_length);
        USB_OneDescriptor descriptor = {fs_string_desc, string_length};
        return Standard_GetDescriptorData(length, &descriptor);
    }
    if (index == 2) {
        uint16_t string_length;
        n32_usbfsd_make_string(PRODUCT, &string_length);
        USB_OneDescriptor descriptor = {fs_string_desc, string_length};
        return Standard_GetDescriptorData(length, &descriptor);
    }
    if (index == 3) {
        return Standard_GetDescriptorData(length, &serial_descriptor);
    }

    return NULL;
}

static uint8_t *n32_usbfsd_get_shared_report_descriptor(uint16_t length) {
    return Standard_GetDescriptorData(length, &shared_report_descriptor);
}

#ifdef N32_USBFSD_HAS_RAW_INTERFACE
static uint8_t *n32_usbfsd_get_raw_report_descriptor(uint16_t length) {
    return Standard_GetDescriptorData(length, &raw_report_descriptor);
}
#endif

static uint8_t *n32_usbfsd_get_hid_descriptor(uint16_t length) {
    return Standard_GetDescriptorData(length, &hid_descriptor);
}

static uint8_t *n32_usbfsd_set_report(uint16_t length) {
    if (length == 0) {
        pInformation->Ctrl_Info.Usb_wLength = pInformation->USBwLength < N32_USBFSD_PACKET_SIZE ? pInformation->USBwLength : N32_USBFSD_PACKET_SIZE;
        return NULL;
    }

    return fs_ep0_report;
}

static uint8_t *n32_usbfsd_get_protocol(uint16_t length) {
    if (length == 0) {
        pInformation->Ctrl_Info.Usb_wLength = 1;
        return NULL;
    }

    return &fs_protocol[pInformation->USBwIndex0];
}

static uint8_t *n32_usbfsd_get_idle(uint16_t length) {
    if (length == 0) {
        pInformation->Ctrl_Info.Usb_wLength = 1;
        return NULL;
    }

    return &fs_idle[pInformation->USBwIndex0];
}

bool n32_usbfsd_send_shared(const void *report, uint8_t length) {
    if (bDeviceState != CONFIGURED || !fs_shared_prev_xfer_complete || length > N32_USBFSD_SHARED_EP_SIZE) {
        return false;
    }

    fs_shared_prev_xfer_complete = 0;
    USB_SilWrite(N32_USBFSD_SHARED_IN_EP, (uint8_t *)report, length);
    SetEPTxStatus(ENDP1, EP_TX_VALID);
    return true;
}

#ifdef RAW_ENABLE
bool n32_usbfsd_send_raw_hid(const uint8_t *data, uint8_t length) {
#    ifndef N32_USBFSD_HAS_RAW_INTERFACE
    (void)data;
    (void)length;
    return false;
#    else
    if (bDeviceState != CONFIGURED || !fs_raw_prev_xfer_complete || length != N32_USBFSD_PACKET_SIZE) {
        return false;
    }

    fs_raw_prev_xfer_complete = 0;
    USB_SilWrite(N32_USBFSD_RAW_IN_EP, (uint8_t *)data, N32_USBFSD_PACKET_SIZE);
    SetEPTxStatus(ENDP2, EP_TX_VALID);
    return true;
#    endif
}

void n32_usbfsd_raw_hid_task(void) {
    uint8_t data[N32_USBFSD_PACKET_SIZE];

    if (!fs_raw_report_pending) {
        return;
    }

    memcpy(data, fs_raw_pending_report, sizeof(data));
    fs_raw_report_pending = 0;
    raw_hid_receive(data, sizeof(data));
}
#endif

void EP1_IN_Callback(void) {
    fs_shared_prev_xfer_complete = 1;
}

void EP1_OUT_Callback(void) {
    uint32_t length = USB_SilRead(N32_USBFSD_SHARED_OUT_EP, fs_shared_out_report);

    if (length > 0) {
        uint8_t led_state = fs_shared_out_report[0] == REPORT_ID_KEYBOARD || fs_shared_out_report[0] == REPORT_ID_NKRO ? fs_shared_out_report[1] : fs_shared_out_report[0];
        n32_usb_set_keyboard_leds(led_state);
    }

    SetEPRxStatus(ENDP1, EP_RX_VALID);
}

#ifdef RAW_ENABLE
#    ifdef N32_USBFSD_HAS_RAW_INTERFACE
void EP2_IN_Callback(void) {
    fs_raw_prev_xfer_complete = 1;
}

void EP3_OUT_Callback(void) {
    USB_SilRead(N32_USBFSD_RAW_OUT_EP, fs_raw_out_report);
    if (!fs_raw_report_pending) {
        memcpy(fs_raw_pending_report, fs_raw_out_report, sizeof(fs_raw_pending_report));
        fs_raw_report_pending = 1;
    }
    SetEPRxStatus(ENDP3, EP_RX_VALID);
}
#    endif
#endif

USB_Result PowerOn(void) {
    wInterrupt_Mask = 0;
    _SetCNTR(CTRL_FRST);
    _SetCNTR(wInterrupt_Mask);
    _SetISTR(0);
    wInterrupt_Mask = CTRL_RSTM | CTRL_SUSPDM | CTRL_WKUPM;
    _SetCNTR(wInterrupt_Mask);
    return Success;
}

USB_Result PowerOff(void) {
    _SetCNTR(CTRL_FRST);
    _SetISTR(0);
    _SetCNTR(CTRL_FRST | CTRL_PD);
    return Success;
}

static void n32_usbfsd_resume_init(void) {
    uint16_t cntr = _GetCNTR();

    cntr &= (uint16_t)~CTRL_LP_MODE;
    _SetCNTR(cntr);
    if (pInformation->CurrentConfiguration != 0) {
        bDeviceState = CONFIGURED;
    } else {
        bDeviceState = ATTACHED;
    }
    _SetCNTR(IMR_MSK);
    _EnPortPullup();
}

void Suspend(void) {
    uint16_t cntr = _GetCNTR();

    bDeviceState = SUSPENDED;
    cntr |= CTRL_FSUSPD;
    _SetCNTR(cntr);
    cntr = _GetCNTR();
    cntr |= CTRL_LP_MODE;
    _SetCNTR(cntr);
}

void Resume(RESUME_STATE eResumeSetVal) {
    static RESUME_STATE state = RESUME_OFF;
    static uint8_t      esof_count;
    uint16_t            cntr;

    if (eResumeSetVal != RESUME_ESOF) {
        state = eResumeSetVal;
    }

    switch (state) {
        case RESUME_EXTERNAL:
            n32_usbfsd_resume_init();
            state = RESUME_OFF;
            break;
        case RESUME_INTERNAL:
            n32_usbfsd_resume_init();
            state      = RESUME_START;
            esof_count = 10;
            break;
        case RESUME_LATER:
            esof_count = 2;
            state      = RESUME_WAIT;
            break;
        case RESUME_WAIT:
            if (esof_count-- == 0) {
                state = RESUME_START;
            }
            break;
        case RESUME_START:
            cntr = _GetCNTR();
            cntr |= CTRL_RESUM;
            _SetCNTR(cntr);
            state      = RESUME_ON;
            esof_count = 10;
            break;
        case RESUME_ON:
            if (esof_count-- == 0) {
                cntr = _GetCNTR();
                cntr &= (uint16_t)~CTRL_RESUM;
                _SetCNTR(cntr);
                state = RESUME_OFF;
            }
            break;
        default:
            state = RESUME_OFF;
            break;
    }
}
