#pragma once

void wifiBegin();
void wifiEnd();
bool wifiIsConnected();
bool wifiIsSearching();
int  wifiGetRssi();
void wifiTick();
void wifiReset();
