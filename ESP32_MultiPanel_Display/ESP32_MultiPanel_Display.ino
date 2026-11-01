/*
 * ESP32 Multi-Panel Display
 * =========================
 * ESP32-2432S028C TFT Display Project
 *
 * Displays three cycling panels:
 * 1. Clock (Primary) - Large digital clock with full date
 * 2. Weather - Current temperature, condition, location with small clock
 * 3. System Monitor - compute01 GPU temps, model status, RAM/disk usage
 *
 * Features:
 * - Touch to cycle panels
 * - Double-tap to return to clock
 * - FreeRTOS multitasking
 * - NTP time sync
 * - WiFi credentials in secure config
 *
 * Hardware: ESP32-2432S028C (2.8" 240x320 TFT)
 * Libraries needed: TFT_eSPI, ArduinoJson, WiFi, HTTPClient, Time, NTPClient
 */

#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include "esp_heap_caps.h"

#include "config.h"
#include "openweathermap_root_ca.h"

// ============================================================================
// TFT_eSPI Configuration for ESP32-2432S028C
// ============================================================================
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1  // Use software reset

#define CST820_SDA  33
#define CST820_SCL  32
#define CST820_RST  25
#define CST820_ADDR 0x15

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ESP32-2432S028C (C revision): CST820 capacitive touch over I2C.
class CST820Touch {
 public:
  bool begin() {
    pinMode(CST820_RST, OUTPUT);
    digitalWrite(CST820_RST, LOW);
    delay(5);
    digitalWrite(CST820_RST, HIGH);
    Wire.begin(CST820_SDA, CST820_SCL);
    delay(50);

    // Probe the controller address and require an I2C acknowledgement.
    Wire.beginTransmission(CST820_ADDR);
    const uint8_t error = Wire.endTransmission();
    return error == 0;
  }

  bool touched() {
    uint8_t points = 0;
    return readRegister(0x02, &points, 1) && (points & 0x0F) != 0;
  }

  bool getPoint(uint16_t &x, uint16_t &y) {
    uint8_t data[4] = {};
    if (!readRegister(0x03, data, sizeof(data))) return false;
    x = ((data[0] & 0x0F) << 8) | data[1];
    y = ((data[2] & 0x0F) << 8) | data[3];
    return true;
  }

 private:
  bool readRegister(uint8_t reg, uint8_t *data, size_t length) {
    Wire.beginTransmission(CST820_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(CST820_ADDR, length) != length) return false;
    for (size_t i = 0; i < length; ++i) data[i] = Wire.read();
    return true;
  }
};

// ============================================================================
// FreeRTOS Task Handles
// ============================================================================
TaskHandle_t displayTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;

// ============================================================================
// Global State
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
CST820Touch ts;

// Time state
unsigned long lastTimeSync = 0;
time_t currentTime = 0;

// Touch state
uint16_t lastTouchX = 0, lastTouchY = 0;
uint16_t lastPressX = 0, lastPressY = 0;
bool timeSynchronized = false;

bool isTouched = false;  // Current physical press state.
unsigned long lastTapTime = 0;
unsigned long lastReleaseTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 50;
const unsigned long DOUBLE_TAP_WINDOW_MS = 300;

// Panel state
int currentPanel = 0;  // 0 = Clock, 1 = Weather, 2 = System
unsigned long lastPanelUpdate = 0;

// Weather state
String weatherCity = WEATHER_CITY;
float weatherTemp = 0.0;
String weatherCondition = "";
String weatherIcon = "";
unsigned long lastWeatherUpdate = 0;
volatile bool weatherDataChanged = true;
SemaphoreHandle_t weatherMutex = nullptr;
const unsigned long WEATHER_UPDATE_INTERVAL = 300000;  // 5 minutes

