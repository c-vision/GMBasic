/* Notebook/main.c — entry point del notebook server (C).
 *
 *   gmb_notebook [--port N] [--autosave <file.json>]
 *
 * Default: porta 8030 su 127.0.0.1 (come il NotebookServer della vecchia REPL C#).
 */
#include "notebook_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    printf("gmb_notebook — notebook server di GMBASIC (C)\n"
           "\n"
           "uso: gmb_notebook [--port N] [--autosave <file.json>]\n"
           "\n"
           "  --port N            porta HTTP su 127.0.0.1 (default 8030)\n"
           "  --autosave <file>   a ogni cella scrive lì lo snapshot delle variabili (JSON)\n"
           "\n"
           "endpoint: GET /health · POST /execute {cellId,source} · GET|POST /vars ·\n"
           "          GET|POST /reset · POST /shutdown\n");
}

int main(int argc, char** argv)
{
    int port = 8030;
    const char* autosave = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(); return 0; }
        if (!strcmp(argv[i], "--port") && i + 1 < argc) { port = atoi(argv[++i]); continue; }
        if (!strncmp(argv[i], "--port=", 7)) { port = atoi(argv[i] + 7); continue; }
        if (!strcmp(argv[i], "--autosave") && i + 1 < argc) { autosave = argv[++i]; continue; }
        if (!strncmp(argv[i], "--autosave=", 11)) { autosave = argv[i] + 11; continue; }
        fprintf(stderr, "?gmb_notebook: argomento sconosciuto '%s' (usa --help)\n", argv[i]);
        return 2;
    }
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "?gmb_notebook: porta non valida (%d)\n", port);
        return 2;
    }
    return nb_server_run(port, autosave);
}
