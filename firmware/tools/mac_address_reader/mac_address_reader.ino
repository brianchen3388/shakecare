#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(200);

  Serial.print("STA MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
}
