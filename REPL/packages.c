/* native/repl/packages.c — speculare a CliPackages.cs (PACKAGE).
 * Parti reali e autonome: INIT (crea gmb-package.json), PKG LIST/REMOVE, CACHE LIST/CLEAN
 * (filesystem su <root>/config/packages — GmbiPaths C: config dir). Parti in sospeso
 * (segnalate): PACK/INSTALL/SEARCH/PUBLISH dipendono da PackageBuilder/PackageManager +
 * registry (pipeline C in corso).
 */
#include "packages.h"
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

static const char* pkg_cache_default(void)
{
    static char buf[1024];
    char d[1024];
    gmb_paths_config_dir(d, sizeof(d));
    snprintf(buf, sizeof(buf), "%s/packages", d);
    return buf;
}

/* flag --name X ... e posizionali; tokenizzazione semplice */
static void pkg_flags(const char* args, char name[128], char ver[64], char cache[1024], char out[512], char type[64], char* pos0, int pos0cap)
{
    name[0] = ver[0] = cache[0] = out[0] = type[0] = pos0[0] = 0;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", args ? args : "");
    char* save = NULL;
    char* tok = strtok_r(buf, " ", &save);
    while (tok) {
        if (strncmp(tok, "--name", 6) == 0) { char* v = strtok_r(NULL, " ", &save); if (v) snprintf(name, 128, "%s", v); }
        else if (strncmp(tok, "--version", 9) == 0) { char* v = strtok_r(NULL, " ", &save); if (v) snprintf(ver, 64, "%s", v); }
        else if (strncmp(tok, "--cache", 7) == 0) { char* v = strtok_r(NULL, " ", &save); if (v) snprintf(cache, 1024, "%s", v); }
        else if (strncmp(tok, "--out", 5) == 0) { char* v = strtok_r(NULL, " ", &save); if (v) snprintf(out, 512, "%s", v); }
        else if (strncmp(tok, "--type", 6) == 0) { char* v = strtok_r(NULL, " ", &save); if (v) snprintf(type, 64, "%s", v); }
        else if (tok[0] != '-' && !pos0[0]) snprintf(pos0, pos0cap, "%s", tok);
        tok = strtok_r(NULL, " ", &save);
    }
}

static int pkg_init(const char* dir, const char* name, const char* ver, const char* type)
{
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) { perror("?init mkdir"); return 1; }
    char path[2048];
    snprintf(path, sizeof(path), "%s/gmb-package.json", dir);
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "?init: impossibile scrivere %s\n", path); return 1; }
    fprintf(f, "{\n  \"name\": \"%s\",\n  \"version\": \"%s\",\n  \"type\": \"%s\",\n  \"edition\": \"Community\"\n}\n",
        name[0] ? name : "package", ver[0] ? ver : "1.0.0", type[0] ? type : "source");
    fclose(f);
    printf("[pkg] init: %s (type=%s, version=%s)\n", path, type[0] ? type : "source", ver[0] ? ver : "1.0.0");
    return 0;
}

static int pkg_list(const char* cache)
{
    DIR* d = opendir(cache);
    if (!d) { printf("(cache vuota o assente: %s)\n", cache); return 0; }
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[1100];
        snprintf(sub, sizeof(sub), "%s/%s", cache, de->d_name);
        struct stat st;
        if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* elenca versioni dentro */
            printf("%s:", de->d_name);
            DIR* d2 = opendir(sub);
            if (d2) { struct dirent* v; while ((v = readdir(d2))) { if (v->d_name[0] != '.') printf(" %s", v->d_name); } closedir(d2); }
            printf("\n");
        }
    }
    closedir(d);
    return 0;
}

static int pkg_remove(const char* cache, const char* name)
{
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/%s", cache, name);
    struct stat st;
    if (stat(dir, &st) != 0) { fprintf(stderr, "?pkg: non installato: %s\n", name); return 1; }
    char cmd[1300];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dir);
    if (system(cmd) == 0) { printf("[pkg] rimosso: %s\n", name); return 0; }
    fprintf(stderr, "?pkg remove fallito\n");
    return 1;
}

