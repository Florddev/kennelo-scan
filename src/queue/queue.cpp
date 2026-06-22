#include "queue.h"
#include "../api/api.h"
#include "../wifi/wifi.h"
#include <Preferences.h>
#include <string.h>

#define QUEUE_MAX_SIZE 50
#define TAG_MAX_LEN    16

static char _queue[QUEUE_MAX_SIZE][TAG_MAX_LEN];
static int  _queueSize = 0;

void queuePush(const char *tagId)
{
    if (_queueSize >= QUEUE_MAX_SIZE) return;
    strncpy(_queue[_queueSize], tagId, TAG_MAX_LEN - 1);
    _queue[_queueSize][TAG_MAX_LEN - 1] = '\0';
    _queueSize++;
}

static void removeFirst()
{
    for (int i = 0; i < _queueSize - 1; i++)
        strncpy(_queue[i], _queue[i + 1], TAG_MAX_LEN);
    _queueSize--;
}

void queueFlush()
{
    bool changed = false;
    while (_queueSize > 0 && wifiIsConnected())
    {
        if (apiSend(_queue[0]))
        {
            removeFirst();
            changed = true;
        }
        else break;
        delay(200);
    }
    if (changed) queueSave();
}

void queueLoad()
{
    Preferences prefs;
    prefs.begin("kq", true);
    _queueSize = prefs.getInt("size", 0);
    if (_queueSize > QUEUE_MAX_SIZE) _queueSize = QUEUE_MAX_SIZE;
    for (int i = 0; i < _queueSize; i++)
    {
        char key[8];
        snprintf(key, sizeof(key), "t%d", i);
        prefs.getString(key, _queue[i], TAG_MAX_LEN);
    }
    prefs.end();
}

int queueSize()
{
    return _queueSize;
}

void queueSave()
{
    Preferences prefs;
    prefs.begin("kq", false);
    prefs.putInt("size", _queueSize);
    for (int i = 0; i < _queueSize; i++)
    {
        char key[8];
        snprintf(key, sizeof(key), "t%d", i);
        prefs.putString(key, _queue[i]);
    }
    prefs.end();
}
