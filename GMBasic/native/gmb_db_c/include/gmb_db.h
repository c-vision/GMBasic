/* ============================================================================
 * gmb_db.h — ABI commerciale DBF / ISAM / SPEED / LEDGER (D8/D11)
 * File: native/gmb_db_c/include/gmb_db.h
 *
 * NON è il runtime community. I stub gmb_dbf_* in gmb_runtime.h restano stub.
 * Handle opachi int64_t. Errori: codice + gmb_db_last_error. Niente eccezioni.
 * ============================================================================ */

#ifndef GMB_DB_H
#define GMB_DB_H

#include <stdint.h>
#include <stddef.h>
#include "gmb_runtime.h"
/* Ordinali TokenKind usati dall'ABI DB (GMB_DB_TOK_*): GENERATI da TokenKind.cs. */
#include "gmb_db_tokenkind_gen.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(GMB_DB_BUILD_SHARED)
#  define GMB_DB_API __declspec(dllexport)
#else
#  define GMB_DB_API
#endif

#define GMB_DB_OK          0
#define GMB_DB_ERR        -1
#define GMB_DB_NOTFOUND    1
#define GMB_DB_VERSION     0x00010000
#define GMB_DB_LEDGER_NAME 128

/* SysCall ordinals — keep in sync with Compiler.SysCall (SysCalls.cs) */
#define GMB_SYS_ISAM_USE        65
#define GMB_SYS_ISAM_SELECT     66
#define GMB_SYS_ISAM_SET_RELATION 80
#define GMB_SYS_ISAM_JOIN_WITH   85
#define GMB_SYS_ISAM_RECORD     67
#define GMB_SYS_ISAM_SKIP       68
/* `GMB_SYS_ISAM_RECORD` (ISAM GO) e `GMB_SYS_SPEED_GO` (SPEED GO) trasportano
 * `stmt.Location.Kind`, che il compilatore C# emette come valore GREZZO dell'enum
 * `TokenKind` (`Compiler.Isam.cs` / `Compiler.Speed.cs`: EmitConstant((double)kind)),
 * cioè la posizione del membro in common/Ast/TokenKind.cs.
 *
 * Qui NON si scrivono numeri: gli ordinali arrivano da `gmb_db_tokenkind_gen.h`, GENERATO
 * da TokenKind.cs da `tools/gen_db_tokenkind.bas` (che con `--check` esce 1 sul drift).
 * Scritti a mano si sono disallineati tre volte (498 → 479 → 480, mentre i valori reali
 * erano 480/481 e poi 482/483): `TOP` finiva nel ramo BOTTOM e `BOTTOM` in un no-op muto
 * senza alcun errore (bug VM-C-Ordinali-TokenKind-Hardcoded-Stale). */
#define GMB_SYS_ISAM_SEEK       69
#define GMB_SYS_ISAM_DELETE     70
#define GMB_SYS_ISAM_RECALL     71
#define GMB_SYS_ISAM_PACK       72
#define GMB_SYS_ISAM_REINDEX    73
#define GMB_SYS_ISAM_REPAIR     74
#define GMB_SYS_ISAM_BACKUP     75
#define GMB_SYS_ISAM_APPEND     76
#define GMB_SYS_ISAM_LOCK       77
#define GMB_SYS_ISAM_UNLOCK     78
#define GMB_SYS_ISAM_SET_ORDER  79
#define GMB_SYS_ISAM_CREATE_INDEX 81
#define GMB_SYS_ISAM_DROP_INDEX 82
#define GMB_SYS_ISAM_VALIDATE   83
#define GMB_SYS_ISAM_COPY_TO    84
#define GMB_SYS_ISAM_SORT_TO    86
#define GMB_SYS_ISAM_SET_FILTER 87
#define GMB_SYS_ISAM_INTEGRITY  88
#define GMB_SYS_ISAM_INFO       89
#define GMB_SYS_ISAM_EOF        90
#define GMB_SYS_ISAM_BOF        91
/* SysCall.ISAM_RECNO (common/Compiler/SysCalls.cs) = 357: ISAM.RECNO(alias). Il valore
 * era fissato a 355 ma l'enum C# e' cresciuto (2 voci inserite prima di ISAM_RECNO) →
 * il chunk emetteva 357 e la route table VM/C (355) non lo instradava → RECNO sempre 0. */
