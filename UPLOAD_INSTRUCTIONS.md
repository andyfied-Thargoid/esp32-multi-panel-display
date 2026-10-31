# ESP32-2432S028C Upload Instructions

## Hardware Setup

### Board Overview
- **Board**: ESP32-2432S028C (2.8" TFT with touch)
- **Display**: 240x320 pixels, IPS LCD
- **Touch Controller**: CST820 capacitive controller (I2C)
- **MCU**: ESP32-WROOM-32

### USB Connection

1. **Connect USB Cable**: Use a Micro-USB or USB-C cable (depending on your board variant)
   - Connect the data-capable USB port on the ESP32 board to your computer
   - **Important**: Use a data-capable cable (not charge-only)

2. **Identify Serial Port**:
   - On Linux: `ls -l /dev/ttyUSB*` or `ls -l /dev/ttyACM*`
   - The ESP32 typically appears as `/dev/ttyUSB0` or `/dev/ttyACM0`
   - On Windows: Check Device Manager under "Ports (COM & LPT)"

3. **Set GPIO0 to Ground for Bootloader Mode**:
   - **Method 1 (Manual)**: Press and hold the BOOT button, then press RESET, release RESET then BOOT
   - **Method 2 (Auto-reset)**: Most ESP32-2432S028C boards support auto-reset via DTR/RTS
   - **Method 3 (Persistent)**: Connect GPIO0 to GND, then power cycle the board

## Arduino IDE Setup

### 1. Install ESP32 Board Package

In Arduino IDE:
- Go to **File > Preferences**
- In "Additional Board Manager URLs", add:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/3.x/package_esp32_dev_index.json
  ```
- Go to **Tools > Board > Boards Manager**
- Search for "esp32" and install `esp32 by Espressif Systems`

### 2. Configure Board Settings

In Arduino IDE **Tools** menu:
- **Board**: `ESP32 Dev Module` (FQBN: `esp32:esp32:esp32`)
- **CPU Frequency**: `240MHz` (or `160MHz` for lower power)
- **Flash Size**: `4MB` (32Mb)
- **Partition Scheme**: `App 2MB/SPiffs 1.1MB` (default is fine)
- **Upload Speed**: `921600` (faster uploads)
- **Debug Level**: `None`

### 3. Install Required Libraries

In Arduino IDE **Sketch > Include Library > Manage Libraries**:
- Search for and install:
  - **TFT_eSPI** by Bodmer (v2.5.44+)
  - **ArduinoJson** by Benoit Blanchon (v6.x or v7.x)
  - No external touch library is required; the firmware uses `Wire` for CST820 I2C
  - **Time** by Paul Stoffregen (v1.6+)
  - **NTPClient** by Rob Tillaart (v3.x+)

### 4. Configure TFT_eSPI (Critical!)

For ESP32-2432S028C, you must configure the display:

1. Navigate to: `~/Arduino/libraries/TFT_eSPI/User_Setup_Select.h`
2. Uncomment the line for your display:
   ```cpp
   #define ESP32_SSD2814
   ```
   OR for custom setup, edit `User_Setup.h`:
   ```cpp
   #define SSD2814_DRIVER
   #define TFT_WIDTH  240
   #define TFT_HEIGHT 320
   #define TFT_MISO 12
   #define TFT_MOSI 13
   #define TFT_SCLK 14
   #define TFT_CS   15
   #define TFT_DC    2
   #define TFT_RST   4  // or -1 for software reset
   #define CST820_SDA 33
   #define CST820_SCL 32
   #define CST820_RST 25
   ```

### 5. Configure WiFi Credentials

Edit `config.h` in the project:
```cpp
#define WIFI_SSID     "your_wifi_name"
#define WIFI_PASSWORD "your_wifi_password"
```

## Upload Procedure

### Method 1: Using Arduino IDE

1. **Open the Project**:
   - In Arduino IDE: **File > Open**
   - Navigate to `esp32-multi-panel-display/ESP32_MultiPanel_Display.ino`

2. **Select Board and Port**:
   - **Tools > Board > ESP32 Dev Module**
   - **Tools > Port > /dev/ttyUSB0** (or your detected port)

3. **Compile and Upload**:
   - Click **Upload** button (→) or **Ctrl+U**
   - If upload fails with "Failed to connect to ESP32":
     - Hold BOOT button, press RESET, release both, try again
     - Or connect GPIO0 to GND temporarily during upload

4. **Monitor Output**:
   - Open **Tools > Serial Monitor** (Ctrl+Shift+M)
   - Baud rate: **115200**
   - Watch for: "ESP32 Multi-Panel Display" startup message

### Method 2: Using arduino-cli (Command Line)

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 ./ESP32_MultiPanel_Display

# Upload
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 ./ESP32_MultiPanel_Display
```

### Method 3: Manual Flashing (Advanced)

```bash
# Flash all binaries at once
esptool.py --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x1000 ESP32_MultiPanel_Display.ino.bootloader.bin \
  0x8000 ESP32_MultiPanel_Display.ino.partitions.bin \
  0x10000 ESP32_MultiPanel_Display.ino.bin
```

## Troubleshooting

### Common Issues

#### 1. **Failed to Connect to ESP32 (Serial Timeout)**
**Symptoms**: Upload fails with "Failed to connect to ESP32" or "Timed out waiting for packet header"

**Solutions**:
- **Boot Mode**: Connect GPIO0 to GND during upload, then disconnect
- **USB Port**: Try a different USB port (USB 2.0 often more reliable)
- **USB Cable**: Ensure it's a data cable, not charge-only
- **Baud Rate**: Try `115200` instead of `921600`
- **Auto-Reset**: Some boards need GPIO0-GND connection

#### 2. **Brownout Detector Reset**
**Symptoms**: Sketch starts then resets repeatedly

**Solutions**:
- Reduce CPU frequency: **Tools > CPU Frequency > 160MHz**
- Check power supply (use 5V 2A adapter, not USB)
- Add decoupling capacitors to power lines

#### 3. **Touch Not Working**
**Symptoms**: Touch panel responds incorrectly or not at all

**Solutions**:
- Check TFT_eSPI `User_Setup.h` has correct touch pins:
  ```cpp
  #define CST820_SDA 33
  #define CST820_SCL 32
  #define CST820_RST 25
  ```
- Verify CST820 I2C wiring and the `CST820 touch initialized` serial message
- Check I2C wiring and address `0x15` if touch is not detected

#### 4. **Display Not Initializing**
**Symptoms**: Screen stays black or shows artifacts

**Solutions**:
- Verify TFT_eSPI `User_Setup.h` matches your board:
  ```cpp
  #define ESP32_SSD2814  // For ESP32-2432S028C
  ```
- Check wiring (SPI pins, CS, DC, RST)
- Verify power to display (5V required)

#### 5. **WiFi Connection Fails**
**Symptoms**: Serial monitor shows "WiFi connection failed"

**Solutions**:
- Verify `config.h` has correct SSID and password
- Check WiFi router compatibility (ESP32 doesn't support WPA3)
- Try 2.4GHz network only (ESP32 doesn't support 5GHz)
- Verify WiFi credentials are in quotes in `config.h`

#### 6. **Weather Not Updating**
**Symptoms**: Weather shows default/empty values

**Solutions**:
- Check `WEATHER_API_KEY` in `config.h` (get free key from openweathermap.org)
- Verify internet connectivity (check Serial Monitor for IP address)
- Check firewall blocking outbound HTTP requests

### Debugging Tips

#### Enable Verbose Compilation
In Arduino IDE:
- **File > Preferences > Show verbose output during:** ✓ compilation ✓ upload

#### Serial Monitor Commands
The sketch outputs to Serial at 115200 baud:
- `Touch: Switching to panel X` - Touch panel change
- `WiFi connected!` - WiFi connected successfully
- `Time synced!` - NTP time sync successful
- IP address - Shows assigned WiFi IP

#### GPIO Pin Verification
If display/touch issues persist, verify pin mapping:
- Display SPI: MISO(12), MOSI(13), SCLK(14), CS(15), DC(2), RST(4)
- Touch CS: 33, Touch IRQ: 25
- Check for conflicts with other peripherals

## Board Characteristics

### ESP32-2432S028C Specific Notes

1. **Power Requirements**:
   - Display draws significant current (~500mA at brightness)
   - Use external 5V power adapter if possible
   - USB may not provide sufficient power for full brightness

2. **Touch Calibration**:
   - CST820 coordinate mapping may need adjustment for a different panel orientation
   - Touch coordinates may be reversed depending on mounting
   - Confirm the CST820 reset line is connected to GPIO25

3. **Display Orientation**:
   - Default rotation: Portrait (1)
   - Change in code: `tft.setRotation(0-3)` for different orientations
   - Touch coordinates auto-adjust with rotation

4. **FreeRTOS**:
   - Uses dual-core FreeRTOS (Display on core 0, Network on core 1)
   - Don't use `delay()` in display code (use `vTaskDelay`)
   - Serial output can impact timing (disable in production)

5. **Boot Behavior**:
   - On power-up, displays startup splash (if added)
   - Touch input active immediately after boot
   - WiFi auto-connects on boot

## Testing Checklist

Before deploying to production:

- [ ] **USB Connection**: Board detected as serial port
- [ ] **Upload**: Code uploads successfully
- [ ] **Display**: Screen shows clock panel on boot
- [ ] **Touch**: Touch cycles through all 3 panels
- [ ] **WiFi**: WiFi indicator shows signal strength
- [ ] **Time**: Clock shows correct time
- [ ] **Weather**: Weather panel shows temperature/condition
- [ ] **System Panel**: Shows compute01 status (requires live compute01)
- [ ] **Double-tap**: Returns to clock panel

## Recovery Options

If board becomes unresponsive:

1. **Hard Reset**: Disconnect power, wait 5 seconds, reconnect
2. **Factory Reset**:
   - Hold BOOT button, power cycle, release BOOT after 2 seconds
   - Upload empty sketch to clear settings
3. **Reflash Bootloader**:
   ```bash
   esptool.py --port /dev/ttyUSB0 erase_flash
   arduino-cli upload --protocol espboot --port /dev/ttyUSB0 ...
   ```

## Resources

- **Board Datasheet**: [ESP32-2432S028C Datasheet](https://www.adafruit.com/product/4757)
- **TFT_eSPI Library**: https://github.com/Bodmer/TFT_eSPI
- **Arduino JSON**: https://arduinojson.org/
- **ESP32 Arduino Core**: https://github.com/espressif/arduino-esp32
- **Seeed Studio Documentation**: https://wiki.seeedstudio.com/ESP32-2432S028C/

## Author

Project developed for ESP32-2432S028C multi-panel display with compute01 monitoring integration.