// System state
float gtx1650Temp = 0.0;
float p40Temp = 0.0;
enum ModelStatus { MODEL_UNKNOWN, MODEL_OFFLINE, MODEL_READY };
ModelStatus qwen35Status = MODEL_UNKNOWN;
ModelStatus ministralStatus = MODEL_UNKNOWN;
float ramUsage = 0.0;
float diskUsage = 0.0;
unsigned long lastSystemUpdate = 0;
volatile bool systemDataChanged = true;
const unsigned long SYSTEM_UPDATE_INTERVAL = 30000;  // 30 seconds
const uint16_t HTTP_CONNECT_TIMEOUT_MS = 5000;
const uint16_t HTTP_REQUEST_TIMEOUT_MS = 8000;

// API endpoints; the server address comes from the local config.h.
const String COMPUTE01_HOST_STR = COMPUTE01_HOST;
#ifndef QWEN_MODEL_ID
#define QWEN_MODEL_ID "qwen35-q3"
#endif
#ifndef MINISTRAL_MODEL_ID
#define MINISTRAL_MODEL_ID "/mnt/marmot/models/ministral3/mistralai_Ministral-3-3B-Instruct-2512-IQ3_XS.gguf"
#endif

// ============================================================================
// Panel Rendering Functions
// ============================================================================

// Draw background
void drawBackground(int color) {
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color);
}

// Draw clock panel (Primary - dominant)
void drawClockPanel() {
  drawBackground(0x000000);  // Black background

  // Large digital clock - centered, taking up most of screen
  tft.setTextColor(0x001F, 0x0000);  // Blue text on black
  tft.setTextSize(5);

  // Format time as HH:MM:SS
  char timeStr[16];
  sprintf(timeStr, "%02d:%02d:%02d", hour(), minute(), second());

  // Calculate position for centered display
  int x = (SCREEN_WIDTH - tft.textWidth(timeStr)) / 2;
  int y = (SCREEN_HEIGHT - tft.fontHeight(5)) / 2 - 20;

  tft.setCursor(x, y);
  tft.print(timeStr);

  // Full date below - smaller but still prominent
  tft.setTextColor(0xFFFF, 0x0000);  // Yellow text on black
  tft.setTextSize(2);

  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char dateStr[32];
  sprintf(dateStr, "%s, %d %s %d",
          days[weekday() - 1],
          day(),
          monthStr(month()),
          year());

  int dateX = (SCREEN_WIDTH - tft.textWidth(dateStr)) / 2;
  int dateY = y + tft.fontHeight(5) + 30;

  tft.setCursor(dateX, dateY);
  tft.print(dateStr);

  // Small WiFi indicator in corner
  drawWiFiIndicator(5, 5);

  // Small clock in bottom right for weather/system panels context
  char smallTime[16];
  sprintf(smallTime, "%02d:%02d", hour(), minute());
  tft.setTextSize(1);
  tft.setTextColor(0x0419, 0x0000);  // Dark blue
  tft.setCursor(SCREEN_WIDTH - tft.textWidth(smallTime) - 5, SCREEN_HEIGHT - 20);
  tft.print(smallTime);
}

void drawContextClock(uint16_t backgroundColor) {
  tft.fillRect(0, 0, 90, 16, backgroundColor);
  char smallTime[16];
  sprintf(smallTime, "%02d:%02d:%02d", hour(), minute(), second());
  tft.setTextColor(0xFFFF, backgroundColor);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print(smallTime);
}

