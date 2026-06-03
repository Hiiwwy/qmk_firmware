# Single-Port Hardware Column DMA Proposal

## Status

Implemented and validated on the N32 devboard test branch.

Current validated matrix:

```text
Rows: B0, B2, D4, A6, C4
Cols: C6, C7, C8, C11, C10, C12
```

Current default firmware mode:

```make
N32_MATRIX_DRIVER ?= dma_timer
N32_MATRIX_DMA_HW_COLUMNS ?= single_port
N32_MATRIX_DMA_COLUMN_US ?= 9U
N32_MATRIX_DMA_PRESS_CONFIRM_FRAMES ?= 2U
N32_MATRIX_DMA_RELEASE_CONFIRM_FRAMES ?= 6U
```

Validated features:

```text
normal key press/release
VIA connection
VIA remap
VIA macros
VIA multimedia keys
VIA matrix tester
USB HS 8 kHz report mode
USB remote wakeup
```

## Goal

Move matrix column switching from the per-column CPU timer interrupt into a
timer-triggered DMA sequence.

The intended runtime model is:

```text
Per column:
  ATIM2 UPDATE -> DMA writes GPIOx->PBSC to select the next column
  ATIM2 CC1..4 -> DMA samples GPIOA/B/C/D row inputs

Per frame:
  CPU decodes one completed sample frame into QMK matrix rows
```

This should reduce CPU interrupt load, make column timing more deterministic,
and keep the existing matrix frame rate near the current validated value.

## Maintenance Constraint

Do not modify upstream/shared QMK code.

This proposal must not require changes under common QMK areas such as:

```text
quantum/
tmk_core/
builddefs/
drivers/ shared code
generic keyboard APIs
generic matrix/debounce APIs
```

The implementation should stay inside N32-owned code only:

```text
keyboards/vendor/n32_devboard/
platforms/n32_freertos/
```

If a future QMK update replaces upstream files, this feature should remain
isolated to the N32 keyboard/platform layer and be easy to rebase or carry as an
overlay.

The feature should use existing QMK extension points:

```text
custom matrix driver selection
keyboard rules.mk options
keyboard config.h pin definitions
platform-specific timer/DMA code
```

Do not introduce behavior that requires patching QMK core matrix scanning,
raw HID, VIA, or build system internals.

## Hardware Requirement

All matrix column pins must be moved to the same GPIO port.

The pins do not need to be contiguous or ordered by bit number.

```text
Required:
  all logical columns on one GPIO port, for example GPIOX

Not required:
  contiguous pin numbers
  physical pin order matching logical column order
```

Rows may remain distributed across multiple GPIO ports, because the row sample
DMA path already reads GPIOA/B/C/D PID registers.

## Previous Design

The previous stable DMA matrix path used DMA for row sampling, but CPU still
switches columns on every ATIM2 update interrupt.

```text
ATIM2 CC1..CC4
    -> DMA reads GPIOA/B/C/D PID

ATIM2 UPDATE IRQ
    -> CPU stores active column sample
    -> CPU advances active_col
    -> CPU releases all columns
    -> CPU waits dead cycles
    -> CPU drives next column low
    -> CPU decodes frame on wrap
```

At 11 us per column and 6 columns:

```text
frame period = 66 us
frame rate   ~= 15151 Hz
column IRQs  ~= 90909 per second
```

## Implemented Design

Precompute a circular DMA table of GPIO PBSC writes:

```text
col_pbsc[0..5]
```

Each entry performs the complete column transition for one logical column:

```text
all_cols_mask = OR of all column pin bits on GPIOX
selected      = bit for the current logical column

col_pbsc[i] =
    (all_cols_mask & ~selected)
  | (selected << 16)
```

PBSC low 16 bits set pins high. PBSC high 16 bits reset pins low.
The selected bit must not be present in the low 16-bit set field.

```text
Example, selected column = PX4

low 16 bits:
  set all column pins except PX4

high 16 bits:
  reset PX4
```

The hardware timeline becomes:

```text
          ATIM2 period: 11 us
┌─────────────────────────────────────────────┐
│ t=0 UPDATE                                  │
│   DMA_CH_COL writes GPIOX->PBSC             │
│   selected column advances                  │
│                                             │
│ t=sample compare                            │
│   DMA_CH1 reads GPIOA->PID                  │
│   DMA_CH2 reads GPIOB->PID                  │
│   DMA_CH3 reads GPIOC->PID                  │
│   DMA_CH4 reads GPIOD->PID                  │
└─────────────────────────────────────────────┘
```

Expected steady-state flow:

```text
ATIM2 UPDATE
    |
    v
DMA write GPIOX->PBSC[col]
    |
    v
column selected by hardware
    |
    v
ATIM2 CC sample events
    |
    v
DMA writes sample_gpioa/b/c/d[col]
    |
    v
frame complete
    |
    v
CPU copies or flips completed frame
    |
    v
CPU decodes matrix rows
```

## Buffer Ownership

CPU must not decode a buffer while DMA is overwriting it.

