// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "usb_driver.h"

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix_drivers.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef VIA_ENABLE
#    include "via.h"
#endif
#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_RGB)
#    include "SEGGER_RTT.h"
#endif

#ifndef N32_USB_REPORT_RATE_DEFAULT
#    ifdef N32_USB_SPEED_FS
#        define N32_USB_REPORT_RATE_DEFAULT 1000
#    else
#        define N32_USB_REPORT_RATE_DEFAULT 8000
#    endif
#endif

#define N32_CONFIG_MAGIC 0xA7
#if N32_USB_REPORT_RATE_DEFAULT == 1000
#    define N32_CONFIG_DEFAULT_REPORT_RATE_INDEX 0
#elif N32_USB_REPORT_RATE_DEFAULT == 2000
#    define N32_CONFIG_DEFAULT_REPORT_RATE_INDEX 1
#elif N32_USB_REPORT_RATE_DEFAULT == 4000
#    define N32_CONFIG_DEFAULT_REPORT_RATE_INDEX 2
#else
#    define N32_CONFIG_DEFAULT_REPORT_RATE_INDEX 3
#endif
#ifdef N32_USB_SPEED_FS
#    define N32_CONFIG_MAX_REPORT_RATE_INDEX 0
#else
#    define N32_CONFIG_MAX_REPORT_RATE_INDEX 3
#endif
#define N32_CONFIG_DEFAULT_DEBOUNCE_MS 5

enum n32_via_value_id {
    id_n32_report_rate = 1,
    id_n32_debounce    = 2,
    id_n32_rgb_enable  = 3,
    id_n32_rgb_mode    = 4,
    id_n32_rgb_hue     = 5,
    id_n32_rgb_sat     = 6,
    id_n32_rgb_val     = 7,
    id_n32_rgb_speed   = 8,
    id_n32_rgb_zone    = 9,
};

typedef struct {
    uint8_t magic;
    uint8_t report_rate_index;
    uint8_t debounce_ms;
    uint8_t reserved;
} n32_runtime_config_t;

static n32_runtime_config_t n32_config = {
    .magic             = N32_CONFIG_MAGIC,
    .report_rate_index = N32_CONFIG_DEFAULT_REPORT_RATE_INDEX,
    .debounce_ms       = N32_CONFIG_DEFAULT_DEBOUNCE_MS,
    .reserved          = 0,
};

static uint16_t n32_last_keycode;
static uint32_t n32_keypress_count;
#ifdef RGB_MATRIX_ENABLE
static bool     n32_rgb_save_pending;
static uint32_t n32_rgb_save_timer;
#endif

#if defined(CONSOLE_ENABLE) && defined(N32_TRACE_RGB)
static void n32_rgb_tracef(const char *format, ...) {
    char    buffer[160];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length > 0) {
        SEGGER_RTT_Write(0, buffer, length < (int)sizeof(buffer) ? (unsigned)length : sizeof(buffer) - 1U);
    }
}
#else
static void n32_rgb_tracef(const char *format, ...) {
    (void)format;
}
#endif

static uint16_t n32_report_rate_hz(void) {
    static const uint16_t rates[] = {1000, 2000, 4000, 8000};

    if (n32_config.report_rate_index >= sizeof(rates) / sizeof(rates[0])) {
        return N32_USB_REPORT_RATE_DEFAULT;
    }

    return rates[n32_config.report_rate_index];
}

#ifdef RGB_MATRIX_ENABLE
static uint8_t n32_via_rgb_mode_to_qmk(uint8_t mode) {
    switch (mode) {
        case 2:
            return RGB_MATRIX_BREATHING;
        case 3:
            return RGB_MATRIX_CYCLE_ALL;
        case 4:
            return RGB_MATRIX_CYCLE_LEFT_RIGHT;
        case 5:
            return RGB_MATRIX_HUE_BREATHING;
        case 6:
            return RGB_MATRIX_RAINBOW_MOVING_CHEVRON;
        case 7:
            return RGB_MATRIX_SOLID_REACTIVE_SIMPLE;
        case 1:
        default:
            return RGB_MATRIX_SOLID_COLOR;
    }
}

static uint8_t n32_qmk_rgb_mode_to_via(uint8_t mode) {
    switch (mode) {
        case RGB_MATRIX_BREATHING:
            return 2;
        case RGB_MATRIX_CYCLE_ALL:
            return 3;
        case RGB_MATRIX_CYCLE_LEFT_RIGHT:
            return 4;
        case RGB_MATRIX_HUE_BREATHING:
            return 5;
        case RGB_MATRIX_RAINBOW_MOVING_CHEVRON:
            return 6;
        case RGB_MATRIX_SOLID_REACTIVE_SIMPLE:
            return 7;
        case RGB_MATRIX_SOLID_COLOR:
        default:
            return 1;
    }
}

