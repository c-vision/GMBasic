/* gmb_embed.h — lettura COMPILE-TIME di un file per la direttiva `EMBEDFILE`
 * (equivalente di `@embedFile` di Zig: le risorse viaggiano dentro l'eseguibile).
 *
 *   EMBEDFILE "shader.glsl" AS shader$
 *
 * Il parser legge il file e lo trasforma in un letterale stringa BINARIO
 * (`gmb_expr_str_n`), quindi il resto della pipeline (bytecode e emitter C99) non
 * sa nulla di file: nessun I/O a runtime, nessun path da consegnare accanto al
 * programma.
 *
 * Risoluzione del percorso: se `path` è relativo si prova, in ordine,
 *   1) la cartella del sorgente che si sta compilando (`gmb_embed_set_base_dir`,
 *      impostata dai driver con `gmb_compile_bas_file` e dagli entry file-based);
 *   2) la cartella di lavoro del processo.
 * Percorsi assoluti usati così come sono.
 */
#ifndef GMB_EMBED_H
#define GMB_EMBED_H

#include <stddef.h>

/* Cartella base per i percorsi relativi ("" = solo CWD). Copiata, non posseduta. */
void gmb_embed_set_base_dir(const char* dir);

/* Come sopra ma da un PERCORSO DI FILE: usa la sua cartella (i driver la chiamano con il
 * file di ingresso, così `EMBEDFILE "x"` è relativo al programma e non alla CWD). */
void gmb_embed_set_base_from_path(const char* file);

/* Legge il file: ritorna un buffer malloc (NUL-terminato in coda, `*out_len` = byte
 * reali) oppure NULL con `*error` = messaggio malloc (da liberare). */
char* gmb_embed_load(const char* path, long* out_len, char** error);

#endif /* GMB_EMBED_H */
