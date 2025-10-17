#define USE_DESFIRE   true

#if USE_DESFIRE
    #define USE_AES   false
    #define COMPILE_SELFTEST  0
    #define ALLOW_ALSO_CLASSIC   true
#endif

#define PASSWORD  ""
#define PASSWORD_TIMEOUT  5
#define DOOR_1_PIN       20
#define DOOR_2_PIN       21
#define RESET_PIN         2
#define SPI_CLK_PIN       3
#define SPI_MISO_PIN      1
#define SPI_MOSI_PIN      4
#define SPI_CS_PIN        0
#define LED_GREEN_PIN    10
#define LED_RED_PIN      12
#define VOLTAGE_MEASURE_PIN  A9
#define MAX_VOLTAGE_DROP  10
#define BUTTON_OPEN_PIN  15
#define BUTTON_OPEN_DOOR  NO_DOOR
#define ANALOG_RESOLUTION  12
#define ANALOG_REFERENCE   1.2
#define VOLTAGE_FACTOR   15.9
#define OPEN_INTERVAL   100
#define RF_OFF_INTERVAL  1000

#if defined(__MK20DX256__)
    #if !defined(USB_SERIAL)
        #error "Switch the compiler to USB Type = 'Serial'"
    #endif
    #if F_CPU != 24000000
        #error "Switch the compiler to CPU Speed = '24 MHz optimized'"
    #endif
#else
    #warning "This code has not been tested on any other board than Teensy 3.1 / 3.2"
#endif

#if USE_DESFIRE
    #if USE_AES
        #define DESFIRE_KEY_TYPE   AES
        #define DEFAULT_APP_KEY    gi_PN532.AES_DEFAULT_KEY
    #else
        #define DESFIRE_KEY_TYPE   DES
        #define DEFAULT_APP_KEY    gi_PN532.DES3_DEFAULT_KEY
    #endif
    
    #include "Desfire.h"
    #include "Secrets.h"
    #include "Buffer.h"
    Desfire          gi_PN532;
    DESFIRE_KEY_TYPE gi_PiccMasterKey;
#else
    #include "Classic.h"
    Classic          gi_PN532;
#endif

#include "UserManager.h"

#define PASSWORD_OFFSET_MS   (2 * PASSWORD_TIMEOUT * 60 * 1000)

enum eLED
{
    LED_OFF,
    LED_RED,
    LED_GREEN,
};

enum eBattCheck
{
    BATT_OK,
    BATT_OLD_RED,
    BATT_OLD_GREEN,
};

struct kCard
{
    byte     u8_UidLength;
    byte     u8_KeyVersion;
    bool      b_PN532_Error;
    eCardType e_CardType;    
};

char       gs8_CommandBuffer[500];
uint32_t   gu32_CommandPos   = 0;
uint64_t   gu64_LastPasswd   = 0;
uint64_t   gu64_LastID       = 0;
bool       gb_InitSuccess    = false;
eBattCheck ge_BattCheck      = BATT_OK;

void setup() 
{
    gs8_CommandBuffer[0] = 0;

    Utils::SetPinMode(DOOR_1_PIN, OUTPUT);  
    Utils::WritePin  (DOOR_1_PIN, LOW);      

    Utils::SetPinMode(DOOR_2_PIN, OUTPUT);  
    Utils::WritePin  (DOOR_2_PIN, LOW);      

    Utils::SetPinMode(LED_GREEN_PIN, OUTPUT);      
    Utils::SetPinMode(LED_RED_PIN,   OUTPUT);
    Utils::SetPinMode(LED_BUILTIN,   OUTPUT);

    Utils::SetPinMode(BUTTON_OPEN_PIN, INPUT_PULLUP);
    
    FlashLED(LED_GREEN, 1000);

    analogReadResolution(ANALOG_RESOLUTION);
    analogReference(INTERNAL1V2);

    gi_PN532.InitSoftwareSPI(SPI_CLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_CS_PIN, RESET_PIN);

    SerialClass::Begin(115200);

    InitReader(false);

    #if USE_DESFIRE
        gi_PiccMasterKey.SetKeyData(SECRET_PICC_MASTER_KEY, sizeof(SECRET_PICC_MASTER_KEY), CARD_KEY_VERSION);
    #endif
}