static void n32_rgb_mark_save_pending(void) {
    n32_rgb_save_pending = true;
    n32_rgb_save_timer   = timer_read32();
}

static void n32_rgb_turn_off_now(void) {
    rgb_matrix_disable_noeeprom();
    rgb_matrix_set_color_all(0, 0, 0);
    rgb_matrix_driver.flush();
}
#endif

static void n32_runtime_config_sanitize(void) {
    if (n32_config.magic != N32_CONFIG_MAGIC) {
        n32_config.magic             = N32_CONFIG_MAGIC;
        n32_config.report_rate_index = N32_CONFIG_DEFAULT_REPORT_RATE_INDEX;
        n32_config.debounce_ms       = N32_CONFIG_DEFAULT_DEBOUNCE_MS;
        n32_config.reserved          = 0;
    }

    if (n32_config.report_rate_index > N32_CONFIG_MAX_REPORT_RATE_INDEX) {
        n32_config.report_rate_index = N32_CONFIG_DEFAULT_REPORT_RATE_INDEX;
    }

    if (n32_config.debounce_ms > 20) {
        n32_config.debounce_ms = N32_CONFIG_DEFAULT_DEBOUNCE_MS;
    }
}

static void n32_runtime_config_apply(void) {
    n32_usb_set_report_rate_hz(n32_report_rate_hz());
}

static void n32_runtime_config_load(void) {
#if defined(VIA_ENABLE) && VIA_EEPROM_CUSTOM_CONFIG_SIZE >= 4
    via_read_custom_config(&n32_config, 0, sizeof(n32_config));
#endif
    n32_runtime_config_sanitize();
    n32_runtime_config_apply();
}

static void n32_runtime_config_save(void) {
    n32_runtime_config_sanitize();
#if defined(VIA_ENABLE) && VIA_EEPROM_CUSTOM_CONFIG_SIZE >= 4
    via_update_custom_config(&n32_config, 0, sizeof(n32_config));
#endif
}

uint8_t n32_get_debounce_ms(void) {
    return n32_config.debounce_ms;
}

#ifdef QUANTUM_PAINTER_ENABLE
#    include "n32_flash_ops.h"
#    include "qp.h"
#    include "qp_st7789.h"

#    ifdef VIA_ENABLE
#        include "raw_hid.h"
#    endif

#    define N32_TFT_PAGE_COUNT 4
#    define N32_VIA_IMAGE_CMD 0x7D
#    define N32_VIA_IMAGE_INFO 0x00
#    define N32_VIA_IMAGE_BEGIN 0x01
#    define N32_VIA_IMAGE_DATA 0x02
#    define N32_VIA_IMAGE_END 0x03
#    define N32_VIA_IMAGE_ABORT 0x04
#    define N32_VIA_IMAGE_SHOW 0x05
#    define N32_VIA_IMAGE_CLEAR 0x06
#    define N32_VIA_IMAGE_STATUS_OK 0x00
#    define N32_VIA_IMAGE_STATUS_BUSY 0x01
#    define N32_VIA_IMAGE_STATUS_RANGE 0x02
#    define N32_VIA_IMAGE_STATUS_ORDER 0x03
#    define N32_VIA_IMAGE_STATUS_FLASH 0x04
#    define N32_VIA_IMAGE_STATUS_INVALID 0x05
#    define N32_VIA_IMAGE_PAYLOAD_SIZE 25

static painter_device_t       n32_tft_display;
static bool                   n32_tft_ready;
static uint8_t                n32_tft_page;
static uint32_t               n32_tft_refresh_timer;
static uint32_t               n32_tft_page_timer;
static painter_image_handle_t n32_tft_image;
static deferred_token         n32_tft_anim_token = INVALID_DEFERRED_TOKEN;
static bool                   n32_tft_image_loaded;
static bool                   n32_tft_upload_active;
static uint32_t               n32_tft_upload_size;
static uint32_t               n32_tft_upload_offset;
static uint32_t               n32_tft_upload_page_base;
static bool                   n32_tft_upload_page_dirty;
static uint8_t                n32_tft_upload_page[N32_TFT_IMAGE_FLASH_PAGE_SIZE];

static const uint8_t font_space[7]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t font_unknown[7] = {0x1E, 0x21, 0x01, 0x06, 0x04, 0x00, 0x04};

