/* gmb_pkg_install.c — materializzazione a install-time (riferimento C del Package Manager). */
#include "gmb_pkg_install.h"
#include "gmb_pkg_manifest.h"
#include "gmb_pkg_lock.h"
#include "gmb_network.h"
#include "gmb_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static char* read_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (l < 0) { fclose(f); return NULL; }
    char* b = (char*)malloc((size_t)l + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)l, f);
    b[rd] = 0;
    fclose(f);
    return b;
}
static int is_dir(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int is_file(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }
static void mkdir_p(const char* path)
{
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}
static void copy_tree(const char* src, const char* dst)
{
    if (is_file(src)) {
        /* mkdir_p(dst) creava una DIRECTORY col nome del FILE di destinazione: fopen(dst,"wb")
         * poi falliva e restava una cartella vuota chiamata come il file (`avif_demo.bas/`,
         * `sample.ppm/`). Va creata la CARTELLA CONTENITRICE del file. */
        {
            char parent[4096];
            snprintf(parent, sizeof(parent), "%s", dst);
            char* sl = strrchr(parent, '/');
            if (sl) { *sl = '\0'; if (parent[0]) mkdir_p(parent); }
        }
        FILE* in = fopen(src, "rb");
        if (!in) return;
        FILE* out = fopen(dst, "wb");
        if (out) {
            char buf[65536]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
            fclose(out);
        }
        fclose(in);
        return;
    }
    if (!is_dir(src)) return;
    mkdir_p(dst);
    DIR* d = opendir(src);
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char s[4096], t[4096];
        snprintf(s, sizeof(s), "%s/%s", src, de->d_name);
        snprintf(t, sizeof(t), "%s/%s", dst, de->d_name);
        copy_tree(s, t);
    }
    closedir(d);
}

static void safe_dir(const char* name, char* out, size_t cap)
{
    size_t j = 0;
    for (const char* p = (name ? name : ""); *p && j + 1 < cap; p++) out[j++] = (*p == '/' || *p == '\\') ? '_' : *p;
    out[j] = 0;
}

/* Fallback (solo se il chiamante non passa la project_root): risale dalla CWD cercando
   GMBASIC.sln, marcatore di progetto VERO. NON basta una cartella samples/: un pacchetto può
   averne una propria (es. nel suo albero sorgente) e verrebbe scambiata per la root. */
static void discover_project_root(char* out, size_t cap)
{
    char cur[2048];
    if (!getcwd(cur, sizeof(cur))) { snprintf(out, cap, "."); return; }
    for (int i = 0; i < 12; i++) {
        char sln[2200];
        snprintf(sln, sizeof(sln), "%s/GMBASIC.sln", cur);
        if (is_file(sln)) { snprintf(out, cap, "%s", cur); return; }
        char* slash = strrchr(cur, '/');
        if (!slash || slash == cur) break;
        *slash = 0;
    }
    snprintf(out, cap, ".");
}

int gmb_pkg_copy_samples(const char* install_dir, const char* project_root)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    char name[256] = "", ver[64] = "";
    gmb_pkg_manifest_get(man, "name", name, sizeof(name));
    gmb_pkg_manifest_get(man, "version", ver, sizeof(ver));
    char root[2048];
    if (project_root && project_root[0]) snprintf(root, sizeof(root), "%s", project_root);
    else discover_project_root(root, sizeof(root));
    char sd[256];
    safe_dir(name, sd, sizeof(sd));
    char dest[2300];
    snprintf(dest, sizeof(dest), "%s/samples/packages/%s@%s", root, sd, ver);

    int ns = gmb_pkg_manifest_count(man, "samples");
    if (ns > 0) {
        for (int i = 0; i < ns; i++) {
            char k[64], v[512];
            if (!gmb_pkg_manifest_item(man, "samples", i, k, sizeof(k), v, sizeof(v)) || !v[0]) continue;
            char src[3000];
            snprintf(src, sizeof(src), "%s/%s", install_dir, v);
            if (is_dir(src)) copy_tree(src, dest); /* contenuti di src dentro dest */
            else if (is_file(src)) {
                mkdir_p(dest);
                char* base = strrchr(v, '/');
                char fd[2400];
                snprintf(fd, sizeof(fd), "%s/%s", dest, base ? base + 1 : v);
                copy_tree(src, fd);
            }
        }
    } else {
        char src[3000];
        snprintf(src, sizeof(src), "%s/samples", install_dir);
        if (is_dir(src)) copy_tree(src, dest);
    }
    gmb_pkg_manifest_free(man);
    return 1;
}

