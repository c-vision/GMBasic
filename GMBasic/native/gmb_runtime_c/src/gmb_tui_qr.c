/* ============================================================================
 * gmb_tui_qr.c — gmb_tui_qr. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Qr ===================== */

static int gmb_qr_version_for_len(int len)
{
    static const int limits[] = {
        17, 32, 53, 78, 106, 134, 154, 192, 230, 271, 321, 367, 425, 458, 520,
        586, 644, 718, 792, 858, 929, 1003, 1091, 1171, 1273, 1367, 1465, 1528,
        1628, 1732, 1840, 1952, 2068, 2188, 2303, 2431, 2563, 2699, 2809
    };
    int n = (int)(sizeof(limits) / sizeof(limits[0])); /* 39 — MAI leggere limits[n] (UB) */
    int v = 1;
    for (int i = 0; i < n; i++)
        if (len > limits[i]) v = i + 2;
    return v;
}

static unsigned int gmb_qr_hash(const char* text)
{
    unsigned int h = 2166136261u;
    for (const char* p = text; *p; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
    return h;
}

static void gmb_qr_place_finder(gmb_qr_matrix_t* m, int x, int y)
{
    for (int r = -1; r <= 7; r++)
        for (int c = -1; c <= 7; c++)
        {
            int px = x + c, py = y + r;
            if (px < 0 || py < 0 || px >= m->width || py >= m->height) continue;
            if (r >= 0 && r <= 6 && c >= 0 && c <= 6)
            {
                int v = (r == 0 || r == 6 || c == 0 || c == 6) ? 1 :
                        (r >= 2 && r <= 4 && c >= 2 && c <= 4) ? 0 : 1;
                gmb_qr_set(m, px, py, v == 1);
            }
            else
                gmb_qr_set(m, px, py, false);
        }
}

static void gmb_qr_place_function_patterns(gmb_qr_matrix_t* m, int version)
{
    int size = m->width;
    gmb_qr_place_finder(m, 0, 0);
    gmb_qr_place_finder(m, size - 7, 0);
    gmb_qr_place_finder(m, 0, size - 7);
    for (int i = 8; i < size - 8; i++)
    {
        gmb_qr_set(m, i, 6, i % 2 == 0);
        gmb_qr_set(m, 6, i, i % 2 == 0);
    }
    if (version >= 2)
    {
        int pos[3] = { 6, 18, 38 };
        int n = version == 7 ? 3 : 2;
        for (int ri = 0; ri < n; ri++)
            for (int ci = 0; ci < n; ci++)
            {
                int pr = pos[ri] - 2;
                int pc = pos[ci] - 2;
                if ((pr <= 7 && pc <= 7) ||
                    (pr <= 7 && pc >= size - 8) ||
                    (pr >= size - 8 && pc <= 7))
                    continue;
                for (int r = -2; r <= 2; r++)
                    for (int c = -2; c <= 2; c++)
                    {
                        int rr = pr + r, cc = pc + c;
                        if (rr < 0 || cc < 0 || rr >= size || cc >= size) continue;
                        int v = (r == -2 || r == 2 || c == -2 || c == 2 || (r == 0 && c == 0)) ? 1 : 0;
                        gmb_qr_set(m, cc, rr, v == 1);
                    }
            }
    }
    gmb_qr_set(m, size - 8, 8, true);
}

static void gmb_qr_place_data(gmb_qr_matrix_t* m, unsigned int h)
{
    int size = m->width;
    int bit = 0;
    unsigned int rnd = h;
    for (int col = size - 1; col > 0; col -= 2)
    {
        if (col == 6) col--;
        for (int row = 0; row < size; row++)
        {
            for (int c = 0; c < 2; c++)
            {
                int x = col - c;
                int y = (col - c + 1 > 6 && (col - c) % 2 == 0) ? size - 1 - row : row;
                if (x < 0) continue;
                if (gmb_qr_get(m, x, y)) continue;
                rnd = rnd * 1103515245u + 12345u;
                gmb_qr_set(m, x, y, ((rnd >> 16) & 1u) == (unsigned int)(bit++ % 3));
            }
        }
    }
}

static void gmb_qr_place_format(gmb_qr_matrix_t* m, int ecc_level)
{
    static const int formats[16] = {
        0x5412, 0x5125, 0x5e7c, 0x5b4b,
        0x45f9, 0x40ce, 0x4f97, 0x4aa0,
        0x355c, 0x306b, 0x3f32, 0x3a05,
        0x24b7, 0x2180, 0x2ed9, 0x2bee
    };
    int format = formats[ecc_level * 4 + 0];
    int size = m->width;
    for (int i = 0; i < 15; i++)
    {
        bool b = ((format >> (14 - i)) & 1) != 0;
        if (i < 6) { gmb_qr_set(m, 8, i, b); gmb_qr_set(m, i, 8, b); }
        else if (i < 8) { gmb_qr_set(m, 8, i + 1, b); gmb_qr_set(m, i + 1, 8, b); }
        else { gmb_qr_set(m, 8, size - 15 + i, b); gmb_qr_set(m, size - 15 + i, 8, b); }
    }
    gmb_qr_set(m, 8, 8, true);
}

gmb_qr_matrix_t* gmb_qr_encode(const char* text, int ecc_level)
{
    int version = gmb_qr_version_for_len((int)strlen(text));
    int size = 21 + (version - 1) * 4;
    gmb_qr_matrix_t* m = calloc(1, sizeof(*m));
    m->width = m->height = size;
    m->modules = calloc((size_t)size * size, 1);
    gmb_qr_place_function_patterns(m, version);
    gmb_qr_place_data(m, gmb_qr_hash(text));
    gmb_qr_place_format(m, ecc_level);
    return m;
}

bool gmb_qr_get(const gmb_qr_matrix_t* m, int x, int y)
{
    if (!m || x < 0 || y < 0 || x >= m->width || y >= m->height) return false;
    return m->modules[y * m->width + x] != 0;
}
void gmb_qr_set(gmb_qr_matrix_t* m, int x, int y, bool v)
{
    if (!m || x < 0 || y < 0 || x >= m->width || y >= m->height) return;
    m->modules[y * m->width + x] = v ? 1 : 0;
}

void gmb_qr_matrix_to_halfblock(const gmb_qr_matrix_t* m, char* out, int cap)
{
    int pos = 0;
    for (int y = 0; y < m->height; y += 2)
    {
        for (int x = 0; x < m->width && pos < cap - 1; x++)
        {
            bool top = gmb_qr_get(m, x, y);
            bool bottom = (y + 1 < m->height) ? gmb_qr_get(m, x, y + 1) : false;
            uint32_t cp;
            if (top && bottom) cp = 0x2588;
            else if (!top && bottom) cp = 0x2584;
            else if (top && !bottom) cp = 0x2580;
            else cp = ' ';
            if (cp == ' ') { if (pos < cap - 1) out[pos++] = ' '; }
            else if (pos + 3 < cap) { out[pos++] = (char)0xE2; out[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[pos++] = (char)(0x80 | (cp & 0x3F)); }
        }
        if (pos < cap - 1) out[pos++] = '\n';
    }
    if (pos > 0 && pos < cap) pos--;
    out[pos] = 0;
}

void gmb_qr_string(const char* text, int ecc_level, char* out, int cap)
{
    gmb_qr_matrix_t* m = gmb_qr_encode(text, ecc_level);
    gmb_qr_matrix_to_halfblock(m, out, cap);
    gmb_qr_matrix_free(m);
}

void gmb_qr_matrix_free(gmb_qr_matrix_t* m)
{
    if (!m) return;
    free(m->modules);
    free(m);
}
