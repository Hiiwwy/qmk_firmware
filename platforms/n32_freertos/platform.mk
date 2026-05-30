# N32H47x/48x + FreeRTOS private QMK platform.

N32_PROJECT_ROOT ?= $(TOP_DIR)/..
N32_SDK_ROOT ?= $(N32_PROJECT_ROOT)/Nations.N32H47x_48x_Library.1.2.0
N32_TARGET_PLATFORM := n32h47x_48x
N32_USBHS_DRIVER := $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_usbhs_driver
N32_USBFSD_DRIVER := $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_usbfsd_driver
N32_BSP_ROOT := $(N32_SDK_ROOT)/projects/n32h47x_48x_EVAL/bsp
FREERTOS_ROOT := $(N32_SDK_ROOT)/middlewares/FreeRTOSv202212.01/Source
N32_EEPROM_LOGICAL_SIZE ?= 4096
N32_EEPROM_BACKING_SIZE ?= 8192
N32_HSE_VALUE ?= 24000000U
N32_EEPROM_MIGRATE_RAW_FORMAT ?= yes
N32_QMK_TASK_STACK_WORDS ?= 4096
N32_QMK_DEBUG_ALL ?= yes
N32_TRACE_EEPROM ?= no
N32_TRACE_KEYBOARD_INIT ?= yes
N32_TRACE_USB_SETUP ?= no
N32_USBFS_SINGLE_INTERFACE ?= no
N32_USBFS_MINIMAL_KEYBOARD ?= no
N32_USBFS_SDK_IDENTITY ?= no
N32_USBFS_ESOF_FORCE_RESET ?= no
N32_USB_SPEED ?= fs

N32_USB_SPEED := $(strip $(N32_USB_SPEED))
ifeq ($(N32_USB_SPEED),hs)
    N32_USB_REPORT_RATE_DEFAULT ?= 8000
    N32_USB_SPEED_DEFS := -DN32_USB_USE_HS -DN32_USB_SPEED_HS -DUSE_USB_HS_IN_HS
else ifeq ($(N32_USB_SPEED),fs)
    N32_USB_REPORT_RATE_DEFAULT ?= 1000
    N32_USB_SPEED_DEFS := -DN32_USB_USE_FSD -DN32_USB_SPEED_FS
else
    $(error N32_USB_SPEED must be hs or fs)
endif

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
LDFLAGS += -T$(PLATFORM_COMMON_DIR)/ld/n32h487_qmk_flash.ld

OPT_DEFS += -DN32H487
OPT_DEFS += -DUSE_STDPERIPH_DRIVER
OPT_DEFS += -D__FPU_PRESENT=1
OPT_DEFS += -DUSE_DEVICE_MODE
OPT_DEFS += $(N32_USB_SPEED_DEFS)
OPT_DEFS += -DHSE_VALUE=$(N32_HSE_VALUE)
OPT_DEFS += -DN32_USB_REPORT_RATE_DEFAULT=$(N32_USB_REPORT_RATE_DEFAULT)
OPT_DEFS += -DN32_QMK_TASK_STACK_WORDS=$(N32_QMK_TASK_STACK_WORDS)
OPT_DEFS += -DSYSCLK_SRC=SYSCLK_USE_HSE_PLL
OPT_DEFS += -DN32_QMK_TASK_DELAY_MS=0
OPT_DEFS += -DSHARED_EP_ENABLE
OPT_DEFS += -DKEYBOARD_SHARED_EP
OPT_DEFS += -DMOUSE_SHARED_EP
OPT_DEFS += -DEEPROM_WEAR_LEVELING
OPT_DEFS += -DWEAR_LEVELING_CUSTOM
OPT_DEFS += -DBACKING_STORE_WRITE_SIZE=8
OPT_DEFS += -DWEAR_LEVELING_BACKING_SIZE=$(N32_EEPROM_BACKING_SIZE)
OPT_DEFS += -DWEAR_LEVELING_LOGICAL_SIZE=$(N32_EEPROM_LOGICAL_SIZE)
ifeq ($(strip $(N32_EEPROM_MIGRATE_RAW_FORMAT)), yes)
OPT_DEFS += -DN32_EEPROM_MIGRATE_RAW_FORMAT
endif

EXTRAINCDIRS += \
    $(N32_SDK_ROOT)/firmware/CMSIS/device/startup \
    $(PLATFORM_COMMON_DIR) \
    $(PLATFORM_COMMON_DIR)/boards/$(BOARD) \
    $(QUANTUM_PATH)/wear_leveling \
    $(TOP_DIR)/lib/fnv \
    $(N32_SDK_ROOT)/firmware/CMSIS/core \
    $(N32_SDK_ROOT)/firmware/CMSIS/device \
    $(N32_SDK_ROOT)/firmware/$(N32_TARGET_PLATFORM)_std_periph_driver/inc \
    $(N32_BSP_ROOT)/inc \
    $(FREERTOS_ROOT)/include \
    $(FREERTOS_ROOT)/portable/GCC/ARM_CM4F \
    $(PLATFORM_COMMON_DIR)/freertos_config \
    $(if $(filter hs,$(N32_USB_SPEED)),$(N32_USBHS_DRIVER)/driver/inc) \
    $(if $(filter hs,$(N32_USB_SPEED)),$(N32_USBHS_DRIVER)/device/core/inc) \
    $(if $(filter fs,$(N32_USB_SPEED)),$(N32_USBFSD_DRIVER)/inc)