/* --- build --- */
static int has_file(const char* dir, const char* rel)
{
    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", dir, rel);
    return is_file(full);
}
static int which(const char* exe)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", exe);
    return system(cmd) == 0;
}
static const char* detect_compiler(const char* tool)
{
    if (tool && tool[0] && which(tool)) return tool;
    if (which("cc")) return "cc";
    if (which("clang")) return "clang";
    if (which("gcc")) return "gcc";
    return NULL;
}

/* Esegue il build dichiarato nel manifest (system) nella cwd indicata. 1 ok. */
static int run_build_cmd(const char* install_dir, gmb_pkg_manifest_t* man, const char* platform)
{
    char bsys[64], target[256], command[1024], cwd[1024], tool[64];
    gmb_pkg_manifest_get(man, "build.system", bsys, sizeof(bsys));
    gmb_pkg_manifest_get(man, "build.target", target, sizeof(target));
    gmb_pkg_manifest_get(man, "build.command", command, sizeof(command));
    gmb_pkg_manifest_get(man, "build.cwd", cwd, sizeof(cwd));
    gmb_pkg_manifest_get(man, "build.tool", tool, sizeof(tool));
    if (!bsys[0] && !command[0]) return 1; /* niente build */

    char wd[2048];
    if (cwd[0]) snprintf(wd, sizeof(wd), "%s/%s", install_dir, cwd);
    else snprintf(wd, sizeof(wd), "%s", install_dir);

    char full[4096];
    if (command[0]) {
        /* {platform} → platformKey */
        char cmd2[2048]; size_t j = 0;
        for (const char* p = command; *p && j + 1 < sizeof(cmd2); p++) {
            if (strncmp(p, "{platform}", 10) == 0) { size_t pl = strlen(platform); memcpy(cmd2 + j, platform, pl); j += pl; p += 9; }
            else cmd2[j++] = *p;
        }
        cmd2[j] = 0;
        snprintf(full, sizeof(full), "cd \"%s\" && %s", wd, cmd2);
    } else if (strcasecmp(bsys, "make") == 0) {
        snprintf(full, sizeof(full), "cd \"%s\" && make %s PLAT=%s", wd, target, platform);
    } else if (strcasecmp(bsys, "cmake") == 0) {
        snprintf(full, sizeof(full), "cd \"%s\" && cmake -S . -B build-gmb -DPLAT=%s && cmake --build build-gmb", wd, platform);
    } else {
        return 1;
    }
    int rc = system(full);
    return rc == 0;
}

static int outputs_present(const char* install_dir, gmb_pkg_manifest_t* man, const char* platform)
{
    int no = gmb_pkg_manifest_count(man, "build.outputs");
    if (no <= 0) return 0; /* nessun output dichiarato: non si può dire "già costruito" */
    for (int i = 0; i < no; i++) {
        char k[64], v[512];
        if (!gmb_pkg_manifest_item(man, "build.outputs", i, k, sizeof(k), v, sizeof(v)) || !v[0]) continue;
        char rel[1024]; size_t j = 0;
        for (const char* p = v; *p && j + 1 < sizeof(rel); p++) {
            if (strncmp(p, "{platform}", 10) == 0) { size_t pl = strlen(platform); memcpy(rel + j, platform, pl); j += pl; p += 9; }
            else rel[j++] = *p;
        }
        rel[j] = 0;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", install_dir, rel);
        struct stat st;
        if (stat(full, &st) != 0) return 0;
    }
    return 1;
}

int gmb_pkg_run_build(const char* install_dir, const char* platform_key)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    if (!gmb_pkg_manifest_has(man, "build")) { gmb_pkg_manifest_free(man); return 0; }
    int ok = run_build_cmd(install_dir, man, platform_key);
    gmb_pkg_manifest_free(man);
    return ok;
}

