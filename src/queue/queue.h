#pragma once
#include <time.h>

void queuePush(const char *tagId, time_t timestamp = 0);
void queueFlush();
void queueLoad();
void queueSave();
int  queueSize();
