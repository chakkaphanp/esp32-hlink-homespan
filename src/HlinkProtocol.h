#pragma once

 

#include <HardwareSerial.h>

#include <functional>

 

// ===== HLINK Protocol Constants =====

constexpr uint8_t HLINK_BUF_SIZE = 64;

constexpr uint8_t ASCII_CR = 0x0D;

constexpr uint32_t HLINK_MIN_INTERVAL_MS = 60;

constexpr uint32_t HLINK_RESPONSE_TIMEOUT_MS = 500;

 

// Feature addresses

enum HlinkFeature : uint16_t {

  FEAT_POWER_STATE     = 0x0000,

  FEAT_MODE            = 0x0001,

  FEAT_FAN_MODE        = 0x0002,

  FEAT_TARGET_TEMP     = 0x0003,

  FEAT_CURRENT_TEMP    = 0x0100,

  FEAT_OUTDOOR_TEMP    = 0x0102,

  FEAT_SWING_MODE      = 0x0014,

  FEAT_REMOTE_LOCK     = 0x0006,

  FEAT_BEEPER          = 0x0800,

  FEAT_MODEL_NAME      = 0x0900,

};

 

// HLINK mode values

constexpr uint16_t HLINK_MODE_HEAT      = 0x0010;

constexpr uint16_t HLINK_MODE_HEAT_AUTO = 0x8010;

constexpr uint16_t HLINK_MODE_COOL      = 0x0040;

constexpr uint16_t HLINK_MODE_COOL_AUTO = 0x8040;

constexpr uint16_t HLINK_MODE_DRY       = 0x0020;

constexpr uint16_t HLINK_MODE_DRY_AUTO  = 0x8020;

constexpr uint16_t HLINK_MODE_FAN       = 0x0050;

constexpr uint16_t HLINK_MODE_AUTO      = 0x8000;

 

// Fan speed values

constexpr uint8_t HLINK_FAN_AUTO   = 0x00;

constexpr uint8_t HLINK_FAN_HIGH   = 0x01;

constexpr uint8_t HLINK_FAN_MEDIUM = 0x02;

constexpr uint8_t HLINK_FAN_LOW    = 0x03;

constexpr uint8_t HLINK_FAN_QUIET  = 0x04;

 

// Response status

enum class HlinkStatus { NONE, PARTIAL, OK, NG, INVALID };

 

struct HlinkResponse {

  HlinkStatus status = HlinkStatus::NONE;

  uint16_t    value  = 0;

  bool        hasValue = false;

};

 

// Simplified AC state

struct HlinkACState {

  bool     powerOn       = false;

  uint16_t hlinkMode     = HLINK_MODE_COOL;

  uint8_t  fanSpeed      = HLINK_FAN_AUTO;

  float    targetTemp    = 24.0f;

  float    currentTemp   = 0.0f;

  bool     valid         = false;  // true once we've read all features at least once

};

 

class HlinkProtocol {

public:

  void begin(HardwareSerial &serial, int txPin, int rxPin, uint32_t baud);

 

  // Read a single feature (blocking, with timeout)

  HlinkResponse readFeature(uint16_t address);

 

  // Write a feature value

  HlinkResponse writeFeature8(uint16_t address, uint8_t value);

  HlinkResponse writeFeature16(uint16_t address, uint16_t value);

 

  // Poll all AC status features (non-blocking, call repeatedly)

  bool pollStatus(HlinkACState &state);

 

  // Control commands

  bool setPower(bool on);

  bool setMode(uint16_t hlinkMode);

  bool setFanSpeed(uint8_t fanSpeed);

  bool setTargetTemp(float temp);

 

  // Timing

  bool canSendNow() const;

 

private:

  HardwareSerial *_serial = nullptr;

  uint32_t _lastFrameMs = 0;

 

  // Internal poll state machine

  uint8_t _pollStep = 0;

  uint32_t _pollStartMs = 0;

 

  void sendFrame(const char *type, uint16_t address, const uint8_t *data = nullptr, size_t dataLen = 0);

  HlinkResponse readFrame();

  void flushRx();

};