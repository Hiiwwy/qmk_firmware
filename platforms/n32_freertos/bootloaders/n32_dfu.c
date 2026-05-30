#include "bootloader.h"
#include "n32h47x_48x.h"
#include "n32h47x_48x_flash.h"
#include "n32h47x_48x_rcc.h"

#include <stdbool.h>
#include <stdint.h>

#define N32_BOOTLOADER_BOOT_ADDR 0x1FFF01A1U
#define N32_BOOTLOADER_GET_NVR_ADDR 0x1FFF02A5U
#define N32_BOOTLOADER_VERSION_ADDR 0x1FFF37A8U
#define N32_BOOTLOADER_SP_V10 0x200024A8U
#define N32_BOOTLOADER_SP_V11 0x200024C0U
#define N32_BOOTLOADER_SRAM_VECTOR_ADDR (0x20000000U + 0x24000U - 0x200U)
#define N32_BOOTLOADER_VECTOR_WORDS 128U

typedef enum {
    N32_BOOT_CMD_SUCCESS = 0x00,
    N32_BOOT_CMD_FAILED  = 0x01,
} n32_boot_cmd_status_t;

typedef n32_boot_cmd_status_t (*n32_boot_get_nvr_t)(uint32_t addr, uint8_t *data, uint32_t len);
typedef void (*n32_boot_jump_t)(void);

static uint32_t n32_bootloader_prepare_vector_table(void) {
    uint8_t               version[8] = {0};
    uint32_t             *vectors    = (uint32_t *)N32_BOOTLOADER_SRAM_VECTOR_ADDR;
    n32_boot_get_nvr_t    get_nvr    = (n32_boot_get_nvr_t)N32_BOOTLOADER_GET_NVR_ADDR;
    n32_boot_cmd_status_t status     = get_nvr(N32_BOOTLOADER_VERSION_ADDR, version, sizeof(version));
    bool                  boot_v11   = status == N32_BOOT_CMD_SUCCESS && version[0] == 0x11;

    for (uint32_t i = 0; i < N32_BOOTLOADER_VECTOR_WORDS; ++i) {
        vectors[i] = 0;
    }

    if (boot_v11) {
        vectors[SysTick_IRQn + 16]   = 0x1FFF2F15U;
        vectors[USART3_IRQn + 16]    = 0x1FFF2F95U;
        vectors[USB_FS_LP_IRQn + 16] = 0x1FFF3095U;
        return N32_BOOTLOADER_SP_V11;
    }

    vectors[SysTick_IRQn + 16]   = 0x1FFF2D31U;
    vectors[USART3_IRQn + 16]    = 0x1FFF2DB1U;
    vectors[USB_FS_LP_IRQn + 16] = 0x1FFF2EB1U;
    return N32_BOOTLOADER_SP_V10;
}

static void n32_bootloader_disable_interrupts(void) {
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (uint32_t i = 0; i < 8; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
}

void bootloader_jump(void) {
    uint32_t        stack_pointer;
    n32_boot_jump_t jump_boot = (n32_boot_jump_t)N32_BOOTLOADER_BOOT_ADDR;

    n32_bootloader_disable_interrupts();

    FLASH_iCacheCmd(DISABLE);
    FLASH_iCacheRST();

    stack_pointer = n32_bootloader_prepare_vector_table();
    RCC_DeInit();

    SCB->VTOR = N32_BOOTLOADER_SRAM_VECTOR_ADDR;
    __set_CONTROL(0);
    __set_MSP(stack_pointer);
    __DSB();
    __ISB();
    __enable_irq();

    jump_boot();
    while (1) {
    }
}

void mcu_reset(void) {
    NVIC_SystemReset();
    while (1) {
    }
}