void loop()
{   
    bool b_KeyPress = ReadKeyboardInput();

    uint32_t u32_Volt = MeasureVoltage();
    bool b_VoltageOK  = (u32_Volt >= 130 && u32_Volt < 140);

    CheckOpenButton();

    uint64_t u64_StartTick = Utils::GetMillis64();

    static uint64_t u64_LastRead = 0;
    if (gb_InitSuccess)
    {
        if (b_KeyPress)
        {
            u64_LastRead = u64_StartTick + 1000;
            return;
        }

        if ((int)(u64_StartTick - u64_LastRead) < RF_OFF_INTERVAL)
            return;
    }

    do
    {
        if (!gb_InitSuccess)
        {
            InitReader(true);
            break;
        }

        kUser k_User;
        kCard k_Card;
        if (!ReadCard(k_User.ID.u8, &k_Card))
        {
            if (IsDesfireTimeout())
            {
            }
            else if (k_Card.b_PN532_Error)
            {
                InitReader(true);
            }
            else
            {
                FlashLED(LED_RED, 1000);
            }
            
            Utils::Print("> ");
            break;
        }

        if (k_Card.u8_UidLength == 0) 
        {
            gu64_LastID = 0;

            eLED e_LED;
            switch (ge_BattCheck)
            {
                case BATT_OLD_RED: 
                    e_LED = LED_RED;
                    ge_BattCheck = BATT_OLD_GREEN;
                    break;
                case BATT_OLD_GREEN: 
                    e_LED = LED_GREEN;
                    ge_BattCheck = BATT_OLD_RED;
                    break;
                default:
                    e_LED = LED_GREEN;
                    break;
            }

            if (!b_VoltageOK) e_LED = LED_RED;
            
            FlashLED(e_LED, 20);
            break;
        }

        if (gu64_LastID == k_User.ID.u64) 
            break;

        OpenDoor(k_User.ID.u64, &k_Card, u64_StartTick);
        Utils::Print("> ");
    }
    while (false);

    gi_PN532.SwitchOffRfField();

    u64_LastRead = Utils::GetMillis64();
}

void InitReader(bool b_ShowError)
{
    if (b_ShowError)
    {
        SetLED(LED_RED);
        Utils::Print("Communication Error -> Reset PN532\r\n");
    }

    do
    {
        gb_InitSuccess = false;
      
        gi_PN532.begin();
    
        byte IC, VersionHi, VersionLo, Flags;
        if (!gi_PN532.GetFirmwareVersion(&IC, &VersionHi, &VersionLo, &Flags))
            break;
    
        char Buf[80];
        sprintf(Buf, "Chip: PN5%02X, Firmware version: %d.%d\r\n", IC, VersionHi, VersionLo);
        Utils::Print(Buf);
        sprintf(Buf, "Supports ISO 14443A:%s, ISO 14443B:%s, ISO 18092:%s\r\n", (Flags & 1) ? "Yes" : "No",
                                                                                (Flags & 2) ? "Yes" : "No",
                                                                                (Flags & 4) ? "Yes" : "No");
        Utils::Print(Buf);
         
        if (!gi_PN532.SetPassiveActivationRetries())
            break;
        
        if (!gi_PN532.SamConfig())
            break;
    
        gb_InitSuccess = true;
    }
    while (false);

    if (b_ShowError)
    {
        Utils::DelayMilli(2000);        
        SetLED(LED_OFF);
        Utils::DelayMilli(100);
    }  
}

void FlashLED(eLED e_LED, int s32_Interval)
{
    SetLED(e_LED);
    Utils::DelayMilli(s32_Interval);
    SetLED(LED_OFF);
}

void SetLED(eLED e_LED)
{
    Utils::WritePin(LED_RED_PIN,   LOW);  
    Utils::WritePin(LED_GREEN_PIN, LOW);
    Utils::WritePin(LED_BUILTIN,   LOW);

    switch (e_LED)
    {
        case LED_RED:   
            Utils::WritePin(LED_RED_PIN, HIGH); 
            Utils::WritePin(LED_BUILTIN, HIGH);
            break;
        case LED_GREEN: 
            Utils::WritePin(LED_GREEN_PIN, HIGH); 
            Utils::WritePin(LED_BUILTIN,   HIGH);
            break;
        default:
            break;
    }
}

