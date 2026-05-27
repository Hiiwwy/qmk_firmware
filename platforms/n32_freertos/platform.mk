# N32H47x/48x + FreeRTOS private QMK platform.

N32_PROJECT_ROOT ?= /f/N32
N32_SDK_ROOT ?= $(N32_PROJECT_ROOT)/Nations.N32H47x_48x_Library.1.2.0
N32_TARGET_PLATFORM := n32h47x_48x
N32_USBHS_DRIVER := $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_usbhs_driver
N32_BSP_ROOT := $(N32_SDK_ROOT)/projects/n32h47x_48x_EVAL/bsp
FREERTOS_ROOT := $(N32_SDK_ROOT)/middlewares/FreeRTOSv202212.01/Source

CC = $(CC_PREFIX) arm-none-eabi-gcc
OBJCOPY = $(CC_PREFIX) arm-none-eabi-objcopy
OBJDUMP = $(CC_PREFIX) arm-none-eabi-objdump
SIZE = $(CC_PREFIX) arm-none-eabi-size
AR = $(CC_PREFIX) arm-none-eabi-ar
NM = $(CC_PREFIX) arm-none-eabi-nm
HEX = $(OBJCOPY) -O ihex -S
BIN = $(OBJCOPY) -O binary -S

FORMAT = ihex
FIRMWARE_FORMAT = hex

MCUFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

COMPILEFLAGS += $(MCUFLAGS)
COMPILEFLAGS += -funsigned-char
COMPILEFLAGS += -funsigned-bitfields
COMPILEFLAGS += -ffunction-sections
COMPILEFLAGS += -fdata-sections
COMPILEFLAGS += -fshort-enums
COMPILEFLAGS += -Wall
COMPILEFLAGS += -Wno-unused-parameter

CFLAGS += $(COMPILEFLAGS)
CFLAGS += -fno-inline-small-functions
CFLAGS += -fno-strict-aliasing
CXXFLAGS += $(COMPILEFLAGS)
CXXFLAGS += -fno-exceptions $(CXXSTANDARD)
ASFLAGS += $(MCUFLAGS)

LDFLAGS += $(MCUFLAGS)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --specs=nosys.specs
LDFLAGS += -T$(N32_SDK_ROOT)/firmware/CMSIS/device/$(N32_TARGET_PLATFORM)_flash.ld

OPT_DEFS += -DN32H487
OPT_DEFS += -DUSE_STDPERIPH_DRIVER
OPT_DEFS += -D__FPU_PRESENT=1
OPT_DEFS += -DN32_USB_USE_HS
OPT_DEFS += -DUSE_DEVICE_MODE
OPT_DEFS += -DUSE_USB_HS_IN_HS
OPT_DEFS += -DSYSCLK_SRC=SYSCLK_USE_HSE_PLL
OPT_DEFS += -DN32_QMK_TASK_DELAY_MS=0

EXTRAINCDIRS += \
    $(N32_SDK_ROOT)/firmware/CMSIS/device/startup \
    $(PLATFORM_COMMON_DIR) \
    $(PLATFORM_COMMON_DIR)/boards/$(BOARD) \
    $(N32_SDK_ROOT)/firmware/CMSIS/core \
    $(N32_SDK_ROOT)/firmware/CMSIS/device \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/inc \
    $(N32_BSP_ROOT)/inc \
    $(FREERTOS_ROOT)/include \
    $(FREERTOS_ROOT)/portable/GCC/ARM_CM4F \
    $(PLATFORM_COMMON_DIR)/freertos_config \
    $(N32_USBHS_DRIVER)/driver/inc \
    $(N32_USBHS_DRIVER)/device/core/inc

SRC += \
    $(PLATFORM_COMMON_DIR)/gpio.c \
    $(PLATFORM_COMMON_DIR)/wait.c \
    $(PLATFORM_COMMON_DIR)/eeprom.c \
    $(PLATFORM_COMMON_DIR)/freertos_hooks.c \
    $(PLATFORM_COMMON_DIR)/interrupt_handlers.c \
    $(PLATFORM_COMMON_DIR)/usb_driver.c \
    $(PLATFORM_COMMON_DIR)/boards/$(BOARD)/board.c \
    $(N32_SDK_ROOT)/firmware/CMSIS/device/system_$(N32_TARGET_PLATFORM).c \
    $(PLATFORM_COMMON_DIR)/n32_startup.S \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/misc.c \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/n32h47x_48x_exti.c \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/n32h47x_48x_flash.c \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/n32h47x_48x_gpio.c \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/n32h47x_48x_pwr.c \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/n32h47x_48x_rcc.c \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/src/n32h47x_48x_tim.c \
    $(N32_BSP_ROOT)/src/delay.c \
    $(FREERTOS_ROOT)/croutine.c \
    $(FREERTOS_ROOT)/event_groups.c \
    $(FREERTOS_ROOT)/list.c \
    $(FREERTOS_ROOT)/queue.c \
    $(FREERTOS_ROOT)/stream_buffer.c \
    $(FREERTOS_ROOT)/tasks.c \
    $(FREERTOS_ROOT)/timers.c \
    $(FREERTOS_ROOT)/portable/GCC/ARM_CM4F/port.c \
    $(FREERTOS_ROOT)/portable/MemMang/heap_4.c \
    $(PLATFORM_COMMON_DIR)/n32_usbhs_bsp.c \
    $(PLATFORM_COMMON_DIR)/n32_usbhs_descriptors.c \
    $(PLATFORM_COMMON_DIR)/n32_usbhs_keyboard.c \
    $(N32_USBHS_DRIVER)/driver/src/usbhs_core.c \
    $(N32_USBHS_DRIVER)/driver/src/usbhs_dcd.c \
    $(N32_USBHS_DRIVER)/driver/src/usbhs_dcd_int.c \
    $(N32_USBHS_DRIVER)/device/core/src/usbd_core.c \
    $(N32_USBHS_DRIVER)/device/core/src/usbd_ioreq.c \
    $(N32_USBHS_DRIVER)/device/core/src/usbd_req.c

VPATH += \
    $(PLATFORM_COMMON_DIR)/boards/$(BOARD) \
    $(PLATFORM_COMMON_DIR)/freertos_config

check-size:
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

bin: $(BUILD_DIR)/$(TARGET).hex
	$(BIN) $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
