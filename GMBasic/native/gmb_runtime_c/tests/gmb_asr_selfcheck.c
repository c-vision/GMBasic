/* gmb_asr_selfcheck.c — conformance del modulo ASR host (front-end + kernel + Q8 + pipeline).
 * Uscita 0 = tutti i check passano. */
/* LLMEnv (2026-10-31) ha rinominato il modulo `gmb_asr_*` → `llm_asr_*`: l'header è `llm_asr.h`. */
#include "llm_asr.h"
#include <stdio.h>

int main(void) {
    return llm_asr_selfcheck(stdout);
}
