#pragma once

#include <Arduino_GFX_Library.h>
#include <time.h>
#include "config.h"
#include "BME280Sensor.h"
#include "HlinkProtocol.h"

// Colors (RGB565)
constexpr uint16_t COL_BG          = 0x0000;  // Deep black background
constexpr uint16_t COL_CARD_BORDER = 0x3186;  // Sleek dark gray card border
constexpr uint16_t COL_TEXT        = 0xFFFF;  // Crisp white
constexpr uint16_t COL_LABEL       = 0x7BEF;  // Muted gray for labels
constexpr uint16_t COL_TEMP        = 0xFD20;  // Vibrant orange for temperature
constexpr uint16_t COL_HUMID       = 0x07FF;  // Electric cyan for humidity
constexpr uint16_t COL_PRESS       = 0xF81F;  // Magenta for pressure

constexpr uint16_t COL_CLOCK       = 0x07E0;  // Electric green for clock
constexpr uint16_t COL_AC_OFF      = 0x7BEF;  // Steel gray when AC is off
constexpr uint16_t COL_AC_COOL     = 0x04FF;  // Cool blue
constexpr uint16_t COL_AC_HEAT     = 0xFBE0;  // Warm orange/red
constexpr uint16_t COL_AC_DRY      = 0xF81F;  // Magenta for dry mode
constexpr uint16_t COL_AC_FAN      = 0xFFFF;  // White for fan mode
constexpr uint16_t COL_AC_AUTO     = 0x07E0;  // Green for auto mode

class Display {

public:
  bool begin() {
    _bus = new Arduino_ESP32SPI(
      LCD_DC_PIN, LCD_CS_PIN, LCD_SCLK_PIN, LCD_MOSI_PIN, -1 /* MISO */
    );

    _gfx = new Arduino_ST7789(
      _bus, LCD_RST_PIN, 1 /* rotation=landscape */, true /* IPS */,
      172, 320,  // width, height (pre-rotation: 172x320)
      34,0,
      34,0
    );

    if (!_gfx->begin(40000000)) {  // 40 MHz SPI clock
      Serial.println("[LCD] Init failed!");
      return false;
    }

    // Backlight on
    pinMode(LCD_BL_PIN, OUTPUT);
    analogWrite(LCD_BL_PIN, 64); // Medium brightness

    _gfx->fillScreen(COL_BG);
    _gfx->setTextWrap(false);

    // After rotation=1 (landscape): 320 wide x 172 tall
    drawStaticLayout(); 

    Serial.println("[LCD] Display initialized (landscape 320x172)");
    return true;
  }