int gmb_packages_dispatch(const char* args)
{
    /* primo token = subcomando; gli args posizionali partono dopo */
    char sub[64] = "";
    sscanf(args, "%63s", sub);
    const char* rest = args + strlen(sub);
    while (*rest == ' ') rest++;

    char name[128], ver[64], cache[1024], out[512], type[64], pos0[512];
    pkg_flags(rest, name, ver, cache, out, type, pos0, sizeof(pos0));
    if (!cache[0]) snprintf(cache, sizeof(cache), "%s", pkg_cache_default());

    if (strcasecmp(sub, "INIT") == 0) {
        if (!pos0[0]) { fprintf(stderr, "?uso: PACKAGE INIT <dir> --name X --version V [--type source|dotnet|vm|c-ffi|mixed]\n"); return 1; }
        return pkg_init(pos0, name, ver, type);
    }
    if (strcasecmp(sub, "LIST") == 0) return pkg_list(cache);
    if (strcasecmp(sub, "REMOVE") == 0) {
        if (!pos0[0]) { fprintf(stderr, "?uso: PACKAGE REMOVE <name>\n"); return 1; }
        return pkg_remove(cache, pos0);
    }
    if (strcasecmp(sub, "CACHE") == 0) {
        char s2[64] = "";
        sscanf(args + strlen(sub), "%63s", s2);
        if (strcasecmp(s2, "CLEAN") == 0) {
            char c[1200];
            snprintf(c, sizeof(c), "rm -rf \"%s\"/*", cache);
            if (system(c) == 0) printf("[pkg] cache pulita: %s\n", cache);
            return 0;
        }
        return pkg_list(cache);
    }
    if (strcasecmp(sub, "PACK") == 0) {
        /* .gmbpkg = zip del contenuto (manifest + sorgenti). Richiede il compilatore per
         * i backend il/c99/vm (binaries); il pack source funziona. */
        if (!pos0[0]) { fprintf(stderr, "?uso: PACKAGE PACK <dir> [--out out.gmbpkg]\n"); return 1; }
        char outfile[1024];
        if (out[0]) snprintf(outfile, sizeof(outfile), "%s", out);
        else snprintf(outfile, sizeof(outfile), "%s/package.gmbpkg", pos0);
        char cmd[4600];
        snprintf(cmd, sizeof(cmd), "(cd \"%s\" && zip -qr \"%s\" gmb-package.json . -x '*.gmbpkg' 2>/dev/null || zip -qr \"%s\" . -x '*.gmbpkg')", pos0, outfile, outfile);
        if (system(cmd) == 0) { printf("[pkg] pack: %s\n", outfile); return 0; }
        fprintf(stderr, "?PACKAGE PACK: zip fallito (manca gmb-package.json?)\n");
        return 1;
    }
    if (strcasecmp(sub, "INSTALL") == 0) {
        /* download .gmbpkg dal registry e lo estrae in cache */
        char spec[256] = "";
        sscanf(args + strlen(sub), "%255s", spec);
        if (!spec[0]) { fprintf(stderr, "?uso: PACKAGE INSTALL <name[@ver]> [--registry url]\n"); return 1; }
        char reg[512] = "http://localhost:8080";
        /* --registry */
        const char* r = strstr(args, "--registry");
        if (r) sscanf(r + 10, " %511s", reg);
        char name[128] = "", ver[64] = "";
        const char* at = strchr(spec, '@');
        if (at) { int nl = (int)(at - spec); snprintf(name, sizeof(name), "%.*s", nl, spec); snprintf(ver, sizeof(ver), "%s", at + 1); }
        else snprintf(name, sizeof(name), "%s", spec);
        char pkgurl[1024];
        snprintf(pkgurl, sizeof(pkgurl), "%s/api/packages/%s/%s.gmbpkg", reg, name, ver[0] ? ver : "latest");
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "/tmp/gmb_pkg_%ld.gmbpkg", (long)getpid());
        char curl[2048];
        snprintf(curl, sizeof(curl), "curl -s -L -o \"%s\" \"%s\"", tmp, pkgurl);
        if (system(curl) != 0) { fprintf(stderr, "?INSTALL: download fallito da %s\n", pkgurl); return 1; }
        char dest[1400];
        snprintf(dest, sizeof(dest), "%s/%s", cache, name);
        char mk[1500];
        snprintf(mk, sizeof(mk), "mkdir -p \"%s\"", dest);
        system(mk);
        char cmd[1600];
        snprintf(cmd, sizeof(cmd), "unzip -o -q \"%s\" -d \"%s\"", tmp, dest);
        if (system(cmd) == 0) { printf("[pkg] installato: %s -> %s\n", name, dest); unlink(tmp); return 0; }
        fprintf(stderr, "?INSTALL: estrazione fallita (%s)\n", pkgurl);
        unlink(tmp);
        return 1;
    }
    if (strcasecmp(sub, "SEARCH") == 0) {
        char term[256] = "";
        sscanf(args + strlen(sub), "%255s", term);
        if (!term[0]) { fprintf(stderr, "?uso: PACKAGE SEARCH <term> [--registry url]\n"); return 1; }
        char reg[512] = "http://localhost:8080";
        const char* r = strstr(args, "--registry");
        if (r) sscanf(r + 10, " %511s", reg);
        char url[1024];
        snprintf(url, sizeof(url), "%s/api/packages/search?q=%s", reg, term);
        char curl[2048];
        snprintf(curl, sizeof(curl), "curl -s -L \"%s\"", url);
        system(curl);
        printf("\n");
        return 0;
    }
    if (strcasecmp(sub, "PUBLISH") == 0) {
        char file[512] = "";
        sscanf(args + strlen(sub), "%511s", file);
        if (!file[0]) { fprintf(stderr, "?uso: PACKAGE PUBLISH <file.gmbpkg> [--registry url] [--token t]\n"); return 1; }
        char reg[512] = "http://localhost:8080";
        const char* r = strstr(args, "--registry");
        if (r) sscanf(r + 10, " %511s", reg);
        char tok[256] = "";
        const char* tk = strstr(args, "--token");
        if (tk) sscanf(tk + 7, " %255s", tok);
        char curl[2400];
        if (tok[0]) snprintf(curl, sizeof(curl), "curl -s -X POST -H \"Authorization: Bearer %s\" -F \"file=@%s\" \"%s/api/packages/publish\"", tok, file, reg);
        else snprintf(curl, sizeof(curl), "curl -s -X POST -F \"file=@%s\" \"%s/api/packages/publish\"", file, reg);
        if (system(curl) == 0) printf("[pkg] pubblicato: %s\n", file);
        else fprintf(stderr, "?PUBLISH fallito\n");
        return 0;
    }
    fprintf(stderr, "?uso: PACKAGE INIT|LIST|REMOVE|CACHE|PACK|INSTALL|SEARCH|PUBLISH\n");
    return 1;
}
