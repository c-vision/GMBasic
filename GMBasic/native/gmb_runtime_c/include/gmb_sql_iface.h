/* gmb_sql_iface.h — interfaccia core -> modulo SQL opzionale.
 *
 * Il core (gmb_c99gpm, gmb_vectordb) non linka direttamente sqlite/zstd: chiama
 * queste funzioni solo se il modulo SQL e' presente (installate al load). In un
 * eseguibile minimo senza SQL i puntatori sono NULL e la feature e' ASSENTE
 * (errore esplicito / selfcheck "non disponibile"), mai un finto successo. */
#ifndef GMB_SQL_IFACE_H
#define GMB_SQL_IFACE_H

typedef int  (*gmb_sql_exec_params_raw_fn)(const char* alias, const char* sql, const char* const* params, int n);
typedef void (*gmb_sql_set_park_hook_fn)(int (*fn)(const char*, const char*, const char* const*, int));
typedef int  (*gmb_sql_selfcheck_fn)(void);

typedef struct {
    gmb_sql_exec_params_raw_fn exec_params_raw;
    gmb_sql_set_park_hook_fn   set_park_hook;
    gmb_sql_selfcheck_fn       selfcheck;
} gmb_sql_iface_t;

extern gmb_sql_iface_t gmb_sql_iface;

#endif
