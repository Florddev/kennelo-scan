#include "storage.h"
#include <Preferences.h>
#include <string.h>

static Preferences _prefs;

void storageBegin()
{
    _prefs.begin("kennelo", false);
}

int storageLoadNetworks(WifiCredential *out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < STORAGE_MAX_NETWORKS && count < maxCount; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "n%d_ssid", i);
        if (!_prefs.isKey(key)) continue;

        _prefs.getString(key, out[count].ssid, sizeof(out[count].ssid));
        snprintf(key, sizeof(key), "n%d_pass", i);
        _prefs.getString(key, out[count].password, sizeof(out[count].password));
        snprintf(key, sizeof(key), "n%d_prio", i);
        out[count].priority = _prefs.getUChar(key, i + 1);
        count++;
    }
    return count;
}

bool storageSaveNetwork(const WifiCredential &net)
{
    int freeSlot = -1;
    for (int i = 0; i < STORAGE_MAX_NETWORKS; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "n%d_ssid", i);

        if (!_prefs.isKey(key))
        {
            if (freeSlot == -1) freeSlot = i;
            continue;
        }

        char existing[33];
        _prefs.getString(key, existing, sizeof(existing));
        if (strcmp(existing, net.ssid) == 0)
        {
            snprintf(key, sizeof(key), "n%d_pass", i);
            _prefs.putString(key, net.password);
            snprintf(key, sizeof(key), "n%d_prio", i);
            _prefs.putUChar(key, net.priority);
            return true;
        }
    }

    if (freeSlot == -1) return false;

    char key[16];
    snprintf(key, sizeof(key), "n%d_ssid", freeSlot);
    _prefs.putString(key, net.ssid);
    snprintf(key, sizeof(key), "n%d_pass", freeSlot);
    _prefs.putString(key, net.password);
    snprintf(key, sizeof(key), "n%d_prio", freeSlot);
    _prefs.putUChar(key, net.priority);
    return true;
}

bool storageDeleteNetwork(const char *ssid)
{
    for (int i = 0; i < STORAGE_MAX_NETWORKS; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "n%d_ssid", i);
        if (!_prefs.isKey(key)) continue;

        char existing[33];
        _prefs.getString(key, existing, sizeof(existing));
        if (strcmp(existing, ssid) == 0)
        {
            _prefs.remove(key);
            snprintf(key, sizeof(key), "n%d_pass", i);
            _prefs.remove(key);
            snprintf(key, sizeof(key), "n%d_prio", i);
            _prefs.remove(key);
            return true;
        }
    }
    return false;
}

void storageClear()
{
    _prefs.clear();
}
