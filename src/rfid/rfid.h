#pragma once

void rfidBegin();
void rfidEnd();
void rfidLoop();
bool rfidGetTag(char* buffer, int maxLen);
