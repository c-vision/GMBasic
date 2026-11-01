/* native/repl/pkg_cmds.c — F3b: comandi di PACCHETTO nel mondo C (manifest, keyring, cache).
 *
 * Nati nella REPL C# (CliPackages), qui usano l'API del runtime C (`gmb_pkg_*`):
 *   ADD <nome[@ver]> [--dev] [--dir d]   → gmb_pkg_manifest_set_dep
 *   REMOVE <nome> [--dir d]              → gmb_pkg_manifest_remove_dep
 *   BUMP <major|minor|patch|X.Y.Z> [--dir d] → bump del campo "version"
 *   TRUST <list | <nome> <chiave-pem|file>>  → keyring (gmb_pkg_keyring_*)
 *   CACHE [LIST|GC|DEDUP]                → content-addr (gmb_pkg_content_gc)
 * RESTORE/UPDATE/ROLLBACK richiedono il resolver/registry (gmb_pkg_resolve_*): F3c.
 */
#include "repl.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_trust.h"
#include "gmb_pkg_contentaddr.h"
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static void pkg_manifest_path(const char* dir, char* out, size_t cap)
{
    if (dir && *dir) snprintf(out, cap, "%s/gmb-package.json", dir);
    else snprintf(out, cap, "gmb-package.json");
}

static int pkg_bump_file(const char* path, const char* how)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "?BUMP: %s non trovato\n", path); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = (char*)malloc((size_t)n + 1);
    if (!s) { fclose(f); return 1; }
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return 1; }
    s[n] = 0; fclose(f);

    char* v = strstr(s, "\"version\"");
    v = v ? strchr(v, ':') : NULL;
    if (!v) { fprintf(stderr, "?BUMP: campo \"version\" assente\n"); free(s); return 1; }
    v = strchr(v, '"');
    char* e = v ? strchr(v + 1, '"') : NULL;
    if (!v || !e) { fprintf(stderr, "?BUMP: version non parsabile\n"); free(s); return 1; }
    char old[64]; int ol = (int)(e - v - 1); if (ol >= (int)sizeof(old)) ol = sizeof(old) - 1;
    memcpy(old, v + 1, (size_t)ol); old[ol] = 0;

    unsigned a = 0, b = 0, c = 0;
    if (sscanf(old, "%u.%u.%u", &a, &b, &c) != 3) { fprintf(stderr, "?BUMP: version '%s' non semver\n", old); free(s); return 1; }
    if (!strcasecmp(how, "major")) { a++; b = 0; c = 0; }
    else if (!strcasecmp(how, "minor")) { b++; c = 0; }
    else if (!strcasecmp(how, "patch")) { c++; }
    else if (strchr(how, '.')) { /* versione esplicita */ }
    else { fprintf(stderr, "?BUMP: usa major|minor|patch|X.Y.Z\n"); free(s); return 1; }

    char now[64];
    if (strchr(how, '.')) snprintf(now, sizeof(now), "%s", how);
    else snprintf(now, sizeof(now), "%u.%u.%u", a, b, c);
    size_t head = (size_t)(v + 1 - s), tail = strlen(e);   /* e = virgoletta di chiusura */
    FILE* o = fopen(path, "wb");
    if (!o) { fprintf(stderr, "?BUMP: scrittura fallita\n"); free(s); return 1; }
    fwrite(s, 1, head, o); fwrite(now, 1, strlen(now), o); fwrite(e, 1, tail, o);
    fclose(o); free(s);
    printf("BUMP: %s -> %s (%s)\n", old, now, path);
    return 0;
}

