#include "PN532.h"

#define PN532_SS   5
#define PN532_RST  21

PN532* nfc;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nBooting ESP32 + PN532 SPI...");

  nfc = new PN532();
  nfc->InitHardwareSPI(PN532_SS, PN532_RST);
  nfc->begin();
  nfc->SetDebugLevel(1);

  if (!nfc->SamConfig()) {
    Serial.println("SAMConfig failed!");
    while (1);
  } else {
    Serial.println("PN532 ready.");
  }
}

void loop() {
  byte uid[8];
  byte uidLength;
  eCardType cardType;

  if (nfc->ReadPassiveTargetID(uid, &uidLength, &cardType)) {
    Serial.print("Card detected! UID: ");
    for (int i = 0; i < uidLength; i++) {
      Serial.printf("%02X ", uid[i]);
    }
    Serial.println();

    if (cardType == CARD_Desfire) Serial.println("Type: DESFire");
    else if (cardType == CARD_DesRandom) Serial.println("Type: DESFire (Random UID)");
    else Serial.println("Type: Other");
  }

  delay(1000);
}
