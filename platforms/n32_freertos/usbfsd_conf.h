#pragma once

#include "usbfsd_regs.h"

#if defined(RAW_ENABLE) && !defined(N32_USBFS_SINGLE_INTERFACE)
#    define EP_NUM 4
#else
#    define EP_NUM 2
#endif

#define BTABLE_ADDRESS 0x00

#define ENDP0_RXADDR 0x40
#define ENDP0_TXADDR 0x80
#define ENDP1_TXADDR 0xC0
#define ENDP1_RXADDR 0x100
#define ENDP2_TXADDR 0x140
#define ENDP2_RXADDR 0x180
#define ENDP3_RXADDR 0x1C0

#define IMR_MSK (CTRL_CTRSM | CTRL_WKUPM | CTRL_SUSPDM | CTRL_ERRORM | CTRL_SOFM | CTRL_ESOFM | CTRL_RSTM)

#define EP3_IN_Callback USB_ProcessNop
#define EP4_IN_Callback USB_ProcessNop
#define EP5_IN_Callback USB_ProcessNop
#define EP6_IN_Callback USB_ProcessNop
#define EP7_IN_Callback USB_ProcessNop

#if !defined(RAW_ENABLE) || defined(N32_USBFS_SINGLE_INTERFACE)
#    define EP3_OUT_Callback USB_ProcessNop
#endif
#define EP4_OUT_Callback USB_ProcessNop
#define EP5_OUT_Callback USB_ProcessNop
#define EP6_OUT_Callback USB_ProcessNop
#define EP7_OUT_Callback USB_ProcessNop

#if !defined(RAW_ENABLE) || defined(N32_USBFS_SINGLE_INTERFACE)
#    define EP2_IN_Callback USB_ProcessNop
#else
void EP3_OUT_Callback(void);
#endif
#define EP2_OUT_Callback USB_ProcessNop
