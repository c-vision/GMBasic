/* gmb_imgdiff.c — confronto pixel di screenshot (PNG via runtime, BMP minimale).
 * Uso: gmb_imgdiff <a> <b> [tolleranza_pct=0.5]
 * Stampa "IMGDIFF total=N diff=M pct=P" e esce 0 se pct <= tolleranza.
 * Confronta solo RGB (l'alpha può differire tra encoder BMP/PNG). */
#include "gmb_graphics_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int w, h; uint32_t* px; } img_t;

static uint32_t rd32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int load_png(const char* path, img_t* out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return -1; }
    unsigned char* data = (unsigned char*)malloc((size_t)len);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)len, f) != (size_t)len) { free(data); fclose(f); return -1; }
    fclose(f);
    uint32_t* px = NULL;
    int w = 0, h = 0;
    int rc = png_decode(data, (size_t)len, &px, &w, &h);
    free(data);
    if (rc != 0 || !px) return -1;
    out->w = w; out->h = h; out->px = px;
    return 0;
}

static int load_bmp(const char* path, img_t* out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char hdr[54];
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') { fclose(f); return -1; }
    uint32_t data_off = rd32(hdr + 10);
    int w = (int)rd32(hdr + 18);
    int h = (int)rd32(hdr + 22);
    uint16_t bpp = rd16(hdr + 28);
    uint32_t comp = rd32(hdr + 30);
    /* BI_RGB (0) o BI_BITFIELDS (3): il C# salva 32bpp con compression=3.
     * In entrambi i casi il layout 32bpp è BGRA in memoria; data_off salta le mask. */
    if ((comp != 0 && comp != 3) || (bpp != 24 && bpp != 32) || w <= 0 || h == 0) { fclose(f); return -1; }
    int top_down = h < 0;
    if (top_down) h = -h;
    int stride = ((w * bpp / 8) + 3) & ~3;
    unsigned char* row = (unsigned char*)malloc((size_t)stride);
    uint32_t* px = (uint32_t*)malloc((size_t)w * h * 4);
    if (!row || !px) { free(row); free(px); fclose(f); return -1; }
    fseek(f, (long)data_off, SEEK_SET);
    for (int y = 0; y < h; y++)
    {
        int dst_y = top_down ? y : (h - 1 - y);
        if (fread(row, 1, (size_t)stride, f) != (size_t)stride) { free(row); free(px); fclose(f); return -1; }
        for (int x = 0; x < w; x++)
        {
            unsigned char b = row[x * (bpp / 8) + 0];
            unsigned char g = row[x * (bpp / 8) + 1];
            unsigned char r = row[x * (bpp / 8) + 2];
            px[dst_y * w + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
    free(row);
    fclose(f);
    out->w = w; out->h = h; out->px = px;
    return 0;
}

static int load(const char* path, img_t* out)
{
    size_t n = strlen(path);
    if (n >= 4 && strcmp(path + n - 4, ".png") == 0) return load_png(path, out);
    if (n >= 4 && strcmp(path + n - 4, ".bmp") == 0) return load_bmp(path, out);
    if (load_png(path, out) == 0) return 0;
    return load_bmp(path, out);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("uso: gmb_imgdiff <a.png|bmp> <b.png|bmp> [tolleranza_pct=0.5]\n");
        return 2;
    }
    double tol = argc > 3 ? atof(argv[3]) : 0.5;
    img_t a = {0}, b = {0};
    if (load(argv[1], &a) != 0) { printf("IMGDIFF errore lettura %s\n", argv[1]); return 2; }
    if (load(argv[2], &b) != 0) { printf("IMGDIFF errore lettura %s\n", argv[2]); return 2; }
    /* HiDPI: il motore C# può salvare a 2x (retina) la pagina logica. Se una
     * immagine è esattamente 2x l'altra, si campiona quella grande a indici pari
     * (equivale al contenuto logico sia per upscale nearest sia bilineare 2x). */
    int phase = getenv("IMGDIFF_PHASE") && strcmp(getenv("IMGDIFF_PHASE"), "odd") == 0 ? 1 : 0;
    if (a.w == b.w * 2 && a.h == b.h * 2)
    {
        img_t d = { b.w, b.h, (uint32_t*)malloc((size_t)b.w * b.h * 4) };
        if (!d.px) { free(a.px); free(b.px); return 2; }
        for (int y = 0; y < d.h; y++)
            for (int x = 0; x < d.w; x++)
                d.px[y * d.w + x] = a.px[(y * 2 + phase) * a.w + (x * 2 + phase)];
        free(a.px); a = d;
    }
    else if (b.w == a.w * 2 && b.h == a.h * 2)
    {
        img_t d = { a.w, a.h, (uint32_t*)malloc((size_t)a.w * a.h * 4) };
        if (!d.px) { free(a.px); free(b.px); return 2; }
        for (int y = 0; y < d.h; y++)
            for (int x = 0; x < d.w; x++)
                d.px[y * d.w + x] = b.px[(y * 2 + phase) * b.w + (x * 2 + phase)];
        free(b.px); b = d;
    }
    if (a.w != b.w || a.h != b.h)
    {
        printf("IMGDIFF size %dx%d vs %dx%d -> DIFF\n", a.w, a.h, b.w, b.h);
        free(a.px); free(b.px);
        return 1;
    }
    long total = (long)a.w * a.h, diff = 0;
    for (long i = 0; i < total; i++)
    {
        uint32_t pa = a.px[i] & 0xFFFFFFu;
        uint32_t pb = b.px[i] & 0xFFFFFFu;
        if (pa != pb) diff++;
    }
    double pct = total > 0 ? 100.0 * (double)diff / (double)total : 0.0;
    if (getenv("IMGDIFF_VERBOSE"))
    {
        long shown = 0;
        for (long i = 0; i < total && shown < 12; i++)
        {
            uint32_t pa = a.px[i] & 0xFFFFFFu, pb = b.px[i] & 0xFFFFFFu;
            if (pa != pb)
            {
                printf("  px(%d,%d) a=%06X b=%06X\n", (int)(i % a.w), (int)(i / a.w), pa, pb);
                shown++;
            }
        }
        /* Bounding box di un colore (IMGDIFF_BBOX=RRGGBB) in entrambe le immagini. */
        const char* bb = getenv("IMGDIFF_BBOX");
        if (bb)
        {
            uint32_t want = (uint32_t)strtoul(bb, NULL, 16);
            for (int img = 0; img < 2; img++)
            {
                img_t* im = img == 0 ? &a : &b;
                int x0 = im->w, y0 = im->h, x1 = -1, y1 = -1;
                for (int y = 0; y < im->h; y++)
                    for (int x = 0; x < im->w; x++)
                        if ((im->px[y * im->w + x] & 0xFFFFFFu) == want)
                        {
                            if (x < x0) x0 = x; if (x > x1) x1 = x;
                            if (y < y0) y0 = y; if (y > y1) y1 = y;
                        }
                printf("  bbox %s img%d = (%d,%d)-(%d,%d)\n", bb, img, x0, y0, x1, y1);
            }
        }
        /* Istogramma coppie (a,b) per capire se è palette o geometria. */
        typedef struct { uint32_t a, b; long n; } pair_t;
        pair_t top[8];
        memset(top, 0, sizeof(top));
        for (long i = 0; i < total; i++)
        {
            uint32_t pa = a.px[i] & 0xFFFFFFu, pb = b.px[i] & 0xFFFFFFu;
            if (pa == pb) continue;
            int slot = -1;
            for (int k = 0; k < 8; k++) if (top[k].n && top[k].a == pa && top[k].b == pb) { slot = k; break; }
            if (slot < 0)
            {
                for (int k = 0; k < 8; k++) if (top[k].n == 0) { slot = k; break; }
                if (slot < 0) { long mn = top[0].n; slot = 0; for (int k = 1; k < 8; k++) if (top[k].n < mn) { mn = top[k].n; slot = k; } }
                top[slot].a = pa; top[slot].b = pb; top[slot].n = 0;
            }
            top[slot].n++;
        }
        for (int k = 0; k < 8; k++)
            if (top[k].n) printf("  pair a=%06X b=%06X n=%ld\n", top[k].a, top[k].b, top[k].n);
        int gx = 16, gy = 12;
        for (int gyi = 0; gyi < gy; gyi++)
        {
            printf("  grid ");
            for (int gxi = 0; gxi < gx; gxi++)
            {
                long cnt = 0;
                for (int y = gyi * a.h / gy; y < (gyi + 1) * a.h / gy; y++)
                    for (int x = gxi * a.w / gx; x < (gxi + 1) * a.w / gx; x++)
                        if ((a.px[y * a.w + x] & 0xFFFFFFu) != (b.px[y * a.w + x] & 0xFFFFFFu)) cnt++;
                printf("%4ld", cnt);
            }
            printf("\n");
        }
    }
    printf("IMGDIFF total=%ld diff=%ld pct=%.3f%% tol=%.2f%% %s\n", total, diff, pct, tol, pct <= tol ? "OK" : "DIFF");
    free(a.px); free(b.px);
    return pct <= tol ? 0 : 1;
}
