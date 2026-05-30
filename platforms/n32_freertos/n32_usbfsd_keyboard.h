#pragma once

#include <stdbool.h>
#include <stdint.h>

#define N32_USBFSD_SHARED_IN_EP 0x81
#define N32_USBFSD_SHARED_OUT_EP 0x01
#define N32_USBFSD_RAW_IN_EP 0x82
#define N32_USBFSD_RAW_OUT_EP 0x03
#define N32_USBFSD_EP0_SIZE 64U
#define N32_USBFSD_PACKET_SIZE 32U
#define N32_USBFSD_POLLING_INTERVAL 1U

typedef struct {
    uint32_t reset_count;
    uint32_t suspend_count;
    uint32_t wakeup_count;
    uint32_t error_count;
    uint32_t ctr_count;
    uint32_t sof_count;
    uint32_t esof_count;
    uint32_t esof_forced_reset_count;
    uint32_t set_address_count;
    uint32_t set_configuration_count;
    uint32_t device_desc_count;
    uint32_t config_desc_count;
    uint32_t string_desc_count;
    uint32_t hid_desc_count;
    uint32_t report_desc_count;
    uint32_t class_data_count;
    uint32_t class_nodata_count;
    uint32_t unsupported_count;
    uint8_t  last_request;
    uint8_t  last_type;
    uint16_t last_value;
    uint16_t last_index;
    uint16_t last_length;
    uint16_t cntr;
    uint16_t istr;
    uint16_t fnr;
    uint16_t daddr;
    uint16_t ep0r;
    uint16_t ep1r;
    uint16_t interrupt_mask;
    uint32_t rcc_cfg3;
    uint32_t rcc_apb1pclken;
    uint32_t rcc_ahb1pclken;
} n32_usbfsd_debug_counters_t;

void n32_usbfsd_debug_snapshot(n32_usbfsd_debug_counters_t *snapshot);

void n32_usbfsd_init(void);
bool n32_usbfsd_send_shared(const void *report, uint8_t length);
#ifdef RAW_ENABLE
void n32_usbfsd_raw_hid_task(void);
bool n32_usbfsd_send_raw_hid(const uint8_t *data, uint8_t length);
#endif
