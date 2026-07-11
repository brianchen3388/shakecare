#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include "shakecare_shared.h"

const uint8_t I2C_SDA = 11;
const uint8_t I2C_SCL = 12;
const uint8_t QMI_ADDR = 0x6B;
const float ACC_SCALE_4G = 4.0f / 32768.0f;

const uint16_t SAMPLE_MS = 40;
const float IMPACT_DELTA_G = 1.35f;
const float IMPACT_MAG_G = 2.5f;
const float SEVERE_IMPACT_DELTA_G = 3.5f;
const float SEVERE_IMPACT_MAG_G = 4.5f;
const float ACCEL_ACTIVITY_G = 3.5f;
const float ACCEL_ACTIVITY_DECAY = 0.86f;
const float SHAKE_DELTA_G = 0.65f;
const uint8_t SHAKE_HITS_REQUIRED = 4;
const uint16_t SHAKE_ARM_MS = 1200;
const uint16_t SHAKE_WINDOW_MS = 2000;
const float STILL_DELTA_G = 0.045f;
const uint16_t STILL_ALERT_MS = 10000;
const uint16_t WARNING_CLEAR_MS = 15000;
const uint16_t ESPNOW_SEND_MS = 150;

// Replace this with the caregiver hub STA MAC printed by caregiver_hub.
const uint8_t CAREGIVER_MAC[] = {0xA0, 0xF2, 0x62, 0xEB, 0x27, 0x88};

const uint8_t QMI_WHOAMI = 0x00;
const uint8_t QMI_CTRL1 = 0x02;
const uint8_t QMI_CTRL2 = 0x03;
const uint8_t QMI_CTRL5 = 0x06;
const uint8_t QMI_CTRL7 = 0x08;
const uint8_t QMI_AX_L = 0x35;
const uint8_t QMI_RESET = 0x60;

Adafruit_NeoPixel pixels(SHAKECARE_LED_COUNT, SHAKECARE_LED_PIN, NEO_RGB + NEO_KHZ800);

CareState state = STATE_NORMAL;
CareState shownState = (CareState)255;
bool imuReady = false;
bool havePrevAccel = false;
bool espNowReady = false;

float prevAx = 0.0f;
float prevAy = 0.0f;
float prevAz = 1.0f;
float accelActivity = 0.0f;
uint32_t lastSampleMs = 0;
uint32_t warningStartMs = 0;
uint32_t stillStartMs = 0;
uint32_t shakeWindowStartMs = 0;
uint32_t lastEspNowSendMs = 0;
uint32_t espNowSeq = 0;
uint8_t shakeHits = 0;

void writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  Wire.requestFrom(QMI_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

bool readRegs(uint8_t reg, uint8_t *data, uint8_t len) {
  Wire.beginTransmission(QMI_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(QMI_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) data[i] = Wire.read();
  return true;
}

bool qmiBegin() {
  Wire.begin(I2C_SDA, I2C_SCL);
  writeReg(QMI_RESET, 0xB0);
  delay(30);
  writeReg(QMI_CTRL1, 0x40);
  if (readReg(QMI_WHOAMI) != 0x05) return false;
  writeReg(QMI_CTRL2, 0x16);
  writeReg(QMI_CTRL5, 0x01);
  writeReg(QMI_CTRL7, 0x01);
  return true;
}

bool readAccel(float &x, float &y, float &z) {
  uint8_t raw[6];
  if (!readRegs(QMI_AX_L, raw, sizeof(raw))) return false;
  int16_t ax = (int16_t)((raw[1] << 8) | raw[0]);
  int16_t ay = (int16_t)((raw[3] << 8) | raw[2]);
  int16_t az = (int16_t)((raw[5] << 8) | raw[4]);
  x = ax * ACC_SCALE_4G;
  y = ay * ACC_SCALE_4G;
  z = az * ACC_SCALE_4G;
  return true;
}

bool isUnsetMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0x00) return false;
  }
  return true;
}

