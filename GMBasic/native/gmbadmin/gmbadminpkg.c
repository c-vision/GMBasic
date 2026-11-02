/* gmbadminpkg.c — sottocomando `pkg` del CLI amministrativo C (`gmbadmin`).
 *
 * Implementazione C del Package Manager CLI (ex GMBasicSharp/Cli + REPL):
 *   init · add · remove · bump · trust · cache · restore · update · rollback ·
 *   install · uninstall · list · info
 * Usa SOLO il runtime C (`gmb_pkg_*`): il C# diventa un wrapper P/Invoke.
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_lock.h"
#include "gmb_pkg_trust.h"
#include "gmb_pkg_resolver.h"
#include "gmb_pkg_registry.h"
#include "gmb_pkg_contentaddr.h"
#include "gmb_pkg_util.h"
#include "gmb_fsutil.h"
#include "gmb_compress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define GMBADMIN_LOCK "gmb-package.lock"

void gmbadmin_cache_root(char* out, size_t cap)
{
    gmb_paths_packages_dir(out, cap);
}

void gmbadmin_keyring(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    if (home && *home) snprintf(out, cap, "%s/.gmbasic/trusted-keys.json", home);
    else snprintf(out, cap, "trusted-keys.json");
}

int gmbadmin_registry_url(int argc, char** argv, char* out, size_t cap)
{
    out[0] = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--registry") && i + 1 < argc) { snprintf(out, cap, "%s", argv[i + 1]); return 0; }
        if (!strncmp(argv[i], "--registry=", 11)) { snprintf(out, cap, "%s", argv[i] + 11); return 0; }
    }
    char cfg[1024];
    gmb_paths_config_dir(cfg, sizeof cfg);
    char cf[1200];
    snprintf(cf, sizeof cf, "%s/config.json", cfg);
    FILE* f = fopen(cf, "rb");
    if (f) {
        char b[8192];
        size_t n = fread(b, 1, sizeof b - 1, f);
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
                memcpy(out, q + 1, l); out[l] = 0;
            }
        }
    }
    if (!out[0]) snprintf(out, cap, "%s", "http://localhost:8087");
    return 0;
}

/* ── argomenti ───────────────────────────────────────────────────────────── */

static const char* opt_val(int argc, char** argv, const char* name)
{
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], name)) return (i + 1 < argc) ? argv[i + 1] : NULL;
        size_t n = strlen(name);
        if (!strncmp(argv[i], name, n) && argv[i][n] == '=') return argv[i] + n + 1;
    }
    return NULL;
}
static int flag(int argc, char** argv, const char* name)
{
    for (int i = 2; i < argc; i++) if (!strcmp(argv[i], name)) return 1;
    return 0;
}
static const char* first_pos(int argc, char** argv, int skip)
{
    int seen = 0;
    for (int i = 3; i < argc; i++) {   /* argv[1]=pkg, argv[2]=sottocomando */

        if (argv[i][0] == '-') {
            if (!strchr(argv[i], '=') && i + 1 < argc && argv[i + 1][0] != '-') i++;
            continue;
        }
        if (seen++ == skip) return argv[i];
    }
    return NULL;
}

static void dir_of_arg(int argc, char** argv, char* out, size_t cap)
{
    const char* d = opt_val(argc, argv, "--dir");
    if (d && *d) { snprintf(out, cap, "%s", d); return; }
    char cwd[1024];
    if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
    snprintf(out, cap, "%s", cwd);
}

static void manifest_path(const char* dir, char* out, size_t cap)
{
    snprintf(out, cap, "%s/gmb-package.json", (dir && *dir) ? dir : ".");
}

/* ── bump ────────────────────────────────────────────────────────────────── */

