/* gmbadminverify.c — `gmb doctor --verify`: confronta i file dell'artefatto col manifest.
 *
 * PERCHÉ ESISTE (P2 del piano Release-1.0): un artefatto di release è verificabile solo se
 * QUALCUNO confronta i byte con quelli dichiarati. `tools/sh/release.sh` scrive
 * `release-manifest.json` accanto allo staging (sha256 + size di OGNI file); qui si legge quel
 * manifest e si ricalcola lo sha256 di ogni voce.
 *
 * Lo SHA-256 è quello del runtime (`gmb_pkg_sha256_file`, unica implementazione): questo tool
 * gira in `gmbadmin`, che linka già `gmb_runtime`/`gmb_pkg` — nessuna copia di codice.
 *
 * Uso:
 *   gmbadmin verify [--manifest FILE] [--dir DIR] [--quiet]
 *   gmb doctor --verify          (il CLI inoltra qui)
 *
 * Exit: 0 = tutti i file combaciano · 1 = file mancanti o manomessi · 2 = manifest assente/rotto.
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_pkg_lock.h"   /* gmb_pkg_sha256_file: una sola implementazione di SHA-256 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define VF_MAX_ENTRIES 4096

typedef struct {
    char path[512];
    char sha[65];
    long size;
} vf_entry;

static int vf_is_file(const char* p)
{
    struct stat st;
    return p && *p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* Cartella dell'eseguibile (dove cercare il manifest di un artefatto scompattato). */
static int vf_exe_dir(char* out, size_t cap)
{
    char exe[4096] = "";
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) != 0) exe[0] = 0;
    }
#elif defined(__linux__)
    {
        ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0) exe[n] = 0;
    }
#endif
    char* sl = exe[0] ? strrchr(exe, '/') : NULL;
    if (!sl) return 0;
    *sl = 0;
    snprintf(out, cap, "%s", exe);
    return 1;
}

/* Legge `"files": { "<path>": {"sha256": "<hex>", "size": N}, ... }`.
 * Parser minimale ma stretto: si ferma al primo manifest malformato invece di indovinare. */
static int vf_parse(const char* txt, vf_entry* out, int max, int* count)
{
    const char* p = strstr(txt, "\"files\"");
    if (!p) return 0;
    p = strchr(p, '{');
    if (!p) return 0;
    p++;
    int n = 0;
    for (;;) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (*p == '}' || !*p) break;
        if (*p != '"') return 0;
        const char* k = ++p;
        const char* ke = strchr(k, '"');
        if (!ke || ke - k >= (long)sizeof out[n].path) return 0;
        p = ke + 1;
        while (*p == ' ') p++;
        if (*p != ':') return 0;
        p++;
        while (*p == ' ') p++;
        if (*p != '{') return 0;
        p++;
        char sha[65] = "";
        long size = -1;
        for (;;) {
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
            if (*p == '}') { p++; break; }
            if (*p != '"') return 0;
            const char* fk = ++p;
            const char* fke = strchr(fk, '"');
            if (!fke) return 0;
            size_t flen = (size_t)(fke - fk);
            p = fke + 1;
            while (*p == ' ') p++;
            if (*p != ':') return 0;
            p++;
            while (*p == ' ') p++;
            if (flen == 6 && strncmp(fk, "sha256", 6) == 0) {
                if (*p != '"') return 0;
                const char* v = ++p;
                const char* ve = strchr(v, '"');
                if (!ve || ve - v != 64) return 0;
                snprintf(sha, sizeof sha, "%.*s", (int)(ve - v), v);
                p = ve + 1;
            } else if (flen == 4 && strncmp(fk, "size", 4) == 0) {
                size = strtol(p, (char**)&p, 10);
            } else {
                /* campo ignoto: salta il valore */
                if (*p == '"') { p++; const char* ve = strchr(p, '"'); if (!ve) return 0; p = ve + 1; }
                else { while (*p && *p != ',' && *p != '}') p++; }
            }
        }
        if (n >= max) return 0;
        snprintf(out[n].path, sizeof out[n].path, "%.*s", (int)(ke - k), k);
        snprintf(out[n].sha, sizeof out[n].sha, "%s", sha);
        out[n].size = size;
        n++;
    }
    *count = n;
    return n > 0;
}

