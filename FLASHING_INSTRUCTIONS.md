# ESP32 Multi-Panel Display flashing instructions

These instructions cover the CYD/ESP32-2432S028C-style board attached to `compute01`.

## Verified hardware

- Host: `compute01`
- Board: ESP32-D0WD-V3, Arduino `ESP32 Dev Module`
- Flash: 4MB
- Display: 240x320 ILI9341 TFT
- Serial device: `/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0`
- FQBN: `esp32:esp32:esp32`

The USB bridge identifies as a CH340. That identifies the UART bridge only; confirm the MCU with `esptool` before flashing.

## Safety and repository state

Do not flash directly from a dirty checkout without checking its status first:

```bash
git -C /home/andyfied/src/esp32-display status --short --branch
```

The documented upload sketch is the nested matching sketch directory:

```text
/home/andyfied/src/esp32-display/ESP32_MultiPanel_Display/ESP32_MultiPanel_Display.ino
```

Keep Wi-Fi and API credentials in the local ignored `config.h`. Never commit or paste that file into a report.

For experiments, copy the sketch and `config.h` into a dated scratch directory and build there.

## Check the board before flashing

```bash
PORT=/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0
python3 -m esptool --port "$PORT" chip-id
python3 -m esptool --port "$PORT" flash-id
```

Confirm the expected ESP32 family, serial port, and 4MB flash before continuing.

## Compile

From the sketch directory or a clean scratch copy:

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32 \
  --libraries /home/andyfied/Arduino/libraries \
  --output-dir build \
  ESP32_MultiPanel_Display
```

A successful compile must report the application size and produce:

- `ESP32_MultiPanel_Display.ino.bin`
- `ESP32_MultiPanel_Display.ino.bootloader.bin`
- `ESP32_MultiPanel_Display.ino.partitions.bin`

The current default partition scheme has a 1.31MB application slot and a filesystem partition beginning at `0x290000` with size `0x160000`.

## Build the LittleFS image

Put files intended for the device in the sketch's `data/` directory. The installed tool is the Arduino ESP32 package copy, not `/usr/local/bin/mklittlefs`:

```bash
MKLITTLEFS=/home/andyfied/.arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs
"$MKLITTLEFS" -c data -s 0x160000 littlefs.bin
```

The filesystem image must fit within `0x160000` bytes.

## Back up before flashing

A full 4MB backup is recommended before a live experiment:

```bash
python3 -m esptool \
  --port "$PORT" \
  read-flash 0x000000 0x400000 preflash-4MiB.bin
```

Keep the backup outside Git, preferably in a dated task directory.

## Flash firmware and filesystem

```bash
python3 -m esptool \
  --port "$PORT" \
  --baud 921600 \
  write-flash \
  --flash-mode dio \
  --flash-freq 40m \
  --flash-size 4MB \
  0x1000 build/ESP32_MultiPanel_Display.ino.bootloader.bin \
  0x8000 build/ESP32_MultiPanel_Display.ino.partitions.bin \
  0x10000 build/ESP32_MultiPanel_Display.ino.bin \
  0x290000 littlefs.bin
```

`esptool` must report `Hash of data verified` for each written region.

If only the application changed, reflash only the application image at `0x10000`; do not rebuild or overwrite the filesystem unnecessarily.

## Verify after flashing

Read serial output at the sketch baud rate:

```bash
python3 - <<'PY'
import serial, time
port = "/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0"
ser = serial.Serial(port, 115200, timeout=1)
ser.reset_input_buffer()
end = time.time() + 15
while time.time() < end:
    line = ser.readline()
    if line:
        print(line.decode("utf-8", "replace").rstrip())
ser.close()
PY
```

Require at least:

- application boot banner
- `LittleFS mounted`
- `TFT initialized`
- `CST820 touch initialized (I2C SDA=33 SCL=32 RST=25)`
- no Guru Meditation or reboot loop

Serial output cannot prove the physical display image. Visual acceptance still requires looking at the TFT or a trusted camera capture.

## Read back exact flash regions

For stronger verification, read the exact written ranges and compare checksums:

```bash
APP_SIZE=$(stat -c %s build/ESP32_MultiPanel_Display.ino.bin)
python3 -m esptool --port "$PORT" read-flash 0x10000 "$APP_SIZE" postflash-app.bin
python3 -m esptool --port "$PORT" read-flash 0x290000 0x160000 postflash-filesystem.bin
sha256sum build/ESP32_MultiPanel_Display.ino.bin postflash-app.bin
sha256sum littlefs.bin postflash-filesystem.bin
```

The corresponding pairs should match exactly.
