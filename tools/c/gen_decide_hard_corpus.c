/* gen_decide_hard_corpus.c — corpus sintetico "duro" per il decision engine (Open-Jev-like).
 *
 * Scopo: i 400 esempi sintetici di Open-Jev sono task di copia (acc ~ 1.0 sui modelli forti,
 * ECE ~ 0): non esercitano la calibrazione. Questo generatore produce domande che richiedono un
 * minimo di ragionamento (aritmetica, confronto, ordinamento transitivo, conteggio di lettere,
 * sequenze, giorni della settimana) con distrattori plausibili, cosi' l'accuratezza dei modelli
 * forti resta sotto 1 e quella dei piccoli sopra il caso.
 *
 * Formato (identico al runner test_decide_openjev):
 *   {"context":"…","options":["…","…"],"label":N}
 * Deterministico: stesso seed -> stesso file. Nessuna dipendenza (C99 puro).
 * Uso: gen_decide_hard_corpus [n_righe=400] [seed=1] > hard_synth.jsonl
 * Licenza del corpus generato: nostra (generato da questo programma, nessun dato esterno).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long g_s = 1;
static unsigned rnd(void)
{
    g_s = g_s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(g_s >> 33);
}
static int rint_(int lo, int hi) { return lo + (int)(rnd() % (unsigned)(hi - lo + 1)); }

typedef struct { char t[8][64]; int n; int label; } opts_t;

/* mescola le opzioni (la prima e' la corretta) e fissa label */
static void shuffle(opts_t* o)
{
    char right[64];
    snprintf(right, sizeof right, "%s", o->t[0]);
    for (int i = o->n - 1; i > 0; i--) {
        int j = (int)(rnd() % (unsigned)(i + 1));
        char tmp[64];
        snprintf(tmp, sizeof tmp, "%s", o->t[i]);
        snprintf(o->t[i], 64, "%s", o->t[j]);
        snprintf(o->t[j], 64, "%s", tmp);
    }
    o->label = 0;
    for (int i = 0; i < o->n; i++) if (strcmp(o->t[i], right) == 0) { o->label = i; break; }
}

static int has(const opts_t* o, const char* s)
{
    for (int i = 0; i < o->n; i++) if (strcmp(o->t[i], s) == 0) return 1;
    return 0;
}

static void emit(const char* ctx, opts_t* o)
{
    shuffle(o);
    printf("{\"context\": \"%s\", \"options\": [", ctx);
    for (int i = 0; i < o->n; i++) printf("%s\"%s\"", i ? ", " : "", o->t[i]);
    printf("], \"label\": %d}\n", o->label);
}

/* distrattori numerici vicini al valore giusto (off-by-one, scambio cifre, +-10) */
static void num_options(opts_t* o, int right)
{
    int want = rint_(3, 5);
    o->n = 0;
    snprintf(o->t[o->n++], 64, "%d", right);
    int cand[6] = { right + 1, right - 1, right + 10, right - 10, right + 2, right - 2 };
    int start = (int)(rnd() % 6);
    for (int k = 0; k < 6 && o->n < want; k++) {
        char b[64];
        snprintf(b, sizeof b, "%d", cand[(start + k) % 6]);
        if (!has(o, b)) snprintf(o->t[o->n++], 64, "%s", b);
    }
}

static void q_add(void)
{
    int a = rint_(12, 89), b = rint_(12, 89);
    char ctx[256]; opts_t o;
    snprintf(ctx, sizeof ctx, "What is %d + %d?", a, b);
    num_options(&o, a + b);
    emit(ctx, &o);
}
static void q_sub(void)
{
    int a = rint_(50, 199), b = rint_(11, 49);
    char ctx[256]; opts_t o;
    snprintf(ctx, sizeof ctx, "What is %d - %d?", a, b);
    num_options(&o, a - b);
    emit(ctx, &o);
}
static void q_mul(void)
{
    int a = rint_(6, 19), b = rint_(6, 19);
    char ctx[256]; opts_t o;
    snprintf(ctx, sizeof ctx, "What is %d times %d?", a, b);
    num_options(&o, a * b);
    emit(ctx, &o);
}

/* il piu' grande tra numeri con cifre scambiate */
static void q_largest(void)
{
    int d[4] = { rint_(1, 9), rint_(0, 9), rint_(0, 9), rint_(1, 9) };
    int v[4], k = 0, big = 0;
    char ctx[256]; opts_t o; o.n = 0;
    int perm[4][4] = { {0,1,2,3}, {0,2,1,3}, {1,0,2,3}, {0,1,3,2} };
    for (int i = 0; i < 4; i++) {
        v[i] = d[perm[i][0]] * 1000 + d[perm[i][1]] * 100 + d[perm[i][2]] * 10 + d[perm[i][3]];
        if (v[i] > v[big]) big = i;
    }
    char b[64];
    snprintf(b, sizeof b, "%d", v[big]);
    snprintf(o.t[o.n++], 64, "%s", b);
    for (int i = 0; i < 4 && o.n < 4; i++) {
        snprintf(b, sizeof b, "%d", v[i]);
        if (!has(&o, b)) snprintf(o.t[o.n++], 64, "%s", b);
    }
    if (o.n < 3) { q_largest(); return; }   /* cifre uguali: rigenera (la ricorsione consuma rnd) */
    (void)k;
    snprintf(ctx, sizeof ctx, "Which of these numbers is the largest?");
    /* i numeri stanno nelle opzioni: il contesto li ripete per renderli leggibili al modello */
    char list[200]; list[0] = 0;
    for (int i = 0; i < o.n; i++) { strcat(list, i ? ", " : ""); strcat(list, o.t[i]); }
    /* ordine di presentazione = quello delle opzioni dopo shuffle: basta il contesto generico */
    emit(ctx, &o);
}

