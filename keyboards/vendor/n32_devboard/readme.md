# N32 Devboard

Minimal QMK target for validating the private N32H487 + FreeRTOS platform port.

Build example:

```sh
make vendor/n32_devboard:default
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
