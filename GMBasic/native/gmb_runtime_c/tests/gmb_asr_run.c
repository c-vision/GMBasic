/* gmb_asr_run.c — harness end-to-end ASR: <model_dir> <file.wav> [seg_sec].
 * Usa il modulo tokenizer del runtime (llm_asr_tok). Stampa la trascrizione. */
/* LLMEnv ha rinominato il motore ASR `gmb_asr_*` → `llm_asr_*` (2026-10-31): gli header ora si
 * chiamano `llm_asr.h` / `llm_asr_tok.h` e i tipi `LlmAsrModel` / `LlmAsrTok`. Questo harness segue
 * il rename: prima includeva `gmb_asr.h`, che non esiste più, e il target CMake restava spento. */
#include "llm_asr.h"
#include "llm_asr_tok.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* piece_fn(int id, void* ctx) { return llm_asr_tok_decode((LlmAsrTok*)ctx, id); }

int main(int argc, char** argv) {
    if (argc >= 2 && (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        fprintf(stderr,
            "uso: %s <model_dir> <file.wav> [seg_sec]\n"
            "     %s <model_dir> --enc <testo>                    # tokenizza e ritokenizza (senza audio)\n"
            "     %s <model_dir> --stream <file.wav> [chunk_sec]  # trascrizione in streaming\n",
            argv[0], argv[0], argv[0]);
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr,
            "uso: %s <model_dir> <file.wav> [seg_sec]\n"
            "     %s <model_dir> --enc <testo>\n"
            "     %s <model_dir> --stream <file.wav> [chunk_sec]\n",
            argv[0], argv[0], argv[0]);
        return 2;
    }
    if (argc >= 4 && !strcmp(argv[2], "--enc")) {
        LlmAsrTok* t = llm_asr_tok_load(argv[1]);
        if (!t) { fprintf(stderr, "load tokenizer fallito\n"); return 1; }
        int n = 0; int* ids = llm_asr_tok_encode(t, argv[3], &n);
        printf("ids:"); for (int i = 0; i < n; i++) printf(" %d", ids[i]); printf("\n");
        printf("dec:"); for (int i = 0; i < n; i++) printf("%s", llm_asr_tok_decode(t, ids[i])); printf("\n");
        free(ids); llm_asr_tok_free(t); return 0;
    }
    LlmAsrModel* m = llm_asr_load_model(argv[1]);
    if (!m) { fprintf(stderr, "load modello fallito\n"); return 1; }
    LlmAsrTok* t = llm_asr_tok_load(argv[1]);
    if (!t) { fprintf(stderr, "load tokenizer fallito\n"); return 1; }
    if (argc >= 4 && !strcmp(argv[2], "--stream")) {
        int sn = 0; float* ss = llm_asr_wav_load(argv[3], &sn);
        if (!ss) { fprintf(stderr, "load wav fallito\n"); return 1; }
        float ch = (argc > 4) ? (float)atof(argv[4]) : 2.0f;
        char* text = llm_asr_transcribe_stream(llm_asr_model_ecfg(m), llm_asr_model_encoder(m),
                                               llm_asr_model_dcfg(m), llm_asr_model_decoder(m),
                                               ss, sn, NULL, 0, NULL, 0, piece_fn, t,
                                               5, ch, NULL, NULL);
        if (text) { printf("%s\n", text); free(text); }
        free(ss); llm_asr_tok_free(t); llm_asr_model_free(m); return 0;
    }
    int ns = 0; float* samples = llm_asr_wav_load(argv[2], &ns);
    if (!samples) { fprintf(stderr, "load wav fallito\n"); return 1; }
    fprintf(stderr, "audio: %.2f s\n", (double)ns / LLM_ASR_SR);
    int nt = 0;
    float seg = (argc > 3) ? (float)atof(argv[3]) : 0.0f;
    char* text = llm_asr_transcribe(llm_asr_model_ecfg(m), llm_asr_model_encoder(m),
                                    llm_asr_model_dcfg(m), llm_asr_model_decoder(m),
                                    samples, ns, NULL, 0, NULL, 0, piece_fn, t, seg, 3.0f, &nt);
    if (!text) { fprintf(stderr, "transcribe fallito\n"); return 1; }
    printf("%s\n", text);
    free(text); free(samples); llm_asr_tok_free(t); llm_asr_model_free(m);
    return 0;
}
