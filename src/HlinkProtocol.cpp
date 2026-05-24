#include "HlinkProtocol.h"

#include <Arduino.h>

 

void HlinkProtocol::begin(HardwareSerial &serial, int txPin, int rxPin, uint32_t baud) {

  _serial = &serial;
  // ESP32-C6 UART1: 9600 baud, 8 data bits, odd parity, 1 stop bit
  _serial->begin(baud, SERIAL_8O1, rxPin, txPin);
  Serial.printf("[HLINK] UART started: TX=%d RX=%d baud=%u\n", txPin, rxPin, baud);

}

 

bool HlinkProtocol::canSendNow() const {
  return (millis() - _lastFrameMs) >= HLINK_MIN_INTERVAL_MS;
}

 

void HlinkProtocol::flushRx() {
  while (_serial->available()) {
    _serial->read();
  }
}

 

// Build and send an HLINK frame
// MT P=XXXX C=XXXX\r          (read request, no data)
// ST P=XXXX,DDDD C=XXXX\r     (write request, with data)
void HlinkProtocol::sendFrame(const char *type, uint16_t address, const uint8_t *data, size_t dataLen) {
  flushRx();

  uint16_t checksum = 0xFFFF - (address >> 8) - (address & 0xFF);
  if (data) {
    for (size_t i = 0; i < dataLen; i++) {
      checksum -= data[i];
    }
  }

  char msg[48];
  if (data && dataLen > 0) {
    // Build hex data string
    char hexData[dataLen * 2 + 1];
    for (size_t i = 0; i < dataLen; i++) {
      sprintf(&hexData[i * 2], "%02X", data[i]);
    }
    hexData[dataLen * 2] = '\0';
    sprintf(msg, "%s P=%04X,%s C=%04X\r", type, address, hexData, checksum);
  } else {
    sprintf(msg, "%s P=%04X C=%04X\r", type, address, checksum);
  }

  _serial->print(msg);
  _lastFrameMs = millis();
  Serial.printf("[HLINK] TX: %s\n", msg);

  // Update stats
  _stats.txFrames++;
  size_t len = strlen(msg);
  if (len > 0 && msg[len - 1] == '\r') {
    msg[len - 1] = '\0';
  }
  strncpy(_stats.lastTxStr, msg, sizeof(_stats.lastTxStr) - 1);
  _stats.lastTxStr[sizeof(_stats.lastTxStr) - 1] = '\0';
}

 