bool ReadKeyboardInput()
{
    uint64_t u64_Now = Utils::GetMillis64() + PASSWORD_OFFSET_MS;

    bool b_KeyPress = false;
    while (SerialClass::Available())
    {
        b_KeyPress = true;
        bool b_PasswordValid = PASSWORD[0] == 0 || (u64_Now - gu64_LastPasswd) < (PASSWORD_TIMEOUT * 60 * 1000);
      
        byte u8_Char = SerialClass::Read();
        char s8_Echo[] = { (char)u8_Char, 0 };        
        
        if (u8_Char == '\r' || u8_Char == '\n')
        {
            OnCommandReceived(b_PasswordValid);
            Utils::Print("\r\n> ");
            continue;
        }

        if (u8_Char == 8)
        {
            if (gu32_CommandPos > 0) 
            {
                gu32_CommandPos --;
                Utils::Print(s8_Echo);
            }
            continue;
        }

        if (u8_Char < 32 || u8_Char > 126)
            continue;

        if (b_PasswordValid) Utils::Print(s8_Echo);
        else                 Utils::Print("*");
        
        if (gu32_CommandPos >= sizeof(gs8_CommandBuffer))
        {
            Utils::Print("ERROR: Command too long\r\n");
            gu32_CommandPos = 0;
        }

        gs8_CommandBuffer[gu32_CommandPos++] = u8_Char;
    } 
    return b_KeyPress;  
}

