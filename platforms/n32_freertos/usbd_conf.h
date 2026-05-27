#pragma once

#include "usbhs_conf.h"

#define USBD_CFG_MAX_NUM 1
#define USBD_ITF_MAX_NUM 2
#define USB_MAX_STR_DESC_SIZ 64

#define USBD_SELF_POWERED
#define USBD_DYNAMIC_DESCRIPTOR_CHANGE_ENABLED

#define HID_IN_EP 0x81
#define HID_OUT_EP 0x09
#define RAW_IN_EP 0x82
#define RAW_OUT_EP 0x0A

#define HID_IN_PACKET 32U
#define HID_OUT_PACKET 32U
#define RAW_IN_PACKET 32U
#define RAW_OUT_PACKET 32U