  void update(const BME280Data &data, const HlinkACState &ac, const struct tm *timeinfo) {
    // 1. Update Room Environment
    if (data.valid) {
      drawClimateValue(18, 42, data.temperature, 1, "C", COL_TEMP, _lastTemp);
      drawClimateValue(18, 82, data.humidity, 1, "%", COL_HUMID, _lastHumid);
      drawPressureValue(18, 124, data.pressure, 1, "hPa", COL_PRESS, _lastPress);
    }

    // 2. Update Clock
    if (timeinfo != nullptr) {
      if (!_timeSynced) {
        // Clear card area where "SYNCING..." might have been written
        _gfx->fillRect(168, 8, 144, 64, COL_BG);
        _timeSynced = true;
      }
      
      if (timeinfo->tm_sec != _lastSec) {
        _lastSec = timeinfo->tm_sec;
        
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
        if (timeinfo->tm_sec % 2 != 0) {
          timeStr[2] = ' '; // Blinking colon
        }
        
        _gfx->fillRect(174, 14, 132, 32, COL_BG);
        _gfx->setTextSize(4);
        _gfx->setTextColor(COL_CLOCK, COL_BG);
        _gfx->setCursor(180, 14); // Centered: 240 - 120/2
        _gfx->print(timeStr);
      }
      
      if (timeinfo->tm_mday != _lastMday) {
        _lastMday = timeinfo->tm_mday;
        
        char dateStr[20];
        strftime(dateStr, sizeof(dateStr), "%a, %b %d", timeinfo);
        
        _gfx->fillRect(174, 52, 132, 12, COL_BG);
        _gfx->setTextSize(1);
        _gfx->setTextColor(COL_LABEL, COL_BG);
        int dateLen = strlen(dateStr);
        int dateX = 240 - (dateLen * 6) / 2;
        _gfx->setCursor(dateX, 52);
        _gfx->print(dateStr);
      }
    } else {
      // Time not synced yet
      if (_timeSynced || _lastSec == -1) {
        _timeSynced = false;
        _lastSec = -1;
        _lastMday = -1;
        
        _gfx->fillRect(168, 8, 144, 64, COL_BG);
        _gfx->setTextSize(2);
        _gfx->setTextColor(COL_LABEL, COL_BG);
        _gfx->setCursor(180, 28); // Centered "SYNCING..."
        _gfx->print("SYNCING...");
      }
    }

    // 3. Update AC Status
    if (!ac.valid) {
      if (_lastAcValid || _lastAcPower /* force update on startup */) {
        _lastAcValid = false;
        // Clear AC status card body
        _gfx->fillRect(168, 100, 144, 64, COL_BG);
        
        _gfx->setTextSize(2);
        _gfx->setTextColor(COL_LABEL, COL_BG);
        _gfx->setCursor(180, 108);
        _gfx->print("CONNECTING");
        _gfx->setCursor(186, 128);
        _gfx->print("AC UNIT...");
      }
    } else {
      bool forceRedraw = !_lastAcValid;
      _lastAcValid = true;
      
      // Power state change or force redraw
      if (forceRedraw || ac.powerOn != _lastAcPower || ac.hlinkMode != _lastAcMode || 
          ac.targetTemp != _lastAcTargetTemp || ac.fanSpeed != _lastAcFan) {
        
        _lastAcPower = ac.powerOn;
        _lastAcMode = ac.hlinkMode;
        _lastAcTargetTemp = ac.targetTemp;
        _lastAcFan = ac.fanSpeed;
        
        // Clear AC values area
        _gfx->fillRect(168, 100, 144, 64, COL_BG);
        
        if (!ac.powerOn) {
          _gfx->setTextSize(2);
          _gfx->setTextColor(COL_AC_OFF, COL_BG);
          _gfx->setCursor(180, 110);
          _gfx->print("SYSTEM OFF");
          
          _gfx->setTextSize(1);
          _gfx->setTextColor(COL_LABEL, COL_BG);
          _gfx->setCursor(176, 136);
          _gfx->print("SET: -- C");
          _gfx->setCursor(176, 148);
          _gfx->print("FAN: --");
        } else {
          // AC is ON
          // Line 1: Mode
          const char* modeStr = "AUTO";
          uint16_t modeColor = COL_AC_AUTO;
          switch (ac.hlinkMode) {
            case HLINK_MODE_COOL:
            case HLINK_MODE_COOL_AUTO:
              modeStr = "COOL";
              modeColor = COL_AC_COOL;
              break;
            case HLINK_MODE_HEAT:
            case HLINK_MODE_HEAT_AUTO:
              modeStr = "HEAT";
              modeColor = COL_AC_HEAT;
              break;
            case HLINK_MODE_DRY:
            case HLINK_MODE_DRY_AUTO:
              modeStr = "DRY";
              modeColor = COL_AC_DRY;
              break;
            case HLINK_MODE_FAN:
              modeStr = "FAN";
              modeColor = COL_AC_FAN;
              break;
          }
          
          _gfx->setTextSize(2);
          _gfx->setCursor(176, 102);
          _gfx->setTextColor(COL_LABEL, COL_BG);
          _gfx->print("M: ");
          _gfx->setTextColor(modeColor, COL_BG);
          _gfx->print(modeStr);
          
          // Line 2: Target Temp
          _gfx->setCursor(176, 122);
          _gfx->setTextColor(COL_LABEL, COL_BG);
          _gfx->print("SET: ");
          _gfx->setTextColor(COL_TEMP, COL_BG);
          _gfx->print((int)ac.targetTemp);
          _gfx->print(" C");
          
          // Line 3: Fan Speed
          const char* fanStr = "AUTO";
          switch (ac.fanSpeed) {
            case HLINK_FAN_QUIET:  fanStr = "QUIET";  break;
            case HLINK_FAN_LOW:    fanStr = "LOW";    break;
            case HLINK_FAN_MEDIUM: fanStr = "MED";    break;
            case HLINK_FAN_HIGH:   fanStr = "HIGH";   break;
          }
          
          _gfx->setCursor(176, 142);
          _gfx->setTextColor(COL_LABEL, COL_BG);
          _gfx->print("FAN: ");
          _gfx->setTextColor(COL_TEXT, COL_BG);
          _gfx->print(fanStr);
        }
      }
    }
  }

private:

