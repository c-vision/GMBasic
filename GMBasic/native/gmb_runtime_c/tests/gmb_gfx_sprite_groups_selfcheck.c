/* ============================================================================
 * gmb_gfx_sprite_groups_selfcheck.c — P7: gruppi di sprite (SPRITE n, SIZE w, h).
 *
 * Ogni check e' una regola del contratto di `gmb_gfx_sprite_set_size`
 * (include/gmb_graphics.h): la testa comanda posizione/ON-OFF/priorita'/rotazione/scala,
 * i membri sono disposti in griglia e le loro impostazioni sono ignorate, lo scioglimento
 * (SIZE 1,1) le ripristina, il blocco si disegna e collide come un oggetto unico, i membri
 * di uno stesso gruppo non collidono tra loro e le collisioni riferiscono la testa.
 *
 * Niente schermo e niente SDL: texture 4x4 registrate in memoria con `gmb_gfx_sprite_def`
 * (0 = trasparente, altrimenti opaco) e canvas offscreen per verificare il disegno.
 *
 * Eseguito da: ctest -R gmb_gfx_sprite_groups_selfcheck
 * ============================================================================ */
#include "gmb_graphics.h"

#include <math.h>
#include <stdio.h>

static int g_fails = 0;
static int g_checks = 0;
static void check(int cond, const char* what, int line)
{
    g_checks++;
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}
#define CHECK(c, w) check((c) != 0, (w), __LINE__)

static int near(double a, double b) { return fabs(a - b) < 0.01; }

/* Sprite 4x4 opaco di un colore. */
static int make(int id, int rgb)
{
    int px[16];
    for (int i = 0; i < 16; i++) px[i] = rgb;
    return gmb_gfx_sprite_def(id, 4, 4, px);
}

