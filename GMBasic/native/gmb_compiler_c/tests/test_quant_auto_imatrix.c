/* test_quant_auto_imatrix.c — §E4a (Q2-b): CONSUMATORE imatrix dell'allocatore mixed (§E3).
 *
 * Modello F32 sintetico con 4 tensori 2D quantizzabili:
 *   - gate_proj: errore di quantizzazione UNIFORME fra le colonne (nessuna colonna "difficile");
 *   - up_proj:   colonne 0..3 schiacciate dal quantizzatore (accanto a un valore grande che gonfia
 *                lo step del gruppo) -> errore UNIFORMEMENTE BASSO ma CONCENTRATO in poche colonne;
 *   - down_proj/q_proj: lisci.
 * Report imatrix con ENERGIE non uniformi (colonne 0..3 a 1e6, resto 1): il peso cambia solo i
 * tensori il cui errore e' concentrato in quelle colonne (per gli altri la ponderazione si elide
 * col segnale). Verifica:
 *   1) con imatrix la mappa bit per-tensore DIFFERISCE da quella non pesata (il consumatore usa
 *      davvero l'importanza);
 *   2) l'errore TOTALE PESATO dell'allocazione guidata dall'imatrix e' STRETTAMENTE minore di quella
 *      non pesata a pari budget (alloca i bit dove l'energia di attivazione li rende utili);
 *   3) l'attention (`q_proj`, non coperta dal report) resta non pesata e riceve gli stessi bit nelle
 *      due allocazioni (nessun effetto collaterale).
 */
#include "gmb_st.h"
#include "gmb_model_quantize.h"
#include "llmcodec_moe_quant.h"
#include "llmengine_moe_quant_alloc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void mkdir_p(const char* p) {
    char buf[1200]; snprintf(buf, sizeof(buf), "%s", p);
    for (char* q = buf + 1; *q; ++q) if (*q == '/') { *q = 0; mkdir(buf, 0777); *q = '/'; }
    mkdir(buf, 0777);
}

static GstEntry* find(GstEntry* ents, uint32_t ne, const char* name) {
    for (uint32_t i = 0; i < ne; ++i) if (strcmp(ents[i].name, name) == 0) return &ents[i];
    return NULL;
}

/* Bit assegnati a un tensore 2D leggendone la forma quantizzata: weight U32 [rows, cols/per] con
 * per = 32/bits -> bits = 32 / (cols / shape[1]). Ritorna -1 se il tensore non esiste/non e' U32. */
static int bits_of(const char* dir, const char* wname, uint32_t cols) {
    char sp[1200]; snprintf(sp, sizeof(sp), "%s/model.safetensors", dir);
    GstEntry* ents = NULL; uint32_t ne = 0;
    if (gst_read_header(sp, &ents, &ne, NULL) != 0) { free(ents); return -1; }
    char nm[300]; snprintf(nm, sizeof(nm), "%s.weight", wname);
    GstEntry* e = find(ents, ne, nm);
    int bits = -1;
    if (e != NULL && strcmp(e->dtype, "U32") == 0 && e->ndim == 2 && e->shape[1] > 0) {
        uint64_t per = cols / e->shape[1];
        if (per > 0 && per <= 32 && (32 % per) == 0) bits = (int)(32 / per);
    }
    free(ents);
    return bits;
}

