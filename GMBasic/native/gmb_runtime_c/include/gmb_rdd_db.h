/* ============================================================================
 * gmb_rdd_db.h — RDD / SQL / VectorDB
 * File: native/gmb_runtime_c/include/gmb_rdd_db.h
 *
 * NON e' gmb_db.h (database su file: DBF/ISAM/SPEED/LEDGER). Questi moduli vivono in
 * libgmb_runtime e sono sempre linkati da C99 e VM/C. SQL e VectorDB usano
 * SQLite3 di sistema; RDD ARRAY/CSV sono in-process.
 * ============================================================================ */
#ifndef GMB_RDD_DB_H
#define GMB_RDD_DB_H

#include <stdint.h>
#include <stddef.h>

/* handle opaco sqlite3 (evita di includere sqlite3.h qui: chi usa gmb_sql_handle include
 * il proprio <sqlite3.h> per dereferenziare) */
struct sqlite3;
typedef struct sqlite3 sqlite3;

#ifdef __cplusplus
extern "C" {
#endif

/* ---- RDD: DataSession work area 1..255 (Clipper SELECT/USE) -------------- */
int  gmb_rdd_select(int area);
int  gmb_rdd_select_alias(const char* alias);
int  gmb_rdd_used(void);
int  gmb_rdd_selected_area(void);
int  gmb_rdd_use(const char* path, const char* alias, const char* driver);
int  gmb_rdd_close(int area);          /* 0 = area corrente */
int  gmb_rdd_set_fields(const char* names_csv);
int  gmb_rdd_gotop(void);
int  gmb_rdd_gobottom(void);
int  gmb_rdd_go(int recno);            /* 1-based */
int  gmb_rdd_skip(int n);
int  gmb_rdd_eof(void);
int  gmb_rdd_bof(void);
int  gmb_rdd_recno(void);
int  gmb_rdd_cur_is_dbf(void);   /* area RDD corrente = DBF (RDD USE x.dbf) */
int  gmb_rdd_lastrec(void);
int  gmb_rdd_field_count(void);
int  gmb_rdd_field_name(int idx, char* buf, int cap);
int  gmb_rdd_get_field(int idx, char* buf, int cap);
int  gmb_rdd_get_field_by_name(const char* name, char* buf, int cap);
int  gmb_rdd_append(const char* values_csv);
int  gmb_rdd_replace_field(int idx, const char* value);
int  gmb_rdd_replace_scoped(int idx, const char* value, int scope, int n, const char* cond);
int  gmb_rdd_area_is_text(void);
int  gmb_rdd_delete(void);            /* soft-delete (Clipper/C#); PACK per togliere le righe */
int  gmb_rdd_recall(void);
int  gmb_rdd_recall_scoped(const char* scope);
int  gmb_rdd_pack(void);
int  gmb_rdd_deleted(void);           /* 1 se il record corrente è marcato deleted */
int  gmb_rdd_set_deleted(int on);     /* SET DELETED ON|OFF: SKIP/GOTOP saltano i deleted */

int  gmb_rdd_set_filter(const char* field, const char* value); /* uguaglianza; field vuoto = CLEAR */
int  gmb_rdd_clear_filter(void);
int  gmb_rdd_zap(void);
int  gmb_rdd_found(void);
int  gmb_rdd_alias(char* buf, int cap);
int  gmb_rdd_driver(char* buf, int cap);
int  gmb_rdd_locate(const char* field, const char* value); /* 1=found, 0=no, -1=err */
int  gmb_rdd_set_order(const char* field); /* vuoto = ordine fisico */
int  gmb_rdd_seek(const char* value);      /* SEEK sul campo di SET ORDER, o 1° campo */
int  gmb_rdd_close_all(void);
int  gmb_rdd_copy_to(const char* path, const char* driver); /* CSV|SDF snapshot dell'area corrente */

/* ---- SQL: SQLite via alias (SQL OPEN / EXEC / QUERY) --------------------- */
int      gmb_sql_open(const char* path, const char* alias);
int      gmb_sql_close(const char* alias);
int      gmb_sql_exec(const char* alias, const char* sql);
int      gmb_sql_exec_params(const char* alias, const char* sql, const char* const* params, int n);
/* 1 = scrittura pesante (riscrittura file csv/parquet) da eseguire fuori dal worker GPM. */
int      gmb_sql_stmt_is_heavy_write(const char* alias, const char* sql);
/* Esecuzione NON-parking (usata dentro il job del hook). */
int      gmb_sql_exec_raw(const char* alias, const char* sql);
int      gmb_sql_exec_params_raw(const char* alias, const char* sql, const char* const* params, int n);
/* Hook di park (coarse) registrato dalla GPM del backend C99. */
typedef int (*gmb_sql_park_fn)(const char*, const char*, const char* const*, int);
void     gmb_sql_set_park_hook(gmb_sql_park_fn fn);
/* handle sqlite3* dell'area aperta (SQL OPEN) per alias — per chi deve operare sullo stesso
 * db (es. gmb_rag, parità con AiVectorRuntime C# che risolve l'alias nel SqlDriver). */
sqlite3* gmb_sql_handle(const char* alias);
int64_t  gmb_sql_query(const char* alias, const char* sql); /* handle result set; 0 = err */
int64_t  gmb_sql_query_params(const char* alias, const char* sql, const char* const* params, int n);
int      gmb_sql_fetch_next(int64_t rs); /* cursore SQL.FETCH: 1 = riga presente */
int      gmb_sql_row_count(int64_t rs);
int      gmb_sql_col_count(int64_t rs);
int      gmb_sql_fetch(int64_t rs, int row, int col, char* buf, int cap); /* row 0-based */
int      gmb_sql_fetch_by_name(int64_t rs, int row, const char* col, char* buf, int cap);
int      gmb_sql_col_name(int64_t rs, int col, char* buf, int cap);
int      gmb_sql_describe(int64_t rs, char* buf, int cap);              /* SQL.DESCRIBE: schema JSON */
int      gmb_sql_to_csv(int64_t rs, const char* path);                  /* SQL.TOCSV: rs -> CSV */
int      gmb_sql_from_csv(const char* alias, const char* table, const char* path); /* SQL.FROMCSV */
int      gmb_sql_close_rs(int64_t rs);
int      gmb_sql_begin(const char* alias);
int      gmb_sql_commit(const char* alias);
int      gmb_sql_rollback(const char* alias);
int64_t  gmb_sql_changes(const char* alias);
int64_t  gmb_sql_last_rowid(const char* alias);
int      gmb_sql_attach(const char* alias, const char* path, const char* schema);
int      gmb_sql_detach(const char* alias, const char* schema);
int      gmb_sql_table_count(const char* alias);
int      gmb_sql_table_name(const char* alias, int idx, char* buf, int cap);
int      gmb_sql_savepoint(const char* alias, const char* name);
int      gmb_sql_release(const char* alias, const char* name);
int      gmb_sql_rollback_to(const char* alias, const char* name);
int      gmb_sql_exec_scalar(const char* alias, const char* sql, char* buf, int cap);
const char* gmb_sql_last_error(void);

/* ---- VectorDB: tabelle vec_* su SQLite + embedding fixture (FNV+sin) ----- */
int64_t  gmb_vecdb_open(const char* sqlite_path);
int      gmb_vecdb_close(int64_t h);
int64_t  gmb_vecdb_ensure_collection(int64_t h, const char* name, int dims);
int      gmb_vecdb_index_text(int64_t h, const char* coll, const char* source_id, const char* text);
int      gmb_vecdb_delete_text(int64_t h, const char* coll, const char* source_id);
int      gmb_vecdb_count(int64_t h, const char* coll);
char*    gmb_vecdb_search(int64_t h, const char* coll, const char* query, int topk); /* malloc JSON */
char*    gmb_vecdb_list_collections(int64_t h); /* malloc JSON array di nomi */
int      gmb_vecdb_selfcheck(void);
int      gmb_rdd_selfcheck(void);
int      gmb_sql_selfcheck(void);
int      gmb_rdd_db_selfcheck(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_RDD_DB_H */

/* RDD campo-livello aggiuntive (parità IlRuntime.Rdd*): oggi/ctod/dtoc, fieldstate,
 * revert/update, lock. */
int  gmb_rdd_get_field_state(int idx);
int  gmb_rdd_table_revert(void);
int  gmb_rdd_table_update(void);
int  gmb_rdd_rlock(void);
int  gmb_rdd_flock(void);
int  gmb_rdd_unlock(void);                  /* rilascia il lock dell'area corrente: 1 = fatto */
int  gmb_rdd_set_lock_timeout(int ms);      /* SET LOCK TIMEOUT; ritorna il valore accettato */
/* Condizione di scope (`RDD … FOR <campo> <op> <letterale>`) sul record corrente. */
int  gmb_rdd_condition_match(const char* cond);   /* 1/0 = esito, -1 = non valutabile */
int  gmb_rdd_lock_timeout(void);            /* timeout corrente in ms */
int  gmb_rdd_set_reprocess(int ms);         /* SET REPROCESS; ritorna il valore accettato */
/* Registro eventi RDD (ON EVENT/OFF EVENT): stato reale e interrogabile (non no-op). */
int  gmb_rdd_onevent(const char* name);
int  gmb_rdd_offevent(const char* name);
int  gmb_rdd_event_count(void);
const char* gmb_rdd_event_at(int i);
int  gmb_rdd_reprocess(void);               /* valore corrente di REPROCESS in ms */
const char* gmb_rdd_oldval(int idx);
const char* gmb_rdd_today(void);
const char* gmb_rdd_ctod(const char* s);
const char* gmb_rdd_dtoc(const char* d);
int  gmb_rdd_field_index(const char* name);
int gmb_rdd_set_default(const char* driver);
const char* gmb_rdd_get_default(void);
