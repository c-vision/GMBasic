/* credential_vault.c — stub in-memory (TODO: Keychain su macOS, DPAPI su win) */
#include "credential_vault.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#define MAX_CREDS 32
typedef struct { char service[64]; char user[64]; char secret[256]; int used; } cred_t;
static cred_t g_creds[MAX_CREDS];

int gmb_credential_vault_store(const char *service, const char *user, const char *secret) {
    if (!service || !user || !secret) return -1;
    for (int i = 0; i < MAX_CREDS; i++) if (g_creds[i].used && strcasecmp(g_creds[i].service, service) == 0 && strcasecmp(g_creds[i].user, user) == 0) {
        strncpy(g_creds[i].secret, secret, 255); return 0;
    }
    for (int i = 0; i < MAX_CREDS; i++) if (!g_creds[i].used) {
        strncpy(g_creds[i].service, service, 63); strncpy(g_creds[i].user, user, 63); strncpy(g_creds[i].secret, secret, 255); g_creds[i].used = 1; return 0;
    }
    return -1;
}
const char *gmb_credential_vault_load(const char *service, const char *user) {
    if (!service || !user) return NULL;
    for (int i = 0; i < MAX_CREDS; i++) if (g_creds[i].used && strcasecmp(g_creds[i].service, service) == 0 && strcasecmp(g_creds[i].user, user) == 0) return g_creds[i].secret;
    return NULL;
}
int gmb_credential_vault_remove(const char *service, const char *user) {
    for (int i = 0; i < MAX_CREDS; i++) if (g_creds[i].used && strcasecmp(g_creds[i].service, service) == 0 && strcasecmp(g_creds[i].user, user) == 0) { g_creds[i].used = 0; return 0; }
    return -1;
}
int gmb_credential_vault_has(const char *service) {
    for (int i = 0; i < MAX_CREDS; i++) if (g_creds[i].used && strcasecmp(g_creds[i].service, service) == 0) return 1;
    return 0;
}
