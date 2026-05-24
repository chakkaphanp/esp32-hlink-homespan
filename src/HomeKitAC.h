
#include <HomeSpan.h>
#include "HlinkProtocol.h"
#include "config.h"
#include <ir_Hitachi.h>

// Forward declaration
extern HlinkProtocol hlink;
extern HlinkACState  acState;
extern bool useIRMode;
extern IRHitachiAc264 irac;

 

// ================================================================

// HeaterCooler Service - controls the AC unit via HLINK protocol

// Characteristics:

//   Active (on/off)

//   CurrentHeaterCoolerState (inactive/idle/heating/cooling)

//   TargetHeaterCoolerState (auto/heat/cool)

//   CurrentTemperature (read from AC indoor sensor)

//   CoolingThresholdTemperature (target temp in cooling mode)

//   HeatingThresholdTemperature (target temp in heating mode)

//   RotationSpeed (fan speed: 0=auto, 25=quiet, 50=low, 75=med, 100=high)

// ================================================================

struct AC_HeaterCooler : Service::HeaterCooler {

 

  SpanCharacteristic *active;

  SpanCharacteristic *currentState;

  SpanCharacteristic *targetState;

  SpanCharacteristic *currentTemp;

  SpanCharacteristic *coolThreshold;

  SpanCharacteristic *heatThreshold;

  SpanCharacteristic *rotationSpeed;

 

  AC_HeaterCooler() : Service::HeaterCooler() {

    active         = new Characteristic::Active(0);

    currentState   = new Characteristic::CurrentHeaterCoolerState(0);  // 0=Inactive

    targetState    = new Characteristic::TargetHeaterCoolerState(0);   // 0=Auto, 1=Heat, 2=Cool

    currentTemp    = new Characteristic::CurrentTemperature(20.0);

    coolThreshold  = new Characteristic::CoolingThresholdTemperature(24.0);

    heatThreshold  = new Characteristic::HeatingThresholdTemperature(22.0);

    rotationSpeed  = new Characteristic::RotationSpeed(0);

 

    currentTemp->setRange(0, 50, 1);

    coolThreshold->setRange(AC_TEMP_MIN, AC_TEMP_MAX, 1);

    heatThreshold->setRange(AC_TEMP_MIN, AC_TEMP_MAX, 1);

    rotationSpeed->setRange(0, 100, 25);

 

    Serial.println("[HomeKit] HeaterCooler service created");

  }

 

