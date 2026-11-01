# ESP32 Multi-Panel Display

A multi-panel TFT display project for the ESP32-2432S028C (2.8" 240x320 native display used as a 320x240 landscape framebuffer) that cycles through Clock, Weather, and compute01 System Monitor panels with touch navigation.

## Features

- **Clock Panel (Primary)** - Large digital clock with full date, takes up most of the screen
- **Weather Panel** - Current temperature, weather condition icon, location, small clock corner
- **System Monitor Panel** - compute01 GPU temps, model inference status, RAM/disk usage gauges

### Navigation
- **Touch anywhere** - Cycle through panels
- **Double-tap** - Quick return to Clock panel
- Smooth transitions between panels

## Hardware Requirements

- **ESP32-2432S028C** (2.8" 240x320 native TFT, 320x240 landscape framebuffer, CST820 capacitive touch)
- USB-C micro-USB cable for programming and power
- WiFi network access
- Access to compute01 (192.168.1.92) for system monitoring

## Software Requirements

### Arduino IDE Setup

- ✅ **Arduino IDE 2.3.4 installed and configured**
- ✅ **arduino-cli 1.5.2-rc.1 installed and configured**
- ✅ **ESP32 Board Package v3.3.12 installed**
- ✅ **All required libraries installed**
- ✅ **Project compiles successfully**

See `SETUP_SUMMARY.md` for detailed installation status, or `UPLOAD_INSTRUCTIONS.md` for complete upload procedures.

#### Manual Setup (if needed)

1. **Install ESP32 Board Manager:**
   - Open Arduino IDE
   - Go to `File > Preferences`
   - In "Additional Board Manager URLs", add: `https://raw.githubusercontent.com/espressif/arduino-esp32/3.x/package_esp32_dev_index.json`
   - Go to `Tools > Board > Boards Manager`
   - Search for "esp32" and install "ESP32 by Espressif Systems"

2. **Install Required Libraries:**
   - `TFT_eSPI` by Bodmer
   - `ArduinoJson` by Benoit Blanchon
   - CST820 capacitive touch is handled over I2C by the firmware
   - `Time` by Paul Stoffregen
   - `NTPClient` by tzapu

3. **Select Board:**
   - `Tools > Board > ESP32 Arduino > ESP32 Dev Module`
   - `Tools > Upload Speed > 921600`
   - `Tools > CPU Frequency > 240MHz`

### Configuration

1. Copy `config.h.example` to `ESP32_MultiPanel_Display/config.h`
2. Edit `ESP32_MultiPanel_Display/config.h` with your credentials:
   ```cpp
   #define WIFI_SSID     "your_wifi_network"
   #define WIFI_PASSWORD "your_wifi_password"
   #define WEATHER_API_KEY "your_openweathermap_key"
   ```

3. Get OpenWeatherMap API Key:
   - Sign up at https://openweathermap.org
   - Navigate to API Keys section
   - Copy your free tier API key (1000 calls/day)

### Project Structure

```
esp32-display/
├── ESP32_MultiPanel_Display/
│   └── ESP32_MultiPanel_Display.ino  # Canonical sketch
├── config.h.example                  # Credential-free configuration template
└── README.md                         # This file
```

## Pin Mapping (ESP32-2432S028C)

The ESP32-2432S028C has integrated display and touch controller. The board uses:

| Signal | ESP32 Pin | Notes |
|--------|-----------|-------|
| TFT_CS | 15 | SPI Chip Select |
| TFT_DC | 2 | Data/Command |
| TFT_RST | -1 | Software reset |
| TFT_MOSI | 13 | SPI MOSI (fixed) |
| TFT_MISO | 12 | SPI MISO (fixed) |
| TFT_SCLK | 14 | SPI Clock (fixed) |
| CST820_SDA | 33 | Capacitive touch I2C SDA |
| CST820_SCL | 32 | Capacitive touch I2C SCL |
| CST820_RST | 25 | Capacitive touch reset |

**Note:** The ESP32-2432S028C board has these pins pre-wired. You typically don't need to connect anything externally unless modifying the setup.

## Compilation & Upload

### Quick Start

The project compiles successfully with Arduino IDE and arduino-cli.

1. **Open the project**: `ESP32_MultiPanel_Display/ESP32_MultiPanel_Display.ino`
2. **Configure**: Edit `config.h` with WiFi credentials
3. **Select board**: ESP32 Dev Module (FQBN: `esp32:esp32:esp32`)
4. **Upload**: Use Arduino IDE Upload button or command line

### Command Line

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 ./ESP32_MultiPanel_Display

# Upload
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 ./ESP32_MultiPanel_Display
```

See `UPLOAD_INSTRUCTIONS.md` for detailed upload procedures and troubleshooting.

## compute01 API Endpoints

The System Monitor panel fetches data from compute01's services:

| Service | Port | Endpoint | Purpose |
|---------|------|----------|---------|
| llama-swap | 11439 | `/health` | Qwen3.5 model status |
| Ministral | 11438 | `/health` | Ministral model status |
| Grafana | 3000 | N/A | Dashboard UI (not directly used) |
| Prometheus | 9090 | `/api/v1/query` | GPU/RAM/disk metrics |

### Current Limitations

- **GPU temperatures:** Currently using placeholder values. To get real GPU temps:
  - Enable Prometheus access from ESP32 network
  - Query DCGM metrics: `DCGM_FI_DEV_GPU_TEMP`

- **RAM/Disk usage:** Placeholder values. To get real values:
  - Query Prometheus: `node_memory_MemAvailable_bytes`
  - Query disk usage via `/proc/mounts` or Prometheus node exporter

### Example Prometheus Queries

```promql
# GPU temperature (GTX 1650)
DCGM_FI_DEV_GPU_TEMP{gpu="0"}

# GPU temperature (P40)
DCGM_FI_DEV_GPU_TEMP{gpu="1"}

# RAM usage percentage
100 - (node_memory_MemAvailable_bytes / node_memory_MemTotal_bytes * 100)

# Disk usage percentage
100 - (node_filesystem_avail{fstype="ext4"} / node_filesystem_size{fstype="ext4"} * 100)
```

## Troubleshooting

### Display shows but no touch response

1. Verify touch controller is initialized:
   - Check Serial output for "CST820 touch initialized"
2. Try recalibrating touch:
   - Check the CST820 I2C wiring: SDA=GPIO33, SCL=GPIO32, reset=GPIO25
3. Confirm the board is the C revision with a CST820 capacitive touch controller

### WiFi not connecting

1. Verify credentials in `config.h`
2. Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Ensure network allows outbound HTTP/HTTPS

### Weather panel shows errors

1. Verify OpenWeatherMap API key is correct and activated
2. Check API key hasn't exceeded rate limit (1000 calls/day free tier)
3. Verify city name format: `City,countrycode` (e.g., "London,uk")

### System panel shows offline

1. Verify compute01 is reachable from ESP32:
   ```bash
   ping 192.168.1.92
   ```
2. Check llama-swap is running:
   ```bash
   curl http://127.0.0.1:11439/health
   ```
3. Ensure ESP32 and compute01 are on the same network

### Panel transitions are slow

1. Reduce panel update intervals in `config.h`:
   - `WEATHER_UPDATE_INTERVAL_SEC` - decrease for more frequent updates
   - `SYSTEM_UPDATE_INTERVAL_SEC` - decrease for more frequent system checks

## Technical Notes

### FreeRTOS Task Structure

- **Display Task (priority 2):** Handles touch input and panel rendering at ~30fps
- **Network Task (priority 1):** Handles WiFi connection, time sync, API polling

### Memory Considerations

The ESP32-2432S028C has 520KB RAM. This project uses heap allocation for:
- JSON parsing (ArduinoJson)
- HTTP responses
- Display buffers

If you encounter memory issues:
- Reduce JSON buffer size in fetch functions
- Use smaller font sizes
- Reduce update frequencies

### Power Considerations

For battery operation:
- Use ESP32 deep sleep between updates
- Disable WiFi when not fetching data
- Reduce backlight brightness
- Consider using ESP32-S3 for better power efficiency

## License

MIT License - Feel free to modify and use for your projects.

## References

- [ESP32-2432S028C Datasheet](https://www.seeedstudio.com/wiki/ESP32-2432S028R)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [OpenWeatherMap API](https://openweathermap.org/api)
- [compute01 Documentation](../hosts/compute01/README.md)

## Version History

- **v1.0** (2026-10-30): Initial release with Clock, Weather, and System Monitor panels
  - Touch cycling between panels
  - Double-tap quick return to clock
  - NTP time synchronization
  - WiFi credentials in secure config
  - compute01 API integration
