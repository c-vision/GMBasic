/* native/lsp/main.c — entry point del Language Server GMBASIC (`gmb_lsp`).
 *
 * Tool SEPARATO dalla REPL (F2c): parla LSP/JSON-RPC su stdio e serve qualunque
 * client — le IDE di GMBasic, VS Code, altri editor. La REPL non lo linka più:
 * se un client vuole il LSP, avvia `gmb_lsp` come processo.
 *
 * Uso:  gmb_lsp            (LSP su stdin/stdout)
 *       gmb_lsp --version
 */
#include "gmb_lsp.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc >= 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
        printf("gmb_lsp 1.0.0 (GMBASIC language server, C)\n");
        return 0;
    }
    if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("uso: gmb_lsp   — language server LSP/JSON-RPC su stdio\n"
               "  (Content-Length framing; initialize/didOpen/didChange/completion/hover)\n"
               "Client: IDE GMBasic, VS Code o qualunque editor LSP.\n");
        return 0;
    }
    return gmb_lsp_run();
}
