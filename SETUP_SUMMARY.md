# ESP32 Arduino IDE Setup - Quick Reference

## ✅ Installation Status

### Arduino IDE
- **Location**: `/home/andyfied/opt/arduino/`
- **Version**: 2.3.4
- **Status**: ✅ Installed

### arduino-cli
- **Location**: `/usr/local/bin/arduino-cli`
- **Version**: 1.5.2-rc.1
- **Status**: ✅ Installed

### ESP32 Board Package
- **Platform**: `esp32:esp32`
- **Version**: 3.3.12
- **FQBN**: `esp32:esp32:esp32`
- **Status**: ✅ Installed

### Libraries Installed
| Library | Version | Location |
|---------|---------|----------|
| TFT_eSPI | 2.5.44 | `~/Arduino/libraries/TFT_eSPI` |
| ArduinoJson | 7.4.2 | `~/Arduino/libraries/ArduinoJson` |
| CST820 touch | built-in Wire I2C implementation | GPIO33 SDA / GPIO32 SCL / GPIO25 reset |
| Time | 1.6.1 | `~/Arduino/libraries/Time` |
| NTPClient | 3.2.1 | `~/Arduino/libraries/NTPClient` |
| Arduino_GFX | latest | `~/.arduino15/libraries/Arduino_GFX` |
| JPEGDEC | latest | `~/.arduino15/libraries/JPEGDEC` |

## ✅ Project Compilation

**Canonical sketch**: `ESP32_MultiPanel_Display/ESP32_MultiPanel_Display.ino`
- **Status**: ✅ Compiles successfully
- **Flash Size**: 1,086,504 bytes (82% of 4MB flash)
- **RAM Usage**: 50,480 bytes (15% of 327,680 bytes)
- **Output**: `.bin`, `.elf`, `.merged.bin` generated

## 📋 Quick Setup Steps

### 1. Open Project in Arduino IDE
```bash
# Or open directly from GUI
arduino-ide --no-sandbox &
```
- File → Open → `esp32-multi-panel-display/ESP32_MultiPanel_Display/ESP32_MultiPanel_Display.ino`

### 2. Configure Board
- **Board**: `ESP32 Dev Module`
- **FQBN**: `esp32:esp32:esp32`
- **CPU Frequency**: 240MHz
- **Flash Size**: 4MB (32Mb)
- **Upload Speed**: 921600

### 3. Configure TFT_eSPI for the exact CYD C revision
Copy the repository setup into the TFT_eSPI setup directory:
```bash
cp Setup801_ESP32_CYD_ILI9341_240x320.h \
  ~/Arduino/libraries/TFT_eSPI/User_Setups/Setup801_ESP32_CYD_ILI9341_240x320.h
```
Then edit `~/Arduino/libraries/TFT_eSPI/User_Setup_Select.h` so the repository setup is the one active include:
```cpp
#include <User_Setups/Setup801_ESP32_CYD_ILI9341_240x320.h>
```
The setup explicitly selects `ILI9341_2_DRIVER` with the panel's native 240x320 dimensions; the sketch rotates it to a 320x240 landscape framebuffer, SPI MISO/MOSI/SCLK 12/13/14, TFT CS/DC 15/2, software reset, GPIO21 active-HIGH backlight, HSPI, and a conservative 27MHz SPI clock. The sketch handles the C revision's CST820 touch separately over I2C (SDA 33, SCL 32, reset 25).

### 4. Set WiFi Credentials
Copy `config.h.example` to `ESP32_MultiPanel_Display/config.h`, then edit the local copy:
```cpp
#define WIFI_SSID     "your_wifi_name"
#define WIFI_PASSWORD "your_wifi_password"
```

### 5. Upload
- Connect ESP32 via USB
- Select port: `/dev/ttyUSB0` (or `/dev/ttyACM0`)
- Click **Upload** button

## 🔧 Command Line Compilation

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 \
  ./esp32-multi-panel-display/ESP32_MultiPanel_Display

# Upload
arduino-cli upload --fqbn esp32:esp32:esp32 \
  -p /dev/ttyUSB0 \
  ./esp32-multi-panel-display/ESP32_MultiPanel_Display
```

## 📝 Board Pin Mapping (ESP32-2432S028C)

| Function | Pin |
|----------|-----|
| SPI MISO | 12 |
| SPI MOSI | 13 |
| SPI SCLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT RST | 4 (or -1 for SW reset) |
| CST820 SDA | 33 | I2C SDA |
| CST820 SCL | 32 | I2C SCL |
| CST820 reset | 25 | Reset |

## 🚀 Upload Troubleshooting

| Issue | Solution |
|-------|----------|
| "Failed to connect" | Hold BOOT, press RESET, release both |
| Brownout reset | Set CPU to 160MHz, use external 5V power |
| Touch not working | Verify `User_Setup.h` touch pin config |
| Display black | Verify the selected setup contains `ILI9341_2_DRIVER`, the CYD SPI pins, GPIO21 backlight, and `USE_HSPI_PORT` |

## 📚 Documentation

- **Full Upload Instructions**: `UPLOAD_INSTRUCTIONS.md`
- **Project README**: `esp32-display/README.md`
- **TFT_eSPI**: https://github.com/Bodmer/TFT_eSPI
- **ESP32 Core**: https://github.com/espressif/arduino-esp32

## ✅ Verification Checklist

- [x] Arduino IDE installed (v2.3.4)
- [x] arduino-cli installed (v1.5.2-rc.1)
- [x] ESP32 board package installed (v3.3.12)
- [x] All required libraries installed
- [x] Project compiles successfully
- [x] Upload instructions documented
- [ ] Board physically connected (pending)
- [ ] First upload completed (pending)