// Draw weather panel
void drawWeatherPanel() {
  drawBackground(0x0019);  // Dark blue background

  // Small clock in top left
  drawContextClock(0x0019);

  String citySnapshot;
  String conditionSnapshot;
  float temperatureSnapshot;
  unsigned long weatherUpdateSnapshot;
  if (weatherMutex != nullptr) xSemaphoreTake(weatherMutex, portMAX_DELAY);
  citySnapshot = weatherCity;
  conditionSnapshot = weatherCondition;
  temperatureSnapshot = weatherTemp;
  weatherUpdateSnapshot = lastWeatherUpdate;
  if (weatherMutex != nullptr) xSemaphoreGive(weatherMutex);

  // Location at top
  tft.setTextColor(0xFFFF, 0x0019);  // Yellow on dark blue
  tft.setTextSize(2);
  tft.setCursor(5, 25);
  tft.print(citySnapshot);

  // Weather icon (using an ASCII-safe text label for the configured bitmap font)
  const char* iconChar = getWeatherIcon(conditionSnapshot);
  tft.setTextColor(0xFFE0, 0x0019);  // Orange on dark blue
  // The fallback label is seven characters; use a smaller size for it and
  // centre every label from its measured width to prevent clipping.
  tft.setTextSize(strcmp(iconChar, "WEATHER") == 0 ? 3 : 6);
  int iconX = (SCREEN_WIDTH - tft.textWidth(iconChar)) / 2;
  int iconY = 70;
  tft.setCursor(iconX, iconY);
  tft.print(iconChar);

  // Temperature - large display
  tft.setTextColor(0xFFFF, 0x0019);
  tft.setTextSize(6);
  char tempStr[16];
  sprintf(tempStr, "%dC", (int)temperatureSnapshot);

  int tempX = (SCREEN_WIDTH - tft.textWidth(tempStr)) / 2;
  int tempY = iconY + 60;
  tft.setCursor(tempX, tempY);
  tft.print(tempStr);

  // Condition text below
  tft.setTextColor(0x07FF, 0x0019);  // Cyan on dark blue
  tft.setTextSize(2);
  tft.setCursor(iconX, tempY + 50);
  tft.print(conditionSnapshot);

  // Draw WiFi indicator
  drawWiFiIndicator(5, 45);

  // Update indicator
  unsigned long elapsed = millis() - weatherUpdateSnapshot;
  if (elapsed > WEATHER_UPDATE_INTERVAL) {
    tft.setTextSize(1);
    tft.setTextColor(0x05A0, 0x0019);  // Dark green
    tft.setCursor(5, SCREEN_HEIGHT - 15);
    tft.print("Refreshing in...");
  }
}

// Draw system monitor panel
void drawSystemPanel() {
  drawBackground(0x0010);  // Dark green background

  // Small clock in top left
  drawContextClock(0x0010);

  // Title. GPU/RAM/disk values below are placeholders until a read-only
  // monitoring endpoint is available; label them so they are not mistaken for
  // live measurements.
  tft.setTextColor(0x07FF, 0x0010);  // Cyan
  tft.setTextSize(2);
  tft.setCursor(5, 25);
  tft.print("compute01 Monitor");

  // GPU Temperatures
  tft.setTextColor(0xFFFF, 0x0010);  // Yellow
  tft.setTextSize(1);
  tft.setCursor(5, 45);
  tft.print("GTX 1650 demo: ");
  tft.setTextColor(0x0414, 0x0010);  // Green
  char gtxTempStr[10];
  sprintf(gtxTempStr, "%.1f°C", gtx1650Temp);
  tft.print(gtxTempStr);

  tft.setCursor(5, 60);
  tft.setTextColor(0xFFFF, 0x0010);
  tft.print("P40 GPU demo: ");
  tft.setTextColor(0x0414, 0x0010);
  char p40TempStr[10];
  sprintf(p40TempStr, "%.1f°C", p40Temp);
  tft.print(p40TempStr);

  // Model Status
  tft.setCursor(5, 85);
  tft.setTextColor(0xFFFF, 0x0010);
  tft.print("Qwen3.5: ");
  tft.setTextColor(qwen35Status == MODEL_READY ? 0x07FF :
                   qwen35Status == MODEL_UNKNOWN ? 0xFFE0 : 0x05A0, 0x0010);
  tft.print(qwen35Status == MODEL_READY ? "READY" :
            qwen35Status == MODEL_UNKNOWN ? "UNKNOWN" : "OFFLINE");

  tft.setCursor(5, 100);
  tft.setTextColor(0xFFFF, 0x0010);
  tft.print("Ministral: ");
  tft.setTextColor(ministralStatus == MODEL_READY ? 0x07FF :
                   ministralStatus == MODEL_UNKNOWN ? 0xFFE0 : 0x05A0, 0x0010);
  tft.print(ministralStatus == MODEL_READY ? "READY" :
            ministralStatus == MODEL_UNKNOWN ? "UNKNOWN" : "OFFLINE");

  // RAM/Disk Usage Gauges (text-based)
  tft.setCursor(5, 130);
  tft.setTextColor(0xFFFF, 0x0010);
  tft.print("RAM demo: ");
  tft.setTextColor(0x07FF, 0x0010);
  char ramStr[10];
  sprintf(ramStr, "%.1f%%", ramUsage);
  tft.print(ramStr);

  // Draw simple progress bar for RAM
  drawGaugeBar(5, 145, SCREEN_WIDTH - 10, 15, ramUsage, 0x001F, 0x07FF, 0x0010);

  tft.setCursor(5, 170);
  tft.setTextColor(0xFFFF, 0x0010);
  tft.print("Disk demo: ");
  tft.setTextColor(0x07FF, 0x0010);
  char diskStr[10];
  sprintf(diskStr, "%.1f%%", diskUsage);
  tft.print(diskStr);

  // Draw simple progress bar for disk
  drawGaugeBar(5, 185, SCREEN_WIDTH - 10, 15, diskUsage, 0x001F, 0x07FF, 0x0010);

  // Status indicators
  tft.setTextColor(0xFFFF, 0x0010);
  tft.setTextSize(1);
  tft.setCursor(5, SCREEN_HEIGHT - 40);
  tft.print("GTX 1650 Model: ");
  tft.setTextColor(0x07FF, 0x0010);
  tft.print(qwen35Status == MODEL_READY ? "Qwen3.5" : "N/A");

  tft.setCursor(5, SCREEN_HEIGHT - 25);
  tft.print("GTX Ministral: ");
  tft.setTextColor(0x07FF, 0x0010);
  tft.print(ministralStatus == MODEL_READY ? "Ready" :
            ministralStatus == MODEL_UNKNOWN ? "Unknown" : "Offline");

  // WiFi indicator in the unused top-right corner; keep it off the GPU rows.
  drawWiFiIndicator(SCREEN_WIDTH - 80, 5);
}

