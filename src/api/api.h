#pragma once
#include <time.h>

void apiInit(const char *url, const char *scannerCode);
bool apiSend(const char *tagId, time_t timestamp = 0);
