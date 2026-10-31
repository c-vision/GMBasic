/* gmb_asr_cmd.c — entry-point NATIVO della funzione BASIC `ASR.RECOGNIZE(model$, wav$[, outPath$])`.
 * Carica (con cache per-dir) modello+tokenizer, trascrive il WAV e ritorna testo malloc'd
 * (liberato da `gmb_asr_free`). Se `out_path` e' non vuoto, usa lo STREAMING (chunk 2s + cache
 * finestre encoder + rollback) e scrive i pezzi IN LIVE sul file. Unico P/Invoke: compilato
 * dentro libgmb_ai_llm. */
/* LLMEnv ha rinominato il motore ASR `gmb_asr_*` → `llm_asr_*` (2026-10-31): gli header
 * ora si chiamano `llm_asr.h` / `llm_asr_tok.h` e i tipi `LlmAsrModel` / `LlmAsrTok`.
 * Questo entry-point (P/Invoke di ASR.RECOGNIZE) segue il rename: senza, il runtime C
 * non compila da zero ("gmb_asr.h file not found"). */
#include "llm_asr.h"
#include "llm_asr_tok.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LlmAsrModel* g_model = NULL;
static LlmAsrTok* g_tok = NULL;
static char g_dir[1024];

static const char* cmd_piece(int id, void* ctx) { return llm_asr_tok_decode((LlmAsrTok*)ctx, id); }

static void cmd_emit(const char* piece, int len, void* ctx) {
    FILE* f = (FILE*)ctx;
    if (f && piece && len > 0) { fwrite(piece, 1, (size_t)len, f); fflush(f); }
}

char* gmb_asr_cmd(const char* model_dir, const char* wav_path, const char* out_path) {
    if (model_dir == NULL || !*model_dir || wav_path == NULL || !*wav_path) return NULL;
    if (g_model == NULL || strcmp(g_dir, model_dir) != 0) {
        if (g_model) { llm_asr_model_free(g_model); g_model = NULL; }
        if (g_tok) { llm_asr_tok_free(g_tok); g_tok = NULL; }
        g_model = llm_asr_load_model(model_dir);
        if (g_model == NULL) { fprintf(stderr, "ASR.RECOGNIZE: load modello fallito (%s)\n", model_dir); return NULL; }
        g_tok = llm_asr_tok_load(model_dir);
        if (g_tok == NULL) { fprintf(stderr, "ASR.RECOGNIZE: load tokenizer fallito (%s)\n", model_dir); llm_asr_model_free(g_model); g_model = NULL; return NULL; }
        snprintf(g_dir, sizeof g_dir, "%s", model_dir);
    }
    int ns = 0;
    float* samples = llm_asr_wav_load(wav_path, &ns);
    if (samples == NULL) { fprintf(stderr, "ASR.RECOGNIZE: load WAV fallito (%s)\n", wav_path); return NULL; }

    char* text;
    if (out_path != NULL && *out_path) {
        FILE* f = fopen(out_path, "wb");
        text = llm_asr_transcribe_stream(llm_asr_model_ecfg(g_model), llm_asr_model_encoder(g_model),
                                         llm_asr_model_dcfg(g_model), llm_asr_model_decoder(g_model),
                                         samples, ns, NULL, 0, NULL, 0, cmd_piece, g_tok,
                                         5, 2.0f, f ? cmd_emit : NULL, f);
        if (f) fclose(f);
    } else {
        int nt = 0;
        text = llm_asr_transcribe(llm_asr_model_ecfg(g_model), llm_asr_model_encoder(g_model),
                                  llm_asr_model_dcfg(g_model), llm_asr_model_decoder(g_model),
                                  samples, ns, NULL, 0, NULL, 0, cmd_piece, g_tok, 0.0f, 3.0f, &nt);
    }
    free(samples);
    return text;
}

void gmb_asr_free(char* p) { free(p); }