// Draw WiFi indicator
void drawWiFiIndicator(int x, int y) {
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.drawString("WiFi", x, y);
    // Draw signal bars
    int bars = min(4, WiFi.RSSI() / -20);
    for (int i = 0; i < bars; i++) {
      tft.fillRect(x + 40 + (i * 4), y, 2, 2, 0xFFFF);
    }
  } else {
    tft.drawString("No WiFi", x, y);
  }
}

// Draw gauge bar (text-based progress bar)
void drawGaugeBar(int x, int y, int width, int height, float percentage,
                  int normalColor, int fullColor, int bgColor) {
  int barWidth = (width - 2) * (percentage / 100.0);

  // Background
  tft.fillRect(x, y, width, height, bgColor);

  // Progress
  tft.fillRect(x, y, barWidth, height, normalColor);

  // Border
  tft.drawRect(x, y, width, height, 0xFFFF);
}

// Get weather icon character
const char* getWeatherIcon(const String &condition) {
  String normalized = condition;
  normalized.toLowerCase();
  if (normalized.indexOf("clear") >= 0 || normalized.indexOf("sunny") >= 0) return "SUN";
  if (normalized.indexOf("cloud") >= 0 || normalized.indexOf("overcast") >= 0) return "CLOUD";
  if (normalized.indexOf("rain") >= 0 || normalized.indexOf("drizzle") >= 0) return "RAIN";
  if (normalized.indexOf("thunder") >= 0 || normalized.indexOf("storm") >= 0) return "STORM";
  if (normalized.indexOf("snow") >= 0) return "SNOW";
  if (normalized.indexOf("fog") >= 0 || normalized.indexOf("mist") >= 0) return "FOG";
  if (normalized.indexOf("partly") >= 0) return "PARTLY";
  return "WEATHER";
}

// ============================================================================
// API Fetch Functions
// ============================================================================

