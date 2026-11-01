/* native/repl/pkg_cmds.c — comandi di PACCHETTO nel mondo C (manifest, resolver, lock, keyring, cache).
 *
 * Nati nella REPL C# (CliPackages), qui usano SOLO l'API del runtime C (`gmb_pkg_*`):
 *   ADD <nome[@ver]> [--dev] [--dir d]        → gmb_pkg_manifest_set_dep
 *   REMOVE <nome> [--dir d]                   → gmb_pkg_manifest_remove_dep
 *   BUMP <major|minor|patch|X.Y.Z> [--dir d]  → bump del campo "version"
 *   RESTORE [dir] [--locked] [--registry URL] → resolver + lock (`gmb_pkg_resolve`, `gmb_pkg_lock_write_with_history`)
 *   UPDATE [dir] [--check] [--registry URL]   → diff fra lock e versione risolta, poi nuovo lock
 *   ROLLBACK [dir] [--list] [--to <prefisso>] → ripristina un lock dalla lock-history e re-installa
 *   TRUST [LIST|ADD|REMOVE|FINGERPRINT] …     → keyring (`gmb_pkg_keyring_*`, `gmb_pkg_key_fingerprint`)
 *   CACHE [LIST|GC|DEDUP|CLEAN [--force]]     → content-addressable (`gmb_pkg_content_*`)
 *
 * Nessuna chiamata a C# e nessun `system()`: tutto passa dal runtime C.
 */
#include "repl.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_trust.h"
#include "gmb_pkg_contentaddr.h"
#include "gmb_pkg_lock.h"
#include "gmb_pkg_resolver.h"
#include "gmb_pkg_registry.h"
#include "gmb_pkg_util.h"
#include "gmb_fsutil.h"
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define PKG_LOCK_NAME "gmb-package.lock"

static void pkg_manifest_path(const char* dir, char* out, size_t cap)
{
    if (dir && *dir) snprintf(out, cap, "%s/gmb-package.json", dir);
    else snprintf(out, cap, "gmb-package.json");
}

/* Radice del progetto: `--dir`, primo argomento posizionale, oppure la CWD. */
static int pkg_root_of(const char* dir, const char* positional, char* out, size_t cap)
{
    if (dir && *dir) { snprintf(out, cap, "%s", dir); return 0; }
    if (positional && *positional && positional[0] != '-') { snprintf(out, cap, "%s", positional); return 0; }
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) return 1;
    snprintf(out, cap, "%s", cwd);
    return 0;
}

static void pkg_cache_root(const char* args, char* out, size_t cap)
{
    const char* cp = args ? strstr(args, "--cache") : NULL;
    if (cp) {
        char d[1024] = "";
        sscanf(cp + 7, " %1023s", d);
        if (d[0]) { snprintf(out, cap, "%s", d); return; }
    }
    /* cache canonica dei pacchetti: ~/.gmbasic/packages/<nome>/<versione>/ (AGENTS §0octies) */
    gmb_paths_packages_dir(out, cap);
}

/* Registry: `--registry URL` → config utente (`RegistryUrl`) → default del PM. */
static void pkg_registry_url(const char* args, char* out, size_t cap)
{
    out[0] = 0;
    const char* rp = args ? strstr(args, "--registry") : NULL;
    if (rp) sscanf(rp + 10, " %511s", out);
    if (!out[0]) {
        char cfg[1024];
        gmb_paths_config_dir(cfg, sizeof(cfg));
        char cf[1200];
        snprintf(cf, sizeof(cf), "%s/config.json", cfg);
        FILE* f = fopen(cf, "rb");
        if (f) {
            char b[8192];
            size_t n = fread(b, 1, sizeof(b) - 1, f);
            b[n] = 0;
            fclose(f);
            const char* k = strstr(b, "\"RegistryUrl\"");
            if (!k) k = strstr(b, "\"registryUrl\"");
            if (k) {
                const char* q = strchr(k, ':');
                q = q ? strchr(q, '"') : NULL;
                const char* e = q ? strchr(q + 1, '"') : NULL;
                if (q && e) {
                    size_t l = (size_t)(e - q - 1);
                    if (l >= cap) l = cap - 1;
                    memcpy(out, q + 1, l);
                    out[l] = 0;
                }
            }
        }
    }
    if (!out[0]) snprintf(out, cap, "%s", "http://localhost:8087");
}

