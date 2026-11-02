/* native/gmb_runtime_c/include/gmb_preprocess.h — preprocessore (PhasePreProcessAll). */
#ifndef GMB_PREPROCESS_H
#define GMB_PREPROCESS_H

#include <stdbool.h>

#define GMB_PP_MAX_DEPTH 64

typedef struct gmb_pp_session gmb_pp_session_t;

void gmb_preprocess_reset(void);
void gmb_preprocess_define(const char* name, const char* value);
/* Defines ESTERNI (-D/--define della CLI): persistono a `gmb_preprocess_reset()` — che viene
 * chiamato a ogni sorgente per non far trapelare i `#DEF` di un file nell'altro. Senza questo
 * i -D della riga di comando venivano cancellati prima del preprocessing (bug reale). */
void gmb_preprocess_define_base(const char* name, const char* value);
void gmb_preprocess_clear_base(void);
void gmb_preprocess_init(gmb_pp_session_t* s);
/* Processa una riga. out = riga da tokenizzare; ritorna true se va tenuta. */
bool gmb_preprocess_step(gmb_pp_session_t* s, const char* line, char* out, int cap);

/* Preprocessa un intero sorgente: ritorna il buffer filtrato (malloc) o NULL. */
char* gmb_preprocess_source(const char* src, long len);

#endif
