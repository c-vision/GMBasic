/* ipc_fake_host.c — fake host nativo per testare il client IPC v1 (R2).
 * Risponde alle richieste JSON su stdin con risposte JSON su stdout,
 * replicando il comportamento documentato del server MLX.
 *
 * Uso: ipc_fake_host  (legge da stdin, scrive su stdout)
 *   - "finetune"  -> risponde progress poi result con metriche fisse.
 *   - "apply"     -> result ok.
 *   - "status"    -> risultato dello stato ultimo job.
 *   - "cancel"    -> result ok.
 * Il fake host e' deterministico: usato dai test come host di riferimento.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void flush(void){ fflush(stdout); }

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char* line = NULL; size_t cap = 0;
    while (getline(&line, &cap, stdin) >= 0) {
        if (strstr(line, "\"method\":\"finetune\"")) {
            printf("{\"id\":1,\"type\":\"progress\",\"data\":{\"step\":0,\"loss\":12.14}}\n");
            printf("{\"id\":1,\"type\":\"result\",\"data\":{\"ok\":true,\"train\":5.19,"
                   "\"hold\":7.07,\"baseTrain\":12.14,\"baseHold\":9.12,"
                   "\"greedyBefore\":1595,\"greedyAfter\":84083,\"demoChanged\":1,"
                   "\"ckpt\":\"work/ft_best\"}}\n");
            flush();
        } else if (strstr(line, "\"method\":\"apply\"")) {
            printf("{\"id\":1,\"type\":\"result\",\"data\":{\"ok\":true,"
                   "\"message\":\"l'adapter ricaricato cambia l'inferenza\"}}\n");
            flush();
        } else if (strstr(line, "\"method\":\"status\"")) {
            printf("{\"id\":1,\"type\":\"result\",\"data\":{\"ok\":true,\"status\":\"ok\"}}\n");
            flush();
        } else if (strstr(line, "\"method\":\"cancel\"")) {
            printf("{\"id\":1,\"type\":\"result\",\"data\":{\"ok\":true,\"status\":\"cancelled\"}}\n");
            flush();
        } else {
            printf("{\"id\":1,\"type\":\"error\",\"data\":{\"message\":\"method sconosciuto\"}}\n");
            flush();
        }
    }
    free(line);
    return 0;
}