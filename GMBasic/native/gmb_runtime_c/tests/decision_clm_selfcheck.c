/* decision_clm_selfcheck.c — provider `clm` (JEV/SystemOne): prosa, wire, head, punteggio.
 * Model-free: nessun encoder, nessuna rete. Gli oracoli sono calcolati A MANO (analitici),
 * non rieseguendo l'algoritmo. Exit 0 = PASS. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gmb_ai_decide.h"
#include "gmb_decision_clm.h"
#include "gmb_st.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)
#define NEAR(a, b, tol) (fabs((double)(a) - (double)(b)) <= (tol))

static void check_str(const char* got, const char* want, const char* msg)
{
    if (!got || strcmp(got, want) != 0) {
        printf("FAIL: %s\n  atteso: [%s]\n  ottenuto: [%s]\n", msg, want, got ? got : "(null)");
        fails++;
    }
}

/* ── scrittura head sintetiche ───────────────────────────────────────────────────────────── */
static void add(GstWriter* w, const char* pre, const char* name, uint32_t nd, uint64_t d0, uint64_t d1,
                const float* data)
{
    char full[160]; uint64_t sh[2] = { d0, d1 };
    snprintf(full, sizeof full, "%s%s", pre, name);
    size_t n = (size_t)d0 * (nd > 1 ? (size_t)d1 : 1);
    gst_writer_add(w, full, "F32", nd, sh, data, n * sizeof(float));
}

/* Head 2x2: inp=I, out=I, bias 0; con `swap_hidden` aggiunge hidden.0 = [[0,1],[1,0]]. */
static void write_head(const char* path, int swap_hidden, float logit_scale)
{
    static const float I[4] = { 1, 0, 0, 1 }, S[4] = { 0, 1, 1, 0 }, Z[2] = { 0, 0 };
    GstWriter* w = NULL;
    gst_writer_create(&w, path);
    const char* pre[2] = { "state_head.", "action_head." };
    for (int k = 0; k < 2; k++) {
        add(w, pre[k], "inp.weight", 2, 2, 2, I);
        add(w, pre[k], "inp.bias", 1, 2, 0, Z);
        if (swap_hidden) {
            add(w, pre[k], "hidden.0.weight", 2, 2, 2, S);
            add(w, pre[k], "hidden.0.bias", 1, 2, 0, Z);
        }
        add(w, pre[k], "out.weight", 2, 2, 2, I);
        add(w, pre[k], "out.bias", 1, 2, 0, Z);
    }
    float ls = logit_scale;
    uint64_t one = 1;
    gst_writer_add(w, "logit_scale", "F32", 1, &one, &ls, sizeof ls);
    gst_writer_finish(w);
}

