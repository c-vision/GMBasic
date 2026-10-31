/* gmb_fetch_cli.c — riga di comando del motore di download (gmb_fetch.h).
 *
 * Serve a tre cose: provare il motore a mano (`gmb_fetch <url> <file>`), dare un comando
 * concreto a chi deve scaricare un asset grosso (un cervello da 1,4 GB) senza riscriverne la
 * politica, e verificare la ripresa: si interrompe con Ctrl-C e si rilancia lo stesso comando,
 * che riparte dall'.incomplete.
 *
 * Il token NON si passa da riga di comando (finirebbe in `ps` e nella cronologia della shell):
 * arriva da un FILE con --token-file.
 *
 * Uso:
 *   gmb_fetch <url> <destinazione> [--sha <64hex>] [--size <byte>] [--token-file <file>]
 *              [--restart] [--quiet] [--verify-only]
 *
 * Exit: 0 = ok; altrimenti il codice del motore (vedi gmb_fetch_strerror).
 */
#include "gmb_fetch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_progress(void* u, long long done, long long total, double rate, long long eta)
{
    (void)u;
    if (total > 0) {
        double pct = 100.0 * (double)done / (double)total;
        printf("\r%6.2f%%  %8.1f MB / %8.1f MB  %6.2f MB/s  ETA %llds   ",
               pct, (double)done / 1048576.0, (double)total / 1048576.0, rate / 1048576.0, eta);
    } else {
        printf("\r%8.1f MB  %6.2f MB/s   ", (double)done / 1048576.0, rate / 1048576.0);
    }
    fflush(stdout);
}

static void usage(void)
{
    fprintf(stderr,
        "uso: gmb_fetch <url> <destinazione> [--sha <64hex>] [--size <byte>]\n"
        "                [--token-file <file>] [--restart] [--quiet] [--verify-only]\n");
}

static char* read_token_file(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) buf[--n] = '\0';
    if (!n) return NULL;
    char* out = (char*)malloc(n + 1);
    if (out) memcpy(out, buf, n + 1);
    return out;
}

int main(int argc, char** argv)
{
    const char* url = NULL;
    const char* dest = NULL;
    const char* sha = NULL;
    const char* token_file = NULL;
    long long size = 0;
    int restart = 0, quiet = 0, verify_only = 0;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--sha") == 0 && i + 1 < argc) sha = argv[++i];
        else if (strcmp(a, "--size") == 0 && i + 1 < argc) size = atoll(argv[++i]);
        else if (strcmp(a, "--token-file") == 0 && i + 1 < argc) token_file = argv[++i];
        else if (strcmp(a, "--restart") == 0) restart = 1;
        else if (strcmp(a, "--quiet") == 0) quiet = 1;
        else if (strcmp(a, "--verify-only") == 0) verify_only = 1;
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) { usage(); return 0; }
        else if (!url) url = a;
        else if (!dest) dest = a;
        else { usage(); return 1; }
    }
    if (!dest) { usage(); return 1; }

    if (verify_only) {
        int ok = gmb_fetch_verify_file(dest, sha, size);
        printf("%s: %s\n", dest, ok ? "valido" : "NON valido");
        return ok ? 0 : GMB_FETCH_E_SHA;
    }
    if (!url) { usage(); return 1; }

    const gmb_fetch_transport* tr = gmb_fetch_transport_curl();
    if (!tr) {
        fprintf(stderr, "gmb_fetch: runtime compilato senza libcurl (nessun trasporto HTTP)\n");
        return GMB_FETCH_E_NO_TRANSPORT;
    }

    char* token = token_file ? read_token_file(token_file) : NULL;
    if (token_file && !token) {
        fprintf(stderr, "gmb_fetch: impossibile leggere il token da %s\n", token_file);
        return GMB_FETCH_E_ARG;
    }

    gmb_fetch_request rq;
    memset(&rq, 0, sizeof rq);
    rq.url = url;
    rq.dest_path = dest;
    rq.sha256 = sha;
    rq.size = size;
    rq.token = token;

    gmb_fetch_opts op;
    gmb_fetch_opts_default(&op);
    op.quiet = quiet;
    if (!quiet) op.progress = on_progress;
    if (restart) op.allow_resume = 0;

    char err[512];
    int rc = gmb_fetch_to_file(&rq, &op, tr, err, sizeof err);
    if (!quiet) printf("\n");

    if (rc == GMB_FETCH_OK) {
        if (!quiet) printf("ok: %s\n", dest);
    } else {
        fprintf(stderr, "gmb_fetch: %s%s%s\n", gmb_fetch_strerror(rc),
                err[0] ? " — " : "", err);
    }

    free(token);
    return rc;
}
