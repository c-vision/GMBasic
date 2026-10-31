/* gmb_fixtures_cli.c — fixture di mercato on demand.
 *
 * Uso:
 *   gmb_fixtures list                 fixture dichiarate nel manifest + quali sono presenti
 *   gmb_fixtures pull [nome...]       scarica+estrae in shared/samples/Fixtures/<nome>/
 *   gmb_fixtures pull --all           tutte quelle dichiarate
 *     opzioni: --dest <dir>  --version <v>  --force  --quiet
 *
 * Le fixture sono DATI DI SVILUPPO (compilatore e test): il programma distribuito non le porta.
 * Exit: 0 = ok, 1 = errore.
 */
#include "gmb_fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    const char* cmd = argc > 1 ? argv[1] : "list";
    const char* dest = NULL;
    const char* version = NULL;
    int force = 0, quiet = 0, all = 0;
    const char* names[32];
    int nnames = 0;

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        fprintf(stderr,
            "uso: gmb_fixtures list\n"
            "     gmb_fixtures pull [nome...] [--all] [--dest dir] [--version v] [--force] [--quiet]\n");
        return 0;
    }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dest") == 0 && i + 1 < argc) dest = argv[++i];
        else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) version = argv[++i];
        else if (strcmp(argv[i], "--force") == 0) force = 1;
        else if (strcmp(argv[i], "--quiet") == 0) quiet = 1;
        else if (strcmp(argv[i], "--all") == 0) all = 1;
        else if (argv[i][0] == '-') { fprintf(stderr, "opzione sconosciuta: %s\n", argv[i]); return 1; }
        else if (nnames < 32) names[nnames++] = argv[i];
    }

    char buf[4096];
    if (strcmp(cmd, "list") == 0) {
        char root[4096];
        gmb_fixtures_root(root, sizeof root);
        printf("cartella: %s\n", root);
        int n = gmb_fixtures_available(buf, sizeof buf);
        if (n < 0) { fprintf(stderr, "?%s\n", gmb_fixtures_strerror(GMB_FIX_E_NO_MANIFEST)); return 1; }
        char present[4096];
        gmb_fixtures_present(present, sizeof present);
        char* save = NULL;
        for (char* tok = strtok_r(buf, ";", &save); tok; tok = strtok_r(NULL, ";", &save)) {
            char name[256];
            snprintf(name, sizeof name, "%s", tok);
            char* at = strchr(name, '@');
            if (at) *at = 0;
            int is_present = 0;
            char pbuf[4200];
            snprintf(pbuf, sizeof pbuf, ";%s;", present);
            char needle[300];
            snprintf(needle, sizeof needle, ";%s;", name);
            is_present = strstr(pbuf, needle) != NULL;
            printf("  %-14s %-10s %s\n", name, at ? at + 1 : "", is_present ? "presente" : "da scaricare");
        }
        return 0;
    }

    if (strcmp(cmd, "pull") != 0) {
        fprintf(stderr, "comando sconosciuto: %s (usa list|pull)\n", cmd);
        return 1;
    }

    if (all) {
        int n = gmb_fixtures_available(buf, sizeof buf);
        if (n < 0) { fprintf(stderr, "?%s\n", gmb_fixtures_strerror(GMB_FIX_E_NO_MANIFEST)); return 1; }
        char* save = NULL;
        for (char* tok = strtok_r(buf, ";", &save); tok && nnames < 32; tok = strtok_r(NULL, ";", &save)) {
            char name[256];
            snprintf(name, sizeof name, "%s", tok);
            char* at = strchr(name, '@');
            if (at) *at = 0;
            if (name[0]) names[nnames++] = strdup(name);
        }
    }
    if (nnames == 0) {
        fprintf(stderr, "quali fixture? indica uno o più nomi, oppure --all (vedi `gmb_fixtures list`)\n");
        return 1;
    }

    const gmb_fetch_transport* tr = gmb_fetch_transport_curl();
    if (!tr) { fprintf(stderr, "runtime compilato senza libcurl\n"); return 1; }
    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = quiet;

    int failed = 0;
    for (int i = 0; i < nnames; i++) {
        char out[4096], err[512];
        int rc = gmb_fixtures_pull(names[i], version, dest, force, tr, &op, out, sizeof out, err, sizeof err);
        if (rc == GMB_FIX_OK) {
            if (!quiet) printf("ok: %s -> %s\n", names[i], out);
        } else {
            failed++;
            fprintf(stderr, "?%s: %s%s%s\n", names[i], gmb_fixtures_strerror(rc), err[0] ? " — " : "", err);
        }
    }
    return failed ? 1 : 0;
}