static int pkg_bump(const char* path, const char* how)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "?bump: %s non trovato\n", path); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* s = (char*)malloc((size_t)n + 1);
    if (!s) { fclose(f); return 1; }
    if (fread(s, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s); return 1; }
    s[n] = 0; fclose(f);

    char* v = strstr(s, "\"version\"");
    v = v ? strchr(v, ':') : NULL;
    v = v ? strchr(v, '"') : NULL;
    char* e = v ? strchr(v + 1, '"') : NULL;
    if (!v || !e) { fprintf(stderr, "?bump: campo version mancante\n"); free(s); return 1; }
    char old[64];
    int ol = (int)(e - v - 1);
    if (ol >= (int)sizeof old) ol = sizeof old - 1;
    memcpy(old, v + 1, (size_t)ol); old[ol] = 0;

    unsigned a = 0, b = 0, c = 0;
    if (sscanf(old, "%u.%u.%u", &a, &b, &c) != 3) { fprintf(stderr, "?bump: '%s' non semver\n", old); free(s); return 1; }
    char now[64];
    if (strchr(how, '.')) snprintf(now, sizeof now, "%s", how);
    else if (!strcasecmp(how, "major")) snprintf(now, sizeof now, "%u.0.0", a + 1);
    else if (!strcasecmp(how, "minor")) snprintf(now, sizeof now, "%u.%u.0", a, b + 1);
    else if (!strcasecmp(how, "patch")) snprintf(now, sizeof now, "%u.%u.%u", a, b, c + 1);
    else { fprintf(stderr, "?bump: usa major|minor|patch|X.Y.Z\n"); free(s); return 1; }

    size_t head = (size_t)(v + 1 - s), tail = strlen(e);
    FILE* o = fopen(path, "wb");
    if (!o) { fprintf(stderr, "?bump: scrittura fallita\n"); free(s); return 1; }
    fwrite(s, 1, head, o); fwrite(now, 1, strlen(now), o); fwrite(e, 1, tail, o);
    fclose(o); free(s);
    printf("%s -> %s (%s)\n", old, now, path);
    return 0;
}

/* ── resolver: requisiti, lock, restore/update ───────────────────────────── */

static int manifest_add_reqs(const char* root, gmb_pkg_resolve_ctx_t* c)
{
    char mp[1200];
    manifest_path(root, mp, sizeof mp);
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
            if (!gmb_pkg_manifest_item(m, secs[s], i, k, sizeof k, v, sizeof v)) continue;
            gmb_pkg_resolve_ctx_add_req(c, k, v[0] ? v : "*");
            total++;
        }
    }
    gmb_pkg_manifest_free(m);
    return total;
}

static int lock_add_locked(const char* root, gmb_pkg_resolve_ctx_t* c)
{
    char lp[1200];
    snprintf(lp, sizeof lp, "%s/%s", root, GMBADMIN_LOCK);
    gmb_pkg_lock_t* l = gmb_pkg_lock_read(lp);
    if (!l) return 0;
    int n = gmb_pkg_lock_count(l), k = 0;
    for (int i = 0; i < n; i++) {
        char name[256] = "", ver[64] = "";
        if (!gmb_pkg_lock_entry(l, i, name, sizeof name, ver, sizeof ver, NULL, 0, NULL, 0, NULL, 0)) continue;
        if (name[0] && ver[0]) { gmb_pkg_resolve_ctx_add_locked(c, name, ver); k++; }
    }
    gmb_pkg_lock_free(l);
    return k;
}

static int lock_versions(const char* root, char (*names)[256], char (*vers)[64], int cap)
{
    char lp[1200];
    snprintf(lp, sizeof lp, "%s/%s", root, GMBADMIN_LOCK);
    gmb_pkg_lock_t* l = gmb_pkg_lock_read(lp);
    if (!l) return 0;
    int n = gmb_pkg_lock_count(l), k = 0;
    for (int i = 0; i < n && k < cap; i++)
        if (gmb_pkg_lock_entry(l, i, names[k], 256, vers[k], 64, NULL, 0, NULL, 0, NULL, 0)) k++;
    gmb_pkg_lock_free(l);
    return k;
}

