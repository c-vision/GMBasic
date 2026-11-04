/* gmbadminfetch.c — download on-demand in C: `setup` componenti, `libs publish`, `hf`/`mlx`.
 *
 * Tutto su API condivise del runtime (nessuna logica solo-C#):
 *   - `gmb_fetch_to_file` (curl: ripresa, sha256, rename atomico) per ogni file;
 *   - `gmb_unzip_files` per gli archivi .zip, `tar` per i .tar.gz;
 *   - manifest JSON letti con `gmb_json_*` (stessa forma dei manifest in shared/settings/).
 *
 *   gmb setup <BROWSER|OBSCURA|TORCH|OCR|LIBS|ALL> [--force] [--yes] [--torch-local dir]
 *   gmb libs publish <support|engine|core> <rid> [--yes]
 *   gmb hf  <repo> [--file F|pattern] [--dest D] [--token T] [--quiet]
 *   gmb mlx <repo> [--dest D] [--token T] [--quiet]      (scarica tutto il repo)
 */
#include "gmbadmin.h"

#include "gmb_runtime.h"
#include "gmb_fetch.h"
#include "gmb_compress.h"
#include "gmb_fsutil.h"
#include "gmb_pkg_util.h"
#include "gmb_commonlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* ── utility ─────────────────────────────────────────────────────────────── */

static const char* fopt(int argc, char** argv, int start, const char* name)
{
    for (int i = start; i < argc; i++) {
        if (!strcmp(argv[i], name) && i + 1 < argc) return argv[i + 1];
        size_t n = strlen(name);
        if (!strncmp(argv[i], name, n) && argv[i][n] == '=') return argv[i] + n + 1;
    }
    return NULL;
}

static int fflag(int argc, char** argv, int start, const char* name)
{
    for (int i = start; i < argc; i++) if (!strcmp(argv[i], name)) return 1;
    return 0;
}

static int run_cmd(char* const av[])
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) { execvp(av[0], av); _exit(127); }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static const char* json_str(gmb_val_t o, const char* k)
{
    gmb_val_t v = gmb_val_dict_get(o, k);
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return v.v_string.data;
    return NULL;
}

static long long json_int(gmb_val_t o, const char* k, long long dflt)
{
    gmb_val_t v = gmb_val_dict_get(o, k);
    if (v.kind == GMB_TYPE_INT) return (long long)v.v_int;
    if (v.kind == GMB_TYPE_DOUBLE) return (long long)v.v_double;
    return dflt;
}

/* I manifest stanno in <monorepo>/shared/settings/. `gmb_paths_repo_root` risponde con la
 * radice del repo C (GMBasic/), quindi si prova anche `<root>/../shared` (monorepo). */
static void manifest_of(const char* file, char* out, size_t cap)
{
    char root[2048];
    char cand[2400];
    if (gmb_paths_repo_root(root, sizeof root) && root[0]) {
        snprintf(cand, sizeof cand, "%s/shared/settings/%s", root, file);
        if (access(cand, R_OK) == 0) { snprintf(out, cap, "%s", cand); return; }
        snprintf(cand, sizeof cand, "%s/../shared/settings/%s", root, file);
        if (access(cand, R_OK) == 0) { snprintf(out, cap, "%s", cand); return; }
    }
    snprintf(cand, sizeof cand, "shared/settings/%s", file);
    if (access(cand, R_OK) == 0) { snprintf(out, cap, "%s", cand); return; }
    snprintf(out, cap, "%s/shared/settings/%s", root, file);
}

static gmb_val_t load_json(const char* path)
{
    gmb_val_t v = gmb_json_load_file(path);
    if (v.kind != GMB_TYPE_OBJECT) { memset(&v, 0, sizeof v); }
    return v;
}

