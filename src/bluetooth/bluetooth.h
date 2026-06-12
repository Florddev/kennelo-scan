#pragma once

void bluetoothBegin(const char *scannerCode);
void bluetoothEnd();
void bluetoothLoop();
bool bluetoothHasNewNetwork();
