#include "usbfsd_istr.h"

#include "n32_usbfsd_keyboard.h"
#include "usbfsd_lib.h"
#include "usbfsd_pwr.h"

__IO uint16_t wIstr;
__IO uint8_t  bIntPackSOF;
__IO uint32_t esof_counter;

extern n32_usbfsd_debug_counters_t n32_usbfsd_debug_counters;

void (*pEpInt_IN[7])(void) = {
    EP1_IN_Callback, EP2_IN_Callback, EP3_IN_Callback, EP4_IN_Callback, EP5_IN_Callback, EP6_IN_Callback, EP7_IN_Callback,
};

void (*pEpInt_OUT[7])(void) = {
    EP1_OUT_Callback, EP2_OUT_Callback, EP3_OUT_Callback, EP4_OUT_Callback, EP5_OUT_Callback, EP6_OUT_Callback, EP7_OUT_Callback,
};

void USB_Istr(void) {
#ifdef N32_USBFS_ESOF_FORCE_RESET
    __IO uint32_t ep[8];
#endif

    wIstr = _GetISTR();

    if (wIstr & STS_CTRS & wInterrupt_Mask) {
        n32_usbfsd_debug_counters.ctr_count++;
        USB_CorrectTransferLp();
    }

    if (wIstr & STS_RST & wInterrupt_Mask) {
        n32_usbfsd_debug_counters.reset_count++;
        _SetISTR((uint16_t)CLR_RST);
        Device_Property.Init();
        Device_Property.Reset();
    }

    if (wIstr & STS_ERROR & wInterrupt_Mask) {
        n32_usbfsd_debug_counters.error_count++;
        _SetISTR((uint16_t)CLR_ERROR);
    }

    if (wIstr & STS_WKUP & wInterrupt_Mask) {
        n32_usbfsd_debug_counters.wakeup_count++;
        _SetISTR((uint16_t)CLR_WKUP);
        Resume(RESUME_EXTERNAL);
    }

    if (wIstr & STS_SUSPD & wInterrupt_Mask) {
        n32_usbfsd_debug_counters.suspend_count++;
        if (fSuspendEnabled) {
            Suspend();
        } else {
            Resume(RESUME_LATER);
        }
        _SetISTR((uint16_t)CLR_SUSPD);
    }

    if (wIstr & STS_SOF & wInterrupt_Mask) {
        n32_usbfsd_debug_counters.sof_count++;
        _SetISTR((uint16_t)CLR_SOF);
        bIntPackSOF++;
    }

    if (wIstr & STS_ESOF & wInterrupt_Mask) {
#ifdef N32_USBFS_ESOF_FORCE_RESET
        uint32_t i;

        n32_usbfsd_debug_counters.esof_count++;
        _SetISTR((uint16_t)CLR_ESOF);
        if ((_GetFNR() & FN_RXDP) != 0) {
            esof_counter++;

            if (esof_counter > 3 && (_GetCNTR() & CTRL_FSUSPD) == 0) {
                uint16_t cntr = _GetCNTR();

                for (i = 0; i < 8; i++) {
                    ep[i] = _GetENDPOINT(i);
                }

                cntr |= CTRL_FRST;
                _SetCNTR(cntr);
                cntr &= (uint16_t)~CTRL_FRST;
                _SetCNTR(cntr);

                while ((_GetISTR() & STS_RST) == 0) {
                }

                _SetISTR((uint16_t)CLR_RST);
                for (i = 0; i < 8; i++) {
                    _SetENDPOINT(i, ep[i]);
                }

                n32_usbfsd_debug_counters.esof_forced_reset_count++;
                esof_counter = 0;
            }
        } else {
            esof_counter = 0;
        }
#else
        n32_usbfsd_debug_counters.esof_count++;
        _SetISTR((uint16_t)CLR_ESOF);
#endif
        Resume(RESUME_ESOF);
    }
}
