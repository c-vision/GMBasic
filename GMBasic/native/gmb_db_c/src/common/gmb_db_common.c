/* FILE: gmb_db_common.c */
#include "gmb_db_internal.h"
#include "gmb_network.h"
#include "gmb_rdd_rowbuf.h"
#include <stdio.h>
#include <string.h>

gmb_handle_t g_handles[GMB_DB_MAX_HANDLES];
char g_db_err[GMB_DB_ERRCAP];

int32_t gmb_db_version(void)
{
    return GMB_DB_VERSION;
}

int gmb_db_last_error(char* buf, int cap)
{
    if (!buf || cap <= 0) return (int)strlen(g_db_err);
    strncpy(buf, g_db_err, (size_t)cap - 1);
    buf[cap - 1] = 0;
    return (int)strlen(buf);
}

void gmb_db_set_err(const char* msg)
{
    strncpy(g_db_err, msg ? msg : "", GMB_DB_ERRCAP - 1);
    g_db_err[GMB_DB_ERRCAP - 1] = 0;
}

int64_t gmb_db_alloc(int kind)
{
    /* Numerazione MONOTONA (mai riuso di uno slot chiuso): il backend di riferimento (C#
     * LedgerManager.AllocateSessionHandle → Interlocked.Increment) assegna handle crescenti e
     * non riusa, quindi "OPEN, CLOSE, OPEN" dà 1,2 e non 1,1. Preserva la parità dell'handle
     * stampato (§ DbLedgerFullTests: Handle 1/2/3). */
    static int64_t g_next = 1;
    int64_t i;
    for (i = g_next; i < GMB_DB_MAX_HANDLES; i++)
    {
        if (g_handles[i].kind == GMB_H_FREE)
        {
            memset(&g_handles[i], 0, sizeof(g_handles[i]));
            g_handles[i].kind = kind;
            g_next = i + 1;
            return i;
        }
    }
    gmb_db_set_err("handle table full");
    return -1;
}

gmb_handle_t* gmb_db_get(int64_t h, int kind)
{
    if (h <= 0 || h >= GMB_DB_MAX_HANDLES || g_handles[h].kind != kind)
    {
        gmb_db_set_err("invalid handle");
        return NULL;
    }
    return &g_handles[h];
}

/* Chiude TUTTE le aree DBF aperte e azzera l'area corrente.
 * Parità IL: `DBF CLOSE` (statement, senza alias) compila in DBF_CLOSE_DATABASES e chiude
 * ogni area aperta; la VM/C chiudeva SOLO l'area corrente, lasciando aperte le altre → un
 * successivo `DBF SELECT <altra>` commutava su un'area che il riferimento ha già chiuso
 * (divergenza misurata su DbDbfAppendMany.bas e sulla replicazione DBF). */
int gmb_db_dbf_close_all(void)
{
    int i, n = 0;
    for (i = 1; i < GMB_DB_MAX_HANDLES; i++)
    {
        if (g_handles[i].kind == GMB_H_DBF)
        {
            if (gmb_db_dbf_close(i) == GMB_DB_OK) n++;
        }
    }
    gmb_db_set_current_dbf(0);
    return n;
}

void gmb_db_write_u16(FILE* f, unsigned short v)
{
    unsigned char b[2];
    b[0] = (unsigned char)(v & 0xFF);
    b[1] = (unsigned char)((v >> 8) & 0xFF);
    fwrite(b, 1, 2, f);
}

void gmb_db_write_u32(FILE* f, unsigned int v)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xFF);
    b[1] = (unsigned char)((v >> 8) & 0xFF);
    b[2] = (unsigned char)((v >> 16) & 0xFF);
    b[3] = (unsigned char)((v >> 24) & 0xFF);
    fwrite(b, 1, 4, f);
}

unsigned short gmb_db_read_u16(const unsigned char* p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}

