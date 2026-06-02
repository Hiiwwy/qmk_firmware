# Single-Port Hardware Column DMA Proposal

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
  COL0..COL10 all on one GPIO port, for example GPIOX

Not required:
  contiguous pin numbers
  physical pin order matching logical column order
```

Rows may remain distributed across multiple GPIO ports, because the row sample
DMA path already reads GPIOA/B/C/D PID registers.

## Current Design

The current stable DMA matrix path uses DMA for row sampling, but CPU still
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

At 11 us per column and 11 columns:

```text
frame period = 121 us
frame rate   ~= 8264 Hz
column IRQs  ~= 90900 per second
```

## Proposed Design

Precompute a circular DMA table of GPIO PBSC writes:

```text
col_pbsc[0..10]
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

The first implementation should use one of these safe approaches:

```text
Option A: shadow copy
  DMA writes circular sample buffers
  frame-complete IRQ copies 44 words to a shadow buffer
  CPU decodes the shadow buffer

Option B: double buffer
  DMA writes buffer 0 while CPU decodes buffer 1
  buffers swap at frame boundary
```

Option A is simpler for initial validation. Option B is cleaner if the
hardware/DMA model supports it without extra complexity.

## Mode Selection

Keep the current stable path as a fallback.

Suggested build-time modes:

```make
N32_MATRIX_DMA_HW_COLUMNS ?= no
```

Potential future values:

```text
no           current stable CPU column switch path
single_port  new single-port hardware column DMA path
```

The existing stable path should remain available until the new path is proven
with physical hardware.

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

### Phase 2: Row Sample Alignment

Enable row sample DMA, but keep decode instrumentation simple.

Expected observation:

```text
sample[0..10] changes only in the slot corresponding to the active key column
pressing one key does not affect unrelated column slots
```

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

## Expected Benefit

The current stable path generates one column IRQ per column:

```text
8264 frames/s * 11 columns ~= 90900 IRQ/s
```

The proposed path should reduce that to approximately one frame event per
matrix frame:

```text
8264 frame events/s
```

Expected impact:

```text
IRQ count reduction: about 91 percent
CPU headroom gain: estimated 8 to 15 percent
matrix frame rate: initially unchanged at about 8.26 kHz
timing jitter: lower, because column select is timer/DMA-driven
```

The first target is not a higher scan rate. The first target is lower CPU load
and more deterministic column timing at the existing 11 us column period.

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

DMA may begin the next frame before CPU decode finishes. Use shadow copy or
double buffering to prevent mixed-frame reads.

### Hardware Pin Conflicts

The selected column GPIO port must avoid conflicts with USB, SWD/JTAG, crystal
pins, WS2812, TFT/SPI, and row pins.

## Open Questions

1. Which GPIO port will host all 11 column pins?
2. Which exact pins map to COL0..COL10?
3. Should the first implementation use shadow copy or double buffer?
4. Should the experimental mode be named `single_port` instead of overloading
   `N32_MATRIX_DMA_HW_COLUMNS=yes`?
5. Should diagnostics be exposed through RTT only, or also through VIA custom
   values?

## Proposed First Implementation Scope

Do not replace the current stable path immediately.

First implementation should add an experimental single-port hardware column
DMA mode with:

```text
single GPIO port column DMA table
ATIM2 UPDATE-triggered GPIOX PBSC DMA
existing ATIM2 CC-triggered row sample DMA
frame shadow copy or double buffer
RTT diagnostics for column POD and DMA counters
build-time fallback to current CPU column switching
```

After the diagnostic mode proves correct on hardware, connect it to the normal
matrix decode/filter path.

Implementation scope must remain limited to the N32-owned keyboard/platform
files. Any design that requires changing QMK core should be rejected or
redesigned before implementation.
