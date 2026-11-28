/* ============================================================================
 * gmb_gfx_sprite_path_selfcheck.c — P7: MOVETO a tick e PATH degli sprite.
 *
 * Ogni check e' una regola del contratto di `gmb_gfx_sprite_moveto` /
 * `gmb_gfx_sprite_path` (include/gmb_graphics.h): un passo per frame, unita' dei comandi
 * (px per frame per k frame), comandi istantanei, ripetizioni annidate, R, durata di
 * default, eventi, limite di 256 comandi istantanei per frame, errori che non toccano il
 * path in corso, MOVETO sostituito da una nuova posizione, path/MOVETO ignorati sui membri
 * di un gruppo.
 *
 * Niente schermo, niente SDL: sprite 4x4 registrati in memoria, tempo avanzato a mano con
 * `gmb_gfx_sprites_tick` (deterministico; `gmb_gfx_sync` fa lo stesso passo di un frame).
 *
 * Eseguito da: ctest -R gmb_gfx_sprite_path_selfcheck
 * ============================================================================ */
#include "gmb_graphics.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

static int near(double a, double b) { return fabs(a - b) < 0.01; }

static int make(int id)
{
    int px[16];
    for (int i = 0; i < 16; i++) px[i] = 0xFFFFFF;
    return gmb_gfx_sprite_def(id, 4, 4, px);
}