static const char* NAMES[] = { "Anna", "Bruno", "Carla", "Dario", "Elena", "Fabio", "Giulia", "Hugo" };

/* ordinamento transitivo: A>B, B>C -> il piu' basso e' C */
static void q_tall(void)
{
    int i0 = rint_(0, 7), i1, i2;
    do { i1 = rint_(0, 7); } while (i1 == i0);
    do { i2 = rint_(0, 7); } while (i2 == i0 || i2 == i1);
    char ctx[256]; opts_t o; o.n = 0;
    int ask_tallest = (int)(rnd() & 1);
    snprintf(ctx, sizeof ctx, "%s is taller than %s. %s is taller than %s. Who is the %s?",
             NAMES[i0], NAMES[i1], NAMES[i1], NAMES[i2], ask_tallest ? "tallest" : "shortest");
    snprintf(o.t[o.n++], 64, "%s", NAMES[ask_tallest ? i0 : i2]);
    snprintf(o.t[o.n++], 64, "%s", NAMES[i1]);
    snprintf(o.t[o.n++], 64, "%s", NAMES[ask_tallest ? i2 : i0]);
    emit(ctx, &o);
}

static const char* WORDS[] = { "banana", "mississippi", "committee", "bookkeeper", "strawberry",
                               "assassin", "parallel", "balloon", "coffee", "tomorrow" };

static void q_count(void)
{
    const char* w = WORDS[rint_(0, 9)];
    /* lettera piu' frequente tra quelle ripetute: scelta casuale tra le presenti */
    char letter = w[rint_(0, (int)strlen(w) - 1)];
    int cnt = 0;
    for (const char* p = w; *p; p++) if (*p == letter) cnt++;
    char ctx[256]; opts_t o;
    snprintf(ctx, sizeof ctx, "How many times does the letter %c appear in the word %s?", letter, w);
    o.n = 0;
    snprintf(o.t[o.n++], 64, "%d", cnt);
    for (int d = 1; o.n < 4; d++) {
        char b[64];
        snprintf(b, sizeof b, "%d", cnt + d);
        if (!has(&o, b)) snprintf(o.t[o.n++], 64, "%s", b);
        if (cnt - d >= 0 && o.n < 4) {
            snprintf(b, sizeof b, "%d", cnt - d);
            if (!has(&o, b)) snprintf(o.t[o.n++], 64, "%s", b);
        }
    }
    emit(ctx, &o);
}

static void q_seq(void)
{
    int a = rint_(2, 20), step = rint_(3, 9);
    int mult = (int)(rnd() & 1);
    char ctx[256]; opts_t o;
    int v[5], nxt;
    if (mult) { step = rint_(2, 3); v[0] = rint_(1, 5); for (int i = 1; i < 5; i++) v[i] = v[i-1] * step; nxt = v[4] * step; }
    else      { for (int i = 0; i < 5; i++) v[i] = a + i * step; nxt = a + 5 * step; }
    snprintf(ctx, sizeof ctx, "What number comes next in the sequence %d, %d, %d, %d, %d?", v[0], v[1], v[2], v[3], v[4]);
    num_options(&o, nxt);
    emit(ctx, &o);
}

static const char* DAYS[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };

static void q_day(void)
{
    int d = rint_(0, 6), k = rint_(2, 11);
    int fwd = (int)(rnd() & 1);
    int r = fwd ? (d + k) % 7 : ((d - k) % 7 + 7) % 7;
    char ctx[256]; opts_t o; o.n = 0;
    snprintf(ctx, sizeof ctx, "Today is %s. What day was it %d days ago?", DAYS[d], k);
    if (fwd) snprintf(ctx, sizeof ctx, "Today is %s. What day will it be in %d days?", DAYS[d], k);
    snprintf(o.t[o.n++], 64, "%s", DAYS[r]);
    snprintf(o.t[o.n++], 64, "%s", DAYS[(r + 1) % 7]);
    snprintf(o.t[o.n++], 64, "%s", DAYS[(r + 6) % 7]);
    snprintf(o.t[o.n++], 64, "%s", DAYS[(r + 2) % 7]);
    emit(ctx, &o);
}

int main(int argc, char** argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 400;
    g_s = argc > 2 ? (unsigned long long)strtoull(argv[2], NULL, 10) : 1ULL;
    if (n <= 0) n = 400;
    for (int i = 0; i < n; i++) {
        switch (i % 9) {
            case 0: q_add(); break;
            case 1: q_sub(); break;
            case 2: q_mul(); break;
            case 3: q_largest(); break;
            case 4: q_tall(); break;
            case 5: q_count(); break;
            case 6: q_seq(); break;
            case 7: q_day(); break;
            default: q_mul(); break;
        }
    }
    return 0;
}
