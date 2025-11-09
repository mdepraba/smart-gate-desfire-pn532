#include <Arduino.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <PN532.h>
#include <Desfire.h>
#include <DesfireService.h>
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

// Desfire nfc;
DesfireService nfc(SECRET_PICC_MASTER_KEY, CARD_KEY_VERSION);
Gate gate;
Connection conn(mqttConfig, gate);

uint64_t  gu64_LastID       = 0; 
bool      cardAuthenticated = false;
#define DIST_THRESHOLD 10

#define LED_PIN 2

void handleMqttMessage(const String& topic, const String& message);

// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========== Smart Gate System ==========");
  
  nfc.begin(PN532_SS, PN532_RST);
  gate.begin(TRIG_PIN, ECHO_PIN, SERVO_PIN);
  gate.setMode(AUTO);
  conn.begin();
  conn.setMessageHandler(handleMqttMessage);

  Serial.println("[OK] Gate system initialized");
}

void loop() {
  if(!conn.isConnected()) {
    conn.reconnect();
  }
  conn.loop();

  byte uid[8] = {0};
  byte uidLength = 0;
  eCardType cardType;

  if (gate.getMode() == AUTO) {
    uint16_t dist = gate.getDistance();
    if (gate.isObjectPassed(dist)) {
      gate.commandGate(CLOSED);
    }
  }

  if (!nfc.desfireReader.ReadPassiveTargetID(uid, &uidLength, &cardType)) return;
  if (uidLength == 0) return;

  nfc.authenticatePiccMaster();
  nfc.authenticateApp(CARD_APPLICATION_ID);
  nfc.readDesfireFile(CARD_FILE_ID, 32, READ_ACCESS_INDEX, SECRET_FILE_READ_ACCESS);
  
  conn.publishStatus();
  delay(1000);
}
// ===========================================================================

void handleMqttMessage(const String& topic, const String& message) {
  Serial.printf("Received message on topic: %s\n", topic.c_str());
  // Serial.print("Message: ");
  // Serial.println(message);
  JsonDocument doc;
  if (deserializeJson(doc, message)) {
    Serial.println("[ERROR] Invalid JSON");
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