static void write_text(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

/* Encoder finto: "auth"/"Login" → (1,0), altrimenti (0,1). */
static int fake_embed(const char* text, float* out, int dim, void* ctx)
{
    (void)ctx;
    if (dim != 2) return -1;
    int a = strstr(text, "auth") || strstr(text, "Login");
    out[0] = a ? 1.0f : 0.0f; out[1] = a ? 0.0f : 1.0f;
    return 0;
}
static int bad_embed(const char* t, float* o, int d, void* c) { (void)t; (void)o; (void)d; (void)c; return -1; }

int main(void)
{
    char dir[] = "/tmp/clm_selfcheck_XXXXXX";
    if (!mkdtemp(dir)) { printf("FAIL: mkdtemp\n"); return 1; }
    char hp[256], hj[256];

    /* ── prosa ── */
    char* t = gmb_clm_to_text("{\"ticket\":\"Login fails\",\"tags\":[\"a\",\"b\"],"
                              "\"user\":{\"name\":\"Ann\",\"age\":30},\"ok\":true}");
    check_str(t, "ticket: Login fails\n\ntags:\n  - a\n  - b\n\nuser:\n  name: Ann\n  age: 30\n\nok: true",
              "to_text oggetto annidato (riga vuota fra i campi di primo livello)");
    free(t);
    t = gmb_clm_to_text("{\"a\":null,\"b\":[],\"c\":{}}");
    check_str(t, "a: \n\nb: \n\nc: ", "to_text null e contenitori vuoti = stringa vuota");
    free(t);
    t = gmb_clm_to_text("[\"x\",{\"k\":1}]");
    check_str(t, "- x\n-\n  k: 1", "to_text array con oggetto");
    free(t);
    t = gmb_clm_to_text("Il cliente e' irritato.");
    check_str(t, "Il cliente e' irritato.", "to_text prosa invariata");
    free(t);
    t = gmb_clm_to_text("{\"a\":");
    check_str(t, "{\"a\":", "to_text JSON malformato invariato");
    free(t);
    t = gmb_clm_to_text("{\"s\":\"riga1\\nriga2 \\\"q\\\"\"}");
    check_str(t, "s: riga1\nriga2 \"q\"", "to_text decodifica escape");
    free(t);
    t = gmb_clm_state_text("{\"a\":1}", "Quale team?");
    check_str(t, "a: 1\n\nQuale team?", "state_text contesto+domanda");
    free(t);
    t = gmb_clm_state_text("  contesto \n", "\n Domanda? ");
    check_str(t, "contesto\n\nDomanda?", "state_text: strip di entrambe le parti");
    free(t);
    t = gmb_clm_state_text("solo stato", "");
    check_str(t, "solo stato", "state_text senza domanda");
    free(t);
    t = gmb_clm_state_text("", "solo domanda");
    check_str(t, "solo domanda", "state_text senza contesto");
    free(t);

    /* ── richiesta ── */
    gmb_decision_option_t ch[2] = {
        { "auth", "Authentication", -1, 0, 1 }, { "b", "b", -1, 0, 1 } };
    char* rq = gmb_clm_build_request("hello", "CHOICE:auth|b", ch, 2, "");
    check_str(rq, "{\"state\":\"hello\",\"questions\":{\"q\":{\"type\":\"choice\",\"instructions\":\"\","
                  "\"criteria\":{\"auth\":\"Authentication\",\"b\":\"\"}}}}", "richiesta choice");
    free(rq);
    rq = gmb_clm_build_request("{\"t\":\"Login\"}", "CHOICE", ch, 2, "Chi?");
    check_str(rq, "{\"state\":{\"t\":\"Login\"},\"questions\":{\"q\":{\"type\":\"choice\","
                  "\"instructions\":\"Chi?\",\"criteria\":{\"auth\":\"Authentication\",\"b\":\"\"}}}}",
              "richiesta: stato JSON incorporato come JSON");
    free(rq);
    rq = gmb_clm_build_request("a \"q\"\n", "NOUL", gmb_decision_slots_noul, 2, "E' vero?");
    check_str(rq, "{\"state\":\"a \\\"q\\\"\\n\",\"questions\":{\"q\":{\"type\":\"noul\","
                  "\"instructions\":\"E' vero?\"}}}", "richiesta noul + escape");
    free(rq);
    gmb_decision_option_t sc[2] = { { "0", "poco", -1, 0, 1 }, { "1", "molto", -1, 0, 1 } };
    rq = gmb_clm_build_request("s", "SCORE", sc, 2, "Quanto?");
    check_str(rq, "{\"state\":\"s\",\"questions\":{\"q\":{\"type\":\"score\",\"instructions\":\"Quanto?\","
                  "\"criteria\":[\"poco\",\"molto\"]}}}", "richiesta score");
    free(rq);

    /* ── risposta ── */
    gmb_decision_result_t r; char err[256];
    CHECK(gmb_clm_parse_response("{\"type\":\"choice\",\"choice\":\"auth\",\"confidence\":0.6,"
          "\"probabilities\":{\"auth\":0.8,\"b\":0.2}}", GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == 1, "choice ok");
    CHECK(strcmp(r.option_id, "auth") == 0 && NEAR(r.probability, 0.8, 1e-9) && NEAR(r.confidence, 0.6, 1e-9)
          && r.has_confidence, "choice campi");
    /* Forma REALE del server: {"model":…,"answers":{"<qid>":{…}},"usage":…} */
    CHECK(gmb_clm_parse_response("{\"model\":\"clm-latest\",\"answers\":{\"q\":{\"type\":\"choice\",\"choice\":\"auth\","
          "\"confidence\":0.5,\"probabilities\":{\"auth\":0.75,\"b\":0.25}}},\"usage\":{\"billing_units\":1}}",
          GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == 1 && strcmp(r.option_id, "auth") == 0
          && NEAR(r.probability, 0.75, 1e-9), "risposta reale sotto 'answers'");
    CHECK(gmb_clm_parse_response("{\"q\":{\"type\":\"choice\",\"choice\":\"b\","
          "\"probabilities\":{\"auth\":0.3,\"b\":0.7}}}", GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == 1, "choice sotto id");
    CHECK(strcmp(r.option_id, "b") == 0 && NEAR(r.confidence, 0.4, 1e-6), "confidence calcolata = 0.7-0.3");
    CHECK(gmb_clm_parse_response("{\"type\":\"noul\",\"noul\":0.83}", GMB_DECISION_NOUL, gmb_decision_slots_noul, 2,
          &r, err, sizeof err) == 1 && strcmp(r.option_id, "true") == 0 && NEAR(r.probability, 0.83, 1e-9)
          && NEAR(r.confidence, 0.66, 1e-6), "noul vero");
    CHECK(gmb_clm_parse_response("{\"type\":\"noul\",\"noul\":0.1}", GMB_DECISION_NOUL, gmb_decision_slots_noul, 2,
          &r, err, sizeof err) == 1 && strcmp(r.option_id, "false") == 0 && NEAR(r.probability, 0.9, 1e-9), "noul falso");
    CHECK(gmb_clm_parse_response("{\"type\":\"score\",\"score\":0.7,\"legend\":{\"0\":\"a\",\"1\":\"b\"},"
          "\"probabilities\":{\"0\":0.3,\"1\":0.7}}", GMB_DECISION_SCORE, sc, 2, &r, err, sizeof err) == 1
          && strcmp(r.option_id, "1") == 0 && NEAR(r.probability, 0.7, 1e-9), "score argmax");
    CHECK(gmb_clm_parse_response("{\"type\":\"noul\",\"noul\":0.5}", GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == -1
          && strstr(err, "incoerente"), "errore: type incoerente");
    CHECK(gmb_clm_parse_response("{\"type\":\"choice\",\"choice\":\"zzz\",\"probabilities\":{\"zzz\":1}}",
          GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == -1 && strstr(err, "non nel dominio"), "errore: option fuori dominio");
    CHECK(gmb_clm_parse_response("{\"type\":\"choice\",\"choice\":\"auth\"}", GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == -1
          && strstr(err, "probabilities"), "errore: senza probabilities");
    CHECK(gmb_clm_parse_response("", GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == -1, "errore: vuota");
    CHECK(gmb_clm_parse_response("{\"detail\":\"unauthorized\"}", GMB_DECISION_CHOICE, ch, 2, &r, err, sizeof err) == -1
          && strstr(err, "type"), "errore: corpo senza risposta");

    /* ── confidence ── */
    float p3[3] = { 0.5f, 0.3f, 0.2f };
    CHECK(NEAR(gmb_clm_confidence(p3, 3), 0.25, 1e-6), "confidence 0.5-0.25");
    CHECK(NEAR(gmb_clm_confidence(p3, 1), 1.0, 1e-9), "confidence <2 opzioni = 1");

    /* ── head identità relu, scale = e^ln2 = 2 ── */
    snprintf(hp, sizeof hp, "%s/h1.safetensors", dir);
    write_head(hp, 0, (float)log(2.0));
    write_text((snprintf(hj, sizeof hj, "%s/h1.json", dir), hj), "{\"activation\":\"relu\"}");
    gmb_clm_head_t* h = gmb_clm_head_load(hp, err, sizeof err);
    CHECK(h != NULL, "caricamento head");
    if (h) {
        CHECK(gmb_clm_head_hidden_size(h) == 2 && gmb_clm_head_projection_dim(h) == 2, "dimensioni dedotte");
        float st[2] = { 1, 0 }, cs[4] = { 1, 0, 0, 1 }, pr[2];
        CHECK(gmb_clm_score(h, st, cs, 2, pr) == 0, "score ok");
        double e2 = exp(2.0);
        CHECK(NEAR(pr[0], e2 / (e2 + 1.0), 1e-5) && NEAR(pr[0] + pr[1], 1.0, 1e-6), "softmax(2,0) = 0.8808");

        gmb_decision_result_t lr;
        CHECK(gmb_clm_decide_local(h, fake_embed, NULL, "Login fails after reset", "", GMB_DECISION_CHOICE,
              (gmb_decision_option_t[2]){ { "auth", "auth", -1, 0, 1 }, { "billing", "billing", -1, 0, 1 } }, 2,
              &lr, err, sizeof err) == 1, "decide_local ok");
        CHECK(strcmp(lr.option_id, "auth") == 0 && NEAR(lr.probability, e2 / (e2 + 1.0), 1e-5)
              && NEAR(lr.confidence, 2.0 * e2 / (e2 + 1.0) - 1.0, 1e-5), "decide_local sceglie auth, conf = p0-p1");
        CHECK(gmb_clm_decide_local(h, bad_embed, NULL, "x", "", GMB_DECISION_CHOICE, ch, 2, &lr, err, sizeof err) == -1
              && strstr(err, "encoder"), "decide_local: errore encoder esplicito");
        gmb_clm_head_free(h);
    }

    /* ── head con blocco hidden (scambio) con/senza residuo: ampiezza 1 ── */
    snprintf(hp, sizeof hp, "%s/h2.safetensors", dir);
    write_head(hp, 1, 0.0f);                                  /* scale = e^0 = 1 */
    snprintf(hj, sizeof hj, "%s/h2.json", dir);
    write_text(hj, "{\"activation\":\"relu\",\"residual\":true}");
    h = gmb_clm_head_load(hp, err, sizeof err);
    CHECK(h != NULL, "head con hidden");
    if (h) {
        float st[2] = { 1, 0 }, cs[4] = { 1, 1, 1, 0 }, pr[2];
        gmb_clm_score(h, st, cs, 2, pr);                      /* stato → (1,1)/√2 ; cand: (1,1)→(2,2)... */
        /* cand (1,1): a=(1,1), hidden swap → (1,1), residuo → (2,2) → dir (1,1) → cos=1.
         * cand (1,0): a=(1,0) → residuo (1,1) → dir (1,1)/√2 → cos=1 (stessa direzione dello stato). */
        CHECK(NEAR(pr[0], 0.5, 1e-5), "residuo: entrambe collineari con lo stato → 0.5");
        gmb_clm_head_free(h);
    }
    write_text(hj, "{\"activation\":\"relu\",\"residual\":false}");
    h = gmb_clm_head_load(hp, err, sizeof err);
    if (h) {
        float st[2] = { 1, 0 }, cs[4] = { 1, 1, 1, 0 }, pr[2];
        gmb_clm_score(h, st, cs, 2, pr);
        /* senza residuo: stato → a=(1,0) → swap → (0,1): z_s=(0,1).
         * cand (1,1) → swap (1,1) → z=(1,1)/√2 → cos=0.70711 ; cand (1,0) → (0,1) → cos=1. */
        double d = 1.0 - 0.70710678;
        CHECK(NEAR(pr[0], 1.0 / (1.0 + exp(d)), 1e-4), "senza residuo: softmax(0.7071,1.0)");
        gmb_clm_head_free(h);
    }

    /* ── GELU esatta: x=(1,-1) → (0.841345,-0.158655), scale 1 ── */
    snprintf(hp, sizeof hp, "%s/h3.safetensors", dir);
    write_head(hp, 0, 0.0f);
    h = gmb_clm_head_load(hp, err, sizeof err);                 /* nessun sidecar → gelu */
    CHECK(h != NULL, "head gelu");
    if (h) {
        float st[2] = { 1, -1 }, cs[4] = { 1, 0, 0, 1 }, pr[2];
        gmb_clm_score(h, st, cs, 2, pr);
        /* z_c = gelu(c) normalizzato: (1,0)→gelu(1)=.841345,gelu(0)=0 → (1,0) ; (0,1) → (0,1).
         * cos(z_s,(1,0)) = .982679 ; cos(z_s,(0,1)) = -.185304 → p0 = 1/(1+e^-1.167983) = 0.762775 */
        CHECK(NEAR(pr[0], 0.762775, 1e-4), "gelu esatta: p0 = 0.7628");
        gmb_clm_head_free(h);
    }

    /* ── errori di caricamento espliciti ── */
    CHECK(gmb_clm_head_load("/nonexistent/h.safetensors", err, sizeof err) == NULL && err[0], "head assente = errore");

    printf(fails ? "decision_clm_selfcheck: %d FAIL\n" : "decision_clm_selfcheck: PASS\n", fails);
    return fails ? 1 : 0;
}