The current implementation uses a shadow copy at the frame boundary:

```text
DMA writes circular sample buffers
DMA frame complete interrupt copies the completed frame
CPU decodes the copied frame
```

This keeps decode from racing with DMA writes while staying simple enough for
initial hardware validation.

## Mode Selection

Keep the previous stable path as a fallback.

Build-time modes:

```make
N32_MATRIX_DMA_HW_COLUMNS ?= single_port
```

Supported values:

```text
no           current stable CPU column switch path
single_port  new single-port hardware column DMA path
```

The CPU column switch path remains available for rollback.

## Validation Plan

### Phase 1: Column DMA Diagnostic

Enable only the column DMA path.

Expected observation:

```text
GPIOX POD shows exactly one column bit low at a time
the low bit advances through all 11 logical columns
the pattern repeats at the configured column period
```

Use RTT, a logic analyzer, or a scope.

Status: completed.

### Phase 2: Row Sample Alignment

Enable row sample DMA, but keep decode instrumentation simple.

Expected observation:

```text
sample[0..10] changes only in the slot corresponding to the active key column
pressing one key does not affect unrelated column slots
```

Status: completed.

### Phase 3: Frame Decode

Enable CPU frame decode/filter on the completed sample frame.

Validate:

```text
single key press
multi-key press
hold and release
fast repeated taps
VIA matrix tester
VIA remap, macro, and media keys
```

Status: completed.

### Phase 4: Performance Comparison

Compare the current stable path and the new single-port hardware path.

Measure:

```text
matrix scan rate
PERF matrix avg/max
PERF loop avg/max
USB/VIA stability
RGB behavior
missed keys
ghosting or stuck keys
```

Status: completed for the current 5x6 validation matrix.

Next test steps:

```text
11 us/col: validated baseline
10 us/col: validated
9 us/col: validated production candidate
8 us/col: single-key passed, multi-key crosstalk risk observed
7 us/col: failed, key chatter/crosstalk observed
6 us/col: failed, key chatter/crosstalk observed
```

Additional multi-key validation:

```text
8 us/col, press=2, release=6:
  single-key testing passed
  multi-key testing showed key chatter/crosstalk risk

8 us/col, press=3, release=7:
  multi-key crosstalk still observed
  increasing confirmation frames did not resolve the raw sampling issue

9 us/col, press=2, release=6:
  simultaneous 5-column press passed on the current 5x6 test matrix

9 us/col, press=1, release=4:
  simultaneous 5-column press passed on the current 5x6 test matrix
```

Current validation limit:

```text
The current test keyboard has only 6 columns.
The multi-key stress result can confirm simultaneous 5-column operation only.
It should not be treated as proof for larger column counts without testing on
the target matrix.
```

## Expected Benefit

The previous stable path generated one column IRQ per column:

```text
15151 frames/s * 6 columns ~= 90909 IRQ/s
```

The single-port DMA path reduces that to approximately one frame event per
matrix frame:

```text
15151 frame events/s
```

Expected impact:

```text
IRQ count reduction: about 83 percent
CPU headroom gain: validated by lower matrix CPU time
matrix frame rate: about 18.52 kHz at 9 us/col
timing jitter: lower, because column select is timer/DMA-driven
```

The first target was lower CPU load and more deterministic column timing at the
validated column period. The validated production candidate is 9 us/col for
the current 5x6 validation matrix.

## Risks

### DMA Request Behavior

The single-port design avoids the previous dual-port risk where one timer
update request had to trigger two GPIO DMA channels.

Remaining risk:

```text
ATIM2 UPDATE -> one DMA channel -> GPIOX PBSC
```

This is much simpler, but still needs physical validation.

### PBSC Word Construction

PBSC entries must never set and reset the same bit in one write.

Incorrect:

```text
set selected bit
reset selected bit
```

Correct:

```text
set all non-selected column bits
reset selected column bit
```

### Frame Consistency

DMA may begin the next frame before CPU decode finishes. The current shadow copy
keeps decode on a completed frame.

### Hardware Pin Conflicts

The selected column GPIO port must avoid conflicts with USB, SWD/JTAG, crystal
pins, WS2812, TFT/SPI, and row pins.

## Open Questions

1. Should the validated production default remain 9 us on larger target
   matrices, or should larger column counts use a more conservative period?
2. Should performance diagnostics stay build-time only, or be exposed through
   VIA custom values?

## Proposed First Implementation Scope

Do not replace the current stable path immediately.

The implemented first scope added single-port hardware column DMA mode with:

```text
single GPIO port column DMA table
ATIM2 UPDATE-triggered GPIOX PBSC DMA
existing ATIM2 CC-triggered row sample DMA
frame shadow copy or double buffer
RTT diagnostics for column POD and DMA counters
build-time fallback to current CPU column switching
```

The diagnostic mode has been connected to the normal matrix decode/filter path.

Implementation scope remains limited to the N32-owned keyboard/platform files.
Any future design that requires changing QMK core should be rejected or
redesigned before implementation.