  boolean update() override {
    if (useIRMode) {
      // 1. Synchronize acState with HomeKit characteristics
      acState.powerOn = active->updated() ? active->getNewVal() : active->getVal();
      
      int targetMode = targetState->updated() ? targetState->getNewVal() : targetState->getVal();
      switch (targetMode) {
        case 0: acState.hlinkMode = HLINK_MODE_COOL; break; // Auto -> Cool
        case 1: acState.hlinkMode = HLINK_MODE_HEAT; break; // Heat
        case 2: acState.hlinkMode = HLINK_MODE_COOL; break; // Cool
        default: acState.hlinkMode = HLINK_MODE_COOL; break;
      }
      
      if (acState.hlinkMode == HLINK_MODE_HEAT) {
        acState.targetTemp = heatThreshold->updated() ? heatThreshold->getNewVal<float>() : heatThreshold->getVal<float>();
      } else {
        acState.targetTemp = coolThreshold->updated() ? coolThreshold->getNewVal<float>() : coolThreshold->getVal<float>();
      }
      
      int fanSpeedPercent = rotationSpeed->updated() ? rotationSpeed->getNewVal() : rotationSpeed->getVal();
      if (fanSpeedPercent <= 0)       acState.fanSpeed = HLINK_FAN_AUTO;
      else if (fanSpeedPercent <= 25) acState.fanSpeed = HLINK_FAN_QUIET;
      else if (fanSpeedPercent <= 50) acState.fanSpeed = HLINK_FAN_LOW;
      else if (fanSpeedPercent <= 75) acState.fanSpeed = HLINK_FAN_MEDIUM;
      else                            acState.fanSpeed = HLINK_FAN_HIGH;
      
      acState.valid = true;

      // 2. Set up IR sender
      irac.setPower(acState.powerOn);
      
      uint8_t irMode;
      switch (acState.hlinkMode) {
        case HLINK_MODE_HEAT: irMode = kHitachiAc264Heat; break;
        case HLINK_MODE_COOL: irMode = kHitachiAc264Cool; break;
        case HLINK_MODE_DRY:  irMode = kHitachiAc264Dry;  break;
        case HLINK_MODE_FAN:  irMode = kHitachiAc264Fan;  break;
        default:              irMode = kHitachiAc264Cool; break;
      }
      irac.setMode(irMode);
      
      irac.setTemp((uint8_t)acState.targetTemp);
      
      uint8_t irFan;
      switch (acState.fanSpeed) {
        case HLINK_FAN_QUIET:  irFan = kHitachiAc264FanMin;    break; // Min
        case HLINK_FAN_LOW:    irFan = kHitachiAc264FanMin;    break; // Min (Low is same)
        case HLINK_FAN_MEDIUM: irFan = kHitachiAc264FanMedium; break;
        case HLINK_FAN_HIGH:   irFan = kHitachiAc264FanHigh;   break;
        case HLINK_FAN_AUTO:
        default:               irFan = kHitachiAc264FanAuto;   break;
      }
      irac.setFan(irFan);

      // 3. Transmit the IR command
      irac.send();
      Serial.printf("[IR] Sent Hitachi AC264 command: Power=%s, Mode=%u (IR Mode=%u), Temp=%.0f C, FanSpeed=%u\n",
                    acState.powerOn ? "ON" : "OFF", acState.hlinkMode, irMode, acState.targetTemp, irFan);
      
      return true;
    }

    // --- Active (power on/off) ---

    if (active->updated()) {

      bool on = active->getNewVal();

      Serial.printf("[HomeKit] Active -> %s\n", on ? "ON" : "OFF");

      hlink.setPower(on);

      if (!on) {

        return true;  // When turning off, no need to set mode

      }

    }

 

    // --- Target state (mode) ---

    if (targetState->updated()) {

      int state = targetState->getNewVal();

      uint16_t hlinkMode;

      switch (state) {

        case 0: hlinkMode = HLINK_MODE_AUTO; break; // Auto

        case 1: hlinkMode = HLINK_MODE_HEAT; break; // Heat

        case 2: hlinkMode = HLINK_MODE_COOL; break; // Cool

        default: hlinkMode = HLINK_MODE_AUTO; break;

      }

      Serial.printf("[HomeKit] TargetState -> %d (HLINK: 0x%04X)\n", state, hlinkMode);

 

      // Ensure power is on when setting mode

      if (!acState.powerOn) {

        hlink.setPower(true);

      }

      hlink.setMode(hlinkMode);

    }

 

    // --- Cooling threshold temperature ---

    if (coolThreshold->updated()) {

      float temp = coolThreshold->getNewVal<float>();

      Serial.printf("[HomeKit] CoolThreshold -> %.1f\n", temp);

      hlink.setTargetTemp(temp);

    }

 

    // --- Heating threshold temperature ---

    if (heatThreshold->updated()) {

      float temp = heatThreshold->getNewVal<float>();

      Serial.printf("[HomeKit] HeatThreshold -> %.1f\n", temp);

      hlink.setTargetTemp(temp);

    }

 

    // --- Fan speed ---

    if (rotationSpeed->updated()) {

      int speed = rotationSpeed->getNewVal();

      uint8_t hlinkFan;

      if (speed <= 0)       hlinkFan = HLINK_FAN_AUTO;

      else if (speed <= 25) hlinkFan = HLINK_FAN_QUIET;

      else if (speed <= 50) hlinkFan = HLINK_FAN_LOW;

      else if (speed <= 75) hlinkFan = HLINK_FAN_MEDIUM;

      else                  hlinkFan = HLINK_FAN_HIGH;

      Serial.printf("[HomeKit] RotationSpeed -> %d%% (HLINK fan: %u)\n", speed, hlinkFan);

      hlink.setFanSpeed(hlinkFan);

    }

 

    return true;

  }

 

