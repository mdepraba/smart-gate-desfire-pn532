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

WiFiClient wifiClient;
PubSubClient client(wifiClient);
Desfire gi_PN532;
Gate gate(TRIG_PIN, ECHO_PIN, SERVO_PIN);

#if USE_AES
  AES gi_PiccMasterKey;
#else
  DES gi_PiccMasterKey;
#endif

struct kCard
{
  byte     u8_UidLength;   // UID = 4 or 7 bytes
  byte     u8_KeyVersion;  // for Desfire random ID cards
  bool      b_PN532_Error; // true -> the error comes from the PN532, false -> crypto error
  eCardType e_CardType;    
};

uint64_t   gu64_LastID       = 0; 
bool gb_InitReaderSuccess = false;

void publishStatus();
void callback(char* topic, byte* payload, unsigned int length);
void InitReader();

#define DIST_THRESHOLD 10


// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Smart Gate System ===");

  gi_PN532.InitHardwareSPI(PN532_SS, PN532_RST);
  InitReader();
  gate.begin();
  gate.setMode(AUTO);
  Serial.println("Gate system initialized");
}

void loop() {
  if (!gb_InitReaderSuccess) {
    InitReader();
    return;
  }

  if (gate.getMode() == AUTO) {
    uint16_t dist = gate.getDistance();
    if (gate.isObjectPassed(DIST_THRESHOLD, dist)) {
      gate.commandGate(CLOSED);
    }
  }

}
// ===========================================================================


void InitReader() {
  Serial.println("Initializing PN532...");
  gb_InitReaderSuccess = false;

  gi_PN532.begin();
  byte IC, VerHi, VerLo, Flags;

  if (!gi_PN532.GetFirmwareVersion(&IC, &VerHi, &VerLo, &Flags)) {
    Serial.println("PN532 not responding ❌");
    return;
  }

  Serial.printf("PN532 found (Chip: PN5%02X, FW: %d.%d)\n", IC, VerHi, VerLo);
  gi_PN532.SamConfig();
  gb_InitReaderSuccess = true;
  Serial.println("PN532 ready ✅");
}