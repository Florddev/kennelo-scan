#include "rfid.h"
#include <Arduino.h>
#include <Rfid134.h>

#define RFID_RX_PIN 3
#define RFID_TX_PIN 4

static char _pendingTag[16] = {0};
static bool _hasTag = false;

class RfidNotify
{
public:
    static void OnError(Rfid134_Error errorCode)
    {
        Serial.print("RFID error: ");
        Serial.println(errorCode);
    }

    static void OnPacketRead(const Rfid134Reading &reading)
    {
        char temp[8];
        sprintf(_pendingTag, "%03u", reading.country);
        sprintf(temp, "%06lu", static_cast<uint32_t>(reading.id / 1000000));
        strcat(_pendingTag, temp);
        sprintf(temp, "%06lu", static_cast<uint32_t>(reading.id % 1000000));
        strcat(_pendingTag, temp);
        _hasTag = true;
    }
};

Rfid134<HardwareSerial, RfidNotify> rfid(Serial1);

void rfidBegin()
{
    Serial1.begin(9600, SERIAL_8N2, RFID_RX_PIN, RFID_TX_PIN);
    rfid.begin();
}

void rfidEnd()
{
    Serial1.end();
}

void rfidLoop()
{
    rfid.loop();
}

bool rfidGetTag(char *buffer, int maxLen)
{
    if (!_hasTag)
        return false;
    strncpy(buffer, _pendingTag, maxLen - 1);
    buffer[maxLen - 1] = '\0';
    _hasTag = false;
    return true;
}
