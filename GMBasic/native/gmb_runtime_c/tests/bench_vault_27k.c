/* bench_vault_27k.c — benchmark di scala del VAULT index C (gmb_vault_create/index)
 * su AiFinanceWiki (~27.8k .md). Verifica L1 del Second Brain: file_index 27k, tempo.
 * Uso: compilato con gmb_runtime; eseguito dalla root repo (config/ in root).
 */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char** argv) {
    /* Radice del vault: SEMPRE da argv (nessun path personale hardcoded). Senza argomento si
     * usa la CWD, che contiene comunque i .md del repo. */
    const char* root = argc > 1 ? argv[1] : ".";
    const char* name = "bench-finance";
    double t0 = now_sec();
    long long cid = gmb_vault_create(name, root, "*.md");
    double t1 = now_sec();
    if (cid < 0) { fprintf(stderr, "vault_create fallito\n"); return 1; }
    long long n = gmb_vault_index(name);
    double t2 = now_sec();
    printf("VAULT create : %lld id  (%.2f s)\n", cid, t1 - t0);
    printf("VAULT index  : %lld file (%.2f s)\n", n, t2 - t1);
    printf("totale       : %.2f s\n", t2 - t0);
    /* verifica conteggio + folder count via JSON */
    char* info = gmb_vault_info_json(name);
    if (info) { printf("info: %.400s\n", info); free(info); }
    char* folders = gmb_vault_folders_json(name);
    if (folders) { printf("folders len: %zu\n", strlen(folders)); free(folders); }
    return 0;
}
