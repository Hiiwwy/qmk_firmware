#pragma once

#include "n32h47x_48x.h"

#define RX_FIFO_HS_SIZE 128
#define TX0_FIFO_HS_SIZE 64
#define TX1_FIFO_HS_SIZE 64
#define TX2_FIFO_HS_SIZE 64
#define TX3_FIFO_HS_SIZE 0
#define TX4_FIFO_HS_SIZE 0
#define TX5_FIFO_HS_SIZE 0
#define TX6_FIFO_HS_SIZE 0
#define TX7_FIFO_HS_SIZE 0
#define TX8_FIFO_HS_SIZE 0

#define USB_INTERNAL_DMA_ENABLED
#ifndef USE_DEVICE_MODE
#    define USE_DEVICE_MODE
#endif

#if ((RX_FIFO_HS_SIZE + TX0_FIFO_HS_SIZE + TX1_FIFO_HS_SIZE + TX2_FIFO_HS_SIZE + TX3_FIFO_HS_SIZE + TX4_FIFO_HS_SIZE + TX5_FIFO_HS_SIZE + TX6_FIFO_HS_SIZE + TX7_FIFO_HS_SIZE + TX8_FIFO_HS_SIZE) > 1024U)
#    error "The USB max FIFO size is 1024 x 4 Bytes!"
#endif

#ifdef USB_INTERNAL_DMA_ENABLED
#    if defined(__GNUC__)
#        define __ALIGN_END __attribute__((aligned(4)))
#        define __ALIGN_BEGIN
#    else
#        define __ALIGN_END
#        define __ALIGN_BEGIN
#    endif
#else
#    define __ALIGN_BEGIN
#    define __ALIGN_END
#endif

#if defined(__GNUC__)
#    define __packed __attribute__((__packed__))
#endif
