#pragma once

#include "usbd_req.h"

#define USB_DEVICE_DESCRIPTOR_TYPE 0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE 0x02
#define USB_STRING_DESCRIPTOR_TYPE 0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE 0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE 0x05

#define USB_SIZ_DEVICE_DESC 18
#define USB_SIZ_STRING_LANGID 4
#define USB_SIZ_STRING_SERIAL 0x1A

extern uint8_t          USBD_DeviceDesc[USB_SIZ_DEVICE_DESC];
extern uint8_t          USBD_StrDesc[USB_MAX_STR_DESC_SIZ];
extern uint8_t          USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC];
extern uint8_t          USBD_LangIDDesc[USB_SIZ_STRING_LANGID];
extern USBD_DEVICE_DESC USBD_desc;

uint8_t *USBD_USER_DeviceDescriptor(uint8_t speed, uint16_t *length);
uint8_t *USBD_USER_LangIDStrDescriptor(uint8_t speed, uint16_t *length);
uint8_t *USBD_USER_ManufacturerStrDescriptor(uint8_t speed, uint16_t *length);
uint8_t *USBD_USER_ProductStrDescriptor(uint8_t speed, uint16_t *length);
uint8_t *USBD_USER_SerialStrDescriptor(uint8_t speed, uint16_t *length);
uint8_t *USBD_USER_ConfigStrDescriptor(uint8_t speed, uint16_t *length);
uint8_t *USBD_USER_InterfaceStrDescriptor(uint8_t speed, uint16_t *length);
