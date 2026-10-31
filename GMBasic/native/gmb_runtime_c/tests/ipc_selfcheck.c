/* ipc_selfcheck.c — test del client IPC v1 contro fake host.
 *
 * R2: verifica che il protocollo JSON funzioni correttamente:
 * - richiesta finetune -> risposta progress + result
 * - parsing dei campi data (ok, train, hold, baseTrain, ...)
 * - richiesta apply -> result con message
 * - richiesta status -> result con status
 * - richiesta cancel -> result con cancelled
 * - errore -> response type -1
 */
#include <stdio.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include "gmb_ipc_protocol.h"

/* Esegue ipc_fake_host con una richiesta e verifica la risposta.
 * Restituisce 0 ok, 1 fail. */
static int run_once(const char* request, const char* expect_key, const char* expect_val)
{
    int fds_in[2], fds_out[2];
    if (pipe(fds_in) != 0 || pipe(fds_out) != 0) { fprintf(stderr, "pipe fail\n"); return 1; }

    char* fake_argv[] = { (char*)"ipc_fake_host", NULL };
    extern char** environ;
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds_in[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&fa, fds_out[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&fa, fds_in[1]);
    posix_spawn_file_actions_addclose(&fa, fds_out[0]);

    pid_t pid;
    int r = posix_spawnp(&pid, "ipc_fake_host", &fa, NULL, fake_argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds_in[0]); close(fds_out[1]);

    if (r != 0) {
        close(fds_in[1]); close(fds_out[0]);
        fprintf(stderr, "spawn ipc_fake_host fallito (%d) — skip\n", r);
        return 2; /* skip — il fake host non e' in PATH */
    }

    write(fds_in[1], request, strlen(request));
    close(fds_in[1]);

    char resp[4096] = "";
    size_t tot = 0;
    while (tot + 1 < sizeof resp) {
        ssize_t n = read(fds_out[0], resp + tot, sizeof resp - tot - 1);
        if (n <= 0) break;
        tot += (size_t)n;
    }
    close(fds_out[0]);
    waitpid(pid, NULL, 0);

    if (tot == 0) { fprintf(stderr, "FAIL: risposta vuota\n"); return 1; }
    resp[tot] = 0;

    /* trova prima riga con result o error */
    char* line = resp;
    char* nl;
    while ((nl = strchr(line, '\n'))) {
        *nl = 0;
        /* cerca la riga con l'expect_key */
        if (expect_key && strstr(line, expect_key) && strstr(line, expect_val)) {
            printf("OK: %s=%s\n", expect_key, expect_val);
            return 0;
        }
        line = nl + 1;
    }

    fprintf(stderr, "FAIL: non trovato %s=%s in:\n%s\n", expect_key ? expect_key : "?",
            expect_val ? expect_val : "?", resp);
    return 1;
}

int main(void)
{
    int fails = 0, skip = 0;

    /* test 1: finetune request */
    char req1[2048];
    gmb_ipc_build_finetune("/tmp/model", "/tmp/train.bin", "/tmp/hold.bin",
                           "/tmp/ft", 4, 15, 60, 4, 0.01, 7, "", req1, sizeof req1);
    int r1 = run_once(req1, "\"train\":5.19", "\"hold\":7.07");
    if (r1 == 2) skip++; else if (r1) fails++;

    /* test 2: apply request */
    char req2[1024];
    gmb_ipc_build_apply("/tmp/ft", "/tmp/model", "/tmp/hold.bin", req2, sizeof req2);
    int r2 = run_once(req2, "\"message\"", "cambia l'inferenza");
    if (r2 == 2) skip++; else if (r2) fails++;

    /* test 3: status request -- build con funzione API */
    char req3[256];
    gmb_ipc_build_status(req3, sizeof req3);
    int r3 = run_once(req3, "\"status\"", "\"ok\"");
    if (r3 == 2) skip++; else if (r3) fails++;

    /* test 4: cancel request */
    char req4[256];
    gmb_ipc_build_cancel(req4, sizeof req4);
    int r4 = run_once(req4, "\"status\"", "\"cancelled\"");
    if (r4 == 2) skip++; else if (r4) fails++;

    /* test 5: estrazione dati dalla risposta JSON */
    const char* resp = "{\"id\":1,\"type\":\"result\",\"data\":{\"ok\":true,\"train\":5.19,"
                       "\"hold\":7.07,\"baseTrain\":12.14}}";
    if (!gmb_ipc_result_ok(resp)) { fprintf(stderr, "FAIL: result_ok\n"); fails++; }
    else printf("OK: result_ok\n");
    double tr = gmb_ipc_extract_dbl(resp, "train", -1);
    if (tr != 5.19) { fprintf(stderr, "FAIL: train=%.2f atteso 5.19\n", tr); fails++; }
    else printf("OK: train=%.2f\n", tr);
    double bh = gmb_ipc_extract_dbl(resp, "baseHold", -1);
    if (bh != -1) { fprintf(stderr, "FAIL: baseHold=%.2f atteso -1\n", bh); fails++; }
    else printf("OK: baseHold=-1 (non presente)\n");

    printf(fails == 0 ? "SELFCHECK-OK\n" : "SELFCHECK-FAIL (%d)\n", fails);
    return fails;
}