void OnCommandReceived(bool b_PasswordValid)
{
    kUser k_User;
    char* s8_Parameter;

    gs8_CommandBuffer[gu32_CommandPos++] = 0;
    gu32_CommandPos = 0;    
    Utils::Print(LF);

    if (!b_PasswordValid)
    {
        b_PasswordValid = strcmp(gs8_CommandBuffer, PASSWORD) == 0;
        if (!b_PasswordValid)
        {
            Utils::Print("Invalid password.\r\n");
            Utils::DelayMilli(500);
            return;           
        }

        Utils::Print("Welcome to the access authorization terminal.\r\n");
        gs8_CommandBuffer[0] = 0;
    }

    gu64_LastPasswd = Utils::GetMillis64() + PASSWORD_OFFSET_MS;

    if (Utils::strnicmp(gs8_CommandBuffer, "DEBUG", 5) == 0)
    {
        if (!ParseParameter(gs8_CommandBuffer + 5, &s8_Parameter, 1, 1))
            return;

        if (s8_Parameter[0] < '0' || s8_Parameter[0] > '3')
        {
            Utils::Print("Invalid debug level.\r\n");
            return;
        }
      
        gi_PN532.SetDebugLevel(s8_Parameter[0] - '0');
        return;
    }    

    if (Utils::stricmp(gs8_CommandBuffer, "RESET") == 0)
    {
        InitReader(false);
        if (gb_InitSuccess)
        {
            Utils::Print("PN532 initialized successfully\r\n");
            return;
        }
    }   

    if (PASSWORD[0] != 0 && Utils::stricmp(gs8_CommandBuffer, "EXIT") == 0)
    {
        gu64_LastPasswd = 0;
        Utils::Print("You have logged out.\r\n");
        return;
    }   
  
    if (gb_InitSuccess)
    {
        if (Utils::stricmp(gs8_CommandBuffer, "CLEAR") == 0)
        {
            ClearEeprom();
            return;
        }
    
        if (Utils::stricmp(gs8_CommandBuffer, "LIST") == 0)
        {
            UserManager::ListAllUsers();
            return;
        }

        #if USE_DESFIRE
            if (Utils::stricmp(gs8_CommandBuffer, "RESTORE") == 0)
            {
                if (RestoreDesfireCard()) Utils::Print("Restore success\r\n");
                else                      Utils::Print("Restore failed\r\n");
                gi_PN532.SwitchOffRfField();
                return;
            }

            if (Utils::stricmp(gs8_CommandBuffer, "MAKERANDOM") == 0)
            {
                if (MakeRandomCard()) Utils::Print("MakeRandom success\r\n");
                else                  Utils::Print("MakeRandom failed\r\n");
                gi_PN532.SwitchOffRfField();
                return;
            }

            #if COMPILE_SELFTEST > 0
                if (Utils::stricmp(gs8_CommandBuffer, "TEST") == 0)
                {
                    gi_PN532.SetDebugLevel(COMPILE_SELFTEST);
                    if (gi_PN532.Selftest()) Utils::Print("\r\nSelftest success\r\n");
                    else                     Utils::Print("\r\nSelftest failed\r\n");
                    gi_PN532.SetDebugLevel(0);
                    gi_PN532.SwitchOffRfField();
                    return;
                }
            #endif
        #endif
    
        if (Utils::strnicmp(gs8_CommandBuffer, "ADD", 3) == 0)
        {
            if (!ParseParameter(gs8_CommandBuffer + 3, &s8_Parameter, 3, NAME_BUF_SIZE -1))
                return;

            AddCardToEeprom(s8_Parameter);

            gi_PN532.SwitchOffRfField();
            return;
        }
    
        if (Utils::strnicmp(gs8_CommandBuffer, "DEL", 3) == 0)
        {
            if (!ParseParameter(gs8_CommandBuffer + 3, &s8_Parameter, 3, NAME_BUF_SIZE -1))
                return;
          
            if (!UserManager::DeleteUser(0, s8_Parameter))
                Utils::Print("Error: User not found.\r\n");
                
            return;
        }    

        if (Utils::strnicmp(gs8_CommandBuffer, "DOOR12", 6) == 0)
        {
            if (!ParseParameter(gs8_CommandBuffer + 6, &s8_Parameter, 3, NAME_BUF_SIZE -1))
                return;
          
            if (!UserManager::SetUserFlags(s8_Parameter, DOOR_BOTH))
                Utils::Print("Error: User not found.\r\n");

            return;
        }    
        if (Utils::strnicmp(gs8_CommandBuffer, "DOOR1", 5) == 0)
        {
            if (!ParseParameter(gs8_CommandBuffer + 5, &s8_Parameter, 3, NAME_BUF_SIZE -1))
                return;
          
            if (!UserManager::SetUserFlags(s8_Parameter, DOOR_ONE))
                Utils::Print("Error: User not found.\r\n");

            return;
        }    
        if (Utils::strnicmp(gs8_CommandBuffer, "DOOR2", 5) == 0)
        {
            if (!ParseParameter(gs8_CommandBuffer + 5, &s8_Parameter, 3, NAME_BUF_SIZE -1))
                return;
          
            if (!UserManager::SetUserFlags(s8_Parameter, DOOR_TWO))
                Utils::Print("Error: User not found.\r\n");

            return;
        }    

        if (strlen(gs8_CommandBuffer))
            Utils::Print("Invalid command.\r\n\r\n");

        Utils::Print("Usage:\r\n");
        Utils::Print(" CLEAR          : Clear all users and their cards from the EEPROM\r\n");    
        Utils::Print(" ADD    {user}  : Add a user and his card to the EEPROM\r\n");
        Utils::Print(" DEL    {user}  : Delete a user and his card from the EEPROM\r\n");
        Utils::Print(" LIST           : List all users that are stored in the EEPROM\r\n");    
        Utils::Print(" DOOR1  {user}  : Open only door 1 for this user\r\n");
        Utils::Print(" DOOR2  {user}  : Open only door 2 for this user\r\n");
        Utils::Print(" DOOR12 {user}  : Open both doors for this user\r\n");
        
        #if USE_DESFIRE
            Utils::Print(" RESTORE        : Removes the master key and the application from the card\r\n");
            Utils::Print(" MAKERANDOM     : Converts the card into a Random ID card (FOREVER!)\r\n");
            #if COMPILE_SELFTEST > 0
                Utils::Print(" TEST           : Execute the selftest (requires an empty Desfire EV1 card)\r\n");
            #endif
        #endif
    }
    else
    {
        Utils::Print("FATAL ERROR: The PN532 did not respond. (Board initialization failed)\r\n");
        Utils::Print("Usage:\r\n");
    }

    Utils::Print(" RESET          : Reset the PN532 and run the chip initialization anew\r\n");
    Utils::Print(" DEBUG {level}  : Set debug level (0= off, 1= normal, 2= RxTx data, 3= details)\r\n");

    if (PASSWORD[0] != 0)
        Utils::Print(" EXIT           : Log out\r\n");
    Utils::Print(LF);

    #if USE_DESFIRE
        #if USE_AES
            Utils::Print("Compiled for Desfire EV1 cards (AES - 128 bit encryption used)\r\n");
        #else
            Utils::Print("Compiled for Desfire EV1 cards (3K3DES - 168 bit encryption used)\r\n");
        #endif
        #if ALLOW_ALSO_CLASSIC
            Utils::Print("Classic cards are also allowed.\r\n");
        #endif
    #else
        Utils::Print("Compiled for Classic cards (not recommended, use only for testing)\r\n");
    #endif

    int s32_MaxUsers = EEPROM.length() / sizeof(kUser);
    char Buf[80];
    sprintf(Buf, "Max %d users with a max name length of %d chars fit into the EEPROM\r\n", s32_MaxUsers, NAME_BUF_SIZE - 1);
    Utils::Print(Buf);

    Utils::Print("Terminal access is password protected: ");
    Utils::Print(PASSWORD[0] ? "Yes\r\n" : "No\r\n");

    uint32_t u32_Volt = MeasureVoltage();
    sprintf(Buf, "Battery voltage: %d.%d Volt\r\n",  (int)(u32_Volt/10), (int)(u32_Volt%10));
    Utils::Print(Buf);

    Utils::Print("System is running since ");   
    Utils::PrintInterval(Utils::GetMillis64(), LF);
}

