/* gmb_model_dequantize — mirror: dir MLX affine -> dir f16.
 * Uso: gmb_model_dequantize --model DIR --out DIR [--quiet] */
#include "gmb_model_quantize.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char** argv) {
    const char* model = NULL, *out = NULL; int verbose = 1;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--model") && i + 1 < argc) model = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--quiet")) verbose = 0;
    }
    if (!model || !out) { fprintf(stderr, "uso: %s --model DIR --out DIR [--quiet]\n", argv[0]); return 2; }
    return gmb_model_dequantize(model, out, verbose);
}
