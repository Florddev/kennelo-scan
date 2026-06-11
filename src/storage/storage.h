#pragma once
#include <stdint.h>

#define STORAGE_MAX_NETWORKS 5

struct WifiCredential
{
    char    ssid[33];
    char    password[65];
    uint8_t priority;
};

void storageBegin();
int  storageLoadNetworks(WifiCredential *out, int maxCount);
bool storageSaveNetwork(const WifiCredential &net);
bool storageDeleteNetwork(const char *ssid);
void storageClear();
