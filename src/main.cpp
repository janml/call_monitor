#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <tr064.h>
#include "secrets.h"


void connectWifi() {
  // TODO: Add a timeout and return int based on success or error conditon!
  Serial.println("Connecting wifi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println("Wifi connected.");
}


void setup() {
  Serial.begin(9600);
  delay(2000);  // Need some time between flashing and reading the output.
  connectWifi();
}

void loop() {

}
