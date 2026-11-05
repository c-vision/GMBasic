/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gm_sym.h — gmbsym: algebra simbolica (CAS) minimale ma reale: parsing di espressioni,
 * semplificazione, derivata simbolica, valutazione numerica, stampa. Self-contained (C99 + libm).
 * Nessun tipo di dominio (fondazione gmbmaths). */
#ifndef GM_SYM_H
#define GM_SYM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_sym_node gm_sym_node;

/* Parsing di un'espressione infissa: + - * / ^, parentesi, unario -, numeri, variabili,
 * funzioni sin/cos/exp/log. Ritorna NULL su errore (messaggio in err, se non NULL). */
gm_sym_node* gm_sym_parse(const char* text, char* err, int errcap);

/* Costruttori (i figli sono copiati in una struttura immutabile a valle). */
gm_sym_node* gm_sym_const(double v);
gm_sym_node* gm_sym_var(const char* name);
gm_sym_node* gm_sym_add(const gm_sym_node* a, const gm_sym_node* b);
gm_sym_node* gm_sym_sub(const gm_sym_node* a, const gm_sym_node* b);
gm_sym_node* gm_sym_mul(const gm_sym_node* a, const gm_sym_node* b);
gm_sym_node* gm_sym_div(const gm_sym_node* a, const gm_sym_node* b);
gm_sym_node* gm_sym_pow(const gm_sym_node* a, const gm_sym_node* b);
gm_sym_node* gm_sym_neg(const gm_sym_node* a);
gm_sym_node* gm_sym_sin(const gm_sym_node* a);
gm_sym_node* gm_sym_cos(const gm_sym_node* a);
gm_sym_node* gm_sym_exp(const gm_sym_node* a);
gm_sym_node* gm_sym_log(const gm_sym_node* a);

/* Operazioni. Ritornano un NUOVO albero (non liberano l'input). */
gm_sym_node* gm_sym_simplify(const gm_sym_node* e);
gm_sym_node* gm_sym_diff(const gm_sym_node* e, const char* var);

double    gm_sym_eval(const gm_sym_node* e, const char* var, double value); /* variabile singola */
char*     gm_sym_tostring(const gm_sym_node* e);                            /* malloc'd, forma infissa */
void      gm_sym_free(gm_sym_node* e);

#ifdef __cplusplus
}
#endif
#endif /* GM_SYM_H */
