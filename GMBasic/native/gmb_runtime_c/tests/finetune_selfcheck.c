/* finetune_selfcheck.c — verifica il flusso del runtime C per AI FINETUNE/LORA APPLY
 * con input NON esistenti (deve dare errore esplicito, mai stub/crash). */
#include <stdio.h>
#include <string.h>
#include "gmb_finetune.h"

int main(void)
{
    char out[4096]; int fails = 0;
    int r = gmb_ai_finetune_run("/tmp/nonexistent_model", "no.bin", "no2.bin", "/tmp/ck_no", 4, 15, 2, 2, 0.01, 7, "", out, sizeof out);
    printf("finetune rc=%d out=%.120s\n", r, out);
    if (r == 0 || !out[0] || strstr(out, "error") == NULL) { printf("FAIL: atteso errore esplicito\n"); fails++; }
    int a = gmb_ai_lora_apply_run("/tmp/ck_no", "/tmp/nonexistent_model", "no.bin", out, sizeof out);
    printf("apply rc=%d out=%.120s\n", a, out);
    if (a == 0 || !out[0] || strstr(out, "error") == NULL) { printf("FAIL: atteso errore esplicito\n"); fails++; }
    printf(fails == 0 ? "SELFCHECK-OK\n" : "SELFCHECK-FAIL\n");
    return fails;
}
