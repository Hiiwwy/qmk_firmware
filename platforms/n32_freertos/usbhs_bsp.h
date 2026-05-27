#pragma once

#include "usbhs_core.h"

void USB_BSP_Init(void);
void USB_BSP_EnableInterrupt(USB_CORE_MODULE *USBx);
void USB_BSP_uDelay(const uint32_t usec);
void USB_BSP_mDelay(const uint32_t msec);