// Fetch time from NTP server
bool fetchTime() {
  // NTPClient updates TimeLib only. configTime/getLocalTime also sets the
  // ESP32 system clock, which WiFiClientSecure requires for certificate checks.
  configTime(TIMEZONE_OFFSET, 0, NTP_SERVER);
  struct tm localTime;
  bool synced = false;
  for (int attempt = 0; attempt < 10 && !synced; ++attempt) synced = getLocalTime(&localTime, 1000);
  if (!synced) {
    timeSynchronized = false;
    return false;
  }
  time_t systemEpoch = time(nullptr);
  setTime(systemEpoch + TIMEZONE_OFFSET);
  currentTime = systemEpoch;
  lastTimeSync = millis();
  timeSynchronized = true;
  return true;
}

// Fetch weather data
void fetchWeather() {
  if (!timeSynchronized || WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure client;
  client.setCACert(OPENWEATHERMAP_ROOT_CA);

  HTTPClient https;
  https.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  https.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
  String url = "https://api.openweathermap.org/data/2.5/weather?q=";
  url += weatherCity;
  url += "&units=metric&lang=en&appid=";
  url += WEATHER_API_KEY;

  if (!https.begin(client, url)) {
    Serial.println("Weather HTTPS setup failed");
    return;
  }
  https.addHeader("Content-Type", "application/json");

  const int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();
    float newWeatherTemp;
    String newWeatherCondition;
    if (weatherMutex != nullptr) xSemaphoreTake(weatherMutex, portMAX_DELAY);
    newWeatherTemp = weatherTemp;
    newWeatherCondition = weatherCondition;
    if (weatherMutex != nullptr) xSemaphoreGive(weatherMutex);

    // Parse weather JSON (ArduinoJson 7.x)
    // Using simple string parsing to avoid ArduinoJson dependency issues
    // Extract temperature
    int tempStart = payload.indexOf("\"temp\":");
    if (tempStart > 0) {
      tempStart += 7;
      int tempEnd = payload.indexOf(',', tempStart);
      if (tempEnd > tempStart) {
        String tempStr = payload.substring(tempStart, tempEnd);
        newWeatherTemp = tempStr.toFloat();
      }
    }

    // Extract condition description
    int condStart = payload.indexOf("\"description\":");
    if (condStart > 0) {
      condStart += 14;
      char quote = payload.charAt(condStart);
      int condEnd = payload.indexOf(quote, condStart + 1);
      if (condEnd > condStart) {
        newWeatherCondition = payload.substring(condStart + 1, condEnd);
      }
    }

    if (weatherMutex != nullptr) xSemaphoreTake(weatherMutex, portMAX_DELAY);
    weatherTemp = newWeatherTemp;
    weatherCondition = newWeatherCondition;
    lastWeatherUpdate = millis();
    if (weatherMutex != nullptr) xSemaphoreGive(weatherMutex);
    weatherDataChanged = true;
  } else {
    Serial.print("Weather HTTPS request failed, HTTP code: ");
    Serial.println(httpCode);
  }

  https.end();
}

ModelStatus modelStatusFromResponse(const String &payload,
                                    const char *intendedModelId,
                                    bool statusRequired) {
  JsonDocument document;
  DeserializationError error = deserializeJson(document, payload);
  if (error) return MODEL_UNKNOWN;

  JsonArray models = document["data"].as<JsonArray>();
  if (models.isNull()) return MODEL_UNKNOWN;

  for (JsonObject model : models) {
    const char *modelId = model["id"] | "";
    if (strcmp(modelId, intendedModelId) != 0) continue;

    JsonVariant statusValue = model["status"]["value"];
    // The standalone Ministral server lists its loaded model without a
    // per-model status field; presence of the exact model ID is its readiness
    // signal. Qwen's llama-swap response must provide status.value.
    if (statusValue.isNull()) return statusRequired ? MODEL_UNKNOWN : MODEL_READY;
    const char *status = statusValue | "";
    return strcmp(status, "loaded") == 0 ? MODEL_READY : MODEL_OFFLINE;
  }

  return MODEL_OFFLINE;
}