// Read and parse response frame
// Response format: "OK P=DDDD C=XXXX\r" or "NG P=DDDD C=XXXX\r" or "OK\r"
HlinkResponse HlinkProtocol::readFrame() {
  HlinkResponse resp;
  resp.status = HlinkStatus::NONE; 

  char buf[HLINK_BUF_SIZE];
  uint8_t idx = 0;
  uint32_t startMs = millis(); 

  while ((millis() - startMs) < HLINK_RESPONSE_TIMEOUT_MS) {
    while (_serial->available()) {
      if (idx >= HLINK_BUF_SIZE - 1) {
        Serial.println("[HLINK] RX buffer overflow");
        resp.status = HlinkStatus::INVALID;
        _stats.rxErrors++;
        strncpy(_stats.lastRxStr, "ERR: OVERFLOW", sizeof(_stats.lastRxStr) - 1);
        _stats.lastRxStr[sizeof(_stats.lastRxStr) - 1] = '\0';
        return resp;
      }
      char c = _serial->read();
      buf[idx] = c;
      if (c == ASCII_CR) {
        buf[idx] = '\0';
        goto parse;
      }
      idx++;
    }

    delay(1);
  }

 

  if (idx == 0) {
    Serial.println("[HLINK] RX timeout - no data");
    _stats.rxErrors++;
    strncpy(_stats.lastRxStr, "ERR: TIMEOUT", sizeof(_stats.lastRxStr) - 1);
    _stats.lastRxStr[sizeof(_stats.lastRxStr) - 1] = '\0';
    return resp; // NONE
  }

  buf[idx] = '\0';
  Serial.printf("[HLINK] RX timeout - partial: %s\n", buf);
  resp.status = HlinkStatus::PARTIAL;
  _stats.rxErrors++;
  strncpy(_stats.lastRxStr, "ERR: PARTIAL", sizeof(_stats.lastRxStr) - 1);
  _stats.lastRxStr[sizeof(_stats.lastRxStr) - 1] = '\0';
  return resp;
 

parse:
  Serial.printf("[HLINK] RX: %s\n", buf);
  _lastFrameMs = millis(); 

  _stats.rxFrames++;
  strncpy(_stats.lastRxStr, buf, sizeof(_stats.lastRxStr) - 1);
  _stats.lastRxStr[sizeof(_stats.lastRxStr) - 1] = '\0';

  // Tokenize: "OK P=XXXX C=XXXX" or "NG P=XXXX C=XXXX" or "OK"
  // Find first space
  char *tokens[4];
  int tokenCount = 0;
  char *p = buf;
 
  while (*p && tokenCount < 4) {
    tokens[tokenCount++] = p;
    while (*p && *p != ' ') p++;
    if (*p == ' ') {
      *p = '\0';
      p++;
    }
  } 

  if (tokenCount == 0) {
    resp.status = HlinkStatus::INVALID;
    _stats.rxErrors++;
    return resp;
  }

 

  // Check OK/NG
  bool isOK = (strcmp(tokens[0], "OK") == 0);
  bool isNG = (strcmp(tokens[0], "NG") == 0); 

  if (!isOK && !isNG) {
    resp.status = HlinkStatus::INVALID;
    _stats.rxErrors++;
    return resp;
  }

 

  if (tokenCount == 1 && isOK) {
    // Simple ACK: "OK"
    resp.status = HlinkStatus::OK;
    return resp;
  } 

  if (tokenCount != 3) {
    resp.status = HlinkStatus::INVALID;
    _stats.rxErrors++;
    return resp;
  }

 

  resp.status = isOK ? HlinkStatus::OK : HlinkStatus::NG;
 
  // Parse P= value (skip "P=" prefix)
  char *pVal = tokens[1];
  if (pVal[0] == 'P' && pVal[1] == '=') {
    pVal += 2;
  } 

  // Parse hex bytes and compute checksum
  size_t pLen = strlen(pVal);
  if (pLen >= 2 && pLen % 2 == 0) {
    uint16_t calcCheck = 0xFFFF;
    uint16_t val = 0;
    for (size_t i = 0; i < pLen; i += 2) {
      char hex[3] = { pVal[i], pVal[i+1], '\0' };
      uint8_t byte = (uint8_t)strtoul(hex, nullptr, 16);
      calcCheck -= byte;
      val = (val << 8) | byte;
    }
    resp.value = val;
    resp.hasValue = true;
 
    // Validate checksum (skip "C=" prefix)
    char *cVal = tokens[2];
    if (cVal[0] == 'C' && cVal[1] == '=') {
      cVal += 2;
    }

    uint16_t rxCheck = (uint16_t)strtoul(cVal, nullptr, 16);
    if (rxCheck != calcCheck) {
      Serial.printf("[HLINK] Checksum mismatch: got %04X expected %04X\n", rxCheck, calcCheck);
      resp.status = HlinkStatus::INVALID;
      _stats.rxErrors++;
    }
  } 

  return resp;
}

 

HlinkResponse HlinkProtocol::readFeature(uint16_t address) {
  if (!canSendNow()) {
    delay(HLINK_MIN_INTERVAL_MS);
  }
  sendFrame("MT", address);
  return readFrame();
}

 

HlinkResponse HlinkProtocol::writeFeature8(uint16_t address, uint8_t value) {

  if (!canSendNow()) {

    delay(HLINK_MIN_INTERVAL_MS);

  }

  sendFrame("ST", address, &value, 1);

  return readFrame();

}

 