int rpl_pkg_cmd(const char* cmd, const char* args)
{
    char a1[256] = "", a2[1024] = "", dir[1024] = "";
    const char* p = args;
    while (*p == ' ') p++;
    sscanf(p, "%255s", a1);
    /* --dir <d> */
    const char* dpos = strstr(args, "--dir");
    if (dpos) { const char* q = dpos + 5; while (*q == ' ') q++; sscanf(q, "%1023s", dir); }

    if (!strcasecmp(cmd, "ADD")) {
        if (!a1[0]) { fprintf(stderr, "?uso: ADD <nome[@ver]> [--dev] [--dir <progetto>]\n"); return 1; }
        char name[256], spec[256] = "latest";
        char* at = strchr(a1, '@');
        if (at) { size_t l = (size_t)(at - a1); if (l >= sizeof(name)) l = sizeof(name) - 1; memcpy(name, a1, l); name[l] = 0; snprintf(spec, sizeof(spec), "%s", at + 1); }
        else snprintf(name, sizeof(name), "%s", a1);
        const char* section = strstr(args, "--dev") ? "devDependencies" : "dependencies";
        char mp[1200]; pkg_manifest_path(dir, mp, sizeof(mp));
        int ok = gmb_pkg_manifest_set_dep(mp, section, name, spec);
        if (ok) printf("ADD: %s@%s -> %s (%s)\n", name, spec, section, mp);
        else    fprintf(stderr, "?ADD fallito: %s\n", mp);
        return ok ? 0 : 1;
    }
    if (!strcasecmp(cmd, "REMOVE")) {
        if (!a1[0]) { fprintf(stderr, "?uso: REMOVE <nome> [--dir <progetto>]\n"); return 1; }
        char mp[1200]; pkg_manifest_path(dir, mp, sizeof(mp));
        int ok = gmb_pkg_manifest_remove_dep(mp, a1);
        if (ok) printf("REMOVE: %s (%s)\n", a1, mp);
        else    fprintf(stderr, "?REMOVE: %s non trovato (%s)\n", a1, mp);
        return ok ? 0 : 1;
    }
    if (!strcasecmp(cmd, "BUMP")) {
        char mp[1200]; pkg_manifest_path(dir, mp, sizeof(mp));
        return pkg_bump_file(mp, a1[0] ? a1 : "patch");
    }
    if (!strcasecmp(cmd, "TRUST")) {
        char bucket[1200];
        { char cfg[1024]; gmb_paths_config_dir(cfg, sizeof(cfg)); snprintf(bucket, sizeof(bucket), "%s/keyring.json", cfg); }
        if (!a1[0] || !strcasecmp(a1, "LIST")) {
            printf("keyring: %s\n", bucket);
            return 0;   /* l'elenco dettagliato arriva con la lettura del keyring (F3c) */
        }
        /* TRUST <nome> <chiave-o-file> */
        const char* rest = p; rest += strlen(a1); while (*rest == ' ') rest++;
        snprintf(a2, sizeof(a2), "%s", rest);
        char key[8192] = "";
        FILE* kf = fopen(a2, "rb");
        if (kf) { size_t n = fread(key, 1, sizeof(key) - 1, kf); key[n] = 0; fclose(kf); }
        else snprintf(key, sizeof(key), "%s", a2);
        if (!key[0]) { fprintf(stderr, "?uso: TRUST <nome> <chiave-pem | file.pem>\n"); return 1; }
        {   /* togli le virgolette di shell/REPL attorno alla chiave */
            char* b = key; while (*b == '"' || *b == '\'') b++;
            size_t l = strlen(b);
            while (l > 0 && (b[l-1] == '"' || b[l-1] == '\'')) b[--l] = 0;
            if (b != key) memmove(key, b, l + 1);
        }
        int ok = gmb_pkg_keyring_set(bucket, a1, key);
        if (ok) printf("TRUST: %s aggiunta al keyring (%s)\n", a1, bucket);
        else    fprintf(stderr, "?TRUST: aggiunta fallita (%s)\n", bucket);
        return ok ? 0 : 1;
    }
    if (!strcasecmp(cmd, "CACHE")) {
        char content[1200], cache[1200];
        { char cfg[1024]; gmb_paths_config_dir(cfg, sizeof(cfg));
          snprintf(content, sizeof(content), "%s/content", cfg);
          snprintf(cache, sizeof(cache), "%s/packages", cfg); }
        if (!a1[0] || !strcasecmp(a1, "LIST")) {
            printf("content-addressable: %s\ncache pacchetti:      %s\n", content, cache);
            return 0;
        }
        if (!strcasecmp(a1, "GC") || !strcasecmp(a1, "DEDUP") || !strcasecmp(a1, "CLEAN")) {
            int rc = gmb_pkg_content_gc(content, cache);
            printf(rc >= 0 ? "CACHE: gc eseguito (riferimenti rimossi: %d)\n" : "?CACHE gc: errore (%d)\n", rc);
            return rc >= 0 ? 0 : 1;
        }
        fprintf(stderr, "?uso: CACHE [LIST|GC]\n");
        return 1;
    }
    fprintf(stderr, "?%s: comando non gestito\n", cmd);
    return 1;
}
