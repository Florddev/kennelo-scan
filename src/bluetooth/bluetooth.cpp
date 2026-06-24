#include "bluetooth.h"
#include "../storage/storage.h"
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <WiFi.h>
#include <esp_gap_ble_api.h>
#include <string.h>
#include <stdio.h>

#define BLE_DEVICE_NAME "KenneloScan"
#define SERVICE_UUID    "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHAR_TX_UUID    "19b10001-e8f2-537e-4f6c-d104768a1214"
#define CHAR_RX_UUID    "19b10002-e8f2-537e-4f6c-d104768a1214"
#define BLE_CHUNK_SIZE  180

// ─── State ───────────────────────────────────────────────────────────────────

static portMUX_TYPE       _mux             = portMUX_INITIALIZER_UNLOCKED;
static BLECharacteristic *_txChar          = nullptr;
static BLEAdvertising    *_adv             = nullptr;
static bool               _initialized     = false;  // stack BLE démarré au moins une fois
static bool               _enabled         = false;  // advertising actif
static bool               _clientConnected = false;
static bool               _newNetworkSaved = false;
static bool               _hasCommand      = false;
static bool               _scanInProgress  = false;
static char               _rxBuffer[256]   = {0};
static char               _scannerCode[32] = {0};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void bleSend(const char *data)
{
    if (!_clientConnected || !_txChar) return;
    size_t len    = strlen(data);
    size_t offset = 0;
    while (offset < len)
    {
        size_t chunk = len - offset;
        if (chunk > BLE_CHUNK_SIZE) chunk = BLE_CHUNK_SIZE;
        _txChar->setValue((uint8_t *)(data + offset), chunk);
        _txChar->notify();
        offset += chunk;
        if (offset < len) delay(20);
    }
}

static void bleSendLine(const char *data)
{
    char buf[280];
    snprintf(buf, sizeof(buf), "%s\n", data);
    bleSend(buf);
}

// ─── Command handling ────────────────────────────────────────────────────────

static void handleCommand(const char *cmd)
{
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, cmd))
    {
        bleSendLine("{\"t\":\"err\",\"m\":\"bad json\"}");
        return;
    }

    const char *type = doc["t"] | "";

    if (strcmp(type, "scan") == 0)
    {
        WiFi.scanNetworks(true);
        _scanInProgress = true;
        return;
    }

    if (strcmp(type, "save") == 0)
    {
        const char *ssid = doc["ssid"] | "";
        const char *pass = doc["pass"] | "";
        if (ssid[0] == '\0' || pass[0] == '\0')
        {
            bleSendLine("{\"t\":\"err\",\"m\":\"missing fields\"}");
            return;
        }
        WifiCredential net = {};
        strncpy(net.ssid,     ssid, sizeof(net.ssid)     - 1);
        strncpy(net.password, pass, sizeof(net.password) - 1);
        net.priority = (uint8_t)(doc["pri"] | 5);
        if (storageSaveNetwork(net))
        {
            bleSendLine("{\"t\":\"ok\"}");
            _newNetworkSaved = true;
        }
        else
        {
            bleSendLine("{\"t\":\"err\",\"m\":\"storage full\"}");
        }
        return;
    }

    if (strcmp(type, "del") == 0)
    {
        const char *ssid = doc["ssid"] | "";
        if (ssid[0] == '\0')
        {
            bleSendLine("{\"t\":\"err\",\"m\":\"missing ssid\"}");
            return;
        }
        bleSendLine(storageDeleteNetwork(ssid) ? "{\"t\":\"ok\"}" : "{\"t\":\"err\",\"m\":\"not found\"}");
        return;
    }

    if (strcmp(type, "list") == 0)
    {
        WifiCredential nets[STORAGE_MAX_NETWORKS];
        int count = storageLoadNetworks(nets, STORAGE_MAX_NETWORKS);
        for (int i = 0; i < count; i++)
        {
            StaticJsonDocument<128> out;
            out["t"]    = "saved";
            out["ssid"] = nets[i].ssid;
            out["pri"]  = nets[i].priority;
            char msg[140];
            serializeJson(out, msg, sizeof(msg));
            bleSendLine(msg);
            delay(30);
        }
        bleSendLine("{\"t\":\"list_done\"}");
        return;
    }

    if (strcmp(type, "clear") == 0)
    {
        storageClear();
        bleSendLine("{\"t\":\"ok\"}");
        return;
    }

    if (strcmp(type, "info") == 0)
    {
        StaticJsonDocument<64> out;
        out["t"]    = "info";
        out["code"] = _scannerCode;
        char msg[80];
        serializeJson(out, msg, sizeof(msg));
        bleSendLine(msg);
        return;
    }

    if (strcmp(type, "unbond") == 0)
    {
        int count = esp_ble_get_bond_device_num();
        if (count > 0)
        {
            esp_ble_bond_dev_t *list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
            esp_ble_get_bond_device_list(&count, list);
            for (int i = 0; i < count; i++)
                esp_ble_remove_bond_device(list[i].bd_addr);
            free(list);
        }
        bleSendLine("{\"t\":\"ok\"}");
        return;
    }

    bleSendLine("{\"t\":\"err\",\"m\":\"unknown command\"}");
}

