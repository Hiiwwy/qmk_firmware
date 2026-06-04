# MCU 内核类型
MCU = cortex-m4
# 具体 N32 芯片型号
N32_MCU = N32H487ZEL7
# N32 平台板级配置
BOARD = N32H487ZEL7_STB
# 使用自定义 bootloader
BOOTLOADER = custom
# N32 DFU 下载方式
N32_BOOTLOADER = n32-dfu
# 使用 N32 FreeRTOS 平台协议
PROTOCOL = n32_freertos

# 使用 N32 自有 EEPROM/Flash wear-leveling 存储
EEPROM_DRIVER = custom
# 使用键盘自有消抖实现
DEBOUNCE_TYPE = custom

# 启用 QMK debug 支持
DEBUG_ENABLE = yes
# 启用 RTT/console 日志输出
CONSOLE_ENABLE = yes
# 启用 VIA 支持
VIA_ENABLE ?= yes
# 启用鼠标键
MOUSEKEY_ENABLE = yes
# 启用多媒体键/系统控制键
EXTRAKEY_ENABLE = yes
# 启用 NKRO
NKRO_ENABLE = yes
# 启用 RGB Matrix
RGB_MATRIX_ENABLE = yes
# RGB Matrix 使用 WS2812 驱动
RGB_MATRIX_DRIVER = ws2812
# 使用 N32 自有 WS2812 驱动入口
WS2812_DRIVER = vendor
ifeq ($(strip $(RGB_MATRIX_ENABLE)),yes)
SRC += n32_rgb_config.c
endif
# USB 默认高速模式；可用 -e N32_USB_SPEED=fs 编译全速
N32_USB_SPEED ?= hs
# 板载 HSE 晶振频率
N32_HSE_VALUE ?= 8000000U
# 输出矩阵扫描调试日志
N32_TRACE_MATRIX ?= no
# 输出矩阵事件日志
N32_TRACE_MATRIX_EVENT ?= no
# 输出性能统计日志
N32_TRACE_PERF ?= no
# 输出 VIA 矩阵测试日志
N32_TRACE_VIA_MATRIX ?= no
# 输出 USB 回报率状态日志
N32_TRACE_USB_REPORT_RATE ?= yes
# 输出 USB 运行时日志
N32_TRACE_USB_RUNTIME ?= no
# 输出 USB HS 底层调试日志
N32_TRACE_USB_HS ?= no
# 矩阵扫描驱动；dma_timer 为定时器+DMA 扫描
N32_MATRIX_DRIVER ?= dma_timer
# 消抖模式；eager_pk 为当前低延迟方案
N32_DEBOUNCE_MODE ?= eager_pk
# DMA 矩阵列周期，单位 us
N32_MATRIX_DMA_COLUMN_US ?= 9U
# 切列后等待稳定的 CPU cycle 数
N32_MATRIX_DMA_SWITCH_DEAD_CYCLES ?= 240U
# 每列周期内采样位置百分比
N32_MATRIX_DMA_SAMPLE_PERCENT ?= 90U
# 采样相位偏移
N32_MATRIX_DMA_SAMPLE_PHASE ?= 0
# 按下确认帧数
N32_MATRIX_DMA_PRESS_CONFIRM_FRAMES ?= 2U
# 释放确认帧数
N32_MATRIX_DMA_RELEASE_CONFIRM_FRAMES ?= 6U
# 列线同端口硬件切列模式
N32_MATRIX_DMA_HW_COLUMNS ?= single_port
# 启用 TFT/Quantum Painter
# QUANTUM_PAINTER_ENABLE = yes
# TFT 使用 ST7789 SPI 驱动
# QUANTUM_PAINTER_DRIVERS = st7789_spi
# 启用 WPM 统计
# WPM_ENABLE = yes

ifeq ($(strip $(N32_TRACE_VIA_MATRIX)),yes)
OPT_DEFS += -DN32_TRACE_VIA_MATRIX
endif
ifeq ($(strip $(N32_TRACE_USB_REPORT_RATE)),yes)
OPT_DEFS += -DN32_TRACE_USB_REPORT_RATE
endif

SRC += n32_debounce.c
ifeq ($(strip $(N32_DEBOUNCE_MODE)),eager_pk)
OPT_DEFS += -DN32_DEBOUNCE_MODE_EAGER_PK
else ifeq ($(strip $(N32_DEBOUNCE_MODE)),defer_g)
OPT_DEFS += -DN32_DEBOUNCE_MODE_DEFER_G
else
    $(error N32_DEBOUNCE_MODE must be eager_pk or defer_g)
endif

ifeq ($(strip $(N32_MATRIX_DRIVER)),dma_timer)
OPT_DEFS += -DN32_MATRIX_DMA_TIMER
OPT_DEFS += -DN32_MATRIX_DMA_COLUMN_US=$(strip $(N32_MATRIX_DMA_COLUMN_US))
OPT_DEFS += -DN32_MATRIX_DMA_SWITCH_DEAD_CYCLES=$(strip $(N32_MATRIX_DMA_SWITCH_DEAD_CYCLES))
OPT_DEFS += -DN32_MATRIX_DMA_SAMPLE_PERCENT=$(strip $(N32_MATRIX_DMA_SAMPLE_PERCENT))
OPT_DEFS += -DN32_MATRIX_DMA_SAMPLE_PHASE=$(strip $(N32_MATRIX_DMA_SAMPLE_PHASE))
OPT_DEFS += -DN32_MATRIX_DMA_PRESS_CONFIRM_FRAMES=$(strip $(N32_MATRIX_DMA_PRESS_CONFIRM_FRAMES))
OPT_DEFS += -DN32_MATRIX_DMA_RELEASE_CONFIRM_FRAMES=$(strip $(N32_MATRIX_DMA_RELEASE_CONFIRM_FRAMES))
ifeq ($(strip $(N32_MATRIX_DMA_HW_COLUMNS)),yes)
OPT_DEFS += -DN32_MATRIX_DMA_HW_COLUMNS=1
else ifeq ($(strip $(N32_MATRIX_DMA_HW_COLUMNS)),single_port)
OPT_DEFS += -DN32_MATRIX_DMA_HW_COLUMNS=2
else ifeq ($(strip $(N32_MATRIX_DMA_HW_COLUMNS)),no)
OPT_DEFS += -DN32_MATRIX_DMA_HW_COLUMNS=0
else
    $(error N32_MATRIX_DMA_HW_COLUMNS must be yes, no, or single_port)
endif
SRC += n32_matrix_dma_timer.c
else ifeq ($(strip $(N32_MATRIX_DRIVER)),gpio)
SRC += n32_matrix.c
else
    $(error N32_MATRIX_DRIVER must be gpio or dma_timer)
endif