#define GMB_SYS_ISAM_RECNO      357
#define GMB_SYS_ISAM_PUT        106
#define GMB_SYS_ISAM_APPENDMANY 110
/* SysCall.ISAM_CLOSE (common/Compiler/SysCalls.cs, valore ESPLICITO 440): ISAM CLOSE [alias] chiude l'area. */
#define GMB_SYS_ISAM_CLOSE      440
#define GMB_SYS_ISAM_SET_REPLICATION 107
#define GMB_SYS_ISAM_APPLY_REPLICATION 108
#define GMB_SYS_ISAM_FOLLOW_REPLICATION 109
#define GMB_SYS_BEGIN_TRANSACTION 111
#define GMB_SYS_COMMIT          112
#define GMB_SYS_ROLLBACK        113
#define GMB_SYS_DBF_USE         117
#define GMB_SYS_DBF_GET_RECORD  119
#define GMB_SYS_DBF_SELECT      118
#define GMB_SYS_DBF_SORT        123
#define GMB_SYS_DBF_UPDATE      124
#define GMB_SYS_DBF_TOTAL_ON    125
#define GMB_SYS_DBF_CLOSE_DATABASES 131
#define GMB_SYS_DBF_APPEND_FROM 137
#define GMB_SYS_DBF_JOIN_WITH   138
#define GMB_SYS_DBF_COPY_STRUCTURE 139
#define GMB_SYS_DBF_SET_RELATION 141
#define GMB_SYS_DBF_INDEX_ON    142
#define GMB_SYS_DBF_SET_DEFAULT 144
#define GMB_SYS_DBF_SET_PATH    145
#define GMB_SYS_DBF_SET_EXCLUSIVE 146
#define GMB_SYS_DBF_SET_SCHEMA  147
#define GMB_SYS_CONVERT_DBF_TO_ISAM 116
#define GMB_SYS_DBF_CONVERT_TO_ISAM 150
#define GMB_SYS_DBF_APPENDMANY  154
#define GMB_SYS_DBF_SET_DURABILITY 155
#define GMB_SYS_DBF_SET_REPLICATION 159
#define GMB_SYS_DBF_APPLY_REPLICATION 160
#define GMB_SYS_DBF_FOLLOW_REPLICATION 161
#define GMB_SYS_DBF_CREATE_TABLE 153
#define GMB_SYS_DBF_APPEND_RECORD 136
#define GMB_SYS_DBF_REPLACE     120
#define GMB_SYS_DBF_DELETE      121
#define GMB_SYS_DBF_RECALL      122
#define GMB_SYS_DBF_PACK        126
#define GMB_SYS_DBF_ZAP         127
#define GMB_SYS_DBF_GO          128
#define GMB_SYS_DBF_SKIP        129
#define GMB_SYS_DBF_SEEK        130
#define GMB_SYS_DBF_SET_DELETED 132
#define GMB_SYS_DBF_SET_FILTER  133
#define GMB_SYS_DBF_COPY_TO     134
#define GMB_SYS_DBF_REINDEX     135
#define GMB_SYS_DBF_SET_ORDER   140
#define GMB_SYS_DBF_SET_EXACT   143
#define GMB_SYS_DBF_APPEND_BLANK 148
#define GMB_SYS_DBF_VALIDATE    149
#define GMB_SYS_DBF_RECNO       151
#define GMB_SYS_DBF_LASTREC     152
#define GMB_SYS_DBF_BEGIN_GROUP_COMMIT 156
#define GMB_SYS_DBF_END_GROUP_COMMIT 157
#define GMB_SYS_DBF_LUPDATE       162
#define GMB_SYS_DBF_CLOSE_CURRENT 158
#define GMB_SYS_SPEED_OPEN      163
#define GMB_SYS_SPEED_USE       164
#define GMB_SYS_SPEED_CREATE_INDEX 165
#define GMB_SYS_SPEED_DROP_INDEX 166
#define GMB_SYS_SPEED_DELETE_RECORD 169
#define GMB_SYS_SPEED_SET_FILTER 174
#define GMB_SYS_SPEED_SET_RELATION 175
#define GMB_SYS_SPEED_CREATE_VIEW 177
#define GMB_SYS_SPEED_REFRESH_VIEW 178
#define GMB_SYS_SPEED_DROP_VIEW  179
#define GMB_SYS_SPEED_PUT       168
#define GMB_SYS_SPEED_GET       167
#define GMB_SYS_SPEED_SEEK      170
#define GMB_SYS_SPEED_GO        171
#define GMB_SYS_SPEED_SKIP      172
#define GMB_SYS_SPEED_SET_ORDER 173
#define GMB_SYS_SPEED_SETMANY    180
#define GMB_SYS_SPEED_SELECT    176
#define GMB_SYS_SPEED_CLOSE     383
#define GMB_SYS_LEDGER_OPEN     181
#define GMB_SYS_LEDGER_ADD      182
#define GMB_SYS_LEDGER_EDIT     186
#define GMB_SYS_LEDGER_BUDGET   193
#define GMB_SYS_LEDGER_VARIANCE 194
#define GMB_SYS_LEDGER_ACCOUNTS 191
#define GMB_SYS_LEDGER_BALANCE  184
#define GMB_SYS_LEDGER_REGISTER 183
#define GMB_SYS_LEDGER_PLOT     195
#define GMB_SYS_LEDGER_GET      185
#define GMB_SYS_LEDGER_DELETE   187
#define GMB_SYS_LEDGER_SAVE     188
#define GMB_SYS_LEDGER_CLOSE    189
#define GMB_SYS_LEDGER_PACK     190
#define GMB_SYS_LEDGER_VALIDATE 192
#define GMB_SYS_LEDGER_IMPORT   196

