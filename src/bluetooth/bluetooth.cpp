#include "bluetooth.h"
#include "../storage/storage.h"
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
static bool               _clientConnected = false;
static bool               _newNetworkSaved = false;
static bool               _hasCommand      = false;
static bool               _scanInProgress  = false;
static char               _rxBuffer[256]   = {0};
static char               _scannerCode[32] = {0};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void jsonEscape(const char *src, char *dst, int maxLen)
{
    int j = 0;
    for (int i = 0; src[i] != '\0' && j < maxLen - 2; i++)
    {
        if (src[i] == '"' || src[i] == '\\')
        {
            if (j >= maxLen - 2) break;
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static bool extractStr(const char *json, const char *key, char *out, int maxLen)
{
    char search[40];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *start = strstr(json, search);
    if (!start) return false;
    start += strlen(search);
    const char *end = strchr(start, '"');
    if (!end) return false;
    int len = (int)(end - start);
    if (len >= maxLen) len = maxLen - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

static int extractInt(const char *json, const char *key, int defaultVal)
{
    char search[40];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *start = strstr(json, search);
    if (!start) return defaultVal;
    return atoi(start + strlen(search));
}

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
    if (strstr(cmd, "\"t\":\"scan\""))
    {
        WiFi.scanNetworks(true);
        _scanInProgress = true;
        return;
    }

    if (strstr(cmd, "\"t\":\"save\""))
    {
        WifiCredential net = {};
        if (!extractStr(cmd, "ssid", net.ssid, sizeof(net.ssid)) ||
            !extractStr(cmd, "pass", net.password, sizeof(net.password)))
        {
            bleSendLine("{\"t\":\"err\",\"m\":\"missing fields\"}");
            return;
        }
        net.priority = (uint8_t)extractInt(cmd, "pri", 5);
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

    if (strstr(cmd, "\"t\":\"del\""))
    {
        char ssid[33] = {};
        if (!extractStr(cmd, "ssid", ssid, sizeof(ssid)))
        {
            bleSendLine("{\"t\":\"err\",\"m\":\"missing ssid\"}");
            return;
        }
        bleSendLine(storageDeleteNetwork(ssid) ? "{\"t\":\"ok\"}" : "{\"t\":\"err\",\"m\":\"not found\"}");
        return;
    }

    if (strstr(cmd, "\"t\":\"list\""))
    {
        WifiCredential nets[STORAGE_MAX_NETWORKS];
        int count = storageLoadNetworks(nets, STORAGE_MAX_NETWORKS);
        for (int i = 0; i < count; i++)
        {
            char ssid[66], msg[110];
            jsonEscape(nets[i].ssid, ssid, sizeof(ssid));
            snprintf(msg, sizeof(msg), "{\"t\":\"saved\",\"ssid\":\"%s\",\"pri\":%d}", ssid, nets[i].priority);
            bleSendLine(msg);
            delay(30);
        }
        bleSendLine("{\"t\":\"list_done\"}");
        return;
    }

    if (strstr(cmd, "\"t\":\"clear\""))
    {
        storageClear();
        bleSendLine("{\"t\":\"ok\"}");
        return;
    }

    if (strstr(cmd, "\"t\":\"info\""))
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "{\"t\":\"info\",\"code\":\"%s\"}", _scannerCode);
        bleSendLine(msg);
        return;
    }

    if (strstr(cmd, "\"t\":\"unbond\""))
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
        char ssid[66], msg[110];
        jsonEscape(WiFi.SSID(i).c_str(), ssid, sizeof(ssid));
        snprintf(msg, sizeof(msg), "{\"t\":\"net\",\"ssid\":\"%s\",\"rssi\":%d}", ssid, WiFi.RSSI(i));
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
        srv->startAdvertising();
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

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
}

void bluetoothEnd()
{
    if (_scanInProgress)
    {
        WiFi.scanDelete();
        _scanInProgress = false;
    }
    BLEDevice::stopAdvertising();
    BLEDevice::deinit(true);
    _txChar          = nullptr;
    _clientConnected = false;
    _hasCommand      = false;
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