int gmb_pkg_ensure_build(const char* install_dir, const char* platform_key)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    if (!gmb_pkg_manifest_has(man, "build")) { gmb_pkg_manifest_free(man); return 0; }

    char lp[128], lib[1024];
    snprintf(lp, sizeof(lp), "ffi.libs.%s", platform_key);
    if (gmb_pkg_manifest_get(man, lp, lib, sizeof(lib)) && lib[0] && has_file(install_dir, lib)) { gmb_pkg_manifest_free(man); return 0; }
    if (outputs_present(install_dir, man, platform_key)) { gmb_pkg_manifest_free(man); return 0; }

    char tool[64];
    gmb_pkg_manifest_get(man, "build.tool", tool, sizeof(tool));
    if (!detect_compiler(tool)) { gmb_pkg_manifest_free(man); return -1; /* compilatore assente */ }

    int ok = run_build_cmd(install_dir, man, platform_key);
    if (ok && !outputs_present(install_dir, man, platform_key)) {
        int no = gmb_pkg_manifest_count(man, "build.outputs");
        if (no > 0) ok = 0; /* output dichiarati ma assenti → build fallita */
    }
    gmb_pkg_manifest_free(man);
    return ok ? 1 : -1;
}

/* --- source (github-release) --- */
static int http_download(const char* url, const char* dest)
{
    int status = 0;
    char* body = NULL;
    size_t len = 0;
    if (gmb_net_http_request(url, NULL, &status, &body, &len, NULL, NULL) != 0 || !body) { if (body) free(body); return 0; }
    FILE* f = fopen(dest, "wb");
    if (!f) { free(body); return 0; }
    size_t w = fwrite(body, 1, len, f);
    fclose(f);
    free(body);
    return w == len && len > 0;
}

/* Copia gli header dichiarati in `source.headers` dell'asset appena estratto in `include/`
 * del pacchetto: è il contratto documentato (Knowledge/Infrastruttura-Moduli-Loading §3.4:
 * «estrae in dest (+ header in include/)») e serve al `cc` del consumatore (-I <pkg>/include).
 * Il path dichiarato è relativo all'asset; molti asset hanno una directory di primo livello,
 * quindi si cerca anche a un livello di profondità. Ritorna il numero di header copiati. */
int gmb_pkg_copy_source_headers(const char* install_dir, const char* extract_dir)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;

    int copied = 0;
    int nh = gmb_pkg_manifest_count(man, "source.headers");
    for (int i = 0; i < nh; i++) {
        char hk[256], hv[512];
        if (!gmb_pkg_manifest_item(man, "source.headers", i, hk, sizeof(hk), hv, sizeof(hv))) continue;
        /* lista di stringhe: chiave vuota, valore = path relativo nell'asset */
        const char* rel = hv[0] ? hv : hk;
        if (!rel[0]) continue;
        char in[2600];
        snprintf(in, sizeof(in), "%s/%s", extract_dir, rel);
        if (!is_file(in)) {
            char alt[2600];
            snprintf(alt, sizeof(alt), "%s", in);
            DIR* d = opendir(extract_dir);
            if (d) {
                struct dirent* de;
                while ((de = readdir(d))) {
                    if (de->d_name[0] == '.') continue;
                    snprintf(alt, sizeof(alt), "%s/%s/%s", extract_dir, de->d_name, rel);
                    if (is_file(alt)) break;
                }
                closedir(d);
            }
            snprintf(in, sizeof(in), "%s", alt);
        }
        if (!is_file(in)) continue;   /* dichiarato ma assente dall'asset: non inventare */
        const char* base = strrchr(rel, '/');
        base = base ? base + 1 : rel;
        char incdir[2300], out[2600];
        snprintf(incdir, sizeof(incdir), "%s/include", install_dir);
        mkdir_p(incdir);
        snprintf(out, sizeof(out), "%s/%s", incdir, base);
        copy_tree(in, out);
        copied++;
    }
    gmb_pkg_manifest_free(man);
    return copied;
}