/* DBF report/query/listing (Cat3) — valori = posizioni enum C# (fine) */
#define GMB_SYS_DBF_LIST         363
#define GMB_SYS_DBF_CREATE_VIEW  364
#define GMB_SYS_DBF_AGGREGATE    365
#define GMB_SYS_DBF_REPORT       366

GMB_DB_API int32_t  gmb_db_version(void);
GMB_DB_API int      gmb_db_last_error(char* buf, int cap);

/* ---- DBF (dBase III/IV / VFP header) ----------------------------------- */
GMB_DB_API int64_t  gmb_db_dbf_create(const char* path, const char* field_defs);
GMB_DB_API int64_t  gmb_db_dbf_open(const char* path);
GMB_DB_API int      gmb_db_dbf_close(int64_t h);
GMB_DB_API int      gmb_db_dbf_close_all(void);
GMB_DB_API int      gmb_db_dbf_gotop(int64_t h);
GMB_DB_API int      gmb_db_dbf_gobottom(int64_t h);
GMB_DB_API int      gmb_db_dbf_go(int64_t h, int recno);
GMB_DB_API int      gmb_db_dbf_skip(int64_t h, int n);
GMB_DB_API int      gmb_db_dbf_eof(int64_t h);
GMB_DB_API int      gmb_db_dbf_bof(int64_t h);
GMB_DB_API int      gmb_db_dbf_found(int64_t h);
GMB_DB_API int      gmb_db_dbf_recno(int64_t h);
GMB_DB_API int      gmb_db_dbf_lastrec(int64_t h);
GMB_DB_API int      gmb_db_dbf_deleted(int64_t h);
GMB_DB_API int      gmb_db_dbf_field_count(int64_t h);
GMB_DB_API int      gmb_db_dbf_field_name(int64_t h, int idx, char* buf, int cap);
GMB_DB_API int      gmb_db_dbf_lookup(int64_t h, const char* retField, const char* searchVal, const char* searchField, char* out, int cap);
GMB_DB_API int      gmb_db_dbf_record_size(int64_t h);
GMB_DB_API int      gmb_db_dbf_relation_key(int64_t h, char* buf, int cap);
GMB_DB_API int      gmb_db_dbf_field_type(int64_t h, int idx);
GMB_DB_API int      gmb_db_dbf_get_field(int64_t h, int idx, char* buf, int cap);
GMB_DB_API int      gmb_db_dbf_get_field_name(int64_t h, const char* name, char* buf, int cap);
GMB_DB_API gmb_val_t gmb_db_dbf_field_val_name(int64_t h, const char* name);
GMB_DB_API int      gmb_db_dbf_append(int64_t h, const char* values_csv);
GMB_DB_API int      gmb_db_dbf_append_many(int64_t h, gmb_list_t list);
GMB_DB_API int      gmb_db_dbf_append_many_boxed(int64_t h, const gmb_val_t* vals, int n);
GMB_DB_API int      gmb_db_isam_append_many_boxed(int64_t h, const gmb_val_t* vals, int n);
GMB_DB_API int      gmb_db_dbf_replace_field(int64_t h, int idx, const char* value);
GMB_DB_API int      gmb_db_dbf_delete(int64_t h);
GMB_DB_API int      gmb_db_dbf_recall(int64_t h);
GMB_DB_API int      gmb_db_dbf_pack(int64_t h);
GMB_DB_API int      gmb_db_dbf_index_on(int64_t h, const char* field, const char* index_path, int unique);
GMB_DB_API int      gmb_db_dbf_append_blank(int64_t h);
GMB_DB_API int      gmb_db_dbf_append_from(int64_t h, const char* sourceFilePath);
GMB_DB_API int      gmb_db_dbf_append_from_cond(int64_t h, const char* sourceFilePath, const char* condition);
GMB_DB_API int      gmb_db_dbf_copy_to(int64_t h, const char* destPath);
GMB_DB_API int      gmb_db_dbf_sort_to(int64_t h, const char* destPath, const char* fieldName, int descending);
GMB_DB_API int      gmb_db_dbf_sort_to_multi(int64_t h, const char* destPath, const char* fieldsCsv, const char* descCsv);
GMB_DB_API int      gmb_db_dbf_copy_structure(int64_t h, const char* newFilePath);
GMB_DB_API int      gmb_db_dbf_total_on(int64_t h, const char* destPath, const char* keyField, const char* sumFieldsCsv);
GMB_DB_API int      gmb_db_dbf_total_on_cond(int64_t h, const char* destPath, const char* keyField, const char* sumFieldsCsv, const char* condition);
GMB_DB_API int      gmb_db_dbf_zap(int64_t h);
GMB_DB_API int      gmb_db_dbf_set_filter(int64_t h, const char* expression);
GMB_DB_API int      gmb_db_dbf_get_filter(int64_t h, char* out, int outCap);
GMB_DB_API gmb_string_t gmb_db_dbf_get_filter_str(int64_t h);
GMB_DB_API int      gmb_db_dbf_set_deleted(int64_t h, int on);
GMB_DB_API int      gmb_db_dbf_set_exact(int64_t h, int on);
GMB_DB_API int      gmb_db_dbf_set_replication(int64_t h, const char* logPath);
GMB_DB_API int      gmb_db_dbf_apply_replication(int64_t h, const char* replLogPath);
GMB_DB_API int      gmb_db_dbf_replication_ship(int64_t h, const char* host, int port);
GMB_DB_API int      gmb_db_dbf_replication_recv(int64_t h, int port);
GMB_DB_API int      gmb_db_isam_replication_ship(int64_t h, const char* host, int port);
GMB_DB_API int      gmb_db_isam_replication_recv(int64_t h, int port);
GMB_DB_API int      gmb_db_dbf_follow_replication(int64_t h, const char* replLogPath, const char* checkpointPath);
GMB_DB_API int      gmb_db_dbf_rlock(int64_t h);
GMB_DB_API int      gmb_db_dbf_flock(int64_t h);
GMB_DB_API int      gmb_db_dbf_begin_group_commit(int64_t h);
GMB_DB_API int      gmb_db_dbf_end_group_commit(int64_t h);
GMB_DB_API int      gmb_db_dbf_begin_transaction(int64_t h);
GMB_DB_API int      gmb_db_dbf_commit(int64_t h);
GMB_DB_API int      gmb_db_dbf_rollback(int64_t h);
GMB_DB_API int      gmb_db_dbf_fields(int64_t h, char* out, int outCap);
GMB_DB_API int      gmb_db_dbf_lupdate(int64_t h, char* out, int outCap);
GMB_DB_API int      gmb_db_dbf_set_default_path(const char* path);
GMB_DB_API int      gmb_db_dbf_validate(int64_t h);
GMB_DB_API int      gmb_db_dbf_set_durability(int64_t h, const char* mode);
GMB_DB_API int      gmb_db_dbf_flush_durability(int64_t h);
GMB_DB_API int      gmb_db_dbf_update(int64_t h, const char* fieldsCsv, const char* exprsCsv);
GMB_DB_API int      gmb_db_dbf_update_on(int64_t dest_h, int64_t src_h, const char* src_alias,
                                          const char* keyField, const char* fieldsCsv,
                                          const char* exprsCsv);
