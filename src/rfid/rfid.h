#pragma once

void rfidBegin();
void rfidLoop();
bool rfidGetTag(char* buffer, int maxLen);
