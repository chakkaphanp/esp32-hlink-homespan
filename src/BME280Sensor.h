#pragma once
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "config.h"

struct BME280Data {
  float temperature = 0.0f;
  float humidity    = 0.0f;
  float pressure    = 0.0f;
  bool  valid       = false;
};

 

class BME280Sensor {
public:
  bool begin() {
    Wire.begin(BME280_SDA_PIN, BME280_SCL_PIN);
    if (!_bme.begin(BME280_ADDR, &Wire)) {
      Serial.println("[BME280] Sensor not found!");
      return false;
    }

    // Weather monitoring: low-frequency polling
    _bme.setSampling(
      Adafruit_BME280::MODE_FORCED,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::FILTER_OFF,
      Adafruit_BME280::STANDBY_MS_1000
    );

    Serial.println("[BME280] Sensor initialized");
    _initialized = true;
    return true;

  }

 

  BME280Data read() {

    BME280Data data;
    if (!_initialized) return data; 

    _bme.takeForcedMeasurement();
    data.temperature = _bme.readTemperature() + BME280_TEMP_OFFSET;
    
    float rawHumidity = _bme.readHumidity() + BME280_HUMID_OFFSET;
    if (rawHumidity > 100.0f) rawHumidity = 100.0f;
    if (rawHumidity < 0.0f) rawHumidity = 0.0f;
    data.humidity    = rawHumidity;

    data.pressure    = _bme.readPressure() / 100.0f;  // hPa
    data.valid       = true;

    return data;

  }

 

private:

  Adafruit_BME280 _bme;
  bool _initialized = false;

};