static int resolve_and_lock(const char* root, const char* registry, const char* cache,
                            int locked, int write_lock, int show)
{
    gmb_pkg_resolve_ctx_t* c = gmb_pkg_resolve_ctx_new();
    char plat[64] = "";
    gmb_pkg_platform_key(plat, sizeof plat);
    gmb_pkg_resolve_ctx_set(c, cache, NULL, "il", plat, NULL, registry[0] ? 0 : 1);
    gmb_pkg_resolve_ctx_set_registry(c, registry);
    int nreq = manifest_add_reqs(root, c);
    if (locked) lock_add_locked(root, c);
    if (nreq == 0) printf("  (nessuna dipendenza dichiarata in %s/gmb-package.json)\n", root);

    gmb_pkg_resolve_result_t* r = gmb_pkg_resolve(c);
    gmb_pkg_resolve_ctx_free(c);
    const char* err = gmb_pkg_result_error(r);
    if (err) { fprintf(stderr, "?resolve: %s\n", err); gmb_pkg_result_free(r); return 1; }

    int n = gmb_pkg_result_count(r);
    size_t cap = 1 << 20, off = 0;
    char* body = (char*)malloc(cap);
    char* entry = (char*)malloc(1 << 16);
    if (!body || !entry) { free(body); free(entry); gmb_pkg_result_free(r); return 1; }
    body[0] = 0;
    for (int i = 0; i < n; i++) {
        char name[256], ver[64], idir[2048];
        if (!gmb_pkg_result_item(r, i, name, sizeof name, ver, sizeof ver, idir, sizeof idir, entry, 1 << 16)) continue;
        if (show) printf("  %s@%s\n", name, ver);
        int w = snprintf(body + off, cap - off, "%s\n", entry);
        if (w > 0) off += (size_t)w;
    }
    if (write_lock) {
        char lockp[1200];
        snprintf(lockp, sizeof lockp, "%s/%s", root, GMBADMIN_LOCK);
        if (gmb_pkg_lock_write_with_history(root, lockp, body)) printf("  lock: %s (%d pacchetti)\n", lockp, n);
        else fprintf(stderr, "?lock: scrittura fallita (%s)\n", lockp);
    }
    free(entry); free(body); gmb_pkg_result_free(r);
    return 0;
}

/* ── cache ───────────────────────────────────────────────────────────────── */

static int cache_list(const char* cache)
{
    DIR* d = opendir(cache);
    if (!d) { printf("cache vuota o assente: %s\n", cache); return 0; }
    struct dirent* de;
    int npkg = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        if (!strcmp(de->d_name, "content")) continue;   /* store content-addressable, non pacchetto */
        char pd[1600];
        snprintf(pd, sizeof pd, "%s/%s", cache, de->d_name);
        DIR* dv = opendir(pd);
        if (!dv) continue;
        printf("%s:", de->d_name);
        struct dirent* ve;
        while ((ve = readdir(dv))) if (ve->d_name[0] != '.') printf(" %s", ve->d_name);
        printf("\n");
        closedir(dv);
        npkg++;
    }
    closedir(d);
    if (!npkg) printf("(nessun pacchetto in %s)\n", cache);
    return 0;
}

static int cache_clean(const char* cache, int force)
{
    if (!force) {
        printf("cache clean: distruttiva su %s (rilancia con --force)\n", cache);
        return 1;
    }
    DIR* d = opendir(cache);
    if (!d) { printf("cache assente: %s\n", cache); return 0; }
    char dirs[256][1024];
    int n = 0;
    struct dirent* de;
    while ((de = readdir(d)) && n < 256) {
        if (de->d_name[0] == '.') continue;
        snprintf(dirs[n++], sizeof dirs[0], "%s/%s", cache, de->d_name);
    }
    closedir(d);
    for (int i = 0; i < n; i++) gmb_fs_rmtree(dirs[i]);
    printf("Cache pulita: %d pacchetti da %s\n", n, cache);
    return 0;
}