  // Called periodically to push AC state to HomeKit

  void refreshFromAC() {

    if (!acState.valid) return;

 

    // Power

    int isActive = acState.powerOn ? 1 : 0;

    if (active->getVal() != isActive) {

      active->setVal(isActive);

    }

 

    // Current temperature

    if (acState.currentTemp > 0 && currentTemp->getVal<float>() != acState.currentTemp) {

      currentTemp->setVal(acState.currentTemp);

    }

 

    // Target temperature

    float tgt = acState.targetTemp;

    if (coolThreshold->getVal<float>() != tgt) {

      coolThreshold->setVal(tgt);

    }

    if (heatThreshold->getVal<float>() != tgt) {

      heatThreshold->setVal(tgt);

    }

 

    // Target state (mode)

    int tState = 0; // Auto

    if (!acState.powerOn) {

      // Don't change target state when off

    } else if (acState.hlinkMode == HLINK_MODE_HEAT || acState.hlinkMode == HLINK_MODE_HEAT_AUTO) {

      tState = 1; // Heat

    } else if (acState.hlinkMode == HLINK_MODE_COOL || acState.hlinkMode == HLINK_MODE_COOL_AUTO) {

      tState = 2; // Cool

    } else {

      tState = 0; // Auto

    }

    if (targetState->getVal() != tState) {

      targetState->setVal(tState);

    }

 

    // Current heater-cooler state

    int cState;

    if (!acState.powerOn) {

      cState = 0; // Inactive

    } else if (acState.hlinkMode == HLINK_MODE_HEAT || acState.hlinkMode == HLINK_MODE_HEAT_AUTO) {

      cState = 2; // Heating

    } else if (acState.hlinkMode == HLINK_MODE_COOL || acState.hlinkMode == HLINK_MODE_COOL_AUTO) {

      cState = 3; // Cooling

    } else {

      cState = 1; // Idle

    }

    if (currentState->getVal() != cState) {

      currentState->setVal(cState);

    }

 

    // Fan speed

    int speed;

    switch (acState.fanSpeed) {

      case HLINK_FAN_QUIET:  speed = 25;  break;

      case HLINK_FAN_LOW:    speed = 50;  break;

      case HLINK_FAN_MEDIUM: speed = 75;  break;

      case HLINK_FAN_HIGH:   speed = 100; break;

      default:               speed = 0;   break; // Auto

    }

    if (rotationSpeed->getVal() != speed) {

      rotationSpeed->setVal(speed);

    }

  }

};

 

// ================================================================

// Temperature Sensor (BME280 room temperature)

// ================================================================

struct RoomTemperature : Service::TemperatureSensor {

  SpanCharacteristic *temp;

  float lastTemp = 20.0f;

 

  RoomTemperature() : Service::TemperatureSensor() {

    temp = new Characteristic::CurrentTemperature(20.0);

    temp->setRange(-40, 85, 0.1);

    Serial.println("[HomeKit] TemperatureSensor service created");

  }

 

  void setTemperature(float t) {

    if (lastTemp != t) {

      lastTemp = t;

      temp->setVal(t);

      Serial.printf("[HomeKit] Room temp -> %.1f C\n", t);

    }

  }

};

 

// ================================================================

// Humidity Sensor (BME280 room humidity)

// ================================================================

struct RoomHumidity : Service::HumiditySensor {

  SpanCharacteristic *hum;

  float lastHum = 50.0f;

 

  RoomHumidity() : Service::HumiditySensor() {

    hum = new Characteristic::CurrentRelativeHumidity(50.0);

    Serial.println("[HomeKit] HumiditySensor service created");

  }

 

  void setHumidity(float h) {

    if (lastHum != h) {

      lastHum = h;

      hum->setVal(h);

      Serial.printf("[HomeKit] Room humidity -> %.1f %%\n", h);

    }

  }

};