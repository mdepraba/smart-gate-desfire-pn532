#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <PN532.h>
#include <Desfire.h>
#include <Buffer.h>
#include <Utils.h>
#include <Gate.h>
#include <Connection.h>
#include "Secrets.h"
#include "Config.h"

#if USE_AES
  AES gi_PiccMasterKey;
#else
  DES gi_PiccMasterKey;
#endif

struct kCard
{
  byte      u8_UidLength;   // UID = 4 or 7 bytes
  byte      u8_KeyVersion;  // for Desfire random ID cards
  bool      b_PN532_Error;  // true -> the error comes from the PN532, false -> crypto error
  eCardType e_CardType;    
};

MqttConfig mqttConfig = {
  .wifi_ssid      = ssid,
  .wifi_password  = password,
  .server         = mqtt_server,
  .port           = mqtt_port,
  .username       = mqtt_user,
  .password       = mqtt_pass,
  .topics = {
    .status   = topic_status,
    .control  = topic_control,
    .rfid     = topic_rfid
  }
};

Desfire gi_PN532;
Gate gate;
Connection conn(mqttConfig, gate);

uint64_t   gu64_LastID       = 0; 
bool gb_InitReaderSuccess = false;

#define DIST_THRESHOLD 10

void handleMqttMessage(const String& topic, const String& message);

// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Smart Gate System ===");

  gi_PN532.InitHardwareSPI(PN532_SS, PN532_RST);
  // InitReader();
  gate.begin(TRIG_PIN, ECHO_PIN, SERVO_PIN);
  gate.setMode(AUTO);
  conn.begin();
  conn.setMessageHandler(handleMqttMessage);

  Serial.println("Gate system initialized");
}

void loop() {
  if (!gb_InitReaderSuccess) {
    // InitReader();
    return;
  }
  conn.reconnect();
  conn.loop();

  if (gate.getMode() == AUTO) {
    uint16_t dist = gate.getDistance();
    if (gate.isObjectPassed(dist)) {
      gate.commandGate(CLOSED);
    }
  }

}
// ===========================================================================

void handleMqttMessage(const String& topic, const String& message) {
  Serial.printf("Received message on topic: %s\n", topic.c_str());
  // Serial.print("Message: ");
  // Serial.println(message);
  JsonDocument doc;
  if (deserializeJson(doc, message)) {
    Serial.println("Invalid JSON");
    return;
  }

  if (topic == mqttConfig.topics.control) {
    if (doc.containsKey("servo")) {
      uint8_t command = doc["servo"];
      gate.commandGate(command == 1 ? OPEN : CLOSED);
      Serial.println(command == 1 ? "Gate opened via MQTT" : "Gate closed via MQTT");
      conn.publishStatus();
    }
    if (doc.containsKey("auto_mode")) {
      String mode = doc["auto_mode"];
      gate.setMode(mode == "manual" ? MANUAL : AUTO);
      Serial.println(mode == "manual" ? "Set to MANUAL mode via MQTT" : "Set to AUTO mode via MQTT");
      conn.publishStatus();
    }
    if (doc.containsKey("threshold")) {
      uint16_t newThreshold = doc["threshold"];
      gate.setThreshold(newThreshold);
      Serial.printf("Threshold updated to %d cm\n", newThreshold);
      conn.publishStatus();
    }
    if (doc.containsKey("access_granted")) {
      bool accessGranted = doc["access_granted"];
      if (accessGranted) {
        Serial.println("Access granted via MQTT");
        gate.commandGate(OPEN);
        conn.publishStatus();
      } else {
        Serial.println("Access denied via MQTT");
      }
    }
    if (doc.containsKey("ping")) {
      Serial.println("Ping received, publishing status");
      conn.publishStatus();
    }
  }
}