static int pkg_uninstall(const char* cache, const char* name)
{
    char d[1400];
    snprintf(d, sizeof d, "%s/%s", cache, name);
    struct stat st;
    if (stat(d, &st) != 0) { fprintf(stderr, "?uninstall: %s non in cache\n", name); return 1; }
    gmb_fs_rmtree(d);
    printf("%s rimosso dalla cache (%s)\n", name, cache);
    return 0;
}

static int pkg_install(const char* cache, const char* registry, const char* spec)
{
    char name[256], ver[64] = "latest";
    char* at = strchr(spec, '@');
    if (at) {
        size_t l = (size_t)(at - spec);
        if (l >= sizeof name) l = sizeof name - 1;
        memcpy(name, spec, l); name[l] = 0;
        snprintf(ver, sizeof ver, "%s", at + 1);
    } else snprintf(name, sizeof name, "%s", spec);

    char dest[1400];
    snprintf(dest, sizeof dest, "%s/%s/%s", cache, name, ver);
    gmb_fs_mkdir_p(dest);
    char zip[1500], qs[256];
    snprintf(zip, sizeof zip, "%s/.gmbadmin_%ld.zip", cache, (long)getpid());
    snprintf(qs, sizeof qs, "?backend=il&platform=any");
    if (!gmb_pkg_registry_download(registry, name, ver, qs, zip)) {
        fprintf(stderr, "?install: download fallito (%s %s da %s)\n", name, ver, registry);
        return 1;
    }
    int rc = gmb_unzip_files(zip, dest);
    unlink(zip);
    if (rc != 0) { fprintf(stderr, "?install: estrazione fallita (%s)\n", zip); return 1; }
    printf("installato: %s@%s -> %s\n", name, ver, dest);
    return 0;
}

/* ── main pkg ────────────────────────────────────────────────────────────── */

