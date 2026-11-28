/* ============================================================================
 * gmb_anim_selfcheck.c — motore di animazione tick-driven.
 *
 * La parte deterministica è `gmb_anim_advance(dt)`: qui si avanza a passi
 * espliciti (nessun tempo di parete) e si verificano valori, easing e durata.
 * Il tempo reale (`gmb_anim_tick`) resta una sola riga che chiama advance con
 * il delta del clock, quindi non va misurato in un test.
 * ============================================================================ */
#include "gmb_anim.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;
static void check(int cond, const char* msg)
{
    if (!cond) { printf("FAIL %s\n", msg); g_fails++; }
}

static int near(double a, double b, double eps)
{
    double d = a - b;
    if (d < 0) d = -d;
    return d <= eps;
}

int main(void)
{
    gmb_anim_clear();

    /* ---- parsing della spec ------------------------------------------------ */
    {
        int prop = -1, ms = -1, easing = -1; double to = 0;
        check(gmb_anim_parse_spec("left:300:400:out", &prop, &to, &ms, &easing) == 1, "parse left");
        check(prop == GMB_ANIM_LEFT, "parse: proprietà left");
        check(near(to, 300, 1e-9), "parse: target 300");
        check(ms == 400, "parse: durata 400");
        check(easing == GMB_ANIM_EASE_OUT, "parse: easing out");

        check(gmb_anim_parse_spec("width:80:250", &prop, &to, &ms, &easing) == 1, "parse width");
        check(prop == GMB_ANIM_WIDTH, "parse: proprietà width");
        check(easing == GMB_ANIM_EASE_LINEAR, "parse: easing linear (default)");

        check(gmb_anim_parse_spec("h:12:0:inout", &prop, &to, &ms, &easing) == 1, "parse alias h");
        check(prop == GMB_ANIM_HEIGHT && ms == 0, "parse alias h: height, ms 0");
        check(easing == GMB_ANIM_EASE_IN_OUT, "parse: easing inout");

        check(gmb_anim_parse_spec("bogus:1:2", &prop, &to, &ms, &easing) == 0, "parse: proprietà ignota rifiutata");
        check(gmb_anim_parse_spec("left:1", &prop, &to, &ms, &easing) == 0, "parse: campi mancanti rifiutati");
        check(gmb_anim_parse_spec(NULL, &prop, &to, &ms, &easing) == 0, "parse: spec NULL rifiutata");
    }

    /* ---- slot lineare ------------------------------------------------------
     * 0 → 100 in 400 ms: a metà strada vale 50 esatti. */
    {
        int i = gmb_anim_start(7, 1, GMB_ANIM_LEFT, 0.0, 100.0, 400, GMB_ANIM_EASE_LINEAR);
        check(i >= 0, "start: slot ottenuto");
        check(gmb_anim_count() == 1, "start: un'animazione attiva");

        check(gmb_anim_advance(0) == 1, "advance 0: ancora attiva");
        int h = -1, tui = -1, prop = -1, done = -1; double v = -1;
        check(gmb_anim_get(i, &h, &tui, &prop, &done, &v) == 1, "get: slot in uso");
        check(h == 7 && tui == 1 && prop == GMB_ANIM_LEFT, "get: handle/is_tui/proprietà");
        check(done == 0 && near(v, 0.0, 1e-9), "advance 0: valore di partenza");

        check(gmb_anim_advance(200) == 1, "advance 200: attiva");
        gmb_anim_get(i, &h, &tui, &prop, &done, &v);
        check(near(v, 50.0, 1e-9), "advance 200: metà lineare = 50");

        check(gmb_anim_advance(200) == 1, "advance 200 (fine): conclusa in questo passo");
        gmb_anim_get(i, &h, &tui, &prop, &done, &v);
        check(done == 1 && near(v, 100.0, 1e-9), "advance fine: valore = target, done");

        /* Il frame successivo libera lo slot già applicato. */
        check(gmb_anim_advance(0) == 0, "frame dopo la fine: nessuna attiva");
        check(gmb_anim_count() == 0, "frame dopo la fine: slot liberato");
        check(gmb_anim_get(i, &h, &tui, &prop, &done, &v) == 0, "get: slot liberato");
    }

    /* ---- easing out: a metà durata è oltre metà (0.75) -------------------- */
    {
        gmb_anim_clear();
        int i = gmb_anim_start(1, 0, GMB_ANIM_TOP, 0.0, 100.0, 400, GMB_ANIM_EASE_OUT);
        check(i >= 0, "out: start");
        gmb_anim_advance(200);
        int h, tui, prop, done; double v;
        gmb_anim_get(i, &h, &tui, &prop, &done, &v);
        check(near(v, 75.0, 1e-9), "out: a metà durata = 75");
    }

    /* ---- easing inout: a metà durata è metà ------------------------------- */
    {
        gmb_anim_clear();
        int i = gmb_anim_start(1, 0, GMB_ANIM_TOP, 100.0, 0.0, 400, GMB_ANIM_EASE_IN_OUT);
        gmb_anim_advance(200);
        int h, tui, prop, done; double v;
        gmb_anim_get(i, &h, &tui, &prop, &done, &v);
        check(near(v, 50.0, 1e-9), "inout: a metà durata = 50");
        /* verso il basso: il valore scende */
        gmb_anim_advance(100);
        gmb_anim_get(i, &h, &tui, &prop, &done, &v);
        check(v < 50.0, "inout: dopo metà scende sotto 50");
    }

    /* ---- durata 0 = snap immediato (ui.reduce_motion) --------------------- */
    {
        gmb_anim_clear();
        int i = gmb_anim_start(2, 1, GMB_ANIM_WIDTH, 10.0, 90.0, 0, GMB_ANIM_EASE_LINEAR);
        check(i >= 0, "snap: start");
        int h, tui, prop, done; double v;
        gmb_anim_get(i, &h, &tui, &prop, &done, &v);
        check(done == 1 && near(v, 90.0, 1e-9), "snap: valore subito al target");
        check(gmb_anim_advance(16) == 0, "snap: nessuna attiva al frame dopo");
        check(gmb_anim_count() == 0, "snap: slot liberato");
    }

    /* ---- forget / clear --------------------------------------------------- */
    {
        gmb_anim_clear();
        gmb_anim_start(10, 1, GMB_ANIM_LEFT, 0, 10, 100, GMB_ANIM_EASE_LINEAR);
        gmb_anim_start(11, 0, GMB_ANIM_TOP,  0, 10, 100, GMB_ANIM_EASE_LINEAR);
        check(gmb_anim_count() == 2, "forget: due slot");
        gmb_anim_forget(10, 1);
        check(gmb_anim_count() == 1, "forget: rimuove solo l'handle indicato");
        gmb_anim_forget(10, 0);
        check(gmb_anim_count() == 1, "forget: is_tui diverso non tocca");
        gmb_anim_clear();
        check(gmb_anim_count() == 0, "clear: azzera tutto");
        check(gmb_anim_active() == 0, "clear: nessuna attiva");
    }

    /* ---- sostituzione: stessa proprietà+handle riusa lo slot --------------- */
    {
        gmb_anim_clear();
        int i1 = gmb_anim_start(20, 1, GMB_ANIM_LEFT, 0.0, 100.0, 400, GMB_ANIM_EASE_LINEAR);
        int i2 = gmb_anim_start(20, 1, GMB_ANIM_LEFT, 0.0, 500.0, 100, GMB_ANIM_EASE_LINEAR);
        check(i1 >= 0 && i1 == i2, "sostituzione: stesso slot");
        check(gmb_anim_count() == 1, "sostituzione: una sola animazione");
        gmb_anim_advance(50);
        int h, tui, prop, done; double v;
        gmb_anim_get(i2, &h, &tui, &prop, &done, &v);
        check(near(v, 250.0, 1e-9), "sostituzione: segue il nuovo target (250 a metà di 100ms)");
        /* una proprietà DIVERSA dello stesso controllo è un altro slot */
        gmb_anim_start(20, 1, GMB_ANIM_TOP, 0.0, 10.0, 100, GMB_ANIM_EASE_LINEAR);
        check(gmb_anim_count() == 2, "proprietà diversa: slot separato");
    }

    /* ---- proprietà fuori intervallo -------------------------------------- */
    {
        check(gmb_anim_start(1, 1, 99, 0, 1, 10, GMB_ANIM_EASE_LINEAR) == -1,
              "start: proprietà non valida rifiutata");
        check(gmb_anim_start(1, 1, -1, 0, 1, 10, GMB_ANIM_EASE_LINEAR) == -1,
              "start: proprietà negativa rifiutata");
    }

    if (g_fails) { printf("%d verifiche fallite\n", g_fails); return 1; }
    printf("gmb_anim_selfcheck: OK\n");
    return 0;
}
