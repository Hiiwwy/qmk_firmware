// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define MATRIX_ROWS 5                               // 矩阵行数
#define MATRIX_COLS 6                               // 矩阵列数
#define MATRIX_ROW_PINS {B0, B2, D4, A6, C4}        // 行引脚
#define MATRIX_COL_PINS {C6, C7, C8, C11, C10, C12} // 列引脚
#define DIODE_DIRECTION ROW2COL                     // 二极管方向
#define EEPROM_SIZE 1024                            // VIA/QMK 配置存储大小
#define NKRO_DEFAULT_ON true                        // 默认开启 NKRO
#define VIA_INSECURE                                // 允许 VIA 动态改键
#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 32            // N32 自定义 VIA 参数存储空间
#define VIA_FIRMWARE_VERSION 0x00000008             // VIA JSON/固件匹配版本

#define WS2812_DI_PIN A8                            // WS2812 数据输出引脚
#define N32_WS2812_DI_PIN_NAME "A8"                 // RGB 日志中显示的引脚名
#define RGB_MATRIX_LED_COUNT 10                     // RGB 灯珠总数
#define N32_RGB_AXIS_LED_COUNT 6                    // 轴灯数量，当前约定 LED 0..5 为轴灯
#define N32_RGB_UNDERGLOW_LED_COUNT 4               // 底灯数量，当前约定紧跟轴灯之后
#define N32_RGB_INDICATOR_LED_INDEX 1               // Caps 指示灯覆盖目标，1 表示第二颗灯
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 160           // RGB 最大亮度限制
#define ENABLE_RGB_MATRIX_BREATHING                 // 启用呼吸灯效
#define ENABLE_RGB_MATRIX_CYCLE_ALL                 // 启用全局循环灯效
#define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT          // 启用左右循环灯效
#define ENABLE_RGB_MATRIX_HUE_BREATHING             // 启用色相呼吸灯效
#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON    // 启用彩虹流动灯效
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE     // 启用简单响应式灯效

#define SPI_SCK_PIN B5                              // TFT SPI SCK 引脚
#define SPI_MISO_PIN C0                             // TFT SPI MISO 引脚
#define SPI_MOSI_PIN B6                             // TFT SPI MOSI 引脚
// Keep TFT backlight away from PB4/NJTRST so J-Link/SWD debug stays reliable.
#define N32_TFT_BL_PIN C1                           // TFT 背光引脚
#define N32_TFT_RST_PIN B7                          // TFT 复位引脚
#define N32_TFT_DC_PIN B8                           // TFT DC 引脚
#define N32_TFT_CS_PIN B9                           // TFT CS 引脚
#define N32_TFT_WIDTH 172                           // TFT 可见宽度
#define N32_TFT_HEIGHT 320                          // TFT 可见高度
#define N32_TFT_OFFSET_X 34                         // TFT X 偏移
#define N32_TFT_OFFSET_Y 0                          // TFT Y 偏移
#define N32_TFT_SPI_DIVISOR 16                      // TFT SPI 分频
#define N32_TFT_SPI_MODE 3                          // TFT SPI 模式

#define QUANTUM_PAINTER_SUPPORTS_256_PALETTE TRUE   // TFT 图像支持 256 色调色板
#define QUANTUM_PAINTER_CONCURRENT_ANIMATIONS 1     // TFT 同时动画数量
#define N32_TFT_IMAGE_FLASH_BASE 0x08070000U        // TFT 图片存储起始地址
#define N32_TFT_IMAGE_FLASH_SIZE 0x0000E000U        // TFT 图片存储区域大小
#define N32_TFT_IMAGE_FLASH_PAGE_SIZE 0x00002000U   // TFT 图片 Flash 页大小
