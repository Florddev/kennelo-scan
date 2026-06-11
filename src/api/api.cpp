#include "api.h"
#include <HTTPClient.h>

static const char *_apiUrl;
static const char *_scannerCode;

void apiInit(const char *url, const char *scannerCode)
{
    _apiUrl = url;
    _scannerCode = scannerCode;
}

bool apiSend(const char *tagId)
{
    HTTPClient http;
    http.begin(_apiUrl);
    http.addHeader("Content-Type", "application/json");

    char body[128];
    snprintf(body, sizeof(body),
             "{\"microchip_number\":\"%s\",\"scanner_code\":\"%s\"}",
             tagId, _scannerCode);

    int code = http.POST(body);
    bool ok = code >= 200 && code < 300;
    http.end();
    return ok;
}