// Fetch system metrics from compute01
void fetchSystemMetrics() {
  // Reset every cycle so a failed request cannot leave a stale RUNNING state.
  qwen35Status = MODEL_UNKNOWN;
  ministralStatus = MODEL_UNKNOWN;

  if (WiFi.status() != WL_CONNECTED) {
    qwen35Status = MODEL_OFFLINE;
    ministralStatus = MODEL_OFFLINE;
    systemDataChanged = true;
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_REQUEST_TIMEOUT_MS);

  // /health only proves the service is alive. Require model-list evidence.
  String healthUrl = "http://";
  healthUrl += COMPUTE01_HOST_STR;
  healthUrl += ":11439/v1/models";
  http.begin(healthUrl);
  int httpCode = http.GET();

  if (httpCode >= 200 && httpCode < 300) {
    qwen35Status = modelStatusFromResponse(http.getString(), QWEN_MODEL_ID, true);
  } else if (httpCode == 404) {
    qwen35Status = MODEL_OFFLINE;
  }
  http.end();

  // Fetch Ministral status
  HTTPClient http2;
  http2.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http2.setTimeout(HTTP_REQUEST_TIMEOUT_MS);
  String ministralUrl = "http://";
  ministralUrl += COMPUTE01_HOST_STR;
  ministralUrl += ":11438/v1/models";
  http2.begin(ministralUrl);
  httpCode = http2.GET();

  if (httpCode >= 200 && httpCode < 300) {
    ministralStatus = modelStatusFromResponse(http2.getString(), MINISTRAL_MODEL_ID, false);
  } else if (httpCode == 404) {
    ministralStatus = MODEL_OFFLINE;
  }
  http2.end();

  // For RAM/Disk, we would need compute01 to expose these metrics
  // Currently using placeholder values - in production,
  // connect to compute01's Prometheus on port 9090
  ramUsage = 65.5;  // Placeholder - would come from Prometheus
  diskUsage = 72.3; // Placeholder - would come from Prometheus

  // GPU temperatures would come from DCGM/Prometheus
  // Placeholder values
  gtx1650Temp = 45.2;
  p40Temp = 52.8;

  lastSystemUpdate = millis();
  systemDataChanged = true;
}

// ============================================================================
// Touch Handling
// ============================================================================

void handleTouch() {
  const bool currentlyTouched = ts.touched();

  // A release arms the next distinct press. No action is taken on release.
  if (!currentlyTouched) {
    if (isTouched) {
      isTouched = false;
      lastReleaseTime = millis();
    }
    return;
  }

  // Ignore every poll while the same finger remains down.
  if (isTouched) return;

  uint16_t rawX = 0;
  uint16_t rawY = 0;
  if (!ts.getPoint(rawX, rawY)) return;

  // CST820 reports native portrait coordinates; map them to the landscape TFT.
  int touchX = map(rawY, 0, 320, 0, SCREEN_WIDTH);
  int touchY = map(rawX, 0, 240, SCREEN_HEIGHT, 0);
  touchX = constrain(touchX, 0, SCREEN_WIDTH - 1);
  touchY = constrain(touchY, 0, SCREEN_HEIGHT - 1);

  const unsigned long now = millis();
  const unsigned long sinceRelease = now - lastReleaseTime;
  const bool secondPress = lastReleaseTime != 0 &&
                           sinceRelease >= TOUCH_DEBOUNCE_MS &&
                           sinceRelease <= DOUBLE_TAP_WINDOW_MS &&
                           abs(touchX - lastPressX) < 20 &&
                           abs(touchY - lastPressY) < 20;

  isTouched = true;
  lastTouchX = touchX;
  lastTouchY = touchY;
  lastPressX = touchX;
  lastPressY = touchY;
  lastTapTime = now;

  if (secondPress) {
    currentPanel = 0;
    lastPanelUpdate = now;
    Serial.println("Double-tap: Switching to Clock panel");
    return;
  }

  currentPanel = (currentPanel + 1) % 3;
  lastPanelUpdate = now;
  Serial.print("Touch: Switching to panel ");
  Serial.println(currentPanel);
}

// ============================================================================
// Display Task (FreeRTOS)
// ============================================================================

