#include <Arduino.h>
#include <esp_system.h>
#include <time.h>
#include "rfid/rfid.h"
#include "wifi/wifi.h"
#include "api/api.h"
#include "queue/queue.h"
#include "button/button.h"
#include "bluetooth/bluetooth.h"
#include "storage/storage.h"
#include "display/display.h"

#define API_URL          "http://10.46.60.230:8000/api/pets/broadcast"
#define DEDUP_COOLDOWN_MS 5000

static bool _btMode = false;
static char _scannerCode[9];

// Dernier tag scanné pour la déduplication
static char          _lastTag[16]   = "";
static unsigned long _lastTagAt     = 0;

// Suivi de la connexion WiFi pour déclencher la sync NTP
static bool _wifiWasConnected = false;

// Retourne le timestamp Unix si le NTP est synchronisé, 0 sinon
static time_t _getTimestamp()
{
    time_t now = time(nullptr);
    return (now > 1000000000UL) ? now : 0;
}

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
        // Déduplication : ignorer le même tag pendant DEDUP_COOLDOWN_MS
        bool isDuplicate = (strcmp(tag, _lastTag) == 0) &&
                           (millis() - _lastTagAt < DEDUP_COOLDOWN_MS);

        if (isDuplicate)
        {
            Serial.printf("[SCAN] doublon ignoré: %s\n", tag);
        }
        else
        {
            strncpy(_lastTag, tag, 15);
            _lastTag[15] = '\0';
            _lastTagAt   = millis();

            Serial.printf("[SCAN] puce détectée: %s\n", tag);
            time_t ts = _getTimestamp();

            if (wifiIsConnected())
            {
                displaySetAction(DISP_SENDING, tag);
                displayTick(); // affiche avant l'appel bloquant

                if (!apiSend(tag, ts))
                {
                    Serial.println("[SCAN] envoi API échoué → mise en file d'attente");
                    queuePush(tag, ts);
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
                queuePush(tag, ts);
                displaySetAction(DISP_SENT_FAIL, tag);
                displaySetQueueSize(queueSize());
            }
        }
    }

    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 3000)
    {
        lastStatus = millis();
        bool connected = wifiIsConnected();

        // Première connexion WiFi : démarrer la sync NTP
        if (connected && !_wifiWasConnected)
        {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            Serial.println("[NTP] synchronisation démarrée");
        }
        _wifiWasConnected = connected;

        Serial.printf("[STATUS] mode=%s wifi=%s ntp=%s\n",
                      _btMode ? "BLUETOOTH" : "WIFI",
                      connected ? "connecté" : "déconnecté",
                      (_getTimestamp() > 0) ? "ok" : "en attente");

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