SRC += \
    $(PLATFORM_COMMON_DIR)/gpio.c \
    $(PLATFORM_COMMON_DIR)/wait.c \
    $(DRIVER_PATH)/eeprom/eeprom_wear_leveling.c \
    $(QUANTUM_PATH)/wear_leveling/wear_leveling.c \
    $(TOP_DIR)/lib/fnv/hash_64a.c \
    $(PLATFORM_COMMON_DIR)/n32_flash_ops.c \
    $(PLATFORM_COMMON_DIR)/wear_leveling_n32_flash.c \
    $(PLATFORM_COMMON_DIR)/freertos_hooks.c \
    $(PLATFORM_COMMON_DIR)/n32_fault_handlers.c \
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
    $(FREERTOS_ROOT)/portable/MemMang/heap_4.c

ifeq ($(N32_USB_SPEED),hs)
SRC += \
    $(PLATFORM_COMMON_DIR)/n32_usbhs_bsp.c \
    $(PLATFORM_COMMON_DIR)/n32_usbhs_descriptors.c \
    $(PLATFORM_COMMON_DIR)/n32_usbhs_keyboard.c \
    $(N32_USBHS_DRIVER)/driver/src/usbhs_core.c \
    $(N32_USBHS_DRIVER)/driver/src/usbhs_dcd.c \
    $(N32_USBHS_DRIVER)/driver/src/usbhs_dcd_int.c \
    $(N32_USBHS_DRIVER)/device/core/src/usbd_core.c \
    $(N32_USBHS_DRIVER)/device/core/src/usbd_ioreq.c \
    $(N32_USBHS_DRIVER)/device/core/src/usbd_req.c
else
SRC += \
    $(PLATFORM_COMMON_DIR)/n32_usbfsd_keyboard.c \
    $(PLATFORM_COMMON_DIR)/n32_usbfsd_istr.c \
    $(N32_USBFSD_DRIVER)/src/usbfsd_core.c \
    $(N32_USBFSD_DRIVER)/src/usbfsd_init.c \
    $(N32_USBFSD_DRIVER)/src/usbfsd_int.c \
    $(N32_USBFSD_DRIVER)/src/usbfsd_mem.c \
    $(N32_USBFSD_DRIVER)/src/usbfsd_regs.c \
    $(N32_USBFSD_DRIVER)/src/usbfsd_sil.c
endif

VPATH += \
    $(PLATFORM_COMMON_DIR) \
    $(PLATFORM_COMMON_DIR)/boards/$(BOARD) \
    $(PLATFORM_COMMON_DIR)/freertos_config

ifeq ($(strip $(CONSOLE_ENABLE)), yes)
ifeq ($(strip $(N32_QMK_DEBUG_ALL)), yes)
OPT_DEFS += -DN32_QMK_DEBUG_ALL -DACTION_DEBUG
endif
ifeq ($(strip $(N32_TRACE_KEYBOARD_INIT)), yes)
OPT_DEFS += -DN32_TRACE_KEYBOARD_INIT
endif
ifeq ($(strip $(N32_TRACE_EEPROM)), yes)
LDFLAGS += -Wl,--wrap=eeprom_read_block -Wl,--wrap=eeprom_write_block
SRC += $(PLATFORM_COMMON_DIR)/n32_eeprom_trace.c
endif
ifeq ($(strip $(N32_TRACE_USB_SETUP)), yes)
OPT_DEFS += -DN32_TRACE_USB_SETUP
endif
ifeq ($(strip $(N32_USBFS_SINGLE_INTERFACE)), yes)
OPT_DEFS += -DN32_USBFS_SINGLE_INTERFACE
endif
ifeq ($(strip $(N32_USBFS_MINIMAL_KEYBOARD)), yes)
OPT_DEFS += -DN32_USBFS_MINIMAL_KEYBOARD
endif
ifeq ($(strip $(N32_USBFS_SDK_IDENTITY)), yes)
OPT_DEFS += -DN32_USBFS_SDK_IDENTITY
endif
ifeq ($(strip $(N32_USBFS_ESOF_FORCE_RESET)), yes)
OPT_DEFS += -DN32_USBFS_ESOF_FORCE_RESET
endif
SRC += \
    $(PLATFORM_COMMON_DIR)/sendchar.c \
    $(PLATFORM_COMMON_DIR)/rtt/SEGGER_RTT.c
EXTRAINCDIRS += $(PLATFORM_COMMON_DIR)/rtt
endif

check-size:
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

bin: $(BUILD_DIR)/$(TARGET).hex
	$(BIN) $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
