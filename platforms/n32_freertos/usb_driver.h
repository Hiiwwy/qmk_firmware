#pragma once

#include <stdint.h>
#include "report.h"

void n32_usb_init(void);
void n32_usb_task(void);
uint8_t n32_usb_keyboard_leds(void);
void n32_usb_set_report_rate_hz(uint16_t hz);
uint16_t n32_usb_get_report_rate_hz(void);
void n32_usb_send_keyboard(report_keyboard_t *report);
void n32_usb_send_nkro(report_nkro_t *report);
void n32_usb_send_mouse(report_mouse_t *report);
void n32_usb_send_extra(report_extra_t *report);
#ifdef RAW_ENABLE
void n32_usb_send_raw_hid(uint8_t *data, uint8_t length);
#endif
void n32_usb_set_keyboard_leds(uint8_t leds);
