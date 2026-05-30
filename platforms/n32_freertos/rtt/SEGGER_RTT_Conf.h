/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*       Solutions for real time microcontroller applications          *
*           RTT configuration for N32 QMK                             *
*********************************************************************/

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#ifndef SEGGER_RTT_MAX_NUM_UP_BUFFERS
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS     1
#endif

#ifndef SEGGER_RTT_MAX_NUM_DOWN_BUFFERS
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS   1
#endif

// Buffer size for channel 0 up (target -> host, for logging output)
#ifndef BUFFER_SIZE_UP
#define BUFFER_SIZE_UP   4096
#endif

// Buffer size for channel 0 down (host -> target, for receiving)
#ifndef BUFFER_SIZE_DOWN
#define BUFFER_SIZE_DOWN 16
#endif

// Lock mechanism: use PRIMASK to disable all IRQs.
// RTT critical sections are very short so this is safe.
#define SEGGER_RTT_LOCK()   {                                           \
    unsigned int _lockState;                                            \
    __asm volatile ("mrs %0, primask" : "=r" (_lockState));             \
    __asm volatile ("cpsid i" ::: "memory");

#define SEGGER_RTT_UNLOCK()                                             \
    __asm volatile ("msr primask, %0" :: "r" (_lockState) : "memory");  \
}

// Auto-initialize on first access (recommended)
#define SEGGER_RTT_CB_ALIGN(N) (N)

#endif // SEGGER_RTT_CONF_H
