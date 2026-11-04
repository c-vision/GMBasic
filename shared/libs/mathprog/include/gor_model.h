/*
 * gor_model.h — modellazione solver-agnostica (DSL/IR) + JSON.
 *
 * Variabili (continue/intere/binarie), vincoli lineari, obiettivo. La solve
 * converte alla forma standard e chiama il MILP (gor_milp_bb). Export JSON.
 */
#ifndef GMBORTOOLS_GOR_MODEL_H
#define GMBORTOOLS_GOR_MODEL_H

#include "gor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { GOR_VAR_CONT = 0, GOR_VAR_INT = 1, GOR_VAR_BIN = 2 } gor_var_kind;
typedef enum { GOR_CMP_LE = 0, GOR_CMP_GE = 1, GOR_CMP_EQ = 2 } gor_cmp;
typedef enum { GOR_OBJ_MIN = 0, GOR_OBJ_MAX = 1 } gor_obj_sense;

typedef struct gor_model gor_model;

gor_model *gor_model_new(void);
void gor_model_free(gor_model *m);

/* Aggiunge una variabile; ritorna l'indice (>=0) o -1. */
int gor_model_add_var(gor_model *m, gor_var_kind kind, double lo, double hi, const char *name);
/* Vincolo lineare: Σ coef[k] * x[vars[k]]  cmp  rhs. Ritorna 0/-1. */
int gor_model_add_constr(gor_model *m, int ncoef, const int *vars, const double *coef,
                         gor_cmp cmp, double rhs);
/* Obiettivo: sense su Σ coef[k]*x[vars[k]]. */
void gor_model_set_obj(gor_model *m, int ncoef, const int *vars, const double *coef,
                       gor_obj_sense sense);

/* Risolve (MILP). Ritorna 0 ottimo, 1 inammissibile, 2 illimitato, 3 nodi. */
int gor_model_solve(gor_model *m, double *obj);
double gor_model_var_value(const gor_model *m, int i);
int gor_model_num_vars(const gor_model *m);

/* Esporta il modello in JSON (stringa allocata, da liberare). NULL su errore. */
char *gor_model_to_json(const gor_model *m);

#ifdef __cplusplus
}
#endif

#endif /* GMBORTOOLS_GOR_MODEL_H */
