/* Notebook/notebook_kernel.h — kernel di una sessione notebook (C).
 *
 * Una sessione = un dict di variabili + il valutatore BASIC del runtime C
 * (`gmb_expr_exec_capture`, lo stesso usato dalle macro &/EVAL): celle successive
 * condividono lo stato, come il kernel della vecchia REPL C#.
 */
#ifndef GMB_NOTEBOOK_KERNEL_H
#define GMB_NOTEBOOK_KERNEL_H

#include <stddef.h>

typedef struct nb_kernel nb_kernel_t;

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
void nb_result_free(nb_result_t* r);

/* JSON `{"vars":[{"name":"x","value":5}]}` (malloc). */
char* nb_kernel_vars_json(nb_kernel_t* k);

/* Azzera le variabili (le celle eseguite restano contate). */
void nb_kernel_reset(nb_kernel_t* k);

int nb_kernel_cell_count(nb_kernel_t* k);

#endif /* GMB_NOTEBOOK_KERNEL_H */
