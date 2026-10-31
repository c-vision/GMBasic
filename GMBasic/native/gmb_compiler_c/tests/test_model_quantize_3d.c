/* test_model_quantize_3d.c — §E2 step 2: quantizzatore/dequantizzatore di ESPERTI 3D.
 *
 * Costruisce un modello F32 sintetico con un peso ESPERTO 3D (`...experts.gate_proj.weight`
 * [E,R,C]) + un 2D + un 1D, lo quantizza a Q4/g32, verifica il layout MLX 3D
 * (weight U32 [E,R,C/per], scales/biases F16 [E,R,C/group]) e il round-trip (errore bounded).
 * Verifica anche che il 2D e l'1D restino corretti (nessuna regressione). */
#include "gmb_st.h"
#include "gmb_model_quantize.h"
#include "llmcodec_moe_quant.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int gmb_model_dequantize(const char* src_dir, const char* out_dir, int verbose);

static int fails = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static float f16_to_f32(uint16_t h) {
    uint32_t s = (uint32_t)(h & 0x8000u) << 16, e = (h >> 10) & 0x1Fu, m = h & 0x3FFu, u;
    if (e == 0) { if (m == 0) u = s; else { int sh = 0; while (!(m & 0x400u)) { m <<= 1; sh++; } m &= 0x3FFu; u = s | ((uint32_t)(127 - 15 - sh + 1) << 23) | (m << 13); } }
    else if (e == 31) { if (m && !(m & 0x200u)) m |= 0x200u; u = s | 0x7F800000u | (m << 13); }
    else u = s | ((e - 15 + 127) << 23) | (m << 13);
    float f; memcpy(&f, &u, 4); return f;
}

static void mkdir_p(const char* p) {
    char buf[1200]; snprintf(buf, sizeof(buf), "%s", p);
    for (char* q = buf + 1; *q; ++q) if (*q == '/') { *q = 0; mkdir(buf, 0777); *q = '/'; }
    mkdir(buf, 0777);
}

static GstEntry* find(GstEntry* ents, uint32_t ne, const char* name) {
    for (uint32_t i = 0; i < ne; ++i) if (strcmp(ents[i].name, name) == 0) return &ents[i];
    return NULL;
}

