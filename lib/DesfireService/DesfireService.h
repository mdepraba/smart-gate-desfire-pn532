#ifndef DESFIRE_SERVICE_H
#define DESFIRE_SERVICE_H
#include <Arduino.h>
#include <Desfire.h>
#include <DesFireKey.h>
#include <Utils.h>


#define USE_AES    true 

class DesfireService {
public:
    DesfireService(const byte* key = nullptr, const byte version = 0x00);
    bool begin(uint8_t PN532_SS, uint8_t PN532_RST);
    bool authenticatePiccMaster();
    bool authenticateApp(const uint32_t AppId);
    String readDesfireFile(uint8_t fileId, uint16_t length);
    String readDesfireFile(uint8_t fileId, uint16_t length, uint8_t keyIndex, const uint8_t* keyData);
    bool authenticateWithIndex(uint8_t keyIndex, const uint8_t* keyData, size_t keyLen);

    Desfire desfireReader;
private:
    #if USE_AES
        AES PICCKeyCipher;
        AES AppKeyCipher;
        AES keyIndexCipher;
    #else
        DES PICCKeyCipher;
        DES AppKeyCipher;
        DES keyIndexCipher;
    #endif
    bool initSuccess = false;
    byte PICCMasterKey[24] = {0x00};
    byte AppMasterKey[24] = {0x00};
    byte CardVersion = 0x00;

    byte buf[128] = {0};
    char out[64];
};

#endif