bool ParseParameter(char* s8_Command, char** ps8_Parameter, int minLength, int maxLength)
{
    int P=0;
    if (s8_Command[P++] != ' ')
    {
        Utils::Print("Invalid command\r\n");
        return false;
    }

    while (s8_Command[P] == ' ')
    { 
        P++;
    }

    char* s8_Param = s8_Command + P;
    int   s32_Len  = strlen(s8_Param);

    while (s32_Len > 0 && s8_Param[s32_Len-1] == ' ')
    {
        s32_Len--;
        s8_Param[s32_Len] = 0;
    }
    
    if (s32_Len > maxLength)
    {
        Utils::Print("Parameter too long.\r\n");
        return false;
    }
    if (s32_Len < minLength)
    {
        Utils::Print("Parameter too short.\r\n");
        return false;
    }    
    
    *ps8_Parameter = s8_Param;
    return true;
}

void AddCardToEeprom(const char* s8_UserName)
{
    kUser k_User;
    kCard k_Card;   
    if (!WaitForCard(&k_User, &k_Card))
        return;
     
    Utils::GenerateRandom((byte*)k_User.s8_Name, NAME_BUF_SIZE);
    strcpy(k_User.s8_Name, s8_UserName);

    kUser k_Found;  
    if (UserManager::FindUser(k_User.ID.u64, &k_Found))
    {
        Utils::Print("This card has already been stored for user ");
        Utils::Print(k_Found.s8_Name, LF);
        return;
    }
  
    #if USE_DESFIRE
        if ((k_Card.e_CardType & CARD_Desfire) == 0)
        {
            #if !ALLOW_ALSO_CLASSIC
                Utils::Print("The card is not a Desfire card.\r\n");
                return;
            #endif
        }
        else
        {    
            if (!ChangePiccMasterKey())
                return;

            if (k_Card.e_CardType != CARD_DesRandom)
            {
                if (!StoreDesfireSecret(&k_User))
                {
                    Utils::Print("Could not personalize the card.\r\n");
                    return;
                }
            }
        }
    #endif

    k_User.u8_Flags = DOOR_ONE;

    UserManager::StoreNewUser(&k_User);
}

void ClearEeprom()
{
    Utils::Print("\r\nATTENTION: ALL cards and users will be erased.\r\nIf you are really sure hit 'Y' otherwise hit 'N'.\r\n\r\n");

    if (!WaitForKeyYesNo())
        return;

    UserManager::DeleteAllUsers();
    Utils::Print("All cards have been deleted.\r\n");
}

bool WaitForKeyYesNo()
{
    uint64_t u64_Start = Utils::GetMillis64();
    while (true)
    {
        char c_Char = SerialClass::Read();
        if  (c_Char == 'n' || c_Char == 'N' || (Utils::GetMillis64() - u64_Start) > 30000)
        {
            Utils::Print("Aborted.\r\n");
            return false;
        }
            
        if  (c_Char == 'y' || c_Char == 'Y')
             return true;

        delay(200);
    } 
}

