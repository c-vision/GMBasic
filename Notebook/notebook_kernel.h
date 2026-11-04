/* Notebook/notebook_kernel.h — kernel di una sessione notebook (C).
 *
 * Una sessione = un dict di variabili; ogni cella è compilata in chunk A1/GMBC dal
 * compilatore C e eseguita dalla VM/C (P-CODE, via libgmb_eval): le celle successive
 * condividono lo stato, come il kernel della vecchia REPL C#.
 */
#ifndef GMB_NOTEBOOK_KERNEL_H
#define GMB_NOTEBOOK_KERNEL_H

#include <stddef.h>

typedef struct nb_kernel nb_kernel_t;

/* Cella memorizzata (per GET /cells, /order, /runall). */
typedef struct nb_cell {
    char* id;
    char* source;
    char* output;
    int ok;
} nb_cell_t;

typedef struct nb_result {
    int ok;              /* 1 = cella eseguita */
    char* output;        /* stdout catturato (malloc, mai NULL) */
    char* value_json;    /* valore dell'espressione (malloc) o NULL */
    char* error;         /* messaggio (malloc) se ok == 0, altrimenti NULL */
    double exec_ms;
} nb_result_t;

nb_kernel_t* nb_kernel_new(void);
void nb_kernel_free(nb_kernel_t* k);

/* Esegue una cella. Ritorna 0 se ok, !=0 errore (r->error valorizzato). */
int nb_kernel_exec(nb_kernel_t* k, const char* source, nb_result_t* r);

/* Esegue una cella con ID e la memorizza/aggiorna (GET /cells, /order, /runall).
 * Celle magiche: `%md testo` → passthrough; `%sql`/`%ai`/`%vdb`/`%chart` → errore esplicito
 * (non supportate nel notebook C: vedi README). */
int nb_kernel_exec_cell(nb_kernel_t* k, const char* id, const char* source, nb_result_t* r);
char* nb_kernel_cells_json(nb_kernel_t* k);   /* [{"cellId","source","output","ok"}] (malloc) */
char* nb_kernel_order_json(nb_kernel_t* k);   /* {"order":["id",...]} (malloc) */
/* Azzera lo scope e riesegue tutte le celle in ordine: JSON con l'esito per cella. */
char* nb_kernel_runall(nb_kernel_t* k);
void nb_result_free(nb_result_t* r);

/* JSON `{"vars":[{"name":"x","value":5}]}` (malloc). */
char* nb_kernel_vars_json(nb_kernel_t* k);

/* Azzera le variabili (le celle eseguite restano contate). */
void nb_kernel_reset(nb_kernel_t* k);

int nb_kernel_cell_count(nb_kernel_t* k);

#endif /* GMB_NOTEBOOK_KERNEL_H */
