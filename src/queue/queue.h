#pragma once
#include <time.h>

void queuePush(const char *tagId, time_t timestamp = 0);
bool queuePeekFront(char *tagId, int maxLen, time_t *timestamp);
void queuePopFront();
void queueLoad();
void queueSave();
int  queueSize();
