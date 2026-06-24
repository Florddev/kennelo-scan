#pragma once
#include <time.h>

struct ApiResult {
    bool   ok;            // requête HTTP réussie (2xx)
    bool   found;         // animal trouvé dans la base
    bool   fromQueue;     // l'envoi provient de la file persistée (re-tentative)
    bool   notRegistered; // HTTP 422 : scanner non associé à un compte
    char   tag[16];       // puce concernée (renvoyée telle quelle)
    time_t timestamp;     // horodatage du scan (renvoyé tel quel)
    char   name[32];
    char   species[32];
    char   breed[48];
};

// Démarre la tâche d'envoi en arrière-plan.
void apiInit(const char *url, const char *scannerCode);

// Met un envoi en file de traitement (non-bloquant).
// fromQueue = true pour le vidage de la file persistée.
// Retourne false si la file d'envoi est pleine.
bool apiEnqueue(const char *tagId, time_t timestamp, bool fromQueue);

// Récupère un résultat d'envoi terminé (non-bloquant).
// Retourne false s'il n'y a rien à dépiler.
bool apiPollResult(ApiResult *out);
