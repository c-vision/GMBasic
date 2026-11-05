/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gs_boost.h — gmbboost: gradient boosting (regressione, perdita quadratica) con i tre "engine"
 * che ricalcano le famiglie note:
 *   - GS_BOOST_XGB      crescita DEPTHWISE (come XGBoost di default);
 *   - GS_BOOST_LGBM     crescita LEAFWISE / best-first (come LightGBM);
 *   - GS_BOOST_CATBOOST alberi OBLIVIOUS/simmetrici (una soglia per livello, come CatBoost).
 * Self-contained (C99 + libm). Nessun tipo di dominio (fondazione gmbmaths). */
#ifndef GS_BOOST_H
#define GS_BOOST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { GS_BOOST_XGB = 0, GS_BOOST_LGBM = 1, GS_BOOST_CATBOOST = 2 } gs_boost_engine;

typedef struct gs_boost_model gs_boost_model;

/* Crea un modello. n_estimators = n. alberi; learning_rate (es. 0.1); max_depth (es. 3). */
gs_boost_model*   gs_boost_new(gs_boost_engine engine, int n_estimators, double learning_rate, int max_depth);
void        gs_boost_free(gs_boost_model*);

/* Allena (regressione, perdita quadratica). X = n × n_features row-major; y = n valori.
 * Ritorna 0 ok, -1 errore. */
int         gs_boost_fit(gs_boost_model*, const double* X, const double* y, int n, int n_features);

/* Predizione per una riga (n_features valori). */
double      gs_boost_predict(const gs_boost_model*, const double* x);

/* R² sui dati (0..1; 1 = perfetto). Ritorna NAN su errore. */
double      gs_boost_score(const gs_boost_model*, const double* X, const double* y, int n, int n_features);

int         gs_boost_n_trees(const gs_boost_model*);
const char* gs_boost_engine_name(gs_boost_engine);

#ifdef __cplusplus
}
#endif
#endif /* GS_BOOST_H */
