# Nonblocking USB Report Slot Proposal

## Status

Implemented and validated on the N32 devboard HS USB path.

## Goal

Keep 8000 Hz shared HID report support without busy-waiting in the QMK report
send path.

The 8000 Hz requirement means the firmware must support a 125 us minimum shared
HID report interval in HS mode:

```text
1 second / 8000 reports = 125 us
```

This interval is a USB report pacing limit. It is not a matrix scan interval.
The current DMA matrix scan can run faster than the USB report rate.

## Current Behavior

The current shared HID path uses the DWT cycle counter to wait before sending a
report:

```text
QMK produces report
    |
    v
send_shared_report()
    |
    v
wait until the 125 us slot is reached
    |
    v
USB endpoint TX
```

This keeps the report cadence stable, but the wait happens synchronously in the
caller. If QMK produces a report early, the CPU spins until the next USB report
slot.

## Proposed Behavior

Keep the DWT cycle counter and the 125 us slot check, but make the send path
nonblocking:

```text
QMK produces report
    |
    v
send_shared_report()
    |
    +-- endpoint idle and slot ready -> TX immediately
    |
    +-- endpoint busy or slot not ready -> cache latest pending report and return

usb_task()
    |
    v
pending report + endpoint idle + slot ready -> TX pending report
```

The shared HID report is a current-state snapshot. If several reports are
generated before the next USB slot, only the latest pending snapshot needs to be
sent.

## Implementation Scope

Stay inside N32-owned platform code:

```text
platforms/n32_freertos/usb_driver.c
```

Do not modify QMK core, VIA, raw HID, debounce, matrix, or generic USB APIs.

Initial implementation scope is HS shared HID reports:

```text
keyboard
NKRO
mouse
extra/media
```

VIA Raw HID keeps its independent raw endpoint behavior and is not paced by this
shared HID slot.

## Expected Benefits

```text
CPU usage:
  lower, because the report path no longer spins while waiting for 125 us

USB cadence:
  still limited to the selected report rate, up to 8000 Hz in HS mode

latency:
  latest report can be sent as soon as both the endpoint and slot are ready

QMK maintenance:
  no upstream/shared QMK changes required
```

## Risks

### Short Pulses

If a report changes state and changes back before any USB report slot is sent,
the intermediate state may be collapsed. This is already the natural tradeoff of
latest-state HID report pacing.

QMK key and macro behavior should remain correct as long as generated press and
release states survive at least one report slot.

### Shared Endpoint Arbitration

Keyboard, NKRO, mouse, and extra reports share the same pacing slot. A report
from one type may delay another until the next slot.

This matches the current shared endpoint design and avoids changing QMK-facing
interfaces.

### Pending Report Overwrite

When multiple reports arrive before the next slot, the pending buffer is
overwritten with the latest state. The existing dropped shared report counter
continues to represent these overwritten pending snapshots.

## Validation Plan

Build the default HS firmware and verify:

```text
normal key press/release
VIA connection
VIA remap
VIA macros
VIA multimedia keys
VIA matrix tester
USB HS 8000 Hz mode
USB remote wakeup
```

Runtime checks:

```text
no stuck endpoint after repeated key presses
no macro regression
no VIA raw HID regression
no unexpected shared report drop growth during normal typing
```

## Validation Result

Validated on hardware after replacing the busy-waiting HS shared HID report
slot with the nonblocking pending-report path.

Confirmed working:

```text
normal key press/release
VIA connection
VIA remap
VIA macros
VIA multimedia keys
VIA matrix tester
USB remote wakeup
```
