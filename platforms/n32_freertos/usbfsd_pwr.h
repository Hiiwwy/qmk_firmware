#pragma once

#include <stdbool.h>

#include "usbfsd_core.h"

typedef enum {
    RESUME_EXTERNAL,
    RESUME_INTERNAL,
    RESUME_LATER,
    RESUME_WAIT,
    RESUME_START,
    RESUME_ON,
    RESUME_OFF,
    RESUME_ESOF,
} RESUME_STATE;

enum {
    UNCONNECTED = 0,
    ATTACHED,
    POWERED,
    SUSPENDED,
    ADDRESSED,
    CONFIGURED,
};

extern __IO uint32_t bDeviceState;
extern __IO bool     fSuspendEnabled;

USB_Result PowerOn(void);
USB_Result PowerOff(void);
void       Suspend(void);
void       Resume(RESUME_STATE eResumeSetVal);
