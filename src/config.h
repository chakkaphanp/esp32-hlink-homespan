#pragma once

 

// ===== WiFi Configuration =====

#define WIFI_SSID     "Portico"
#define WIFI_PASSWORD "asdf1234"

 

// ===== HomeSpan Configuration =====

#define HOMEKIT_NAME       "HLINK AC"
#define HOMEKIT_SETUP_CODE "11122333"  // HomeKit pairing code (format: XXX-XX-XXX)

 

// ===== LCD Pins (Waveshare ESP32-C6-LCD-1.47, ST7789V3) =====

#define LCD_MOSI_PIN   6
#define LCD_SCLK_PIN   7
#define LCD_CS_PIN     14
#define LCD_DC_PIN     15
#define LCD_RST_PIN    21
#define LCD_BL_PIN     22
#define LCD_WIDTH      172
#define LCD_HEIGHT     320

#define BOOT_BUTTON_PIN       9   // Onboard BOOT button
#define HOMESPAN_CONTROL_PIN  10  // Unused GPIO to redirect HomeSpan's control button



// ===== BME280 I2C Pins =====

#define BME280_SDA_PIN  4
#define BME280_SCL_PIN  5
#define BME280_ADDR     0x76  // or 0x77 depending on your module

// ===== BME280 Calibration Offsets =====
#define BME280_TEMP_OFFSET  -0.7f  // Calibration offset for temperature
#define BME280_HUMID_OFFSET 19.8f  // Calibration offset for relative humidity

 

// ===== HLINK AC UART Pins (9600 baud, 8-O-1) =====

#define HLINK_TX_PIN   16
#define HLINK_RX_PIN   17
#define HLINK_BAUD     9600

 

// ===== AC Temperature Limits =====

#define AC_TEMP_MIN 10.0f
#define AC_TEMP_MAX 32.0f

 

// ===== Polling Intervals (ms) =====

#define AC_POLL_INTERVAL_MS    5000
#define BME_READ_INTERVAL_MS   10000
#define DISPLAY_UPDATE_MS      1000

// ===== NTP / Time Configuration =====
#define NTP_SERVER             "pool.ntp.org"
#define GMT_OFFSET_SEC         (7 * 3600)      // GMT +7 (user's timezone)
#define DAYLIGHT_OFFSET_SEC    0