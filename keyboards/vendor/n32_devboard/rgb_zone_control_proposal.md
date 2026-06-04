# RGB Zone Control Proposal

## Status

Implemented and hardware validated.

## Goal

Add N32-owned RGB zone control for three logical zones:

```text
Axis       key switch LEDs
Underglow  bottom LEDs
Indicator  logical status override zone
```

Users should be able to control the three zones from VIA without modifying QMK
core. The first version keeps a single shared RGB effect control surface, while
Axis, Underglow, and Indicator expose simple enable switches.

## Maintenance Boundary

Do not modify any upstream/shared QMK code.

This includes:

```text
quantum/
tmk_core/
builddefs/
drivers/ shared code
generic RGB Matrix code
generic VIA/raw HID code
generic keyboard APIs
```

Implementation scope is limited to N32-owned files:

```text
keyboards/vendor/n32_devboard/
platforms/n32_freertos/
```

## First Scope

The first implementation intentionally does not create three independent full
QMK RGB Matrix engines.

Instead, QMK RGB Matrix remains the base effect engine, and the N32 keyboard
layer applies zone-specific post-processing.

```text
QMK RGB Matrix base effect
    |
    v
N32 zone post-processing
    |
    +-- Axis: enabled follows main effect, disabled forced off
    +-- Underglow: enabled follows main effect, disabled forced off
    +-- Indicator: enabled allows logical status override
```

## Current LED Mapping

The current first firmware mapping assumes ten LEDs:

```text
LED 0..5  Axis
LED 6..9  Underglow
```

The indicator zone is logical. It may target existing axis/underglow LEDs or a
future dedicated indicator LED.

For the current board, the default indicator target is LED 1.

## VIA Behavior

VIA presents the RGB controls by behavior, not by pretending each zone has its
own full effect engine.

The `RGB Main Effect` group exposes the shared effect control surface:

```text
Mode
Hue
Saturation
Brightness
Speed
```

These controls configure the shared RGB Matrix effect. Enabled Axis and
Underglow LEDs follow this same base effect.

The `RGB Zones` group exposes only zone enable switches:

```text
Axis
Underglow
Indicator
```

When Axis is enabled, key switch LEDs show the main effect. When disabled, N32
post-processing forces only the key switch LEDs off.

When Underglow is enabled, bottom LEDs follow the main effect. When disabled,
N32 post-processing forces the bottom LEDs off.

Indicator exposes only an enable switch. When enabled and a keyboard status
override is active, the indicator target LED is forced to the N32 indicator
color. When disabled, no indicator override is applied and the LED falls back
to its normal physical zone behavior.

## Persistence

N32 stores zone settings in the keyboard's VIA custom config area.

The config layout is N32-owned and should be sanitized using a versioned magic
value so older EEPROM contents fall back to defaults.

## Validation Plan

Build HS firmware and validate:

```text
VIA shows RGB Main Effect and RGB Zones groups
RGB Main Effect controls affect the main RGB Matrix effect
Axis Enable toggles only key switch LEDs
Underglow Enable toggles bottom LEDs while enabled state follows main effect
Indicator Enable toggles Caps Lock override
normal keys still work
VIA remap/macro/media/matrix tester still work
```

## Validation Result

Hardware validation passed on the current N32H487 board:

```text
Axis Enable toggles only key switch LEDs
Underglow Enable toggles only bottom LEDs
RGB Main Effect Off disables Axis and Underglow output
Indicator Enable toggles Caps Lock override
WS2812 PWM DMA first-LED color is correct after leading reset slots
normal keys work
VIA remap/macro/media/matrix tester work
```
