/* gmb_code_exec.h — esecuzione BASIC generato (CODE mode), mondo C.
 * Scrive il testo su un .bas e lo esegue col runner figlio (executor nativo C)
 * catturando l'output. Indipendente dal mondo C# (regola separazione compilatori). */
#ifndef GMB_CODE_EXEC_H
#define GMB_CODE_EXEC_H
#include <stddef.h>

/* Esegue `code` (programma BASIC). out (cap) riceve l'output catturato (o messaggio
 * errore esplicito). Ritorna 0 ok (programma eseguito), -1 errore. */
int gmb_code_exec(const char* code, char* out, size_t cap);

#endif
