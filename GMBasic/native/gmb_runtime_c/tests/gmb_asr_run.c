/* gmb_asr_run.c — harness end-to-end ASR: <model_dir> <file.wav> [seg_sec].
 * Usa il modulo tokenizer del runtime (gmb_asr_tok). Stampa la trascrizione. */
#include "gmb_asr.h"
#include "gmb_asr_tok.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* piece_fn(int id, void* ctx) { return gmb_asr_tok_decode((GmbAsrTok*)ctx, id); }

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
        GmbAsrTok* t = gmb_asr_tok_load(argv[1]);
        if (!t) { fprintf(stderr, "load tokenizer fallito\n"); return 1; }
        int n = 0; int* ids = gmb_asr_tok_encode(t, argv[3], &n);
        printf("ids:"); for (int i = 0; i < n; i++) printf(" %d", ids[i]); printf("\n");
        printf("dec:"); for (int i = 0; i < n; i++) printf("%s", gmb_asr_tok_decode(t, ids[i])); printf("\n");
        free(ids); gmb_asr_tok_free(t); return 0;
    }
    GmbAsrModel* m = gmb_asr_load_model(argv[1]);
    if (!m) { fprintf(stderr, "load modello fallito\n"); return 1; }
    GmbAsrTok* t = gmb_asr_tok_load(argv[1]);
    if (!t) { fprintf(stderr, "load tokenizer fallito\n"); return 1; }
    if (argc >= 4 && !strcmp(argv[2], "--stream")) {
        int sn = 0; float* ss = gmb_asr_wav_load(argv[3], &sn);
        if (!ss) { fprintf(stderr, "load wav fallito\n"); return 1; }
        float ch = (argc > 4) ? (float)atof(argv[4]) : 2.0f;
        char* text = gmb_asr_transcribe_stream(gmb_asr_model_ecfg(m), gmb_asr_model_encoder(m),
                                               gmb_asr_model_dcfg(m), gmb_asr_model_decoder(m),
                                               ss, sn, NULL, 0, NULL, 0, piece_fn, t,
                                               5, ch, NULL, NULL);
        if (text) { printf("%s\n", text); free(text); }
        free(ss); gmb_asr_tok_free(t); gmb_asr_model_free(m); return 0;
    }
    int ns = 0; float* samples = gmb_asr_wav_load(argv[2], &ns);
    if (!samples) { fprintf(stderr, "load wav fallito\n"); return 1; }
    fprintf(stderr, "audio: %.2f s\n", (double)ns / GMB_ASR_SR);
    int nt = 0;
    float seg = (argc > 3) ? (float)atof(argv[3]) : 0.0f;
    char* text = gmb_asr_transcribe(gmb_asr_model_ecfg(m), gmb_asr_model_encoder(m),
                                    gmb_asr_model_dcfg(m), gmb_asr_model_decoder(m),
                                    samples, ns, NULL, 0, NULL, 0, piece_fn, t, seg, 3.0f, &nt);
    if (!text) { fprintf(stderr, "transcribe fallito\n"); return 1; }
    printf("%s\n", text);
    free(text); free(samples); gmb_asr_tok_free(t); gmb_asr_model_free(m);
    return 0;
}
