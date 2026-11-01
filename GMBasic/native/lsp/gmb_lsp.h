/* native/repl/gmb_lsp.h — Language Server (LSP) per GMBASIC, in C (modalità --lsp).
 * Trasporto JSON-RPC su stdio (Content-Length framing) + ciclo di vita LSP.
 * La diagnostica/simboli si agganceranno a gmb_lexer/gmb_parser (Fase A-D del
 * C-Compiler-Autonomo-Plan) quando saranno pronti; qui il server è già funzionante
 * (initialize/capabilities/shutdown/exit, didOpen/didChange con diagnostica vuota).
 */
#ifndef GMB_LSP_H
#define GMB_LSP_H

/* Esegue il loop del language server su stdin/stdout finché exit. Ritorna exit code. */
int gmb_lsp_run(void);

#endif