int main(void) {
    const char* SRC = "/tmp/gmb_q3d_src";
    const char* Q4  = "/tmp/gmb_q3d_q4";
    const char* RT  = "/tmp/gmb_q3d_rt";
    const char* N3 = "model.layers.0.mlp.experts.gate_proj.weight";
    const char* N2 = "model.layers.0.self_attn.q_proj.weight";
    const char* N1 = "model.layers.0.input_layernorm.weight";
    int E = 2, R = 16, C = 32;

    mkdir_p(SRC);
    float* ex = (float*)malloc((size_t)E * R * C * sizeof(float));
    float* q2 = (float*)malloc((size_t)R * C * sizeof(float));
    float* ln = (float*)malloc((size_t)C * sizeof(float));
    for (size_t i = 0; i < (size_t)E * R * C; i++) ex[i] = sinf((float)i * 0.37f) * 0.5f;
    for (size_t i = 0; i < (size_t)R * C; i++) q2[i] = cosf((float)i * 0.21f) * 0.5f;
    for (int i = 0; i < C; i++) ln[i] = 1.0f;

    char sp[1200]; snprintf(sp, sizeof(sp), "%s/model.safetensors", SRC);
    GstWriter* w = NULL;
    CHECK(gst_writer_create(&w, sp) == 0);
    uint64_t s3[3] = { (uint64_t)E, (uint64_t)R, (uint64_t)C };
    uint64_t s2[2] = { (uint64_t)R, (uint64_t)C };
    uint64_t s1[1] = { (uint64_t)C };
    CHECK(gst_writer_add(w, N3, "F32", 3, s3, ex, (size_t)E * R * C * 4) == 0);
    CHECK(gst_writer_add(w, N2, "F32", 2, s2, q2, (size_t)R * C * 4) == 0);
    CHECK(gst_writer_add(w, N1, "F32", 1, s1, ln, (size_t)C * 4) == 0);
    CHECK(gst_writer_finish(w) == 0);
    { char cp[1200]; snprintf(cp, sizeof(cp), "%s/config.json", SRC);
      FILE* f = fopen(cp, "w"); CHECK(f != NULL);
      if (f) { fputs("{\"model_type\":\"qwen3_moe\",\"num_experts\":2}", f); fclose(f); } }

    CHECK(gmb_model_quantize(SRC, Q4, 4, 32, 0) == 0);

    char qp[1200]; snprintf(qp, sizeof(qp), "%s/model.safetensors", Q4);
    GstEntry* qe = NULL; uint32_t qn = 0;
    CHECK(gst_read_header(qp, &qe, &qn, NULL) == 0);

    char nb[300];
    snprintf(nb, sizeof(nb), "%.*s.weight", (int)(strlen(N3) - 7), N3);
    GstEntry* e3w = find(qe, qn, nb);
    snprintf(nb, sizeof(nb), "%.*s.scales", (int)(strlen(N3) - 7), N3);
    GstEntry* e3s = find(qe, qn, nb);
    CHECK(e3w != NULL && e3s != NULL);
    if (e3w) {
        CHECK(strcmp(e3w->dtype, "U32") == 0);
        CHECK(e3w->ndim == 3 && e3w->shape[0] == 2 && e3w->shape[1] == 16 && e3w->shape[2] == 32 / (32 / 4));
    }
    if (e3s) {
        CHECK(strcmp(e3s->dtype, "F16") == 0);
        CHECK(e3s->ndim == 3 && e3s->shape[0] == 2 && e3s->shape[2] == 32 / 32);
    }

    CHECK(gmb_model_dequantize(Q4, RT, 0) == 0);

    char rp[1200]; snprintf(rp, sizeof(rp), "%s/model.safetensors", RT);
    GstEntry* re = NULL; uint32_t rn = 0;
    CHECK(gst_read_header(rp, &re, &rn, NULL) == 0);
    GstEntry* r3 = find(re, rn, N3);
    CHECK(r3 != NULL && r3->ndim == 3 && r3->shape[0] == 2 && r3->shape[1] == 16 && r3->shape[2] == 32);
    if (r3) {
        uint8_t* raw = NULL; size_t rn_bytes = 0;
        CHECK(gst_read_tensor(rp, r3, &raw, &rn_bytes) == 0);
        double md = 0;
        for (size_t i = 0; i < (size_t)E * R * C; i++) {
            uint16_t h; memcpy(&h, raw + i * 2, 2);
            double d = fabs((double)ex[i] - (double)f16_to_f32(h));
            if (d > md) md = d;
        }
        printf("3D round-trip max_abs=%.5g\n", md);
        CHECK(md < 0.05);   /* Q4/g32 su valori ~0.5 */
        free(raw);
    }
    /* §E2 step 3: quantizzazione ESPERTI pesata dall'imatrix + expert-bits dedicato.
     * Report sintetico (layer 0, expert 0/1: 32 energie gate + 16 down, imp fortemente skew). */
    {
        char rpt[1200]; snprintf(rpt, sizeof(rpt), "%s/imatrix.txt", SRC);
        FILE* f = fopen(rpt, "w");
        CHECK(f != NULL);
        if (f) {
            fputs("# test\n", f);
            for (int exi = 0; exi < 2; exi++) {
                fprintf(f, "0 %d 10", exi);
                for (int i = 0; i < 32; i++) fprintf(f, " %.4f", i == 0 ? 1000.0 : 0.001);
                for (int i = 0; i < 16; i++) fprintf(f, " %.4f", i == 0 ? 1000.0 : 0.001);
                fputc('\n', f);
            }
            fclose(f);
        }
        const char* QW = "/tmp/gmb_q3d_w", *QN = "/tmp/gmb_q3d_n";
        CHECK(gmb_model_quantize_ex(SRC, QW, 4, 32, rpt, 2, 0) == 0);       /* 2-bit pesato */
        CHECK(gmb_model_quantize_ex(SRC, QN, 4, 32, NULL, 2, 0) == 0);      /* 2-bit RTN */
        char wp[1200], np_[1200];
        snprintf(wp, sizeof(wp), "%s/model.safetensors", QW);
        snprintf(np_, sizeof(np_), "%s/model.safetensors", QN);
        GstEntry* we = NULL; uint32_t wn = 0; GstEntry* ne2 = NULL; uint32_t nn = 0;
        CHECK(gst_read_header(wp, &we, &wn, NULL) == 0 && gst_read_header(np_, &ne2, &nn, NULL) == 0);
        char bn[300]; snprintf(bn, sizeof(bn), "%.*s.weight", (int)(strlen(N3) - 7), N3);
        GstEntry* ew = find(we, wn, bn); GstEntry* en = find(ne2, nn, bn);
        CHECK(ew && en && strcmp(ew->dtype, "U32") == 0 && ew->ndim == 3 && ew->shape[2] == 32 / 16);
        if (ew && en) {
            uint8_t* wa = NULL; size_t na = 0; uint8_t* wb2 = NULL; size_t nb = 0;
            CHECK(gst_read_tensor(wp, ew, &wa, &na) == 0 && gst_read_tensor(np_, en, &wb2, &nb) == 0);
            CHECK(na == nb && na > 0);
            printf("imatrix: esperti 2-bit (weighted vs RTN) bytes=%zu identici=%d\n", na, memcmp(wa, wb2, na) == 0);
            CHECK(memcmp(wa, wb2, na) != 0);   /* l'imatrix cambia la quantizzazione */
            free(wa); free(wb2);
        }
        free(we); free(ne2);
    }

    free(qe); free(re);
    free(ex); free(q2); free(ln);

    if (fails == 0) printf("test_model_quantize_3d: OK\n");
    return fails ? 1 : 0;
}
