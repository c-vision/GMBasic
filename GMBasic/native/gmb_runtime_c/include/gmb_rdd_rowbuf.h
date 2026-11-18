/* FILE: native/gmb_runtime_c/include/gmb_rdd_rowbuf.h
 * Row-buffer RDD (table buffering Clipper) — unica implementazione condivisa (§0bis).
 * Spec canonica: 01-Projects/GMBasic/Decisions/ADR-RowBuffer-RDD-Canonica.md
 *
 * Semantica (per area attiva, record corrente):
 *  - BUFFER(area, mode): mode 2 pessimistico / 3 ottimistico → snapshot del record corrente
 *    e deviazione delle scritture nel buffer; mode 0/1 → disattiva (scarta). Ritorna 1/0.
 *  - RDD SET FIELD / RDD REPLACE / assegnazione di campo (via gmb_rdd_replace_field) con
 *    buffer attivo scrivono NEL BUFFER e marcano lo stato del campo; il motore non tocca.
 *  - gmb_rdd_get_field con buffer attivo legge dal buffer.
 *  - gmb_rdd_oldval(idx): valore dello snapshot ("" senza buffer).
 *  - gmb_rdd_get_field_state(idx): 0 non modificato, 1 modificato, 2 record cancellato.
 *  - gmb_rdd_table_update(): applica i campi modificati al motore, azzera gli stati,
 *    disattiva; ritorna 1 al successo, 0 se nessun buffer/nessun record.
 *  - gmb_rdd_table_revert(): scarta il buffer (rilettura dal motore); ritorna 1 se c'era
 *    un buffer da scartare, 0 senza.
 * Indici campo: 0-based (colonna del motore). La conversione 1-based del bytecode avviene
 * NEI boundary dei backend (syscall/builtin/emettitori), NON qui.
 *
 * Motori: l'engine store RDD del runtime (ARRAY/CSV/SDF/SQL/DBF) è gestito internamente.
 * Il motore ISAM (commerciale, gmb_db_c) NON è raggiungibile dal runtime condiviso: chi lo
 * possiede registra gli hook qui sotto (gmb_rdd_rowbuf_set_engine) alla prima open ISAM.
 */
#ifndef GMB_RDD_ROWBUF_H
#define GMB_RDD_ROWBUF_H

#include "gmb_rdd_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hook del motore ISAM (registered by the owner of the commercial engine).
 * Tutti gli indici campo sono 0-based. "current record" = il record corrente
 * dell'handle ISAM attivo. */
typedef struct gmb_rdd_engine_hooks
{
    int (*active)(void);                                    /* 1 se l'area corrente è ISAM */
    int (*nfields)(void);                                   /* numero campi, 0 se nessuno */
    int (*field_name)(int ord0, char* buf, int cap);        /* nome campo ord0 → 0 ok */
    int (*read_field)(int ord0, char* buf, int cap);        /* valore corrente → 0 ok */
    int (*write_record)(const char* const* vals, int n);    /* commit record corrente */
    int (*recno)(void);                                     /* record corrente (1-based), 0 = nessuno */
} gmb_rdd_engine_hooks;

/* Registra (o con NULL rimuove) l'engine hook ISAM. Idempotente. */
void gmb_rdd_rowbuf_set_engine(const gmb_rdd_engine_hooks* hooks);

/* Scarta il buffer dell'area (usato da gmb_rdd_close/use su riapertura). */
void gmb_rdd_rowbuf_forget(int area);

/* ── Punti di intercettazione usati da gmb_rdd.c (non chiamare dai backend) ──
 * rb_read/rb_write restituiscono 1 quando il buffer ha gestito l'operazione. */
int gmb_rdd_rb_read(int area, int idx, char* buf, int cap);
int gmb_rdd_rb_write(int area, int idx, const char* value);
int gmb_rdd_rb_oldval(int area, int idx, char* buf, int cap);   /* 1 = buffer attivo */
int gmb_rdd_rb_state(int area, int idx);                        /* -1 = nessun buffer */

/* ── ciclo di vita (chiamati dalle API pubbliche in gmb_rdd.c) ──
 * Ritornano 1/0 (attiva ok / update applicato / revert eseguito). */
int gmb_rdd_rowbuf_activate(int area, int mode);
int gmb_rdd_rowbuf_update(int area);
int gmb_rdd_rowbuf_revert(int area);
/* Pass-through degli hook ISAM usati dal routing in gmb_rdd.c (solo via hook registrato). */
int gmb_rdd_engine_isam_active(void);
int gmb_rdd_engine_isam_nfields(void);
int gmb_rdd_engine_isam_read(int ord0, char* buf, int cap);
int gmb_rdd_engine_isam_field_index(const char* name);
/* Scrittura campo singolo sul motore ISAM via hook (read-modify-write del record). */
int gmb_rdd_engine_isam_write_field(int ord0, const char* value);
/* Numero grezzo dell'area selezionata (anche se non «used»: caso ISAM), 1..255. */
int gmb_rdd_current_area(void);

/* ── API interna condivisa con gmb_rdd.c (engine RDD cell-store raw access) ── */
int gmb_rdd_raw_field_count(void);
int gmb_rdd_raw_field_name(int ord0, char* buf, int cap);
int gmb_rdd_raw_read(int ord0, char* buf, int cap);
int gmb_rdd_raw_write(int ord0, const char* value);
int gmb_rdd_raw_recno(void);
int gmb_rdd_raw_deleted(void);

#ifdef __cplusplus
}
#endif
#endif /* GMB_RDD_ROWBUF_H */
