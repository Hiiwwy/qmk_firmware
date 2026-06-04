// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "n32_rgb_config.h"

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
        case N32_RGB_MODE_SOLID:
        default:
            return RGB_MATRIX_SOLID_COLOR;
    }
}

uint8_t n32_rgb_zone_value(uint8_t value) {
    return value > RGB_MATRIX_MAXIMUM_BRIGHTNESS ? RGB_MATRIX_MAXIMUM_BRIGHTNESS : value;
}

// 将某个区域的 HSV 配置转换成实际 RGB，主要用于指示灯覆盖色。
static rgb_t n32_rgb_zone_rgb(const n32_rgb_zone_config_t *zone) {
    hsv_t hsv = {
        .h = zone->hue,
        .s = zone->sat,
        .v = n32_rgb_zone_value(zone->val),
    };

    return hsv_to_rgb(hsv);
}

// 防止 EEPROM/VIA 里保存的配置越界。
void n32_rgb_sanitize_zone(n32_rgb_zone_config_t *zone) {
    zone->enable = zone->enable ? 1 : 0;
    if (zone->mode > 7) {
        zone->mode = N32_RGB_MODE_SOLID;
    }
    zone->val = n32_rgb_zone_value(zone->val);
}

// 把 VIA Main Effect 参数同步给 QMK RGB Matrix，作为轴灯/底灯共享基础灯效。
void n32_rgb_apply_main_effect_to_qmk(const n32_rgb_zone_config_t *axis) {
    rgb_matrix_set_flags_noeeprom(LED_FLAG_ALL);
    rgb_matrix_enable_noeeprom();

    if (axis->mode > N32_RGB_MODE_OFF) {
        rgb_matrix_mode_noeeprom(n32_via_rgb_mode_to_qmk(axis->mode));
        rgb_matrix_sethsv_noeeprom(axis->hue, axis->sat, n32_rgb_zone_value(axis->val));
        rgb_matrix_set_speed_noeeprom(axis->speed);
    }
}

static void n32_rgb_set_led_range(uint8_t first, uint8_t count, rgb_t color) {
    for (uint8_t i = 0; i < count && first + i < RGB_MATRIX_LED_COUNT; ++i) {
        rgb_matrix_set_color(first + i, color.r, color.g, color.b);
    }
}

// RGB Matrix 生成基础效果后，再用这里实现 Axis/Underglow/Indicator 区域开关。
void n32_rgb_apply_zone_postprocess(const n32_rgb_zone_config_t *axis, const n32_rgb_zone_config_t *underglow, const n32_rgb_zone_config_t *indicator) {
    rgb_t black = {0, 0, 0};

    if (!axis->enable || axis->mode == N32_RGB_MODE_OFF) {
        n32_rgb_set_led_range(0, N32_RGB_AXIS_LED_COUNT, black);
    }

    if (axis->mode == N32_RGB_MODE_OFF || !underglow->enable) {
        n32_rgb_set_led_range(N32_RGB_AXIS_LED_COUNT, N32_RGB_UNDERGLOW_LED_COUNT, black);
    }

    led_t led_state = host_keyboard_led_state();
    if (led_state.caps_lock && indicator->enable && indicator->mode != N32_RGB_MODE_OFF && N32_RGB_INDICATOR_LED_INDEX < RGB_MATRIX_LED_COUNT) {
        rgb_t color = n32_rgb_zone_rgb(indicator);
        rgb_matrix_set_color(N32_RGB_INDICATOR_LED_INDEX, color.r, color.g, color.b);
    }
}

// RGB Matrix LED 映射。新键盘需要按实际灯珠顺序修改矩阵映射、坐标和区域 flag。
led_config_t g_led_config = {
    {
        {0, 1, 2, 3, 4, 5},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
        {NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED},
    },
    {
        {0, 0},
        {45, 0},
        {90, 0},
        {134, 0},
        {179, 0},
        {224, 0},
        {0, 64},
        {75, 64},
        {149, 64},
        {224, 64},
    },
    {
        LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW,
    },
};
#endif