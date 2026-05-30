/*********************************************************************
 *                    SEGGER Microcontroller GmbH                     *
 *       Solutions for real time microcontroller applications          *
 *           Minimal SEGGER RTT implementation for N32 QMK            *
 *********************************************************************/

#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#include <stdint.h>

#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE
#    define SEGGER_RTT_PRINTF_BUFFER_SIZE 64u
#endif

// Mode flags for up-channel
#define SEGGER_RTT_MODE_NO_BLOCK_SKIP 0
#define SEGGER_RTT_MODE_NO_BLOCK_TRIM 1
#define SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL 2

// Control block structures
typedef struct {
    const char       *sName;
    char             *pBuffer;
    unsigned          SizeOfBuffer;
    unsigned          WrOff;
    volatile unsigned RdOff;
    unsigned          Flags;
} SEGGER_RTT_BUFFER_UP;

typedef struct {
    const char       *sName;
    char             *pBuffer;
    unsigned          SizeOfBuffer;
    volatile unsigned WrOff;
    unsigned          RdOff;
    unsigned          Flags;
} SEGGER_RTT_BUFFER_DOWN;

typedef struct {
    char                   acID[16];
    int                    MaxNumUpBuffers;
    int                    MaxNumDownBuffers;
    SEGGER_RTT_BUFFER_UP   aUp[1];
    SEGGER_RTT_BUFFER_DOWN aDown[1];
} SEGGER_RTT_CB;

extern SEGGER_RTT_CB _SEGGER_RTT;
extern SEGGER_RTT_CB SEGGER_RTT;
extern SEGGER_RTT_CB _SEGGER_RTT_CB;

// API
void     SEGGER_RTT_Init(void);
int      SEGGER_RTT_HasData(unsigned BufferIndex);
int      SEGGER_RTT_HasKey(void);
int      SEGGER_RTT_GetKey(void);
unsigned SEGGER_RTT_Read(unsigned BufferIndex, void *pData, unsigned BufferSize);
int      SEGGER_RTT_PutChar(unsigned BufferIndex, char c);
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void *pData, unsigned NumBytes);
unsigned SEGGER_RTT_WriteNoLock(unsigned BufferIndex, const void *pData, unsigned NumBytes);
unsigned SEGGER_RTT_WriteSkipNoLock(unsigned BufferIndex, const void *pData, unsigned NumBytes);

#endif // SEGGER_RTT_H
