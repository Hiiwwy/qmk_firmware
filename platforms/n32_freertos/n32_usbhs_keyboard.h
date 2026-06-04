#pragma once

#include "usbd_ioreq.h"

#define N32_USBHS_HID_IN_EP HID_IN_EP
#define N32_USBHS_HID_OUT_EP HID_OUT_EP
#define N32_USBHS_RAW_IN_EP RAW_IN_EP
#define N32_USBHS_RAW_OUT_EP RAW_OUT_EP
#ifdef RAW_ENABLE
#    define N32_USBHS_INTERFACE_COUNT 2U
#    define N32_USBHS_KEYBOARD_CFGDESC_SIZE 73U
#else
#    define N32_USBHS_INTERFACE_COUNT 1U
#    define N32_USBHS_KEYBOARD_CFGDESC_SIZE 41U
#endif
#define N32_USBHS_KEYBOARD_DESC_SIZE 9U
#define N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE 0x21U
#define N32_USBHS_KEYBOARD_REPORT_DESC 0x22U
#define N32_USBHS_KEYBOARD_REQ_SET_PROTOCOL 0x0BU
#define N32_USBHS_KEYBOARD_REQ_GET_PROTOCOL 0x03U
#define N32_USBHS_KEYBOARD_REQ_SET_IDLE 0x0AU
#define N32_USBHS_KEYBOARD_REQ_GET_IDLE 0x02U
#define N32_USBHS_KEYBOARD_REQ_SET_REPORT 0x09U
#define N32_USBHS_POLLING_INTERVAL 1U

extern USBD_Class_cb_TypeDef n32_usbhs_keyboard_cb;

typedef struct {
    uint32_t reset_count;
    uint32_t configured_count;
    uint32_t suspended_count;
    uint32_t resumed_count;
    uint32_t connected_count;
    uint32_t disconnected_count;
    uint32_t class_init_count;
    uint32_t class_deinit_count;
    uint32_t setup_count;
    uint32_t get_config_count;
    uint32_t ep0_rx_count;
    uint32_t datain_count;
    uint32_t dataout_count;
    uint32_t raw_ep0_out_count;
    uint32_t raw_out_count;
    uint32_t raw_task_count;
    uint32_t raw_drop_count;
    uint16_t last_raw_ep0_length;
    uint16_t last_raw_out_length;
    uint8_t  last_speed;
    uint8_t  last_cfgidx;
    uint8_t  last_ep;
    uint8_t  last_request_type;
    uint8_t  last_request;
    uint16_t last_value;
    uint16_t last_index;
    uint16_t last_length;
} n32_usbhs_debug_counters_t;

void n32_usbhs_debug_snapshot(n32_usbhs_debug_counters_t *snapshot);
void n32_usbhs_shared_tx_done(void);
#ifdef RAW_ENABLE
void n32_usbhs_raw_tx_done(void);
void n32_usbhs_raw_tx_reset(void);
void n32_usbhs_raw_hid_task(void);
#endif
void n32_usbhs_user_init(void);
void n32_usbhs_user_device_reset(uint8_t speed);
void n32_usbhs_user_device_configured(void);
void n32_usbhs_user_device_suspended(void);
void n32_usbhs_user_device_resumed(void);
void n32_usbhs_user_device_connected(void);
void n32_usbhs_user_device_disconnected(void);
