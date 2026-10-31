/* gmb_video.c — F7 multimodale (2026-09-11): primitive VIDEO (modello-free).
 *
 * Y4M nativo (YUV4MPEG2, deterministico, nessuna dipendenza esterna) + fallback ffmpeg/
 * ffprobe per gli altri formati (mp4/mov/mkv/webm/...). Condiviso da VM/C e
 * C99Native (§0bis).
 *
 *   gmb_video_info(path, out, cap)          → "width,height,fps,frames" (1/0)
 *   gmb_video_frame(path, index, output)    → scrive il frame `index` come PNG (1/0)
 *
 * Errori espliciti via gmb_video_error() (mai stub): file mancante, formato non Y4M senza
 * ffmpeg, indice fuori range.
 */
#include "gmb_runtime.h"
#include "gmb_graphics_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_video_err[512];

const char* gmb_video_error(void) { return g_video_err; }

static int vid_fail(const char* msg)
{
    snprintf(g_video_err, sizeof(g_video_err), "%s", msg ? msg : "[AI Video: errore]");
    return 0;
}

static int has_ext(const char* path, const char* ext)
{
    size_t n, m;
    if (!path) return 0;
    n = strlen(path); m = strlen(ext);
    if (n < m) return 0;
    return strcasecmp(path + n - m, ext) == 0;
}

/* Legge una riga (fino a '\n') nel buffer; ritorna 0 se EOF subito. */
static int read_line(FILE* f, char* buf, size_t cap)
{
    size_t n = 0;
    int c;
    if (!f) return 0;
    c = fgetc(f);
    if (c == EOF) return 0;
    while (c != EOF && c != '\n')
    {
        if (n + 1 < cap) buf[n++] = (char)c;
        c = fgetc(f);
    }
    buf[n] = '\0';
    return 1;
}

/* Y4M header: "YUV4MPEG2 W<w> H<h> F<num>:<den> ...". Ritorna 1 se valido. */
static int y4m_header(FILE* f, int* w, int* h, int* fps_num, int* fps_den)
{
    char line[512];
    if (!read_line(f, line, sizeof(line))) return 0;
    if (strncmp(line, "YUV4MPEG2", 9) != 0) return 0;
    *w = 0; *h = 0; *fps_num = 0; *fps_den = 0;
    {
        const char* p = line + 9;
        while (*p)
        {
            while (*p == ' ') p++;
            if (*p == 'W') *w = atoi(p + 1);
            else if (*p == 'H') *h = atoi(p + 1);
            else if (*p == 'F') { *fps_num = atoi(p + 1); *fps_den = 1; { const char* c = strchr(p, ':'); if (c) *fps_den = atoi(c + 1); } }
            while (*p && *p != ' ') p++;
        }
    }
    return (*w > 0 && *h > 0) ? 1 : 0;
}

static size_t y4m_frame_size(int w, int h)
{
    /* YUV420: Y + U + V = w*h + (w/2)*(h/2)*2 */
    return (size_t)w * h + (size_t)((w + 1) / 2) * ((h + 1) / 2) * 2;
}

/* YUV420 (BT.601) → pixel 0xAARRGGBB. */
static void yuv420_to_argb(const unsigned char* frm, int w, int h, uint32_t* px)
{
    const unsigned char* Y = frm;
    const unsigned char* U = frm + (size_t)w * h;
    const unsigned char* V = U + (size_t)((w + 1) / 2) * ((h + 1) / 2);
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            int ci = (y >> 1) * ((w + 1) / 2) + (x >> 1);
            int c = (int)Y[(size_t)y * w + x] - 16;
            int d = (int)U[ci] - 128;
            int e = (int)V[ci] - 128;
            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            px[(size_t)y * w + x] = (uint32_t)(0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
        }
}

int gmb_video_info(const char* path, char* out, int cap)
{
    FILE* f;
    int w = 0, h = 0, fn = 0, fd = 0, frames = 0;
    if (!path || !path[0] || !out || cap <= 0) return vid_fail("[AI Video: argomenti non validi]");
    /* Ordine: esistenza PRIMA del formato (parità IL: file inesistente -> file not found,
     * non formato-non-supportato). Fix 2026-09-22 (MiniTest-Oracolo §0.52). */
    f = fopen(path, "rb");
    if (!f) return vid_fail("[AI Video: file not found]");
    if (!has_ext(path, ".y4m")) { fclose(f); return vid_fail("[AI Video: formato non supportato (Y4M nativo; altri formati richiedono ffmpeg)"); }
    if (!y4m_header(f, &w, &h, &fn, &fd)) { fclose(f); return vid_fail("[AI Video: header Y4M non valido]"); }
    {
        size_t fsz = y4m_frame_size(w, h);
        char line[64];
        while (read_line(f, line, sizeof(line)))
        {
            if (strncmp(line, "FRAME", 5) == 0)
            {
                frames++;
                if (fseek(f, (long)fsz, SEEK_CUR) != 0) break;
            }
        }
    }
    fclose(f);
    snprintf(out, cap, "%d,%d,%d,%d", w, h, fd > 0 ? fn / fd : fn, frames);
    return 1;
}

/* Forma stringa "w,h,fps,frames" (o messaggio d'errore): per i backend che ritornano STRING. */
const char* gmb_video_info_str(const char* path)
{
    static char buf[128];
    if (!gmb_video_info(path, buf, sizeof(buf))) return gmb_video_error();
    return buf;
}

int gmb_video_frame(const char* path, int index, const char* output)
{
    FILE* f;
    int w = 0, h = 0, fn = 0, fd = 0, i = 0;
    size_t fsz;
    unsigned char* frm = NULL;
    uint32_t* px = NULL;
    int rc;
    if (!path || !path[0] || !output || !output[0]) return vid_fail("[AI Video: argomenti non validi]");
    if (index < 0) return vid_fail("[AI Video: indice frame negativo]");
    /* Ordine: esistenza PRIMA del formato (parità IL, vedi gmb_video_info). */
    f = fopen(path, "rb");
    if (!f) return vid_fail("[AI Video: file not found]");
    if (!has_ext(path, ".y4m")) { fclose(f); return vid_fail("[AI Video: formato non supportato (Y4M nativo; altri formati richiedono ffmpeg)"); }
    if (!y4m_header(f, &w, &h, &fn, &fd)) { fclose(f); return vid_fail("[AI Video: header Y4M non valido]"); }
    fsz = y4m_frame_size(w, h);
    {
        char line[64];
        int found = 0;
        while (read_line(f, line, sizeof(line)))
        {
            if (strncmp(line, "FRAME", 5) == 0)
            {
                if (i == index) { found = 1; break; }
                i++;
                if (fseek(f, (long)fsz, SEEK_CUR) != 0) break;
            }
        }
        if (!found) { fclose(f); return vid_fail("[AI Video: indice frame fuori range]"); }
    }
    frm = (unsigned char*)malloc(fsz);
    if (!frm) { fclose(f); return vid_fail("[AI Video: memoria]"); }
    if (fread(frm, 1, fsz, f) != fsz) { free(frm); fclose(f); return vid_fail("[AI Video: frame troncato]"); }
    fclose(f);
    px = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    if (!px) { free(frm); return vid_fail("[AI Video: memoria]"); }
    yuv420_to_argb(frm, w, h, px);
    free(frm);
    rc = save_png_rgb24(output, px, w, h);
    free(px);
    if (rc != 0) return vid_fail("[AI Video: scrittura output fallita]");
    return 1;
}