int main(void)
{
    int cvs;

    CHECK(make(1, 0xFF0000) == 1, "sprite 1 registrato (testa, rosso)");
    CHECK(make(2, 0x00FF00) == 2, "sprite 2 registrato (membro, verde)");
    CHECK(make(3, 0x0000FF) == 3, "sprite 3 registrato (membro, blu)");
    CHECK(make(4, 0xFFFF00) == 4, "sprite 4 registrato (membro, giallo)");
    for (int i = 5; i <= 12; i++) CHECK(make(i, 0x808080 + i) == i, "sprite di servizio registrato");
    CHECK(gmb_gfx_sprite_count() == 12, "dodici sprite vivi");
    /* sprite dedicati alla matrice della testa (fuori da tutti i blocchi precedenti) */
    CHECK(make(13, 0x00FFFF) == 13, "sprite 13 registrato (testa della matrice)");
    CHECK(make(14, 0xFF00FF) == 14, "sprite 14 registrato (membro a destra)");
    CHECK(make(15, 0xFFFFFF) == 15, "sprite 15 registrato (membro sotto)");
    CHECK(make(16, 0x101010) == 16, "sprite 16 registrato (membro in diagonale)");

    /* ---- 1. creazione e layout rigido ---- */
    gmb_gfx_sprite_set_position(1, 8, 8);
    gmb_gfx_sprite_set_position(2, 40, 40);       /* posizione propria, da ripristinare */
    CHECK(gmb_gfx_sprite_set_size(1, 2, 2) == 0, "SIZE 2,2 sulla testa");
    CHECK(gmb_gfx_sprite_cols(1) == 2 && gmb_gfx_sprite_rows(1) == 2, "cols/rows del blocco");
    CHECK(gmb_gfx_sprite_group_head(1) == 1, "la testa appartiene al gruppo (se stessa)");
    CHECK(gmb_gfx_sprite_group_head(2) == 1, "il membro 2 riferisce la testa 1");
    CHECK(gmb_gfx_sprite_group_head(5) == 0, "uno sprite fuori dal gruppo non ha testa");
    CHECK(near(gmb_gfx_sprite_x(2), 12) && near(gmb_gfx_sprite_y(2), 8), "membro 2 a una cella a destra");
    CHECK(near(gmb_gfx_sprite_x(3), 8) && near(gmb_gfx_sprite_y(3), 12), "membro 3 alla riga sotto");
    CHECK(near(gmb_gfx_sprite_x(4), 12) && near(gmb_gfx_sprite_y(4), 12), "membro 4 in diagonale");

    /* ---- 2. le impostazioni dei membri sono ignorate ---- */
    gmb_gfx_sprite_set_position(2, 100, 100);
    CHECK(near(gmb_gfx_sprite_x(2), 12) && near(gmb_gfx_sprite_y(2), 8), "posizione del membro ignorata");
    gmb_gfx_sprite_set_visible(2, 0);
    CHECK(gmb_gfx_sprite_visible(2) == 1, "ON/OFF del membro ignorato (vale la testa)");
    gmb_gfx_sprite_set_priority(2, 9);
    CHECK(gmb_gfx_sprite_priority(2) == 0, "priorita' del membro ignorata");
    gmb_gfx_sprite_set_rotation(2, 45);
    CHECK(near(gmb_gfx_sprite_x(2), 12), "rotazione del membro ignorata (nessuna ridisposizione)");

    /* ---- 3. la testa comanda: posizione, ON/OFF, priorita' ---- */
    gmb_gfx_sprite_set_position(1, 20, 20);
    CHECK(near(gmb_gfx_sprite_x(2), 24) && near(gmb_gfx_sprite_y(2), 20), "spostare la testa sposta il blocco");
    CHECK(near(gmb_gfx_sprite_x(4), 24) && near(gmb_gfx_sprite_y(4), 24), "tutti i membri seguono");
    gmb_gfx_sprite_set_visible(1, 0);
    CHECK(gmb_gfx_sprite_visible(3) == 0, "SPRITE OFF sulla testa spegne il blocco");
    gmb_gfx_sprite_set_visible(1, 1);
    CHECK(gmb_gfx_sprite_visible(3) == 1, "SPRITE ON sulla testa riaccende il blocco");
    gmb_gfx_sprite_set_priority(1, 5);
    CHECK(gmb_gfx_sprite_priority(4) == 5, "priorita' della testa propagata ai membri");
    gmb_gfx_sprite_set_priority(1, 0);

    /* ---- 4. rotazione: il blocco e' un corpo rigido ---- */
    gmb_gfx_sprite_set_position(1, 8, 8);
    gmb_gfx_sprite_set_rotation(1, 90);
    CHECK(near(gmb_gfx_sprite_x(2), 8) && near(gmb_gfx_sprite_y(2), 12),
          "rotazione 90°: il membro a destra finisce sotto");
    gmb_gfx_sprite_set_rotation(1, 0);
    CHECK(near(gmb_gfx_sprite_x(2), 12) && near(gmb_gfx_sprite_y(2), 8), "rotazione 0: layout ripristinato");

    /* ---- 5. argomenti non validi e conflitti fra gruppi ---- */
    CHECK(gmb_gfx_sprite_set_size(1, 0, 2) == -1, "w = 0 rifiutato");
    CHECK(gmb_gfx_sprite_set_size(1, 9, 2) == -1, "w > 8 rifiutato");
    CHECK(gmb_gfx_sprite_set_size(2, 2, 2) == -1, "un membro non puo' diventare testa");
    CHECK(gmb_gfx_sprite_set_size(1, 8, 8) == 0, "8x8 = 64 sprite ammesso");
    CHECK(gmb_gfx_sprite_set_size(1, 1, 1) == 0, "SIZE 1,1 scioglie");
    CHECK(gmb_gfx_sprite_set_size(0, 2, 2) == -1, "testa 0 (inesistente) rifiutata");
    CHECK(gmb_gfx_sprite_set_size(9999, 2, 2) == -1, "testa fuori intervallo rifiutata");
    CHECK(gmb_gfx_sprite_set_size(5, 2, 2) == 0, "gruppo 2x2 sugli sprite 5..8");
    CHECK(gmb_gfx_sprite_set_size(1, 5, 1) == -1, "blocco che invade un altro gruppo rifiutato");

    /* ---- 6. ridimensionamento e scioglimento ripristinano i membri ---- */
    gmb_gfx_sprite_set_size(1, 2, 2);
    gmb_gfx_sprite_set_size(1, 2, 1);
    CHECK(gmb_gfx_sprite_cols(1) == 2 && gmb_gfx_sprite_rows(1) == 1, "SIZE 2,1 ridimensiona il blocco");
    CHECK(gmb_gfx_sprite_group_head(3) == 0, "il membro uscito dal blocco torna libero");
    CHECK(gmb_gfx_sprite_group_head(2) == 1, "il membro rimasto resta nel blocco");
    gmb_gfx_sprite_set_position(10, 40, 40);
    gmb_gfx_sprite_set_position(11, 40, 40);
    CHECK(gmb_gfx_sprite_set_size(10, 2, 2) == 0, "secondo gruppo 2x2 sugli sprite 10..13");
    CHECK(near(gmb_gfx_sprite_x(11), 44), "membro 11 disposto dalla testa 10");
    CHECK(gmb_gfx_sprite_set_size(10, 1, 1) == 0, "SIZE 1,1 scioglie il secondo gruppo");
    CHECK(gmb_gfx_sprite_group_head(11) == 0, "membro 11 non appartiene piu' al gruppo");
    CHECK(near(gmb_gfx_sprite_x(11), 40) && near(gmb_gfx_sprite_y(11), 40), "posizione propria ripristinata");
    CHECK(gmb_gfx_sprite_cols(10) == 1 && gmb_gfx_sprite_rows(10) == 1, "cols/rows tornano 1");
    CHECK(gmb_gfx_sprite_set_size(1, 1, 1) == 0, "sciolto anche il primo gruppo");
    CHECK(near(gmb_gfx_sprite_x(2), 40) && near(gmb_gfx_sprite_y(2), 40), "sprite 2 torna dov'era prima del gruppo");

    /* ---- 7. collisioni: un blocco e' un oggetto solo ---- */
    gmb_gfx_sprite_set_size(1, 2, 2);
    gmb_gfx_sprite_set_position(1, 8, 8);
    gmb_gfx_sprite_set_position(2, 8, 8);        /* ignorata: vale il layout */
    gmb_gfx_sprite_set_position(9, 12, 8);       /* sopra il membro 2 */
    CHECK(gmb_gfx_collide(1, 9) == -1, "AABB: il blocco collide con uno sprite sul membro 2");
    CHECK(gmb_gfx_collide(9, 1) == -1, "AABB: simmetrico");
    CHECK(gmb_gfx_collide_pixels(1, 9) == -1, "per-pixel: pixel del membro 2 contro sprite 9");
    CHECK(gmb_gfx_collide(1, 2) == 0, "membri dello stesso gruppo: nessuna collisione (1 contro 2)");
    CHECK(gmb_gfx_collide(2, 4) == 0, "membri dello stesso gruppo: 2 contro 4");
    CHECK(gmb_gfx_collide_pixels(1, 3) == 0, "per-pixel: testa contro proprio membro = 0");
    CHECK(gmb_gfx_collide(1, 1) == 0, "una testa non collide con se stessa (stesso oggetto)");

    /* due gruppi che si sovrappongono: collisione fra blocchi, riferita alle teste */
    CHECK(gmb_gfx_sprite_set_size(5, 2, 2) == 0, "secondo gruppo 2x2 sugli sprite 5..8");
    gmb_gfx_sprite_set_position(5, 8, 8);
    CHECK(gmb_gfx_collide(1, 5) == -1, "blocco contro blocco: collisione");
    CHECK(gmb_gfx_collide_pixels(1, 5) == -1, "blocco contro blocco: pixel in comune");
    CHECK(gmb_gfx_collide(1, 6) == -1, "la collisione con un membro si riporta alla sua testa");
    gmb_gfx_sprite_set_position(5, 200, 200);
    CHECK(gmb_gfx_collide(1, 5) == 0, "blocchi lontani: nessuna collisione");
    CHECK(near(gmb_gfx_sprite_x(6), 204) && near(gmb_gfx_sprite_y(6), 200), "il blocco 5 si e' mosso intero");

    /* ---- 8. MOVETO e PATH sulla testa muovono il blocco ---- */
    gmb_gfx_sprite_set_position(1, 8, 8);
    CHECK(gmb_gfx_sprite_moveto(1, 18, 8, 4) == 0, "MOVETO sulla testa");
    CHECK(gmb_gfx_sprite_moving(1) == 1, "in movimento");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 13) && near(gmb_gfx_sprite_y(1), 8), "a meta' strada la testa e' a 13");
    CHECK(near(gmb_gfx_sprite_x(2), 17), "il membro 2 segue la testa durante il MOVETO");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 18) && gmb_gfx_sprite_moving(1) == 0, "arrivo esatto e movimento finito");
    CHECK(near(gmb_gfx_sprite_x(2), 22), "il membro 2 e' ancora a una cella dalla testa");

    gmb_gfx_sprite_set_position(1, 0, 0);
    CHECK(gmb_gfx_sprite_path(1, "X+2*2") == 0, "PATH sulla testa");
    gmb_gfx_sprites_tick(2);
    CHECK(near(gmb_gfx_sprite_x(1), 4), "il path ha spostato la testa di 4 px");
    CHECK(near(gmb_gfx_sprite_x(2), 8) && near(gmb_gfx_sprite_y(2), 0), "il membro ha seguito il path della testa");

    /* ---- 9. disegno: la testa disegna tutto il blocco ---- */
    gmb_gfx_sprite_set_size(1, 2, 2);
    gmb_gfx_sprite_set_position(1, 8, 8);
    cvs = gmb_gfx_canvas_new(16, 16);
    CHECK(cvs > 0, "canvas offscreen creato");
    CHECK(gmb_gfx_canvas_use(cvs) == 0, "canvas attivo");
    gmb_gfx_sprite_draw(1);
    CHECK((gmb_gfx_canvas_get(cvs, 8, 8) & 0xFFFFFF) == 0xFF0000, "pixel della testa (rosso)");
    CHECK((gmb_gfx_canvas_get(cvs, 12, 8) & 0xFFFFFF) == 0x00FF00, "pixel del membro 2 (verde)");
    CHECK((gmb_gfx_canvas_get(cvs, 8, 12) & 0xFFFFFF) == 0x0000FF, "pixel del membro 3 (blu)");
    CHECK((gmb_gfx_canvas_get(cvs, 12, 12) & 0xFFFFFF) == 0xFFFF00, "pixel del membro 4 (giallo)");
    /* disegnare un membro da solo non produce nulla: lo disegna la testa */
    {
        int before = gmb_gfx_canvas_get(cvs, 12, 8);
        gmb_gfx_sprite_draw(2);
        CHECK(gmb_gfx_canvas_get(cvs, 12, 8) == before, "disegnare un membro non ridisegna nulla");
    }
    gmb_gfx_sprite_set_visible(1, 0);
    gmb_gfx_sprite_draw(1);
    CHECK((gmb_gfx_canvas_get(cvs, 12, 8) & 0xFFFFFF) == 0x00FF00, "SPRITE OFF non cancella i pixel gia' disegnati");
    gmb_gfx_canvas_use(0);

    /* ---- 10. matrice della testa: stessa decomposizione del disegno ----
     * `gmb_gfx_sprite_set_matrix` non e' un'impostazione a se': la matrice si decompone in
     * rotazione+scala (gmb_gfx_sprite_rot_scale), quindi una matrice sulla testa dispone il
     * blocco ESATTAMENTE come farebbe la rotazione/scala equivalente — e la testa viene
     * disegnata con la stessa trasformazione. */
    {
        const double rot90[6] = { 0, 1, -1, 0, 0, 0 };   /* rotazione 90°, scala 1 */
        const double scale2[6] = { 2, 0, 0, 2, 0, 0 };   /* scala 2, nessuna rotazione */
        gmb_gfx_sprite_set_position(13, 8, 8);
        CHECK(gmb_gfx_sprite_set_size(13, 2, 2) == 0, "gruppo 2x2 sulla testa 13");
        CHECK(near(gmb_gfx_sprite_x(14), 12) && near(gmb_gfx_sprite_y(14), 8),
              "senza matrice: griglia dritta");
        gmb_gfx_sprite_set_matrix(13, rot90);
        CHECK(near(gmb_gfx_sprite_x(14), 8) && near(gmb_gfx_sprite_y(14), 12),
              "matrice 90°: il membro a destra finisce sotto");
        CHECK(near(gmb_gfx_sprite_x(15), 4) && near(gmb_gfx_sprite_y(15), 8),
              "matrice 90°: la riga sotto finisce a sinistra");
        gmb_gfx_sprite_set_matrix(13, scale2);
        CHECK(near(gmb_gfx_sprite_x(14), 16) && near(gmb_gfx_sprite_y(14), 8),
              "matrice con scala 2: la cella raddoppia");
        CHECK(near(gmb_gfx_sprite_x(16), 16) && near(gmb_gfx_sprite_y(16), 16),
              "matrice con scala 2: anche la diagonale raddoppia");
        /* membro: la matrice e' ignorata come rotazione/scala (comanda la testa) */
        gmb_gfx_sprite_set_matrix(14, rot90);
        CHECK(near(gmb_gfx_sprite_x(14), 16) && near(gmb_gfx_sprite_y(14), 8),
              "matrice di un membro ignorata");
        /* la testa disegna il blocco con la matrice, non con la vecchia rotazione/scala */
        gmb_gfx_sprite_set_matrix(13, rot90);
        cvs = gmb_gfx_canvas_new(16, 16);
        CHECK(cvs > 0, "canvas offscreen per la matrice");
        CHECK(gmb_gfx_canvas_use(cvs) == 0, "canvas attivo per la matrice");
        gmb_gfx_sprite_draw(13);
        CHECK((gmb_gfx_canvas_get(cvs, 8, 12) & 0xFFFFFF) == 0xFF00FF,
              "il membro 14 e' disegnato dove la matrice lo dispone");
        gmb_gfx_canvas_use(0);
        CHECK(gmb_gfx_sprite_set_size(13, 1, 1) == 0, "sciolto il gruppo della matrice");
    }

    printf("P7-SPRITE-GROUPS ok=%d fail=%d\n", g_checks - g_fails, g_fails);
    return g_fails == 0 ? 0 : 1;
}
