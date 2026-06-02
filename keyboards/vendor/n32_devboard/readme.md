# N32 Devboard

Minimal QMK target for validating the private N32H487 + FreeRTOS platform port.

Build example:

```sh
make vendor/n32_devboard:default -j16 USE_CCACHE=yes
```

USB speed can be selected at build time:

```sh
make vendor/n32_devboard:default N32_USB_SPEED=hs
make vendor/n32_devboard:default N32_USB_SPEED=fs
qmk compile -kb vendor/n32_devboard -km default -e N32_USB_SPEED=fs
```

The default is `fs` for boards that only expose the USBFSD peripheral and limits the report rate to 1000 Hz. `hs` selects the USBHS backend and uses an 8k report-rate default for boards wired for high-speed USB.

The N32 devboard HSE is configured as 8 MHz by default through `N32_HSE_VALUE=8000000U`. Override it at build time if your board uses a different crystal:

```sh
qmk compile -kb vendor/n32_devboard -km default -e N32_HSE_VALUE=16000000U
```

## Matrix Scan Backend

This keyboard keeps matrix changes local to the N32 devboard target so QMK common code can still be updated normally.

Backend selection:

```sh
make vendor/n32_devboard:default -j16 USE_CCACHE=yes N32_MATRIX_DRIVER=dma_timer
make vendor/n32_devboard:default -j16 USE_CCACHE=yes N32_MATRIX_DRIVER=gpio
```

The validated default is `N32_MATRIX_DRIVER=dma_timer`. It uses ATIM2 as a fixed-cadence column sequencer and DMA2_CH1-4 to sample GPIO row ports:

```text
Rows: B0, B2, D4, A6, C4
Cols: C8, A0, C3, C1, A1, C0, C2, A3, A5, A7, C5
Diode: ROW2COL
Timer: ATIM2
DMA: DMA2_CH1-4 for GPIOA/B/C/D PID sampling
Stable timing: 11 us per column, about 8.26 kHz full matrix frame cadence
Default debounce: press 2 frames, release 6 frames
```

The stable column path is CPU-driven at the ATIM2 update interrupt:

```make
N32_MATRIX_DMA_HW_COLUMNS ?= no
```

An experimental hardware-column DMA mode exists:

```sh
make vendor/n32_devboard:default -j16 USE_CCACHE=yes N32_MATRIX_DMA_HW_COLUMNS=yes
```

On the current board this mode is not validated. RTT logs showed that the column DMA sequence did not advance beyond the C8 column, so key presses decoded as a whole row. Keep `N32_MATRIX_DMA_HW_COLUMNS=no` for normal firmware builds.

Useful RTT fields:

```text
cu  column period in microseconds
dc  CPU dead-cycle delay used by the stable column-switch path
sp  timer compare sample percentage
ph  sample phase
pf  DMA press confirm frames
rf  DMA release confirm frames
hc  hardware-column DMA mode, 0 means stable CPU column switch, 1 means experimental DMA column switch
```
