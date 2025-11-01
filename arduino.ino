#include "PN532.h"
#include "Desfire.h"
#include "Secrets.h"
#include "Buffer.h"
#include "Utils.h"

// ================== CONFIG ====================
#define PN532_SS   5
#define PN532_RST  21
#define LED_PIN    2
#define USE_AES    true   // true = AES, false = 3DES
// ==============================================

Desfire gi_PN532;
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

  // wait for a card to enter the field
  if (!gi_PN532.ReadPassiveTargetID(uid, &uidLength, &cardType)) return;
  if (uidLength == 0) return;     // no card found

  Serial.print("\n[+] Card detected, UID: ");
  for (int i = 0; i < uidLength; i++) Serial.printf("%02X ", uid[i]);
  Serial.println();

  // Authenticate PICC master key
  if (AuthenticateDesfire(&gi_PiccMasterKey)) {
    Serial.println("[OK] Authentication successful ✅");
    Utils::BlinkLed(LED_PIN, HIGH);
  } else {
    Serial.println("[FAIL] Authentication failed ❌");
    BlinkError();
  }


  gi_PN532.SelectApplication(CARD_APPLICATION_ID);
  ReadFileWithAutoAuth(CARD_APPLICATION_ID, 4, 32);
  // AES appKey;
  // appKey.SetKeyData(SECRET_APPLICATION_KEY, sizeof(SECRET_APPLICATION_KEY), CARD_KEY_VERSION);
  // if (!AuthenticateApplication(&appKey)) return;

  // if (!AuthenticateKey3()) return;
  // ReadDesfireFile(2, 32);
  // DumpFileSettings(2);
  // DumpFileSettings(3);
  // ReadDesfireFile(CARD_FILE_ID, 16);
  // ReadDesfireFile(3, 32);

  delay(500); // delay antar pembacaan
  digitalWrite(LED_PIN, LOW);
}

// ======================================================
// Authenticate PICC master key (application 0x000000, key 0)
// ======================================================
bool AuthenticateDesfire(DESFireKey* key) {
  if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID)) {
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


bool ReadDesfireFile(uint8_t fileId, uint16_t length) {
  byte data[64] = {0};
  Serial.printf("[INFO] Reading file ID %d ...\n", fileId);

  if (!gi_PN532.ReadFileData(fileId, 0, length, data)) {
    Serial.println("ReadFileData failed ❌");
    return false;
  }

  Serial.printf("[OK] File %d Data (%d bytes): ", fileId, length);
  for (int i = 0; i < length; i++) Serial.printf("%02X ", data[i]);
  Serial.println();
  return true;
}


bool AuthenticateApplication(DESFireKey* appKey) {
  Serial.println("[INFO] Authenticating to Application...");
  if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID)) {
    Serial.println("SelectApplication failed ❌");
    return false;
  }
  if (!gi_PN532.Authenticate(0, appKey)) {
    Serial.println("App Authentication failed ❌");
    return false;
  }
  Serial.println("[OK] App Authentication successful ✅");
  return true;
}


void DumpFileSettings(uint8_t fileId) {
  DESFireFileSettings settings;
  if (gi_PN532.GetFileSettings(fileId, &settings)) {
    Serial.printf("[File %d] CommMode: %d, ReadAccess: %d, WriteAccess: %d\n",
                  fileId, settings.u32_FileSize, settings.k_Permis.e_ReadAccess, settings.k_Permis.e_WriteAccess);
  } else {
    Serial.println("GetFileSettings failed ❌");
  }
}

// helper: authenticate given key index using AES or DES depending on USE_AES
bool AuthenticateWithIndex(uint8_t keyIndex, const uint8_t* keyData, size_t keyLen) {
  #if USE_AES
    AES key;
  #else
    DES key;
  #endif
    key.SetKeyData(keyData, keyLen, CARD_KEY_VERSION);
    Serial.printf("[INFO] Authenticating key index %d...\n", keyIndex);
    if (!gi_PN532.Authenticate(keyIndex, &key)) {
      Serial.printf("[WARN] Authenticate key %d FAILED\n", keyIndex);
      return false;
    }
    Serial.printf("[OK] Authenticate key %d OK\n", keyIndex);
    return true;
  }
  
  // higher-level: try read file, if auth error -> try key3 (default) then read again
  bool ReadFileWithAutoAuth(uint32_t appId, uint8_t fileId, uint16_t length) {
    byte buf[128] = {0};
    char out[64];
  
    // 1) ensure app selected
    if (!gi_PN532.SelectApplication(appId)) {
      Serial.println("SelectApplication(app) failed");
      return false;
    }
  
    // 2) try reading directly (if file set AR_FREE)
    if (gi_PN532.ReadFileData(fileId, 0, length, buf)) {
      Serial.println("[OK] Read without extra auth:");
      for (int i=0;i<length;i++) Serial.printf("%02X ", buf[i]);
      Serial.println();
      return true;
    }
  
    // 3) read failed: try authenticate with app key0 first (if you have it)
    Serial.println("[INFO] Read failed, attempting app-key 0 authentication...");
  #if USE_AES
    AES appKey;
    appKey.SetKeyData(SECRET_APPLICATION_KEY, sizeof(SECRET_APPLICATION_KEY), CARD_KEY_VERSION);
  #else
    DES appKey;
    appKey.SetKeyData(SECRET_APPLICATION_KEY, sizeof(SECRET_APPLICATION_KEY), CARD_KEY_VERSION);
  #endif
    if (gi_PN532.Authenticate(0, &appKey)) {
      // after successful app auth try read again
      if (gi_PN532.ReadFileData(fileId, 0, length, buf)) {
        Serial.println("[OK] Read after app-key0 auth:");
        for (int i=0;i<length;i++) Serial.printf("%02X ", buf[i]);
        Serial.println();
        return true;
      }
      Serial.println("[INFO] Still failed after app-key0 auth.");
    } else {
      Serial.println("[INFO] App-key0 authenticate failed (or not allowed).");
    }
  
    // 4) file likely needs key index 3 (readAccess=3). Try key index 3 (default all-zero key)
    Serial.println("[INFO] Trying key index 3 (default zeros) ...");
  #if USE_AES
    // default AES zero key (16 bytes)
    const uint8_t zeroAES[16] = {0};
    if (!AuthenticateWithIndex(3, zeroAES, sizeof(zeroAES))) {
      Serial.println("[WARN] key3 authenticate (AES) failed");
    } else {
      if (gi_PN532.ReadFileData(fileId, 0, length, buf)) {
        Serial.println("[OK] Read after key3 auth:");
        for (int i=0;i<length;i++) Serial.printf("%02X ", buf[i]);
        Serial.println();

        Utils::HexBufToAsciiBuf(buf, sizeof(buf), out, sizeof(out));
        Serial.println(out);
        return true;
      }
      Serial.println("[WARN] ReadFileData failed even after key3 auth");
    }
  #else
    const uint8_t zeroDES[8] = {0};
    if (!AuthenticateWithIndex(3, zeroDES, sizeof(zeroDES))) {
      Serial.println("[WARN] key3 authenticate (DES) failed");
    } else {
      if (gi_PN532.ReadFileData(fileId, 0, length, buf)) {
        Serial.println("[OK] Read after key3 auth:");
        for (int i=0;i<length;i++) Serial.printf("%02X ", buf[i]);
        Serial.println();
        return true;
      }
      Serial.println("[WARN] ReadFileData failed even after key3 auth");
    }
  #endif
  
  return false;
}
  