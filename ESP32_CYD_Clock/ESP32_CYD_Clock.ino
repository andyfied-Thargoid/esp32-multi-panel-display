#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>
#define GFXFF 1
#include "config.h"

TFT_eSPI tft;

static const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";
static const char* NTP_1 = "pool.ntp.org";
static const char* NTP_2 = "time.nist.gov";
static const char* NTP_3 = "time.google.com";

void drawClock(const tm& now) {
  char timeText[12];
  char dateText[32];
  strftime(timeText, sizeof(timeText), "%H:%M", &now);
  strftime(dateText, sizeof(dateText), "%A, %d %B %Y", &now);

  tft.fillRect(0, 30, 320, 180, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setFreeFont(&FreeSerifBold24pt7b);
  tft.setTextSize(2);
  tft.drawString(timeText, 160, 82, GFXFF);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setFreeFont(&FreeSerif12pt7b);
  tft.drawString(dateText, 160, 182, GFXFF);
  tft.setTextDatum(TL_DATUM);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.invertDisplay(false);
  tft.setRotation(1); // landscape: 320x240
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setFreeFont(&FreeMono12pt7b);
  tft.drawString("Connecting to WiFi...", 160, 120, GFXFF);
  tft.setTextDatum(TL_DATUM);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi: %s\n", WIFI_SSID);
  unsigned long deadline = millis() + 20000;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    configTzTime(TZ_INFO, NTP_1, NTP_2, NTP_3);
  } else {
    Serial.println("WiFi connection failed");
  }
}

void loop() {
  static int lastMinute = -1;
  tm now{};
  if (getLocalTime(&now, 1000)) {
    if (now.tm_min != lastMinute) {
      lastMinute = now.tm_min;
      drawClock(now);
      Serial.printf("%04d-%02d-%02d %02d:%02d\n", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min);
    }
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setFreeFont(&FreeMono12pt7b);
    tft.drawString("Waiting for NTP...", 160, 120, GFXFF);
    tft.setTextDatum(TL_DATUM);
  }
  delay(1000);
}
