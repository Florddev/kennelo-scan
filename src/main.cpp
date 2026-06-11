#include <Arduino.h>
#include "rfid/rfid.h"
#include "wifi/wifi.h"
#include "api/api.h"
#include "queue/queue.h"
#include "button/button.h"
#include "bluetooth/bluetooth.h"
#include "storage/storage.h"

#define API_URL      "http://10.134.217.230:8000/api/pets/broadcast"
#define SCANNER_CODE "SCANNER-001"

static bool _btMode = false;

void setup()
{
    Serial.begin(115200);
    delay(2000);
    storageBegin();
    buttonBegin();
    _btMode = buttonCheckBtMode();
    wifiBegin();
    apiInit(API_URL, SCANNER_CODE);
    rfidBegin();
    queueLoad();
        
    Serial.print("Starting...");

    if (_btMode)
    {
        Serial.print("Starting Bluetooth...");
        bluetoothBegin();
    }
    else
    {
        Serial.print("Starting WiFi...");
        wifiTick();
    }
}

void loop()
{
    if (buttonLoop() == BTN_HOLD_SLEEP)
    {
        queueSave();
        if (_btMode) bluetoothEnd();
        buttonGoToSleep();
    }

    rfidLoop();

    char tag[16];
    if (rfidGetTag(tag, sizeof(tag)))
    {
        if (wifiIsConnected())
        {
            if (!apiSend(tag)) queuePush(tag);
        }
        else
        {
            queuePush(tag);
        }
    }

    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 3000)
    {
        lastStatusPrint = millis();
        Serial.printf("[STATUS] mode=%s wifi=%s\n",
            _btMode ? "BLUETOOTH" : "WIFI",
            wifiIsConnected() ? "connecté" : "déconnecté");
    }

    if (_btMode)
    {
        bluetoothLoop();
        if (bluetoothHasNewNetwork())
        {
            bluetoothEnd();
            _btMode = false;
            wifiReset();
        }
        return;
    }

    wifiTick();

    static unsigned long lastFlush = 0;
    if (millis() - lastFlush > 10000)
    {
        lastFlush = millis();
        queueFlush();
    }
}