/* Legge la chiave da un file (se esiste) o la prende com'è, togliendo spazi/virgolette. */
static void pkg_key_arg(const char* s, char* out, size_t cap)
{
    out[0] = 0;
    if (!s || !*s) return;
    FILE* f = fopen(s, "rb");
    if (f) { size_t n = fread(out, 1, cap - 1, f); out[n] = 0; fclose(f); }
    else snprintf(out, cap, "%s", s);
    char* b = out;
    while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n' || *b == '"' || *b == '\'') b++;
    size_t l = strlen(b);
    while (l > 0) {
        char c = b[l - 1];
        if (c != '\r' && c != '\n' && c != '\t' && c != ' ' && c != '"' && c != '\'') break;
        b[--l] = 0;
    }
    if (b != out) memmove(out, b, l + 1);
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

/* ── Resolver: requisiti del manifest + pin del lock ───────────────────────── */

static int pkg_manifest_add_reqs(const char* root, gmb_pkg_resolve_ctx_t* c)
{
    char mp[1200];
    snprintf(mp, sizeof(mp), "%s/gmb-package.json", root);
    FILE* f = fopen(mp, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)n + 1);
    if (!js) { fclose(f); return 0; }
    if (fread(js, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(js); return 0; }
    js[n] = 0; fclose(f);
    gmb_pkg_manifest_t* m = gmb_pkg_manifest_parse(js);
    free(js);
    if (!m) return 0;

    int total = 0;
    static const char* secs[3] = { "dependencies", "devDependencies", "optionalDependencies" };
    for (int s = 0; s < 3; s++) {
        int nd = gmb_pkg_manifest_count(m, secs[s]);
        for (int i = 0; i < nd; i++) {
            char k[256] = "", v[128] = "";
            if (!gmb_pkg_manifest_item(m, secs[s], i, k, sizeof(k), v, sizeof(v))) continue;
            gmb_pkg_resolve_ctx_add_req(c, k, v[0] ? v : "*");
            total++;
        }
    }
    gmb_pkg_manifest_free(m);
    return total;
}

static int pkg_lock_add_locked(const char* root, gmb_pkg_resolve_ctx_t* c)
{
    char lp[1200];
    snprintf(lp, sizeof(lp), "%s/%s", root, PKG_LOCK_NAME);
    gmb_pkg_lock_t* l = gmb_pkg_lock_read(lp);
    if (!l) return 0;
    int n = gmb_pkg_lock_count(l), k = 0;
    for (int i = 0; i < n; i++) {
        char name[256] = "", ver[64] = "";
        if (!gmb_pkg_lock_entry(l, i, name, sizeof(name), ver, sizeof(ver), NULL, 0, NULL, 0, NULL, 0)) continue;
        if (name[0] && ver[0]) { gmb_pkg_resolve_ctx_add_locked(c, name, ver); k++; }
    }
    gmb_pkg_lock_free(l);
    return k;
}

static int pkg_lock_versions(const char* root, char (*names)[256], char (*vers)[64], int cap)
{
    char lp[1200];
    snprintf(lp, sizeof(lp), "%s/%s", root, PKG_LOCK_NAME);
    gmb_pkg_lock_t* l = gmb_pkg_lock_read(lp);
    if (!l) return 0;
    int n = gmb_pkg_lock_count(l), k = 0;
    for (int i = 0; i < n && k < cap; i++)
        if (gmb_pkg_lock_entry(l, i, names[k], 256, vers[k], 64, NULL, 0, NULL, 0, NULL, 0)) k++;
    gmb_pkg_lock_free(l);
    return k;
}

/* Risolve, stampa `nome@versione` e (se richiesto) scrive il lock con la history. */
static int pkg_resolve_and_lock(const char* root, const char* registry, const char* cache_dir, int locked, int write_lock, int show,
                                char (*oname)[256], char (*over)[64], int ocap, int* on)
{
    gmb_pkg_resolve_ctx_t* c = gmb_pkg_resolve_ctx_new();
    char cache[1024], plat[64] = "";
    if (cache_dir && *cache_dir) snprintf(cache, sizeof(cache), "%s", cache_dir);
    else pkg_cache_root(NULL, cache, sizeof(cache));
    gmb_pkg_platform_key(plat, sizeof(plat));
    gmb_pkg_resolve_ctx_set(c, cache, NULL, "il", plat, NULL, registry[0] ? 0 : 1);
    gmb_pkg_resolve_ctx_set_registry(c, registry);
    int nreq = pkg_manifest_add_reqs(root, c);
    if (locked) pkg_lock_add_locked(root, c);
    if (nreq == 0)
        printf("  (nessuna dipendenza dichiarata in %s/gmb-package.json)\n", root);

    gmb_pkg_resolve_result_t* r = gmb_pkg_resolve(c);
    gmb_pkg_resolve_ctx_free(c);

    const char* err = gmb_pkg_result_error(r);
    if (err) {
        fprintf(stderr, "?resolve: %s\n", err);
        gmb_pkg_result_free(r);
        return 1;
    }

    int n = gmb_pkg_result_count(r);
    size_t cap = 1 << 20, off = 0;
    char* body = (char*)malloc(cap);
    char* entry = (char*)malloc(1 << 16);
    if (!body || !entry) { free(body); free(entry); gmb_pkg_result_free(r); return 1; }
    body[0] = 0;
    for (int i = 0; i < n; i++) {
        char name[256], ver[64], idir[2048];
        if (!gmb_pkg_result_item(r, i, name, sizeof(name), ver, sizeof(ver), idir, sizeof(idir), entry, 1 << 16)) continue;
        if (show) printf("  %s@%s\n", name, ver);
        if (oname && over && i < ocap) {
            snprintf(oname[i], 256, "%s", name);
            snprintf(over[i], 64, "%s", ver);
        }
        int w = snprintf(body + off, cap - off, "%s\n", entry);
        if (w > 0) off += (size_t)w;
    }
    if (write_lock) {
        char lockp[1200];
        snprintf(lockp, sizeof(lockp), "%s/%s", root, PKG_LOCK_NAME);
        if (gmb_pkg_lock_write_with_history(root, lockp, body))
            printf("  lock: %s (%d pacchetti)\n", lockp, n);
        else
            fprintf(stderr, "?lock: scrittura fallita (%s)\n", lockp);
    }
    if (on) *on = n;
    free(entry);
    free(body);
    gmb_pkg_result_free(r);
    return 0;
}

/* ── CACHE ─────────────────────────────────────────────────────────────────── */

static int pkg_cache_list(const char* cache)
{
    DIR* d = opendir(cache);
    if (!d) { printf("cache vuota o assente: %s\n", cache); return 0; }
    struct dirent* de;
    int npkg = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char pd[1600];
        snprintf(pd, sizeof(pd), "%s/%s", cache, de->d_name);
        DIR* dv = opendir(pd);
        if (!dv) continue;
        struct stat st;
        if (stat(pd, &st) != 0 || !S_ISDIR(st.st_mode)) { closedir(dv); continue; }
        printf("%s:", de->d_name);
        struct dirent* ve;
        while ((ve = readdir(dv))) {
            if (ve->d_name[0] == '.') continue;
            printf(" %s", ve->d_name);
        }
        printf("\n");
        closedir(dv);
        npkg++;
    }
    closedir(d);
    if (!npkg) printf("(nessun pacchetto in %s)\n", cache);
    return 0;
}

