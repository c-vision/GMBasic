/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 */
/* run_tests.c — test di funzionamento di netlb (model-free, deterministici). */
#include "netlb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int g_checks = 0, g_fails = 0;
#define CHECK(cond, msg) do { g_checks++; if (!(cond)) { g_fails++; printf("  FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static netlb_backend_spec mk(const char* n, double w) {
    netlb_backend_spec s; memset(&s, 0, sizeof s);
    snprintf(s.name, sizeof s.name, "%s", n);
    s.weight = w; s.healthy = 1; s.cap = 0;
    return s;
}
static void key_of(int i, char* buf) { snprintf(buf, 32, "10.0.0.%d:8080", i); }

/* ── 1. base ── */
static void t_base(void) {
    printf("[base]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 1);
    CHECK(p != NULL, "pool_new");
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1), c = mk("c", 1);
    CHECK(netlb_add_backend(p, &a) == 0, "add a");
    CHECK(netlb_add_backend(p, &b) == 1, "add b");
    CHECK(netlb_add_backend(p, &c) == 2, "add c");
    CHECK(netlb_backend_count(p) == 3, "count 3");
    CHECK(netlb_pool_ready(p), "ready");
    CHECK(netlb_pick(p, NULL, 0) >= 0, "pick != -1");
    netlb_free(p);
}

