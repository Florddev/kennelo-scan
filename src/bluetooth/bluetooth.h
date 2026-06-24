#pragma once

void bluetoothBegin(const char *scannerCode);
void bluetoothEnd();        // arrêt temporaire (mode switch) — stack BLE conservé
void bluetoothShutdown();   // arrêt complet pour deep sleep uniquement
void bluetoothLoop();
bool bluetoothHasNewNetwork();