void displayTask(void* parameter) {
  int lastPanel = -1;
  unsigned long lastUpdate = millis();
  unsigned long lastContextClockUpdate = 0;

  while (true) {
    // Handle touch
    handleTouch();

    // Update panel display at ~30fps
    if (millis() - lastUpdate >= 33) {
      lastUpdate = millis();

      // Redraw when selected, and whenever the selected panel's data changes.
      // NetworkTask marks the data dirty after each successful refresh.
      bool panelNeedsRedraw = currentPanel != lastPanel;
      if (currentPanel == 1 && weatherDataChanged) {
        weatherDataChanged = false;
        panelNeedsRedraw = true;
      } else if (currentPanel == 2 && systemDataChanged) {
        systemDataChanged = false;
        panelNeedsRedraw = true;
      }

      const unsigned long now = millis();
      if (panelNeedsRedraw) {
        switch (currentPanel) {
          case 0:
            drawClockPanel();
            break;
          case 1:
            drawWeatherPanel();
            break;
          case 2:
            drawSystemPanel();
            break;
        }
        lastPanel = currentPanel;
        lastContextClockUpdate = now;
      } else if (currentPanel == 0) {
        // Clock content changes frequently, so keep its existing redraw behavior.
        drawClockPanel();
      } else if (now - lastContextClockUpdate >= 1000) {
        // Refresh only the small context clock once per second; preserve the
        // weather/system data and avoid redrawing the entire panel.
        drawContextClock(currentPanel == 1 ? 0x0019 : 0x0010);
        lastContextClockUpdate = now;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============================================================================
// Network Task (FreeRTOS)
// ============================================================================

void networkTask(void* parameter) {
  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 60) {
      Serial.println("\nWiFi connection failed!");
      // Try to continue anyway
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
  } else {
    Serial.println("\nWiFi unavailable");
  }
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Fetch initial time and only report success when the system clock is set.
  const bool initialTimeSynchronized = fetchTime();
  if (initialTimeSynchronized) {
    Serial.println("Time synchronized!");
  } else {
    Serial.println("Time synchronization failed; HTTPS weather disabled");
  }

  // Fetch both panels immediately after boot instead of waiting for their
  // normal five-minute/30-second polling intervals.
  if (initialTimeSynchronized) fetchWeather();
  fetchSystemMetrics();

  // Main network loop. Timestamps start after the initial requests complete.
  unsigned long lastWeatherFetch = millis();
  unsigned long lastSystemFetch = millis();

  while (true) {
    // Update time periodically (every hour). A newly successful sync also
    // triggers the first weather request when boot synchronization failed.
    if (!timeSynchronized || millis() - lastTimeSync >= 3600000) {
      if (fetchTime()) {
        fetchWeather();
        lastWeatherFetch = millis();
      }
    }

    // Fetch weather every 5 minutes
    if (millis() - lastWeatherFetch >= WEATHER_UPDATE_INTERVAL) {
      fetchWeather();
      lastWeatherFetch = millis();
    }

    // Fetch system metrics every 30 seconds
    if (millis() - lastSystemFetch >= SYSTEM_UPDATE_INTERVAL) {
      fetchSystemMetrics();
      lastSystemFetch = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Multi-Panel Display ===");
  Serial.println("Initializing...");

  // Initialize TFT
  tft.init();
  tft.setRotation(1);  // Landscape: 320x240 framebuffer
  tft.fillScreen(0x0000);

  Serial.println("TFT initialized");

  // Initialize the CST820 capacitive touch controller over I2C.
  if (ts.begin()) {
    Serial.println("CST820 touch initialized (I2C SDA=33 SCL=32 RST=25)");
  } else {
    Serial.println("CST820 touch controller not detected (no I2C acknowledgement at 0x15)");
  }

  weatherMutex = xSemaphoreCreateMutex();
  if (weatherMutex == nullptr) Serial.println("Weather mutex creation failed");

  // Create FreeRTOS tasks
  xTaskCreate(displayTask, "Display", 4096, NULL, 2, &displayTaskHandle);
  xTaskCreate(networkTask, "Network", 4096, NULL, 1, &networkTaskHandle);

  Serial.println("Tasks created. Starting FreeRTOS...");
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
  // Nothing here - FreeRTOS handles all tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}
