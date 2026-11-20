/* gmb_clock_selfcheck.c — verifica del motore a tempo (gmb_clock.h).
 *
 * I numeri sono oracoli espliciti: passo fisso, accumulatore, clamp del frame,
 * tetto al recupero, scala, pausa, avanzamento deterministico, fps misurati.
 * Nessuna finestra: gira in headless, e solo l'ultima prova (pacing) usa il
 * tempo reale — con bound larghi, perché la macchina è condivisa.
 *
 * Eseguito da: cmake --build <build> --target gmb_clock_selfcheck && ./gmb_clock_selfcheck
 */
#include "gmb_clock.h"
#include "gmb_runtime.h"

#include <math.h>
#include <stdio.h>

static int g_fails = 0;

static void check(int cond, const char* what, int line)
{
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}

#define CHECK(c, w) check((c) != 0, (w), __LINE__)
#define NEAR(a, b, tol) (fabs((a) - (b)) <= (tol))

int main(void)
{
    gmb_clock_t c;

    printf("clock: init e passo fisso... ");
    gmb_clock_init(&c, 60.0);
    CHECK(NEAR(c.step, 1.0 / 60.0, 1e-12), "step = 1/60");
    CHECK(NEAR(gmb_clock_dt(&c), 1.0 / 60.0, 1e-12), "dt = 1/60");
    CHECK(c.scale == 1.0, "scala iniziale 1");
    CHECK(c.paused == 0, "non in pausa");
    CHECK(c.max_ticks == GMB_CLOCK_DEFAULT_MAX_TICKS, "tetto recupero di default");
    CHECK(NEAR(c.max_frame, GMB_CLOCK_DEFAULT_MAX_FRAME, 1e-12), "clamp di default");
    gmb_clock_set_fps(&c, 0.0);            /* fuori range: si torna al default */
    CHECK(NEAR(c.fps, GMB_CLOCK_DEFAULT_FPS, 1e-12), "fps invalido -> 60");
    gmb_clock_set_fps(&c, 60.0);
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: un passo per un passo... ");
    {
        int n = gmb_clock_advance(&c, 1.0 / 60.0);
        CHECK(n == 1, "1/60 s = 1 tick");
        CHECK(NEAR(c.time, 1.0 / 60.0, 1e-9), "tempo simulato = 1/60");
        CHECK(NEAR(gmb_clock_alpha(&c), 0.0, 1e-6), "alpha ~0 dopo un passo esatto");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: il resto resta nell'accumulatore... ");
    gmb_clock_reset(&c);
    {
        int n = gmb_clock_advance(&c, 0.4 / 60.0);
        CHECK(n == 0, "0.4 passi = 0 tick");
        CHECK(NEAR(gmb_clock_alpha(&c), 0.4, 1e-6), "alpha 0.4");
        CHECK(NEAR(gmb_clock_rest(&c), 0.6 / 60.0, 1e-9), "mancano 0.6 passi");
        n = gmb_clock_advance(&c, 0.6 / 60.0);
        CHECK(n == 1, "0.4 + 0.6 = 1 tick");
        CHECK(NEAR(gmb_clock_alpha(&c), 0.0, 1e-6), "alpha torna a 0");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: clamp di un blocco lungo... ");
    gmb_clock_reset(&c);
    {
        int n = gmb_clock_advance(&c, 0.5);   /* 30 passi richiesti, clamp 250 ms */
        CHECK(n == GMB_CLOCK_DEFAULT_MAX_TICKS, "recupero limitato a 8 passi");
        CHECK(c.clamped == 1, "clamp contato");
        CHECK(c.dropped == 7, "7 passi scartati (15 richiesti, 8 eseguiti)");
        CHECK(NEAR(c.accum, 0.0, 1e-12), "il ritardo non resta in accumulatore");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: tetto al recupero a 120 Hz... ");
    {
        gmb_clock_t f;
        gmb_clock_init(&f, 120.0);
        int n = gmb_clock_advance(&f, 0.1);   /* 12 passi, tetto 8 */
        CHECK(n == 8, "8 passi, non 12");
        CHECK(f.dropped == 4, "4 passi scartati");
        CHECK(f.ticks == 8, "contatore passi coerente");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: pausa e scala... ");
    gmb_clock_reset(&c);
    {
        double t0;
        gmb_clock_set_paused(&c, 1);
        gmb_clock_advance(&c, 1.0);
        CHECK(c.ticks == 0, "in pausa nessun passo");
        t0 = gmb_clock_time(&c);
        CHECK(NEAR(t0, 0.0, 1e-12), "in pausa il tempo non avanza");
        gmb_clock_set_paused(&c, 0);
        gmb_clock_set_scale(&c, 0.5);
        CHECK(gmb_clock_advance(&c, 2.0 / 60.0) == 1, "scala 0.5: 2 passi reali = 1 simulato");
        gmb_clock_set_scale(&c, 0.0);
        CHECK(gmb_clock_advance(&c, 1.0) == 0, "scala 0: tempo congelato");
        gmb_clock_set_scale(&c, 20.0);          /* fuori range: ignorato */
        CHECK(NEAR(c.scale, 0.0, 1e-12), "scala fuori range ignorata");
        gmb_clock_set_scale(&c, 1.0);
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: passo deterministico... ");
    gmb_clock_reset(&c);
    {
        double a0 = c.accum;
        int n = gmb_clock_step(&c, 5);
        CHECK(n == 5, "5 passi");
        CHECK(c.ticks == 5, "contatore");
        CHECK(NEAR(c.time, 5.0 / 60.0, 1e-12), "tempo = 5/60");
        CHECK(NEAR(c.accum, a0, 1e-12), "l'accumulatore non si tocca");
        CHECK(gmb_clock_step(&c, 0) == 0, "0 passi = no-op");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: fps misurati e alpha... ");
    {
        gmb_clock_t f;
        gmb_clock_init(&f, 60.0);
        for (int i = 0; i < 40; i++) gmb_clock_advance(&f, 1.0 / 30.0);
        CHECK(NEAR(gmb_clock_fps_measured(&f), 30.0, 1.0), "fps misurati ~30");
        CHECK(gmb_clock_alpha(&f) >= 0.0 && gmb_clock_alpha(&f) < 1.0, "alpha in [0,1)");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: orologio monotono... ");
    {
        int64_t a = gmb_clock_now_ns();
        gmb_sleep_ms(2);
        int64_t b = gmb_clock_now_ns();
        CHECK(b > a, "il tempo monotono cresce");
        CHECK(b - a >= 1000000, "almeno 1 ms in 2 ms di attesa");
        CHECK(gmb_clock_frame_clock() == gmb_clock_frame_clock(), "clock del frame = singleton");
        CHECK(gmb_clock_frame_clock() != NULL, "clock del frame esiste");
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    printf("clock: pacing reale (10 frame a 100 Hz, bound larghi)... ");
    {
        gmb_clock_t p;
        int total = 0;
        int64_t t0, t1;
        double elapsed;
        gmb_clock_init(&p, 100.0);
        t0 = gmb_clock_now_ns();
        for (int i = 0; i < 10; i++) {
            total += gmb_clock_begin_frame(&p);
            double rest = gmb_clock_rest(&p);
            if (rest > 0.001) gmb_sleep_ms((int)(rest * 1000.0 + 0.5));
        }
        t1 = gmb_clock_now_ns();
        elapsed = (double)(t1 - t0) / 1e9;
        CHECK(total >= 8 && total <= 10, "10 frame -> 8..10 passi");
        CHECK(elapsed > 0.06 && elapsed < 1.0, "circa 0.1 s (macchina condivisa)");
        printf("[%.3f s, %d passi] ", elapsed, total);
    }
    printf("%s\n", g_fails ? "KO" : "OK");

    if (g_fails) { printf("FALLITI: %d\n", g_fails); return 1; }
    printf("TUTTO OK\n");
    return 0;
}
