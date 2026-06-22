#pragma once
#include <time.h>

struct ApiResult {
    bool ok;       // requête HTTP réussie (2xx)
    bool found;    // animal trouvé dans la base
    char name[32];
    char species[32];
    char breed[48];
};

void      apiInit(const char *url, const char *scannerCode);
ApiResult apiSend(const char *tagId, time_t timestamp = 0);
