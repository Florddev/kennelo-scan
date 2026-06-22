#include "api.h"
#include <HTTPClient.h>

// Délais courts pour ne pas bloquer la boucle quand le serveur est injoignable.
#define API_CONNECT_TIMEOUT_MS 1500
#define API_RESPONSE_TIMEOUT_MS 5000

static const char *_apiUrl;
static const char *_scannerCode;

void apiInit(const char *url, const char *scannerCode)
{
    _apiUrl = url;
    _scannerCode = scannerCode;
}

bool apiSend(const char *tagId, time_t timestamp)
{
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
    String response = http.getString();

    if (code > 0)
    {
        Serial.printf("[API] réponse HTTP %d: %s\n", code, response.c_str());
    }
    else
    {
        Serial.printf("[API] échec requête (%d): %s\n", code, http.errorToString(code).c_str());
    }

    bool ok = code >= 200 && code < 300;
    http.end();
    return ok;
}