  Arduino_DataBus     *_bus = nullptr;
  Arduino_ST7789      *_gfx = nullptr;

  float _lastTemp  = -999.0f;
  float _lastHumid = -999.0f;
  float _lastPress = -999.0f;

  bool _timeSynced = false;
  int  _lastSec    = -1;
  int  _lastMday   = -1;

  bool     _lastAcValid      = false;
  bool     _lastAcPower      = false;
  uint16_t _lastAcMode       = 0xFFFF;
  float    _lastAcTargetTemp = -999.0f;
  uint8_t  _lastAcFan        = 0xFF;

  void drawStaticLayout() {
    // Card 1: Room Env
    _gfx->drawRoundRect(6, 6, 148, 160, 8, COL_CARD_BORDER);
    _gfx->setTextSize(1);
    _gfx->setTextColor(COL_LABEL, COL_BG);
    _gfx->setCursor(44, 14); // Centered: 6 + 148/2 - (12 chars * 6)/2 = 80 - 36 = 44.
    _gfx->print("ROOM CLIMATE");
    _gfx->drawFastHLine(14, 25, 132, COL_CARD_BORDER);

    // Static labels inside Card 1
    _gfx->setCursor(18, 30);
    _gfx->print("Temp");
    _gfx->setCursor(18, 70);
    _gfx->print("Humidity");
    _gfx->setCursor(18, 112);
    _gfx->print("Pressure");

    // Card 2: Clock
    _gfx->drawRoundRect(166, 6, 148, 68, 8, COL_CARD_BORDER);

    // Card 3: AC Status
    _gfx->drawRoundRect(166, 80, 148, 86, 8, COL_CARD_BORDER);
    _gfx->setCursor(213, 88); // Centered: 166 + 148/2 - (9 chars * 6)/2 = 240 - 27 = 213.
    _gfx->print("AC STATUS");
    _gfx->drawFastHLine(174, 98, 132, COL_CARD_BORDER);
  }

  void drawClimateValue(int x, int y, float val, int decimals, const char *unit,
                         uint16_t color, float &oldVal) {
    if (val == oldVal) return;
    oldVal = val;

    // Clear old value area
    _gfx->fillRect(x, y, 124, 24, COL_BG);

    _gfx->setTextSize(3);
    _gfx->setTextColor(color, COL_BG);
    _gfx->setCursor(x, y);
    _gfx->print(val, decimals);
    _gfx->setTextSize(2);
    _gfx->print(" ");
    _gfx->print(unit);
  }

  void drawPressureValue(int x, int y, float val, int decimals, const char *unit,
                          uint16_t color, float &oldVal) {
    if (val == oldVal) return;
    oldVal = val;

    // Clear old value area
    _gfx->fillRect(x, y, 124, 16, COL_BG);

    _gfx->setTextSize(2);
    _gfx->setTextColor(color, COL_BG);
    _gfx->setCursor(x, y);
    _gfx->print(val, decimals);
    _gfx->print(" ");
    _gfx->print(unit);
  }

};