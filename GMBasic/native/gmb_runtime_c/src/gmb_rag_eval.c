/* gmb_rag_eval.c — metriche di valutazione RAG (todo Rag-Eval-Gap-Analysis, Fase A: R1+R2).
 * §0bis: runtime C condiviso — fonte unica per VM/C e C99 (C99Native); il canone
 * gestito (IL/PCodeVM) è AiVectorRuntime.cs e DEVE rimanere paritario (stesse formule,
 * stessa tokenizzazione).
 *
 * R1 — quartetto retrieval (rilevanza binaria, formule IR standard):
 *   gmb_rageval_precision_at(retrieved, ideal, k) = hit nei primi k / k
 *   gmb_rageval_recall_at(retrieved, ideal, k)    = hit nei primi k / |ideal| (k<=0: tutta la lista)
 *   gmb_rageval_mrr(retrieved, ideal)             = 1/rango del primo hit
 *   gmb_rageval_ndcg_at(retrieved, ideal, k)      = DCG/IDCG binario, sconto log2(i+2)
 * R2 — giudizi generazione tier-1 (euristiche deterministiche su token ASCII [a-z0-9]+ lowercase):
 *   gmb_rageval_faithfulness(contesto, risposta)  = quota token risposta radicati nel contesto
 *   gmb_rageval_hallucination(contesto, risposta) = 1 - faithfulness
 *   gmb_rageval_relevance(domanda, risposta)      = quota token domanda presenti nella risposta
 *   gmb_rageval_completeness(domanda, risposta)   = idem sui soli token di contenuto
 *       (>= 3 char, non stopwords); ricade su relevance se non ci sono token di contenuto.
 * ID: CSV separati da virgola, trim, vuoti scartati, confronto case-insensitive.
 * gmb_rageval_selfcheck(): asserzioni handbook esatte per costruzione; 0 = OK, -1 = KO. */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define RGE_MAX_TOKENS 512

