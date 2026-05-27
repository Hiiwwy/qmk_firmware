ifeq ($(strip $(BOOTLOADER)),)
    BOOTLOADER = none
endif

ifeq ($(strip $(N32_BOOTLOADER)),n32-dfu)
    BOOTLOADER_TYPE = n32_dfu
else
    BOOTLOADER_TYPE = none
endif
