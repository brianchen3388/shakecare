#pragma once

#include <Arduino.h>

const uint8_t SHAKECARE_LED_PIN = 14;
const uint8_t SHAKECARE_WIDTH = 8;
const uint8_t SHAKECARE_HEIGHT = 8;
const uint16_t SHAKECARE_LED_COUNT = SHAKECARE_WIDTH * SHAKECARE_HEIGHT;
const uint8_t SHAKECARE_BRIGHTNESS = 20;

const uint8_t SHAKECARE_ESPNOW_CHANNEL = 1;
const uint8_t SHAKECARE_ESPNOW_MSG_STATE = 1;

enum CareState : uint8_t {
  STATE_NORMAL,
  STATE_WARNING,
  STATE_ALERT
};

struct ShakeCareMessage {
  uint8_t type;
  uint8_t state;
  uint32_t seq;
};

inline uint16_t shakeCarePixelIndex(uint8_t row, uint8_t col) {
  return (SHAKECARE_WIDTH - 1 - col) * SHAKECARE_WIDTH + row;
}

inline const char *shakeCareStateKey(uint8_t state) {
  if (state == STATE_NORMAL) return "normal";
  if (state == STATE_ALERT) return "alert";
  return "warning";
}

inline const char *shakeCareStateText(uint8_t state) {
  if (state == STATE_NORMAL) return "NORMAL";
  if (state == STATE_ALERT) return "ALERT";
  return "WARNING";
}
