#include "wifi.h"
#include "../storage/storage.h"
#include <WiFi.h>
#include <string.h>

#define RSSI_THRESHOLD  -70
#define CONNECT_TIMEOUT 8000

static const unsigned long _retryDelays[] = {5000, 10000, 20000, 40000, 60000};

enum WifiState { WIFI_IDLE, WIFI_SCANNING, WIFI_CONNECTING, WIFI_CONNECTED };

static WifiState      _state          = WIFI_IDLE;
static unsigned long  _lastAttempt    = 0;
static unsigned long  _connectStart   = 0;
static int            _retryIndex     = 0;
static WifiCredential _candidates[STORAGE_MAX_NETWORKS];
static int            _candidateCount = 0;
static int            _candidateIdx   = 0;

static void sortByPriority(WifiCredential *nets, int count)
{
    for (int i = 0; i < count - 1; i++)
        for (int j = i + 1; j < count; j++)
            if (nets[j].priority < nets[i].priority)
            {
                WifiCredential tmp = nets[i];
                nets[i] = nets[j];
                nets[j] = tmp;
            }
}

void wifiBegin()
{
    WiFi.mode(WIFI_STA);
}

bool wifiIsConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

void wifiReset()
{
    WiFi.disconnect();
    _state       = WIFI_IDLE;
    _lastAttempt = 0;
    _retryIndex  = 0;
}

void wifiTick()
{
    switch (_state)
    {
        case WIFI_IDLE:
        {
            if (wifiIsConnected()) { _state = WIFI_CONNECTED; return; }
            unsigned long retryDelay = _retryDelays[_retryIndex < 5 ? _retryIndex : 4];
            if (_lastAttempt != 0 && millis() - _lastAttempt < retryDelay) return;
            _lastAttempt = millis();
            WiFi.scanNetworks(true);
            _state = WIFI_SCANNING;
            break;
        }

        case WIFI_SCANNING:
        {
            int n = WiFi.scanComplete();
            if (n < 0) return;

            WifiCredential saved[STORAGE_MAX_NETWORKS];
            int savedCount = storageLoadNetworks(saved, STORAGE_MAX_NETWORKS);
            sortByPriority(saved, savedCount);

            _candidateCount = 0;
            for (int i = 0; i < savedCount; i++)
                for (int j = 0; j < n; j++)
                    if (strcmp(saved[i].ssid, WiFi.SSID(j).c_str()) == 0
                        && WiFi.RSSI(j) > RSSI_THRESHOLD)
                    {
                        _candidates[_candidateCount++] = saved[i];
                        break;
                    }

            WiFi.scanDelete();

            if (_candidateCount == 0)
            {
                if (_retryIndex < 4) _retryIndex++;
                _state = WIFI_IDLE;
                return;
            }

            _candidateIdx = 0;
            WiFi.begin(_candidates[0].ssid, _candidates[0].password);
            _connectStart = millis();
            _state = WIFI_CONNECTING;
            break;
        }

        case WIFI_CONNECTING:
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                _retryIndex = 0;
                _state = WIFI_CONNECTED;
                return;
            }
            if (millis() - _connectStart >= CONNECT_TIMEOUT)
            {
                WiFi.disconnect();
                _candidateIdx++;
                if (_candidateIdx < _candidateCount)
                {
                    WiFi.begin(_candidates[_candidateIdx].ssid, _candidates[_candidateIdx].password);
                    _connectStart = millis();
                }
                else
                {
                    if (_retryIndex < 4) _retryIndex++;
                    _state = WIFI_IDLE;
                }
            }
            break;
        }

        case WIFI_CONNECTED:
        {
            if (!wifiIsConnected()) _state = WIFI_IDLE;
            break;
        }
    }
}