GMB_DB_API int      gmb_db_dbf_join_with(int64_t h1, int64_t h2, const char* destPath, const char* condition);
GMB_DB_API int      gmb_db_dbf_set_exclusive(int64_t h, int on);
GMB_DB_API int      gmb_db_dbf_set_schema_strict(int64_t h, int on);
GMB_DB_API int      gmb_db_dbf_set_relation(int64_t child, int64_t parent, const char* keyExpr);
GMB_DB_API int      gmb_db_dbf_clear_relation(int64_t child);
GMB_DB_API int      gmb_db_dbf_sync_relations(int64_t parent);
GMB_DB_API int      gmb_db_dbf_list(int64_t h, const char* fieldsCsv, const char* destination, int toPrint);
GMB_DB_API int      gmb_db_dbf_create_view(const char* viewName);
GMB_DB_API int      gmb_db_dbf_drop_view(const char* viewName);
GMB_DB_API int      gmb_db_dbf_aggregate(int64_t h, const char* keyField, const char* destPath, const char* aggregations);
GMB_DB_API int      gmb_db_dbf_report(int64_t h, const char* formFile, const char* destination, int toPrint);
GMB_DB_API int      gmb_db_isam_set_relation(int64_t child, int64_t parent, const char* keyField);
GMB_DB_API int      gmb_db_isam_clear_relation(int64_t child);
GMB_DB_API int      gmb_db_isam_sync_relations(int64_t parent);
GMB_DB_API int      gmb_db_speed_set_relation(int64_t child, int64_t parent);
GMB_DB_API int      gmb_db_speed_clear_relation(int64_t child);
GMB_DB_API int      gmb_db_speed_sync_relations(int64_t parent);
GMB_DB_API int      gmb_db_speed_set_durability(int64_t h, const char* mode);
GMB_DB_API int      gmb_db_speed_flush_durability(int64_t h);
GMB_DB_API int      gmb_db_speed_begin_group_commit(int64_t h);
GMB_DB_API int      gmb_db_speed_end_group_commit(int64_t h);
GMB_DB_API gmb_string_t gmb_db_speed_set_durability_str(int64_t h, const char* mode);
GMB_DB_API gmb_string_t gmb_db_speed_info_str(int64_t h);
GMB_DB_API int      gmb_db_speed_register_table_alias(const char* alias, int tableId);
GMB_DB_API int      gmb_db_speed_table_by_alias(const char* alias);
GMB_DB_API int      gmb_db_speed_set_relation_table(int64_t h, int parentTable, int childTable);
GMB_DB_API int      gmb_db_speed_create_view(const char* viewName, int fromTableId);
GMB_DB_API int      gmb_db_speed_refresh_view(const char* viewName);
GMB_DB_API int      gmb_db_speed_drop_view(const char* viewName);
GMB_DB_API int      gmb_db_speed_view_table(const char* viewName);
GMB_DB_API int      gmb_db_speed_curtable(int64_t h);
GMB_DB_API int      gmb_db_dbf_replace_field_name(int64_t h, const char* fieldName, const char* value);