bool WaitForCard(kUser* pk_User, kCard* pk_Card)
{
    Utils::Print("Please approximate the card to the reader now!\r\nYou have 30 seconds. Abort with ESC.\r\n");
    uint64_t u64_Start = Utils::GetMillis64();
    
    while (true)
    {
        if (ReadCard(pk_User->ID.u8, pk_Card) && pk_Card->u8_UidLength > 0)
        {
            gu64_LastID = pk_User->ID.u64;

            Utils::Print("Processing... (please do not remove the card)\r\n");
            return true;
        }
      
        if ((Utils::GetMillis64() - u64_Start) > 30000)
        {
            Utils::Print("Timeout waiting for card.\r\n");
            return false;
        }

        if (SerialClass::Read() == 27)
        {
            Utils::Print("Aborted.\r\n");
            return false;
        }
    }
}

bool ReadCard(byte u8_UID[8], kCard* pk_Card)
{
    memset(pk_Card, 0, sizeof(kCard));
  
    if (!gi_PN532.ReadPassiveTargetID(u8_UID, &pk_Card->u8_UidLength, &pk_Card->e_CardType))
    {
        pk_Card->b_PN532_Error = true;
        return false;
    }

    if (pk_Card->e_CardType == CARD_DesRandom)
    {
        #if USE_DESFIRE
            if (!AuthenticatePICC(&pk_Card->u8_KeyVersion))
                return false;
        
            if (!gi_PN532.GetRealCardID(u8_UID))
                return false;

            pk_Card->u8_UidLength = 7;
        #else
            Utils::Print("Cards with random ID are not supported in Classic mode.\r\n");
            return false;    
        #endif
    }
    return true;
}

bool IsDesfireTimeout()
{
    #if USE_DESFIRE
        if (gi_PN532.GetLastPN532Error() == 0x01)
        {
            Utils::Print("A Timeout mostly means that the card is too far away from the reader.\r\n");
            
            FlashLED(LED_RED, 200);
            return true;
        }
    #endif
    return false;
}

void OpenDoor(uint64_t u64_ID, kCard* pk_Card, uint64_t u64_StartTick)
{
    kUser k_User;  
    if (!UserManager::FindUser(u64_ID, &k_User))
    {
        Utils::Print("Unknown person tries to open the door: ");
        Utils::PrintHexBuf((byte*)&u64_ID, 7, LF);
        FlashLED(LED_RED, 1000);
        return;
    }

    #if USE_DESFIRE
        if ((pk_Card.e_CardType & CARD_Desfire) == 0)
        {
            #if !ALLOW_ALSO_CLASSIC
                Utils::Print("The card is not a Desfire card.\r\n");
                FlashLED(LED_RED, 1000);
                return;
            #endif
        }
        else
        {
            if (pk_Card->e_CardType == CARD_DesRandom)
            {
                if (pk_Card->u8_KeyVersion != CARD_KEY_VERSION)
                {
                    Utils::Print("The card is not personalized.\r\n");
                    FlashLED(LED_RED, 1000);
                    return;
                }
            }
            else
            {
                if (!CheckDesfireSecret(&k_User))
                {
                    if (IsDesfireTimeout())
                        return;
        
                    Utils::Print("The card is not personalized.\r\n");
                    FlashLED(LED_RED, 1000);
                    return;
                }
            }
        }
    #endif

    #if false
        char s8_Buf[80];
        sprintf(s8_Buf, "Reading the card took %d ms.\r\n", (int)(Utils::GetMillis64() - u64_StartTick));
        Utils::Print(s8_Buf);
    #endif

    switch (k_User.u8_Flags & DOOR_BOTH)
    {
        case DOOR_ONE:  Utils::Print("Opening door 1 for ");     break;
        case DOOR_TWO:  Utils::Print("Opening door 2 for ");     break;
        case DOOR_BOTH: Utils::Print("Opening door 1 + 2 for "); break;
        default:        Utils::Print("No door specified for ");  break;
    }
    Utils::Print(k_User.s8_Name);
    switch (pk_Card->e_CardType)
    {
        case CARD_DesRandom: Utils::Print(" (Desfire random card)",  LF); break;
        case CARD_Desfire:   Utils::Print(" (Desfire default card)", LF); break;
        default:             Utils::Print(" (Classic card)",         LF); break;
    }

    ActivateRelais(k_User.u8_Flags);

    gu64_LastID = u64_ID;
}

