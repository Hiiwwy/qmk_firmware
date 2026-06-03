# VIA HS Report Rate Control Proposal

## Status

Implemented and validated in the N32 devboard HS USB path.

## Goal

Allow users to change the keyboard shared HID report rate from VIA while the
firmware is running in HS USB mode.

The VIA control maps to the firmware-side shared HID pacing interval:

```text
0 -> 1000 Hz -> 1000 us
1 -> 2000 Hz ->  500 us
2 -> 4000 Hz ->  250 us
3 -> 8000 Hz ->  125 us
```

This control changes how often the firmware sends shared HID keyboard/NKRO,
mouse, and extra/media reports. It does not change the USB descriptor at
runtime, and it does not require USB re-enumeration.

## Maintenance Boundary

Do not modify any upstream/shared QMK code.

This includes, but is not limited to:

```text
quantum/
tmk_core/
builddefs/
drivers/ shared code
generic keyboard APIs
generic matrix/debounce APIs
generic VIA/raw HID logic
generic USB APIs
```

Implementation scope remains limited to N32-owned code:

```text
keyboards/vendor/n32_devboard/
platforms/n32_freertos/
```

If the feature appears to require changes outside these N32-owned files, the
implementation should stop and the design should be reconsidered instead of
patching QMK.

## Existing Runtime Path

The VIA custom value path already stores and applies a report-rate index:

```text
VIA custom value id_n32_report_rate
    |
    v
n32_config.report_rate_index
    |
    v
n32_report_rate_hz()
    |
    v
n32_usb_set_report_rate_hz()
    |
    v
n32_usb_report_interval_cycles
```

HS mode accepts 1000/2000/4000/8000 Hz.

FS mode is limited to 1000 Hz.

## Interaction With Nonblocking USB Pacing

The HS shared HID sender is nonblocking:

```text
endpoint idle + report slot ready
  -> send immediately

endpoint busy or report slot not ready
  -> cache latest pending report and return

usb_task()
  -> flush pending report when endpoint and slot are ready
```

The VIA report-rate control should reuse this mechanism. It must not reintroduce
busy-waiting.

## Rate Change Semantics

Changing the report rate should make the next shared HID report eligible for
immediate transmission.

After that report starts TX, subsequent shared HID reports are paced by the new
interval.

Desired behavior:

```text
VIA changes report rate
    |
    v
update interval_cycles
    |
    v
force next shared HID slot ready
    |
    v
next shared HID report TX starts
    |
    v
clear force-ready flag and record DWT cycle
    |
    v
future reports follow the new interval
```

If the endpoint is busy when the rate changes, the force-ready state must remain
set until a pending or newly generated shared HID report can actually start TX.

## Raw HID Behavior

VIA Raw HID uses its own raw endpoint and is not paced by the shared HID report
slot.

Changing the report rate must not affect:

```text
VIA connection
VIA remap commands
VIA macro configuration
VIA matrix tester communication
TFT image raw HID protocol
```

## Persistence

The runtime report-rate index is stored in the existing VIA custom config
structure. The firmware applies the saved value during keyboard startup.

Runtime changes apply immediately. Persistence depends on the existing VIA
custom save command path.

## Validation Plan

Build HS firmware and test:

```text
VIA report-rate selector works at 1000/2000/4000/8000 Hz
normal key press/release
VIA remap
VIA macros
VIA multimedia keys
VIA matrix tester
USB remote wakeup
no VIA Raw HID regression
```

Recommended runtime checks:

```text
switch 1k -> 8k and verify the next report is not delayed by the old 1k slot
switch 8k -> 1k and verify later reports follow the new slower interval
verify macros still work at each selected rate
```

## Validation Result

Validated on hardware.

Confirmed behavior:

```text
VIA report-rate changes take effect immediately
RTT logs show the selected firmware-side report rate
normal key press/release works
VIA connection works
VIA remap works
VIA macros work
VIA multimedia keys work
VIA matrix tester works
```

UsbTreeView continues to show:

```text
bInterval: 0x01 (1 microframe -> 0.125 ms)
```

This is expected. The USB descriptor still declares the HS endpoint's maximum
polling capability of one microframe. The VIA setting changes firmware-side
shared HID pacing and does not rewrite descriptors or trigger USB
re-enumeration.