GMB_DB_API int      gmb_db_dbf_set_order(int64_t h, int order);
GMB_DB_API int      gmb_db_dbf_set_order_by_name(int64_t h, const char* name);
GMB_DB_API int      gmb_db_dbf_seek(int64_t h, const char* key);
GMB_DB_API int      gmb_db_dbf_reindex(int64_t h);

/* ---- SPEED (logical k/v; WAL proprio) ---------------------------------- */
GMB_DB_API int64_t  gmb_db_speed_open(const char* dir);
GMB_DB_API int      gmb_db_speed_close(int64_t h);
GMB_DB_API int      gmb_db_speed_set_many(int64_t h, gmb_list_t list, const char* alias);
GMB_DB_API int      gmb_db_speed_set_filter(int64_t h, const char* filterExpr);
GMB_DB_API int      gmb_db_speed_set(int64_t h, int table_id, const void* key, int klen,
                                     const void* val, int vlen);
GMB_DB_API int      gmb_db_speed_get(int64_t h, int table_id, const void* key, int klen,
                                     void* val, int* vlen);
GMB_DB_API int      gmb_db_speed_delete(int64_t h, int table_id, const void* key, int klen);
GMB_DB_API int      gmb_db_speed_compact(int64_t h);
GMB_DB_API int      gmb_db_speed_use(int64_t h, int tableId);
GMB_DB_API int      gmb_db_speed_create_table(int64_t h, const char* typeName, const char* fieldsCsv);
GMB_DB_API int      gmb_db_speed_drop_table(int64_t h, const char* tableName);
GMB_DB_API int      gmb_db_speed_create_index(int64_t h, int tableId, const char* indexName, const char* keyExpression, int keyLength);
GMB_DB_API int      gmb_db_speed_drop_index(int64_t h, int tableId, const char* indexName);
GMB_DB_API int      gmb_db_speed_set_order(int64_t h, int tableId, const char* indexName);
GMB_DB_API int      gmb_db_speed_go_top(int64_t h, int tableId);
GMB_DB_API int      gmb_db_speed_go_bottom(int64_t h, int tableId);
GMB_DB_API int      gmb_db_speed_skip(int64_t h, int tableId, int recordCount);
GMB_DB_API int      gmb_db_speed_seek(int64_t h, int tableId, const void* key, int keyLen, const char* indexName);
GMB_DB_API int      gmb_db_speed_get_current(int64_t h, int tableId, void* val, int* vlen);
GMB_DB_API int      gmb_db_speed_eof(int64_t h, int tableId);
GMB_DB_API int      gmb_db_speed_validate(int64_t h, char* out, int outCap);
/* Motore di query SPEED (port JSON di SpeedExecuteSelect): piano JSON → array JSON di righe.
 * Vedi native/gmb_db_c/src/speed/gmb_speed_query.c per il formato del piano. */
GMB_DB_API int      gmb_db_speed_select(int64_t h, const char* planJson, gmb_string_t* outJson);


