/* test_wsp_agent.c — selfcheck di W9: sessione a verbi, budget, rifiuti, trascrizione e REPLAY deterministico. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_agent.h"
#include "llmwsp_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int fwd_calls = 0;
static int fake_forward(void *ctx, const char *args, char *out, size_t cap) { (void)ctx; snprintf(out, cap, "forward#%d(%s) -> token 42", ++fwd_calls, args); return 0; }   /* NON deterministico: dipende dal contatore */

static char *slurp(const char *p) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char *b = malloc((size_t)n + 1); if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); return NULL; } fclose(f); b[n] = 0; return b; }

static void open_model(const char *dir, unsigned seed, llmwsp_store **s, llmwsp_doc **d) {
    char err[300], p[500]; llmwsp_fx_truth tr;
    if (llmwsp_fixture_compile("induction", dir, seed, &tr, err, sizeof err) < 0 || llmwsp_store_open(dir, s, err, sizeof err) < 0) { fails++; return; }
    snprintf(p, sizeof p, "%s/config.json", dir); if (llmwsp_doc_build(*s, p, d, err, sizeof err) < 0) fails++;
}

int main(void) {
    char dir[] = "/tmp/wsp_ag_XXXXXX", dir2[] = "/tmp/wsp_ag2_XXXXXX", p[500], tp[500];
    if (!mkdtemp(dir) || !mkdtemp(dir2)) return 1;
    llmwsp_store *s, *s2; llmwsp_doc *d, *d2;
    open_model(dir, 3, &s, &d); open_model(dir2, 4, &s2, &d2);
    llmwsp_budget b = {3, 1};
    llmwsp_session *x = llmwsp_session_new(s, d, dir, b, fake_forward, NULL);
    snprintf(tp, sizeof tp, "%s/transcript.jsonl", dir);
    CHECK(llmwsp_session_transcript_open(x, tp) == 0, "transcript");
    char out[8192]; int rc;
    #define RUN(cmd) (rc = llmwsp_session_exec(x, cmd, out, sizeof out))
    RUN("NORMS 0"); CHECK(rc == 0 && strstr(out, "norme"), "NORMS: %s", out);
    RUN("SPECTRUM 1 0 ov"); CHECK(rc == 0 && strstr(out, "copy_sum_ratio=1"), "SPECTRUM induction L1H0: %s", out);
    RUN("COMPOSE 0 0 1 0 k"); CHECK(rc == 0 && strstr(out, "PK=4"), "COMPOSE: %s", out);
    RUN("DEFINE d1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1"); CHECK(rc == 0 && strstr(out, "16 valori"), "DEFINE: %s", out);
    RUN("SCALE layers.0.attn.o.heads.1 2.0"); CHECK(rc == 0 && strstr(out, "toccati="), "SCALE: %s", out);
    RUN("PROJECTOUT layers.1.attn.o.heads.2 0 1 16 d1"); CHECK(rc == 0 && strstr(out, "ok"), "PROJECTOUT: %s", out);
    RUN("CHECKPOINT dopo-due"); CHECK(rc == 0, "CHECKPOINT");
    RUN("ZERO layers.0.attn.o.heads.3"); CHECK(rc == 0, "ZERO");
    RUN("ZERO layers.0.attn.o.heads.2"); CHECK(rc == 1 && strstr(out, "budget di edit esaurito"), "budget edit: rc=%d %s", rc, out);
    RUN("ROLLBACK dopo-due"); CHECK(rc == 0 && strstr(out, "pos=2"), "ROLLBACK: %s", out);
    RUN("FORWARD 151644,872 8"); CHECK(rc == 0 && strstr(out, "forward#1"), "FORWARD: %s", out);
    RUN("FORWARD 1,2 4"); CHECK(rc == 1 && strstr(out, "budget di forward esaurito"), "budget forward");
    RUN("SHELL ls /"); CHECK(rc == 1 && strstr(out, "verbo non ammesso"), "verbo vietato: %s", out);
    RUN("HTTP http://example.org"); CHECK(rc == 1, "rete vietata");
    { llmwsp_budget big = {10, 10}; llmwsp_session *e = llmwsp_session_new(s, d, dir, big, NULL, NULL); char o3[1024];
      CHECK(llmwsp_session_exec(e, "SCALE layers.9.attn.o.heads.1 2.0", o3, sizeof o3) == -1 && strstr(o3, "ERRORE"), "indirizzo ignoto: %s", o3);
      CHECK(llmwsp_session_exec(e, "SET layers.0.attn.o.heads.1 mancante", o3, sizeof o3) == -1, "vettore non definito");
      CHECK(llmwsp_session_exec(e, "FORWARD 1", o3, sizeof o3) == -1 && strstr(o3, "nessun motore"), "forward senza motore");
      CHECK(llmwsp_session_edits_used(e) == 0, "gli errori non consumano budget"); llmwsp_session_free(e); }
    RUN("SUMMARY"); CHECK(rc == 0 && strstr(out, "TOTALE"), "SUMMARY: %s", out);
    RUN("SHA model.layers.0.self_attn.o_proj.weight"); CHECK(rc == 0 && strlen(out) == 64, "SHA");
    CHECK(llmwsp_session_edits_used(x) == 3 && llmwsp_session_forwards_used(x) == 1, "contatori: %d %d", llmwsp_session_edits_used(x), llmwsp_session_forwards_used(x));
    char sha_final[65]; llmwsp_ov_tensor_sha256(llmwsp_session_overlay(x), "model.layers.1.self_attn.o_proj.weight", sha_final);
    llmwsp_session_transcript_close(x);
    snprintf(p, sizeof p, "%s/run.json", dir); CHECK(llmwsp_session_write_run(x, p) == 0, "run.json");
    { char *r = slurp(p); CHECK(r && strstr(r, "\"tool_free\":true") && strstr(r, "\"SCALE\"") && strstr(r, "\"max_edits\":3") && !strstr(r, "SHELL"), "run.json: %s", r ? r : ""); free(r); }

    /* REPLAY su sessione nuova: stessi output (FORWARD riusato dalla trascrizione, anche se il callback è cambiato) */
    fwd_calls = 100;
    llmwsp_session *y = llmwsp_session_new(s, d, dir, b, fake_forward, NULL);
    int mm = -1; char err[300];
    CHECK(llmwsp_replay(y, tp, &mm, err, sizeof err) == 0 && mm == 0, "replay: %s", err);
    CHECK(llmwsp_session_edits_used(y) == 3 && llmwsp_session_forwards_used(y) == 1, "contatori dopo replay");
    char sha2[65]; llmwsp_ov_tensor_sha256(llmwsp_session_overlay(y), "model.layers.1.self_attn.o_proj.weight", sha2);
    CHECK(!strcmp(sha_final, sha2), "stato finale del modello dopo replay diverso");
    llmwsp_session_free(y);

    /* trascrizione manomessa: un output alterato deve essere rilevato */
    { char *t = slurp(tp); char *q = strstr(t, "copy_sum_ratio=1"); CHECK(q != NULL, "pattern");
      if (q) { q[16] = '9'; FILE *f = fopen(tp, "wb"); fputs(t, f); fclose(f); }
      free(t);
      llmwsp_session *z = llmwsp_session_new(s, d, dir, b, fake_forward, NULL);
      CHECK(llmwsp_replay(z, tp, &mm, err, sizeof err) == -1 && mm == 2, "manomissione: mm=%d %s", mm, err);
      llmwsp_session_free(z); }

    /* replay su un ALTRO modello: divergenza */
    { llmwsp_session_transcript_open(x, tp); llmwsp_session_free(x); x = NULL; }
    { llmwsp_session *w = llmwsp_session_new(s, d, dir, b, fake_forward, NULL); llmwsp_session_transcript_open(w, tp);
      char o2[4096]; llmwsp_session_exec(w, "SPECTRUM 1 0 ov", o2, sizeof o2); llmwsp_session_exec(w, "NORMS 0", o2, sizeof o2); llmwsp_session_free(w);
      llmwsp_session *v = llmwsp_session_new(s2, d2, dir2, b, fake_forward, NULL);
      CHECK(llmwsp_replay(v, tp, &mm, err, sizeof err) == -1, "modello diverso: il replay deve divergere");
      llmwsp_session_free(v); }
    CHECK(llmwsp_replay(NULL == NULL ? llmwsp_session_new(s, d, dir, b, NULL, NULL) : NULL, "/tmp/inesistente_wsp.jsonl", &mm, err, sizeof err) == -2, "trascrizione assente");

    llmwsp_doc_free(d); llmwsp_store_close(s); llmwsp_doc_free(d2); llmwsp_store_close(s2);
    printf(fails ? "WSP AGENT: %d FALLITI\n" : "WSP AGENT: OK\n", fails);
    return fails != 0;
}
