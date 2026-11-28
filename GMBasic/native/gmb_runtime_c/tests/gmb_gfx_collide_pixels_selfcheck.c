/* ============================================================================
 * gmb_gfx_collide_pixels_selfcheck.c — P7: collisione per pixel OPACO.
 *
 * Il difetto che chiude: `gmb_gfx_collide` e' solo AABB, quindi due sprite con buchi
 * trasparenti "collidono" dove non c'e' nessun pixel visibile. Il caso canonico e' due
 * sprite COMPLEMENTARI (meta' sinistra / meta' destra): rettangoli identici, zero pixel
 * in comune. Ogni check e' una regola del contratto di `gmb_gfx_collide_pixels`
 * (include/gmb_graphics.h): AABB invariato, variante per-pixel con la geometria del
 * renderer (centro, scala, rotazione, matrice) e la stessa soglia alpha.
 *
 * Niente schermo, niente SDL: le texture si registrano in memoria con `gmb_gfx_sprite_def`
 * (formato del contratto: 0 = trasparente, altrimenti opaco).
 *
 * Eseguito da: ctest -R gmb_gfx_collide_pixels_selfcheck
 * ============================================================================ */
#include "gmb_graphics.h"

#include <stdio.h>

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

/* Anello 8x8: cornice opaca di 2 px, buco 4x4 al centro (locali 2..5). */
static int make_ring(int id)
{
    int px[64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            px[y * 8 + x] = (x >= 2 && x <= 5 && y >= 2 && y <= 5) ? 0 : 0xFFFFFF;
    return gmb_gfx_sprite_def(id, 8, 8, px);
}

/* Blocco 4x4 interamente opaco (entra esatto nel buco dell'anello). */
static int make_block(int id)
{
    int px[16];
    for (int i = 0; i < 16; i++) px[i] = 0x00FF00;
    return gmb_gfx_sprite_def(id, 4, 4, px);
}

/* Meta' sprite: `half` 0 = colonne 0..3 opache, 1 = colonne 4..7 opache.
 * Due meta' opposte hanno lo STESSO rettangolo e nessun pixel visibile in comune. */
static int make_half(int id, int right)
{
    int px[64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            px[y * 8 + x] = (right ? (x >= 4) : (x <= 3)) ? 0xFFFFFF : 0;
    return gmb_gfx_sprite_def(id, 8, 8, px);
}

int main(void)
{
    int left, right, ring, block, off, matrix_ring;

    /* ---- 1. registrazione: la geometria e' quella del renderer ---- */
    left = make_half(1, 0);
    right = make_half(2, 1);
    ring = make_ring(3);
    block = make_block(4);
    off = make_ring(5);
    matrix_ring = make_ring(6);
    CHECK(left == 1 && right == 2 && ring == 3 && block == 4, "sprite registrati da array di pixel");
    CHECK(gmb_gfx_sprite_count() == 6, "sei sprite vivi");
    gmb_gfx_sprite_set_position(1, 16, 16);
    gmb_gfx_sprite_set_position(2, 16, 16);

    /* ---- 2. IL DIFETTO: rettangoli identici, pixel disgiunti ---- */
    CHECK(gmb_gfx_collide(1, 2) == -1, "AABB: le due meta' hanno lo stesso rettangolo (contratto invariato)");
    CHECK(gmb_gfx_collide_pixels(1, 2) == 0, "per-pixel: meta' sinistra contro destra NON collide");
    CHECK(gmb_gfx_collide_pixels(2, 1) == 0, "per-pixel: simmetrico");

    /* Scorrimento di 4 px: il rettangolo si sovrappone ancora, i pixel no. */
    gmb_gfx_sprite_set_position(2, 20, 16);
    CHECK(gmb_gfx_collide(1, 2) == -1, "AABB: +4 px sovrappone i rettangoli");
    CHECK(gmb_gfx_collide_pixels(1, 2) == 0, "per-pixel: +4 px, solo zona trasparente");

    /* Bordi adiacenti (left + width == right): non e' sovrapposizione. */
    gmb_gfx_sprite_set_position(2, 24, 16);
    CHECK(gmb_gfx_collide_pixels(1, 2) == 0, "per-pixel: bordi adiacenti non collidono");

    /* Scorrimento di 2 px: le meta' opache si incontrano davvero. */
    gmb_gfx_sprite_set_position(2, 14, 16);
    CHECK(gmb_gfx_collide_pixels(1, 2) == -1, "per-pixel: +2 px = pixel opachi in comune");
    CHECK(gmb_gfx_collide_pixels(2, 1) == -1, "per-pixel: simmetrico sulla sovrapposizione vera");

    /* Lontani: potatura dell'AABB, nessun pixel da guardare. */
    gmb_gfx_sprite_set_position(2, 200, 200);
    CHECK(gmb_gfx_collide(1, 2) == 0 && gmb_gfx_collide_pixels(1, 2) == 0, "lontani: 0 per entrambe");

    /* ---- 3. blocco 4x4 interamente dentro il buco dell'anello ---- */
    gmb_gfx_sprite_set_position(3, 16, 16);
    gmb_gfx_sprite_set_position(4, 16, 16);
    CHECK(gmb_gfx_collide(3, 4) == -1, "AABB: anello e blocco centrale si sovrappongono");
    CHECK(gmb_gfx_collide_pixels(3, 4) == 0, "per-pixel: blocco nel buco = nessuna collisione");
    CHECK(gmb_gfx_collide_pixels(4, 3) == 0, "per-pixel: blocco nel buco (ordine invertito)");

    /* Blocco spostato sulla cornice: collide, e qui l'AABB ha ragione. */
    gmb_gfx_sprite_set_position(4, 13, 16);
    CHECK(gmb_gfx_collide(3, 4) == -1, "AABB: blocco sulla cornice");
    CHECK(gmb_gfx_collide_pixels(3, 4) == -1, "per-pixel: blocco sulla cornice = collisione vera");

    /* ---- 4. SPRITE OFF e id invalidi (stesso contratto di collide) ---- */
    gmb_gfx_sprite_set_visible(4, 0);
    CHECK(gmb_gfx_collide_pixels(3, 4) == 0, "per-pixel: sprite spento non collide");
    gmb_gfx_sprite_set_visible(4, 1);
    CHECK(gmb_gfx_collide_pixels(0, 3) == 0, "per-pixel: id 0 non valido");
    CHECK(gmb_gfx_collide_pixels(3, 9999) == 0, "per-pixel: id fuori intervallo");
    CHECK(gmb_gfx_collide_pixels(3, 3) == -1, "per-pixel: sprite contro se stesso = -1");

    /* ---- 5. SCALA: il buco cresce con lo sprite (2x = buco 8x8) ---- */
    gmb_gfx_sprite_set_position(4, 16, 16);
    gmb_gfx_sprite_set_scale(3, 2, 2);
    CHECK(gmb_gfx_collide(3, 4) == -1, "AABB: anello 2x e blocco centrale");
    CHECK(gmb_gfx_collide_pixels(3, 4) == 0, "per-pixel: a 2x il buco 8x8 contiene il blocco");
    gmb_gfx_sprite_set_scale(3, 1, 1);

    /* Scala 0 = sprite invisibile per il renderer: nessun pixel, nessuna collisione. */
    gmb_gfx_sprite_set_scale(3, 0, 0);
    CHECK(gmb_gfx_collide_pixels(3, 4) == 0, "per-pixel: scala 0 non disegna nulla");
    gmb_gfx_sprite_set_scale(3, 1, 1);

    /* ---- 6. ROTAZIONE ---- */
    /* L'anello ruotato di 90° e' simmetrico: il buco resta al centro. */
    gmb_gfx_sprite_set_rotation(3, 90);
    CHECK(gmb_gfx_collide_pixels(3, 4) == 0, "per-pixel: anello ruotato 90°, buco ancora centrale");
    /* Un blocco OPACO ruotato sulla cornice collide (al centro cadrebbe nel buco). */
    gmb_gfx_sprite_set_position(4, 13, 13);
    gmb_gfx_sprite_set_rotation(4, 45);
    CHECK(gmb_gfx_collide_pixels(3, 4) == -1, "per-pixel: blocco ruotato 45° sulla cornice");
    gmb_gfx_sprite_set_rotation(4, 0);
    gmb_gfx_sprite_set_rotation(3, 0);

    /* ---- 7. MATRICE: stessa decomposizione rot+scala di gmb_gfx_sprite_draw ---- */
    {
        double m[6] = { 2, 0, 0, 2, 0, 0 };   /* scala 2x, nessuna rotazione */
        gmb_gfx_sprite_set_matrix(matrix_ring, m);
        gmb_gfx_sprite_set_position(matrix_ring, 16, 16);
        gmb_gfx_sprite_set_position(4, 16, 16);
        CHECK(gmb_gfx_collide_pixels(matrix_ring, 4) == 0,
              "per-pixel: matrice 2x = buco 8x8, blocco dentro");
    }

    /* Sprite non usati dal test restano vivi e non interferiscono. */
    CHECK(gmb_gfx_sprite_count() == 6, "nessuno sprite liberato per errore");

    printf("P7-COLLIDE-PIXELS ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