/* ---- LEDGER (journal UTF-8 + .idx LDGIDX) ------------------------------ */
GMB_DB_API int64_t  gmb_db_ledger_open(const char* journal_path);
GMB_DB_API int      gmb_db_ledger_close(int64_t h);
GMB_DB_API int64_t  gmb_db_ledger_add(int64_t h, const char* date_ymd, const char* desc,
                                      const char* const* accounts, const double* amounts, int n);
GMB_DB_API int64_t  gmb_db_ledger_add_csv(int64_t h, const char* date_ymd, const char* desc,
                                          const char* accounts_csv, const char* amounts_csv);
GMB_DB_API int      gmb_db_ledger_count(int64_t h);
GMB_DB_API int      gmb_db_ledger_get(int64_t h, int64_t tx_id, char* date_buf, int dcap,
                                      char* desc_buf, int cap);

/* ---- LEDGER cursore + operazioni estese (port C# ILBackend 2026-08-24) ------
   Cursore = posizione nella lista transazioni ORDINATA per ID (pattern DBF/ISAM):
   -1 = BOF, nIndex = EOF. EOF/BOF/FOUND ritornano 1 (vero) / 0. ----------------- */
GMB_DB_API int      gmb_db_ledger_go_top(int64_t h);
GMB_DB_API int      gmb_db_ledger_go_bottom(int64_t h);
GMB_DB_API int      gmb_db_ledger_go(int64_t h, int64_t tx_id);
GMB_DB_API int      gmb_db_ledger_skip(int64_t h, int n);
GMB_DB_API int      gmb_db_ledger_eof(int64_t h);
GMB_DB_API int      gmb_db_ledger_bof(int64_t h);
GMB_DB_API int64_t  gmb_db_ledger_recno(int64_t h);
GMB_DB_API int      gmb_db_ledger_found(int64_t h);
GMB_DB_API int      gmb_db_ledger_get_current(int64_t h, char* date_buf, int dcap,
                                              char* desc_buf, int cap);
GMB_DB_API int64_t  gmb_db_ledger_edit(int64_t h, int64_t tx_id, const char* date_ymd,
                                       const char* desc, const char* const* accounts,
                                       const double* amounts, int n);
GMB_DB_API int      gmb_db_ledger_delete(int64_t h, int64_t tx_id);
GMB_DB_API int      gmb_db_ledger_save(int64_t h);
GMB_DB_API int      gmb_db_ledger_pack(int64_t h);
GMB_DB_API int      gmb_db_ledger_validate(int64_t h);
/* Varianti VALORE (liste) per l'emitter C99Native: gli array del ledger come liste, non JSON. */
GMB_DB_API gmb_val_t gmb_db_ledger_accounts_val(int64_t h, const char* root);
GMB_DB_API gmb_val_t gmb_db_ledger_balance_val(int64_t h, const char* query);
GMB_DB_API gmb_val_t gmb_db_ledger_register_val(int64_t h, const char* query);
GMB_DB_API int      gmb_db_ledger_budget(int64_t h, const char* account, double amount,
                                         const char* period);
GMB_DB_API int      gmb_db_ledger_variance(int64_t h, const char* account, const char* period,
                                           double* var_out);
GMB_DB_API int      gmb_db_ledger_get_json(int64_t h, int64_t tx_id, char* out, int outCap);
/* GET come lista (parità IL object?[]): elementi = movimenti della transazione. */
GMB_DB_API gmb_val_t gmb_db_ledger_get_val(int64_t h, int64_t tx_id);
GMB_DB_API int      gmb_db_ledger_plot(int64_t h, const char* query, char labels[][GMB_DB_LEDGER_NAME],
                                       double* amounts, int max, int* n_out);
GMB_DB_API int      gmb_db_ledger_import(int64_t h, const char* csv_path, const char* mapping_rules);
GMB_DB_API gmb_string_t gmb_db_dbf_lupdate_str(int64_t h);
GMB_DB_API gmb_string_t gmb_db_dbf_field_name_str(int64_t h, int idx);
GMB_DB_API gmb_string_t gmb_db_dbf_relation_key_str(int64_t h);
GMB_DB_API gmb_string_t gmb_db_dbf_set_replication_str(int64_t h, const char* logPath);
GMB_DB_API int         gmb_db_dbf_set_alias(int64_t h, const char* alias);
GMB_DB_API int         gmb_db_dbf_get_alias(int64_t h, char* out, int cap);
GMB_DB_API int64_t     gmb_db_dbf_find_handle_by_alias(const char* alias);
GMB_DB_API gmb_double_t gmb_indexord_dbl(int area);
GMB_DB_API gmb_string_t gmb_db_dbf_get_alias_str(int64_t h);
GMB_DB_API gmb_string_t gmb_alias(int area);
GMB_DB_API gmb_double_t gmb_selected(const char* alias);
/* Numero d'area 1-based dell'area DBF corrente / di un alias (0 = nessuna). Ordine di
 * apertura, parità IL/PCode; lo usa SELECTED() su VM/C e C99Native. */