bool espNowBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(SHAKECARE_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (isUnsetMac(CAREGIVER_MAC)) return false;
  if (esp_now_init() != ESP_OK) return false;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, CAREGIVER_MAC, 6);
  peer.channel = SHAKECARE_ESPNOW_CHANNEL;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void sendEspNowState(uint32_t now) {
  if (!espNowReady || now - lastEspNowSendMs < ESPNOW_SEND_MS) return;
  lastEspNowSendMs = now;

  ShakeCareMessage msg = {SHAKECARE_ESPNOW_MSG_STATE, (uint8_t)state, espNowSeq++};
  esp_now_send(CAREGIVER_MAC, (uint8_t *)&msg, sizeof(msg));
}

void showState(CareState next) {
  if (shownState == next) return;
  shownState = next;

  uint32_t color = pixels.Color(0, 80, 0);
  if (next == STATE_WARNING) color = pixels.Color(90, 55, 0);
  if (next == STATE_ALERT) color = pixels.Color(100, 0, 0);

  pixels.setBrightness(SHAKECARE_BRIGHTNESS);
  for (uint8_t row = 0; row < SHAKECARE_HEIGHT; row++) {
    for (uint8_t col = 0; col < SHAKECARE_WIDTH; col++) {
      pixels.setPixelColor(shakeCarePixelIndex(row, col), color);
    }
  }
  pixels.show();
}

void enterWarning(uint32_t now) {
  state = STATE_WARNING;
  accelActivity = 0.0f;
  warningStartMs = now;
  stillStartMs = now;
  shakeWindowStartMs = now;
  shakeHits = 0;
  showState(state);
}

void enterAlert() {
  state = STATE_ALERT;
  accelActivity = 0.0f;
  showState(state);
}

void enterNormal() {
  state = STATE_NORMAL;
  accelActivity = 0.0f;
  shakeHits = 0;
  showState(state);
}

void updateCareState(float ax, float ay, float az, uint32_t now) {
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float delta = 0.0f;
  if (havePrevAccel) {
    float dx = ax - prevAx;
    float dy = ay - prevAy;
    float dz = az - prevAz;
    delta = sqrtf(dx * dx + dy * dy + dz * dz);
  }

  prevAx = ax;
  prevAy = ay;
  prevAz = az;
  havePrevAccel = true;

  if (state == STATE_ALERT) return;

  if (state == STATE_NORMAL) {
    accelActivity = accelActivity * ACCEL_ACTIVITY_DECAY + max(0.0f, delta - STILL_DELTA_G);
    if (accelActivity > ACCEL_ACTIVITY_G && (delta > SEVERE_IMPACT_DELTA_G || mag > SEVERE_IMPACT_MAG_G)) {
      enterAlert();
    } else if (accelActivity > ACCEL_ACTIVITY_G && (delta > IMPACT_DELTA_G || mag > IMPACT_MAG_G)) {
      enterWarning(now);
    }
    return;
  }

  if (delta < STILL_DELTA_G) {
    if (now - stillStartMs >= STILL_ALERT_MS) {
      enterAlert();
      return;
    }
  } else {
    stillStartMs = now;
  }

  if (now - warningStartMs < SHAKE_ARM_MS) {
    shakeWindowStartMs = now;
    shakeHits = 0;
  } else if (delta > SHAKE_DELTA_G) {
    if (now - shakeWindowStartMs > SHAKE_WINDOW_MS) {
      shakeWindowStartMs = now;
      shakeHits = 0;
    }
    shakeHits++;
    if (shakeHits >= SHAKE_HITS_REQUIRED) {
      enterAlert();
      return;
    }
  }

  if (now - warningStartMs > WARNING_CLEAR_MS) {
    enterNormal();
  }
}

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.clear();
  pixels.show();

  espNowReady = espNowBegin();
  Serial.print("Elder STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println(espNowReady ? "ESP-NOW unicast ready" : "Set CAREGIVER_MAC first");

  imuReady = qmiBegin();
  if (imuReady) {
    enterNormal();
    Serial.println("ShakeCare elder ready");
  } else {
    enterAlert();
    Serial.println("QMI8658 not found");
  }
}

void loop() {
  uint32_t now = millis();
  if (!imuReady) {
    sendEspNowState(now);
    return;
  }
  if (now - lastSampleMs < SAMPLE_MS) {
    sendEspNowState(now);
    return;
  }
  lastSampleMs = now;

  float ax, ay, az;
  if (readAccel(ax, ay, az)) {
    updateCareState(ax, ay, az, now);
  } else {
    enterAlert();
  }
  sendEspNowState(now);
}
