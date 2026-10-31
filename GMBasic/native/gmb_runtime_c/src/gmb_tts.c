/* gmb_tts.c — F4 multimodale (2026-09-11): sintesi vocale (TTS) via motore di SISTEMA.
 *
 * Condivisa da VM/C, C99Native (§0bis): gmb_tts_synthesize(text, output)
 * scrive un WAV in `output` usando il motore OS (macOS `say`, Linux `espeak`/`espeak-ng`).
 * Il testo passa da un file temporaneo (nessun testo nella riga di comando). Ritorna
 * 1 (ok) / 0 (errore: argomenti vuoti, motore assente, scrittura fallita) — mai stub.
 */
#include "gmb_runtime.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Quoting shell POSIX: racchiude tra apici singoli, sostituendo ' con '\'' . */
static void tts_shq(char* dst, size_t cap, const char* s)
{
    size_t o = 0;
    if (cap < 4) { if (cap) dst[0] = '\0'; return; }
    dst[o++] = '\'';
    for (; s && *s && o + 5 < cap; s++)
    {
        if (*s == '\'') { memcpy(dst + o, "'\\''", 4); o += 4; }
        else dst[o++] = *s;
    }
    dst[o++] = '\'';
    dst[o] = '\0';
}

/* mkdir -p della cartella che contiene `path` (best effort; se esiste, no-op). */
static void tts_mkdir_parent(const char* path)
{
    char dir[1024];
    char* slash;
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(dir)) return;
    memcpy(dir, path, n + 1);
    slash = strrchr(dir, '/');
    if (!slash || slash == dir) return;
    *slash = '\0';
    /* mkdir -p annidato via shell (path quotato). */
    {
        char q[1400], cmd[1600];
        tts_shq(q, sizeof(q), dir);
        snprintf(cmd, sizeof(cmd), "mkdir -p %s >/dev/null 2>&1", q);
        system(cmd);
    }
}

int gmb_tts_synthesize(const char* text, const char* output)
{
    char tmpl[] = "/tmp/gmb_tts_XXXXXX";
    char qt[512], qo[1200], cmd[2048];
    int fd, rc;
    size_t n;
    ssize_t w;

    if (!text || !text[0] || !output || !output[0]) return 0;
    tts_mkdir_parent(output);

    fd = mkstemp(tmpl);
    if (fd < 0) return 0;
    n = strlen(text);
    w = write(fd, text, n);
    close(fd);
    if (w != (ssize_t)n) { unlink(tmpl); return 0; }

    tts_shq(qt, sizeof(qt), tmpl);
    tts_shq(qo, sizeof(qo), output);
#if defined(__APPLE__)
    snprintf(cmd, sizeof(cmd),
        "/usr/bin/say -f %s -o %s --data-format=LEI16@22050 >/dev/null 2>&1", qt, qo);
#else
    snprintf(cmd, sizeof(cmd),
        "espeak -f %s -w %s >/dev/null 2>&1", qt, qo);
#endif
    rc = system(cmd);
    unlink(tmpl);
    return rc == 0 ? 1 : 0;
}
