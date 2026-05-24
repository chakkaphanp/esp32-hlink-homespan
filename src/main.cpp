// HLINK AC Controller - HomeSpan (Apple HomeKit)
// Target: Waveshare ESP32-C6-LCD-1.47
// Features: AC control via HLINK, BME280 environment sensor, LCD display


#include <HomeSpan.h>
#include <time.h>
#include "config.h"
#include "HlinkProtocol.h"
#include "HomeKitAC.h"
#include "BME280Sensor.h"
#include "Display.h"

 

// ===== Global objects =====

HlinkProtocol hlink;
HlinkACState  acState;
BME280Sensor  bme;
Display       lcd;

 

// HomeKit accessory pointers (for periodic updates)
AC_HeaterCooler  *pHeaterCooler = nullptr;
RoomTemperature  *pRoomTemp     = nullptr;
RoomHumidity     *pRoomHumid    = nullptr;

 

// Timing

uint32_t lastACPoll     = 0;
uint32_t lastBMERead    = 0;
uint32_t lastDisplayUpd = 0;
BME280Data lastBME;

 

void setup() {

  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("  HLINK AC Controller - HomeSpan");
  Serial.println("  Board: ESP32-C6-LCD-1.47");
  Serial.println("========================================");
 
  // --- HLINK UART ---
  hlink.begin(Serial1, HLINK_TX_PIN, HLINK_RX_PIN, HLINK_BAUD);
 
  // --- BME280 ---
  if (!bme.begin()) {
    Serial.println("[MAIN] WARNING: BME280 not available");
  }
 
  // --- LCD ---
  if (!lcd.begin()) {
    Serial.println("[MAIN] WARNING: LCD not available");
  }

  // --- BOOT Button ---
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
 
  // --- HomeSpan ---
  homeSpan.setControlPin(HOMESPAN_CONTROL_PIN);
  homeSpan.setWifiCredentials(WIFI_SSID, WIFI_PASSWORD);
  homeSpan.begin(Category::AirConditioners, HOMEKIT_NAME, "HLINK", "HLINK-AC-1");
  homeSpan.setQRID(HOMEKIT_SETUP_CODE);

  // --- NTP Time Sync ---
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.printf("[MAIN] NTP Time Sync initialized: server=%s, offset=%d\n", NTP_SERVER, GMT_OFFSET_SEC);
 
  // Accessory 1: HLINK AC (HeaterCooler)
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("HLINK AC");
      new Characteristic::Manufacturer("Hitachi");
      new Characteristic::Model("HLINK-AC");
      new Characteristic::SerialNumber("001");
    pHeaterCooler = new AC_HeaterCooler();
 

  // Accessory 2: Room Temperature (BME280)
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("Room Temp");
      new Characteristic::Manufacturer("BME280");
      new Characteristic::SerialNumber("002");
    pRoomTemp = new RoomTemperature();
 
  // Accessory 3: Room Humidity (BME280)
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("Room Humidity");
      new Characteristic::Manufacturer("BME280");
      new Characteristic::SerialNumber("003");
    pRoomHumid = new RoomHumidity(); 

  Serial.println("[MAIN] Setup complete - waiting for HomeKit pairing");
  Serial.printf("[MAIN] Setup code: %s\n", HOMEKIT_SETUP_CODE);
}

 

void loop() {
  // HomeSpan main loop
  homeSpan.poll();
 
  uint32_t now = millis();
 
  // --- Read Boot Button to toggle screen ---
  static bool lastBtnReading = HIGH;
  static bool btnState = HIGH;
  static uint32_t lastDebounceTime = 0;
  
  bool reading = digitalRead(BOOT_BUTTON_PIN);
  if (reading != lastBtnReading) {
    lastDebounceTime = now;
  }
  lastBtnReading = reading;

  if ((now - lastDebounceTime) > 50) {
    if (reading != btnState) {
      btnState = reading;
      if (btnState == LOW) { // Button pressed (active-low)
        Serial.println("[MAIN] Boot button pressed - toggling screen");
        lcd.toggleScreen();
      }
    }
  }

  // --- Poll AC status via HLINK ---
  if (now - lastACPoll >= AC_POLL_INTERVAL_MS) {
    lastACPoll = now;
    bool cycleComplete = hlink.pollStatus(acState);
    if (cycleComplete && pHeaterCooler) {
      pHeaterCooler->refreshFromAC();
    }
  }
 
  // --- Read BME280 ---
  if (now - lastBMERead >= BME_READ_INTERVAL_MS) {
    lastBMERead = now;
    lastBME = bme.read();
    if (lastBME.valid) {
      if (pRoomTemp)  pRoomTemp->setTemperature(lastBME.temperature);
      if (pRoomHumid) pRoomHumid->setHumidity(lastBME.humidity);
    }
  }
 
  // --- Update LCD ---
  if (now - lastDisplayUpd >= DISPLAY_UPDATE_MS) {
    lastDisplayUpd = now;
    
    time_t rawTime = time(nullptr);
    struct tm timeInfo;
    bool timeValid = (rawTime > 1000000000) && localtime_r(&rawTime, &timeInfo);
    
    lcd.update(lastBME, acState, timeValid ? &timeInfo : nullptr, hlink.getStats());
  }
}