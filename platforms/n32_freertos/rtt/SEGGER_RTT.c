/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*       Solutions for real time microcontroller applications          *
*           Minimal SEGGER RTT implementation for N32 QMK            *
*********************************************************************/

#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"
#include <string.h>

// RTT control block placed in RAM. Keep the SEGGER symbol name visible so
// debuggers can find it by symbol as well as by scanning for the ID string.
SEGGER_RTT_CB _SEGGER_RTT __attribute__((used, aligned(4), section(".rtt")));
extern SEGGER_RTT_CB SEGGER_RTT __attribute__((alias("_SEGGER_RTT")));
extern SEGGER_RTT_CB _SEGGER_RTT_CB __attribute__((alias("_SEGGER_RTT")));

// TX buffer for channel 0
static char _aUp[BUFFER_SIZE_UP] __attribute__((used, aligned(4)));

// RX buffer for channel 0
static char _aDown[BUFFER_SIZE_DOWN] __attribute__((used, aligned(4)));

// ID string that J-Link searches for in RAM
static const char _aTerminalId[16] = "SEGGER RTT\0\0\0\0\0\0";

static uint8_t _SEGGER_RTT_Initialized;

/*********************************************************************
*       SEGGER_RTT_Init()
*
*  Function description
*    Initializes the RTT control block.
*    Called automatically on first access if SEGGER_RTT_CB_ALIGN is set.
*/
void SEGGER_RTT_Init(void) {
    if (_SEGGER_RTT_Initialized) {
        return;
    }

    memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));
    memcpy(_SEGGER_RTT.acID, _aTerminalId, 16);

    _SEGGER_RTT.MaxNumUpBuffers   = 1;
    _SEGGER_RTT.MaxNumDownBuffers = 1;

    // Channel 0 up (target -> host)
    _SEGGER_RTT.aUp[0].sName        = "Terminal";
    _SEGGER_RTT.aUp[0].pBuffer      = _aUp;
    _SEGGER_RTT.aUp[0].SizeOfBuffer = sizeof(_aUp);
    _SEGGER_RTT.aUp[0].WrOff        = 0;
    _SEGGER_RTT.aUp[0].RdOff        = 0;
    _SEGGER_RTT.aUp[0].Flags        = SEGGER_RTT_MODE_NO_BLOCK_SKIP;

    // Channel 0 down (host -> target)
    _SEGGER_RTT.aDown[0].sName        = "Terminal";
    _SEGGER_RTT.aDown[0].pBuffer      = _aDown;
    _SEGGER_RTT.aDown[0].SizeOfBuffer = sizeof(_aDown);
    _SEGGER_RTT.aDown[0].WrOff        = 0;
    _SEGGER_RTT.aDown[0].RdOff        = 0;
    _SEGGER_RTT.aDown[0].Flags        = SEGGER_RTT_MODE_NO_BLOCK_SKIP;

    _SEGGER_RTT_Initialized = 1;
}

/*********************************************************************
*       SEGGER_RTT_PutChar()
*
*  Function description
*    Writes a single character to an RTT up-channel.
*/
int SEGGER_RTT_PutChar(unsigned BufferIndex, char c) {
    SEGGER_RTT_BUFFER_UP *pRing;
    unsigned WrOff;
    unsigned RdOff;
    int      r;

    SEGGER_RTT_Init();
    SEGGER_RTT_LOCK();
    if (BufferIndex < (unsigned)_SEGGER_RTT.MaxNumUpBuffers) {
        pRing = &_SEGGER_RTT.aUp[BufferIndex];
        WrOff = pRing->WrOff + 1;
        if (WrOff == pRing->SizeOfBuffer) {
            WrOff = 0;
        }
        RdOff = pRing->RdOff;
        if (WrOff != RdOff) {
            pRing->pBuffer[pRing->WrOff] = c;
            pRing->WrOff = WrOff;
            r = 0;
        } else {
            r = -1; // Buffer full
        }
    } else {
        r = -1;
    }
    SEGGER_RTT_UNLOCK();
    return r;
}