/* Cerca `name` in `dir` (fino a `depth` livelli): scrive il primo percorso trovato. */
static int find_under(const char* dir, const char* name, int depth, char* out, size_t cap)
{
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* de;
    int found = 0;
    while ((de = readdir(d)) && !found) {
        if (de->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if ((S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) && !strcmp(de->d_name, name)) {
            /* anche le directory: su macOS il "binario" del browser è l'app bundle
             * (Google Chrome for Testing.app). */
            snprintf(out, cap, "%s", p);
            found = 1;
        } else if (S_ISDIR(st.st_mode) && depth > 1 && find_under(p, name, depth - 1, out, cap)) {
            found = 1;
        }
    }
    closedir(d);
    return found;
}

static int file_exists(const char* p)
{
    return p && *p && access(p, R_OK) == 0;
}

/* Scarica url → dest con verifica sha/size (0 = ok). */
static int download(const char* url, const char* sha, long long size, const char* dest, int quiet)
{
    gmb_fetch_request req;
    memset(&req, 0, sizeof req);
    req.url = url;
    req.dest_path = dest;
    req.sha256 = (sha && *sha) ? sha : NULL;
    req.size = size;
    gmb_fetch_opts o;
    gmb_fetch_opts_default(&o);
    o.quiet = quiet;
    char err[512] = "";
    int rc = gmb_fetch_to_file(&req, &o, gmb_fetch_transport_curl(), err, sizeof err);
    if (rc != 0) fprintf(stderr, "?download: %s (%s)\n", err[0] ? err : gmb_fetch_strerror(rc), url);
    return rc;
}

static void mkdirs_for(const char* path)
{
    char dir[4096];
    snprintf(dir, sizeof dir, "%s", path);
    char* sl = strrchr(dir, '/');
    if (sl) { *sl = 0; gmb_fs_mkdir_p(dir); }
}

/* Estrae un archivio in dest (zip → gmb_unzip_files, tar.* → tar). */
static int extract_archive(const char* archive, const char* dest, int quiet)
{
    size_t l = strlen(archive);
    if ((l > 4 && !strcasecmp(archive + l - 4, ".zip"))) {
        if (!quiet) printf("  estrazione zip -> %s\n", dest);
        return gmb_unzip_files(archive, dest) == 0 ? 0 : 1;
    }
    if (!quiet) printf("  estrazione tar -> %s\n", dest);
    if ((l > 7 && !strcasecmp(archive + l - 7, ".tar.gz")) ||
        (l > 4 && !strcasecmp(archive + l - 4, ".tgz"))) {
        char* av[] = { "tar", "-xzf", (char*)archive, "-C", (char*)dest, NULL };
        return run_cmd(av) == 0 ? 0 : 1;
    }
    if (l > 8 && !strcasecmp(archive + l - 8, ".tar.bz2")) {
        char* av[] = { "tar", "-xjf", (char*)archive, "-C", (char*)dest, NULL };
        return run_cmd(av) == 0 ? 0 : 1;
    }
    if (l > 4 && !strcasecmp(archive + l - 4, ".tar")) {
        char* av[] = { "tar", "-xf", (char*)archive, "-C", (char*)dest, NULL };
        return run_cmd(av) == 0 ? 0 : 1;
    }
    fprintf(stderr, "?estrazione: formato non riconosciuto (%s)\n", archive);
    return 1;
}

/* ── componenti a archivio singolo (BROWSER/OBSCURA) ─────────────────────── */

static int setup_archive_component(const char* manifest_file, int is_browser,
                                   int argc, char** argv, int quiet)
{
    char mp[2400];
    manifest_of(manifest_file, mp, sizeof mp);
    gmb_val_t root = load_json(mp);
    if (root.kind != GMB_TYPE_OBJECT) {
        fprintf(stderr, "?setup: manifest non leggibile (%s)\n", mp);
        return 1;
    }
    const char* rid = gmb_libs_rid();
    gmb_val_t runtimes = gmb_val_dict_get(root, "runtimes");
    gmb_val_t e = gmb_val_dict_get(runtimes, rid);
    if (e.kind != GMB_TYPE_OBJECT) {
        fprintf(stderr, "?setup: nessuna entry per RID '%s' in %s\n", rid, mp);
        return 1;
    }
    const char* url = json_str(e, "archiveUrl");
    const char* sha = json_str(e, "sha256");
    const char* bin = json_str(e, "binName");
    long long size = json_int(e, "size", 0);
    const char* ver = json_str(e, "version");
    if (!url || !*url) { fprintf(stderr, "?setup: archiveUrl mancante per %s\n", rid); return 1; }
    if (!bin || !*bin) bin = is_browser ? "chrome" : "obscura";

    char dest[2400];
    if (is_browser) gmb_paths_chromium_dir(rid, dest, sizeof dest);
    else gmb_paths_obscura_dir(rid, dest, sizeof dest);

    int force = fflag(argc, argv, 3, "--force");
    if (!force) {
        char found[4096];
        if (find_under(dest, bin, 3, found, sizeof found)) {
            printf("[%s] già in cache e valido: %s\n", is_browser ? "browser" : "obscura", found);
            return 0;
        }
    } else {
        gmb_fs_rmtree(dest);
    }
    gmb_fs_mkdir_p(dest);

    char cache[2500], tmpdir[2500], tmpfile[2600];
    snprintf(cache, sizeof cache, "%s/.pkg", dest);
    gmb_fs_mkdir_p(cache);
    const char* base = strrchr(url, '/');
    snprintf(tmpfile, sizeof tmpfile, "%s/%s", cache, base ? base + 1 : "pkg.bin");
    snprintf(tmpdir, sizeof tmpdir, "%s/.unpack", cache);

    printf("[%s] RID=%s versione %s — scarico in %s\n",
           is_browser ? "browser" : "obscura", rid, ver ? ver : "?", dest);
    if (download(url, sha, size, tmpfile, quiet) != 0) return 1;

    gmb_fs_rmtree(tmpdir);
    gmb_fs_mkdir_p(tmpdir);
    if (extract_archive(tmpfile, tmpdir, quiet) != 0) { gmb_fs_rmtree(tmpdir); return 1; }

    char found[4096];
    if (!find_under(tmpdir, bin, 4, found, sizeof found)) {
        fprintf(stderr, "?setup: '%s' non trovato nell'archivio estratto (%s)\n", bin, tmpdir);
        return 1;
    }
    /* sposta il contenuto estratto nella cache definitiva */
    char* inner = strdup(found);
    char* sl = strrchr(inner, '/');
    char inner_root[4096];
    snprintf(inner_root, sizeof inner_root, "%s", tmpdir);
    if (sl) {
        *sl = 0;
        /* risale fino a un livello che contenga il binario */
        char probe[4096];
        snprintf(probe, sizeof probe, "%s/%s", inner, bin);
        if (file_exists(probe)) snprintf(inner_root, sizeof inner_root, "%s", inner);
    }
    free(inner);
    if (strcmp(inner_root, tmpdir) == 0) {
        /* contenuto già alla radice: sposta i figli */
        DIR* d = opendir(tmpdir);
        struct dirent* de;
        while (d && (de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            char a[4096], b[4096];
            snprintf(a, sizeof a, "%s/%s", tmpdir, de->d_name);
            snprintf(b, sizeof b, "%s/%s", dest, de->d_name);
            rename(a, b);
        }
        if (d) closedir(d);
    } else {
        DIR* d = opendir(inner_root);
        struct dirent* de;
        while (d && (de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            char a[4096], b[4096];
            snprintf(a, sizeof a, "%s/%s", inner_root, de->d_name);
            snprintf(b, sizeof b, "%s/%s", dest, de->d_name);
            rename(a, b);
        }
        if (d) closedir(d);
    }
    gmb_fs_rmtree(tmpdir);
    unlink(tmpfile);
    gmb_fs_rmtree(cache);

    if (!find_under(dest, bin, 3, found, sizeof found)) {
        fprintf(stderr, "?setup: binario '%s' non presente in %s dopo l'estrazione\n", bin, dest);
        return 1;
    }
    if (!is_browser) chmod(found, 0755);
    printf("[OK] %s pronto: %s\n", is_browser ? "browser" : "obscura", found);
    return 0;
}

/* ── TORCH (ml-manifest: file url/archive/build/local) ───────────────────── */

/* ── TORCH locale: auto-detect PRIMA di scaricare ─────────────────────────
 * Ordine: --torch-local → option `paths.torchLocal` (system/app settings) → scansione dei
 * venv Python più comuni (incluso ~/ai, dove il progetto installa gli ambienti) → PATH.
 * Una cartella è valida se contiene `lib/libtorch*` (pacchetto torch) oppure `libtorch*`
 * direttamente (cartella lib). Restituisce 1 e scrive il path; 0 se non trovato.
 * NIENTE variabili d'ambiente: la selezione passa da flag o options (AGENTS §0bis). */

static int torch_dir_has_libs(const char* dir)
{
    char lib[2400];
    snprintf(lib, sizeof lib, "%s/lib", dir);
    DIR* d = opendir(lib);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (!strncmp(e->d_name, "libtorch", 8) || !strncmp(e->d_name, "torch", 5)) {
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    d = opendir(dir);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (!strncmp(e->d_name, "libtorch", 8)) {
                closedir(d);
                return 1;
            }
        }
        closedir(d);
    }
    return 0;
}

static int torch_try(const char* dir, char* out, size_t cap)
{
    if (!dir || !*dir || access(dir, F_OK) != 0) return 0;
    if (!torch_dir_has_libs(dir)) return 0;
    snprintf(out, cap, "%s", dir);
    return 1;
}

/* Scansiona una radice di venv provando le posizioni note: <env>/site-packages/torch e
 * <env>/lib/pythonX.Y/site-packages/torch. Non serve un glob completo. */
static int torch_scan_venv(const char* base, char* out, size_t cap)
{
    DIR* d = opendir(base);
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char env[2400];
        snprintf(env, sizeof env, "%s/%s", base, e->d_name);
        struct stat st;
        if (stat(env, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        /* pacchetto torch in <env>/site-packages oppure in <env>/lib/pythonX.Y/site-packages */
        char c[2600];
        snprintf(c, sizeof c, "%s/site-packages/torch", env);
        if (torch_try(c, out, cap)) { found = 1; break; }
        snprintf(c, sizeof c, "%s/lib", env);
        DIR* l = opendir(c);
        if (l) {
            struct dirent* p;
            while (!found && (p = readdir(l))) {
                if (strncmp(p->d_name, "python", 6) != 0) continue;
                char c2[2800];
                snprintf(c2, sizeof c2, "%s/%s/site-packages/torch", c, p->d_name);
                if (torch_try(c2, out, cap)) found = 1;
            }
            closedir(l);
        }
    }
    closedir(d);
    return found;
}

static int torch_autodetect(char* out, size_t cap)
{
    const char* home = getenv("HOME");
    if (!home || !*home) return 0;

    /* 1) venv del progetto (~/ai/<env>) — la convenzione documentata in Ambienti-Env-AI */
    {
        char base[2400];
        snprintf(base, sizeof base, "%s/ai", home);
        if (torch_scan_venv(base, out, cap)) return 1;
    }
    /* 2) venv comuni nella home */
    static const char* venvs[] = { ".venv", "venv", "miniconda3/envs", "anaconda3/envs",
                                   "miniconda3", "anaconda3", NULL };
    for (int i = 0; venvs[i]; i++) {
        char base[2400];
        snprintf(base, sizeof base, "%s/%s", home, venvs[i]);
        if (torch_scan_venv(base, out, cap)) return 1;
        /* il venv stesso può essere la radice (pacchetto in lib/pythonX.Y/site-packages) */
        char c[2600];
        snprintf(c, sizeof c, "%s/%s/site-packages/torch", home, venvs[i]);
        if (torch_try(c, out, cap)) return 1;
    }
    /* 3) Python di sistema / Homebrew (site-packages globale) */
    static const char* sysbases[] = { "/opt/homebrew/lib", "/usr/local/lib",
                                      "/Library/Python", NULL };
    for (int i = 0; sysbases[i]; i++) {
        DIR* d = opendir(sysbases[i]);
        if (!d) continue;
        struct dirent* e;
        int found = 0;
        while (!found && (e = readdir(d))) {
            if (strncmp(e->d_name, "python", 6) != 0) continue;
            char c[2600];
            snprintf(c, sizeof c, "%s/%s/site-packages/torch", sysbases[i], e->d_name);
            if (torch_try(c, out, cap)) found = 1;
        }
        closedir(d);
        if (found) return 1;
    }
    return 0;
}

static int setup_torch(int argc, char** argv, int quiet)
{
    char mp[2400];
    manifest_of("ml-manifest.json", mp, sizeof mp);
    gmb_val_t root = load_json(mp);
    if (root.kind != GMB_TYPE_OBJECT) { fprintf(stderr, "?setup TORCH: manifest non leggibile (%s)\n", mp); return 1; }
    const char* rid = gmb_libs_rid();
    gmb_val_t runtimes = gmb_val_dict_get(root, "runtimes");
    gmb_val_t e = gmb_val_dict_get(runtimes, rid);
    if (e.kind != GMB_TYPE_OBJECT) { fprintf(stderr, "?setup TORCH: nessuna entry per RID '%s'\n", rid); return 1; }

    char dest[2048];
    gmb_paths_ml_dir(rid, dest, sizeof dest);
    gmb_fs_mkdir_p(dest);
    int force = fflag(argc, argv, 3, "--force");
    /* TORCH locale: flag → option `paths.torchLocal` → auto-detect (venv ~/ai, .venv, …).
     * Si prova PRIMA di scaricare: se il torch locale c'è, i file `source=local` vengono
     * copiati da lì e non serve rete per quelli. */
    const char* local_dir = fopt(argc, argv, 3, "--torch-local");
    const char* local_from = local_dir && *local_dir ? "--torch-local" : NULL;
    char local_cfg[2048] = "";
    if (!local_from && gmb_paths_get("torchLocal", local_cfg, sizeof local_cfg) && local_cfg[0]) {
        local_dir = local_cfg;
        local_from = "paths.torchLocal";
    }
    static char local_det[2048];
    if (!local_from && torch_autodetect(local_det, sizeof local_det)) {
        local_dir = local_det;
        local_from = "auto-detect";
    }
    if (local_from) printf("[torch] torch locale: %s (%s)\n", local_dir, local_from);
    else printf("[torch] nessun torch locale rilevato (flag --torch-local / option paths.torchLocal / venv ~/ai)\n");
    char libs_cache[2048];
    gmb_paths_libs_cache_dir(libs_cache, sizeof libs_cache);
    char libs_rid[2200];
    snprintf(libs_rid, sizeof libs_rid, "%s/%s", libs_cache, rid);

    gmb_val_t files = gmb_val_dict_get(e, "files");
    gmb_list_t list = gmb_val_foreach_list(files);
    gmb_int_t n = gmb_list_count(list);
    if (n <= 0) { fprintf(stderr, "?setup TORCH: nessun file dichiarato per %s\n", rid); return 1; }

    /* archivio ufficiale (una sola volta, solo se serve) */
    gmb_val_t arch = gmb_val_dict_get(e, "archive");
    const char* arch_url = (arch.kind == GMB_TYPE_OBJECT) ? json_str(arch, "url") : NULL;
    const char* arch_sha = (arch.kind == GMB_TYPE_OBJECT) ? json_str(arch, "sha256") : NULL;
    const char* arch_lib = (arch.kind == GMB_TYPE_OBJECT) ? json_str(arch, "libDir") : NULL;
    if (!arch_lib || !*arch_lib) arch_lib = "libtorch/lib";
    char arch_file[2400] = "", arch_dir[2400] = "";
    int arch_ready = 0;

    printf("[torch] RID=%s — destinazione %s\n", rid, dest);
    int ok = 0, failed = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_val_t f = gmb_list_get(list, i);
        if (f.kind != GMB_TYPE_OBJECT) continue;
        const char* name = json_str(f, "name");
        const char* sha = json_str(f, "sha256");
        const char* src = json_str(f, "source");
        const char* url = json_str(f, "url");
        long long size = json_int(f, "size", 0);
        if (!name || !*name) continue;
        char target[2600];
        snprintf(target, sizeof target, "%s/%s", dest, name);

        if (!force && file_exists(target)) {
            char have[65] = "";
            if (!sha || !gmb_sha256_file_hex(target, have, sizeof have) || !strcasecmp(have, sha)) {
                printf("  = %s (già in cache)\n", name);
                ok++;
                continue;
            }
        }
        mkdirs_for(target);

        if (src && !strcasecmp(src, "url") && url && *url) {
            if (download(url, sha, size, target, quiet) == 0) { printf("  + %s\n", name); ok++; }
            else failed++;
        } else if (src && !strcasecmp(src, "build")) {
            char from[2600];
            snprintf(from, sizeof from, "%s/%s", libs_rid, name);
            if (!file_exists(from)) {
                fprintf(stderr, "  ! %s: source=build ma %s non esiste (esegui `gmb libs ensure`)\n", name, from);
                failed++;
                continue;
            }
            FILE* in = fopen(from, "rb");
            FILE* out = fopen(target, "wb");
            if (!in || !out) { if (in) fclose(in); if (out) fclose(out); failed++; continue; }
            char buf[65536];
            size_t r;
            while ((r = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, r, out);
            fclose(in); fclose(out);
            printf("  + %s (da build)\n", name);
            ok++;
        } else if (src && !strcasecmp(src, "local")) {
            if (!local_dir || !*local_dir) {
                fprintf(stderr, "  ! %s: source=local ma nessun torch locale rilevato — passa "
                                "--torch-local <dir>, imposta paths.torchLocal, oppure usa un venv "
                                "in ~/ai (<venv>/lib/python*/site-packages/torch)\n", name);
                failed++;
                continue;
            }
            char from[2600];
            snprintf(from, sizeof from, "%s/lib/%s", local_dir, name);
            if (!file_exists(from)) snprintf(from, sizeof from, "%s/%s", local_dir, name);
            if (!file_exists(from)) { fprintf(stderr, "  ! %s: non trovato in %s\n", name, local_dir); failed++; continue; }
            FILE* in = fopen(from, "rb");
            FILE* out = fopen(target, "wb");
            if (!in || !out) { if (in) fclose(in); if (out) fclose(out); failed++; continue; }
            char buf[65536];
            size_t r;
            while ((r = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, r, out);
            fclose(in); fclose(out);
            printf("  + %s (da %s)\n", name, from);
            ok++;
        } else if (src && !strcasecmp(src, "archive")) {
            if (!arch_url || !*arch_url) {
                fprintf(stderr, "  ! %s: source=archive ma il manifest non ha archive.url\n", name);
                failed++;
                continue;
            }
            if (!arch_ready) {
                const char* ab = strrchr(arch_url, '/');
                snprintf(arch_file, sizeof arch_file, "%s/.pkg/%s", dest, ab ? ab + 1 : "libtorch.zip");
                snprintf(arch_dir, sizeof arch_dir, "%s/.pkg/unpacked", dest);
                mkdirs_for(arch_file);
                if (download(arch_url, arch_sha, 0, arch_file, quiet) != 0) { failed++; continue; }
                gmb_fs_rmtree(arch_dir);
                gmb_fs_mkdir_p(arch_dir);
                if (extract_archive(arch_file, arch_dir, quiet) != 0) { failed++; continue; }
                arch_ready = 1;
            }
            char from[2600];
            snprintf(from, sizeof from, "%s/%s/%s", arch_dir, arch_lib, name);
            if (!file_exists(from)) {
                fprintf(stderr, "  ! %s: non presente nell'archivio (%s)\n", name, from);
                failed++;
                continue;
            }
            FILE* in = fopen(from, "rb");
            FILE* out = fopen(target, "wb");
            if (!in || !out) { if (in) fclose(in); if (out) fclose(out); failed++; continue; }
            char buf[65536];
            size_t r;
            while ((r = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, r, out);
            fclose(in); fclose(out);
            printf("  + %s (da archivio)\n", name);
            ok++;
        } else {
            fprintf(stderr, "  ! %s: source '%s' non gestito\n", name, src ? src : "?");
            failed++;
        }
        if (sha && *sha) {
            char have[65] = "";
            if (gmb_sha256_file_hex(target, have, sizeof have) && strcasecmp(have, sha) != 0) {
                fprintf(stderr, "  ! %s: sha256 diverso dal manifest\n", name);
                unlink(target);
                failed++;
            }
        }
    }
    gmb_list_free(&list);
    if (arch_ready) gmb_fs_rmtree(arch_dir);
    char pkg[2400];
    snprintf(pkg, sizeof pkg, "%s/.pkg", dest);
    gmb_fs_rmtree(pkg);
    printf("[%s] torch: %d file pronti, %d errori -> %s\n", failed ? "PARZIALE" : "OK", ok, failed, dest);
    return failed ? 1 : 0;
}

/* ── OCR / LIBS: bundle runtime (tesseract incluso) ──────────────────────── */

static int setup_libs(void)
{
    printf("[libs] bootstrap dello scope ALL in ~/.gmbasic/libs/<rid>/ (curl + sha256)\n");
    int r = gmb_bootstrap(GMB_SCOPE_ALL);
    if (!r) { fprintf(stderr, "?setup libs: bootstrap fallito (rete o indice assente)\n"); return 1; }
    printf("[OK] librerie native pronte (tesseract/OCR incluso nel bundle)\n");
    return 0;
}

int gmbadmin_setup_component(const char* name, int argc, char** argv, int quiet, int yes)
{
    if (!name || !*name) name = "ALL";
    if (!strcasecmp(name, "LIBS")) return setup_libs();
    if (!strcasecmp(name, "OCR")) {
        printf("[ocr] tesseract/leptonica/tessdata fanno parte del bundle LIBS\n");
        return setup_libs();
    }
    if (!strcasecmp(name, "BROWSER")) return setup_archive_component("chromium-manifest.json", 1, argc, argv, quiet);
    if (!strcasecmp(name, "OBSCURA")) return setup_archive_component("obscura-manifest.json", 0, argc, argv, quiet);
    if (!strcasecmp(name, "TORCH"))   return setup_torch(argc, argv, quiet);
    if (!strcasecmp(name, "ALL")) {
        if (!yes && !fflag(argc, argv, 3, "--force")) {
            printf("Componenti: LIBS (runtime+OCR), TORCH, BROWSER, OBSCURA — alcuni download sono\n"
                   "grandi (centinaia di MB). Rilancia con --yes per procedere.\n");
            return 2;
        }
        int rc = 0;
        if (setup_libs()) rc = 1;
        if (setup_torch(argc, argv, quiet)) rc = 1;
        if (setup_archive_component("chromium-manifest.json", 1, argc, argv, quiet)) rc = 1;
        if (setup_archive_component("obscura-manifest.json", 0, argc, argv, quiet)) rc = 1;
        return rc;
    }
    fprintf(stderr, "?setup: componente '%s' sconosciuto (TORCH|BROWSER|OBSCURA|OCR|LIBS|ALL)\n", name);
    return 2;
}

/* ── libs publish: delega allo script di release (tool, non C#) ───────────── */

int gmbadmin_libs_publish(int argc, char** argv)
{
    const char* group = (argc > 3) ? argv[3] : NULL;
    const char* rid = (argc > 4) ? argv[4] : NULL;
    if (!group || !rid) {
        fprintf(stderr, "?uso: gmb libs publish <support|engine|core> <rid> [--yes]\n");
        return 2;
    }
    char root[2048], script[2400];
    if (!gmb_paths_repo_root(root, sizeof root) || !root[0]) {
        fprintf(stderr, "?libs publish: repo root non trovata\n");
        return 1;
    }
    snprintf(script, sizeof script, "%s/tools/sh/libs_publish_gh.sh", root);
    if (access(script, R_OK) != 0) { fprintf(stderr, "?libs publish: script assente (%s)\n", script); return 1; }
    char* av[8];
    int n = 0;
    av[n++] = "bash";
    av[n++] = script;
    av[n++] = (char*)group;
    av[n++] = (char*)rid;
    av[n++] = (char*)"--repo";
    av[n++] = (char*)"c-vision/GMBasicModules";
    av[n++] = (char*)"--index";
    av[n] = NULL;
    int rc = run_cmd(av);
    printf("libs publish: %s %s -> %s\n", group, rid, rc == 0 ? "OK" : "FALLITO");
    return rc == 0 ? 0 : 1;
}

/* ── libs index: rigenera l'indice delle librerie (script di release) ────── */

int gmbadmin_libs_index(int argc, char** argv)
{
    const char* rid = gmb_libs_rid();
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--rid") && i + 1 < argc) rid = argv[i + 1];
        else if (!strncmp(argv[i], "--rid=", 6)) rid = argv[i] + 6;
    }
    char root[2048], script[2400];
    if (!gmb_paths_repo_root(root, sizeof root) || !root[0]) {
        fprintf(stderr, "?libs index: repo root non trovata\n");
        return 1;
    }
    snprintf(script, sizeof script, "%s/tools/sh/libs_manifest_gh.sh", root);
    if (access(script, R_OK) != 0) { fprintf(stderr, "?libs index: script assente (%s)\n", script); return 1; }
    char* av[] = { "bash", script, "build", "--rid", (char*)rid, "--repo", "c-vision/GMBasicModules", NULL };
    int rc = run_cmd(av);
    printf("libs index (rid %s): %s\n", rid, rc == 0 ? "OK" : "FALLITO");
    return rc == 0 ? 0 : 1;
}

/* ── hf / mlx: download modelli (HF API + gmbfetch) ─────────────────────── */

static int token_from_cache(char* out, size_t cap)
{
    const char* h = getenv("HOME");
    if (!h) return 0;
    char p[1200];
    snprintf(p, sizeof p, "%s/.cache/huggingface/token", h);
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = 0;
    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = 0;
    return n > 0;
}

/* glob minimale: '*' = qualunque sequenza, altrimenti confronto esatto. */
static int glob_match(const char* pat, const char* s)
{
    if (!pat || !*pat) return 1;
    const char* star = strchr(pat, '*');
    if (!star) return strcmp(pat, s) == 0;
    size_t pre = (size_t)(star - pat);
    if (strncmp(pat, s, pre) != 0) return 0;
    const char* rest = star + 1;
    if (!*rest) return 1;
    return strstr(s + pre, rest) != NULL;
}

int gmbadmin_hf(int argc, char** argv, int mlx)
{
    const char* repo = NULL;
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') { repo = argv[i]; break; }
        if (i + 1 < argc && argv[i][1] != '-') i++;
    }
    if (!repo || !strcmp(repo, "--help") || !strcmp(repo, "-h")) {
        printf("uso: gmb %s <repo-id> [--dest D] [--token T] [--quiet]%s\n",
               mlx ? "mlx" : "hf", mlx ? "" : " [--file F|pattern]");
        if (mlx) printf("     MLX scarica sempre l'intero repo (--file non supportato).\n");
        return repo ? 0 : 2;
    }
    const char* file_pat = fopt(argc, argv, 2, "--file");
    if (mlx && file_pat) {
        fprintf(stderr, "?mlx: --file non supportato (scarica l'intero repo): usa `gmb hf`\n");
        return 2;
    }
    int quiet = fflag(argc, argv, 2, "--quiet");

    char dest[2048];
    const char* dopt = fopt(argc, argv, 2, "--dest");
    if (dopt && *dopt) snprintf(dest, sizeof dest, "%s", dopt);
    else {
        char root[1800];
        if (gmb_paths_repo_root(root, sizeof root) && root[0])
            snprintf(dest, sizeof dest, "%s/samples/ai/models/%s", root, mlx ? "mlx" : "hf");
        else
            snprintf(dest, sizeof dest, "models/%s", mlx ? "mlx" : "hf");
    }
    char token[512] = "";
    const char* topt = fopt(argc, argv, 2, "--token");
    if (topt && *topt) snprintf(token, sizeof token, "%s", topt);
    else token_from_cache(token, sizeof token);

    char url[2048];
    snprintf(url, sizeof url, "https://huggingface.co/api/models/%s", repo);
    if (!quiet) printf("[%s] '%s' — lista file da %s\n", mlx ? "MLX" : "HF", repo, url);
    char* body = gmb_net_http_get(url);
    if (!body) { fprintf(stderr, "?hf: API non raggiungibile (%s)\n", url); return 1; }
    gmb_val_t js = gmb_json_parse(body);
    free(body);
    if (js.kind != GMB_TYPE_OBJECT) { fprintf(stderr, "?hf: risposta API non valida (repo '%s'?)\n", repo); return 1; }
    gmb_val_t sib = gmb_val_dict_get(js, "siblings");
    gmb_list_t list = gmb_val_foreach_list(sib);
    gmb_int_t n = gmb_list_count(list);
    if (n <= 0) { fprintf(stderr, "?hf: nessun file elencato per '%s'\n", repo); gmb_list_free(&list); return 1; }

    char root[2400];
    snprintf(root, sizeof root, "%s/%s", dest, strrchr(repo, '/') ? strrchr(repo, '/') + 1 : repo);
    gmb_fs_mkdir_p(root);

    int done = 0, skipped = 0, failed = 0;
    for (gmb_int_t i = 0; i < n; i++) {
        gmb_val_t it = gmb_list_get(list, i);
        if (it.kind != GMB_TYPE_OBJECT) continue;
        const char* rf = json_str(it, "rfilename");
        if (!rf || !*rf) continue;
        if (file_pat && !glob_match(file_pat, rf)) continue;
        char target[4096];
        snprintf(target, sizeof target, "%s/%s", root, rf);
        mkdirs_for(target);
        if (file_exists(target) && !fflag(argc, argv, 2, "--force")) { skipped++; continue; }
        char furl[4096];
        snprintf(furl, sizeof furl, "https://huggingface.co/%s/resolve/main/%s", repo, rf);
        if (!quiet) printf("  + %s\n", rf);
        gmb_fetch_request req;
        memset(&req, 0, sizeof req);
        req.url = furl;
        req.token = token[0] ? token : NULL;
        req.dest_path = target;
        gmb_fetch_opts o;
        gmb_fetch_opts_default(&o);
        o.quiet = quiet;
        char err[512] = "";
        int rc = gmb_fetch_to_file(&req, &o, gmb_fetch_transport_curl(), err, sizeof err);
        if (rc == 0) done++;
        else { failed++; fprintf(stderr, "  ! %s: %s\n", rf, err[0] ? err : gmb_fetch_strerror(rc)); }
    }
    gmb_list_free(&list);
    printf("[%s] %s -> %s: %d scaricati, %d già presenti, %d errori\n",
           failed ? "PARZIALE" : "OK", repo, root, done, skipped, failed);
    return failed ? 1 : 0;
}

/* ── setup rm: rimuove la cache di un componente ─────────────────────────── */

int gmbadmin_setup_rm(const char* comp, int force)
{
    if (!comp || !*comp) comp = "ALL";
    const char* rid = gmb_libs_rid();
    char dirs[4][2048];
    int n = 0;
    int wants_libs = 0;
    if (!strcasecmp(comp, "BROWSER")) gmb_paths_chromium_dir(rid, dirs[n++], sizeof dirs[0]);
    else if (!strcasecmp(comp, "OBSCURA")) gmb_paths_obscura_dir(rid, dirs[n++], sizeof dirs[0]);
    else if (!strcasecmp(comp, "TORCH")) gmb_paths_ml_dir(rid, dirs[n++], sizeof dirs[0]);
    else if (!strcasecmp(comp, "LIBS") || !strcasecmp(comp, "OCR")) {
        char c[2048];
        gmb_paths_libs_cache_dir(c, sizeof c);
        snprintf(dirs[n++], sizeof dirs[0], "%s/%s", c, rid);
        wants_libs = 1;
    } else if (!strcasecmp(comp, "ALL")) {
        gmb_paths_chromium_dir(rid, dirs[n++], sizeof dirs[0]);
        gmb_paths_obscura_dir(rid, dirs[n++], sizeof dirs[0]);
        gmb_paths_ml_dir(rid, dirs[n++], sizeof dirs[0]);
    } else {
        fprintf(stderr, "?setup rm: componente '%s' sconosciuto (TORCH|BROWSER|OBSCURA|OCR|LIBS|ALL)\n", comp);
        return 2;
    }
    for (int i = 0; i < n; i++) {
        if (access(dirs[i], F_OK) != 0) { printf("[%s] nessuna cache: %s\n", comp, dirs[i]); continue; }
        if (!force && !wants_libs) {
            printf("[%s] rimuoverei %s (rilancia con --force)\n", comp, dirs[i]);
            continue;
        }
        if (wants_libs && !force) {
            fprintf(stderr, "?setup rm %s: rimuove TUTTE le librerie native di %s — serve --force\n", comp, rid);
            return 1;
        }
        gmb_fs_rmtree(dirs[i]);
        printf("[%s] rimosso %s\n", comp, dirs[i]);
    }
    return 0;
}

/* ── libs prune: file in cache non presenti nel manifest ─────────────────── */

int gmbadmin_libs_prune(int argc, char** argv)
{
    int dry = fflag(argc, argv, 3, "--dry-run");
    char mp[2400];
    manifest_of("libs-manifest.json", mp, sizeof mp);
    gmb_val_t root = load_json(mp);
    if (root.kind != GMB_TYPE_OBJECT) { fprintf(stderr, "?libs prune: manifest non leggibile (%s)\n", mp); return 1; }
    const char* rid = gmb_libs_rid();
    gmb_val_t r = gmb_val_dict_get(gmb_val_dict_get(root, "rids"), rid);
    gmb_val_t files = gmb_val_dict_get(r, "files");
    if (files.kind != GMB_TYPE_OBJECT) { fprintf(stderr, "?libs prune: nessuna entry per RID '%s'\n", rid); return 1; }

    char dir[2048];
    gmb_paths_libs_cache_dir(dir, sizeof dir);
    char full[2400];
    snprintf(full, sizeof full, "%s/%s", dir, rid);
    DIR* d = opendir(full);
    if (!d) { printf("[libs] cache assente: %s\n", full); return 0; }

    /* set dei nomi noti (il manifest elenca i file per nome, non per path) */
    gmb_int_t nk = 0;
    gmb_string_t* keys = gmb_val_dict_keys_array(files, &nk);
    int nkeys = (int)nk;

    struct dirent* de;
    int removed = 0, kept = 0;
    /* prima passata: file di primo livello */
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char p[4096];
        snprintf(p, sizeof p, "%s/%s", full, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) continue;
        int known = 0;
        for (int i = 0; keys && i < nkeys; i++)
            if (keys[i].data && !strcmp(keys[i].data, de->d_name)) { known = 1; break; }
        if (known) { kept++; continue; }
        if (S_ISLNK(st.st_mode)) { kept++; continue; }   /* alias derivati */
        if (dry) printf("  rimuoverei %s\n", de->d_name);
        else { unlink(p); printf("  rimosso %s\n", de->d_name); }
        removed++;
    }
    closedir(d);
    printf("[libs] %s: %d rimossi%s, %d noti in cache (%s/%s)\n",
           dry ? "dry-run" : "prune", removed, dry ? " (simulati)" : "", kept, dir, rid);
    return 0;
}
