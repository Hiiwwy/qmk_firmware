// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>

// RGB 分区 ID；必须和 VIA JSON 中的 zone 选项顺序保持一致。
#define N32_RGB_ZONE_AXIS 0
#define N32_RGB_ZONE_UNDERGLOW 1
#define N32_RGB_ZONE_INDICATOR 2
#define N32_RGB_ZONE_COUNT 3

// VIA 自定义 RGB 模式 ID；0=关闭，1=纯色，其余映射到 QMK RGB Matrix 效果。
#define N32_RGB_MODE_OFF 0
#define N32_RGB_MODE_SOLID 1

// 单个 RGB 区域的运行时配置。Axis 的 mode/hsv/speed 是共享主灯效来源。
typedef struct {
    uint8_t enable;
    uint8_t mode;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t speed;
} n32_rgb_zone_config_t;

#ifdef RGB_MATRIX_ENABLE
uint8_t n32_rgb_zone_value(uint8_t value);
void    n32_rgb_sanitize_zone(n32_rgb_zone_config_t *zone);
void    n32_rgb_apply_main_effect_to_qmk(const n32_rgb_zone_config_t *axis);
void    n32_rgb_apply_zone_postprocess(const n32_rgb_zone_config_t *axis, const n32_rgb_zone_config_t *underglow, const n32_rgb_zone_config_t *indicator);
#endif