GMB_DB_API int gmb_db_current_area(void);
GMB_DB_API int gmb_db_area_of_alias(const char* alias);
GMB_DB_API gmb_string_t gmb_indexord(int area);
GMB_DB_API gmb_string_t gmb_indexkey(int area);
GMB_DB_API gmb_double_t gmb_neterr(int area);
GMB_DB_API gmb_string_t gmb_fieldtype(int idx);
GMB_DB_API gmb_string_t gmb_db_dbf_lookup_str(int64_t h, const char* retField, const char* searchVal, const char* searchField);
GMB_DB_API gmb_string_t gmb_db_isam_set_replication_str(int64_t h, const char* logPath);
GMB_DB_API gmb_string_t gmb_db_isam_integrity_str(int64_t h);
GMB_DB_API gmb_string_t gmb_db_ledger_register_str(int64_t h, const char* query);
GMB_DB_API gmb_string_t gmb_db_ledger_balance_str(int64_t h, const char* query);
GMB_DB_API gmb_string_t gmb_db_ledger_accounts_str(int64_t h, const char* root);
GMB_DB_API gmb_string_t gmb_db_ledger_plot_json(int64_t h, const char* query);
GMB_DB_API int      gmb_db_ledger_accounts(int64_t h, const char* root, char* out, int outCap);
GMB_DB_API int      gmb_db_ledger_balance(int64_t h, const char* query, char* out, int outCap);
GMB_DB_API int      gmb_db_ledger_register(int64_t h, const char* query, char* out, int outCap);


/* ---- ISAM (heap + sorted PK; convert da DBF) --------------------------- */
GMB_DB_API int      gmb_db_isam_create(const char* path, const char* schema);
GMB_DB_API int64_t  gmb_db_isam_open(const char* path);
GMB_DB_API int      gmb_db_isam_close(int64_t h);
GMB_DB_API void     gmb_db_isam_set_alias(int64_t h, const char* alias);
GMB_DB_API int64_t  gmb_db_isam_select(const char* alias);
GMB_DB_API int64_t  gmb_db_isam_selected(const char* alias);   /* read-only */
GMB_DB_API int      gmb_db_isam_insert(int64_t h, const char* values_csv);
GMB_DB_API int      gmb_db_isam_go(int64_t h, int recno);
GMB_DB_API int      gmb_db_isam_skip(int64_t h, int n);
GMB_DB_API int      gmb_db_isam_seek(int64_t h, const char* key);
GMB_DB_API int      gmb_db_isam_eof(int64_t h);
GMB_DB_API int      gmb_db_isam_found(int64_t h);
GMB_DB_API int      gmb_db_isam_flock(int64_t h);
GMB_DB_API int      gmb_db_isam_funlock(int64_t h);
GMB_DB_API int      gmb_db_isam_recsize(int64_t h);
GMB_DB_API int64_t  gmb_db_isam_handle_for(const char* alias);
GMB_DB_API int      gmb_db_isam_count(int64_t h);
GMB_DB_API int      gmb_db_isam_read(int64_t h, int field_idx, char* buf, int cap);
/* Tipo del campo ISAM i-esimo ('N'/'L'/'D'/'C' — stessa convenzione di
 * gmb_db_dbf_field_type). Permette a chi legge un record ISAM (gmb_syscall_fileio.c,
 * GET_FILE) di popolare l'UDT col tipo giusto (double vs stringa) invece di sempre testo. */
