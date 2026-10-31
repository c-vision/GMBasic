/* gmb_model_quantize — driver: quantizza un modello HF/MLX (F32/F16/BF16) in MLX affine.
 * Uso: gmb_model_quantize --model DIR --out DIR [--bits N] [--group G]
 *        [--imatrix REPORT] [--expert-bits M] [--auto-bpw BPW] [--bits-lo A --bits-hi B] [--quiet]
 * Con --imatrix (+ --expert-bits) gli esperti 3D sono quantizzati a M bit con errore PESATO
 * dall'importanza (DS4-Q1.b §3.5); gli altri tensori restano a --bits.
 * Con --auto-bpw (§E3) i bit sono allocati PER-TENSORE dalla sensibilità misurata (errore relativo)
 * sotto il budget medio BPW, con candidati --bits-lo/--bits-hi (default 4/8) e config mixed.
 * Con --auto-bpw + --imatrix (§E4a/Q2-b) la sensibilità dei tensori del FFN è PESATA dall'energia
 * di attivazione misurata (report imatrix): gli altri tensori restano non pesati. */
#include "gmb_model_quantize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    const char* model = NULL, *out = NULL, *imatrix = NULL;
    uint32_t bits = 4, group = 64, expert_bits = 0, bits_lo = 4, bits_hi = 8; int verbose = 1;
    double auto_bpw = 0.0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--model") && i + 1 < argc) model = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--bits") && i + 1 < argc) bits = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--group") && i + 1 < argc) group = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--imatrix") && i + 1 < argc) imatrix = argv[++i];
        else if (!strcmp(argv[i], "--expert-bits") && i + 1 < argc) expert_bits = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--auto-bpw") && i + 1 < argc) auto_bpw = strtod(argv[++i], NULL);
        else if (!strcmp(argv[i], "--bits-lo") && i + 1 < argc) bits_lo = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--bits-hi") && i + 1 < argc) bits_hi = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--quiet")) verbose = 0;
    }
    if (!model || !out) {
        fprintf(stderr, "uso: %s --model DIR --out DIR [--bits N --group G --imatrix REPORT --expert-bits M]\n"
                        "     | %s --model DIR --out DIR --auto-bpw BPW [--bits-lo A --bits-hi B --group G --quiet]\n", argv[0], argv[0]);
        return 2;
    }
    if (auto_bpw > 0.0) return gmb_model_quantize_auto_ex(model, out, group, bits_lo, bits_hi, auto_bpw, imatrix, verbose);
    return gmb_model_quantize_ex(model, out, bits, group, imatrix, expert_bits, verbose);
}