int main(void) {
    const char* SRC = "/tmp/gmb_qauto_src";
    const char* IM  = "/tmp/gmb_qauto_im";   /* allocazione guidata dall'imatrix */
    const char* UN  = "/tmp/gmb_qauto_un";   /* allocazione non pesata */
    const char* RPT = "/tmp/gmb_qauto_imatrix.txt";
    const char* NG  = "model.layers.0.mlp.gate_proj";
    const char* NU  = "model.layers.0.mlp.up_proj";
    const char* ND  = "model.layers.0.mlp.down_proj";
    const char* NQ  = "model.layers.0.self_attn.q_proj";
    const uint32_t R = 64, C = 64, GROUP = 4, LO = 2, HI = 4;
    const double TARGET = 3.0;

    mkdir_p(SRC);
    float *g = (float*)malloc((size_t)R * C * 4), *u = (float*)malloc((size_t)R * C * 4);
    float *d = (float*)malloc((size_t)R * C * 4), *q = (float*)malloc((size_t)R * C * 4);
    float* ln = (float*)malloc(C * 4);
    CHECK(g && u && d && q && ln);
    if (!g || !u || !d || !q || !ln) return 2;
    for (uint32_t r = 0; r < R; ++r) {
        for (uint32_t c = 0; c < C; ++c) {
            size_t i = (size_t)r * C + c;
            g[i] = 0.5f * sinf((float)i * 0.37f);                    /* uniforme fra le colonne */
            /* up_proj: SOLO la colonna 1 e' difficile (segnale piccolo, errore riducibile) e sara'
             * quella ad alta energia; il resto e' rappresentabile esattamente a ogni bit -> l'errore
             * NON pesato resta minuscolo, quello PESATO sulla colonna 1 e' grande (e cala molto). */
            u[i] = (i % 2 == 0) ? 1.0f : 0.0f;
            if (c == 0) u[i] = 0.1f;                 /* massimo del gruppo 0 (step Q2 = 0.1/3) */
            else if (c == 1) u[i] = 0.02f;           /* cattiva a Q2 (-> 0 o 0.033), esatta a Q4 (0.02) */
            else if (c < 4) u[i] = 0.0f;
            d[i] = 0.5f * cosf((float)i * 0.21f);
            q[i] = 0.5f * sinf((float)i * 0.13f + 1.0f);
        }
    }
    for (uint32_t c = 0; c < C; ++c) ln[c] = 1.0f;

    char sp[1200]; snprintf(sp, sizeof(sp), "%s/model.safetensors", SRC);
    uint64_t s2[2] = { R, C }, s1[1] = { C };
    GstWriter* w = NULL;
    CHECK(gst_writer_create(&w, sp) == 0);
    CHECK(gst_writer_add(w, "model.layers.0.mlp.gate_proj.weight", "F32", 2, s2, g, (size_t)R * C * 4) == 0);
    CHECK(gst_writer_add(w, "model.layers.0.mlp.up_proj.weight",   "F32", 2, s2, u, (size_t)R * C * 4) == 0);
    CHECK(gst_writer_add(w, "model.layers.0.mlp.down_proj.weight", "F32", 2, s2, d, (size_t)R * C * 4) == 0);
    CHECK(gst_writer_add(w, "model.layers.0.self_attn.q_proj.weight", "F32", 2, s2, q, (size_t)R * C * 4) == 0);
    CHECK(gst_writer_add(w, "model.layers.0.input_layernorm.weight", "F32", 1, s1, ln, (size_t)C * 4) == 0);
    CHECK(gst_writer_finish(w) == 0);
    { char cp[1200]; snprintf(cp, sizeof(cp), "%s/config.json", SRC);
      FILE* f = fopen(cp, "w"); CHECK(f != NULL);
      if (f) { fputs("{\"model_type\":\"llama\"}", f); fclose(f); } }

    /* Report imatrix: H (input FFN) con le colonne 0..3 a energia enorme; M (uscita SwiGLU) uniforme. */
    { FILE* f = fopen(RPT, "w"); CHECK(f != NULL);
      if (f) {
          fputs("# test\n", f);
          fprintf(f, "0 0 100");
          for (uint32_t c = 0; c < C; ++c) fprintf(f, " %.6g", (c == 1) ? 1000000.0 : 1.0);
          for (uint32_t c = 0; c < C; ++c) fputs(" 1", f);
          fputc('\n', f);
          fclose(f);
      } }

    CHECK(gmb_model_quantize_auto_ex(SRC, IM, GROUP, LO, HI, TARGET, RPT, 1) == 0);
    CHECK(gmb_model_quantize_auto(SRC, UN, GROUP, LO, HI, TARGET, 1) == 0);

    const char* tn[4] = { NG, NU, ND, NQ };
    int b_im[4], b_un[4];
    for (int i = 0; i < 4; ++i) { b_im[i] = bits_of(IM, tn[i], C); b_un[i] = bits_of(UN, tn[i], C); }
    printf("bit (imatrix): gate=%d up=%d down=%d q=%d\n", b_im[0], b_im[1], b_im[2], b_im[3]);
    printf("bit (non pesata): gate=%d up=%d down=%d q=%d\n", b_un[0], b_un[1], b_un[2], b_un[3]);
    for (int i = 0; i < 4; ++i) CHECK(b_im[i] >= (int)LO && b_im[i] <= (int)HI && b_un[i] >= (int)LO && b_un[i] <= (int)HI);

    /* Importanza per-colonna COME la vede il consumatore (stessa convenzione di im_imp_for). */
    float H_imp[64], M_imp[64];
    for (uint32_t c = 0; c < C; ++c) { H_imp[c] = (c == 1) ? 1000000.0f : 1.0f; M_imp[c] = 1.0f; }
    const float* x[4] = { g, u, d, q };
    const float* imp[4] = { H_imp, H_imp, M_imp, NULL };   /* q_proj: non coperta -> non pesata */
    double wt_im = 0.0, wt_un = 0.0;
    for (int i = 0; i < 4; ++i) {
        double ri = 0.0, ru = 0.0;
        CHECK(llmengine_moe_quant_rel_error(x[i], imp[i], R, C, GROUP, (uint32_t)b_im[i], &ri) == 0);
        CHECK(llmengine_moe_quant_rel_error(x[i], imp[i], R, C, GROUP, (uint32_t)b_un[i], &ru) == 0);
        wt_im += ri; wt_un += ru;
    }
    printf("errore pesato totale: imatrix=%.6f  non pesata=%.6f\n", wt_im, wt_un);

    int diff = 0;
    for (int i = 0; i < 4; ++i) if (b_im[i] != b_un[i]) diff++;
    CHECK(diff > 0);                 /* il consumatore USA l'imatrix (mappa diversa) */
    CHECK(b_im[3] == b_un[3]);       /* q_proj non coperta: nessun effetto su di lei */
    CHECK(wt_im < wt_un);            /* alloca i bit dove l'energia li rende utili */

    free(g); free(u); free(d); free(q); free(ln);
    if (fails == 0) printf("test_quant_auto_imatrix: OK\n");
    return fails ? 1 : 0;
}