static void processScan()
{
    int n = WiFi.scanComplete();
    if (n < 0) return;

    for (int i = 0; i < n; i++)
    {
        StaticJsonDocument<128> out;
        out["t"]    = "net";
        out["ssid"] = WiFi.SSID(i);
        out["rssi"] = WiFi.RSSI(i);
        char msg[140];
        serializeJson(out, msg, sizeof(msg));
        bleSendLine(msg);
        delay(30);
    }
    bleSendLine("{\"t\":\"scan_done\"}");
    WiFi.scanDelete();
    _scanInProgress = false;
}

// ─── BLE callbacks ───────────────────────────────────────────────────────────

class ServerCB : public BLEServerCallbacks
{
    void onConnect(BLEServer *) override { _clientConnected = true; }
    void onDisconnect(BLEServer *srv) override
    {
        _clientConnected = false;
        // Ne redémarre l'advertising que si le mode BT est toujours actif
        if (_enabled) srv->startAdvertising();
    }
};

class RxCB : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *chr) override
    {
        std::string val = chr->getValue();
        size_t len = val.length();
        if (len > 0 && len < sizeof(_rxBuffer))
        {
            portENTER_CRITICAL(&_mux);
            memcpy(_rxBuffer, val.data(), len);
            _rxBuffer[len] = '\0';
            _hasCommand    = true;
            portEXIT_CRITICAL(&_mux);
        }
    }
};

// ─── Public API ──────────────────────────────────────────────────────────────

void bluetoothBegin(const char *scannerCode)
{
    char deviceName[48];
    snprintf(deviceName, sizeof(deviceName), "%s-%s", BLE_DEVICE_NAME, scannerCode);
    strncpy(_scannerCode, scannerCode, sizeof(_scannerCode) - 1);
    _scannerCode[sizeof(_scannerCode) - 1] = '\0';

    Serial.printf("[BLE] démarrage avec nom: %s\n", deviceName);

    if (!_initialized)
    {
        // Première initialisation — crée le stack et les objets BLE (une seule fois)
        BLEDevice::init(deviceName);
        BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);

        BLESecurity *sec = new BLESecurity();
        sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
        sec->setCapability(ESP_IO_CAP_NONE);
        sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
        sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

        BLEServer *server = BLEDevice::createServer();
        server->setCallbacks(new ServerCB());

        BLEService *service = server->createService(SERVICE_UUID);

        _txChar = service->createCharacteristic(CHAR_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        _txChar->addDescriptor(new BLE2902());
        _txChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);

        BLECharacteristic *rxChar = service->createCharacteristic(CHAR_RX_UUID, BLECharacteristic::PROPERTY_WRITE);
        rxChar->setCallbacks(new RxCB());
        rxChar->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);

        service->start();

        _adv = BLEDevice::getAdvertising();
        _adv->addServiceUUID(SERVICE_UUID);
        _adv->setScanResponse(true);

        _initialized = true;
    }

    // (Ré)applique le nom et (re)démarre l'advertising — sans réinitialiser le stack
    esp_ble_gap_set_device_name(deviceName);
    BLEAdvertisementData scanResp;
    scanResp.setName(deviceName);
    _adv->setScanResponseData(scanResp);

    _enabled         = true;
    _clientConnected = false;
    _hasCommand      = false;
    BLEDevice::startAdvertising();
}

void bluetoothEnd()
{
    // Arrêt temporaire : stoppe l'advertising sans détruire le stack BLE.
    // Permet de reprendre immédiatement avec bluetoothBegin() sans crash.
    _enabled = false;
    if (_scanInProgress)
    {
        WiFi.scanDelete();
        _scanInProgress = false;
    }
    BLEDevice::stopAdvertising();
    _clientConnected = false;
    _hasCommand      = false;
}

void bluetoothShutdown()
{
    // Arrêt complet — uniquement avant deep sleep.
    bluetoothEnd();
    if (_initialized)
    {
        BLEDevice::deinit(true);
        _initialized = false;
        _txChar      = nullptr;
        _adv         = nullptr;
    }
}

void bluetoothLoop()
{
    char localCmd[256];
    bool hasCmd = false;

    portENTER_CRITICAL(&_mux);
    if (_hasCommand)
    {
        memcpy(localCmd, _rxBuffer, sizeof(localCmd));
        _hasCommand = false;
        hasCmd      = true;
    }
    portEXIT_CRITICAL(&_mux);

    if (hasCmd) handleCommand(localCmd);
    if (_scanInProgress) processScan();
}

bool bluetoothHasNewNetwork()
{
    if (!_newNetworkSaved) return false;
    _newNetworkSaved = false;
    return true;
}
