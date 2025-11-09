#include "DesfireService.h"

DesfireService::DesfireService(const byte* key, byte version) 
: CardVersion(version) {
    if (key != nullptr) {
        memcpy(PICCMasterKey, key, sizeof(PICCMasterKey));
    }
}

bool DesfireService::begin(uint8_t PN532_SS, uint8_t PN532_RST) {
    initSuccess = false;
    byte IC, VerHi, VerLo, Flags;

    desfireReader.InitHardwareSPI(PN532_SS, PN532_RST);
    desfireReader.begin();
    if (!desfireReader.GetFirmwareVersion(&IC, &VerHi, &VerLo, &Flags)) {
        Serial.println("[ERROR] PN532 not responding");
        return false;
    }

    Serial.printf("[OK] PN532 Found (Chip: PN5%02X, FW: %d.%d)\n", IC, VerHi, VerLo);
    desfireReader.SamConfig();
    initSuccess = true;
    Serial.println("[OK] PN532 ready");
    return true;
}

bool DesfireService::authenticatePiccMaster() {
    #if USE_AES
        PICCKeyCipher.SetKeyData(PICCMasterKey, sizeof(PICCMasterKey), CardVersion);
    #else
        PICCKeyCipher.SetKeyData(PICCMasterKey, 8, CardVersion);
    #endif

    // select root (PICC) application
    if(!desfireReader.SelectApplication(0x000000)) {
        Serial.println("[ERROR] Select PICC application failed");
        return false;
    }

    if(!desfireReader.Authenticate(0, &PICCKeyCipher)) {
        Serial.println("[ERROR] PICC Master Key authentication failed");
        return false;
    }
    Serial.println("[OK] PICC Master Key authentication successful");
    return true;
}

bool DesfireService::authenticateApp(const uint32_t AppId) {
    memcpy(AppMasterKey, AppMasterKey, sizeof(AppMasterKey));
    #if USE_AES
        AppKeyCipher.SetKeyData(AppMasterKey, sizeof(AppMasterKey), CardVersion);
    #else
        AppKeyCipher.SetKeyData(AppMasterKey, 8, CardVersion);
    #endif

    if(!desfireReader.SelectApplication(AppId)) {
        Serial.println("[ERROR] Select application failed");
        return false;
    }
    if(!desfireReader.Authenticate(0, &AppKeyCipher)) {
        Serial.println("[ERROR] Application Key authentication failed");
        return false;
    }

    Serial.printf("[OK] Application 0x%06X Key authentication successful\n", AppId);
    return true;
}

String DesfireService::readDesfireFile(uint8_t fileId, uint16_t length) {
    if (!desfireReader.ReadFileData(fileId, 0, length, buf)) {
        Serial.println("[ERROR] Read File Data failed");
        return String("");
    }

    Serial.printf("[OK] File %d Data (%d bytes): ", fileId, length);
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", buf[i]);
    }
    Utils::HexBufToAsciiBuf(buf, length, out, sizeof(out));
    String strData = String(out);
    Serial.println(strData);
    return strData;
}

String DesfireService::readDesfireFile(uint8_t fileId, uint16_t length, uint8_t keyIndex, const uint8_t* keyData) {
    size_t keyDataLen = USE_AES ? 16 : 8;

    if (!authenticateWithIndex(keyIndex, keyData, keyDataLen)) {
        Serial.printf("[ERROR] Authentication with key index %d failed\n", keyIndex);
        return String("");
    }
    if (!desfireReader.ReadFileData(fileId, 0, length, buf)) {
        Serial.println("[ERROR] Read File Data failed");
        return String("");
    }

    Serial.printf("[OK] File %d Data (%d bytes): ", fileId, length);
    for (int i = 0; i < length; i++) {
        Serial.printf("%02X ", buf[i]);
    }
    Utils::HexBufToAsciiBuf(buf, length, out, sizeof(out));
    String strData = String(out);
    return strData;
}

bool DesfireService::authenticateWithIndex(uint8_t keyIndex, const uint8_t* keyData, size_t keyLen) {
    keyIndexCipher.SetKeyData(keyData, keyLen, CardVersion);
    Serial.printf("[INFO] Authenticating key index %d...\n", keyIndex);
    if (!desfireReader.Authenticate(keyIndex, &keyIndexCipher)) {
        Serial.printf("[ERROR] Authenticate key %d FAILED\n", keyIndex);
        return false;
    }
    Serial.printf("[OK] Authenticate key %d OK\n", keyIndex);
    return true;
}
