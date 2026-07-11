#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "shakecare_shared.h"
#include "dashboard_page.h"

const char *AP_SSID = "ShakeCare-Caregiver";
const char *AP_PASS = "12345678";
const uint16_t ESPNOW_TIMEOUT_MS = 1000;

Adafruit_NeoPixel pixels(SHAKECARE_LED_COUNT, SHAKECARE_LED_PIN, NEO_RGB + NEO_KHZ800);
WebServer server(80);

CareState shownState = (CareState)255;
volatile uint8_t remoteState = STATE_WARNING;
volatile uint32_t lastRemoteMs = 0;
volatile uint32_t lastSeq = 0;
volatile bool haveRemote = false;
bool espNowReady = false;

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

void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(ShakeCareMessage)) return;

  ShakeCareMessage msg;
  memcpy(&msg, data, sizeof(msg));
  if (msg.type != SHAKECARE_ESPNOW_MSG_STATE || msg.state > STATE_ALERT) return;

  remoteState = msg.state;
  lastSeq = msg.seq;
  lastRemoteMs = millis();
  haveRemote = true;
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  uint32_t seenMs = lastRemoteMs;
  bool online = haveRemote && millis() - seenMs <= ESPNOW_TIMEOUT_MS;
  uint8_t state = online ? remoteState : STATE_WARNING;
  int32_t age = haveRemote ? (int32_t)(millis() - seenMs) : -1;

  char json[260];
  snprintf(json, sizeof(json),
           "{\"online\":%s,\"state\":%u,\"stateKey\":\"%s\",\"stateText\":\"%s\",\"ageMs\":%ld,\"seq\":%lu,\"uptimeMs\":%lu,\"clients\":%u}",
           online ? "true" : "false", state, shakeCareStateKey(state), shakeCareStateText(state), (long)age,
           (unsigned long)lastSeq, (unsigned long)millis(), WiFi.softAPgetStationNum());
  server.send(200, "application/json", json);
}

bool radioBegin() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS, SHAKECARE_ESPNOW_CHANNEL);
  esp_wifi_set_channel(SHAKECARE_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(onEspNowRecv);
  return true;
}

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.clear();
  pixels.show();
  showState(STATE_WARNING);

  espNowReady = radioBegin();
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();

  Serial.print("Caregiver STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Caregiver AP: ");
  Serial.print(AP_SSID);
  Serial.println(" / 12345678 / http://192.168.4.1");
  Serial.println(espNowReady ? "ESP-NOW receiver ready" : "ESP-NOW failed");
}

void loop() {
  server.handleClient();

  uint32_t seenMs = lastRemoteMs;
  bool online = haveRemote && millis() - seenMs <= ESPNOW_TIMEOUT_MS;
  showState(online ? (CareState)remoteState : STATE_WARNING);
}
