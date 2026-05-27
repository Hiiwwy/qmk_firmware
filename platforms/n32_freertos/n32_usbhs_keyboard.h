#pragma once

#include "usbd_ioreq.h"

#define N32_USBHS_HID_IN_EP HID_IN_EP
#define N32_USBHS_HID_OUT_EP HID_OUT_EP
#define N32_USBHS_KEYBOARD_CFGDESC_SIZE 41U
#define N32_USBHS_KEYBOARD_DESC_SIZE 9U
#define N32_USBHS_KEYBOARD_REPORT_DESC_SIZE 63U
#define N32_USBHS_KEYBOARD_DESCRIPTOR_TYPE 0x21U
#define N32_USBHS_KEYBOARD_REPORT_DESC 0x22U
#define N32_USBHS_KEYBOARD_REQ_SET_PROTOCOL 0x0BU
#define N32_USBHS_KEYBOARD_REQ_GET_PROTOCOL 0x03U
#define N32_USBHS_KEYBOARD_REQ_SET_IDLE 0x0AU
#define N32_USBHS_KEYBOARD_REQ_GET_IDLE 0x02U
#define N32_USBHS_POLLING_INTERVAL 1U

extern USBD_Class_cb_TypeDef n32_usbhs_keyboard_cb;

void n32_usbhs_keyboard_tx_done(void);
void n32_usbhs_user_init(void);
void n32_usbhs_user_device_reset(uint8_t speed);
void n32_usbhs_user_device_configured(void);
void n32_usbhs_user_device_suspended(void);
void n32_usbhs_user_device_resumed(void);
void n32_usbhs_user_device_connected(void);
void n32_usbhs_user_device_disconnected(void);
