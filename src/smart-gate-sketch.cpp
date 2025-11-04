#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <PN532.h>
#include <Desfire.h>
#include <Buffer.h>
#include <Utils.h>
#include "Secrets.h"
#include "Config.h"

WiFiClient wifiClient;
PubSubClient client(wifiClient);