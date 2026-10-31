/* gmb_codegen.h — CodeGen loop in C (Fase 2 FaseC). */
#ifndef GMB_CODEGEN_H
#define GMB_CODEGEN_H
#include <stddef.h>
#include "gmb_runtime.h"

typedef struct {
    int ok;                 /* 1 = compilato ed eseguito */
    char code[16384];       /* codice BASIC generato (ultimo tentativo) */
    char output[16384];     /* output del run (se ok) */
    char diag[2048];        /* diagnostics: LLM error / fix esauriti */
} gmb_codegen_result_t;

/* Esegue il loop: prompt → brain → .bas → exec → fix (max_fix) → out_bas (se non NULL scrive
   il codice). Ritorna 0 se ok (r->ok=1), -1 errore (r->diag spiega). */
int gmb_codegen_run(const char* prompt, const char* out_bas, int max_fix, gmb_codegen_result_t* r);

/* Wrapper BASIC-value (gmb_string_t heap): output del run o "Error: <diag>". */
gmb_string_t gmb_agent_code(const char* task, const char* outpath);
gmb_string_t gmb_agent_fix(const char* err, const char* outpath);

#endif
