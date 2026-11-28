/* ============================================================================
 * gmb_peek_selfcheck.c — P7 blocco 5: PEEK/POKE su memoria virtuale del programma.
 *
 * Verifica il contratto di include/gmb_peek.h: area da 64 KB, byte 0..255 (POKE maschera),
 * prima e ultima cella indirizzabili, indirizzi fuori intervallo RIFIUTATI (-1) invece di
 * clamp silenziosi, fill che tocca tutta l'area.
 *
 * Eseguito da: ctest -R gmb_peek_selfcheck
 * ============================================================================ */
#include "gmb_peek.h"

#include <stdio.h>

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

int main(void)
{
    CHECK(gmb_peek_size() == GMB_PEEK_SIZE, "l'area è di 64 KB");
    CHECK(gmb_peek_size() == 65536, "64 KB = 65536 byte");

    /* ---- 1. stato iniziale azzerato, estremi indirizzabili ---- */
    CHECK(gmb_peek(0) == 0, "primo byte a 0");
    CHECK(gmb_peek(GMB_PEEK_SIZE - 1) == 0, "ultimo byte a 0");

    /* ---- 2. POKE/ PEEK round-trip e mascheramento ---- */
    CHECK(gmb_poke(0x1234, 0x7F) == 0, "POKE in mezzo all'area");
    CHECK(gmb_peek(0x1234) == 0x7F, "PEEK restituisce il byte scritto");
    CHECK(gmb_poke(0x1234, 300) == 0, "POKE accetta valori oltre 255");
    CHECK(gmb_peek(0x1234) == 44, "il valore è mascherato a 0..255 (300 & 0xFF)");
    CHECK(gmb_poke(0x1234, -1) == 0, "POKE di un negativo");
    CHECK(gmb_peek(0x1234) == 255, "-1 vale 0xFF");
    CHECK(gmb_peek(0x1235) == 0, "la cella accanto non è toccata");

    /* ---- 3. estremi ---- */
    CHECK(gmb_poke(0, 1) == 0 && gmb_peek(0) == 1, "indirizzo 0 scrivibile");
    CHECK(gmb_poke(GMB_PEEK_SIZE - 1, 2) == 0 && gmb_peek(GMB_PEEK_SIZE - 1) == 2,
          "ultimo indirizzo scrivibile");
    CHECK(gmb_poke(GMB_PEEK_SIZE, 3) == -1, "indirizzo 65536 rifiutato");
    CHECK(gmb_peek(GMB_PEEK_SIZE) == -1, "PEEK oltre l'area è errore, non 0");
    CHECK(gmb_peek(0xFFFFFFFFu) == -1, "indirizzo massimo rifiutato");
    CHECK(gmb_poke(0xFFFFFFFFu, 1) == -1, "POKE su indirizzo massimo rifiutato");

    /* ---- 4. fill ---- */
    CHECK(gmb_peek_fill(0xAB) == 0, "fill eseguito");
    CHECK(gmb_peek(0) == 0xAB, "fill sul primo byte");
    CHECK(gmb_peek(0x8000) == 0xAB, "fill in mezzo");
    CHECK(gmb_peek(GMB_PEEK_SIZE - 1) == 0xAB, "fill sull'ultimo byte");
    CHECK(gmb_peek_fill(-1) == 0, "fill con valore mascherato");
    CHECK(gmb_peek(123) == 0xFF, "-1 in fill vale 0xFF");

    printf("P7-PEEK ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
