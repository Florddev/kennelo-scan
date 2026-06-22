#pragma once

enum DisplayAction {
    DISP_IDLE,
    DISP_SCAN_DETECTED,
    DISP_SENDING,
    DISP_SENT_OK,
    DISP_SENT_FAIL,
    DISP_ANIMAL_FOUND,
    DISP_ANIMAL_UNKNOWN,
    DISP_SLEEPING
};

void displayBegin(const char *scannerCode);
void displaySetBtMode(bool active);
void displaySetWifiConnected(bool connected);
void displaySetBattery(int percent, bool charging); // -1 = non disponible
void displaySetQueueSize(int size);
void displaySetAction(DisplayAction action, const char *tag = nullptr);
void displaySetAnimalResult(bool found, const char *name, const char *species, const char *breed);
void displayTick();
