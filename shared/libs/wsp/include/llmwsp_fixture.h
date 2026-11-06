/* llmwsp_fixture.h — W4: mini-compilatore di FIXTURE a verità nota (stile Tracr).
 *
 * Scrive su disco un piccolo checkpoint safetensors+config.json i cui pesi sono COSTRUITI per realizzare
 * meccanismi dichiarati (copia, previous-token, induzione) più teste di rumore casuale deterministico.
 * Restituisce la verità: quali teste fanno cosa e quali composizioni esistono. Servono a misurare precisione e
 * richiamo degli analizzatori (W4) contro una risposta esatta. Sono vincoli sui PESI (circuiti OV/composizione):
 * il comportamento in forward va validato con W5. Spazio residuo H=16: T (dim 0..3) = token, P (dim 4..7) =
 * "token precedente"; hd=4, 4 teste per layer, MHA. */
#ifndef LLMWSP_FIXTURE_H
#define LLMWSP_FIXTURE_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int layer, head; char role[16]; } llmwsp_fx_head;     /* "copy"|"prevtoken"|"induction"|"noise" */
typedef struct { int l1, h1, l2, h2; char which; } llmwsp_fx_edge;     /* O di (l1,h1) alimenta q/k/v di (l2,h2) */

typedef struct llmwsp_fx_truth {
    int            layers, heads;
    int            nheads;
    llmwsp_fx_head hd[64];
    int            nedges;
    llmwsp_fx_edge edges[16];
} llmwsp_fx_truth;

/* kind: "copy" (1 layer: L0H0 copia) | "induction" (2 layer: L0H0 previous-token, L1H0 induzione che legge P
 * con K e scrive su T). Le altre teste sono rumore gaussiano deterministico (seed). 0 = ok. */
int llmwsp_fixture_compile(const char *kind, const char *outdir, unsigned seed, llmwsp_fx_truth *truth,
                           char *err, size_t errcap);

#ifdef __cplusplus
}
#endif
#endif