void ActivateRelais(byte u8_Flags)
{
    ge_BattCheck = BATT_OK;
  
    SetLED(LED_GREEN);
    
    if (u8_Flags & DOOR_ONE)
    {
        if (!ActivateRelaisCheckVoltage(DOOR_1_PIN))
            ge_BattCheck = BATT_OLD_RED;
    }
    if ((u8_Flags & DOOR_BOTH) == DOOR_BOTH)
    {
        Utils::DelayMilli(500);
    }
    if (u8_Flags & DOOR_TWO)
    {
        if (!ActivateRelaisCheckVoltage(DOOR_2_PIN))
            ge_BattCheck = BATT_OLD_RED;
    }
    
    Utils::DelayMilli(1000);
    SetLED(LED_OFF);  
}

bool ActivateRelaisCheckVoltage(byte u8_Pin)
{
    uint32_t u32_VoltHigh = MeasureVoltage();
    Utils::WritePin(u8_Pin, HIGH);
    
    Utils::DelayMilli(OPEN_INTERVAL);

    uint3232 u32_VoltLow = MeasureVoltage();
    Utils::WritePin(u8_Pin, LOW);

    if (u32_VoltLow > u32_VoltHigh)
        return true;

    uint32_t u32_Drop = u32_VoltHigh - u32_VoltLow;

    char s8_Buf[100];
    sprintf(s8_Buf, "Voltage drop: %d.%d Volt\r\n",  (int)(u32_Drop/10), (int)(u32_Drop%10));
    Utils::Print(s8_Buf);

    return (u32_Drop <= MAX_VOLTAGE_DROP);
}

uint32_t MeasureVoltage()
{
    const uint32_t maxValue = (1 << ANALOG_RESOLUTION) -1;

    float value = 10.0 * analogRead(VOLTAGE_MEASURE_PIN);
    return (uint32_t)((value * ANALOG_REFERENCE * VOLTAGE_FACTOR) / maxValue);
}

void CheckOpenButton()
{
    if (BUTTON_OPEN_DOOR == NO_DOOR)
        return;
    
    static uint64_t u64_ButtonPress = 0;

    if (Utils::ReadPin(BUTTON_OPEN_PIN) == LOW)
    {
        if (u64_ButtonPress == 0)
        {
            Utils::Print("Button pressed -> opening the door(s)");
            ActivateRelais(BUTTON_OPEN_DOOR);
        }

        u64_ButtonPress = Utils::GetMillis64();        
    }
    else
    {
        if (u64_ButtonPress == 0)
            return;

        if (Utils::GetMillis64() - u64_ButtonPress >= 2000)
            u64_ButtonPress = 0;
    }
}

#if USE_DESFIRE

bool AuthenticatePICC(byte* pu8_KeyVersion)
{
    if (!gi_PN532.SelectApplication(0x000000))
        return false;

    if (!gi_PN532.GetKeyVersion(0, pu8_KeyVersion))
        return false;

    if (*pu8_KeyVersion == CARD_KEY_VERSION)
    {
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    else
    {
        if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY))
            return false;
    }
    return true;
}

bool GenerateDesfireSecrets(kUser* pk_User, DESFireKey* pi_AppMasterKey, byte u8_StoreValue[16])
{
    byte u8_Data[24] = {0}; 

    memcpy(u8_Data, pk_User->ID.u8, 7);

    int B=0;
    for (int N=0; N<NAME_BUF_SIZE; N++)
    {
        u8_Data[B++] ^= pk_User->s8_Name[N];
        if (B > 15) B = 0;
    }

    byte u8_AppMasterKey[24];

    DES i_3KDes;
    if (!i_3KDes.SetKeyData(SECRET_APPLICATION_KEY, sizeof(SECRET_APPLICATION_KEY), 0) ||
        !i_3KDes.CryptDataCBC(CBC_SEND, KEY_ENCIPHER, u8_AppMasterKey, u8_Data, 24))
        return false;
    
    if (!i_3KDes.SetKeyData(SECRET_STORE_VALUE_KEY, sizeof(SECRET_STORE_VALUE_KEY), 0) ||
        !i_3KDes.CryptDataCBC(CBC_SEND, KEY_ENCIPHER, u8_StoreValue, u8_Data, 16))
        return false;

    if (!pi_AppMasterKey->SetKeyData(u8_AppMasterKey, sizeof(u8_AppMasterKey), CARD_KEY_VERSION))
        return false;

    return true;
}