/* --- util --- */
static int rge_is_word(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int rge_casecmp(const char* a, const char* b)
{
    while (*a && *b) {
        char ca = (char)((*a >= 'A' && *a <= 'Z') ? *a + 32 : *a);
        char cb = (char)((*b >= 'A' && *b <= 'Z') ? *b + 32 : *b);
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* Analizza una lista CSV: item[i] punta dentro buf (mutabile); ritorna il numero di item. */
static int rge_split_csv(char* buf, char** items, int max)
{
    int n = 0;
    char* p = buf;
    if (!buf) return 0;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) break;
        char* start = p;
        while (*p && *p != ',') p++;
        char* end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
        int stopped_at_comma = (*p == ','); /* catturato PRIMA dell'eventuale NUL */
        if (end > start && n < max) {
            *end = '\0';
            items[n++] = start;
        }
        if (stopped_at_comma) p++; /* salta la virgola originale (anche se sovrascritta) */
    }
    return n;
}

static const char* RGE_STOPWORDS[] = {
    "the", "a", "an", "of", "to", "in", "on", "at", "is", "are", "was", "were", "be",
    "and", "or", "for", "with", "by", "from", "it", "its", "this", "that",
    "what", "who", "how", "why", "when", "where", "which",
};
#define RGE_STOPWORD_N (int)(sizeof(RGE_STOPWORDS) / sizeof(RGE_STOPWORDS[0]))

static int rge_is_stopword(const char* t)
{
    for (int i = 0; i < RGE_STOPWORD_N; i++)
        if (rge_casecmp(t, RGE_STOPWORDS[i]) == 0) return 1;
    return 0;
}

/* Tokenizza buf (mutabile) in token lowercase NUL-terminati; token[i] punta dentro buf. */
static int rge_split_tokens(char* buf, char** tokens, int max)
{
    int n = 0;
    char* p = buf;
    if (!buf) return 0;
    while (*p) {
        if (rge_is_word(*p)) {
            char* start = p;
            while (*p && rge_is_word(*p)) {
                if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
                p++;
            }
            char term = *p; /* separatore originale o '\0' di fine stringa */
            *p = '\0';      /* termina il token al separatore */
            if (n < max) tokens[n++] = start;
            if (term == '\0') break;
            p++;            /* salta il separatore */
        } else {
            p++;
        }
    }
    return n;
}

/* Quota di token di src_buf presenti nell'insieme dei token di set_buf (0 se src vuota). */
static double rge_overlap_share(char* src_buf, char* set_buf)
{
    char* src[RGE_MAX_TOKENS];
    char* set[RGE_MAX_TOKENS];
    int ns = rge_split_tokens(src_buf, src, RGE_MAX_TOKENS);
    if (ns == 0) return 0.0;
    int ks = rge_split_tokens(set_buf, set, RGE_MAX_TOKENS);
    int hits = 0;
    for (int i = 0; i < ns; i++)
        for (int j = 0; j < ks; j++)
            if (rge_casecmp(src[i], set[j]) == 0) { hits++; break; }
    return (double)hits / (double)ns;
}

static char* rge_strdup_or_null(const char* s)
{
    return (s && *s) ? strdup(s) : NULL;
}

/* --- L7 Training: BLEU + ROUGE-L (variante GMBasic documentata, NON sacrebleu) ---
 * Token = run [a-z0-9]+ lowercase (stessa tokenizzazione di R2 in questo file, che il C#
 * deve replicare per parita'), case-insensitive, singolo riferimento, niente smoothing.
 * BLEU-4 standard non smussato: BP * exp(media(log p_n)); p_n con clipping (ogni n-gramma
 * del riferimento usato una volta); 0 se un p_n e' 0 o se hyp e' vuota.
 * BP = 1 se hyp_len >= ref_len, altrimenti exp(1 - ref_len/hyp_len).
 * ROUGE-L: LCS su token, R=lcs/m, P=lcs/n, F=2PR/(P+R) (beta=1); 0 se un lato e' vuoto
 * (1.0 se entrambi vuoti). Cap RGE_MAX_TOKENS per lato (troncamento oltre). */

/* N-grammi di hyp[0..nh) che compaiono in ref[0..nr), ognuno usato una volta (clipping).
 * In *total il numero di n-grammi dell'ipotesi. Ritorna i match. */
static int rge_ngram_clipped(char** hyp, int nh, char** ref, int nr, int n, int* total)
{
    int th = (nh >= n) ? nh - n + 1 : 0;
    int tr = (nr >= n) ? nr - n + 1 : 0;
    *total = th;
    if (th == 0 || tr == 0) return 0;
    char* used = (char*)calloc((size_t)tr, 1);
    if (!used) return 0;
    int match = 0;
    for (int i = 0; i < th; i++) {
        for (int r = 0; r < tr; r++) {
            if (used[r]) continue;
            int eq = 1;
            for (int k = 0; k < n; k++)
                if (strcmp(hyp[i + k], ref[r + k]) != 0) { eq = 0; break; }
            if (eq) { used[r] = 1; match++; break; }
        }
    }
    free(used);
    return match;
}

double gmb_bleu_score(const char* ref_text, const char* hyp_text)
{
    if (!ref_text || !*ref_text || !hyp_text || !*hyp_text) return 0.0;
    char* rb = rge_strdup_or_null(ref_text);
    char* hb = rge_strdup_or_null(hyp_text);
    if (!rb || !hb) { free(rb); free(hb); return 0.0; }
    char* rtoks[RGE_MAX_TOKENS];
    char* htoks[RGE_MAX_TOKENS];
    int nr = rge_split_tokens(rb, rtoks, RGE_MAX_TOKENS);
    int nh = rge_split_tokens(hb, htoks, RGE_MAX_TOKENS);
    double logsum = 0.0;
    for (int n = 1; n <= 4; n++) {
        int total = 0;
        int clip = rge_ngram_clipped(htoks, nh, rtoks, nr, n, &total);
        if (total == 0 || clip == 0) { free(rb); free(hb); return 0.0; }
        logsum += log((double)clip / (double)total);
    }
    double bp = (nh >= nr) ? 1.0 : exp(1.0 - (double)nr / (double)nh);
    free(rb);
    free(hb);
    return bp * exp(logsum / 4.0);
}

double gmb_rouge_l(const char* ref_text, const char* hyp_text)
{
    /* Bordi espliciti PRIMA di rge_strdup_or_null (che mappa "" su NULL): due vuoti -> 1.0,
     * un vuoto solo -> 0.0. NULL conta come vuoto (input degenere, mai errore). */
    int re = !ref_text || !*ref_text;
    int he = !hyp_text || !*hyp_text;
    if (re && he) return 1.0;
    if (re || he) return 0.0;
    char* rb = rge_strdup_or_null(ref_text);
    char* hb = rge_strdup_or_null(hyp_text);
    if (!rb || !hb) { free(rb); free(hb); return 0.0; }
    char* rtoks[RGE_MAX_TOKENS];
    char* htoks[RGE_MAX_TOKENS];
    int nr = rge_split_tokens(rb, rtoks, RGE_MAX_TOKENS);
    int nh = rge_split_tokens(hb, htoks, RGE_MAX_TOKENS);
    double f = 0.0;
    if (nr == 0 && nh == 0) {
        f = 1.0;
    } else if (nr > 0 && nh > 0) {
        int* prev = (int*)calloc((size_t)nh + 1, sizeof(int));
        int* cur = (int*)calloc((size_t)nh + 1, sizeof(int));
        if (prev && cur) {
            for (int i = 1; i <= nr; i++) {
                for (int j = 1; j <= nh; j++) {
                    if (strcmp(rtoks[i - 1], htoks[j - 1]) == 0) cur[j] = prev[j - 1] + 1;
                    else cur[j] = (prev[j] >= cur[j - 1]) ? prev[j] : cur[j - 1];
                }
                int* tmp = prev; prev = cur; cur = tmp;
            }
            int lcs = prev[nh];
            double r = (double)lcs / (double)nr;
            double p = (double)lcs / (double)nh;
            if (r + p > 0.0) f = 2.0 * r * p / (r + p);
        }
        free(prev);
        free(cur);
    }
    free(rb);
    free(hb);
    return f;
}

/* --- R1: quartetto retrieval --- */
double gmb_rageval_precision_at(const char* retrieved, const char* ideal, int k)
{
    char* rbuf = rge_strdup_or_null(retrieved);
    char* ibuf = rge_strdup_or_null(ideal);
    double rc = 0.0;
    if (k <= 0 || !rbuf) { free(rbuf); free(ibuf); return 0.0; }
    {
        char* a[RGE_MAX_TOKENS];
        char* r[RGE_MAX_TOKENS];
        int na = rge_split_csv(rbuf, a, RGE_MAX_TOKENS);
        int nr = ibuf ? rge_split_csv(ibuf, r, RGE_MAX_TOKENS) : 0;
        int cnt = (na < k) ? na : k;
        int nh = 0;
        for (int i = 0; i < cnt; i++)
            for (int j = 0; j < nr; j++)
                if (rge_casecmp(a[i], r[j]) == 0) { nh++; break; }
        rc = cnt > 0 ? (double)nh / (double)cnt : 0.0;
    }
    free(rbuf); free(ibuf);
    return rc;
}

double gmb_rageval_recall_at(const char* retrieved, const char* ideal, int k)
{
    char* rbuf = rge_strdup_or_null(retrieved);
    char* ibuf = rge_strdup_or_null(ideal);
    double rc = 0.0;
    if (!rbuf || !ibuf) { free(rbuf); free(ibuf); return 0.0; }
    {
        char* a[RGE_MAX_TOKENS];
        char* r[RGE_MAX_TOKENS];
        int na = rge_split_csv(rbuf, a, RGE_MAX_TOKENS);
        int nr = rge_split_csv(ibuf, r, RGE_MAX_TOKENS);
        int cnt = (k > 0) ? ((na < k) ? na : k) : na;
        int nh = 0;
        for (int i = 0; i < cnt; i++)
            for (int j = 0; j < nr; j++)
                if (rge_casecmp(a[i], r[j]) == 0) { nh++; break; }
        rc = nr > 0 ? (double)nh / (double)nr : 0.0;
    }
    free(rbuf); free(ibuf);
    return rc;
}

double gmb_rageval_mrr(const char* retrieved, const char* ideal)
{
    char* rbuf = rge_strdup_or_null(retrieved);
    char* ibuf = rge_strdup_or_null(ideal);
    double rc = 0.0;
    if (rbuf && ibuf) {
        char* a[RGE_MAX_TOKENS];
        char* r[RGE_MAX_TOKENS];
        int na = rge_split_csv(rbuf, a, RGE_MAX_TOKENS);
        int nr = rge_split_csv(ibuf, r, RGE_MAX_TOKENS);
        int done = 0;
        for (int i = 0; i < na && !done; i++)
            for (int j = 0; j < nr; j++)
                if (rge_casecmp(a[i], r[j]) == 0) { rc = 1.0 / (double)(i + 1); done = 1; break; }
    }
    free(rbuf); free(ibuf);
    return rc;
}

double gmb_rageval_ndcg_at(const char* retrieved, const char* ideal, int k)
{
    char* rbuf = rge_strdup_or_null(retrieved);
    char* ibuf = rge_strdup_or_null(ideal);
    double rc = 0.0;
    if (k > 0 && rbuf && ibuf) {
        char* a[RGE_MAX_TOKENS];
        char* r[RGE_MAX_TOKENS];
        char* u[RGE_MAX_TOKENS]; /* ideali deduplicati case-insensitive (canone C#: HashSet) */
        int na = rge_split_csv(rbuf, a, RGE_MAX_TOKENS);
        int nr = rge_split_csv(ibuf, r, RGE_MAX_TOKENS);
        int nu = 0;
        for (int j = 0; j < nr; j++) {
            int dup = 0;
            for (int m = 0; m < nu; m++)
                if (rge_casecmp(r[j], u[m]) == 0) { dup = 1; break; }
            if (!dup) u[nu++] = r[j];
        }
        int n = (na < k) ? na : k;
        if (n > 0) {
            double dcg = 0.0;
            for (int i = 0; i < n; i++) {
                int rel = 0;
                for (int j = 0; j < nu; j++)
                    if (rge_casecmp(a[i], u[j]) == 0) { rel = 1; break; }
                if (rel) dcg += 1.0 / log2((double)(i + 2));
            }
            int ideal_n = (nu < k) ? nu : k;
            double idcg = 0.0;
            for (int i = 0; i < ideal_n; i++) idcg += 1.0 / log2((double)(i + 2));
            rc = idcg > 0.0 ? dcg / idcg : 0.0;
        }
    }
    free(rbuf); free(ibuf);
    return rc;
}

/* --- R2: giudizi generazione tier-1 --- */
double gmb_rageval_faithfulness(const char* context, const char* answer)
{
    char* cbuf = rge_strdup_or_null(context);
    char* abuf = rge_strdup_or_null(answer);
    double rc = abuf ? rge_overlap_share(abuf, cbuf ? cbuf : (char*)"") : 0.0;
    free(cbuf); free(abuf);
    return rc;
}

double gmb_rageval_hallucination(const char* context, const char* answer)
{
    return 1.0 - gmb_rageval_faithfulness(context, answer);
}

double gmb_rageval_relevance(const char* question, const char* answer)
{
    char* qbuf = rge_strdup_or_null(question);
    char* abuf = rge_strdup_or_null(answer);
    double rc = qbuf ? rge_overlap_share(qbuf, abuf ? abuf : (char*)"") : 0.0;
    free(qbuf); free(abuf);
    return rc;
}

double gmb_rageval_completeness(const char* question, const char* answer)
{
    char* qbuf = rge_strdup_or_null(question);
    char* abuf = rge_strdup_or_null(answer);
    double rc = 0.0;
    if (qbuf) {
        char* q[RGE_MAX_TOKENS];
        char* content[RGE_MAX_TOKENS];
        char* a[RGE_MAX_TOKENS];
        int nq = rge_split_tokens(qbuf, q, RGE_MAX_TOKENS);
        int na = rge_split_tokens(abuf ? abuf : (char*)"", a, RGE_MAX_TOKENS);
        /* BUG fix (2026-09-14, beccato dalla parità VM/C, regression test completeness.bas
         * caso "fallback"): qbuf è GIÀ mutato dalla prima tokenizzazione (NUL ai separatori)
         * — ri-tokenizzarlo fermava l'analisi al primo token (rge_split_tokens esce al primo
         * '\0'). I token già pronti vanno riusati, MAI ri-parsare il buffer. */
        int nc = 0;
        for (int i = 0; i < nq; i++)
            if (strlen(q[i]) >= 3 && !rge_is_stopword(q[i])) content[nc++] = q[i];
        int hits = 0;
        int nden = (nc > 0) ? nc : nq; /* fallback: tutti i token della domanda */
        for (int i = 0; i < nden; i++)
            for (int j = 0; j < na; j++)
                if (rge_casecmp(nc > 0 ? content[i] : q[i], a[j]) == 0) { hits++; break; }
        rc = nden > 0 ? (double)hits / (double)nden : 0.0;
    }
    free(qbuf); free(abuf);
    return rc;
}

/* --- selfcheck: valori handbook esatti per costruzione (tol = 0) o entro tolleranza --- */
int gmb_rageval_selfcheck(void)
{
    struct { const char* name; double got; double want; double tol; } cases[] = {
        { "p@2",    gmb_rageval_precision_at("a,b,c,d", "b,d", 2), 0.5, 0.0 },
        { "p@4",    gmb_rageval_precision_at("a,b,c,d", "b,d", 4), 0.5, 0.0 },
        { "r@2",    gmb_rageval_recall_at("a,b,c,d", "b,d", 2), 0.5, 0.0 },
        { "r@all",  gmb_rageval_recall_at("a,b,c,d", "b,d", 0), 1.0, 0.0 },
        { "mrr",    gmb_rageval_mrr("a,b,c,d", "b,d"), 0.5, 0.0 },
        { "mrr-no", gmb_rageval_mrr("x,y", "b,d"), 0.0, 0.0 },
        { "fai",    gmb_rageval_faithfulness("il motore mlx gestisce la memoria", "motore mlx memoria rapido"), 0.75, 0.0 },
        { "hal",    gmb_rageval_hallucination("il motore mlx gestisce la memoria", "motore mlx memoria rapido"), 0.25, 0.0 },
        { "rel",    gmb_rageval_relevance("che cosa gestisce il motore", "il motore gestisce la memoria"), 0.6, 0.0 },
        { "com",    gmb_rageval_completeness("che cosa gestisce il motore", "il motore gestisce la memoria"), 0.5, 0.0 },
        { "ndcg1",  gmb_rageval_ndcg_at("b,d,a,c", "b,d", 4), 1.0, 1e-12 },
        { "ndcg2",  gmb_rageval_ndcg_at("a,b,c,d", "b,d", 4), 0.6509209, 1e-6 }, /* 1.0616063117/1.6309297536 */
        /* L7: oracoli calcolati a mano sotto la tokenizzazione di questo file
         * ([a-z0-9]+ lowercase, case-insensitive). */
        { "bleu-ident", gmb_bleu_score("the cat sat on the mat", "the cat sat on the mat"), 1.0, 0.0 },
        { "bleu-zero",  gmb_bleu_score("the cat is on the mat", "the the the the the the the"), 0.0, 0.0 }, /* p2 senza match */
        { "bleu-bp",    gmb_bleu_score("a b c d e f", "a b c d"), 0.6065306597, 1e-9 }, /* exp(-0.5): p1..p4 = 1, BP<1 */
        { "rouge-ident", gmb_rouge_l("the cat sat on the mat", "the cat sat on the mat"), 1.0, 0.0 },
        { "rouge-413",  gmb_rouge_l("the cat is on the mat", "the the the the the the the"), 0.3076923077, 1e-9 }, /* lcs=2: 4/13 */
        { "rouge-45",   gmb_rouge_l("a b c d e f", "a b c d"), 0.8, 1e-12 }, /* lcs=4: R=2/3 P=1 F=4/5 */
        { "rouge-empty", gmb_rouge_l("", ""), 1.0, 0.0 },
        { "bleu-empty", gmb_bleu_score("hello", ""), 0.0, 0.0 },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (cases[i].got < cases[i].want - cases[i].tol || cases[i].got > cases[i].want + cases[i].tol) {
            fprintf(stderr, "gmb_rageval_selfcheck: %s = %.12f (atteso %.12f +- %g)\n",
                    cases[i].name, cases[i].got, cases[i].want, cases[i].tol);
            return -1;
        }
    }
    return 0;
}
