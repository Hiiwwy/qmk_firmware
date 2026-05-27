# N32 TFT Image Upload Protocol

The keyboard accepts uploaded Quantum Painter QGF images through the VIA Raw HID endpoint.
Static images and GIF-like animations should be converted on the host side to QGF before upload.

## Storage

- Flash base: `0x08070000`
- Capacity: `57344` bytes
- Flash page size: `8192` bytes
- Payload format: QGF

## Raw HID Packet

All packets are 32 bytes.

- Byte 0: command magic, `0x7D`
- Byte 1: subcommand
- Byte 2: response status
- Bytes 3..31: command-specific payload

The firmware replies with the same command and subcommand. Status values:

- `0x00`: OK
- `0x01`: busy
- `0x02`: range or size error
- `0x03`: sequence/order error
- `0x04`: flash write error
- `0x05`: invalid image or command

## Commands

### Info: `0x00`

Request:

- `7D 00 ...`

Response:

- Bytes 3..6: image flash capacity, little-endian
- Bytes 7..10: active upload size, little-endian
- Bytes 11..14: active upload offset, little-endian
- Byte 15: max payload bytes per data packet
- Byte 16: loaded frame count, 0 if no valid image
- Bytes 17..18: image width, little-endian
- Bytes 19..20: image height, little-endian

### Begin Upload: `0x01`

Request:

- Bytes 2..5: total QGF size, little-endian

### Data: `0x02`

Request:

- Bytes 2..5: absolute upload offset, little-endian
- Byte 6: payload length, max `25`
- Bytes 7..31: payload bytes

Data packets must be sequential.

### End Upload: `0x03`

Flushes the final flash page, validates the uploaded QGF, then switches the TFT to image mode.

### Abort Upload: `0x04`

Cancels the current upload session.

### Show Image: `0x05`

Switches the TFT to image mode.

### Clear Image: `0x06`

Invalidates the stored image by erasing the first image flash page.
