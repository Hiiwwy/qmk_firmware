#include "bootloader.h"
#include "n32h47x_48x.h"

void bootloader_jump(void) {
    /* TODO: jump to ROM bootloader or a project-specific USB DFU loader. */
}

void mcu_reset(void) {
    NVIC_SystemReset();
    while (1) {
    }
}