bool CheckDesfireSecret(kUser* pk_User)
{
    DESFIRE_KEY_TYPE i_AppMasterKey;
    byte u8_StoreValue[16];
    if (!GenerateDesfireSecrets(pk_User, &i_AppMasterKey, u8_StoreValue))
        return false;

    if (!gi_PN532.SelectApplication(0x000000))
        return false;

    byte u8_Version; 
    if (!gi_PN532.GetKeyVersion(0, &u8_Version))
        return false;

    if (u8_Version != CARD_KEY_VERSION)
        return false;

    if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID))
        return false;

    if (!gi_PN532.Authenticate(0, &i_AppMasterKey))
        return false;

    byte u8_FileData[16];
    if (!gi_PN532.ReadFileData(CARD_FILE_ID, 0, 16, u8_FileData))
        return false;

    if (memcmp(u8_FileData, u8_StoreValue, 16) != 0)
        return false;

    return true;
}

bool ChangePiccMasterKey()
{
    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    if (u8_KeyVersion != CARD_KEY_VERSION)
    {
        if (!gi_PN532.ChangeKey(0, &gi_PiccMasterKey, NULL))
            return false;

        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    return true;
}

bool StoreDesfireSecret(kUser* pk_User)
{
    if (CARD_APPLICATION_ID == 0x000000 || CARD_KEY_VERSION == 0)
        return false;
  
    DESFIRE_KEY_TYPE i_AppMasterKey;
    byte u8_StoreValue[16];
    if (!GenerateDesfireSecrets(pk_User, &i_AppMasterKey, u8_StoreValue))
        return false;

    if (!gi_PN532.DeleteApplicationIfExists(CARD_APPLICATION_ID))
        return false;

    if (!gi_PN532.CreateApplication(CARD_APPLICATION_ID, KS_FACTORY_DEFAULT, 1, i_AppMasterKey.GetKeyType()))
        return false;

    if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID))
        return false;

    if (!gi_PN532.Authenticate(0, &DEFAULT_APP_KEY))
        return false;

    if (!gi_PN532.ChangeKey(0, &i_AppMasterKey, NULL))
        return false;

    if (!gi_PN532.Authenticate(0, &i_AppMasterKey))
        return false;

    if (!gi_PN532.ChangeKeySettings(KS_CHANGE_KEY_FROZEN))
        return false;

    DESFireFilePermissions k_Permis;
    k_Permis.e_ReadAccess         = AR_KEY0;
    k_Permis.e_WriteAccess        = AR_KEY0;
    k_Permis.e_ReadAndWriteAccess = AR_KEY0;
    k_Permis.e_ChangeAccess       = AR_KEY0;
    if (!gi_PN532.CreateStdDataFile(CARD_FILE_ID, &k_Permis, 16))
        return false;

    if (!gi_PN532.WriteFileData(CARD_FILE_ID, 0, 16, u8_StoreValue))
        return false;       
  
    return true;
}

bool RestoreDesfireCard()
{
    kUser k_User;
    kCard k_Card;  
    if (!WaitForCard(&k_User, &k_Card))
        return false;

    UserManager::DeleteUser(k_User.ID.u64, NULL);    

    if ((k_Card.e_CardType & CARD_Desfire) == 0)
    {
        Utils::Print("The card is not a Desfire card.\r\n");
        return false;
    }

    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    if (u8_KeyVersion == 0)
        return true;

    bool b_Success = gi_PN532.DeleteApplicationIfExists(CARD_APPLICATION_ID);
    if (!b_Success)
    {
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    
    if (!gi_PN532.ChangeKey(0, &gi_PN532.DES2_DEFAULT_KEY, NULL))
        return false;

    if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY))
        return false;

    return b_Success;
}

bool MakeRandomCard()
{
    Utils::Print("\r\nATTENTION: Configuring the card to send a random ID cannot be reversed.\r\nThe card will be a random ID card FOREVER!\r\nIf you are really sure what you are doing hit 'Y' otherwise hit 'N'.\r\n\r\n");
    if (!WaitForKeyYesNo())
        return false;
    
    kUser k_User;
    kCard k_Card;  
    if (!WaitForCard(&k_User, &k_Card))
        return false;

    if ((k_Card.e_CardType & CARD_Desfire) == 0)
    {
        Utils::Print("The card is not a Desfire card.\r\n");
        return false;
    }

    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    return gi_PN532.EnableRandomIDForever();
}

#endif