int gmb_pkg_ensure_source(const char* install_dir, const char* platform_key)
{
    char mpath[2600];
    snprintf(mpath, sizeof(mpath), "%s/gmb-package.json", install_dir);
    char* js = read_file(mpath);
    if (!js) return 0;
    gmb_pkg_manifest_t* man = gmb_pkg_manifest_parse(js);
    free(js);
    if (!man) return 0;
    if (!gmb_pkg_manifest_has(man, "source")) { gmb_pkg_manifest_free(man); return 1; }

    char type[64], repo[512], tag[256], ver[64];
    gmb_pkg_manifest_get(man, "source.type", type, sizeof(type));
    gmb_pkg_manifest_get(man, "source.repo", repo, sizeof(repo));
    gmb_pkg_manifest_get(man, "source.tag", tag, sizeof(tag));
    gmb_pkg_manifest_get(man, "version", ver, sizeof(ver));
    if (strcmp(type, "github-release") != 0 || !repo[0]) { gmb_pkg_manifest_free(man); return 0; }

    if (!tag[0]) snprintf(tag, sizeof(tag), "v%s", ver);
    else { /* template {version} */
        char t2[256]; size_t j = 0;
        for (const char* p = tag; *p && j + 1 < sizeof(t2); p++) {
            if (strncmp(p, "{version}", 9) == 0) { size_t vl = strlen(ver); memcpy(t2 + j, ver, vl); j += vl; p += 8; }
            else t2[j++] = *p;
        }
        t2[j] = 0; snprintf(tag, sizeof(tag), "%s", t2);
    }

    char ap[200], asset[512];
    snprintf(ap, sizeof(ap), "source.assets.%s", platform_key);
    if (!(gmb_pkg_manifest_get(man, ap, asset, sizeof(asset)) && asset[0])) {
        gmb_pkg_manifest_get(man, "source.assets.any", asset, sizeof(asset));
    }
    if (!asset[0]) { gmb_pkg_manifest_free(man); return 0; }

    char url[2048];
    snprintf(url, sizeof(url), "https://github.com/%s/releases/download/%s/%s", repo, tag, asset);
    char tmp[2300];
    snprintf(tmp, sizeof(tmp), "%s/.gmb-source.tmp", install_dir);
    if (!http_download(url, tmp)) { gmb_pkg_manifest_free(man); return 0; }

    /* verifica sha256 (se dichiarato) */
    char sp[200], expect[128];
    snprintf(sp, sizeof(sp), "source.sha256.%s", platform_key);
    if (gmb_pkg_manifest_get(man, sp, expect, sizeof(expect)) && expect[0]) {
        char actual[65];
        gmb_pkg_sha256_file(tmp, actual, sizeof(actual));
        if (strcasecmp(actual, expect) != 0) { unlink(tmp); gmb_pkg_manifest_free(man); return 0; }
    }

    /* estrazione nella dest (default lib/<platform>). ATTENZIONE: `source` NON è l'archivio del
     * pacchetto (quello è estratto dal chiamante nella RADICE del pacchetto da
     * PackageManagerHost.DownloadFromRegistryAsync / gmb_pkg_resolver.ensure_cached), ma un ASSET
     * BINARIO precompilato da una release GitHub (Knowledge/Infrastruttura-Moduli-Loading §3.4):
     * il suo posto è `lib/<platform>/`, dove il loader FFI lo cerca. */
    char dest[512];
    gmb_pkg_manifest_get(man, "source.dest", dest, sizeof(dest));
    if (!dest[0]) snprintf(dest, sizeof(dest), "lib/%s", platform_key);
    else { /* {platform} */
        char d2[512]; size_t j = 0;
        for (const char* p = dest; *p && j + 1 < sizeof(d2); p++) {
            if (strncmp(p, "{platform}", 10) == 0) { size_t pl = strlen(platform_key); memcpy(d2 + j, platform_key, pl); j += pl; p += 9; }
            else d2[j++] = *p;
        }
        d2[j] = 0; snprintf(dest, sizeof(dest), "%s", d2);
    }
    char outdir[2300];
    snprintf(outdir, sizeof(outdir), "%s/%s", install_dir, dest);
    mkdir_p(outdir);
    gmb_unzip_files(tmp, outdir);
    /* `source.headers` (documentato in Infrastruttura §3.4: «estrae in dest + header in
     * include/»): gli header servono al `cc` del consumatore (-I <pkg>/include). Prima NON
     * venivano copiati: il pacchetto dichiarava gli header e il compilatore non li trovava.
     * Funzione ESPOSTA (non inline) perché è testabile senza rete: vedi selfcheck. */
    gmb_pkg_copy_source_headers(install_dir, outdir);
    unlink(tmp);
    gmb_pkg_manifest_free(man);
    return 1;
}