GMB_DB_API int      gmb_db_isam_field_type(int64_t h, int idx);
GMB_DB_API int      gmb_db_isam_pack(int64_t h);
GMB_DB_API int      gmb_db_isam_reindex(int64_t h);
GMB_DB_API int      gmb_db_isam_import_dbf(int64_t h, const char* dbf_path);
GMB_DB_API int      gmb_db_isam_integrity(int64_t h, char* out, int outCap);
GMB_DB_API int      gmb_db_isam_fields(int64_t h, char* out, int outCap);
GMB_DB_API int      gmb_db_isam_go_top(int64_t h);
GMB_DB_API int      gmb_db_isam_go_bottom(int64_t h);
GMB_DB_API int      gmb_db_isam_bof(int64_t h);
GMB_DB_API int64_t  gmb_db_isam_recno(int64_t h);
GMB_DB_API int      gmb_db_isam_put(int64_t h, const char* values_csv);
GMB_DB_API int      gmb_db_isam_delete(int64_t h);
GMB_DB_API int      gmb_db_isam_recall(int64_t h);
GMB_DB_API int      gmb_db_isam_appendmany(int64_t h, const char* records_csv);
GMB_DB_API int      gmb_db_isam_append_many(int64_t h, gmb_list_t list);
GMB_DB_API int      gmb_db_isam_zap(int64_t h);
GMB_DB_API int      gmb_db_isam_set_filter(int64_t h, const char* expression);
GMB_DB_API int      gmb_db_isam_set_order(int64_t h, const char* indexName);
GMB_DB_API int      gmb_db_isam_create_index(int64_t h, const char* indexName, const char* keyExpression, int keyLength);
GMB_DB_API int      gmb_db_isam_drop_index(int64_t h, const char* indexName);
GMB_DB_API int      gmb_db_isam_lock(int64_t h);
GMB_DB_API int      gmb_db_isam_unlock(int64_t h);
GMB_DB_API int      gmb_db_isam_validate(int64_t h);
GMB_DB_API int      gmb_db_isam_info(int64_t h, char* out, int outCap);
GMB_DB_API gmb_string_t gmb_db_isam_info_str(int64_t h);
GMB_DB_API int      gmb_db_isam_backup(int64_t h, const char* destDir);
GMB_DB_API int      gmb_db_isam_restore(const char* backupDir, const char* targetIsdb);
GMB_DB_API int      gmb_db_isam_repair(int64_t h);
GMB_DB_API int      gmb_db_isam_copy_to(int64_t h, const char* destPath, const char* forCondition);
GMB_DB_API int      gmb_db_isam_sort_to(int64_t h, const char* destPath, const char* fieldName, int descending, const char* forCondition);
GMB_DB_API int      gmb_db_isam_begin_transaction(int64_t h);
GMB_DB_API int      gmb_db_isam_commit(int64_t h);
GMB_DB_API int      gmb_db_isam_rollback(int64_t h);
GMB_DB_API int      gmb_db_isam_set_replication(int64_t h, const char* logPath);
GMB_DB_API int      gmb_db_isam_join_with(int64_t h1, int64_t h2, const char* destPath, const char* leftField, const char* rightField);
GMB_DB_API int      gmb_db_isam_apply_replication(int64_t h, const char* replLogPath);
GMB_DB_API int      gmb_db_isam_follow_replication(int64_t h, const char* replLogPath, const char* checkpointPath);



/* ---- WAL: due moduli distinti (DBF/ISAM vs SPEED) ---------------------- */
GMB_DB_API int64_t  gmb_db_wal_open(const char* path);
GMB_DB_API int      gmb_db_wal_append(int64_t h, int op, int64_t offset, const void* data, int len);
GMB_DB_API int      gmb_db_wal_checkpoint(int64_t h);
GMB_DB_API int      gmb_db_wal_replay_count(int64_t h);
GMB_DB_API int      gmb_db_wal_close(int64_t h);

GMB_DB_API int64_t  gmb_db_speed_wal_open(const char* dir);
GMB_DB_API int      gmb_db_speed_wal_log_set(int64_t h, int table_id, const void* key, int klen,
                                             const void* val, int vlen);
GMB_DB_API int      gmb_db_speed_wal_log_delete(int64_t h, int table_id, const void* key, int klen);
GMB_DB_API int      gmb_db_speed_wal_close(int64_t h);

/* ---- lock / replica ---------------------------------------------------- */
GMB_DB_API int      gmb_db_lock_file(const char* path, int exclusive);
GMB_DB_API int      gmb_db_unlock(int fd);
GMB_DB_API int      gmb_db_repl_append(const char* log_path, const char* line);
GMB_DB_API int      gmb_db_repl_count(const char* log_path);

/* Syscall helper (VM commerciale / test): 0 handled, 1 unknown. */
GMB_DB_API int      gmb_db_apply_syscall(int code, const char* sarg, int64_t iarg, int64_t* result);
GMB_DB_API int      gmb_db_apply_syscall_known(int code);
GMB_DB_API int64_t  gmb_db_current_isam(void);
GMB_DB_API void     gmb_db_set_current_isam(int64_t h);
GMB_DB_API int64_t  gmb_db_current_dbf(void);
GMB_DB_API void     gmb_db_set_current_dbf(int64_t h);
GMB_DB_API void     gmb_db_set_current_ledger(int64_t h);
GMB_DB_API int64_t  gmb_db_current_speed(void);
GMB_DB_API void     gmb_db_set_current_speed(int64_t h);
GMB_DB_API int64_t  gmb_db_current_ledger(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_DB_H */