static int pkg_cache_clean(const char* cache, int force)
{
    if (!force) {
        printf("CACHE CLEAN: operazione distruttiva su %s\n"
               "  rilancia con `CACHE CLEAN --force` per eliminare le versioni in cache.\n", cache);
        return 1;
    }
    DIR* d = opendir(cache);
    if (!d) { printf("cache assente: %s\n", cache); return 0; }
    char dirs[256][1024];
    int n = 0;
    struct dirent* de;
    while ((de = readdir(d)) && n < 256) {
        if (de->d_name[0] == '.') continue;
        snprintf(dirs[n++], sizeof(dirs[0]), "%s/%s", cache, de->d_name);
    }
    closedir(d);
    for (int i = 0; i < n; i++) gmb_fs_rmtree(dirs[i]);
    printf("CACHE: %d pacchetti rimossi da %s\n", n, cache);
    return 0;
}

int rpl_pkg_cmd(const char* cmd, const char* args)
{
    char a1[256] = "", a2[2048] = "", dir[1024] = "";
    const char* p = args;
    while (*p == ' ') p++;
    sscanf(p, "%255s", a1);
    const char* dpos = strstr(args, "--dir");
    if (dpos) { const char* q = dpos + 5; while (*q == ' ') q++; sscanf(q, "%1023s", dir); }

    if (!strcasecmp(cmd, "ADD")) {
        if (!a1[0]) { fprintf(stderr, "?uso: ADD <nome[@ver]> [--dev] [--dir <progetto>]\n"); return 1; }
        char name[256], spec[256] = "latest";
        char* at = strchr(a1, '@');
        if (at) {
            size_t l = (size_t)(at - a1);
            if (l >= sizeof(name)) l = sizeof(name) - 1;
            memcpy(name, a1, l); name[l] = 0;
            snprintf(spec, sizeof(spec), "%s", at + 1);
        } else snprintf(name, sizeof(name), "%s", a1);
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
        /* stesso keyring del PM C#: ~/.gmbasic/trusted-keys.json */
        char bucket[1200];
        { const char* home = getenv("HOME");
          if (home && *home) snprintf(bucket, sizeof(bucket), "%s/.gmbasic/trusted-keys.json", home);
          else snprintf(bucket, sizeof(bucket), "trusted-keys.json"); }

        char sub[64] = "";
        sscanf(p, "%63s", sub);
        int is_sub = !strcasecmp(sub, "LIST") || !strcasecmp(sub, "SHOW")
                  || !strcasecmp(sub, "ADD") || !strcasecmp(sub, "REMOVE") || !strcasecmp(sub, "RM")
                  || !strcasecmp(sub, "FINGERPRINT") || !strcasecmp(sub, "FP");
        char name[256] = "", keysrc[2048] = "";
        if (is_sub) {
            const char* r = p + strlen(sub);
            while (*r == ' ') r++;
            sscanf(r, "%255s", name);
            r += strlen(name);
            while (*r == ' ') r++;
            snprintf(keysrc, sizeof(keysrc), "%s", r);
        } else {
            snprintf(name, sizeof(name), "%s", sub);
            snprintf(keysrc, sizeof(keysrc), "%s", p + strlen(sub));
        }

        if (!sub[0] || !strcasecmp(sub, "LIST") || !strcasecmp(sub, "SHOW")) {
            int n = gmb_pkg_keyring_count(bucket);
            printf("keyring: %s (%d chiavi)\n", bucket, n);
            for (int i = 0; i < n; i++) {
                char kn[256] = "", kv[8192] = "", fp[64] = "";
                if (!gmb_pkg_keyring_item(bucket, i, kn, sizeof(kn), kv, sizeof(kv))) continue;
                gmb_pkg_key_fingerprint(kv, fp, sizeof(fp));
                printf("  %-20s %s\n", kn, fp[0] ? fp : "(fingerprint n/d)");
            }
            return 0;
        }
        if (!strcasecmp(sub, "FINGERPRINT") || !strcasecmp(sub, "FP")) {
            if (!name[0]) { fprintf(stderr, "?uso: TRUST FINGERPRINT <chiave | file.pem>\n"); return 1; }
            char key[8192] = "", fp[64] = "";
            pkg_key_arg(name, key, sizeof(key));
            gmb_pkg_key_fingerprint(key, fp, sizeof(fp));
            printf("%s\n", fp[0] ? fp : "(fingerprint non calcolabile)");
            return 0;
        }
        if (!strcasecmp(sub, "REMOVE") || !strcasecmp(sub, "RM")) {
            if (!name[0]) { fprintf(stderr, "?uso: TRUST REMOVE <nome>\n"); return 1; }
            int ok = gmb_pkg_keyring_remove(bucket, name);
            if (ok) printf("TRUST: %s rimossa (%s)\n", name, bucket);
            else    fprintf(stderr, "?TRUST: %s non presente\n", name);
            return ok ? 0 : 1;
        }
        /* ADD / forma legacy `TRUST <nome> <chiave|file>` */
        if (!name[0]) { fprintf(stderr, "?uso: TRUST ADD <nome> <chiave-pem | file.pem>\n"); return 1; }
        {
            char key[8192] = "", fp[64] = "";
            pkg_key_arg(keysrc, key, sizeof(key));
            if (!key[0]) { fprintf(stderr, "?uso: TRUST ADD <nome> <chiave-pem | file.pem>\n"); return 1; }
            int ok = gmb_pkg_keyring_set(bucket, name, key);
            if (!ok) { fprintf(stderr, "?TRUST: aggiunta fallita (%s)\n", bucket); return 1; }
            gmb_pkg_key_fingerprint(key, fp, sizeof(fp));
            printf("TRUST: %s (%s) -> %s\n", name, fp[0] ? fp : "fingerprint n/d", bucket);
            return 0;
        }
    }
    if (!strcasecmp(cmd, "CACHE")) {
        char content[1200], cache[1200];
        /* il content-addressable vive DENTRO la cache (`<cache>/content`), come nel PM C# */
        pkg_cache_root(args, cache, sizeof(cache));
        snprintf(content, sizeof(content), "%s/content", cache);

        if (!a1[0] || !strcasecmp(a1, "LIST")) {
            printf("content-addressable: %s\n", content);
            return pkg_cache_list(cache);
        }
        if (!strcasecmp(a1, "GC")) {
            int rc = gmb_pkg_content_gc(content, cache);
            if (rc < 0) { fprintf(stderr, "?CACHE gc: errore (%d)\n", rc); return 1; }
            printf("CACHE: gc eseguito (blob non referenziati rimossi: %d)\n", rc);
            return 0;
        }
        if (!strcasecmp(a1, "DEDUP")) {
            DIR* d = opendir(cache);
            if (!d) { printf("cache assente: %s\n", cache); return 0; }
            int interned = 0;
            struct dirent* de;
            while ((de = readdir(d))) {
                if (de->d_name[0] == '.') continue;
                char pd[1600];
                snprintf(pd, sizeof(pd), "%s/%s", cache, de->d_name);
                struct stat st;
                if (stat(pd, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
                int n = gmb_pkg_content_intern_dir(content, pd);
                if (n > 0) interned += n;
            }
            closedir(d);
            printf("CACHE: dedup — %d file internati in %s\n", interned, content);
            return 0;
        }
        if (!strcasecmp(a1, "CLEAN")) {
            return pkg_cache_clean(cache, strstr(args, "--force") != NULL);
        }
        fprintf(stderr, "?uso: CACHE [LIST|GC|DEDUP|CLEAN [--force]]\n");
        return 1;
    }
    if (!strcasecmp(cmd, "RESTORE")) {
        char root[1024];
        if (pkg_root_of(dir, a1, root, sizeof(root))) { fprintf(stderr, "?RESTORE: cwd\n"); return 1; }
        char reg[512], cache[1024];
        pkg_registry_url(args, reg, sizeof(reg));
        pkg_cache_root(args, cache, sizeof(cache));
        int locked = strstr(args, "--locked") != NULL;
        printf("RESTORE %s (registry: %s%s)\n", root, reg[0] ? reg : "(cache)", locked ? ", locked" : "");
        int rc = pkg_resolve_and_lock(root, reg, cache, locked, 1, 1, NULL, NULL, 0, NULL);
        return rc;
    }
    if (!strcasecmp(cmd, "UPDATE")) {
        char root[1024];
        if (pkg_root_of(dir, a1, root, sizeof(root))) { fprintf(stderr, "?UPDATE: cwd\n"); return 1; }
        char reg[512], cache[1024];
        pkg_registry_url(args, reg, sizeof(reg));
        pkg_cache_root(args, cache, sizeof(cache));
        int check = strstr(args, "--check") != NULL;

        char oldn[256][256], oldv[256][64];
        int nold = pkg_lock_versions(root, (char (*)[256])oldn, (char (*)[64])oldv, 256);

        char newn[256][256], newv[256][64];
        int nnew = 0;
        printf("UPDATE %s (registry: %s%s)\n", root, reg[0] ? reg : "(cache)", check ? ", check" : "");
        /* Il resolver sceglie già la versione migliore ammessa dai range del manifest. */
        int rc = pkg_resolve_and_lock(root, reg, cache, 0, 0, 0,
                                      (char (*)[256])newn, (char (*)[64])newv, 256, &nnew);
        if (rc != 0) return rc;

        int changes = 0;
        for (int i = 0; i < nnew; i++) {
            const char* oldv2 = "(nuovo)";
            for (int j = 0; j < nold; j++)
                if (!strcasecmp(oldn[j], newn[i])) { oldv2 = oldv[j]; break; }
            if (strcmp(oldv2, newv[i]) != 0) {
                printf("  %s: %s -> %s\n", newn[i], oldv2, newv[i]);
                changes++;
            }
        }
        for (int j = 0; j < nold; j++) {
            int still = 0;
            for (int i = 0; i < nnew; i++) if (!strcasecmp(oldn[j], newn[i])) { still = 1; break; }
            if (!still) { printf("  %s: %s -> (rimosso)\n", oldn[j], oldv[j]); changes++; }
        }
        if (changes == 0) { printf("  tutto aggiornato (%d pacchetti)\n", nnew); return 0; }
        if (check) { printf("  (--check: nessuna modifica al lock)\n"); return 0; }
        return pkg_resolve_and_lock(root, reg, cache, 0, 1, 0, NULL, NULL, 0, NULL);
    }
    if (!strcasecmp(cmd, "ROLLBACK")) {
        char root[1024];
        if (pkg_root_of(dir, a1, root, sizeof(root))) { fprintf(stderr, "?ROLLBACK: cwd\n"); return 1; }
        char hist[1200];
        snprintf(hist, sizeof(hist), "%s/.gmbasic/lock-history", root);

        DIR* d = opendir(hist);
        char names[256][256];
        int n = 0;
        if (d) {
            struct dirent* de;
            while ((de = readdir(d)) && n < 256) {
                if (de->d_name[0] == '.') continue;
                size_t l = strlen(de->d_name);
                if (l < 5 || strcasecmp(de->d_name + l - 5, ".lock")) continue;
                snprintf(names[n++], sizeof(names[0]), "%s", de->d_name);
            }
            closedir(d);
            for (int i = 0; i < n - 1; i++)
                for (int j = i + 1; j < n; j++)
                    if (strcmp(names[j], names[i]) > 0) { char t[256]; memcpy(t, names[i], 256); memcpy(names[i], names[j], 256); memcpy(names[j], t, 256); }
        }

        if (strstr(args, "--list")) {
            if (n == 0) { printf("(nessuno storico lock in %s)\n", hist); return 0; }
            for (int i = 0; i < n; i++) printf("  %s\n", names[i]);
            return 0;
        }
        if (n == 0) { fprintf(stderr, "?ROLLBACK: nessuno storico lock in %s\n", hist); return 1; }

        const char* chosen = names[0];
        const char* to = strstr(args, "--to");
        if (to) {
            char pref[128] = "";
            sscanf(to + 4, " %127s", pref);
            chosen = NULL;
            for (int i = 0; i < n; i++)
                if (!strncasecmp(names[i], pref, strlen(pref))) { chosen = names[i]; break; }
            if (!chosen) { fprintf(stderr, "?ROLLBACK: lock '%s' non trovato (usa --list)\n", pref); return 1; }
        }

        char lockp[1200], src[1600];
        snprintf(lockp, sizeof(lockp), "%s/%s", root, PKG_LOCK_NAME);
        snprintf(src, sizeof(src), "%s/%s", hist, chosen);

        /* archivia il lock corrente: il rollback resta reversibile */
        struct stat st;
        if (stat(lockp, &st) == 0) {
            char stamp[64], dst[1600];
            time_t now = time(NULL);
            struct tm tmv;
            localtime_r(&now, &tmv);
            strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
            snprintf(dst, sizeof(dst), "%s/%s-rollback.lock", hist, stamp);
            FILE* in = fopen(lockp, "rb");
            FILE* out = fopen(dst, "wb");
            if (in && out) {
                char buf[8192]; size_t r;
                while ((r = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, r, out);
            }
            if (in) fclose(in);
            if (out) fclose(out);
        }
        {
            FILE* in = fopen(src, "rb");
            if (!in) { fprintf(stderr, "?ROLLBACK: %s\n", src); return 1; }
            FILE* out = fopen(lockp, "wb");
            if (!out) { fclose(in); fprintf(stderr, "?ROLLBACK: %s\n", lockp); return 1; }
            char buf[8192]; size_t r;
            while ((r = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, r, out);
            fclose(in); fclose(out);
        }
        printf("ROLLBACK: %s -> %s\n", src, lockp);

        char reg[512], cache[1024];
        pkg_registry_url(args, reg, sizeof(reg));
        pkg_cache_root(args, cache, sizeof(cache));
        return pkg_resolve_and_lock(root, reg, cache, 1, 1, 1, NULL, NULL, 0, NULL);
    }
    fprintf(stderr, "?%s: comando non gestito\n", cmd);
    return 1;
}