int gmbadmin_pkg(int argc, char** argv)
{
    const char* sub = (argc > 2) ? argv[2] : "help";
    char cache[1024];
    const char* copt = opt_val(argc, argv, "--cache");
    if (copt && *copt) snprintf(cache, sizeof cache, "%s", copt);
    else gmbadmin_cache_root(cache, sizeof cache);
    char registry[512];
    gmbadmin_registry_url(argc, argv, registry, sizeof registry);
    char dir[1024];
    dir_of_arg(argc, argv, dir, sizeof dir);
    /* restore/update/rollback accettano anche la dir POSIZIONALE (contratto REPL/gmb). */
    if (!opt_val(argc, argv, "--dir")) {
        const char* pos = first_pos(argc, argv, 0);
        if (pos && (strcmp(sub, "restore") == 0 || strcmp(sub, "update") == 0 || strcmp(sub, "rollback") == 0))
            snprintf(dir, sizeof dir, "%s", pos);
    }

    if (!strcmp(sub, "help") || !strcmp(sub, "--help") || !strcmp(sub, "-h")) {
        printf("uso: gmb pkg <init|add|remove|bump|list|info|install|uninstall|restore|update|trust|cache|rollback>\n");
        return 0;
    }
    if (!strcmp(sub, "init")) {
        const char* d = first_pos(argc, argv, 0);
        if (!d) { fprintf(stderr, "?uso: gmb pkg init <dir> --name X --version V [--type source]\n"); return 2; }
        const char* name = opt_val(argc, argv, "--name");
        const char* ver = opt_val(argc, argv, "--version");
        const char* type = opt_val(argc, argv, "--type");
        if (!name || !ver) { fprintf(stderr, "?init: --name e --version obbligatori\n"); return 2; }
        gmb_fs_mkdir_p(d);
        char mp[1200];
        manifest_path(d, mp, sizeof mp);
        if (access(mp, F_OK) == 0) { fprintf(stderr, "?init: %s esiste gia'\n", mp); return 1; }
        FILE* f = fopen(mp, "wb");
        if (!f) { fprintf(stderr, "?init: scrittura fallita\n"); return 1; }
        fprintf(f, "{\n  \"name\": \"%s\",\n  \"version\": \"%s\",\n  \"type\": \"%s\",\n  \"dependencies\": {}\n}\n",
                name, ver, type ? type : "source");
        fclose(f);
        printf("creato %s\n", mp);
        return 0;
    }
    if (!strcmp(sub, "add")) {
        const char* spec = first_pos(argc, argv, 0);
        if (!spec) { fprintf(stderr, "?uso: gmb pkg add <nome[@ver]> [--dev] [--dir d]\n"); return 2; }
        char name[256], ver[256] = "latest";
        char* at = strchr(spec, '@');
        if (at) {
            size_t l = (size_t)(at - spec);
            if (l >= sizeof name) l = sizeof name - 1;
            memcpy(name, spec, l); name[l] = 0;
            snprintf(ver, sizeof ver, "%s", at + 1);
        } else snprintf(name, sizeof name, "%s", spec);
        const char* section = flag(argc, argv, "--dev") ? "devDependencies" : "dependencies";
        char mp[1200];
        manifest_path(dir, mp, sizeof mp);
        if (!gmb_pkg_manifest_set_dep(mp, section, name, ver)) { fprintf(stderr, "?add fallito: %s\n", mp); return 1; }
        printf("%s@%s -> %s (%s)\n", name, ver, section, mp);
        return 0;
    }
    if (!strcmp(sub, "remove")) {
        const char* name = first_pos(argc, argv, 0);
        if (!name) { fprintf(stderr, "?uso: gmb pkg remove <nome> [--dir d]\n"); return 2; }
        char mp[1200];
        manifest_path(dir, mp, sizeof mp);
        if (!gmb_pkg_manifest_remove_dep(mp, name)) { fprintf(stderr, "?remove: %s non trovato (%s)\n", name, mp); return 1; }
        printf("%s rimosso (%s)\n", name, mp);
        return 0;
    }
    if (!strcmp(sub, "bump")) {
        const char* how = first_pos(argc, argv, 0);
        if (!how) { fprintf(stderr, "?uso: gmb pkg bump <major|minor|patch|X.Y.Z> [--dir d]\n"); return 2; }
        char mp[1200];
        manifest_path(dir, mp, sizeof mp);
        return pkg_bump(mp, how);
    }
    if (!strcmp(sub, "list")) return cache_list(cache);
    if (!strcmp(sub, "info")) {
        const char* name = first_pos(argc, argv, 0);
        if (!name) { fprintf(stderr, "?uso: gmb pkg info <nome> [--cache d]\n"); return 2; }
        char d[1400];
        snprintf(d, sizeof d, "%s/%s", cache, name);
        DIR* dv = opendir(d);
        if (!dv) { fprintf(stderr, "?info: %s non in cache\n", name); return 1; }
        printf("%s:", name);
        struct dirent* ve;
        while ((ve = readdir(dv))) if (ve->d_name[0] != '.') printf(" %s", ve->d_name);
        printf("\n");
        closedir(dv);
        return 0;
    }
    if (!strcmp(sub, "install")) {
        const char* spec = first_pos(argc, argv, 0);
        if (!spec) { fprintf(stderr, "?uso: gmb pkg install <nome[@ver]> [--cache d] [--registry url]\n"); return 2; }
        return pkg_install(cache, registry, spec);
    }
    if (!strcmp(sub, "uninstall")) {
        const char* name = first_pos(argc, argv, 0);
        if (!name) { fprintf(stderr, "?uso: gmb pkg uninstall <nome> [--cache d]\n"); return 2; }
        return pkg_uninstall(cache, name);
    }
    if (!strcmp(sub, "restore")) {
        printf("RESTORE %s (registry: %s%s)\n", dir, registry, flag(argc, argv, "--locked") ? ", locked" : "");
        return resolve_and_lock(dir, registry, cache, flag(argc, argv, "--locked"), 1, 1);
    }
    if (!strcmp(sub, "update")) {
        int check = flag(argc, argv, "--check");
        char oldn[256][256], oldv[256][64];
        int nold = lock_versions(dir, (char (*)[256])oldn, (char (*)[64])oldv, 256);
        char newn[256][256], newv[256][64];
        int nnew = 0;
        printf("UPDATE %s (registry: %s%s)\n", dir, registry, check ? ", check" : "");
        int rc = resolve_and_lock(dir, registry, cache, 0, 0, 0);
        (void)nnew;
        if (rc) return rc;
        /* confronto: la risoluzione ha già scelto; per il diff servono le coppie: si rilegge il lock nuovo */
        char ln[256][256], lv[256][64];
        int nl = lock_versions(dir, (char (*)[256])ln, (char (*)[64])lv, 256);
        int changes = 0;
        for (int i = 0; i < nl; i++) {
            const char* ov = "(nuovo)";
            for (int j = 0; j < nold; j++) if (!strcasecmp(oldn[j], ln[i])) { ov = oldv[j]; break; }
            if (strcmp(ov, lv[i]) != 0) { printf("  %s: %s -> %s\n", ln[i], ov, lv[i]); changes++; }
        }
        if (changes == 0) { printf("  tutto aggiornato (%d pacchetti)\n", nl); return 0; }
        if (check) { printf("  (--check: nessuna modifica al lock)\n"); return 0; }
        return resolve_and_lock(dir, registry, cache, 0, 1, 0);
    }
    if (!strcmp(sub, "rollback")) {
        char hist[1200];
        snprintf(hist, sizeof hist, "%s/.gmbasic/lock-history", dir);
        DIR* d = opendir(hist);
        char names[256][256];
        int n = 0;
        if (d) {
            struct dirent* de;
            while ((de = readdir(d)) && n < 256) {
                size_t l = strlen(de->d_name);
                if (de->d_name[0] == '.' || l < 5 || strcasecmp(de->d_name + l - 5, ".lock")) continue;
                snprintf(names[n++], sizeof names[0], "%s", de->d_name);
            }
            closedir(d);
            for (int i = 0; i < n - 1; i++)
                for (int j = i + 1; j < n; j++)
                    if (strcmp(names[j], names[i]) > 0) { char t[256]; memcpy(t, names[i], 256); memcpy(names[i], names[j], 256); memcpy(names[j], t, 256); }
        }
        if (flag(argc, argv, "--list")) {
            if (!n) { printf("(nessuno storico lock in %s)\n", hist); return 0; }
            for (int i = 0; i < n; i++) printf("  %s\n", names[i]);
            return 0;
        }
        if (!n) { fprintf(stderr, "?rollback: nessuno storico lock in %s\n", hist); return 1; }
        const char* chosen = names[0];
        const char* to = opt_val(argc, argv, "--to");
        if (to) {
            chosen = NULL;
            for (int i = 0; i < n; i++) if (!strncasecmp(names[i], to, strlen(to))) { chosen = names[i]; break; }
            if (!chosen) { fprintf(stderr, "?rollback: lock '%s' non trovato\n", to); return 1; }
        }
        char src[1600], dst[1200];
        snprintf(src, sizeof src, "%s/%s", hist, chosen);
        snprintf(dst, sizeof dst, "%s/%s", dir, GMBADMIN_LOCK);
        FILE* in = fopen(src, "rb");
        if (!in) { fprintf(stderr, "?rollback: %s\n", src); return 1; }
        FILE* out = fopen(dst, "wb");
        if (!out) { fclose(in); fprintf(stderr, "?rollback: %s\n", dst); return 1; }
        char buf[8192]; size_t r;
        while ((r = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, r, out);
        fclose(in); fclose(out);
        printf("rollback: %s -> %s\n", src, dst);
        return resolve_and_lock(dir, registry, cache, 1, 1, 1);
    }
    if (!strcmp(sub, "trust")) {
        char bucket[1200];
        gmbadmin_keyring(bucket, sizeof bucket);
        const char* a1 = first_pos(argc, argv, 0);
        if (!a1 || !strcasecmp(a1, "list")) {
            int n = gmb_pkg_keyring_count(bucket);
            printf("keyring: %s (%d chiavi)\n", bucket, n);
            for (int i = 0; i < n; i++) {
                char kn[256] = "", kv[8192] = "", fp[64] = "";
                if (!gmb_pkg_keyring_item(bucket, i, kn, sizeof kn, kv, sizeof kv)) continue;
                gmb_pkg_key_fingerprint(kv, fp, sizeof fp);
                printf("  %-20s %s\n", kn, fp[0] ? fp : "(fp n/d)");
            }
            return 0;
        }
        if (!strcasecmp(a1, "fingerprint") || !strcasecmp(a1, "fp")) {
            const char* k = first_pos(argc, argv, 1);
            if (!k) { fprintf(stderr, "?uso: gmb pkg trust fingerprint <chiave|file>\n"); return 2; }
            char key[8192] = "", fp[64] = "";
            FILE* kf = fopen(k, "rb");
            if (kf) { size_t n = fread(key, 1, sizeof key - 1, kf); key[n] = 0; fclose(kf); }
            else snprintf(key, sizeof key, "%s", k);
            gmb_pkg_key_fingerprint(key, fp, sizeof fp);
            printf("%s\n", fp[0] ? fp : "(non calcolabile)");
            return 0;
        }
        if (!strcasecmp(a1, "remove") || !strcasecmp(a1, "rm")) {
            const char* name = first_pos(argc, argv, 1);
            if (!name) { fprintf(stderr, "?uso: gmb pkg trust remove <nome>\n"); return 2; }
            return gmb_pkg_keyring_remove(bucket, name) ? 0 : 1;
        }
        /* add <nome> <chiave|file> */
        const char* name = a1;
        const char* src = first_pos(argc, argv, 1);
        if (!src) { fprintf(stderr, "?uso: gmb pkg trust add <nome> <chiave|file>\n"); return 2; }
        char key[8192] = "";
        FILE* kf = fopen(src, "rb");
        if (kf) { size_t n = fread(key, 1, sizeof key - 1, kf); key[n] = 0; fclose(kf); }
        else snprintf(key, sizeof key, "%s", src);
        if (!gmb_pkg_keyring_set(bucket, name, key)) { fprintf(stderr, "?trust: scrittura fallita\n"); return 1; }
        char fp[64] = "";
        gmb_pkg_key_fingerprint(key, fp, sizeof fp);
        printf("Fidato: %s (%s)\n", name, fp[0] ? fp : "fp n/d");
        return 0;
    }
    if (!strcmp(sub, "cache")) {
        const char* a1 = first_pos(argc, argv, 0);
        if (!a1 || !strcasecmp(a1, "list")) return cache_list(cache);
        if (!strcasecmp(a1, "gc")) {
            char content[1200];
            snprintf(content, sizeof content, "%s/content", cache);
            int rc = gmb_pkg_content_gc(content, cache);
            printf("gc: %d blob non referenziati rimossi\n", rc);
            return rc < 0 ? 1 : 0;
        }
        if (!strcasecmp(a1, "dedup")) {
            char content[1200];
            snprintf(content, sizeof content, "%s/content", cache);
            DIR* d = opendir(cache);
            if (!d) return 0;
            int interned = 0;
            struct dirent* de;
            while ((de = readdir(d))) {
                if (de->d_name[0] == '.') continue;
                char pd[1600];
                snprintf(pd, sizeof pd, "%s/%s", cache, de->d_name);
                int k = gmb_pkg_content_intern_dir(content, pd);
                if (k > 0) interned += k;
            }
            closedir(d);
            printf("dedup: %d file internati in %s\n", interned, content);
            return 0;
        }
        if (!strcasecmp(a1, "clean")) return cache_clean(cache, flag(argc, argv, "--force"));
        fprintf(stderr, "?uso: gmb pkg cache <list|gc|dedup|clean [--force]>\n");
        return 2;
    }
    fprintf(stderr, "?pkg: comando sconosciuto '%s' (gmb pkg help)\n", sub);
    return 2;
}
