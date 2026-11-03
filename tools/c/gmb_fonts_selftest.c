/* gmb_fonts_selftest.c — selftest del runtime FONTS (TrueType/OpenType).
 *
 * Verifica le INVARIANTI dell'API, non numeri legati a un font specifico: cosi' il test
 * resta valido su qualunque TTF/OTF (Apple, Linux, font di un progetto).
 *
 * Uso (dalla root del monorepo):
 *   cc -I GMBasic/native/gmb_runtime_c/include tools/c/gmb_fonts_selftest.c \
 *      -L GMBasic/native/gmb_runtime_c/build -lgmb_fonts \
 *      -Wl,-rpath,"$PWD/GMBasic/native/gmb_runtime_c/build" -o /tmp/gmb_fonts_selftest
 *   /tmp/gmb_fonts_selftest "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
 *
 * Exit 0 = tutte le invarianti rispettate; 1 = almeno un FAIL (stampato).
 */
#include "gmb_fonts.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static int g_ok = 0;

static void check_true(int cond, const char* label) {
    if (cond) {
        g_ok++;
    } else {
        g_fail++;
        printf("FAIL %s\n", label);
    }
}

int main(int argc, char** argv) {
    const char* path;
    static unsigned char buf[65536];
    static unsigned char buf2[65536];
    double m[8];
    int w = 0, h = 0, xo = 0, yo = 0, rc, i, n;
    int aw = 0, ah = 0;   /* dimensioni della bitmap di 'A' (m[] viene riusato dopo) */
    long ink = 0, diff = 0;
    int codes[16];

    if (argc < 2) {
        printf("uso: %s <font.ttf|font.otf>\n", argv[0]);
        return 1;
    }
    path = argv[1];

    /* 1. metriche di un glifo presente */
    memset(m, 0, sizeof m);
    rc = gmb_fonts_tt_metrics(path, 16.0, 'A', m);
    check_true(rc == 0, "metriche rc==0");
    check_true(m[0] > 0.0 && m[1] > 0.0, "metriche w/h > 0");
    check_true(m[4] > 0.0, "advance > 0");
    check_true(m[5] > 0.0, "ascent > 0");
    check_true(m[6] <= 0.0, "descent <= 0");

    /* 2. render: bitmap non vuota e deterministica */
    rc = gmb_fonts_tt_render(path, 16.0, 'A', buf, (int)sizeof buf, &w, &h, &xo, &yo);
    check_true(rc == 0, "render rc==0");
    check_true(w > 0 && h > 0, "render w/h > 0");
    check_true(w == (int)m[0] && h == (int)m[1], "render coerente con le metriche");
    for (i = 0; i < w * h; i++) if (buf[i] > 127) ink++;
    aw = w;
    ah = h;
    check_true(ink > 0, "render ha inchiostro");
    memset(buf2, 0, sizeof buf2);
    rc = gmb_fonts_tt_render(path, 16.0, 'A', buf2, (int)sizeof buf2, &w, &h, &xo, &yo);
    for (i = 0; i < w * h; i++) if (buf[i] != buf2[i]) diff++;
    check_true(diff == 0, "render deterministico");
    check_true(xo == (int)m[2] && yo == (int)m[3], "offset coerenti con le metriche");

    /* 3. glifo non coperto: nessun errore, bitmap vuota */
    memset(m, 0, sizeof m);
    rc = gmb_fonts_tt_metrics(path, 16.0, 0x10FFFF, m);
    check_true(rc == 0, "codepoint assente: rc==0");
    check_true(m[0] == 0.0 && m[1] == 0.0, "codepoint assente: metriche a zero");

    /* 4. cmap: conteggio e codici ordinati */
    n = gmb_fonts_tt_glyph_count(path);
    check_true(n > 0, "cmap non vuoto");
    memset(codes, 0, sizeof codes);
    {
        int got = gmb_fonts_tt_glyph_codes(path, codes, 16);
        check_true(got == n, "glyph_codes ritorna il totale");
        check_true(codes[0] > 0, "primo codepoint valido");
        check_true(codes[0] < codes[1] && codes[1] < codes[2], "codici in ordine crescente");
        /* buffer piccolo: ritorna la capacita' necessaria, senza overflow */
        check_true(gmb_fonts_tt_glyph_codes(path, codes, 1) == n, "buffer piccolo segnala la capacita'");
    }

    /* 5. errori espliciti, mai falsi successi */
    check_true(gmb_fonts_tt_metrics("/tmp/gmb_fonts_non_esiste.ttf", 16.0, 'A', m) == -1, "file assente: -1");
    check_true(gmb_fonts_tt_glyph_count("/tmp/gmb_fonts_non_esiste.ttf") == -1, "file assente: count -1");
    check_true(gmb_fonts_tt_metrics(path, 0.0, 'A', m) == -3, "dimensione 0: -3");
    rc = gmb_fonts_tt_render(path, 16.0, 'A', buf, 4, &w, &h, &xo, &yo);
    check_true(rc == -4, "buffer troppo piccolo: -4");

    printf("font      : %s\n", path);
    printf("controlli : %d ok, %d falliti\n", g_ok, g_fail);
    printf("glifi cmap: %d | bitmap 'A' %dx%d ink=%ld\n", n, aw, ah, ink);
    if (g_fail == 0) printf("GMB-FONTS-SELFTEST-OK\n");
    return g_fail == 0 ? 0 : 1;
}