/*********************************************************************
*       SEGGER_RTT_WriteNoLock()
*
*  Function description
*    Writes data to an RTT up-channel without locking.
*    Caller must ensure exclusive access.
*/
unsigned SEGGER_RTT_WriteNoLock(unsigned BufferIndex, const void *pData, unsigned NumBytes) {
    SEGGER_RTT_BUFFER_UP *pRing;
    const char *pSrc;
    unsigned WrOff;
    unsigned RdOff;
    unsigned NumWritten;
    unsigned NumToCopy;

    SEGGER_RTT_Init();
    if (BufferIndex < (unsigned)_SEGGER_RTT.MaxNumUpBuffers) {
        pRing  = &_SEGGER_RTT.aUp[BufferIndex];
        pSrc   = (const char *)pData;
        WrOff  = pRing->WrOff;
        RdOff  = pRing->RdOff;

        // Phase 1: copy data from WrOff to end of buffer (or RdOff, whichever comes first)
        if (RdOff > WrOff) {
            NumToCopy = RdOff - WrOff - 1u;
        } else {
            NumToCopy = pRing->SizeOfBuffer - WrOff;
            if (RdOff == 0) {
                NumToCopy--;
            }
        }
        if (NumToCopy > NumBytes) {
            NumToCopy = NumBytes;
        }
        if (NumToCopy > 0) {
            memcpy(pRing->pBuffer + WrOff, pSrc, NumToCopy);
            NumWritten = NumToCopy;
            WrOff     += NumToCopy;
            pSrc      += NumToCopy;
            NumBytes  -= NumToCopy;
            if (WrOff >= pRing->SizeOfBuffer) {
                WrOff = 0;
            }
        } else {
            NumWritten = 0;
        }

        // Phase 2: if we wrapped and there's still data, copy from 0 toward RdOff
        if (NumBytes > 0 && RdOff > 0) {
            NumToCopy = RdOff - WrOff - 1u;
            if (NumToCopy > NumBytes) {
                NumToCopy = NumBytes;
            }
            if (NumToCopy > 0) {
                memcpy(pRing->pBuffer + WrOff, pSrc, NumToCopy);
                NumWritten += NumToCopy;
                WrOff      += NumToCopy;
            }
        }

        pRing->WrOff = WrOff;
        return NumWritten;
    }
    return 0u;
}

/*********************************************************************
*       SEGGER_RTT_WriteSkipNoLock()
*
*  Function description
*    Writes data to an RTT up-channel without locking.
*    Skips data that would overflow the buffer (non-blocking).
*/
unsigned SEGGER_RTT_WriteSkipNoLock(unsigned BufferIndex, const void *pData, unsigned NumBytes) {
    return SEGGER_RTT_WriteNoLock(BufferIndex, pData, NumBytes);
}

/*********************************************************************
*       SEGGER_RTT_Write()
*
*  Function description
*    Writes data to an RTT up-channel with locking.
*/
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void *pData, unsigned NumBytes) {
    unsigned NumWritten;
    SEGGER_RTT_LOCK();
    NumWritten = SEGGER_RTT_WriteNoLock(BufferIndex, pData, NumBytes);
    SEGGER_RTT_UNLOCK();
    return NumWritten;
}

/*********************************************************************
*       SEGGER_RTT_Read()
*
*  Function description
*    Reads data from an RTT down-channel (host -> target).
*/
unsigned SEGGER_RTT_Read(unsigned BufferIndex, void *pData, unsigned BufferSize) {
    SEGGER_RTT_BUFFER_DOWN *pRing;
    char *pDst;
    unsigned WrOff;
    unsigned RdOff;
    unsigned NumRead;
    unsigned NumToCopy;

    SEGGER_RTT_Init();
    SEGGER_RTT_LOCK();
    if (BufferIndex < (unsigned)_SEGGER_RTT.MaxNumDownBuffers) {
        pRing = &_SEGGER_RTT.aDown[BufferIndex];
        pDst  = (char *)pData;
        WrOff = pRing->WrOff;
        RdOff = pRing->RdOff;
        NumRead = 0;

        while (BufferSize > 0 && RdOff != WrOff) {
            if (RdOff < WrOff) {
                NumToCopy = WrOff - RdOff;
            } else {
                NumToCopy = pRing->SizeOfBuffer - RdOff;
            }
            if (NumToCopy > BufferSize) {
                NumToCopy = BufferSize;
            }
            memcpy(pDst, pRing->pBuffer + RdOff, NumToCopy);
            RdOff     += NumToCopy;
            pDst      += NumToCopy;
            BufferSize -= NumToCopy;
            NumRead    += NumToCopy;
            if (RdOff >= pRing->SizeOfBuffer) {
                RdOff = 0;
            }
        }
        pRing->RdOff = RdOff;
    } else {
        NumRead = 0;
    }
    SEGGER_RTT_UNLOCK();
    return NumRead;
}

/*********************************************************************
*       SEGGER_RTT_HasData()
*
*  Function description
*    Checks if data is available in a down-channel.
*/
int SEGGER_RTT_HasData(unsigned BufferIndex) {
    SEGGER_RTT_BUFFER_DOWN *pRing;
    SEGGER_RTT_Init();
    if (BufferIndex < (unsigned)_SEGGER_RTT.MaxNumDownBuffers) {
        pRing = &_SEGGER_RTT.aDown[BufferIndex];
        return (pRing->WrOff != pRing->RdOff);
    }
    return 0;
}

/*********************************************************************
*       SEGGER_RTT_HasKey()
*
*  Function description
*    Checks if a key is available from the host.
*/
int SEGGER_RTT_HasKey(void) {
    return SEGGER_RTT_HasData(0);
}

/*********************************************************************
*       SEGGER_RTT_GetKey()
*
*  Function description
*    Reads one character from the host.
*/
int SEGGER_RTT_GetKey(void) {
    char c;
    if (SEGGER_RTT_Read(0, &c, 1) == 1) {
        return (int)(unsigned char)c;
    }
    return -1;
}
