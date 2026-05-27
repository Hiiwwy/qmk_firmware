# N32 TFT Image Uploader Template

This is a host-side template for uploading TFT images to the N32 Devboard firmware through the same Raw HID endpoint used by VIA.

The firmware does not decode PNG/JPG/GIF directly. Convert images to QMK's QGF format first, then upload the `.qgf` file.

## Convert An Image

From QMK MSYS:

```sh
cd /f/N32/qmk_firmware
qmk painter-convert-graphics -w -f pal256 -i /path/to/image.gif -o /path/to/output
```

For smaller files, try `pal16`, `pal4`, or `mono16`.

The firmware image slot is 57344 bytes. Keep the generated `.qgf` file below that size.

## Use The Template

Open `index.html` in a Chromium-based browser with WebHID support.

1. Click Connect.
2. Pick the N32 Devboard.
3. Choose a `.qgf` file.
4. Click Upload.
5. Click Show Image if needed.

If VIA is already connected to the keyboard, close VIA first because only one app can claim the HID device at a time.

## Protocol

See `../../tft_image_protocol.md`.