static const uint8_t *n32_tft_glyph(char ch) {
    static const uint8_t glyph_0[7]     = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const uint8_t glyph_1[7]     = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t glyph_2[7]     = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t glyph_3[7]     = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E};
    static const uint8_t glyph_4[7]     = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const uint8_t glyph_5[7]     = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
    static const uint8_t glyph_6[7]     = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_7[7]     = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t glyph_8[7]     = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_9[7]     = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
    static const uint8_t glyph_a[7]     = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t glyph_b[7]     = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const uint8_t glyph_c[7]     = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t glyph_d[7]     = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t glyph_e[7]     = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t glyph_f[7]     = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t glyph_g[7]     = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_h[7]     = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t glyph_i[7]     = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t glyph_j[7]     = {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_k[7]     = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const uint8_t glyph_l[7]     = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t glyph_m[7]     = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const uint8_t glyph_n[7]     = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t glyph_o[7]     = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_p[7]     = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t glyph_q[7]     = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    static const uint8_t glyph_r[7]     = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t glyph_s[7]     = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t glyph_t[7]     = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t glyph_u[7]     = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_v[7]     = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const uint8_t glyph_w[7]     = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    static const uint8_t glyph_x[7]     = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    static const uint8_t glyph_y[7]     = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t glyph_z[7]     = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    static const uint8_t glyph_colon[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    static const uint8_t glyph_dash[7]  = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    static const uint8_t glyph_slash[7] = {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};

    if (ch >= 'a' && ch <= 'z') {
        ch -= 'a' - 'A';
    }

    switch (ch) {
        case '0':
            return glyph_0;
        case '1':
            return glyph_1;
        case '2':
            return glyph_2;
        case '3':
            return glyph_3;
        case '4':
            return glyph_4;
        case '5':
            return glyph_5;
        case '6':
            return glyph_6;
        case '7':
            return glyph_7;
        case '8':
            return glyph_8;
        case '9':
            return glyph_9;
        case 'A':
            return glyph_a;
        case 'B':
            return glyph_b;
        case 'C':
            return glyph_c;
        case 'D':
            return glyph_d;
        case 'E':
            return glyph_e;
        case 'F':
            return glyph_f;
        case 'G':
            return glyph_g;
        case 'H':
            return glyph_h;
        case 'I':
            return glyph_i;
        case 'J':
            return glyph_j;
        case 'K':
            return glyph_k;
        case 'L':
            return glyph_l;
        case 'M':
            return glyph_m;
        case 'N':
            return glyph_n;
        case 'O':
            return glyph_o;
        case 'P':
            return glyph_p;
        case 'Q':
            return glyph_q;
        case 'R':
            return glyph_r;
        case 'S':
            return glyph_s;
        case 'T':
            return glyph_t;
        case 'U':
            return glyph_u;
        case 'V':
            return glyph_v;
        case 'W':
            return glyph_w;
        case 'X':
            return glyph_x;
        case 'Y':
            return glyph_y;
        case 'Z':
            return glyph_z;
        case ':':
            return glyph_colon;
        case '-':
            return glyph_dash;
        case '/':
            return glyph_slash;
        case ' ':
            return font_space;
        default:
            return font_unknown;
    }
}

static void n32_tft_char(uint16_t x, uint16_t y, char ch, uint8_t scale, uint8_t hue, uint8_t sat, uint8_t val) {
    const uint8_t *glyph = n32_tft_glyph(ch);

    for (uint8_t row = 0; row < 7; row++) {
        for (uint8_t col = 0; col < 5; col++) {
            if (glyph[row] & (1 << (4 - col))) {
                uint16_t x0 = x + (col * scale);
                uint16_t y0 = y + (row * scale);
                qp_rect(n32_tft_display, x0, y0, x0 + scale - 1, y0 + scale - 1, hue, sat, val, true);
            }
        }
    }
}

static void n32_tft_text(uint16_t x, uint16_t y, const char *text, uint8_t scale, uint8_t hue, uint8_t sat, uint8_t val) {
    while (*text) {
        n32_tft_char(x, y, *text, scale, hue, sat, val);
        x += 6 * scale;
        text++;
    }
}

static void n32_tft_label(uint16_t y, const char *label, const char *value) {
    n32_tft_text(8, y, label, 1, 135, 120, 220);
    n32_tft_text(74, y, value, 1, 0, 0, 255);
}

static void n32_tft_badge(uint16_t x, uint16_t y, const char *text, bool active, uint8_t hue) {
    qp_rect(n32_tft_display, x, y, x + 42, y + 18, hue, active ? 220 : 0, active ? 180 : 42, true);
    n32_tft_text(x + 4, y + 6, text, 1, 0, 0, active ? 255 : 140);
}

static void n32_tft_bar(uint16_t x, uint16_t y, uint8_t value, uint8_t hue, uint8_t sat) {
    uint16_t width = ((uint16_t)value * 80) / 255;

    qp_rect(n32_tft_display, x, y, x + 80, y + 8, 0, 0, 34, true);
    if (width > 0) {
        qp_rect(n32_tft_display, x, y, x + width - 1, y + 8, hue, sat, 220, true);
    }
}

static uint32_t n32_u32_le(const uint8_t *data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void n32_put_u32_le(uint8_t *data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

static uint16_t n32_tft_image_x(void) {
    if (!n32_tft_image || n32_tft_image->width >= N32_TFT_WIDTH) {
        return 0;
    }
    return (N32_TFT_WIDTH - n32_tft_image->width) / 2;
}

static uint16_t n32_tft_image_y(void) {
    if (!n32_tft_image || n32_tft_image->height >= N32_TFT_HEIGHT) {
        return 0;
    }
    return (N32_TFT_HEIGHT - n32_tft_image->height) / 2;
}

static void n32_tft_stop_image(void) {
    if (n32_tft_anim_token != INVALID_DEFERRED_TOKEN) {
        qp_stop_animation(n32_tft_anim_token);
        n32_tft_anim_token = INVALID_DEFERRED_TOKEN;
    }

    if (n32_tft_image) {
        qp_close_image(n32_tft_image);
        n32_tft_image = NULL;
    }

    n32_tft_image_loaded = false;
}

static bool n32_tft_load_image(void) {
    if (n32_tft_image_loaded) {
        return n32_tft_image != NULL;
    }

    if (*(const uint32_t *)N32_TFT_IMAGE_FLASH_BASE == 0xFFFFFFFFU) {
        n32_tft_image_loaded = true;
        return false;
    }

    n32_tft_image        = qp_load_image_mem((const void *)N32_TFT_IMAGE_FLASH_BASE);
    n32_tft_image_loaded = true;
    return n32_tft_image != NULL;
}

static bool n32_tft_flush_upload_page(void) {
    if (!n32_tft_upload_page_dirty) {
        return true;
    }

    bool flash_open = n32_flash_begin();
    bool status     = flash_open;

    if (status) {
        status = n32_flash_erase_page(n32_tft_upload_page_base);
    }
    if (status) {
        for (uint32_t i = 0; i < N32_TFT_IMAGE_FLASH_PAGE_SIZE; i += 8) {
            uint32_t data0 = 0xFFFFFFFFU;
            uint32_t data1 = 0xFFFFFFFFU;

            memcpy(&data0, &n32_tft_upload_page[i], sizeof(data0));
            memcpy(&data1, &n32_tft_upload_page[i + sizeof(data0)], sizeof(data1));

            status = n32_flash_program_doubleword(n32_tft_upload_page_base + i, data0, data1);
            if (!status) {
                break;
            }
        }
    }
    if (flash_open) {
        n32_flash_end();
    }

    n32_tft_upload_page_dirty = false;
    return status;
}

static bool n32_tft_prepare_upload_page(uint32_t address) {
    uint32_t page_base = address & ~(N32_TFT_IMAGE_FLASH_PAGE_SIZE - 1U);

    if (n32_tft_upload_page_base == page_base) {
        return true;
    }

    if (!n32_tft_flush_upload_page()) {
        return false;
    }

    n32_tft_upload_page_base  = page_base;
    n32_tft_upload_page_dirty = false;
    memset(n32_tft_upload_page, 0xFF, sizeof(n32_tft_upload_page));
    return true;
}

static uint8_t n32_tft_upload_begin(uint32_t size) {
    if (n32_tft_upload_active) {
        return N32_VIA_IMAGE_STATUS_BUSY;
    }
    if (size == 0 || size > N32_TFT_IMAGE_FLASH_SIZE) {
        return N32_VIA_IMAGE_STATUS_RANGE;
    }

    n32_tft_stop_image();
    n32_tft_upload_active     = true;
    n32_tft_upload_size       = size;
    n32_tft_upload_offset     = 0;
    n32_tft_upload_page_base  = 0;
    n32_tft_upload_page_dirty = false;
    memset(n32_tft_upload_page, 0xFF, sizeof(n32_tft_upload_page));

    return N32_VIA_IMAGE_STATUS_OK;
}

static uint8_t n32_tft_upload_data(uint32_t offset, uint8_t length, const uint8_t *payload) {
    if (!n32_tft_upload_active) {
        return N32_VIA_IMAGE_STATUS_ORDER;
    }
    if (length > N32_VIA_IMAGE_PAYLOAD_SIZE || offset != n32_tft_upload_offset || offset + length > n32_tft_upload_size) {
        return N32_VIA_IMAGE_STATUS_RANGE;
    }

    for (uint8_t i = 0; i < length; i++) {
        uint32_t address = N32_TFT_IMAGE_FLASH_BASE + n32_tft_upload_offset;
        if (!n32_tft_prepare_upload_page(address)) {
            n32_tft_upload_active = false;
            return N32_VIA_IMAGE_STATUS_FLASH;
        }

        n32_tft_upload_page[address - n32_tft_upload_page_base] = payload[i];
        n32_tft_upload_page_dirty                               = true;
        n32_tft_upload_offset++;
    }

    return N32_VIA_IMAGE_STATUS_OK;
}

static uint8_t n32_tft_upload_end(void) {
    if (!n32_tft_upload_active || n32_tft_upload_offset != n32_tft_upload_size) {
        n32_tft_upload_active = false;
        return N32_VIA_IMAGE_STATUS_ORDER;
    }

    n32_tft_upload_active = false;
    if (!n32_tft_flush_upload_page()) {
        return N32_VIA_IMAGE_STATUS_FLASH;
    }

    n32_tft_image_loaded = false;
    if (!n32_tft_load_image()) {
        return N32_VIA_IMAGE_STATUS_INVALID;
    }

    n32_tft_page       = 3;
    n32_tft_page_timer = timer_read32();
    return N32_VIA_IMAGE_STATUS_OK;
}

static const char *n32_tft_rgb_zone(void) {
#    ifdef RGB_MATRIX_ENABLE
    led_flags_t flags = rgb_matrix_get_flags();

    if (flags == LED_FLAG_ALL) {
        return "ALL";
    }
    if (flags == (LED_FLAG_KEYLIGHT | LED_FLAG_INDICATOR)) {
        return "KEY";
    }
    if (flags == LED_FLAG_UNDERGLOW) {
        return "UNDER";
    }
    if (flags == LED_FLAG_NONE) {
        return "OFF";
    }
#    endif
    return "CUSTOM";
}

static void n32_tft_header(const char *title, uint8_t hue) {
    qp_rect(n32_tft_display, 0, 0, N32_TFT_WIDTH - 1, N32_TFT_HEIGHT - 1, 0, 0, 0, true);
    qp_rect(n32_tft_display, 0, 0, N32_TFT_WIDTH - 1, 31, hue, 170, 120, true);
    n32_tft_text(8, 9, title, 2, 0, 0, 255);
}

static void n32_tft_page_status(void) {
    char  text[24];
    led_t led_state = host_keyboard_led_state();

    n32_tft_header("STATUS", 140);

    snprintf(text, sizeof(text), "%u", get_highest_layer(layer_state));
    n32_tft_label(48, "LAYER", text);

    n32_tft_text(8, 72, "LOCKS", 1, 135, 120, 220);
    n32_tft_badge(8, 88, "NUM", led_state.num_lock, 150);
    n32_tft_badge(64, 88, "CAP", led_state.caps_lock, 0);
    n32_tft_badge(120, 88, "SCR", led_state.scroll_lock, 28);

    n32_tft_label(128, "NKRO", keymap_config.nkro ? "ON" : "OFF");
    n32_tft_label(150, "VIA", "RAW HID");
    snprintf(text, sizeof(text), "%uHZ", n32_report_rate_hz());
    n32_tft_label(172, "RATE", text);

    snprintf(text, sizeof(text), "%lu", (unsigned long)n32_keypress_count);
    n32_tft_label(214, "COUNT", text);
    snprintf(text, sizeof(text), "0X%04X", n32_last_keycode);
    n32_tft_label(236, "KEY", text);
}

static void n32_tft_page_rgb(void) {
    char text[24];

    n32_tft_header("RGB", 0);
#    ifdef RGB_MATRIX_ENABLE
    snprintf(text, sizeof(text), "%u", rgb_matrix_get_mode());
    n32_tft_label(48, "MODE", text);
    n32_tft_label(70, "ZONE", n32_tft_rgb_zone());

    snprintf(text, sizeof(text), "%u", rgb_matrix_get_hue());
    n32_tft_label(106, "HUE", text);
    n32_tft_bar(74, 121, rgb_matrix_get_hue(), rgb_matrix_get_hue(), 255);

    snprintf(text, sizeof(text), "%u", rgb_matrix_get_sat());
    n32_tft_label(140, "SAT", text);
    n32_tft_bar(74, 155, rgb_matrix_get_sat(), 150, 220);

    snprintf(text, sizeof(text), "%u", rgb_matrix_get_val());
    n32_tft_label(174, "VAL", text);
    n32_tft_bar(74, 189, rgb_matrix_get_val(), 28, 220);

    snprintf(text, sizeof(text), "%u", rgb_matrix_get_speed());
    n32_tft_label(214, "SPD", text);
#    else
    n32_tft_text(8, 54, "RGB OFF", 1, 0, 0, 255);
#    endif
}

static void n32_tft_page_debug(void) {
    char text[24];

    n32_tft_header("DEBUG", 95);

#    ifdef DEBUG_MATRIX_SCAN_RATE
    snprintf(text, sizeof(text), "%lu", (unsigned long)get_matrix_scan_rate());
#    else
    snprintf(text, sizeof(text), "OFF");
#    endif
    n32_tft_label(48, "SCAN", text);

#    ifdef WPM_ENABLE
    snprintf(text, sizeof(text), "%u", get_current_wpm());
#    else
    snprintf(text, sizeof(text), "OFF");
#    endif
    n32_tft_label(70, "WPM", text);

    snprintf(text, sizeof(text), "%lu", (unsigned long)n32_keypress_count);
    n32_tft_label(106, "COUNT", text);
    snprintf(text, sizeof(text), "0X%04X", n32_last_keycode);
    n32_tft_label(128, "LAST", text);

    n32_tft_label(164, "TFT", "172X320");
    n32_tft_label(186, "PANEL", "ST7789");
    snprintf(text, sizeof(text), "%uHZ", n32_report_rate_hz());
    n32_tft_label(208, "RATE", text);
    snprintf(text, sizeof(text), "%uMS", n32_config.debounce_ms);
    n32_tft_label(230, "DBNC", text);
}

static void n32_tft_page_image(void) {
    if (!n32_tft_load_image()) {
        n32_tft_header("IMAGE", 40);
        n32_tft_label(54, "STATE", n32_tft_upload_active ? "UPLOAD" : "EMPTY");
        n32_tft_label(76, "FORMAT", "QGF");
        n32_tft_label(98, "VIA CMD", "0X7D");
        return;
    }

    qp_rect(n32_tft_display, 0, 0, N32_TFT_WIDTH - 1, N32_TFT_HEIGHT - 1, 0, 0, 0, true);

    if (n32_tft_image->frame_count > 1) {
        if (n32_tft_anim_token == INVALID_DEFERRED_TOKEN) {
            n32_tft_anim_token = qp_animate(n32_tft_display, n32_tft_image_x(), n32_tft_image_y(), n32_tft_image);
        }
    } else {
        qp_drawimage(n32_tft_display, n32_tft_image_x(), n32_tft_image_y(), n32_tft_image);
    }
}

static void n32_tft_render(void) {
    if (!n32_tft_ready) {
        return;
    }

    if (n32_tft_page != 3 && n32_tft_anim_token != INVALID_DEFERRED_TOKEN) {
        qp_stop_animation(n32_tft_anim_token);
        n32_tft_anim_token = INVALID_DEFERRED_TOKEN;
    }

    switch (n32_tft_page) {
        case 0:
            n32_tft_page_status();
            break;
        case 1:
            n32_tft_page_rgb();
            break;
        case 2:
            n32_tft_page_debug();
            break;
        default:
            n32_tft_page_image();
            break;
    }

    qp_flush(n32_tft_display);
}

static void n32_tft_next_page(void) {
    n32_tft_page = (n32_tft_page + 1) % N32_TFT_PAGE_COUNT;
    n32_tft_render();
}
#endif

#ifdef RGB_MATRIX_ENABLE
led_config_t g_led_config = {
    {
        {0, 1, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
    },
    {
        {0, 0},
        {224, 0},
    },
    {
        LED_FLAG_KEYLIGHT,
        LED_FLAG_INDICATOR,
    },
};
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (record->event.pressed) {
        n32_last_keycode = keycode;
        n32_keypress_count++;
    }

#ifdef QUANTUM_PAINTER_ENABLE
    if (keycode == QK_KB_0 && record->event.pressed) {
        n32_tft_next_page();
        return false;
    }
#endif

#ifdef RGB_MATRIX_ENABLE
    if (keycode == QK_RGB_MATRIX_TOGGLE && record->event.pressed) {
        switch (rgb_matrix_get_flags()) {
            case LED_FLAG_ALL:
                rgb_matrix_set_flags(LED_FLAG_KEYLIGHT | LED_FLAG_INDICATOR);
                break;
            case (LED_FLAG_KEYLIGHT | LED_FLAG_INDICATOR):
                rgb_matrix_set_flags(LED_FLAG_UNDERGLOW);
                break;
            case LED_FLAG_UNDERGLOW:
                rgb_matrix_set_flags(LED_FLAG_NONE);
                rgb_matrix_set_color_all(0, 0, 0);
                break;
            default:
                rgb_matrix_set_flags(LED_FLAG_ALL);
                rgb_matrix_enable_noeeprom();
                break;
        }
        return false;
    }
#endif

    return true;
}

void keyboard_post_init_kb(void) {
    n32_runtime_config_load();
    keyboard_post_init_user();

#ifdef RGB_MATRIX_ENABLE
    n32_rgb_tracef("RGB config: enabled=%u mode=%u hsv=(%u,%u,%u) speed=%u flags=0x%02x count=%u caps_only=1\n", rgb_matrix_is_enabled(), rgb_matrix_get_mode(), rgb_matrix_get_hue(), rgb_matrix_get_sat(), rgb_matrix_get_val(), rgb_matrix_get_speed(), rgb_matrix_get_flags(), (unsigned)RGB_MATRIX_LED_COUNT);
#endif

#ifdef QUANTUM_PAINTER_ENABLE
    gpio_set_pin_output(N32_TFT_BL_PIN);
    gpio_write_pin_high(N32_TFT_BL_PIN);

    n32_tft_display = qp_st7789_make_spi_device(N32_TFT_WIDTH, N32_TFT_HEIGHT, N32_TFT_CS_PIN, N32_TFT_DC_PIN, N32_TFT_RST_PIN, N32_TFT_SPI_DIVISOR, N32_TFT_SPI_MODE);
    if (n32_tft_display) {
        qp_set_viewport_offsets(n32_tft_display, N32_TFT_OFFSET_X, N32_TFT_OFFSET_Y);
        n32_tft_ready         = qp_init(n32_tft_display, QP_ROTATION_180) && qp_power(n32_tft_display, true);
        n32_tft_refresh_timer = timer_read32();
        n32_tft_page_timer    = timer_read32();
        n32_tft_render();
    }
#endif
}

void housekeeping_task_kb(void) {
#ifdef QUANTUM_PAINTER_ENABLE
    if (n32_tft_page != 3 && timer_elapsed32(n32_tft_refresh_timer) > 500) {
        n32_tft_refresh_timer = timer_read32();
        n32_tft_render();
    }

    if (n32_tft_page != 3 && timer_elapsed32(n32_tft_page_timer) > 5000) {
        uint8_t page_count = n32_tft_load_image() ? N32_TFT_PAGE_COUNT : 3;

        n32_tft_page_timer = timer_read32();
        n32_tft_page       = (n32_tft_page + 1) % page_count;
        n32_tft_render();
    }
#endif

#ifdef RGB_MATRIX_ENABLE
    if (n32_rgb_save_pending && timer_elapsed32(n32_rgb_save_timer) > 1500) {
        n32_rgb_save_pending = false;
        eeconfig_force_flush_rgb_matrix();
        n32_rgb_tracef("RGB config saved\n");
    }
#endif
}

#ifdef VIA_ENABLE
static void n32_via_set_value(uint8_t *data) {
    uint8_t *value_id   = &data[0];
    uint8_t *value_data = &data[1];

    switch (*value_id) {
        case id_n32_report_rate:
            n32_config.report_rate_index = value_data[0];
            n32_runtime_config_sanitize();
            n32_runtime_config_apply();
            break;
        case id_n32_debounce:
            n32_config.debounce_ms = value_data[0];
            n32_runtime_config_sanitize();
            break;
#    ifdef RGB_MATRIX_ENABLE
        case id_n32_rgb_enable:
            if (value_data[0]) {
                rgb_matrix_enable_noeeprom();
            } else {
                n32_rgb_turn_off_now();
            }
            n32_rgb_mark_save_pending();
            break;
        case id_n32_rgb_mode: {
            if (value_data[0] == 0) {
                n32_rgb_turn_off_now();
            } else {
                rgb_matrix_enable_noeeprom();
                rgb_matrix_mode_noeeprom(n32_via_rgb_mode_to_qmk(value_data[0]));
            }
            n32_rgb_mark_save_pending();
            break;
        }
        case id_n32_rgb_hue:
            rgb_matrix_sethsv_noeeprom(value_data[0], rgb_matrix_get_sat(), rgb_matrix_get_val());
            n32_rgb_mark_save_pending();
            break;
        case id_n32_rgb_sat:
            rgb_matrix_sethsv_noeeprom(rgb_matrix_get_hue(), value_data[0], rgb_matrix_get_val());
            n32_rgb_mark_save_pending();
            break;
        case id_n32_rgb_val:
            rgb_matrix_sethsv_noeeprom(rgb_matrix_get_hue(), rgb_matrix_get_sat(), value_data[0] > RGB_MATRIX_MAXIMUM_BRIGHTNESS ? RGB_MATRIX_MAXIMUM_BRIGHTNESS : value_data[0]);
            n32_rgb_mark_save_pending();
            break;
        case id_n32_rgb_speed:
            rgb_matrix_set_speed_noeeprom(value_data[0]);
            n32_rgb_mark_save_pending();
            break;
        case id_n32_rgb_zone:
            switch (value_data[0]) {
                case 0:
                    rgb_matrix_set_flags_noeeprom(LED_FLAG_ALL);
                    break;
                case 1:
                    rgb_matrix_set_flags_noeeprom(LED_FLAG_KEYLIGHT);
                    break;
                case 2:
                    rgb_matrix_set_flags_noeeprom(LED_FLAG_INDICATOR);
                    break;
                default:
                    rgb_matrix_set_flags_noeeprom(LED_FLAG_ALL);
                    break;
            }
            n32_rgb_mark_save_pending();
            break;
#    endif
    }
}

static void n32_via_get_value(uint8_t *data) {
    uint8_t *value_id   = &data[0];
    uint8_t *value_data = &data[1];

    switch (*value_id) {
        case id_n32_report_rate:
            value_data[0] = n32_config.report_rate_index;
            break;
        case id_n32_debounce:
            value_data[0] = n32_config.debounce_ms;
            break;
#    ifdef RGB_MATRIX_ENABLE
        case id_n32_rgb_enable:
            value_data[0] = rgb_matrix_is_enabled();
            break;
        case id_n32_rgb_mode:
            value_data[0] = rgb_matrix_is_enabled() ? n32_qmk_rgb_mode_to_via(rgb_matrix_get_mode()) : 0;
            break;
        case id_n32_rgb_hue:
            value_data[0] = rgb_matrix_get_hue();
            break;
        case id_n32_rgb_sat:
            value_data[0] = rgb_matrix_get_sat();
            break;
        case id_n32_rgb_val:
            value_data[0] = rgb_matrix_get_val();
            break;
        case id_n32_rgb_speed:
            value_data[0] = rgb_matrix_get_speed();
            break;
        case id_n32_rgb_zone:
            switch (rgb_matrix_get_flags()) {
                case LED_FLAG_KEYLIGHT:
                    value_data[0] = 1;
                    break;
                case LED_FLAG_INDICATOR:
                    value_data[0] = 2;
                    break;
                default:
                    value_data[0] = 0;
                    break;
            }
            break;
#    endif
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uint8_t *command_id        = &data[0];
    uint8_t *channel_id        = &data[1];
    uint8_t *value_id_and_data = &data[2];

    (void)length;

    if (*channel_id == id_custom_channel) {
        switch (*command_id) {
            case id_custom_set_value:
                n32_via_set_value(value_id_and_data);
                break;
            case id_custom_get_value:
                n32_via_get_value(value_id_and_data);
                break;
            case id_custom_save:
                n32_runtime_config_save();
                break;
            default:
                *command_id = id_unhandled;
                break;
        }
        return;
    }

    *command_id = id_unhandled;
}
#endif

#if defined(QUANTUM_PAINTER_ENABLE) && defined(VIA_ENABLE)
bool via_command_kb(uint8_t *data, uint8_t length) {
    if (length < 32 || data[0] != N32_VIA_IMAGE_CMD) {
        return false;
    }

    uint8_t subcmd = data[1];
    uint8_t status = N32_VIA_IMAGE_STATUS_OK;

    switch (subcmd) {
        case N32_VIA_IMAGE_INFO:
            n32_put_u32_le(&data[3], N32_TFT_IMAGE_FLASH_SIZE);
            n32_put_u32_le(&data[7], n32_tft_upload_active ? n32_tft_upload_size : 0);
            n32_put_u32_le(&data[11], n32_tft_upload_offset);
            data[15] = N32_VIA_IMAGE_PAYLOAD_SIZE;
            data[16] = n32_tft_load_image() ? n32_tft_image->frame_count : 0;
            data[17] = n32_tft_image ? (n32_tft_image->width & 0xFF) : 0;
            data[18] = n32_tft_image ? (n32_tft_image->width >> 8) : 0;
            data[19] = n32_tft_image ? (n32_tft_image->height & 0xFF) : 0;
            data[20] = n32_tft_image ? (n32_tft_image->height >> 8) : 0;
            break;
        case N32_VIA_IMAGE_BEGIN:
            status = n32_tft_upload_begin(n32_u32_le(&data[2]));
            break;
        case N32_VIA_IMAGE_DATA:
            status = n32_tft_upload_data(n32_u32_le(&data[2]), data[6], &data[7]);
            break;
        case N32_VIA_IMAGE_END:
            status = n32_tft_upload_end();
            if (status == N32_VIA_IMAGE_STATUS_OK) {
                n32_tft_render();
            }
            break;
        case N32_VIA_IMAGE_ABORT:
            n32_tft_upload_active     = false;
            n32_tft_upload_page_dirty = false;
            break;
        case N32_VIA_IMAGE_SHOW:
            n32_tft_page       = 3;
            n32_tft_page_timer = timer_read32();
            n32_tft_render();
            break;
        case N32_VIA_IMAGE_CLEAR:
            n32_tft_stop_image();
            if (n32_flash_begin()) {
                status = n32_flash_erase_page(N32_TFT_IMAGE_FLASH_BASE) ? N32_VIA_IMAGE_STATUS_OK : N32_VIA_IMAGE_STATUS_FLASH;
                n32_flash_end();
            } else {
                status = N32_VIA_IMAGE_STATUS_BUSY;
            }
            n32_tft_page = 3;
            n32_tft_render();
            break;
        default:
            status = N32_VIA_IMAGE_STATUS_INVALID;
            break;
    }

    data[0] = N32_VIA_IMAGE_CMD;
    data[1] = subcmd;
    data[2] = status;
    raw_hid_send(data, length);
    return true;
}
#endif

#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_kb(void) {
    if (!rgb_matrix_indicators_user()) {
        return false;
    }

    static uint8_t last_caps = 0xFF;
    led_t          led_state = host_keyboard_led_state();

    if (last_caps != led_state.caps_lock) {
        last_caps = led_state.caps_lock;
        n32_rgb_tracef("RGB indicator: caps=%u led1=%s\n", led_state.caps_lock, led_state.caps_lock ? "white" : "effect");
    }

    if (led_state.caps_lock) {
        rgb_matrix_set_color(1, 255, 255, 255);
    }

    return true;
}
#endif