int main(void)
{
    for (int i = 1; i <= 5; i++) CHECK(make(i) == i, "sprite registrato");

    /* ---- 1. X+n*k: n px PER FRAME per k frame ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "X+3*20") == 0, "path X+3*20 accettato");
    CHECK(gmb_gfx_sprite_path_active(1) == -1, "path attivo = -1 (contratto del riferimento)");
    CHECK(gmb_gfx_sprites_tick(1) == 1, "un tick");
    CHECK(near(gmb_gfx_sprite_x(1), 3), "un frame = 3 px");
    gmb_gfx_sprites_tick(4);
    CHECK(near(gmb_gfx_sprite_x(1), 15), "cinque frame = 15 px");
    gmb_gfx_sprites_tick(15);
    CHECK(near(gmb_gfx_sprite_x(1), 60), "venti frame = 60 px");
    CHECK(gmb_gfx_sprite_path_active(1) == 0, "path finito = 0");

    /* ---- 2. "": ferma il path lasciando lo sprite dov'e' ---- */
    CHECK(gmb_gfx_sprite_path(1, "X+1*100") == 0, "nuovo path");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 61), "un passo del nuovo path");
    CHECK(gmb_gfx_sprite_path(1, "") == 0, "stringa vuota ferma il path");
    CHECK(gmb_gfx_sprite_path_active(1) == 0, "path fermato");
    gmb_gfx_sprites_tick(3);
    CHECK(near(gmb_gfx_sprite_x(1), 61), "fermo: la posizione non cambia");

    /* ---- 3. X=n / Y=n: istantanei ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "X=10 Y=5") == 0, "coordinate assolute");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 10) && near(gmb_gfx_sprite_y(1), 5), "impostate subito, in un frame");
    CHECK(gmb_gfx_sprite_path_active(1) == 0, "finito nello stesso frame");

    /* ---- 4. W n: n frame fermo ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "W 3 X+1") == 0, "W 3");
    gmb_gfx_sprites_tick(3);
    CHECK(near(gmb_gfx_sprite_x(1), 0), "tre frame fermo");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 1), "al quarto frame parte il movimento");

    /* ---- 5. T n: durata di default dei movimenti successivi ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "T 5 X+1") == 0, "T 5");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 1), "T 5: 1 px al primo frame");
    gmb_gfx_sprites_tick(4);
    CHECK(near(gmb_gfx_sprite_x(1), 5), "T 5: 5 px in cinque frame");
    CHECK(gmb_gfx_sprite_path_active(1) == 0, "T 5 finito");

    /* ---- 6. M dx,dy*k ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "M 2,-1*3") == 0, "M 2,-1*3");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 2) && near(gmb_gfx_sprite_y(1), -1), "M: primo passo (2,-1)");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 6) && near(gmb_gfx_sprite_y(1), -3), "M: tre passi (6,-3)");

    /* ---- 7. ripetizioni, anche annidate ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "[ X+1 ]3") == 0, "[ X+1 ]3");
    gmb_gfx_sprites_tick(3);
    CHECK(near(gmb_gfx_sprite_x(1), 3), "il blocco e' eseguito 3 volte, non 4");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 3), "finita la ripetizione non prosegue");

    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "[ [ X+1 ]2 ]2") == 0, "annidamento 2 livelli");
    gmb_gfx_sprites_tick(4);
    CHECK(near(gmb_gfx_sprite_x(1), 4), "2 x 2 = 4 passi");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 4), "nessun passo in piu'");

    /* ---- 8. R: riparte da capo ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "X+1 R") == 0, "R");
    gmb_gfx_sprites_tick(3);
    CHECK(near(gmb_gfx_sprite_x(1), 3), "un px a ogni frame, per sempre");
    CHECK(gmb_gfx_sprite_path_active(1) == -1, "con R il path non finisce");

    /* ---- 9. eventi ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "E 7 W 1") == 0, "emissione evento");
    gmb_gfx_sprites_tick(1);
    CHECK(gmb_gfx_sprite_path_events() == 1, "un evento in coda");
    {
        int sp = -1;
        CHECK(gmb_gfx_sprite_path_event(&sp) == 7, "valore dell'evento");
        CHECK(sp == 1, "sprite dell'evento");
    }
    CHECK(gmb_gfx_sprite_path_events() == 0, "coda svuotata");
    CHECK(gmb_gfx_sprite_path_event(NULL) == -1, "nessun evento: -1");

    /* anello da 256: il 257esimo si scarta, non sovrascrive il primo */
    gmb_gfx_sprite_set_position(2, 0, 0);
    {
        char prog[2048];
        size_t off = 0;
        for (int i = 0; i < 300 && off < sizeof(prog); i++) off += (size_t)snprintf(prog + off, sizeof(prog) - off, "E 1 ");
        CHECK(gmb_gfx_sprite_path(2, prog) == 0, "300 eventi istantanei");
    }
    gmb_gfx_sprites_tick(2);
    CHECK(gmb_gfx_sprite_path_events() == 256, "l'anello si ferma a 256 eventi");
    {
        int drained = 0;
        while (gmb_gfx_sprite_path_event(NULL) >= 0) drained++;
        CHECK(drained == 256, "tutti prelevabili");
    }

    /* ---- 10. errori: il path precedente resta intatto ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "X+1*10") == 0, "path valido in corso");
    gmb_gfx_sprites_tick(1);
    CHECK(gmb_gfx_sprite_path(1, "Z 1") == -1, "comando sconosciuto rifiutato");
    CHECK(gmb_gfx_sprite_path_error()[0] != '\0', "il messaggio dice perche'");
    CHECK(gmb_gfx_sprite_path_active(1) == -1, "il path precedente e' ancora attivo");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 2), "il path precedente continua");
    CHECK(gmb_gfx_sprite_path(1, "A 1") == -1, "A (sequenze immagini) non implementato: errore esplicito");
    CHECK(gmb_gfx_sprite_path(1, "[ X+1") == -1, "'[' non chiusa rifiutata");
    CHECK(gmb_gfx_sprite_path(1, "[[[[[ X+1 ]1 ]1 ]1 ]1 ]1") == -1, "oltre 4 livelli di annidamento");
    CHECK(gmb_gfx_sprite_path(1, "X") == -1, "X senza operatore");
    CHECK(gmb_gfx_sprite_path(1, "M 1") == -1, "M senza coordinata");
    {
        char* prog = (char*)malloc(40000);
        size_t off = 0;
        for (int i = 0; i < 5000 && off < 40000; i++) off += (size_t)snprintf(prog + off, 40000 - off, "X+1 ");
        CHECK(gmb_gfx_sprite_path(1, prog) == -1, "oltre 4096 comandi rifiutato");
        free(prog);
    }
    CHECK(gmb_gfx_sprite_path(9999, "X+1") == -1, "id inesistente rifiutato");

    /* ---- 11. limite di 256 comandi istantanei per frame ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    {
        char prog[4096];
        size_t off = 0;
        for (int i = 0; i < 300; i++) off += (size_t)snprintf(prog + off, sizeof(prog) - off, "X=1 ");
        CHECK(gmb_gfx_sprite_path(1, prog) == 0, "300 comandi istantanei");
    }
    gmb_gfx_sprites_tick(1);
    CHECK(gmb_gfx_sprite_path_active(1) == -1, "dopo un frame il path non e' finito (limite 256)");
    gmb_gfx_sprites_tick(1);
    CHECK(gmb_gfx_sprite_path_active(1) == 0, "il resto al frame dopo: nessun frame bloccato");

    /* ---- 12. MOVETO ---- */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_moveto(1, 8, 0, 4) == 0, "MOVETO 8,0,4");
    CHECK(gmb_gfx_sprite_moving(1) == 1, "in movimento");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 2), "primo passo = 2 px");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 6), "tre passi = 6 px");
    gmb_gfx_sprites_tick(1);
    CHECK(near(gmb_gfx_sprite_x(1), 8) && gmb_gfx_sprite_moving(1) == 0, "arrivo esatto, movimento finito");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 8), "finito il MOVETO non si muove piu'");

    /* una posizione scritta dal programma sostituisce il MOVETO */
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_moveto(1, 100, 0, 10) == 0, "MOVETO lungo");
    gmb_gfx_sprites_tick(1);
    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_moving(1) == 0, "una nuova posizione annulla il movimento");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 0), "niente passi dopo l'annullamento");

    /* ticks <= 0 = posizione subito */
    CHECK(gmb_gfx_sprite_moveto(1, 5, 6, 0) == 0, "MOVETO 0 tick");
    CHECK(near(gmb_gfx_sprite_x(1), 5) && near(gmb_gfx_sprite_y(1), 6), "posizione immediata");
    CHECK(gmb_gfx_sprite_moving(1) == 0, "non e' in movimento");
    CHECK(gmb_gfx_sprite_moveto(9999, 1, 1, 2) == -1, "MOVETO su id inesistente rifiutato");

    /* ---- 13. gruppi: la testa comanda, il membro e' ignorato ---- */
    gmb_gfx_sprite_set_position(2, 0, 0);
    CHECK(gmb_gfx_sprite_set_size(2, 2, 2) == 0, "gruppo 2x2 sulla testa 2");
    CHECK(gmb_gfx_sprite_path(3, "X+1*5") == 0, "path su un membro: accettato ma ignorato");
    CHECK(gmb_gfx_sprite_path_active(3) == 0, "il membro non ha un path attivo");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(3), 4), "il membro non si e' mosso da solo (resta a una cella dalla testa)");
    CHECK(gmb_gfx_sprite_moveto(3, 50, 50, 2) == 0, "MOVETO su un membro: ignorato");
    CHECK(gmb_gfx_sprite_moving(3) == 0, "il membro non si muove");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(2), 0) && near(gmb_gfx_sprite_x(3), 4), "il blocco e' rimasto fermo");

    CHECK(gmb_gfx_sprite_path(2, "X+3*2") == 0, "path sulla testa del gruppo");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(2), 6), "la testa si e' mossa di 6 px");
    CHECK(near(gmb_gfx_sprite_x(3), 10), "il membro ha seguito la testa");
    CHECK(gmb_gfx_sprite_moveto(2, 0, 0, 2) == 0, "MOVETO sulla testa");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(2), 0) && near(gmb_gfx_sprite_x(3), 4), "MOVETO sulla testa: blocco intero");

    /* ---- 14. tick invalidi ---- */
    CHECK(gmb_gfx_sprites_tick(0) == 0, "tick 0 = nessun frame");
    CHECK(gmb_gfx_sprites_tick(-3) == 0, "tick negativo = nessun frame");

    printf("P7-SPRITE-PATH ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