/* ── 2. round robin: uniforme ── */
static void t_rr(void) {
    printf("[round-robin]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 7);
    for (int i = 0; i < 3; i++) { netlb_backend_spec s = mk("x", 1); s.name[1] = (char)('0' + i); netlb_add_backend(p, &s); }
    int cnt[3] = {0};
    for (int i = 0; i < 300; i++) cnt[netlb_pick(p, NULL, 0)]++;
    CHECK(cnt[0] == 100 && cnt[1] == 100 && cnt[2] == 100, "rr uniforme 100/100/100");
    netlb_free(p);
}

/* ── 3. weighted round robin: proporzione ── */
static void t_wrr(void) {
    printf("[weighted-round-robin]\n");
    netlb_pool* p = netlb_pool_new(NETLB_WRR, 7);
    netlb_backend_spec a = mk("a", 5), b = mk("b", 1), c = mk("c", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b); netlb_add_backend(p, &c);
    int cnt[3] = {0};
    for (int i = 0; i < 700; i++) cnt[netlb_pick(p, NULL, 0)]++;
    /* atteso ~500/100/100 */
    CHECK(cnt[0] > 450 && cnt[0] < 550, "wrr a ~500");
    CHECK(abs(cnt[1] - 100) < 30 && abs(cnt[2] - 100) < 30, "wrr b,c ~100");
    netlb_free(p);
}

/* ── 4. random: chi-quadro basso su 4 ── */
static void t_random(void) {
    printf("[random]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RANDOM, 42);
    for (int i = 0; i < 4; i++) { netlb_backend_spec s = mk("r", 1); s.name[1] = (char)('0' + i); netlb_add_backend(p, &s); }
    int cnt[4] = {0}; int N = 8000;
    for (int i = 0; i < N; i++) cnt[netlb_pick(p, NULL, 0)]++;
    double exp = N / 4.0, chi = 0;
    for (int i = 0; i < 4; i++) chi += (cnt[i] - exp) * (cnt[i] - exp) / exp;
    CHECK(chi < 16.27, "random: chi2 < 16.27 (p=0.001, 3 df)");
    netlb_free(p);
}

/* ── 5. least connections ── */
static void t_lc(void) {
    printf("[least-connections]\n");
    netlb_pool* p = netlb_pool_new(NETLB_LC, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    netlb_begin(p, 0); netlb_begin(p, 0); netlb_begin(p, 0);   /* a ha 3 inflight */
    int id = netlb_pick(p, NULL, 0);
    CHECK(id == 1, "lc scegle il backend con meno connessioni");
    netlb_end(p, 0, 1, 0); netlb_end(p, 0, 1, 0);
    CHECK(netlb_backend_inflight(p, 0) == 1, "inflight decrementato");
    netlb_free(p);
}

/* ── 6. P2C + EWMA: evita il backend lento ── */
static void t_p2c(void) {
    printf("[p2c-ewma]\n");
    netlb_pool* p = netlb_pool_new(NETLB_P2C, 99);
    netlb_backend_spec a = mk("fast", 1), b = mk("slow", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    for (int i = 0; i < 50; i++) { netlb_begin(p, 0); netlb_end(p, 0, 1, 10.0); }   /* a: 10ms */
    for (int i = 0; i < 50; i++) { netlb_begin(p, 1); netlb_end(p, 1, 1, 200.0); }  /* b: 200ms */
    /* ora b ha una coda: 5 richieste in corso su b */
    for (int i = 0; i < 5; i++) netlb_begin(p, 1);
    int picks_b = 0;
    for (int i = 0; i < 200; i++) { int id = netlb_pick(p, NULL, 0); if (id == 1) picks_b++; }
    CHECK(picks_b < 100, "p2c evita il backend lento (b scelto <50%)");
    netlb_free(p);
}

/* ── 7. consistent hash: determinismo + minimal disruption ── */
static void t_chash(void) {
    printf("[consistent-hash]\n");
    netlb_pool* p = netlb_pool_new(NETLB_CHASH, 3);
    for (int i = 0; i < 4; i++) { netlb_backend_spec s = mk("be", 1); s.name[2] = (char)('0' + i); netlb_add_backend(p, &s); }
    int N = 2000; char k[32]; int before[2000];
    for (int i = 0; i < N; i++) { key_of(i, k); before[i] = netlb_pick(p, k, strlen(k)); }
    /* determinismo */
    int same = 1; for (int i = 0; i < N; i++) { key_of(i, k); if (netlb_pick(p, k, strlen(k)) != before[i]) same = 0; }
    CHECK(same, "chash: stesso key -> stesso backend");
    /* distribuzione (uniformita' approssimativa) */
    int cnt[4] = {0}; for (int i = 0; i < N; i++) cnt[before[i]]++;
    int ok = 1; for (int i = 0; i < 4; i++) if (cnt[i] < N / 8 || cnt[i] > N / 2) ok = 0;
    CHECK(ok, "chash: distribuzione ragionevole (4 backend)");
    /* rimuovi un backend -> ~25% delle chiavi si sposta */
    netlb_remove_backend(p, 2);
    int moved = 0; for (int i = 0; i < N; i++) { key_of(i, k); if (netlb_pick(p, k, strlen(k)) != before[i]) moved++; }
    double frac = (double)moved / N;
    CHECK(frac > 0.15 && frac < 0.40, "chash: disruption ~25% (misurata)");
    printf("  (chash disruption = %.1f%%)\n", frac * 100.0);
    netlb_free(p);
}

/* ── 8. HRW/rendezvous ── */
static void t_hrw(void) {
    printf("[rendezvous-hrw]\n");
    netlb_pool* p = netlb_pool_new(NETLB_HRW, 5);
    for (int i = 0; i < 4; i++) { netlb_backend_spec s = mk("h", 1); s.name[1] = (char)('0' + i); netlb_add_backend(p, &s); }
    int N = 2000; char k[32]; int before[2000];
    for (int i = 0; i < N; i++) { key_of(i, k); before[i] = netlb_pick(p, k, strlen(k)); }
    int same = 1; for (int i = 0; i < N; i++) { key_of(i, k); if (netlb_pick(p, k, strlen(k)) != before[i]) same = 0; }
    CHECK(same, "hrw: deterministico");
    netlb_remove_backend(p, 3);
    int moved = 0; for (int i = 0; i < N; i++) { key_of(i, k); if (netlb_pick(p, k, strlen(k)) != before[i]) moved++; }
    double frac = (double)moved / N;
    CHECK(frac > 0.15 && frac < 0.40, "hrw: disruption ~25%");
    printf("  (hrw disruption = %.1f%%)\n", frac * 100.0);
    netlb_free(p);
}

/* ── 9. Maglev ── */
static void t_maglev(void) {
    printf("[maglev]\n");
    netlb_pool* p = netlb_pool_new(NETLB_MAGLEV, 11);
    for (int i = 0; i < 4; i++) { netlb_backend_spec s = mk("m", 1); s.name[1] = (char)('0' + i); netlb_add_backend(p, &s); }
    CHECK(netlb_maglev_build(p, 4099) == 0, "maglev build");
    int N = 2000; char k[32]; int before[2000]; int cnt[4] = {0};
    for (int i = 0; i < N; i++) { key_of(i, k); before[i] = netlb_pick(p, k, strlen(k)); cnt[before[i]]++; }
    int distinct = 0; for (int i = 0; i < 4; i++) if (cnt[i] > 0) distinct++;
    CHECK(distinct == 4, "maglev: usa tutti i backend");
    netlb_remove_backend(p, 1);
    int moved = 0; for (int i = 0; i < N; i++) { key_of(i, k); if (netlb_pick(p, k, strlen(k)) != before[i]) moved++; }
    double frac = (double)moved / N;
    CHECK(frac > 0.15 && frac < 0.40, "maglev: disruption ~25%");
    printf("  (maglev disruption = %.1f%%)\n", frac * 100.0);
    netlb_free(p);
}

/* ── 10. salute: attiva/passiva + eiezione ── */
static void t_health(void) {
    printf("[health]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    netlb_probe(p, 0, 0);            /* a unhealthy (attivo) */
    CHECK(netlb_is_healthy(p, 0) == 0, "probe fail -> unhealthy");
    int all_b = 1; for (int i = 0; i < 50; i++) if (netlb_pick(p, NULL, 0) != 1) all_b = 0;
    CHECK(all_b, "backend unhealthy saltato");
    netlb_probe(p, 0, 1);            /* rientro */
    CHECK(netlb_is_healthy(p, 0) == 1, "probe ok -> healthy");
    /* eiezione passiva: 3 fallimenti consecutivi su b */
    netlb_begin(p, 1); netlb_end(p, 1, 0, 5); netlb_begin(p, 1); netlb_end(p, 1, 0, 5); netlb_begin(p, 1); netlb_end(p, 1, 0, 5);
    CHECK(netlb_is_healthy(p, 1) == 0, "3 fail -> eiezione passiva");
    netlb_begin(p, 1); netlb_end(p, 1, 1, 5);   /* ok resetta */
    /* (rientro richiede probe: opzionale) */
    netlb_free(p);
}

/* ── 11. slow-start: il nuovo backend non viene inondato ── */
static void t_slowstart(void) {
    printf("[slow-start]\n");
    netlb_pool* p = netlb_pool_new(NETLB_WRR, 1);
    netlb_set_slow_start(p, 1000.0);
    netlb_set_now(p, 0.0);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    netlb_set_now(p, 5000.0);   /* a,b maturano (t_added=0, finestra 1000ms) */
    netlb_backend_spec c = mk("c", 1); netlb_add_backend(p, &c);   /* c nuovo a t=5000 (fattore 0) */
    /* entro la finestra: eff_weight di c ~ 0 -> pochi pick */
    int picks_c = 0; for (int i = 0; i < 300; i++) if (netlb_pick(p, NULL, 0) == 2) picks_c++;
    CHECK(picks_c < 60, "slow-start: nuovo backend poco caricato (fattore <20%)");
    netlb_set_now(p, 7000.0);  /* oltre la finestra (c: 5000+1000): peso pieno */
    int picks_c2 = 0; for (int i = 0; i < 300; i++) if (netlb_pick(p, NULL, 0) == 2) picks_c2++;
    (void)picks_c2;
    CHECK(picks_c2 > 50, "slow-start: dopo la finestra peso pieno (>50)");
    netlb_free(p);
}

/* ── 12. draining: backend rimosso non riceve nuovi pick ── */
static void t_drain(void) {
    printf("[draining]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    netlb_begin(p, 0);                 /* richiesta in corso su a */
    netlb_remove_backend(p, 0);
    int all_b = 1; for (int i = 0; i < 50; i++) if (netlb_pick(p, NULL, 0) != 1) all_b = 0;
    CHECK(all_b, "draining: a non riceve nuovi pick");
    CHECK(netlb_backend_inflight(p, 0) == 1, "draining: richiesta in corso preservata");
    netlb_end(p, 0, 1, 2.0);
    CHECK(netlb_backend_inflight(p, 0) == 0, "draining: richiesta chiusa");
    netlb_free(p);
}

/* ── 13. retry budget ── */
static void t_retry(void) {
    printf("[retry-budget]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1), c = mk("c", 1);
    netlb_add_backend(p, &a); netlb_add_backend(p, &b); netlb_add_backend(p, &c);
    netlb_set_retry_budget(p, 0.5);
    netlb_backend_id tried[2] = {0, 1};
    int id = netlb_pick_ex(p, NULL, 0, tried, 2);
    CHECK(id == 2, "pick_ex evita i tentati");
    netlb_begin(p, 0); netlb_end(p, 0, 0, 1);  /* attempts++ */
    int r1 = netlb_should_retry(p, 0, 1);
    CHECK(r1 == 1, "retry consentito entro budget");
    netlb_free(p);
}

/* ── 14. JIQ: idle-first + fallback + coda ── */
static void t_jiq(void) {
    printf("[jiq]\n");
    netlb_pool* p = netlb_pool_new(NETLB_JIQ, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1), c = mk("c", 1);
    netlb_add_backend(p, &a); netlb_add_backend(p, &b); netlb_add_backend(p, &c);
    CHECK(netlb_idle_count(p) == 3, "jiq: 3 backend idle all'avvio");
    netlb_begin(p, 0); netlb_begin(p, 1);                 /* 0,1 occupati */
    CHECK(netlb_idle_count(p) == 1, "jiq: idle = 1");
    int id = netlb_pick(p, NULL, 0);
    CHECK(id == 2, "jiq: scegle l'unico backend idle");
    netlb_begin(p, 2);                                  /* ora tutti occupati */
    int id2 = netlb_pick(p, NULL, 0);
    CHECK(id2 >= 0 && id2 < 3, "jiq: fallback P2C se nessun idle");
    netlb_end(p, 0, 1, 5); netlb_end(p, 1, 1, 5); netlb_end(p, 2, 1, 5);
    CHECK(netlb_idle_count(p) == 3, "jiq: tutti di nuovo idle dopo end");
    netlb_free(p);
}

/* Simulazione di coda (deterministica): JIQ (idle-first) vs P2C. Ritorna l'attesa media (ms). */
static double sim_wait(netlb_policy pol, int servers, double s_ms, double dt_ms, int N) {
    netlb_pool* p = netlb_pool_new(pol, 123);
    for (int i = 0; i < servers; i++) { netlb_backend_spec b = mk("s", 1); netlb_add_backend(p, &b); }
    double freeat[64]; for (int i = 0; i < servers; i++) freeat[i] = 0.0;
    struct { double t; int id; } comp[8192]; int nc = 0;
    double wait = 0.0;
    for (int r = 0; r < N; r++) {
        double now = (double)r * dt_ms;
        for (int j = 0; j < nc; ) {                    /* completa le richieste finite */
            if (comp[j].t <= now) { netlb_end(p, comp[j].id, 1, s_ms); comp[j] = comp[--nc]; }
            else j++;
        }
        int id = netlb_pick(p, NULL, 0);
        if (id < 0) break;
        double ready = freeat[id] > now ? freeat[id] : now;
        wait += ready - now;
        freeat[id] = ready + s_ms;
        netlb_begin(p, id);
        comp[nc].t = freeat[id]; comp[nc].id = id; nc++;
    }
    netlb_free(p);
    return wait / (double)N;
}

static void t_jiq_sim(void) {
    printf("[jiq-queue-latency]\n");
    double j = sim_wait(NETLB_JIQ, 16, 20.0, 1.4, 3000);
    double pc = sim_wait(NETLB_P2C, 16, 20.0, 1.4, 3000);
    printf("  (attesa media: JIQ=%.3f ms  P2C=%.3f ms)\n", j, pc);
    CHECK(j <= pc + 1e-9, "jiq: attesa di coda <= P2C");
    CHECK(j < 5.0, "jiq: attesa di coda contenuta (<5 ms)");
}

/* ── 15. sticky soft: affinita' cookie->backend con fallback e TTL ── */
static void t_sticky(void) {
    printf("[sticky-soft]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1); netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    netlb_set_now(p, 0.0);
    netlb_set_sticky(p, 1000.0);                         /* TTL 1s */
    const char* cookie = "sess-42"; size_t cl = strlen(cookie);
    int id = netlb_pick(p, cookie, cl);
    CHECK(id == 0, "sticky: primo pick -> policy (RR=a)");
    CHECK(netlb_sticky_count(p) == 1, "sticky: 1 voce in tabella");
    CHECK(netlb_pick(p, cookie, cl) == id, "sticky: stesso cookie -> stesso backend (affinita')");
    const char* c2 = "sess-99";
    CHECK(netlb_pick(p, c2, strlen(c2)) != id, "sticky: cookie diverso -> backend diverso");
    netlb_probe(p, 0, 0);                                /* a decade */
    CHECK(netlb_pick(p, cookie, cl) == 1, "sticky: fallback se il backend mappato non e' sano");
    netlb_probe(p, 0, 1);
    netlb_set_now(p, 5000.0);                            /* oltre il TTL */
    CHECK(netlb_pick(p, cookie, cl) >= 0, "sticky: dopo il TTL ri-mappa senza crash");
    netlb_free(p);
}

/* ── 16. limite pending (circuit breaking): cap per backend ── */
static void t_cap(void) {
    printf("[pending-limit]\n");
    netlb_pool* p = netlb_pool_new(NETLB_RR, 1);
    netlb_backend_spec a = mk("a", 1), b = mk("b", 1);
    a.cap = 1; b.cap = 1;                              /* max 1 richiesta in corso ciascuno */
    netlb_add_backend(p, &a); netlb_add_backend(p, &b);
    int i1 = netlb_pick(p, NULL, 0); netlb_begin(p, i1);   /* a pieno */
    int i2 = netlb_pick(p, NULL, 0); netlb_begin(p, i2);   /* b pieno */
    CHECK(i1 != i2, "cap: le due richieste vanno su backend diversi");
    CHECK(netlb_pick(p, NULL, 0) == -1, "cap: tutti al limite -> nessun pick (-1)");
    netlb_end(p, i1, 1, 5);
    CHECK(netlb_pick(p, NULL, 0) == i1, "cap: il backend liberato torna eleggibile");
    netlb_free(p);

    /* SETCAP a runtime */
    netlb_pool* q = netlb_pool_new(NETLB_RR, 1);
    netlb_backend_spec c = mk("c", 1); c.cap = 1; netlb_add_backend(q, &c);
    netlb_begin(q, 0);
    CHECK(netlb_pick(q, NULL, 0) == -1, "setcap: cap=1 raggiunto -> nessun pick");
    netlb_set_cap(q, 0, 3);
    CHECK(netlb_pick(q, NULL, 0) == 0, "setcap: cap alzato a runtime -> di nuovo eleggibile");
    netlb_free(q);
}

/* p99 del tempo di risposta (ms) in una simulazione di coda deterministica.
 * Servizio STOCASTICO (2..32 ms): e' il caso in cui P2C batte RR sul tail, perche' evita i server
 * momentaneamente occupati da una richiesta lunga, che RR invece colpisce a turno. */
static int cmpd(const void* a, const void* b) { double x = *(const double*)a, y = *(const double*)b; return x < y ? -1 : (x > y ? 1 : 0); }
static uint64_t tRng;
static uint64_t next_svc_rnd(void) {
    tRng += 0x9E3779B97F4A7C15ULL; uint64_t z = tRng;
    z ^= z >> 30; z *= 0xBF58476D1CE4E5B9ULL; z ^= z >> 27; z *= 0x94D049BB133111EBULL; z ^= z >> 31;
    return z;
}
static double sim_p99(netlb_policy pol, int servers, int nslow, double dt_ms, int N) {
    netlb_pool* p = netlb_pool_new(pol, 77);
    for (int i = 0; i < servers; i++) { netlb_backend_spec b = mk("s", 1); netlb_add_backend(p, &b); }
    double base[64], rng2[64];
    for (int i = 0; i < servers; i++) {
        int slow = (i >= servers - nslow);                 /* gli ultimi nslow sono lenti */
        base[i] = slow ? 60.0 : 3.0; rng2[i] = slow ? 40.0 : 4.0;
    }
    double freeat[64]; for (int i = 0; i < servers; i++) freeat[i] = 0.0;
    struct { double t; int id; double s; } comp[16384]; int nc = 0;
    double* rt = (double*)malloc(sizeof(double) * (size_t)N); int nr = 0;
    tRng = 0x9E3779B9ULL;
    for (int r = 0; r < N; r++) {
        double now = (double)r * dt_ms;
        for (int j = 0; j < nc; ) { if (comp[j].t <= now) { netlb_end(p, comp[j].id, 1, comp[j].s); comp[j] = comp[--nc]; } else j++; }
        int id = netlb_pick(p, NULL, 0); if (id < 0) break;
        double s = base[id] + (double)(next_svc_rnd() % 1000u) / 1000.0 * rng2[id];
        double ready = freeat[id] > now ? freeat[id] : now;
        rt[nr++] = ready - now + s;
        freeat[id] = ready + s;
        netlb_begin(p, id);
        comp[nc].t = freeat[id]; comp[nc].id = id; comp[nc].s = s; nc++;
    }
    double p99 = 0.0;
    if (nr > 0) { qsort(rt, (size_t)nr, sizeof(double), cmpd); p99 = rt[(int)((double)nr * 0.99)]; }
    free(rt); netlb_free(p);
    return p99;
}
/* ── 18. P2C batte RR sul tail (p99) con backend ETEROGENEI: RR manda a turno anche sui lenti
 * (che si saturano), P2C li evita scegliendo tra i meno carichi. ── */
static void t_p99(void) {
    printf("[p2c-vs-rr-p99]\n");
    double p_rr = sim_p99(NETLB_RR, 8, 2, 6.0, 3000);
    double p_p2c = sim_p99(NETLB_P2C, 8, 2, 6.0, 3000);
    printf("  (p99 coda: RR=%.1f ms  P2C=%.1f ms)\n", p_rr, p_p2c);
    CHECK(p_p2c < p_rr, "p2c batte rr sul p99 (tail)");
}

/* ── 17. HRW gerarchico (skeleton): O(log n) = O(n) ── */
static void t_hrw_skel(void) {
    printf("[hrw-skeleton]\n");
    int N = 400;
    netlb_pool* p = netlb_pool_new(NETLB_HRW, 5);
    for (int i = 0; i < N; i++) {
        netlb_backend_spec s = mk("node", 1);
        snprintf(s.name, sizeof s.name, "node-%03d", i);
        netlb_add_backend(p, &s);
    }
    netlb_set_hrw_skeleton(p, 8, 8);                     /* cluster=8, fanout=8 */
    char k[32]; int cnt[400]; memset(cnt, 0, sizeof cnt);
    long maxh = 0, sumh = 0;
    for (int i = 0; i < N; i++) {
        key_of(i, k);
        long before = netlb_hrw_hashes(p);
        int id = netlb_pick(p, k, strlen(k));
        long used = netlb_hrw_hashes(p) - before;
        if (used > maxh) maxh = used;
        sumh += used;
        if (id >= 0 && id < N) cnt[id]++;
    }
    int distinct = 0; for (int i = 0; i < N; i++) if (cnt[i] > 0) distinct++;
    printf("  (skel N=%d: hash/pick max=%ld tot=%ld; backend distinti=%d)\n", N, maxh, sumh, distinct);
    CHECK(distinct > N / 2, "skel: distribuzione su buona parte dei backend");
    CHECK(maxh <= 64, "skel: hash/pick O(log n) (<=64, non O(n)=400)");
    netlb_free(p);

    /* N <= m -> un solo cluster -> deve coincidere con l'HRW piatto */
    netlb_pool* q = netlb_pool_new(NETLB_HRW, 9);
    for (int i = 0; i < 4; i++) { netlb_backend_spec s = mk("s", 1); s.name[1] = (char)('0' + i); netlb_add_backend(q, &s); }
    char k2[32]; int flat[1000];
    for (int i = 0; i < 1000; i++) { key_of(i, k2); flat[i] = netlb_pick(q, k2, strlen(k2)); }
    netlb_set_hrw_skeleton(q, 8, 4);                     /* m=8 > 4 backend */
    int eq = 1; for (int i = 0; i < 1000; i++) { key_of(i, k2); if (netlb_pick(q, k2, strlen(k2)) != flat[i]) eq = 0; }
    CHECK(eq, "skel: con N<=m coincide con HRW piatto");
    netlb_free(q);

    /* disruption: i cluster per id STABILE -> rimuovere un backend tocca solo il suo cluster.
     * N=200, m=8 -> 25 cluster: atteso ~1/25 dei key nel cluster, di cui ~1/8 su quel backend. */
    netlb_pool* r = netlb_pool_new(NETLB_HRW, 5);
    for (int i = 0; i < 200; i++) { netlb_backend_spec s = mk("n", 1); snprintf(s.name, sizeof s.name, "n-%03d", i); netlb_add_backend(r, &s); }
    netlb_set_hrw_skeleton(r, 8, 8);
    char kb[32]; int before2[2000];
    for (int i = 0; i < 2000; i++) { key_of(i, kb); before2[i] = netlb_pick(r, kb, strlen(kb)); }
    netlb_remove_backend(r, 100);
    int moved = 0; for (int i = 0; i < 2000; i++) { key_of(i, kb); if (netlb_pick(r, kb, strlen(kb)) != before2[i]) moved++; }
    double frac = (double)moved / 2000.0;
    printf("  (skel disruption (N=200, 1 rimosso) = %.2f%%)\n", frac * 100.0);
    CHECK(frac < 0.03, "skel: rimozione tocca solo il cluster (<3%)");
    netlb_free(r);
}

int main(void) {
    printf("=== netlb run_tests ===\n");
    t_base(); t_rr(); t_wrr(); t_random(); t_lc(); t_p2c();
    t_chash(); t_hrw(); t_maglev(); t_health(); t_slowstart(); t_drain(); t_retry();
    t_jiq(); t_jiq_sim(); t_sticky(); t_cap(); t_hrw_skel(); t_p99();
    printf("\n=== %d check, %d FAIL ===\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