HlinkResponse HlinkProtocol::writeFeature16(uint16_t address, uint16_t value) {

  if (!canSendNow()) {

    delay(HLINK_MIN_INTERVAL_MS);

  }

  uint8_t data[2] = { (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };

  sendFrame("ST", address, data, 2);

  return readFrame();

}

 

// Non-blocking poll that reads one feature per call

// Returns true when a full poll cycle completes

bool HlinkProtocol::pollStatus(HlinkACState &state) {

  if (!canSendNow()) return false;

 

  HlinkResponse resp;

 

  switch (_pollStep) {

    case 0: { // Power

      resp = readFeature(FEAT_POWER_STATE);

      if (resp.status == HlinkStatus::OK && resp.hasValue) {

        state.powerOn = (resp.value != 0);

        Serial.printf("[HLINK] Power: %s\n", state.powerOn ? "ON" : "OFF");

      }

      _pollStep = 1;

      return false;

    }

    case 1: { // Mode

      resp = readFeature(FEAT_MODE);

      if (resp.status == HlinkStatus::OK && resp.hasValue) {

        state.hlinkMode = resp.value;

        Serial.printf("[HLINK] Mode: 0x%04X\n", state.hlinkMode);

      }

      _pollStep = 2;

      return false;

    }

    case 2: { // Target temperature

      resp = readFeature(FEAT_TARGET_TEMP);

      if (resp.status == HlinkStatus::OK && resp.hasValue) {

        if (resp.value >= 10 && resp.value <= 32) {

          state.targetTemp = (float)resp.value;

        }

        Serial.printf("[HLINK] Target temp: %.0f\n", state.targetTemp);

      }

      _pollStep = 3;

      return false;

    }

    case 3: { // Current indoor temperature

      resp = readFeature(FEAT_CURRENT_TEMP);

      if (resp.status == HlinkStatus::OK && resp.hasValue) {

        state.currentTemp = (float)resp.value;

        Serial.printf("[HLINK] Current temp: %.0f\n", state.currentTemp);

      }

      _pollStep = 4;

      return false;

    }

    case 4: { // Fan mode

      resp = readFeature(FEAT_FAN_MODE);

      if (resp.status == HlinkStatus::OK && resp.hasValue) {

        state.fanSpeed = (uint8_t)resp.value;

        Serial.printf("[HLINK] Fan: %u\n", state.fanSpeed);

      }

      _pollStep = 0;

      state.valid = true;

      return true; // Full cycle done

    }

  }

 

  _pollStep = 0;

  return false;

}

 

bool HlinkProtocol::setPower(bool on) {

  HlinkResponse resp = writeFeature8(FEAT_POWER_STATE, on ? 0x01 : 0x00);

  Serial.printf("[HLINK] Set power %s -> %s\n", on ? "ON" : "OFF",

                resp.status == HlinkStatus::OK ? "OK" : "FAIL");

  return resp.status == HlinkStatus::OK;

}

 

bool HlinkProtocol::setMode(uint16_t hlinkMode) {

  HlinkResponse resp = writeFeature16(FEAT_MODE, hlinkMode);

  Serial.printf("[HLINK] Set mode 0x%04X -> %s\n", hlinkMode,

                resp.status == HlinkStatus::OK ? "OK" : "FAIL");

  return resp.status == HlinkStatus::OK;

}

 

bool HlinkProtocol::setFanSpeed(uint8_t fanSpeed) {

  HlinkResponse resp = writeFeature8(FEAT_FAN_MODE, fanSpeed);

  Serial.printf("[HLINK] Set fan %u -> %s\n", fanSpeed,

                resp.status == HlinkStatus::OK ? "OK" : "FAIL");

  return resp.status == HlinkStatus::OK;

}

 

bool HlinkProtocol::setTargetTemp(float temp) {

  uint16_t t = (uint16_t)constrain(temp, 10.0f, 32.0f);

  HlinkResponse resp = writeFeature16(FEAT_TARGET_TEMP, t);

  Serial.printf("[HLINK] Set target temp %u -> %s\n", t,

                resp.status == HlinkStatus::OK ? "OK" : "FAIL");

  return resp.status == HlinkStatus::OK;

}