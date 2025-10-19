#include "PN532.h"
#include "Desfire.h"
#include "Secrets.h"
#include "Buffer.h"
#include "Utils.h"

// ================== CONFIG ====================
#define PN532_SS   5
#define PN532_RST  21
#define LED_PIN    2
#define USE_AES    false   // true = AES, false = 3DES
// ==============================================

Desfire gi_PN532;
#if USE_AES
  AES gi_PiccMasterKey;
#else
  DES gi_PiccMasterKey;
#endif

bool gb_InitSuccess = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 + PN532 (DESFire Reader) ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  gi_PN532.InitHardwareSPI(PN532_SS, PN532_RST);
  InitReader();

  // Set PICC Master Key (default 16-byte 0x00..00)
  gi_PiccMasterKey.SetKeyData(SECRET_PICC_MASTER_KEY, sizeof(SECRET_PICC_MASTER_KEY), CARD_KEY_VERSION);
}

void loop() {
  if (!gb_InitSuccess) {
    InitReader();
    return;
  }

  byte uid[8];
  byte uidLength;
  eCardType cardType;

  // Coba baca kartu
  if (!gi_PN532.ReadPassiveTargetID(uid, &uidLength, &cardType)) {
    delay(100);
    return;
  }

  // Tidak ada kartu
  if (uidLength == 0) return;

  Serial.print("\n[+] Card detected, UID: ");
  for (int i = 0; i < uidLength; i++) Serial.printf("%02X ", uid[i]);
  Serial.println();

  // Coba autentikasi dengan master key
  if (AuthenticateDesfire(&gi_PiccMasterKey)) {
    Serial.println("[OK] Authentication successful ✅");
    digitalWrite(LED_PIN, HIGH);
    delay(500);
  } else {
    Serial.println("[FAIL] Authentication failed ❌");
    BlinkError();
  }

  digitalWrite(LED_PIN, LOW);
  delay(500); // delay antar pembacaan
}

// ======================================================
// Authenticate PICC master key (application 0x000000, key 0)
// ======================================================
bool AuthenticateDesfire(DESFireKey* key) {
  if (!gi_PN532.SelectApplication(0x000000)) {
      Serial.println("SelectApplication failed");
      return false;
  }
  if (!gi_PN532.Authenticate(0, key)) {
      Serial.println("Desfire authentication failed");
      return false;
  }

  return true;
}


// ======================================================
void BlinkError() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}

// ======================================================
void InitReader() {
  Serial.println("Initializing PN532...");
  gb_InitSuccess = false;

  gi_PN532.begin();
  byte IC, VerHi, VerLo, Flags;

  if (!gi_PN532.GetFirmwareVersion(&IC, &VerHi, &VerLo, &Flags)) {
    Serial.println("PN532 not responding ❌");
    return;
  }

  Serial.printf("PN532 found (Chip: PN5%02X, FW: %d.%d)\n", IC, VerHi, VerLo);
  gi_PN532.SamConfig();
  gb_InitSuccess = true;
  Serial.println("PN532 ready ✅");
}
