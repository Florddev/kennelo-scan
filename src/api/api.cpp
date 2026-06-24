#include "api.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define API_CONNECT_TIMEOUT_MS  1500
#define API_RESPONSE_TIMEOUT_MS 5000
#define API_REQ_QUEUE_LEN       16
#define API_RES_QUEUE_LEN       16
#define API_TASK_STACK          8192

// Requête interne déposée dans la file de la tâche d'envoi
struct ApiRequest {
    char   tag[16];
    time_t timestamp;
    bool   fromQueue;
};

static const char  *_apiUrl      = nullptr;
static const char  *_scannerCode = nullptr;
static QueueHandle_t _reqQueue   = nullptr;
static QueueHandle_t _resQueue   = nullptr;

// Envoi HTTP bloquant — exécuté uniquement par la tâche d'arrière-plan.
static ApiResult doSend(const char *tagId, time_t timestamp)
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
    } else if (code > 0) {
        String errBody = http.getString();
        Serial.printf("[API] échec HTTP %d: %s\n", code, errBody.c_str());
        if (code == 422)
            result.notRegistered = true;
    } else {
        Serial.printf("[API] erreur connexion (%d): %s\n", code, http.errorToString(code).c_str());
    }

    http.end();
    return result;
}

// Tâche d'arrière-plan : consomme les requêtes et publie les résultats.
static void apiTask(void *)
{
    ApiRequest req;
    for (;;) {
        if (xQueueReceive(_reqQueue, &req, portMAX_DELAY) != pdTRUE) continue;

        ApiResult res  = doSend(req.tag, req.timestamp);
        res.fromQueue  = req.fromQueue;
        res.timestamp  = req.timestamp;
        strncpy(res.tag, req.tag, sizeof(res.tag) - 1);
        res.tag[sizeof(res.tag) - 1] = '\0';

        xQueueSend(_resQueue, &res, portMAX_DELAY);
    }
}

void apiInit(const char *url, const char *scannerCode)
{
    _apiUrl      = url;
    _scannerCode = scannerCode;

    _reqQueue = xQueueCreate(API_REQ_QUEUE_LEN, sizeof(ApiRequest));
    _resQueue = xQueueCreate(API_RES_QUEUE_LEN, sizeof(ApiResult));
    xTaskCreate(apiTask, "apiTask", API_TASK_STACK, nullptr, 1, nullptr);
}

bool apiEnqueue(const char *tagId, time_t timestamp, bool fromQueue)
{
    if (!_reqQueue) return false;

    ApiRequest req = {};
    strncpy(req.tag, tagId, sizeof(req.tag) - 1);
    req.tag[sizeof(req.tag) - 1] = '\0';
    req.timestamp = timestamp;
    req.fromQueue = fromQueue;

    return xQueueSend(_reqQueue, &req, 0) == pdTRUE;
}

bool apiPollResult(ApiResult *out)
{
    if (!_resQueue) return false;
    return xQueueReceive(_resQueue, out, 0) == pdTRUE;
}
