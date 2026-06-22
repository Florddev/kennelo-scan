#include "api.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define API_CONNECT_TIMEOUT_MS  1500
#define API_RESPONSE_TIMEOUT_MS 5000

static const char *_apiUrl;
static const char *_scannerCode;

void apiInit(const char *url, const char *scannerCode)
{
    _apiUrl      = url;
    _scannerCode = scannerCode;
}

ApiResult apiSend(const char *tagId, time_t timestamp)
{
    ApiResult result = {};

    HTTPClient http;
    http.begin(_apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.setConnectTimeout(API_CONNECT_TIMEOUT_MS);
    http.setTimeout(API_RESPONSE_TIMEOUT_MS);
    http.setReuse(false);

    char body[192];
    if (timestamp > 0) {
        struct tm t;
        gmtime_r(&timestamp, &t);
        char iso[25];
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &t);
        snprintf(body, sizeof(body),
                 "{\"microchip_number\":\"%s\",\"scanner_code\":\"%s\",\"scanned_at\":\"%s\"}",
                 tagId, _scannerCode, iso);
    } else {
        snprintf(body, sizeof(body),
                 "{\"microchip_number\":\"%s\",\"scanner_code\":\"%s\"}",
                 tagId, _scannerCode);
    }

    Serial.printf("[API] POST %s\n", _apiUrl);
    Serial.printf("[API] body: %s\n", body);

    int code = http.POST(body);

    if (code >= 200 && code < 300) {
        result.ok = true;
        String response = http.getString();
        Serial.printf("[API] réponse %d: %s\n", code, response.c_str());

        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, response)) {
            result.found = doc["found"] | false;
            strncpy(result.name,    doc["name"]    | "", sizeof(result.name)    - 1);
            strncpy(result.species, doc["species"] | "", sizeof(result.species) - 1);
            strncpy(result.breed,   doc["breed"]   | "", sizeof(result.breed)   - 1);
        }
    } else {
        Serial.printf("[API] échec (%d): %s\n", code, http.errorToString(code).c_str());
    }

    http.end();
    return result;
}
