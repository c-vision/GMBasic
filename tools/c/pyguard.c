/* ============================================================================
 * pyguard.c — sorvegliante del monorepo: QUARANTENA i file .py appena creati.
 *
 * Regola del progetto GMBasicMonoRepo: gli strumenti si scrivono in BASIC
 * (tools/basic), non in Python. Questo sorvegliante rende la regola meccanica:
 * appena un file *.py compare nell'albero viene spostato nella quarantena
 * (~/.gmbasic/quarantine/py/) e l'evento e' registrato in un log.
 *
 * Sono ESCLUSI (sorgenti di terzi, non nostri strumenti):
 *   build/  _deps/  .build/  .content/  node_modules/  .git/  vendor/
 *
 * Uso:
 *   pyguard <radice> [quarantena]     (resta in ascolto; FSEvents su macOS)
 *   pyguard --scan <radice> [quarantena]  (una passata, per il primo avvio)
 *
 * Compilato da tools/sh/no_python_guard.sh (cc), nessuna dipendenza esterna.
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

#define MAXQ 4096

static char g_quarantine[MAXQ];
static char g_root[MAXQ];

static int skip_path(const char* p) {
    static const char* skips[] = {
        "/build/", "/_deps/", "/.build/", "/.content/",
        "/node_modules/", "/.git/", "/vendor/", NULL
    };
    for (int i = 0; skips[i]; i++) if (strstr(p, skips[i])) return 1;
    return 0;
}

static int ends_py(const char* p) {
    size_t n = strlen(p);
    return n > 3 && strcmp(p + n - 3, ".py") == 0;
}

static void mkdirs(const char* path) {
    char tmp[MAXQ];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
    }
    mkdir(tmp, 0755);
}

/* Sposta il file in quarantena. Ritorna 1 se quarantena effettuata. */
static int quarantine(const char* path) {
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);

    char destDir[MAXQ + 64];
    snprintf(destDir, sizeof(destDir), "%s/%s", g_quarantine, stamp);
    mkdirs(destDir);

    char dest[MAXQ + 320];
    snprintf(dest, sizeof(dest), "%s/%s", destDir, base);

    /* Evita sovrascritture: aggiunge un contatore. */
    int k = 1;
    while (access(dest, F_OK) == 0) {
        snprintf(dest, sizeof(dest), "%s/%d_%s", destDir, k, base);
        k++;
    }

    if (rename(path, dest) != 0) {
        /* fallback: copia + rimozione (volumi diversi) */
        FILE* in = fopen(path, "rb");
        if (!in) return 0;
        FILE* out = fopen(dest, "wb");
        if (!out) { fclose(in); return 0; }
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
        fclose(in);
        fclose(out);
        unlink(path);
    }

    FILE* log = fopen("/tmp/gmb_pyguard.log", "a");
    if (log) {
        fprintf(log, "%s\t%s\t%s\n", stamp, path, dest);
        fclose(log);
    }
    printf("[pyguard] QUARANTENA: %s -> %s\n", path, dest);
    fflush(stdout);
    return 1;
}

/* ---- una passata ricorsiva (primo avvio / --scan) ------------------------- */
static int scan_dir(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    int count = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[MAXQ + 512];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (skip_path(path)) continue;
        struct stat st;
        if (lstat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            count += scan_dir(path);
        } else if (S_ISREG(st.st_mode) && ends_py(path)) {
            count += quarantine(path);
        }
    }
    closedir(d);
    return count;
}

#ifdef __APPLE__
static void on_events(ConstFSEventStreamRef stream, void* info, size_t numEvents,
                      void* eventPaths, const FSEventStreamEventFlags* flags,
                      const FSEventStreamEventId* ids) {
    (void)stream; (void)info; (void)ids;
    char** paths = (char**)eventPaths;
    for (size_t i = 0; i < numEvents; i++) {
        const char* p = paths[i];
        if (!p || !ends_py(p) || skip_path(p)) continue;
        if (access(p, F_OK) != 0) continue;   /* gia' rimosso */
        quarantine(p);
    }
}
#endif

int main(int argc, char** argv) {
    int scan_only = 0;
    int argi = 1;
    if (argc > 1 && strcmp(argv[1], "--scan") == 0) { scan_only = 1; argi = 2; }
    if (argc <= argi) {
        fprintf(stderr, "uso: pyguard [--scan] <radice> [quarantena]\n");
        return 2;
    }
    snprintf(g_root, sizeof(g_root), "%s", argv[argi]);
    if (argc > argi + 1) {
        snprintf(g_quarantine, sizeof(g_quarantine), "%s", argv[argi + 1]);
    } else {
        const char* home = getenv("HOME");
        snprintf(g_quarantine, sizeof(g_quarantine), "%s/.gmbasic/quarantine/py", home ? home : ".");
    }
    mkdirs(g_quarantine);

    if (scan_only) {
        int n = scan_dir(g_root);
        printf("[pyguard] passata completata: %d file in quarantena\n", n);
        return n > 0 ? 0 : 0;
    }

#ifdef __APPLE__
    CFStringRef path = CFStringCreateWithCString(NULL, g_root, kCFStringEncodingUTF8);
    CFArrayRef paths = CFArrayCreate(NULL, (const void**)&path, 1, &kCFTypeArrayCallBacks);
    FSEventStreamRef stream = FSEventStreamCreate(NULL, on_events, NULL, paths,
        kFSEventStreamEventIdSinceNow, 0.2,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer |
        kFSEventStreamCreateFlagWatchRoot);
    if (!stream) {
        fprintf(stderr, "[pyguard] FSEvents non disponibile\n");
        return 1;
    }
    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    if (!FSEventStreamStart(stream)) {
        fprintf(stderr, "[pyguard] avvio FSEvents fallito\n");
        return 1;
    }
    printf("[pyguard] attivo su %s (quarantena: %s)\n", g_root, g_quarantine);
    fflush(stdout);
    CFRunLoopRun();
    return 0;
#else
    fprintf(stderr, "[pyguard] watcher nativo solo macOS; usare --scan (polling) su questa piattaforma\n");
    return 3;
#endif
}
