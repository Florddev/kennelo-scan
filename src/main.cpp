#include <Arduino.h>
#include <esp_system.h>
#include "rfid/rfid.h"
#include "wifi/wifi.h"
#include "api/api.h"
#include "queue/queue.h"
#include "button/button.h"
#include "bluetooth/bluetooth.h"
#include "storage/storage.h"

#define API_URL "http://10.134.217.230:8000/api/pets/broadcast"

static bool _btMode = false;
static char _scannerCode[9];

void setup()
{
    Serial.begin(115200);
    delay(2000);

    uint64_t chipId = ESP.getEfuseMac();
    snprintf(_scannerCode, sizeof(_scannerCode), "%08X", (uint32_t)(chipId >> 32));
    Serial.printf("[MAIN] scanner code: %s\n", _scannerCode);

    storageBegin();
    buttonBegin();
    _btMode = buttonCheckBtMode();
    wifiBegin();
    apiInit(API_URL, _scannerCode);
    rfidBegin();
    queueLoad();

    Serial.print("Starting...");

    if (_btMode)
    {
        Serial.print("Starting Bluetooth...");
        bluetoothBegin(_scannerCode);
    }
    else
    {
        Serial.print("Starting WiFi...");
        wifiTick();
    }
}

void loop()
{
    ButtonEvent evt = buttonLoop();

    if (evt == BTN_HOLD_SLEEP)
    {
        queueSave();
        if (_btMode)
            bluetoothEnd();
        buttonGoToSleep();
    }
    else if (evt == BTN_TRIPLE_CLICK)
    {
        if (!_btMode)
        {
            Serial.println("[MAIN] triple-click → mode BLE");
            _btMode = true;
            bluetoothBegin(_scannerCode);
        }
        else
        {
            Serial.println("[MAIN] triple-click → retour mode WiFi");
            bluetoothEnd();
            _btMode = false;
            wifiBegin();
            wifiReset();
        }
    }

    rfidLoop();

    char tag[16];
    if (rfidGetTag(tag, sizeof(tag)))
    {
        if (wifiIsConnected())
        {
            if (!apiSend(tag))
                queuePush(tag);
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
            wifiBegin();
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
