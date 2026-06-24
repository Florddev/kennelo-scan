#pragma once
#include <time.h>

enum DisplayAction {
    DISP_IDLE,
    DISP_SCAN_DETECTED,
    DISP_SENDING,
    DISP_SENT_OK,
    DISP_SENT_FAIL,
    DISP_ANIMAL_FOUND,
    DISP_ANIMAL_UNKNOWN,
    DISP_NOT_REGISTERED,
    DISP_HISTORY,
    DISP_SLEEPING
};

void displayBegin(const char *scannerCode);
void displaySetBtMode(bool active);
void displaySetWifiConnected(bool connected);
void displaySetWifiScanning(bool scanning);
void displaySetWifiRssi(int rssi);
void displaySetFlushing(bool flushing);
void displaySetHistoryEntry(int pos, int total, const char *tag,
                             const char *name, const char *species, const char *breed,
                             time_t timestamp, bool found);
void displaySetQueueSize(int size);
void displaySetAction(DisplayAction action, const char *tag = nullptr);
void displaySetAnimalResult(bool found, const char *name, const char *species, const char *breed);
void displayTick();
void displayOff();
