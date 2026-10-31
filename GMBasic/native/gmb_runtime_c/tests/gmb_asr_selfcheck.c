/* gmb_asr_selfcheck.c — conformance del modulo ASR host (front-end + kernel + Q8 + pipeline).
 * Uscita 0 = tutti i check passano. */
#include "gmb_asr.h"
#include <stdio.h>

int main(void) {
    return gmb_asr_selfcheck(stdout);
}
