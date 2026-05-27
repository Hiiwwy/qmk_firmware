#include "host.h"
#include "host_driver.h"
#include "usb_driver.h"

static host_driver_t n32_freertos_driver = {
    .keyboard_leds = n32_usb_keyboard_leds,
    .send_keyboard = n32_usb_send_keyboard,
    .send_nkro = n32_usb_send_nkro,
    .send_mouse = n32_usb_send_mouse,
    .send_extra = n32_usb_send_extra,
};

void protocol_setup(void) {}
void protocol_pre_init(void) {}

void protocol_post_init(void) {
    host_set_driver(&n32_freertos_driver);
}

void protocol_pre_task(void) {
    n32_usb_task();
}

void protocol_post_task(void) {}
