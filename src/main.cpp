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

// ── Historique RAM des scans depuis le démarrage ──────────────────────────
#define HISTORY_MAX 20

struct ScanRecord {
    char   tag[16];
    char   name[32];
    char   species[32];
    char   breed[48];
    bool   found;
    time_t timestamp;
};

static ScanRecord    _history[HISTORY_MAX];
static int           _historyCount  = 0;
static int           _historyPos    = 0;  // 0 = plus récent
static bool          _historyActive = false;
static unsigned long _historyShowAt = 0;
#define HISTORY_TIMEOUT_MS 5000

static void _historyAdd(const char *tag, time_t ts, bool found,
                        const char *name, const char *species, const char *breed)
{
    if (_historyCount < HISTORY_MAX) {
        _historyCount++;
    } else {
        memmove(&_history[0], &_history[1], sizeof(ScanRecord) * (HISTORY_MAX - 1));
    }
    ScanRecord &r = _history[_historyCount - 1];
    strncpy(r.tag,     tag     ? tag     : "", sizeof(r.tag)     - 1);
    strncpy(r.name,    name    ? name    : "", sizeof(r.name)    - 1);
    strncpy(r.species, species ? species : "", sizeof(r.species) - 1);
    strncpy(r.breed,   breed   ? breed   : "", sizeof(r.breed)   - 1);
    r.found     = found;
    r.timestamp = ts;
}

static void _historyShow(int pos)
{
    if (_historyCount == 0) return;
    _historyPos    = pos % _historyCount;
    _historyActive = true;
    _historyShowAt = millis();
    int arrayIdx   = _historyCount - 1 - _historyPos;
    ScanRecord &r  = _history[arrayIdx];
    displaySetHistoryEntry(_historyPos + 1, _historyCount,
                           r.tag, r.name, r.species, r.breed,
                           r.timestamp, r.found);
    displaySetAction(DISP_HISTORY);
}

// ── Un envoi de la file persistée est en vol (un seul à la fois pour préserver l'ordre)
static bool          _flushInFlight = false;
// Timestamp du prochain essai autorisé après un échec (backoff)
static unsigned long _nextFlushAt   = 0;
#define FLUSH_RETRY_MS 8000

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
    snprintf(_scannerCode, sizeof(_scannerCode), "KNL-%04X", (uint16_t)(chipId >> 32));
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

    if (evt == BTN_SINGLE_CLICK)
    {
        if (_historyCount > 0)
        {
            int nextPos = _historyActive ? (_historyPos + 1) % _historyCount : 0;
            _historyShow(nextPos);
        }
    }
    else if (evt == BTN_HOLD_SLEEP)
    {
        displaySetAction(DISP_SLEEPING);
        displayTick();
        delay(400);

        queueSave();

        if (_btMode)
            bluetoothShutdown();  // arrêt complet avant sleep
        else
            wifiEnd();

        rfidEnd();
        displayOff();
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
                // Envoi non-bloquant : confié à la tâche API en arrière-plan.
                // Le résultat est récupéré plus bas via apiPollResult().
                displaySetAction(DISP_SENDING, tag);
                if (!apiEnqueue(tag, ts, false))
                {
                    Serial.println("[SCAN] file d'envoi pleine → mise en file d'attente");
                    queuePush(tag, ts);
                    displaySetAction(DISP_SENT_FAIL, tag);
                    displaySetQueueSize(queueSize());
                }
            }
            else
            {
                Serial.println("[SCAN] WiFi déconnecté → mise en file d'attente");
                queuePush(tag, ts);
                _historyAdd(tag, ts, false, "", "", "");
                _historyActive = false;
                displaySetAction(DISP_SENT_FAIL, tag);
                displaySetQueueSize(queueSize());
            }
        }
    }

    // Traitement des résultats d'envoi asynchrones produits par la tâche API
    ApiResult apiRes;
    while (apiPollResult(&apiRes))
    {
        if (apiRes.fromQueue)
        {
            _flushInFlight = false;
            if (apiRes.ok)
            {
                Serial.printf("[FLUSH] succès → suppression de la file (restant: %d)\n", queueSize() - 1);
                queuePopFront();
                queueSave();
                displaySetQueueSize(queueSize());
                _nextFlushAt = 0;
                // Repasser en IDLE si l'écran "non lié" était affiché
                displaySetAction(DISP_IDLE);
            }
            else if (apiRes.notRegistered)
            {
                Serial.println("[FLUSH] scanner non enregistré → affichage + pause 60s");
                displaySetAction(DISP_NOT_REGISTERED);
                _nextFlushAt = millis() + 60000UL;
            }
            else
            {
                Serial.printf("[FLUSH] échec → nouvel essai dans %dms\n", FLUSH_RETRY_MS);
                _nextFlushAt = millis() + FLUSH_RETRY_MS;
            }
        }
        else if (apiRes.ok)
        {
            Serial.printf("[SCAN] animal: found=%d name=%s\n", apiRes.found, apiRes.name);
            _historyAdd(apiRes.tag, apiRes.timestamp, apiRes.found,
                        apiRes.name, apiRes.species, apiRes.breed);
            _historyActive = false;
            displaySetAnimalResult(apiRes.found, apiRes.name, apiRes.species, apiRes.breed);
        }
        else if (apiRes.notRegistered)
        {
            Serial.println("[SCAN] scanner non enregistré");
            _historyAdd(apiRes.tag, apiRes.timestamp, false, "", "", "");
            _historyActive = false;
            displaySetAction(DISP_NOT_REGISTERED);
            queuePush(apiRes.tag, apiRes.timestamp);
            queueSave();
            displaySetQueueSize(queueSize());
        }
        else
        {
            Serial.println("[SCAN] envoi API échoué → mise en file d'attente");
            _historyAdd(apiRes.tag, apiRes.timestamp, false, "", "", "");
            _historyActive = false;
            queuePush(apiRes.tag, apiRes.timestamp);
            displaySetAction(DISP_SENT_FAIL, apiRes.tag);
            displaySetQueueSize(queueSize());
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
        displaySetWifiRssi(connected ? wifiGetRssi() : -100);
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
    displaySetWifiScanning(wifiIsSearching());

    // Vidage asynchrone de la file persistée : un seul envoi en vol à la fois.
    // Après un échec, on attend FLUSH_RETRY_MS avant de réessayer.
    if (wifiIsConnected() && !_flushInFlight && queueSize() > 0 && millis() >= _nextFlushAt)
    {
        char   qtag[16];
        time_t qts;
        if (queuePeekFront(qtag, sizeof(qtag), &qts) && apiEnqueue(qtag, qts, true))
        {
            _flushInFlight = true;
            Serial.printf("[FLUSH] envoi entrée (queue: %d)\n", queueSize());
        }
    }

    displaySetFlushing(_flushInFlight && wifiIsConnected());

    // Retour automatique à l'écran principal après 5s sans appui en mode historique
    if (_historyActive && millis() - _historyShowAt > HISTORY_TIMEOUT_MS)
    {
        _historyActive = false;
        displaySetAction(DISP_IDLE);
    }

    displayTick();
}
