/* gmb_semquery.h — §4.10 esposizione agenti (lato C, INDIPENDENTE dal C#): query sul grafo
 * semantico di un FILE `.bas`, per il tool-loop agentico `code.*` (VM/C, C99Native).
 * Riusa `gmb_semgraph` + il fold `gmb_semfold` — nessuna dipendenza da C#, nessun P/Invoke. */
#ifndef GMB_SEMQUERY_H
#define GMB_SEMQUERY_H

#include <stddef.h>

/* Compila `path` (lex→parse), ne estrae il grafo e risponde alla query `verb`:
 *   SYMBOL | CALLMAP | IMPACT | CONTEXT | FINDSYMBOL | FILEOUTLINE | REPOMAP | INSPECT
 * (`arg` = nome/simbolo/prefix/file; `depth < 0` = assente). Scrive la stringa canonica in
 * `out` (identica alla superficie BASIC `LLM.<verb>`). Ritorna 0 ok, -1 su errore (parse/file).
 * Per FILEOUTLINE/REPOMAP passare `arg` = basename del file (o NULL) e `depth < 0`. */
int gmb_semquery_file(const char* path, const char* verb, const char* arg, int depth, char* out, size_t cap);

#endif /* GMB_SEMQUERY_H */