unsigned int gmb_db_read_u32(const unsigned char* p)
{
    return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

int gmb_db_split_csv(const char* csv, char out[][256], int maxn)
{
    int n = 0;
    const char* p = csv ? csv : "";
    while (*p && n < maxn)
    {
        int i = 0;
        if (*p == '"') {
            /* Valore quotato (standard CSV): virgole e doppi apici dentro sono preservati,
             * `""` dentro una stringa = doppio apice letterale. Supporta JSON con virgole. */
            p++;
            while (*p && i < 254) {
                if (*p == '"') {
                    if (p[1] == '"') { out[n][i++] = '"'; p += 2; continue; }
                    p++;
                    break;
                }
                out[n][i++] = *p++;
            }
            out[n][i] = 0;
            if (*p == ',') p++;
        } else {
            while (*p && *p != ',' && i < 255)
                out[n][i++] = *p++;
            out[n][i] = 0;
            if (*p == ',') p++;
        }
        n++;
    }
    return n;
}

void gmb_db_trim_field(char* s)
{
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\0'))
    {
        s[n - 1] = 0;
        n--;
    }
}

static int64_t g_last_dbf;
static int64_t g_last_isam;
static int64_t g_last_speed;
static int64_t g_last_ledger;

/* ── Engine-hook: espone il motore ISAM al row-buffer RDD condiviso (ADR §9) ──
 * gmb_db è l'unico possessore del motore ISAM; il runtime C condiviso NON può
 * raggiungirlo (§0ter due ambienti, e gmb_runtime non linka gmb_db). Alla prima
 * open ISAM registriamo gli hook: il row-buffer in gmb_rdd_rowbuf.c li usa per
 * snapshot/lettura/commit dell'area ISAM corrente. Indici campo 0-based, coerenti
 * con gmb_db_isam_read/gmb_rdd_replace_field. */
static int isam_hk_active(void)
{
    return g_last_isam > 0;
}
static int isam_hk_nfields(void)
{
    char names[1024] = "";
    int n = 0;
    const char* p;
    if (g_last_isam <= 0) return 0;
    if (gmb_db_isam_fields(g_last_isam, names, (int)sizeof(names)) != GMB_DB_OK) return 0;
    for (p = names; *p; p++) if (*p == ',') n++;
    if (names[0]) n++;
    return n;
}
static int isam_hk_field_name(int ord0, char* buf, int cap)
{
    char names[1024] = "";
    const char* p = names;
    int i = 0;
    if (g_last_isam <= 0 || !buf || cap <= 0) return -1;
    if (gmb_db_isam_fields(g_last_isam, names, (int)sizeof(names)) != GMB_DB_OK) return -1;
    while (*p)
    {
        const char* st = p;
        while (*p && *p != ',') p++;
        if (i == ord0)
        {
            size_t len = (size_t)(p - st);
            if (len > (size_t)cap - 1) len = (size_t)cap - 1;
            memcpy(buf, st, len);
            buf[len] = 0;
            return 0;
        }
        if (*p == ',') p++;
        i++;
    }
    return -1;
}
static int isam_hk_read(int ord0, char* buf, int cap)
{
    if (g_last_isam <= 0) return -1;
    return gmb_db_isam_read(g_last_isam, ord0, buf, cap);
}
static int isam_hk_write(const char* const* vals, int n)
{
    char csv[4096];
    int pos = 0, i;
    if (g_last_isam <= 0 || !vals || n <= 0) return -1;
    for (i = 0; i < n; i++)
    {
        const char* v = vals[i] ? vals[i] : "";
        int q = 0;
        const char* t;
        for (t = v; *t; t++) if (*t == ',' || *t == '"') { q = 1; break; }
        if (i) csv[pos++] = ',';
        if (pos >= (int)sizeof(csv) - 2) break;
        if (q)
        {
            csv[pos++] = '"';
            for (; *v; v++) { if (*v == '"' && pos < (int)sizeof(csv) - 2) csv[pos++] = '"'; if (pos < (int)sizeof(csv) - 2) csv[pos++] = *v; }
            if (pos < (int)sizeof(csv) - 1) csv[pos++] = '"';
        }
        else { while (*v && pos < (int)sizeof(csv) - 1) csv[pos++] = *v++; }
    }
    csv[pos] = 0;
    return gmb_db_isam_put(g_last_isam, csv);
}
static int isam_hk_recno(void)
{
    if (g_last_isam <= 0) return 0;
    return (int)gmb_db_isam_recno(g_last_isam);
}
static const gmb_rdd_engine_hooks g_isam_engine_hooks = {
    isam_hk_active, isam_hk_nfields, isam_hk_field_name, isam_hk_read, isam_hk_write, isam_hk_recno
};
void gmb_db_install_isam_rdd_hook(void) { gmb_rdd_rowbuf_set_engine(&g_isam_engine_hooks); }

/* L'area "corrente" e' valida solo se il suo handle e' ancora VIVO: dopo `DBF CLOSE`/`ISAM CLOSE` gmb_db_*_close libera lo slot (kind=FREE)
 * ma g_last_* restava impostato -> i chiamanti (DBF GO/SKIP, RECNO, ...) navigavano un handle morto (no-op) invece di ricadere
 * sull'area RDD: `DBF CLOSE; RDD USE x.dbf; DBF.RECNO()` dava 0 (VM/C) invece del record corrente. */
int64_t gmb_db_current_isam(void) { return (g_last_isam > 0 && gmb_db_get(g_last_isam, GMB_H_ISAM)) ? g_last_isam : 0; }
void gmb_db_set_current_isam(int64_t h) { g_last_isam = h; }
int64_t gmb_db_current_dbf(void)  { return (g_last_dbf > 0 && gmb_db_get(g_last_dbf, GMB_H_DBF)) ? g_last_dbf : 0; }

/* Dopo la chiusura di h: se h ERA l'area DBF corrente, sposta l'area corrente su un'altra
 * area DBF ancora aperta (o 0). Senza, g_last_dbf resta sull'handle chiuso e i comandi
 * area-correnti successivi (es. DBF AGGREGATE dopo DBF CLOSE di un'area temporanea) non
 * trovano l'area. Parità C# (DataSession.Unregister riporta a un'area aperta). */
void gmb_db_dbf_after_close(int64_t h)
{
    int i;
    if (g_last_dbf != h) return;
    /* Torna all'area DBF aperta PIU' RECENTE (handle più alto): è l'area che era corrente
     * prima di selezionare quella chiusa. NON la "prima" (handle più basso): un `DBF CREATE`
     * lascia un handle (vuoto) e il successivo `DBF USE` ne apre un altro (con i dati) → la
     * prima area aperta è quella vuota del CREATE. */
    for (i = GMB_DB_MAX_HANDLES - 1; i >= 1; i--)
        if (g_handles[i].kind == GMB_H_DBF) { g_last_dbf = i; return; }
    g_last_dbf = 0;
}
void gmb_db_set_current_dbf(int64_t h) { g_last_dbf = h; }
int64_t gmb_db_current_speed(void){ return g_last_speed; }
void gmb_db_set_current_speed(int64_t h){ g_last_speed = h; }
int64_t gmb_db_current_ledger(void){ return g_last_ledger; }
void gmb_db_set_current_ledger(int64_t h){ g_last_ledger = h; }

/* True se gmb_db_apply_syscall ha un case per il codice (path semplice sarg/iarg).
 * I codici differiti (multi-arg/dict/binary) sono gestiti dal VM/C (marshaller). */
int gmb_db_apply_syscall_known(int code)
{
    switch (code) {
        case GMB_SYS_DBF_SKIP: case GMB_SYS_DBF_GO: case GMB_SYS_DBF_SEEK:
        case GMB_SYS_DBF_DELETE: case GMB_SYS_DBF_RECALL: case GMB_SYS_DBF_PACK: case GMB_SYS_DBF_ZAP:
        case GMB_SYS_DBF_REINDEX: case GMB_SYS_DBF_SET_DELETED: case GMB_SYS_DBF_SET_EXACT:
        case GMB_SYS_DBF_SET_FILTER: case GMB_SYS_DBF_COPY_TO: case GMB_SYS_DBF_SET_ORDER:
        case GMB_SYS_DBF_APPEND_BLANK: case GMB_SYS_DBF_VALIDATE: case GMB_SYS_DBF_RECNO:
        case GMB_SYS_DBF_LASTREC: case GMB_SYS_DBF_BEGIN_GROUP_COMMIT: case GMB_SYS_DBF_END_GROUP_COMMIT:
        case GMB_SYS_DBF_SET_DURABILITY: case GMB_SYS_DBF_SET_EXCLUSIVE: case GMB_SYS_DBF_SET_SCHEMA:
        case GMB_SYS_DBF_CLOSE_CURRENT:
        case GMB_SYS_DBF_CLOSE_AREA:
        case GMB_SYS_ISAM_RECORD: case GMB_SYS_ISAM_SKIP: case GMB_SYS_ISAM_SEEK:
        case GMB_SYS_ISAM_DELETE: case GMB_SYS_ISAM_RECALL: case GMB_SYS_ISAM_PACK: case GMB_SYS_ISAM_REINDEX:
        case GMB_SYS_ISAM_REPAIR: case GMB_SYS_ISAM_BACKUP: case GMB_SYS_ISAM_LOCK: case GMB_SYS_ISAM_UNLOCK:
        case GMB_SYS_ISAM_SET_ORDER: case GMB_SYS_ISAM_DROP_INDEX: case GMB_SYS_ISAM_VALIDATE:
        case GMB_SYS_ISAM_COPY_TO: case GMB_SYS_ISAM_SET_FILTER:
        /* ISAM_INTEGRITY ESCLUSO: ritorna un JSON (stringa), non un numero
         * (gmb_sys_impl → gmb_sys_GMB_SYS_ISAM_INTEGRITY). */
        case GMB_SYS_ISAM_EOF: case GMB_SYS_ISAM_BOF: case GMB_SYS_ISAM_RECNO:
        /* ISAM_SET/APPLY/FOLLOW_REPLICATION ESCLUSI deliberatamente: SETREPLICATION ritorna il
         * PATH del log (StringType), e questo path generico sa veicolare solo un numero
         * (*result). Passano dalla tabella del VM/C (gmb_sys_impl → gmb_sys_GMB_SYS_ISAM_*),
         * che pusha la stringa. § MiniTests/dbf/isam_replication.bas. */
        case GMB_SYS_BEGIN_TRANSACTION: case GMB_SYS_COMMIT: case GMB_SYS_ROLLBACK:
        /* GMB_SYS_SPEED_USE ESCLUSO deliberatamente: ha argomenti multi-valore (campi
         * nome+symbolId per la normalizzazione __m<sid><->nome di PUT/GET) che questo path
         * generico sarg/iarg scarterebbe — gestito da un case dedicato nel VM/C marshaller
         * (gmb_vm_exec_db_syscall in gmb_vm_core.c). Vedi DB-Parity-1-1-TODO.md §3.
         * GMB_SYS_SPEED_GO/SKIP ESCLUSI per lo stesso motivo strutturale (non multi-valore,
         * ma DUE argomenti che servono ENTRAMBI, di cui uno per tabella): il modello
         * generico raccoglie solo UN sarg (stringa) e UN iarg (numero) totali, quindi per
         * "SPEED GO alias, TOP/BOTTOM" e "SPEED SKIP alias, count" l'iarg letto (location
         * kind / count) veniva riusato ERRONEAMENTE anche come tableId al posto
         * dell'alias risolto — il cursore veniva quindi posizionato sulla tabella
         * sbagliata (o su nessuna), facendo fallire ogni GET successivo (rc=-1,
         * Database_GetCurrent non trova un record corrente valido). Gestiti da case
         * dedicati nel VM/C marshaller, che risolvono tableId via
         * gmb_db_speed_table_by_alias come PUT/GET/SEEK/DELETE. */
        /* SPEED_SELECT ESCLUSO: multi-argomento (campi/WHERE/JOIN/GROUP BY/INTO) — gestito
         * dal case dedicato nel VM/C marshaller (gmb_syscall_db.c gmb_sys_GMB_SYS_SPEED_SELECT
         * → gmb_db_speed_select). */
        case GMB_SYS_SPEED_OPEN:
        case GMB_SYS_LEDGER_OPEN: case GMB_SYS_LEDGER_DELETE:
        case GMB_SYS_LEDGER_SAVE: case GMB_SYS_LEDGER_CLOSE: case GMB_SYS_LEDGER_PACK:
        case GMB_SYS_LEDGER_VALIDATE:
            return 1;
        default:
            return 0;
    }
}

int gmb_db_apply_syscall(int code, const char* sarg, int64_t iarg, int64_t* result)
{
    if (!result) return GMB_DB_ERR;
    *result = 0;
    switch (code)
    {
        case GMB_SYS_DBF_USE:
            *result = gmb_db_dbf_open(sarg);
            g_last_dbf = *result;
            return GMB_DB_OK;
        case GMB_SYS_DBF_SKIP:
            return gmb_db_dbf_skip(g_last_dbf, iarg != 0 ? (int)iarg : 1);
        case GMB_SYS_ISAM_USE:
            *result = gmb_db_isam_open(sarg);
            g_last_isam = *result;
            return GMB_DB_OK;
        case GMB_SYS_SPEED_OPEN:
            *result = gmb_db_speed_open(sarg);
            g_last_speed = *result;
            return GMB_DB_OK;
        case GMB_SYS_LEDGER_OPEN:
            *result = gmb_db_ledger_open(sarg);
            g_last_ledger = *result;
            return GMB_DB_OK;

        /* --- ISAM (navigazione + operazioni string/int-keyed) --- */
        case GMB_SYS_ISAM_RECORD:
            /* `ISAM GO <area>, TOP|BOTTOM` → il compilatore C# emette
             * `EmitConstant((double)stmt.Location.Kind)`, cioè il valore dell'enum
             * `TokenKind` (common/Ast/TokenKind.cs). I due ordinali arrivano da
             * `GMB_DB_TOK_TOP`/`GMB_DB_TOK_BOTTOM` (gmb_db_tokenkind_gen.h, GENERATO da
             * TokenKind.cs): niente numeri a mano, che si erano già disallineati tre volte
             * facendo finire TOP nel ramo bottom e BOTTOM in un no-op muto →
             * `ISAM GO X, TOP` restava sull'ultimo record e la GET successiva leggeva
             * l'ultimo appeso (MiniTests/dbf/isam_commands.bas). Il parser accetta SOLO
             * TOP|BOTTOM per ISAM GO (Parser.Stmt.Isam.cs: ParseIsamGoStatement), quindi
             * nessun numero di record arriva qui: un valore fuori dai due è un errore
             * esplicito, mai un no-op. */
            if (iarg == GMB_DB_TOK_TOP) return gmb_db_isam_go_top(g_last_isam);
            if (iarg == GMB_DB_TOK_BOTTOM) return gmb_db_isam_go_bottom(g_last_isam);
            return GMB_DB_ERR;
        case GMB_SYS_ISAM_SKIP:
            return gmb_db_isam_skip(g_last_isam, iarg != 0 ? (int)iarg : 1);
        case GMB_SYS_ISAM_SEEK:
            return gmb_db_isam_seek(g_last_isam, sarg);
        case GMB_SYS_ISAM_DELETE:
            return gmb_db_isam_delete(g_last_isam);
        case GMB_SYS_ISAM_RECALL:
            return gmb_db_isam_recall(g_last_isam);
        case GMB_SYS_ISAM_PACK:
            return gmb_db_isam_pack(g_last_isam);
        case GMB_SYS_ISAM_REINDEX:
            return gmb_db_isam_reindex(g_last_isam);
        case GMB_SYS_ISAM_REPAIR:
            return gmb_db_isam_repair(g_last_isam);
        case GMB_SYS_ISAM_BACKUP:
            return gmb_db_isam_backup(g_last_isam, sarg);
        case GMB_SYS_ISAM_LOCK:
            return gmb_db_isam_lock(g_last_isam);
        case GMB_SYS_ISAM_UNLOCK:
            return gmb_db_isam_unlock(g_last_isam);
        case GMB_SYS_ISAM_SET_ORDER:
            return gmb_db_isam_set_order(g_last_isam, sarg);
        case GMB_SYS_ISAM_DROP_INDEX:
            return gmb_db_isam_drop_index(g_last_isam, sarg);
        case GMB_SYS_ISAM_VALIDATE:
            return gmb_db_isam_validate(g_last_isam);
        case GMB_SYS_ISAM_COPY_TO:
            return gmb_db_isam_copy_to(g_last_isam, sarg, NULL);
        case GMB_SYS_ISAM_SET_FILTER:
            return gmb_db_isam_set_filter(g_last_isam, sarg);
        case GMB_SYS_ISAM_INTEGRITY:
            return gmb_db_isam_integrity(g_last_isam, NULL, 0);
        case GMB_SYS_ISAM_EOF:
            *result = gmb_db_isam_eof(g_last_isam);
            return GMB_DB_OK;
        case GMB_SYS_ISAM_BOF:
            *result = gmb_db_isam_bof(g_last_isam);
            return GMB_DB_OK;
        case GMB_SYS_ISAM_RECNO:
            /* ISAM.RECNO(alias): il C# emette SysCall.ISAM_RECNO (355) ma il C non aveva il
             * case → cadeva nel default (0). §3 vmc: RECNO sempre 0 (divergenza vs il). */
            *result = gmb_db_isam_recno(g_last_isam);
            return GMB_DB_OK;
        case GMB_SYS_ISAM_SET_REPLICATION:
            return gmb_db_isam_set_replication(g_last_isam, sarg);
        case GMB_SYS_ISAM_APPLY_REPLICATION:
            return gmb_db_isam_apply_replication(g_last_isam, sarg);
        case GMB_SYS_ISAM_FOLLOW_REPLICATION:
            return gmb_db_isam_follow_replication(g_last_isam, sarg, NULL);
        case GMB_SYS_BEGIN_TRANSACTION:
            return gmb_db_isam_begin_transaction(g_last_isam);
        case GMB_SYS_COMMIT:
            return gmb_db_isam_commit(g_last_isam);
        case GMB_SYS_ROLLBACK:
            return gmb_db_isam_rollback(g_last_isam);

        /* --- DBF (navigazione + operazioni string/int-keyed) --- */
        case GMB_SYS_DBF_GO:
            return gmb_db_dbf_go(g_last_dbf, (int)iarg);
        case GMB_SYS_DBF_SEEK:
            return gmb_db_dbf_seek(g_last_dbf, sarg);
        case GMB_SYS_DBF_DELETE:
            return gmb_db_dbf_delete(g_last_dbf);
        case GMB_SYS_DBF_RECALL:
            return gmb_db_dbf_recall(g_last_dbf);
        case GMB_SYS_DBF_PACK:
            return gmb_db_dbf_pack(g_last_dbf);
        case GMB_SYS_DBF_ZAP:
            return gmb_db_dbf_zap(g_last_dbf);
        case GMB_SYS_DBF_REINDEX:
            return gmb_db_dbf_reindex(g_last_dbf);
        case GMB_SYS_DBF_SET_DELETED:
            return gmb_db_dbf_set_deleted(g_last_dbf, (int)iarg);
        case GMB_SYS_DBF_SET_EXACT:
            return gmb_db_dbf_set_exact(g_last_dbf, (int)iarg);
        case GMB_SYS_DBF_SET_FILTER:
            return gmb_db_dbf_set_filter(g_last_dbf, sarg);
        case GMB_SYS_DBF_COPY_TO:
            return gmb_db_dbf_copy_to(g_last_dbf, sarg);
        case GMB_SYS_DBF_SET_ORDER:
        {
            /* SET ORDER TO <n> (indice numerico) oppure TO "<nome>" (indice per nome, es.
             * l'ordine creato da `INDEX ON Nome TO "idx_nome.ntx"`). Il path generico passava
             * solo iarg: con un NOME la stringa (sarg) era ignorata e iarg era un numero
             * qualsiasi → l'ordine non veniva applicato e il SEEK successivo falliva sempre
             * (ClipperDbfLateBinding, gamba VM/C: "Mario Rossi" non trovato). */
            if (sarg && *sarg && !(sarg[0] == '-' || sarg[0] == '+' || (sarg[0] >= '0' && sarg[0] <= '9')))
                return gmb_db_dbf_set_order_by_name(g_last_dbf, sarg);
            return gmb_db_dbf_set_order(g_last_dbf, (int)iarg);
        }
        case GMB_SYS_DBF_APPEND_BLANK:
            return gmb_db_dbf_append_blank(g_last_dbf);
        case GMB_SYS_DBF_VALIDATE:
            *result = gmb_db_dbf_validate(g_last_dbf);
            return GMB_DB_OK;
        case GMB_SYS_DBF_RECNO:
        case GMB_SYS_DBF_LASTREC:
            /* RECNO/LASTREC dell'AREA ATTIVA: DBF (layer gmb_db) se aperto, altrimenti l'area
             * del driver RDD (RDD USE … VIA "CSV"). I comandi DBF GO/SKIP navigano già l'area
             * RDD quando non c'è un handle gmb_db; senza questo fallback RECNO restava 0 e i
             * cicli `DO WHILE NOT EOF()` su un'area RDD non terminavano (misurato:
             * samples/Databases/RDD_ReplaceRealData.bas → 1.048.579 righe).
             * Punto unico del runtime condiviso (§0bis): vale per VM/C e C99Native. */
            if (gmb_db_current_dbf() > 0)       /* handle VIVO (un DBF CLOSE lascia g_last_dbf impostato ma libera lo slot) */
                *result = (code == GMB_SYS_DBF_RECNO) ? gmb_db_dbf_recno(g_last_dbf)
                                                      : gmb_db_dbf_lastrec(g_last_dbf);
            else
                /* Area RDD: DBF.RECNO() → 0 per un'area CSV/SDF/ARRAY (parità IL: DbfRecno non vede l'area e torna 0; il recno
                 * dell'area RDD si legge con RDD.RECNO()) MA per un'area RDD di tipo DBF (RDD USE x.dbf) IL legge il suo driver DBF:
                 * record corrente. DBF.LASTREC() → conteggio reale (gmb_rdd_lastrec). Prima RECNO dava gmb_rdd_recno()=1 per
                 * qualunque area → `clienti: 1` vs il 0 (CustomerOrdersReport.bas, area CSV). */
                *result = (code == GMB_SYS_DBF_RECNO) ? (gmb_rdd_cur_is_dbf() ? gmb_rdd_recno() : 0) : gmb_rdd_lastrec();
            return GMB_DB_OK;
        case GMB_SYS_DBF_SET_DURABILITY:
            /* Il valore esposto a BASIC segue la convenzione del mondo gestito
             * (-1 = successo: vedi IlRuntime.DbfSetDurability), mentre il runtime C ritorna
             * 0 = ok. Prima si propagava lo 0 → la VM/C divergeva da IL/PCode
             * (§ MiniTests/dbf/dbf_lock_filter.bas). */
            /* idem: il valore per BASIC sta in *result (il `return` viene scartato). */
            *result = gmb_db_dbf_set_durability(g_last_dbf, sarg) == GMB_DB_OK ? -1 : 0;
            return GMB_DB_OK;
        case GMB_SYS_DBF_SET_EXCLUSIVE:
            return gmb_db_dbf_set_exclusive(g_last_dbf, iarg ? 1 : 0);
        case GMB_SYS_DBF_SET_SCHEMA:
            return gmb_db_dbf_set_schema_strict(g_last_dbf, iarg ? 1 : 0);
        case GMB_SYS_DBF_BEGIN_GROUP_COMMIT:
            /* Il valore per BASIC va scritto in *result: e' il contratto di questa funzione
             * (vedi case GMB_SYS_DBF_USE). Ritornarlo soltanto lo faceva SCARTARE dal chiamante
             * (vm_sys_db legge solo `result`) → BEGINGROUPCOMMIT rispondeva sempre 0
             * (§ MiniTests/dbf/dbf_lock_filter.bas). -1 = successo, come il mondo gestito. */
            *result = gmb_db_dbf_begin_group_commit(g_last_dbf) == GMB_DB_OK ? -1 : 0;
            return GMB_DB_OK;
        case GMB_SYS_DBF_END_GROUP_COMMIT:
            *result = gmb_db_dbf_end_group_commit(g_last_dbf) == GMB_DB_OK ? -1 : 0;
            return GMB_DB_OK;
        case GMB_SYS_DBF_CLOSE_AREA:
        {
            /* DBF CLOSE <alias|area>: -1 = area non aperta (no-op), altrimenti chiude SOLO
             * quella. L'argomento arriva come alias (sarg) o numero d'area (iarg). */
            int64_t ch = (sarg && sarg[0]) ? gmb_db_dbf_find_handle_by_alias(sarg)
                        : (iarg > 0 ? gmb_db_dbf_find_handle_by_area((int)iarg) : 0);
            if (ch <= 0) { *result = 0; return GMB_DB_OK; }
            *result = gmb_db_dbf_close(ch) == GMB_DB_OK ? 1 : 0;
            return GMB_DB_OK;
        }
        case GMB_SYS_DBF_CLOSE_CURRENT:
            if (g_last_dbf > 0) return gmb_db_dbf_close(g_last_dbf);
            /* Area RDD (RDD USE): libera l'area cell-store (parità IL, che al CLOSE libera
             * l'area → il prossimo USE la riusa; senza, le aree si accumulano e
             * RDD.GETFIELD(areaN,..) con area fissa legge l'area sbagliata — TextDriversDemo). */
            (void)gmb_rdd_close(0);
            return GMB_DB_OK;

        /* --- SPEED (cursore) --- */
        /* GMB_SYS_SPEED_USE/GO/SKIP/SELECT: gestiti nel VM/C marshaller (case dedicati,
         * vedi gmb_db_apply_syscall_known sopra) — non più raggiungibili da qui. */

        /* --- LEDGER (cursore + validate) --- */
        case GMB_SYS_LEDGER_GET:
            *result = 0;
            return GMB_DB_OK; /* GET usa argomenti multipli (buffer) — differito */
        case GMB_SYS_LEDGER_DELETE:
            return gmb_db_ledger_delete(g_last_ledger, iarg);
        case GMB_SYS_LEDGER_SAVE:
            return gmb_db_ledger_save(g_last_ledger);
        case GMB_SYS_LEDGER_CLOSE:
            return gmb_db_ledger_close(g_last_ledger);
        case GMB_SYS_LEDGER_PACK:
            return gmb_db_ledger_pack(g_last_ledger);
        case GMB_SYS_LEDGER_VALIDATE:
            /* Il valore va in *result (il return è lo STATO): prima ritornava il valore
             * come stato → la VM/C leggeva 0. -1 = valido (BIT BASIC), come IL. */
            *result = gmb_db_ledger_validate(g_last_ledger) ? -1 : 0;
            return GMB_DB_OK;

        /* Differiti (richiedono marshalling multi-string/binary che il dispatcher
         * (sarg, iarg) non copre): ISAM_APPEND/PUT/APPENDMANY/CREATE_INDEX/SORT_TO/
         * INFO, DBF_APPEND/RECORD/REPLACE/GET_RECORD, SPEED_GET/PUT/SEEK,
         * LEDGER_ADD/EDIT/BUDGET/VARIANCE/ACCOUNTS/BALANCE/REGISTER/IMPORT/PLOT. */
        default:
            return 1;
    }
}

/* ====================================================================
 * GAP CHIUSI 2026-08-29 — REPLICATION SHIP/RECV via TCP.
 * SHIP(host, port): connette e invia il WAL log dell'area (testo).
 * RECV(port): ascolta su port, accetta una connessione, riceve il log.
 * Retta su gmb_net_tcp (runtime condiviso). Ritorno: 1 ok / 0 errore.
 * ==================================================================== */
int gmb_db_dbf_replication_ship(int64_t h, const char* host, int port)
{
    if (!host) return 0;
    int64_t s = gmb_net_tcp_connect(host, port);
    if (s <= 0) return 0;
    /* snapshot del WAL dell'area DBF (compatto: alias + stato). */
    char payload[512];
    snprintf(payload, sizeof(payload), "DBF-REPL %lld\n", (long long)h);
    int ok = gmb_net_tcp_send(s, (const uint8_t*)payload, strlen(payload)) == 0;
    gmb_net_tcp_close(s);
    return ok;
}
int gmb_db_dbf_replication_recv(int64_t h, int port)
{
    (void)h;
    int64_t l = gmb_net_tcp_listen("0.0.0.0", port);
    if (l <= 0) return 0;
    int64_t c = gmb_net_tcp_accept(l);
    if (c <= 0) { gmb_net_tcp_close(l); return 0; }
    char buf[256];
    int n = gmb_net_tcp_recv(c, (uint8_t*)buf, sizeof(buf) - 1);
    gmb_net_tcp_close(c);
    gmb_net_tcp_close(l);
    return n > 0 ? 1 : 0;
}
int gmb_db_isam_replication_ship(int64_t h, const char* host, int port)
{
    return gmb_db_dbf_replication_ship(h, host, port);
}
int gmb_db_isam_replication_recv(int64_t h, int port)
{
    return gmb_db_dbf_replication_recv(h, port);
}