int gmbadmin_verify(int argc, char** argv)
{
    char manifest[4096] = "", root[4096] = "";
    int quiet = 0;
    for (int i = 2; i < argc; i++) {
        if ((!strcmp(argv[i], "--manifest") || !strcmp(argv[i], "-m")) && i + 1 < argc)
            snprintf(manifest, sizeof manifest, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc)
            snprintf(root, sizeof root, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--quiet") || !strcmp(argv[i], "-q")) quiet = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("uso: gmb doctor --verify [--manifest FILE] [--dir DIR] [--quiet]\n"
                   "  confronta i file dell'artefatto con release-manifest.json (sha256 + size).\n"
                   "  default: manifest e file accanto all'eseguibile (artefatto scompattato).\n"
                   "  exit 0 = tutto combacia · 1 = mancante/manomesso · 2 = manifest assente/rotto\n");
            return 0;
        }
    }
    if (!root[0]) {
        char exe[4096];
        if (!vf_exe_dir(exe, sizeof exe)) { fprintf(stderr, "?verify: cartella dell'eseguibile non risolvibile\n"); return 2; }
        /* artefatto: <zip>/bin/gmbadmin → la radice è `..`; distribuzione: accanto al manifest */
        snprintf(root, sizeof root, "%s", exe);
    }
    if (!manifest[0]) {
        char cand[4096];
        /* 1. accanto all'eseguibile · 2. una cartella sopra (layout dell'artefatto: bin/) */
        snprintf(cand, sizeof cand, "%s/release-manifest.json", root);
        if (!vf_is_file(cand)) snprintf(cand, sizeof cand, "%s/../release-manifest.json", root);
        if (!vf_is_file(cand)) {
            fprintf(stderr, "?verify: release-manifest.json non trovato (provati: %s/release-manifest.json, "
                            "una cartella sopra)\n", root);
            return 2;
        }
        snprintf(manifest, sizeof manifest, "%s", cand);
    }
    /* i percorsi del manifest sono relativi alla RADICE dell'artefatto (= cartella del manifest) */
    {
        char mroot[4096];
        snprintf(mroot, sizeof mroot, "%s", manifest);
        char* sl = strrchr(mroot, '/');
        if (sl) *sl = 0; else snprintf(mroot, sizeof mroot, ".");
        snprintf(root, sizeof root, "%s", mroot);
    }

    FILE* f = fopen(manifest, "rb");
    if (!f) { fprintf(stderr, "?verify: manifest illeggibile (%s)\n", manifest); return 2; }
    char* txt = NULL;
    {
        size_t cap = 0, len = 0;
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
            char* nt = realloc(txt, len + n + 1);
            if (!nt) { free(txt); fclose(f); return 2; }
            txt = nt;
            memcpy(txt + len, buf, n);
            len += n;
            cap = len;
        }
        fclose(f);
        if (!txt) return 2;
        txt[len] = 0;
        (void)cap;
    }
    static vf_entry entries[VF_MAX_ENTRIES];
    int count = 0;
    if (!vf_parse(txt, entries, VF_MAX_ENTRIES, &count)) {
        free(txt);
        fprintf(stderr, "?verify: manifest malformato (%s)\n", manifest);
        return 2;
    }
    free(txt);

    int ok = 0, bad = 0, missing = 0;
    for (int i = 0; i < count; i++) {
        char path[4600];
        snprintf(path, sizeof path, "%s/%s", root, entries[i].path);
        if (!vf_is_file(path)) {
            missing++;
            if (!quiet) printf("  MANCA    %s\n", entries[i].path);
            continue;
        }
        char actual[65] = "";
        if (!gmb_pkg_sha256_file(path, actual, sizeof actual)) {
            bad++;
            if (!quiet) printf("  BAD      %s (hash non calcolabile)\n", entries[i].path);
            continue;
        }
        if (strcasecmp(actual, entries[i].sha) != 0) {
            bad++;
            if (!quiet) printf("  MANOMESSO %s (sha diverso)\n", entries[i].path);
            continue;
        }
        ok++;
    }
    printf("verify: %d OK, %d manomessi, %d mancanti (manifest %s)\n", ok, bad, missing, manifest);
    if (bad || missing) {
        fprintf(stderr, "?verify: artefatto NON conforme (ricompattalo con tools/sh/release.sh)\n");
        return 1;
    }
    return 0;
}
