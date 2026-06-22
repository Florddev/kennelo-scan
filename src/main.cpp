#include <Arduino.h>
#include <esp_system.h>
#include "rfid/rfid.h"
#include "wifi/wifi.h"
#include "api/api.h"
#include "queue/queue.h"
#include "button/button.h"
#include "bluetooth/bluetooth.h"
#include "storage/storage.h"
#include "display/display.h"

#define API_URL "http://10.46.60.230:8000/api/pets/broadcast"

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
    displayBegin(_scannerCode);

    _btMode = buttonCheckBtMode();
    displaySetBtMode(_btMode);

    wifiBegin();
    apiInit(API_URL, _scannerCode);
    rfidBegin();
    queueLoad();
    displaySetQueueSize(queueSize());

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
        displaySetAction(DISP_SLEEPING);
        displayTick();
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
            displaySetBtMode(true);
            displaySetAction(DISP_IDLE);
            bluetoothBegin(_scannerCode);
        }
        else
        {
            Serial.println("[MAIN] triple-click → retour mode WiFi");
            bluetoothEnd();
            _btMode = false;
            displaySetBtMode(false);
            displaySetAction(DISP_IDLE);
            wifiBegin();
            wifiReset();
        }
    }

    rfidLoop();

    char tag[16];
    if (rfidGetTag(tag, sizeof(tag)))
    {
        Serial.printf("[SCAN] puce détectée: %s\n", tag);

        if (wifiIsConnected())
        {
            displaySetAction(DISP_SENDING, tag);
            displayTick(); // affiche "Envoi en cours..." avant l'appel bloquant

            if (!apiSend(tag))
            {
                Serial.println("[SCAN] envoi API échoué → mise en file d'attente");
                queuePush(tag);
                displaySetAction(DISP_SENT_FAIL, tag);
                displaySetQueueSize(queueSize());
            }
            else
            {
                displaySetAction(DISP_SENT_OK, tag);
            }
        }
        else
        {
            Serial.println("[SCAN] WiFi déconnecté → mise en file d'attente");
            queuePush(tag);
            displaySetAction(DISP_SENT_FAIL, tag);
            displaySetQueueSize(queueSize());
        }
    }

    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 3000)
    {
        lastStatus = millis();
        bool connected = wifiIsConnected();
        Serial.printf("[STATUS] mode=%s wifi=%s\n",
                      _btMode ? "BLUETOOTH" : "WIFI",
                      connected ? "connecté" : "déconnecté");
        displaySetWifiConnected(connected);
    }

    if (_btMode)
    {
        bluetoothLoop();
        if (bluetoothHasNewNetwork())
        {
            bluetoothEnd();
            _btMode = false;
            displaySetBtMode(false);
            displaySetAction(DISP_IDLE);
            wifiBegin();
            wifiReset();
        }
        displayTick();
        return;
    }

    wifiTick();

    static unsigned long lastFlush = 0;
    if (millis() - lastFlush > 2000)
    {
        lastFlush = millis();
        queueFlush();
        displaySetQueueSize(queueSize());
    }

    displayTick();
}
