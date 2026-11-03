/* FILE: gmb_dbf.c — ABI gmb_db_dbf_* delega a DbfEngine (port 1:1) */
#include "gmb_db_internal.h"
#include "gmb_runtime.h"
#include "gmb_db_port.h"

/* Default path per i file DBF aperti con percorso relativo (parita' C# DbfSetDefaultPath). */
static char g_dbf_default_path[1024] = "";
static void dbf_resolve_path(const char* path, char* out, int cap);

static int dbf_parse_fields(const char* defs, DbfField* fields, int* nfields)
{
    char parts[GMB_DB_MAX_FIELDS][256];
    int n = gmb_db_split_csv(defs, parts, GMB_DB_MAX_FIELDS);
    int i;
    *nfields = 0;
    if (n <= 0) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        /* name deve contenere l'INTERO nome: con name[12]/ni<10 un nome >10 char
         * ("Descrizione") veniva troncato lasciando `p` sull'11° carattere, letto come
         * TYPE-CODE → campo con tipo/lunghezza sbagliati (header DBF corrotto). */
        char name[64];
        char type = 'C';
        int len = 10, dec = 0;
        const char* p = parts[i];
        int ni = 0;
        memset(name, 0, sizeof(name));
        while (*p && *p != ':' && ni < (int)sizeof(name) - 1)
            name[ni++] = (char)toupper((unsigned char)*p++);
        if (*p == ':') p++;
        if (*p) type = (char)toupper((unsigned char)*p++);
        if (*p == ':') p++;
        if (*p) len = atoi(p);
        while (*p && *p != ':') p++;
        if (*p == ':') dec = atoi(p + 1);
        if (len <= 0 || len > 255) len = 10;
        /* CURRENCY DBF = 8 byte (i64 scalato ×10000, `fp_parse_currency`/WriteCurrency).
         * Lo schema emette Y:16 (CURRENCY ISAM = decimal .NET 16 byte): forzare 8 qui. */
        if (type == 'Y') len = 8;
        fields[*nfields] = DbfField_Create(name, (uint8_t)type, (uint8_t)len, (uint8_t)dec);
        (*nfields)++;
    }
    return GMB_DB_OK;
}

static void dbf_cache_meta(gmb_handle_t* hh)
{
    DbfEngine* e = hh->u.dbf.engine;
    const DbfHeader* hdr;
    const DbfField* fields;
    int n = 0, i;
    unsigned int off = 0;
    if (!e) return;
    hdr = DbfEngine_GetHeader(e);
    fields = DbfEngine_GetFields(e, &n);
    hh->u.dbf.version = hdr ? hdr->VersionByte : 0;
    hh->u.dbf.rec_count = hdr ? hdr->NumberOfRecords : 0;
    hh->u.dbf.header_len = hdr ? hdr->HeaderLength : 0;
    hh->u.dbf.rec_len = hdr ? hdr->RecordLength : 0;
    hh->u.dbf.field_count = n > GMB_DB_MAX_FIELDS ? GMB_DB_MAX_FIELDS : n;
    for (i = 0; i < hh->u.dbf.field_count; i++) {
        memset(&hh->u.dbf.fields[i], 0, sizeof(hh->u.dbf.fields[i]));
        memcpy(hh->u.dbf.fields[i].name, fields[i].Name, 11);
        hh->u.dbf.fields[i].type = (char)fields[i].Type;
        hh->u.dbf.fields[i].length = fields[i].Length;
        hh->u.dbf.fields[i].decimals = fields[i].DecimalCount;
        hh->u.dbf.fields[i].offset = off;
        off += fields[i].Length;
    }
}

void gmb_dbf_sync(gmb_handle_t* hh)
{
    DbfEngine* e;
    const DbfHeader* hdr;
    if (!hh || !hh->u.dbf.engine) return;
    e = hh->u.dbf.engine;
    hdr = DbfEngine_GetHeader(e);
    hh->u.dbf.recno = DbfEngine_GetCurrentRecordNumber(e);
    hh->u.dbf.eof = DbfEngine_get_Eof(e);
    hh->u.dbf.bof = DbfEngine_get_Bof(e);
    hh->u.dbf.found = DbfEngine_get_Found(e);
    if (hdr) hh->u.dbf.rec_count = hdr->NumberOfRecords;
}

/* CREATE crea il file E lo APRE (come il C#: `g_dbf_wa = gmb_db_dbf_create(path, defs)`
 * nell'emitter emitter C#). Ritorna l'handle (>=0) o -1: prima ritornava uno STATO e non
 * apriva nulla → dopo il CREATE l'area corrente non esisteva (DBF APPEND/… fallivano).
 * L'apertura imposta anche l'area corrente (gmb_db_set_current_dbf dentro gmb_db_dbf_open). */
int64_t gmb_db_dbf_create(const char* path, const char* field_defs)
{
    DbfField fields[GMB_DB_MAX_FIELDS];
    int nfields = 0;
    char resolved[1024];
    if (!path || dbf_parse_fields(field_defs, fields, &nfields) != GMB_DB_OK) {
        gmb_db_set_err("invalid field defs");
        return -1;
    }
    dbf_resolve_path(path, resolved, (int)sizeof(resolved));
    if (DbfFile_CreateNew(resolved, "windows-1252", fields, nfields) != GMB_DB_OK) {
        gmb_db_set_err("cannot create dbf");
        return -1;
    }
    return gmb_db_dbf_open(resolved);
}

int gmb_db_dbf_set_default_path(const char* path)
{
    if (!path) { g_dbf_default_path[0] = 0; return GMB_DB_OK; }
    snprintf(g_dbf_default_path, sizeof(g_dbf_default_path), "%s", path);
    return GMB_DB_OK;
}

/* Risolve un percorso: se relativo e c'e' un default path, lo antepone. */
static void dbf_resolve_path(const char* path, char* out, int cap)
{
    if (path && *path && g_dbf_default_path[0]
        && !strchr(path, '/') && !strchr(path, '\\'))
    {
        snprintf(out, cap, "%s/%s", g_dbf_default_path, path);
    }
    else
    {
        snprintf(out, cap, "%s", path ? path : "");
    }
}

/* Assegna il primo numero d'area 1-based libero fra le aree DBF aperte (ordine di
 * apertura, come IL/PCode). Le aree DBF della VM/C/C99 vivono qui, non nel RDD. */
static int dbf_area_alloc(void)
{
    for (int a = 1; a < GMB_DB_MAX_HANDLES; a++) {
        int used = 0;
        for (int64_t i = 1; i < GMB_DB_MAX_HANDLES; i++) {
            if (g_handles[i].kind != GMB_H_DBF) continue;
            if (g_handles[i].u.dbf.area == a) { used = 1; break; }
        }
        if (!used) return a;
    }
    return 0;
}

int64_t gmb_db_dbf_open(const char* path)
{
    int64_t h;
    gmb_handle_t* hh;
    if (!path) {
        gmb_db_set_err("null path");
        return -1;
    }
    char resolved[1024];
    dbf_resolve_path(path, resolved, (int)sizeof(resolved));
    h = gmb_db_alloc(GMB_H_DBF);
    if (h < 0) return -1;
    hh = &g_handles[h];
    hh->u.dbf.area = 0;
    strncpy(hh->u.dbf.path, resolved, sizeof(hh->u.dbf.path) - 1);
    hh->u.dbf.fp = NULL;
    hh->u.dbf.engine = DbfEngine_New(resolved, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!hh->u.dbf.engine) {
        hh->kind = GMB_H_FREE;
        gmb_db_set_err("cannot open dbf");
        
        return -1;
    }
    dbf_cache_meta(hh);
    hh->u.dbf.area = dbf_area_alloc();
    /* Apertura: il cursore va sul PRIMO record (parity IL/PCode/VM: dopo un USE BOF è
     * falso e RECNO vale 1). Prima restava recno=0/bof=1 → "bof_iniziale" si invece di no
     * (§ MiniTests/dbf/dbf_state.bas). */
    if (hh->u.dbf.rec_count > 0) DbfEngine_GoTop(hh->u.dbf.engine);
    gmb_dbf_sync(hh);
    /* L'area APERTA diventa quella CORRENTE: senza questa riga un DBF USE dopo un DBF CLOSE
     * lasciava gmb_db_current_dbf() sull'handle CHIUSO → USED/RECSIZE/FIELDNAME/RECNO
     * davano 0/-1/"" (il "gap del reopen" documentato in MiniTests/dbf/dbf_state.bas: la VM/C
     * lo mascherava col proprio layer alias, C99 e C99Native no). */
    gmb_db_set_current_dbf(h);
    return h;
}

int gmb_db_dbf_close(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh) return GMB_DB_ERR;
    if (hh->u.dbf.engine) {
        DbfEngine_Dispose(hh->u.dbf.engine);
        hh->u.dbf.engine = NULL;
    }
    if (hh->u.dbf.fp) {
        fclose(hh->u.dbf.fp);
        hh->u.dbf.fp = NULL;
    }
    free(hh->u.dbf.idx);
    hh->u.dbf.idx = NULL;
    hh->u.dbf.area = 0;
    hh->kind = GMB_H_FREE;
    /* Se questa ERA l'area corrente, torna a un'altra area DBF aperta: senza, i comandi
     * area-correnti successivi (es. DBF AGGREGATE dopo DBF CLOSE di un'area temporanea) non
     * trovano l'area (g_last_dbf sull'handle chiuso). Parità C# (DataSession.Unregister). */
    gmb_db_dbf_after_close(h);
    return GMB_DB_OK;
}

int gmb_db_dbf_gotop(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    /* GO TOP con SET DELETED ON / filtro attivo: il cursore va sul primo record
     * VISIBILE (come GoTopInternal); prima usava GoTop fisico e il DO WHILE
     * mostrava un record cancellato (parità IL). */
    DbfEngine_GoTopInternal(hh->u.dbf.engine);
    gmb_dbf_sync(hh);
        gmb_db_dbf_sync_relations(h);
return GMB_DB_OK;
}

int gmb_db_dbf_gobottom(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_GoBottom(hh->u.dbf.engine);
    gmb_dbf_sync(hh);
        gmb_db_dbf_sync_relations(h);
return GMB_DB_OK;
}

int gmb_db_dbf_go(int64_t h, int recno)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_PositionAtRecord(hh->u.dbf.engine, recno) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
        gmb_db_dbf_sync_relations(h);
return GMB_DB_OK;
}

int gmb_db_dbf_skip(int64_t h, int n)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_Skip(hh->u.dbf.engine, n);
    gmb_dbf_sync(hh);
        gmb_db_dbf_sync_relations(h);
return GMB_DB_OK;
}

int gmb_db_dbf_eof(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.eof : 1;
}

int gmb_db_dbf_bof(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.bof : 1;
}

int gmb_db_dbf_found(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.found : 0;
}

int gmb_db_dbf_recno(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.recno : 0;
}

int gmb_db_dbf_lastrec(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? (int)hh->u.dbf.rec_count : 0;
}

int gmb_db_dbf_deleted(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return 0;
    return DbfEngine_IsCurrentRecordDeleted(hh->u.dbf.engine);
}

int gmb_db_dbf_field_count(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.field_count : 0;
}

/* RECSIZE: somma delle lunghezze dei campi, stessa semantica di C99 C#/PCode (senza il
 * byte di flag cancellazione). Prima la VM/C non la implementava (§ dbf_state.bas). */
/* LOOKUP(returnField, searchExpr, searchField [, area]) sul layer DBF: scan del primo record
 * in cui searchField == searchExpr, poi lettura di returnField (stringa vuota se non trovato).
 * Serve alla VM/C: il layer RDD ha gmb_rdd_locate, ma un'area aperta con DBF USE vive QUI
 * (§ MiniTests/dbf/dbf_lock_filter.bas). */
int gmb_db_dbf_lookup(int64_t h, const char* retField, const char* searchVal, const char* searchField, char* out, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int i, sf = -1, rf = -1, found = 0;
    if (!out || cap <= 0) return GMB_DB_ERR;
    out[0] = 0;
    if (!hh || !hh->u.dbf.engine || !retField || !searchField) return GMB_DB_ERR;
    for (i = 0; i < hh->u.dbf.field_count; i++) {
        if (strcasecmp(hh->u.dbf.fields[i].name, searchField) == 0) sf = i;
        if (strcasecmp(hh->u.dbf.fields[i].name, retField) == 0) rf = i;
    }
    if (sf < 0 || rf < 0) return GMB_DB_ERR;
    gmb_db_dbf_gotop(h);
    for (i = 0; i < hh->u.dbf.rec_count; i++) {
        char buf[256];
        if (gmb_db_dbf_get_field(h, sf, buf, (int)sizeof(buf)) == GMB_DB_OK) {
            /* I campi DBF sono padded a destra: si confrontano TRIMMATI (parity engine C#). */
            size_t bl = strlen(buf);
            const char* sv = searchVal ? searchVal : "";
            while (bl > 0 && buf[bl - 1] == ' ') buf[--bl] = 0;
            if (strcasecmp(buf, sv) == 0) { found = 1; break; }
        }
        gmb_db_dbf_skip(h, 1);
    }
    if (found) gmb_db_dbf_get_field(h, rf, out, cap);
    return GMB_DB_OK;
}

int gmb_db_dbf_record_size(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int i, sum = 0;
    if (!hh) return GMB_DB_ERR;
    for (i = 0; i < hh->u.dbf.field_count; i++) sum += hh->u.dbf.fields[i].length;
    return sum;
}

int gmb_db_dbf_field_name(int64_t h, int idx, char* buf, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || idx < 0 || idx >= hh->u.dbf.field_count || !buf || cap <= 0) return GMB_DB_ERR;
    strncpy(buf, hh->u.dbf.fields[idx].name, (size_t)cap - 1);
    buf[cap - 1] = 0;
    gmb_db_trim_field(buf);
    return GMB_DB_OK;
}

/* Tipo del campo DBF i-esimo ('C'/'N'/'F'/'L'/'D'/'M'... o 0/err). Per il record-scope
 * EVAL: permette di costruire un gmb_val_t con il tipo giusto (numerico vs stringa). */
int gmb_db_dbf_field_type(int64_t h, int idx)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || idx < 0 || idx >= hh->u.dbf.field_count) return 0;
    return (int)hh->u.dbf.fields[idx].type;
}

int gmb_db_dbf_get_field(int64_t h, int idx, char* buf, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    uint8_t rec[4096];
    gmb_dbf_field_t* f;
    int isDeleted = 0;
    DbfFile* dbf;
    if (!hh || !hh->u.dbf.engine || idx < 0 || idx >= hh->u.dbf.field_count || !buf || cap <= 0)
        return GMB_DB_ERR;
    if (hh->u.dbf.rec_len > (unsigned short)sizeof(rec)) return GMB_DB_ERR;
    dbf = DbfEngine_get_DbfFile(hh->u.dbf.engine);
    if (DbfFile_ReadRawRecordInto(dbf, hh->u.dbf.recno, rec, (int)hh->u.dbf.rec_len, &isDeleted) != GMB_DB_OK)
        return GMB_DB_ERR;
    f = &hh->u.dbf.fields[idx];
    /* §0bis — DECODIFICA tramite il PARSER CANONICO (DbfFieldParser_Parse): unica fonte di
     * verità type→testo. NON copiare i byte grezzi: i tipi BINARI (CURRENCY 'Y', INTEGER,
     * DateTime, Double) diventavano garbage (bug ricorrente: il memcpy grezzo è stato
     * reintrodotto più volte). */
    {
        DbfField df;
        memset(&df, 0, sizeof(df));
        snprintf(df.Name, sizeof(df.Name), "%s", f->name);
        df.Type = (uint8_t)f->type;
        df.Length = f->length;
        df.DecimalCount = f->decimals;
        if (DbfFieldParser_Parse(&df, rec + 1 + f->offset, (int)f->length, NULL, buf, cap) != GMB_DB_OK)
            return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

static void dbf_write_field_bytes(unsigned char* dest, const gmb_dbf_field_t* f, const char* value)
{
    /* §0bis — CODIFICA tramite il WRITER CANONICO (DbfFieldWriter_Write): unica fonte di
     * verità testo→byte. NON copiare il testo grezzo: i tipi BINARI (CURRENCY 'Y', INTEGER,
     * DateTime, Double) corrompono il record (bug ricorrente). */
    DbfField df;
    memset(&df, 0, sizeof(df));
    snprintf(df.Name, sizeof(df.Name), "%s", f->name);
    df.Type = (uint8_t)f->type;
    df.Length = f->length;
    df.DecimalCount = f->decimals;
    DbfFieldWriter_Write(dest, (int)f->length, &df, value ? value : "", NULL);
}

int gmb_db_dbf_append(int64_t h, const char* values_csv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char vals[GMB_DB_MAX_FIELDS][256];
    unsigned char rec[4096];
    int nv, i, payload;
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (hh->u.dbf.rec_len > sizeof(rec)) return GMB_DB_ERR;
    payload = (int)hh->u.dbf.rec_len - 1;
    nv = gmb_db_split_csv(values_csv, vals, GMB_DB_MAX_FIELDS);
    memset(rec, ' ', (size_t)payload);
    for (i = 0; i < hh->u.dbf.field_count; i++)
        dbf_write_field_bytes(rec + hh->u.dbf.fields[i].offset, &hh->u.dbf.fields[i],
                              i < nv ? vals[i] : "");
    if (DbfEngine_AppendRecord(hh->u.dbf.engine, rec, payload) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    /* APPEND lascia il cursore sul record appena scritto (parity IL: dopo un append BOF è
     * falso e RECNO punta all'ultimo). Senza questo, dopo il CREATE/APPEND l'area restava in
     * stato BOF (MiniTests/dbf/dbf_state.bas: "bof_iniziale" si invece di no). */
    if (hh->u.dbf.rec_count > 0) gmb_db_dbf_go(h, (int)hh->u.dbf.rec_count);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_replace_field(int64_t h, int idx, const char* value)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine || idx < 0 || idx >= hh->u.dbf.field_count) return GMB_DB_ERR;
    if (DbfEngine_ReplaceField(hh->u.dbf.engine, idx, value) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

/* Durability: FULL -> fsync ad ogni auto-commit; qualunque altro (BATCH/PERIODIC/NONE)
 * -> Buffered (il WAL resta, flush rimandato). Parita' C# DbfSetDurability. */
static int dbf_durability_from_mode(const char* mode)
{
    if (!mode) return (int)DbfDurabilityMode_Full;
    if (strcasecmp(mode, "FULL") == 0) return (int)DbfDurabilityMode_Full;
    return (int)DbfDurabilityMode_Buffered;
}

int gmb_db_dbf_set_durability(int64_t h, const char* mode)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_SetDurability(hh->u.dbf.engine, (DbfDurabilityMode)dbf_durability_from_mode(mode));
}

int gmb_db_dbf_flush_durability(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_FlushDurability(hh->u.dbf.engine);
    return GMB_DB_OK;
}

#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif

/* DBF SET EXCLUSIVE: flock esclusivo (LOCK_EX) sul file quando on, rilascio quando off.
 * Parita' del SEMANTICO Clipper 'EXCLUSIVE' (la variante C# e' un no-op commentato). */
int gmb_db_dbf_set_exclusive(int64_t h, int on)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
#ifdef __APPLE__
    {
        int fd = open(hh->u.dbf.path, O_RDWR);
        if (fd < 0) return GMB_DB_ERR;
        if (flock(fd, on ? LOCK_EX : LOCK_UN) != 0) { close(fd); return GMB_DB_ERR; }
        close(fd);
    }
#endif
    hh->u.dbf.exclusive = on ? 1 : 0;
    return GMB_DB_OK;
}

/* DBF SET SCHEMA ON: all'open successivo la coerenza interna dello schema viene
 * validata (campo count > 0, lunghezza record = 1 flag + somma lunghezze campi).
 * Parita' del SEMANTICO 'strict schema' (la C# e' un no-op commentato). */
static int dbf_schema_consistent(const DbfHeader* hdr, const DbfField* fields, int n)
{
    int i, sum = 0;
    if (!hdr || !fields || n <= 0) return 0;
    for (i = 0; i < n; i++) sum += fields[i].Length;
    if ((int)hdr->RecordLength != 1 + sum) return 0;
    return 1;
}

int gmb_db_dbf_set_schema_strict(int64_t h, int on)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh) return GMB_DB_ERR;
    hh->u.dbf.strict_schema = on ? 1 : 0;
    return GMB_DB_OK;
}

/* ── SET RELATION: registry globale (figlio → padre + chiave). Su navigazione del
 * padre (gmb_db_dbf_sync_relations chiamato da go/gotop/gobottom/skip) il figlio
 * viene riposizionato con SEEK sul valore corrente della chiave del padre — parita'
 * Clipper/CoreDb (la C# lo fa in DataSession, driver-level). */
#define GMB_DBF_MAX_RELATIONS 16
static struct { int64_t child; int64_t parent; char key[128]; } g_dbf_relations[GMB_DBF_MAX_RELATIONS];
static int g_dbf_relations_n = 0;

int gmb_db_dbf_set_relation(int64_t child, int64_t parent, const char* keyExpr)
{
    int i;
    if (child <= 0 || parent <= 0) return GMB_DB_ERR;
    for (i = 0; i < g_dbf_relations_n; i++) {
        if (g_dbf_relations[i].child == child) {
            g_dbf_relations[i].parent = parent;
            snprintf(g_dbf_relations[i].key, sizeof(g_dbf_relations[i].key), "%s", keyExpr ? keyExpr : "");
            return GMB_DB_OK;
        }
    }
    if (g_dbf_relations_n >= GMB_DBF_MAX_RELATIONS) return GMB_DB_ERR;
    g_dbf_relations[g_dbf_relations_n].child = child;
    g_dbf_relations[g_dbf_relations_n].parent = parent;
    snprintf(g_dbf_relations[g_dbf_relations_n].key, sizeof(g_dbf_relations[g_dbf_relations_n].key), "%s", keyExpr ? keyExpr : "");
    g_dbf_relations_n++;
    return GMB_DB_OK;
}

/* DBRELATION: espressione chiave della relazione attiva per l'area (stringa vuota se non
 * c'e' relazione) — stessa semantica di IlRuntime.DbfRelation. Prima la VM/C non l'aveva. */
int gmb_db_dbf_relation_key(int64_t h, char* buf, int cap)
{
    int i;
    if (!buf || cap <= 0) return GMB_DB_ERR;
    buf[0] = 0;
    for (i = 0; i < g_dbf_relations_n; i++)
        if (g_dbf_relations[i].child == h) {
            strncpy(buf, g_dbf_relations[i].key, (size_t)cap - 1);
            buf[cap - 1] = 0;
            return GMB_DB_OK;
        }
    return GMB_DB_OK;   /* nessuna relazione attiva → stringa vuota (non un errore) */
}

int gmb_db_dbf_clear_relation(int64_t child)
{
    int i;
    for (i = 0; i < g_dbf_relations_n; i++) {
        if (g_dbf_relations[i].child == child) {
            if (i < g_dbf_relations_n - 1)
                g_dbf_relations[i] = g_dbf_relations[g_dbf_relations_n - 1];
            g_dbf_relations_n--;
            return GMB_DB_OK;
        }
    }
    return GMB_DB_OK;
}

int gmb_db_dbf_sync_relations(int64_t parent)
{
    int i, rc = GMB_DB_OK;
    for (i = 0; i < g_dbf_relations_n; i++) {
        if (g_dbf_relations[i].parent == parent) {
            gmb_handle_t* ph = gmb_db_get(parent, GMB_H_DBF);
            gmb_handle_t* ch = gmb_db_get(g_dbf_relations[i].child, GMB_H_DBF);
            char keytext[256];
            int fidx;
            if (!ph || !ch || !ph->u.dbf.engine || !ch->u.dbf.engine) continue;
            fidx = -1;
            for (int q = 0; q < ph->u.dbf.field_count && fidx < 0; q++)
                if (strcasecmp(ph->u.dbf.fields[q].name, g_dbf_relations[i].key) == 0) fidx = q;
            if (fidx < 0) { rc = GMB_DB_ERR; continue; }
            keytext[0] = 0;
            DbfEngine_GetFieldText(ph->u.dbf.engine, fidx, keytext, (int)sizeof(keytext));
            { char* t1 = keytext + strlen(keytext); while (t1 > keytext && (t1[-1] == ' ' || t1[-1] == '\t')) *--t1 = 0; }
            if (gmb_db_dbf_seek(g_dbf_relations[i].child, keytext) != GMB_DB_OK)
                rc = GMB_DB_ERR;
        }
    }
    return rc;
}

/* ── DBF LIST: itera i record (ABI pubblica) e stampa i campi selezionati
 * (console o file). Parita' del comando Clipper LIST. ---- */
static int dbf_field_index_public(gmb_handle_t* hh, const char* name)
{
    int q;
    for (q = 0; q < hh->u.dbf.field_count; q++)
        if (strcasecmp(hh->u.dbf.fields[q].name, name) == 0) return q;
    return -1;
}

/* Lettura di un campo DBF PER NOME (name→index), per il late-binding Clipper di un campo
 * nudo (`ITEM` → `_dbRecord.ITEM`, che il linker C# riscrive in FieldAccess). Parità con le
 * gambe IL/PCode/VM-C, che risolvono il campo a runtime dallo schema. */
int gmb_db_dbf_get_field_name(int64_t h, const char* name, char* buf, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int idx;
    if (!hh || !name || !buf || cap <= 0) return GMB_DB_ERR;
    buf[0] = 0;
    idx = dbf_field_index_public(hh, name);
    if (idx < 0) return GMB_DB_ERR;
    return gmb_db_dbf_get_field(h, idx, buf, cap);
}

/* Lettura TIPIZZATA di un campo DBF per NOME → gmb_val_t (logico se L, CURRENCY se N/F/Y,
 * stringa altrimenti). È la late-binding Clipper del campo nudo: parità col runtime C#
 * `_dbRecord.<campo>` (IlRuntime/VM-C/pcode). Un campo NUMERICO (N/F) e un CURRENCY (Y) sono
 * resi come CURRENCY (numerico + scala letta dal formato: 100.00 → scale 2), come il decimal
 * della gamba PCode: PRINT "100.00", CSTR " 100.00", aritmetica/confronti numerici
 * (gmb_val_to_double → 100). Prima il campo Y cadeva sul ramo STRINGA → sui backend C
 * (VM/C, C99, C99Native) VALTYPE/TYPENAME davano "C"/"STRING", STR$ dava 0 e i confronti
 * non erano numerici, mentre IL/PCode lo leggevano come currency. */
gmb_val_t gmb_db_dbf_field_val_name(int64_t h, const char* name)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int idx, ty;
    char buf[4096];
    if (!hh || !name) return gmb_make_val_double(0);
    /* Nessun record corrente (BOF/EOF): il campo non ha valore → stringa vuota.
     * Prima restituiva 0 e `PRINT … ; Importo` stampava " 0" su c99/c99n mentre
     * IL/VM-C (lettura testuale) stampavano vuoto (§8 relazione su ID 1). */
    if (hh->u.dbf.bof || hh->u.dbf.eof) return gmb_make_val_string("");
    idx = dbf_field_index_public(hh, name);
    if (idx < 0) return gmb_make_val_double(0);
    ty = (int)hh->u.dbf.fields[idx].type;
    buf[0] = 0;
    (void)gmb_db_dbf_get_field(h, idx, buf, (int)sizeof(buf));
    if (ty == 'L')
        return gmb_make_val_bool((buf[0] == 'T' || buf[0] == 't' || buf[0] == 'Y'
                                  || buf[0] == 'y' || buf[0] == '1') ? -1 : 0);
    if (ty == 'N' || ty == 'F' || ty == 'Y')
    {
        const char* p = strchr(buf, '.');
        int sc = 0;
        if (p) for (p++; *p >= '0' && *p <= '9'; p++) sc++;
        return gmb_make_val_currency(atof(buf), sc);
    }
    return gmb_make_val_string(buf);
}

int gmb_db_dbf_list(int64_t h, const char* fieldsCsv, const char* destination, int toPrint)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    FILE* out;
    char destRes[1024];
    static char allf[4096];
    if (!hh) return GMB_DB_ERR;
    /* DBF LIST nudo (senza FIELDS) = TUTTI i campi (semantica Clipper), non righe vuote.
     * Prima fieldsCsv="" → nessun campo → una riga vuota per record. */
    if (!fieldsCsv || !*fieldsCsv) {
        if (gmb_db_dbf_fields(h, allf, (int)sizeof(allf)) == GMB_DB_OK && allf[0]) fieldsCsv = allf;
    }
    /* Il destinatario di LIST TO <file> è un FILE DI TESTO, non un DBF: si apre
     * rispetto alla CWD come fa `IO.FileExists("x")` (parità IL: §8B stampava
     * "File creato" su il/vmc ma non su c99/c99n perché il file finiva nel
     * default path dei DBF). */
    (void)destRes;
    out = (toPrint || !destination || !*destination) ? stdout : fopen(destination, "w");
    if (!out) return GMB_DB_ERR;
    /* Itera nell'ORDINE ATTIVO (indice/fisico): GO TOP + SKIP, NON go(r) fisico. Prima la
     * LIST ignorava l'indice attivo (SET ORDER) e usciva in ordine fisico. */
    gmb_db_dbf_gotop(h);
    while (!gmb_db_dbf_eof(h)) {
        char line[2048]; int lp = 0, fi = 0;
        const char* p = fieldsCsv ? fieldsCsv : "";
        while (*p) {
            char fn[64]; int k = 0; char txt[256];
            while (*p && *p != ',' && k < 63) fn[k++] = *p++; fn[k] = 0; if (*p == ',') p++;
            if (fi++ > 0 && lp < (int)sizeof(line) - 2) line[lp++] = ' ';
            {
                int idx = dbf_field_index_public(hh, fn);
                if (idx >= 0 && gmb_db_dbf_get_field(h, idx, txt, (int)sizeof(txt)) == 0) {
                    char* t1 = txt + strlen(txt); while (t1 > txt && (t1[-1]==' '||t1[-1]=='\t')) *--t1 = 0;
                    int w = snprintf(line + lp, (int)sizeof(line) - lp, "%s", txt); if (w > 0) lp += w;
                }
            }
        }
        fprintf(out, "%s\n", line);
        gmb_db_dbf_skip(h, 1);
    }
    if (out != stdout) fclose(out);
    return GMB_DB_OK;
}

/* ── DBF CREATE VIEW: registry nome -> path del file DBF corrente (snapshot). ---- */
#define GMB_DBF_MAX_VIEWS 32
static struct { char name[64]; char path[1024]; } g_dbf_views[GMB_DBF_MAX_VIEWS];
static int g_dbf_views_n = 0;

int gmb_db_dbf_create_view(const char* viewName)
{
    gmb_handle_t* hh = gmb_db_get(gmb_db_current_dbf(), GMB_H_DBF);
    int i;
    if (!viewName || !*viewName || !hh) return GMB_DB_ERR;
    for (i = 0; i < g_dbf_views_n; i++) {
        if (strcasecmp(g_dbf_views[i].name, viewName) == 0) {
            snprintf(g_dbf_views[i].path, sizeof(g_dbf_views[i].path), "%s", hh->u.dbf.path);
            return GMB_DB_OK;
        }
    }
    if (g_dbf_views_n >= GMB_DBF_MAX_VIEWS) return GMB_DB_ERR;
    snprintf(g_dbf_views[g_dbf_views_n].name, 64, "%s", viewName);
    snprintf(g_dbf_views[g_dbf_views_n].path, sizeof(g_dbf_views[g_dbf_views_n].path), "%s", hh->u.dbf.path);
    g_dbf_views_n++;
    return GMB_DB_OK;
}

int gmb_db_dbf_drop_view(const char* viewName)
{
    int i;
    if (!viewName) return GMB_DB_ERR;
    for (i = 0; i < g_dbf_views_n; i++) {
        if (strcasecmp(g_dbf_views[i].name, viewName) == 0) {
            if (i < g_dbf_views_n - 1) g_dbf_views[i] = g_dbf_views[g_dbf_views_n - 1];
            g_dbf_views_n--;
            return GMB_DB_OK;
        }
    }
    return GMB_DB_ERR;
}

/* ── DBF AGGREGATE: group-by sulla chiave con funzioni (SUM/AVG/COUNT/MIN/MAX) sul
 * testo "SUM(f),AVG(g)". Il file dest ha la chiave + una colonna numerica per agg. ---- */
int gmb_db_dbf_aggregate(int64_t h, const char* keyField, const char* destPath, const char* aggregations)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int keyIdx, aggN = 0, i;
    char func[32][8], aggField[32][64];
    double sums[32]; long counts[32]; double mins[32], maxs[32];
    char lastKey[256], curKey[256];
    int haveGroup = 0;
    int64_t dh;
    if (!hh || !keyField || !destPath) return GMB_DB_ERR;
    keyIdx = dbf_field_index_public(hh, keyField);
    if (keyIdx < 0) return GMB_DB_ARG;
    /* parsa aggregazioni */
    {
        const char* p = aggregations ? aggregations : "";
        char tmp[256];
        while (*p && aggN < 32) {
            int k = 0; while (*p && *p != ',' && k < 255) tmp[k++] = *p++; tmp[k] = 0; if (*p == ',') p++;
            char* paren = strchr(tmp, '(');
            if (paren) {
                char fn[8]; int fl = 0; char* q = tmp;
                while (q < paren && fl < 7) fn[fl++] = *q++;
                fn[fl] = 0; { char* t1 = fn + strlen(fn); while (t1 > fn && (t1[-1]==' '||t1[-1]=='\t')) *--t1 = 0; }
                snprintf(func[aggN], 8, "%s", fn);
                char* f2 = paren + 1; char* f3 = strchr(f2, ')');
                int w = f3 ? (int)(f3 - f2) : 0; if (w > 63) w = 63;
                memcpy(aggField[aggN], f2, (size_t)w); aggField[aggN][w] = 0;
                { char* t1 = aggField[aggN] + strlen(aggField[aggN]); while (t1 > aggField[aggN] && (t1[-1]==' '||t1[-1]=='\t')) *--t1 = 0; }
                { char* t2 = aggField[aggN]; while (*t2 && (*t2==' '||*t2=='\t')) t2++; memmove(aggField[aggN], t2, strlen(t2)+1); }
                aggN++;
            }
        }
    }
    for (i = 0; i < aggN; i++) { sums[i] = 0; counts[i] = 0; mins[i] = 0; maxs[i] = 0; }
    /* crea dest: chiave + 1 colonna per agg */
    {
        char defs[1024]; int pd = 0;
        const gmb_dbf_field_t* kf = &hh->u.dbf.fields[keyIdx];
        pd += snprintf(defs + pd, (int)sizeof(defs) - pd, "%s:%c:%d", kf->name, kf->type, kf->length);
        for (i = 0; i < aggN; i++)
            pd += snprintf(defs + pd, (int)sizeof(defs) - pd, ",%s_%s:N:18:2", func[i], aggField[i]);
        /* gmb_db_dbf_create crea E APRE (ritorna l'handle): usarlo DIRETTAMENTE. Prima si
         * faceva create() + open() → DUE handle per lo stesso file: quello del create restava
         * APERTO e VUOTO (leak) e, alla chiusura di un'area successiva, diventava la "più
         * recente area aperta" → il 2° DBF AGGREGATE leggeva 0 record. */
        dh = gmb_db_dbf_create(destPath, defs);
        if (dh <= 0) return GMB_DB_ERR;
    }
    gmb_db_set_current_dbf(dh);
    /* Itera nell'ORDINE ATTIVO (indice/fisico) via GO TOP + SKIP: il raggruppamento per chiave
     * richiede l'ordine attivo (Clipper AGGREGATE su dati ordinati). Prima go(r) FISICO →
     * gruppi sparsi (Nord,Sud,Nord,Sud invece di Nord,Sud). */
    gmb_db_dbf_gotop(h);
    while (!gmb_db_dbf_eof(h)) {
        char num[64];
        if (gmb_db_dbf_get_field(h, keyIdx, curKey, (int)sizeof(curKey)) != 0) curKey[0] = 0;
        if (haveGroup && strcmp(curKey, lastKey) != 0) {
            /* scrivi riga gruppo */
            char csv[1024]; int cp = 0;
            { char* t1 = lastKey + strlen(lastKey); while (t1 > lastKey && (t1[-1]==' '||t1[-1]=='\t')) *--t1 = 0; }
            cp += snprintf(csv + cp, (int)sizeof(csv) - cp, "%s", lastKey);
            for (i = 0; i < aggN; i++) {
                double v = 0;
                if      (strcasecmp(func[i], "SUM") == 0) v = sums[i];
                else if (strcasecmp(func[i], "AVG") == 0) v = counts[i] ? sums[i] / counts[i] : 0;
                else if (strcasecmp(func[i], "COUNT") == 0) v = (double)counts[i];
                else if (strcasecmp(func[i], "MIN") == 0) v = mins[i];
                else if (strcasecmp(func[i], "MAX") == 0) v = maxs[i];
                cp += snprintf(csv + cp, (int)sizeof(csv) - cp, ",%g", v);
            }
            gmb_db_dbf_append(dh, csv);
            memset(sums, 0, sizeof(double) * 32); memset(counts, 0, sizeof(long) * 32);
            memset(mins, 0, sizeof(double) * 32); memset(maxs, 0, sizeof(double) * 32);
        }
        snprintf(lastKey, sizeof(lastKey), "%s", curKey);
        haveGroup = 1;
        for (i = 0; i < aggN; i++) {
            int idx = dbf_field_index_public(hh, aggField[i]);
            if (idx < 0) continue;
            if (gmb_db_dbf_get_field(h, idx, num, (int)sizeof(num)) != 0) num[0] = 0;
            {
                double v = atof(num);
                if      (strcasecmp(func[i], "SUM") == 0) sums[i] += v;
                else if (strcasecmp(func[i], "AVG") == 0) { sums[i] += v; counts[i]++; }
                else if (strcasecmp(func[i], "COUNT") == 0) counts[i]++;
                else if (strcasecmp(func[i], "MIN") == 0) { if (counts[i] == 0 || v < mins[i]) mins[i] = v; counts[i]++; }
                else if (strcasecmp(func[i], "MAX") == 0) { if (counts[i] == 0 || v > maxs[i]) maxs[i] = v; counts[i]++; }
            }
        }
        gmb_db_dbf_skip(h, 1);
    }
    if (haveGroup) {
        char csv[1024]; int cp = 0;
        { char* t1 = lastKey + strlen(lastKey); while (t1 > lastKey && (t1[-1]==' '||t1[-1]=='\t')) *--t1 = 0; }
        cp += snprintf(csv + cp, (int)sizeof(csv) - cp, "%s", lastKey);
        for (i = 0; i < aggN; i++) {
            double v = 0;
            if      (strcasecmp(func[i], "SUM") == 0) v = sums[i];
            else if (strcasecmp(func[i], "AVG") == 0) v = counts[i] ? sums[i] / counts[i] : 0;
            else if (strcasecmp(func[i], "COUNT") == 0) v = (double)counts[i];
            else if (strcasecmp(func[i], "MIN") == 0) v = mins[i];
            else if (strcasecmp(func[i], "MAX") == 0) v = maxs[i];
            cp += snprintf(csv + cp, (int)sizeof(csv) - cp, ",%g", v);
        }
        gmb_db_dbf_append(dh, csv);
    }
    gmb_db_dbf_close(dh);
    gmb_db_set_current_dbf(h);
    return GMB_DB_OK;
}

/* ── DBF REPORT: stampa i record (versione base; il FORM non e' interpretato — la
 * C# non ha mai implementato DBF REPORT nel backend IL). ---- */
int gmb_db_dbf_report(int64_t h, const char* formFile, const char* destination, int toPrint)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    FILE* out;
    int32_t n, r, fi;
    char destRes[1024];
    if (!hh) return GMB_DB_ERR;
    (void)formFile;
    if (destination && *destination) { dbf_resolve_path(destination, destRes, (int)sizeof(destRes)); destination = destRes; }
    out = (toPrint || !destination || !*destination) ? stdout : fopen(destination, "w");
    if (!out) return GMB_DB_ERR;
    n = (int32_t)gmb_db_dbf_lastrec(h);
    for (r = 1; r <= n; r++) {
        char line[2048]; int lp = 0;
        gmb_db_dbf_go(h, r);
        for (fi = 0; fi < hh->u.dbf.field_count && lp < (int)sizeof(line) - 2; fi++) {
            char txt[256];
            if (fi > 0 && lp < (int)sizeof(line) - 2) line[lp++] = ' ';
            if (gmb_db_dbf_get_field(h, fi, txt, (int)sizeof(txt)) == 0) {
                char* t1 = txt + strlen(txt); while (t1 > txt && (t1[-1]==' '||t1[-1]=='\t')) *--t1 = 0;
                int w = snprintf(line + lp, (int)sizeof(line) - lp, "%s", txt); if (w > 0) lp += w;
            }
        }
        fprintf(out, "%s\n", line);
    }
    if (out != stdout) fclose(out);
    return GMB_DB_OK;
}

int gmb_db_dbf_join_with(int64_t h1, int64_t h2, const char* destPath, const char* condition)
{
    gmb_handle_t* a = gmb_db_get(h1, GMB_H_DBF);
    gmb_handle_t* b = gmb_db_get(h2, GMB_H_DBF);
    char alA[64], alB[64];
    if (!a || !a->u.dbf.engine || !b || !b->u.dbf.engine || !destPath) return GMB_DB_ERR;
    /* Alias per le condizioni qualificate Alias.Campo (il bare resta A-poi-B). */
    if (gmb_db_dbf_get_alias(h1, alA, (int)sizeof(alA)) != GMB_DB_OK) alA[0] = 0;
    if (gmb_db_dbf_get_alias(h2, alB, (int)sizeof(alB)) != GMB_DB_OK) alB[0] = 0;
    {
        char res[1024];
        dbf_resolve_path(destPath, res, (int)sizeof(res));
        return DbfEngine_Join(a->u.dbf.engine, b->u.dbf.engine, res, condition, alA, alB);
    }
}

int gmb_db_dbf_update(int64_t h, const char* fieldsCsv, const char* exprsCsv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_Update(hh->u.dbf.engine, fieldsCsv, exprsCsv);
}

/* DBF UPDATE ON <key> FROM <alias> REPLACE …: per ogni record della DESTINAZIONE cerca nel
 * SORGENTE il record con lo stesso <key> e applica le sostituzioni. Nomi nudi = campi della
 * destinazione; `<alias>.<campo>` = campi del sorgente (parità Clipper). */
int gmb_db_dbf_update_on(int64_t dest_h, int64_t src_h, const char* src_alias,
                         const char* keyField, const char* fieldsCsv, const char* exprsCsv)
{
    gmb_handle_t* d = gmb_db_get(dest_h, GMB_H_DBF);
    gmb_handle_t* s = gmb_db_get(src_h, GMB_H_DBF);
    if (!d || !d->u.dbf.engine || !s || !s->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_UpdateOn(d->u.dbf.engine, s->u.dbf.engine, src_alias, keyField,
                              fieldsCsv, exprsCsv);
}

int gmb_db_dbf_validate(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_ValidateIntegrity(hh->u.dbf.engine) == GMB_DB_OK ? GMB_DB_OK : GMB_DB_ERR;
}

int gmb_db_dbf_delete(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_DeleteCurrent(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_recall(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_RecallCurrent(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_pack(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_Pack(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}


/* --- FASE 3: ABI DBF estesa (parita' con i syscall C#) --- */

int gmb_db_dbf_append_blank(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_AppendBlankRecord(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_append_from_cond(int64_t h, const char* sourceFilePath, const char* condition)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char res[1024];
    if (!hh || !hh->u.dbf.engine || !sourceFilePath) return GMB_DB_ERR;
    dbf_resolve_path(sourceFilePath, res, (int)sizeof(res));
    if (DbfEngine_AppendFromCond(hh->u.dbf.engine, res, condition) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_append_from(int64_t h, const char* sourceFilePath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char res[1024];
    if (!hh || !hh->u.dbf.engine || !sourceFilePath) return GMB_DB_ERR;
    /* Default path (DBF SET DEFAULT TO): come open/create. Senza, un nome relativo veniva
     * aperto rispetto alla CWD del processo e il file non si trovava. */
    dbf_resolve_path(sourceFilePath, res, (int)sizeof(res));
    if (DbfEngine_AppendFrom(hh->u.dbf.engine, res) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_copy_to(int64_t h, const char* destPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char res[1024];
    if (!hh || !hh->u.dbf.engine || !destPath) return GMB_DB_ERR;
    dbf_resolve_path(destPath, res, (int)sizeof(res));
    return DbfEngine_CopyTo(hh->u.dbf.engine, res);
}

int gmb_db_dbf_sort_to(int64_t h, const char* destPath, const char* fieldName, int descending)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char res[1024];
    if (!hh || !hh->u.dbf.engine || !destPath) return GMB_DB_ERR;
    dbf_resolve_path(destPath, res, (int)sizeof(res));
    return DbfEngine_SortTo(hh->u.dbf.engine, res, fieldName, descending);
}

/* SORT multi-chiave: fieldsCsv "Citta,Saldo", descCsv "0,1" (1 = discendente).
 * Parità IL (`DbfSortStmt`→fields[]+descending[]): prima l'API accettava UN solo campo
 * e i backend C passavano solo il primo → ordine diverso da IL su SORT a più chiavi
 * (ClipperDbfLateBinding §6: "poi Saldo Decrescente"). */
int gmb_db_dbf_sort_to_multi(int64_t h, const char* destPath, const char* fieldsCsv, const char* descCsv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char res[1024];
    if (!hh || !hh->u.dbf.engine || !destPath || !fieldsCsv) return GMB_DB_ERR;
    dbf_resolve_path(destPath, res, (int)sizeof(res));
    return DbfEngine_SortToMulti(hh->u.dbf.engine, res, fieldsCsv, descCsv);
}

int gmb_db_dbf_copy_structure(int64_t h, const char* newFilePath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char res[1024];
    if (!hh || !hh->u.dbf.engine || !newFilePath) return GMB_DB_ERR;
    dbf_resolve_path(newFilePath, res, (int)sizeof(res));
    return DbfEngine_CloneStructure(hh->u.dbf.engine, res);
}

int gmb_db_dbf_total_on_cond(int64_t h, const char* destPath, const char* keyField,
                             const char* sumFieldsCsv, const char* condition)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    char parts[GMB_DB_MAX_FIELDS][256];
    const char* sumFields[GMB_DB_MAX_FIELDS];
    char res[1024];
    int n, i;
    if (!hh || !hh->u.dbf.engine || !destPath) return GMB_DB_ERR;
    dbf_resolve_path(destPath, res, (int)sizeof(res));
    destPath = res;
    n = sumFieldsCsv ? gmb_db_split_csv(sumFieldsCsv, parts, GMB_DB_MAX_FIELDS) : 0;
    for (i = 0; i < n; i++) sumFields[i] = parts[i];
    return DbfEngine_TotalOnCond(hh->u.dbf.engine, destPath, keyField, sumFields, n, condition);
}

int gmb_db_dbf_total_on(int64_t h, const char* destPath, const char* keyField,
                        const char* sumFieldsCsv)
{
    return gmb_db_dbf_total_on_cond(h, destPath, keyField, sumFieldsCsv, NULL);
}

int gmb_db_dbf_zap(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_Zap(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_set_filter(int64_t h, const char* expression)
{
    /* La condizione può arrivare SERIALIZZATA (ExprPlain del compilatore C#): un letterale
     * stringa porta le virgolette esterne → DBFILTER() le mostrerebbe (parità con IL/PCode,
     * che restituiscono il testo nudo). Si tolgono SOLO se esterne. */
    char unquoted[1024];
    if (expression && expression[0] == '"') {
        size_t n = strlen(expression);
        if (n >= 2 && expression[n - 1] == '"') {
            size_t i;
            for (i = 1; i + 1 < n && i - 1 < sizeof(unquoted) - 1; i++) unquoted[i - 1] = expression[i];
            unquoted[i - 1] = 0;
            expression = unquoted;
        }
    }
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_SetFilter(hh->u.dbf.engine, expression) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_get_filter(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine || !out || outCap <= 0) return GMB_DB_ERR;
    const char* f = DbfEngine_GetFilterExpression(hh->u.dbf.engine);
    if (!f) { out[0] = 0; return GMB_DB_OK; }
    snprintf(out, (size_t)outCap, "%s", f);
    return GMB_DB_OK;
}

int gmb_db_dbf_set_deleted(int64_t h, int on)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_SetDeleted(hh->u.dbf.engine, on);
    return GMB_DB_OK;
}

int gmb_db_dbf_set_exact(int64_t h, int on)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_SetExact(hh->u.dbf.engine, on);
    return GMB_DB_OK;
}

int gmb_db_dbf_set_replication(int64_t h, const char* logPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_SetReplication(hh->u.dbf.engine, logPath);
}

int gmb_db_dbf_apply_replication(int64_t h, const char* replLogPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_ApplyReplication(hh->u.dbf.engine, replLogPath);
}

int gmb_db_dbf_follow_replication(int64_t h, const char* replLogPath, const char* checkpointPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_FollowReplication(hh->u.dbf.engine, replLogPath, checkpointPath);
}

/* RLOCK/FLOCK: lock in-process di record/file. Il runtime DBF C non aveva un lock manager
 * (la VM/C ritornava sempre 0): tabella per-handle condivisa da VM/C, C99 e C99Native
 * (§0bis). GMB_DB_OK = lock acquisito (la VM mappa su -1, come IL/PCode). */
#define GMB_DBF_MAX_LOCKS 64
static struct { int64_t h; int rlock; int flock; } g_dbf_locks[GMB_DBF_MAX_LOCKS];
static int dbf_lock_slot(int64_t h)
{
    int i, free_slot = -1;
    if (h <= 0) return -1;
    for (i = 0; i < GMB_DBF_MAX_LOCKS; i++) {
        if (g_dbf_locks[i].h == h) return i;
        if (free_slot < 0 && g_dbf_locks[i].h == 0) free_slot = i;
    }
    if (free_slot >= 0) g_dbf_locks[free_slot].h = h;
    return free_slot;
}
int gmb_db_dbf_rlock(int64_t h) { int sl = dbf_lock_slot(h); if (sl < 0) return GMB_DB_ERR; g_dbf_locks[sl].rlock = 1; return GMB_DB_OK; }
int gmb_db_dbf_flock(int64_t h) { int sl = dbf_lock_slot(h); if (sl < 0) return GMB_DB_ERR; g_dbf_locks[sl].flock = 1; return GMB_DB_OK; }

int gmb_db_dbf_begin_group_commit(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_BeginGroupCommit(hh->u.dbf.engine);
    return GMB_DB_OK;
}

int gmb_db_dbf_end_group_commit(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    DbfEngine_EndGroupCommit(hh->u.dbf.engine);
    return GMB_DB_OK;
}

int gmb_db_dbf_begin_transaction(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    return DbfEngine_BeginTransaction(hh->u.dbf.engine);
}

int gmb_db_dbf_commit(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_Commit(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_rollback(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_Rollback(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    dbf_cache_meta(hh);
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}


/* Nomi campo DBF separati da virgola — per il marshalling dict->CSV nel VM/C. */
int gmb_db_dbf_fields(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    const DbfField* fields;
    int n = 0, i, pos = 0;
    if (!hh || !hh->u.dbf.engine || !out || outCap <= 0) return GMB_DB_ERR;
    fields = DbfEngine_GetFields(hh->u.dbf.engine, &n);
    if (!fields) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        if (pos >= outCap - 1) break;
        pos += snprintf(out + pos, (size_t)(outCap - pos), "%s%s",
            i ? "," : "", fields[i].Name);
    }
    return GMB_DB_OK;
}


/* Data ultimo aggiornamento (header DBF): YYYYMMDD -> stringa. */
int gmb_db_dbf_lupdate(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    const DbfHeader* hdr;
    if (!hh || !hh->u.dbf.engine || !out || outCap <= 0) return GMB_DB_ERR;
    hdr = DbfEngine_GetHeader(hh->u.dbf.engine);
    if (!hdr) return GMB_DB_ERR;
    /* L'header DBF scrive l'anno come DateTime.Now.Year % 100 (DbfFile.cs:275/348), NON
     * "anni dal 1900": con +1900 un file del 2026 (byte 26) dava 1926 (§ MiniTests/dbf/
     * dbf_replication.bas: VMC "19260911"). Il riferimento è il path PCode, che costruisce
     * GbDate(2000 + Year, Month, Day) e stampa "2026-09-11": stesso anno e stesso formato. */
    /* Il byte è "Year - 1900" (writer C, DbfFile_CreateNew: 126 per il 2026) oppure
     * "Year % 100" (writer C#): la regola sotto copre ENTRAMBI (126 -> 2026, 26 -> 2026). */
    {
        int yy = (int)hdr->Year < 100 ? 2000 + (int)hdr->Year : 1900 + (int)hdr->Year;
        snprintf(out, (size_t)outCap, "%04d-%02d-%02d", yy, (int)hdr->Month, (int)hdr->Day);
    }
    return GMB_DB_OK;
}


/* Replace campo per NOME (usa DbfEngine_ReplaceFieldName). */
int gmb_db_dbf_replace_field_name(int64_t h, const char* fieldName, const char* value)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine || !fieldName) return GMB_DB_ERR;
    if (DbfEngine_ReplaceFieldName(hh->u.dbf.engine, fieldName, value) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}


/* DBF.APPENDMANY(list): bulk insert da LIST OF UDT boxate (gmb_udt_box). Per ogni
 * elemento: risolve il layout UDT dal type_name (registrato dall'emitter con
 * gmb_type_register_udt + offsetof), legge i campi in ordine di schema e appende. */
/* Variante per gli emitter C: prende un ARRAY di valori boxati (l'array di UDT del C
 * generato) invece di una gmb_list_t già costruita — così l'emissione resta una ESPRESSIONE
 * pura (niente blocchi `({…})` dentro gli statement, che rompevano le label del gestore
 * errori) e ritorna il numero di record scritti. */
int gmb_db_dbf_append_many_boxed(int64_t h, const gmb_val_t* vals, int n)
{
    gmb_list_t l = gmb_list_new();
    int i, rc = 0;
    if (!vals || n <= 0) return 0;
    for (i = 0; i < n; i++) gmb_list_add(&l, vals[i]);
    rc = (int)gmb_db_dbf_append_many(h, l);
    gmb_list_free(&l);
    /* Il runtime ritorna uno STATO (GMB_DB_OK = 0): il builtin, come IlRuntime.*AppendMany,
     * ritorna il NUMERO di record scritti (l'oracolo dei MiniTest è "appendmany: 2"). */
    /* append_many ritorna il NUMERO di record scritti (o <= 0 in errore): è il valore che
     * l'oracolo espone (IlRuntime.*AppendMany → records.Count). */
    return rc > 0 ? rc : 0;
}

int gmb_db_dbf_append_many(int64_t h, gmb_list_t list)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    char fieldsBuf[4096];
    if (gmb_db_dbf_fields(h, fieldsBuf, sizeof(fieldsBuf)) != GMB_DB_OK) return GMB_DB_ERR;
        /* Parse nomi campi schema: gmb_db_dbf_fields ritorna "ID,NAME" (solo nomi, ',' separati). */
    char fnames[GMB_DB_MAX_FIELDS][64];
    int nf = 0;
    {
        char* p = fieldsBuf;
        while (*p && nf < GMB_DB_MAX_FIELDS) {
            char* comma = strchr(p, ',');
            size_t fl = comma ? (size_t)(comma - p) : strlen(p);
            if (fl > 63) fl = 63;
            memcpy(fnames[nf], p, fl); fnames[nf][fl] = 0;
            nf++;
            if (!comma) break;
            p = comma + 1;
        }
    }
    int ok = 0;
    for (size_t i = 0; i < list.count; i++) {
        gmb_val_t v = gmb_list_get(list, (gmb_int_t)i);
        if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) continue;
        gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
        const char* tn = o->type_name;
        if (!tn || !*tn) continue;
        /* tn = "gmb_udt_Row" → "Row" */
        const char* udt = strstr(tn, "gmb_udt_");
        udt = udt ? udt + strlen("gmb_udt_") : tn;
        const uint8_t* base = (const uint8_t*)o->data;
        char csv[4096]; int pos = 0; csv[0] = 0;
        int first = 1;
        for (int f = 0; f < nf; f++) {
            long off = 0; int t = 0;
            char val[1024];
            if (gmb_type_udt_lookup(udt, fnames[f], &off, &t)) {
                gmb_type_udt_read_field(base, off, t, val, sizeof(val));
                } else { val[0] = 0; }
            if (!first) { if (pos < (int)sizeof(csv) - 1) csv[pos++] = ','; }
            first = 0;
            int w = snprintf(csv + pos, sizeof(csv) - (size_t)pos, "%s", val);
            if (w > 0) pos += w;
            if (pos >= (int)sizeof(csv) - 1) break;
        }
        if (gmb_db_dbf_append(h, csv) == GMB_DB_OK) ok++;
    }
    return ok;
}

/* Variante gmb_string_t di get_filter (C99: niente buffer inline nell'emitter). */
gmb_string_t gmb_db_dbf_get_filter_str(int64_t h)
{
    char buf[4096]; buf[0] = 0;
    gmb_db_dbf_get_filter(h, buf, sizeof(buf));
    return gmb_string_alloc(buf);
}

/* ALIAS()/SELECTED() usati dagli emitter C (C99Native): stanno qui e non nel
 * runtime RDD perché devono consultare ENTRAMBI — l'alias dell'area DBF e quello RDD. */
gmb_string_t gmb_db_dbf_get_alias_str(int64_t h)
{
    char buf[128]; buf[0] = 0;
    if (gmb_db_dbf_get_alias(h, buf, (int)sizeof(buf)) != GMB_DB_OK) buf[0] = 0;
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_alias(int area)
{
    char buf[128]; buf[0] = 0;
    (void)area;
    if (gmb_db_dbf_get_alias(gmb_db_current_dbf(), buf, (int)sizeof(buf)) != GMB_DB_OK || !buf[0])
        gmb_rdd_alias(buf, (int)sizeof(buf));
    return gmb_string_alloc(buf);
}

int gmb_db_current_area(void)
{
    int64_t h = gmb_db_current_dbf();
    if (h <= 0) return 0;
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.area : 0;
}

int gmb_db_area_of_alias(const char* alias)
{
    int64_t h = gmb_db_dbf_find_handle_by_alias(alias);
    if (h <= 0) return 0;
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    return hh ? hh->u.dbf.area : 0;
}

gmb_double_t gmb_selected(const char* alias)
{
    if (!alias || !alias[0]) return 0;
    /* `SELECTED(<numero>)` e `<numero>` passato come stringa: è il NUMERO d'area 1-based,
     * non un alias. Ritorna l'area se è aperta, altrimenti 0 (parità IL/PCode). */
    {
        char* end = NULL;
        double n = strtod(alias, &end);
        if (end && end != alias && *end == 0 && n >= 1 && n <= 65535 && n == (double)(int)n)
            return gmb_db_dbf_find_handle_by_area((int)n) > 0 ? n : 0;
    }
    {
        /* ISAM: gli alias ISAM sono in g_handles[].u.isam.alias (gmb_db_isam_set_alias),
         * NON nel registro RDD → gmb_rdd_select_alias dava 0 (ISAM.SELECTED=0 sulle gambe C). */
        int64_t ih = gmb_db_isam_selected(alias);
        if (ih > 0) return (gmb_double_t)ih;
    }
    return (gmb_double_t)gmb_rdd_select_alias(alias);
}

/* SET/GET ALIAS dell'area DBF: ALIAS() e SELECTED() nel mondo C (una sola area). */
int gmb_db_dbf_set_alias(int64_t h, const char* alias)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh) return GMB_DB_ERR;
    snprintf(hh->u.dbf.alias, sizeof(hh->u.dbf.alias), "%s", alias ? alias : "");
    return GMB_DB_OK;
}

int gmb_db_dbf_get_alias(int64_t h, char* out, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !out || cap <= 0) return GMB_DB_ERR;
    snprintf(out, (size_t)cap, "%s", hh->u.dbf.alias);
    return GMB_DB_OK;
}

int64_t gmb_db_dbf_find_handle_by_area(int area)
{
    if (area <= 0) return 0;
    for (int64_t i = 1; i < GMB_DB_MAX_HANDLES; i++) {
        if (g_handles[i].kind != GMB_H_DBF) continue;
        if (g_handles[i].u.dbf.area == area) return i;
    }
    return 0;
}

int64_t gmb_db_dbf_find_handle_by_alias(const char* alias)
{
    if (!alias || !alias[0]) return 0;
    char buf[64];
    for (int64_t i = 1; i < GMB_DB_MAX_HANDLES; i++) {
        if (g_handles[i].kind != GMB_H_DBF) continue;
        if (gmb_db_dbf_get_alias(i, buf, (int)sizeof(buf)) != GMB_DB_OK) continue;
        if (strcasecmp(buf, alias) == 0) return i;
    }
    return 0;
}

/* INDEXORD: numero dell'ordine attivo (0 = fisico) — LongType in StdLib, NON una stringa. */
gmb_double_t gmb_indexord_dbl(int area)
{
    gmb_handle_t* hh = gmb_db_get(gmb_db_current_dbf(), GMB_H_DBF);
    (void)area;
    return hh ? (gmb_double_t)hh->u.dbf.active_order : 0.0;
}

/* INDEXORD/INDEXKEY/NETERR/FIELDTYPE: REFERENZIATI dal transpiler C#
 * (C99CodeEmitter.Builtin.Core.cs: gmb_indexord/gmb_indexkey/gmb_neterr/gmb_fieldtype) e mai
 * definiti → il C generato non compilava. Vivono nel runtime DBF condiviso (§0bis). */
gmb_string_t gmb_indexord(int area)
{
    int64_t h = gmb_db_current_dbf();
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int ai;
    (void)area;
    if (!hh || hh->u.dbf.active_order <= 0) return gmb_string_alloc("");
    ai = hh->u.dbf.active_order - 1;
    if (ai >= hh->u.dbf.index_count) return gmb_string_alloc("");
    {
        const char* p = strrchr(hh->u.dbf.index_path[ai], '/');
        return gmb_string_alloc(p ? p + 1 : hh->u.dbf.index_path[ai]);
    }
}

gmb_string_t gmb_indexkey(int area)
{
    int64_t h = gmb_db_current_dbf();
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int ai;
    char buf[128]; buf[0] = 0;
    (void)area;
    if (!hh || hh->u.dbf.active_order <= 0) return gmb_string_alloc("");
    ai = hh->u.dbf.active_order - 1;
    if (ai >= hh->u.dbf.index_count) return gmb_string_alloc("");
    if (gmb_db_dbf_field_name(h, hh->u.dbf.index_key_field[ai], buf, (int)sizeof(buf)) != GMB_DB_OK) buf[0] = 0;
    return gmb_string_alloc(buf);
}

/* NETERR: il motore C delle aree non ha un layer di rete → nessun errore di rete (0).
 * Non è uno stub silenzioso: è l'assenza del sottosistema, dichiarata qui. */
gmb_double_t gmb_neterr(int area)
{
    (void)area;
    return 0.0;
}

/* FIELDTYPE(n): lettera del tipo DBF del campo n (C/N/L/D/...) come stringa. */
gmb_string_t gmb_fieldtype(int idx)
{
    int64_t h = gmb_db_current_dbf();
    int t = gmb_db_dbf_field_type(h, idx);
    char buf[8];
    buf[0] = (char)(t ? t : 'C');
    buf[1] = 0;
    return gmb_string_alloc(buf);
}

/* SETREPLICATION come espressione: ritorna il PATH del log (StringType), vuoto se fallisce
 * — stessa semantica di IlRuntime.DbfSetReplication e del ramo PCode. */
gmb_string_t gmb_db_dbf_set_replication_str(int64_t h, const char* logPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (gmb_db_dbf_set_replication(h, logPath) != GMB_DB_OK) return gmb_string_alloc("");
    /* Ritorna il path RISOLTO dal motore, non quello passato: `DbfEngine_SetReplication`
     * trasforma "."/"DEFAULT" in `<percorso tabella>.repl` (identica regola del motore di
     * riferimento, `modules/Database/DBF/DbfEngine.cs:SetReplication`), e il backend IL indeed
     * ritorna quel path. Prima si ritornava la stringa in ingresso → con
     * `DBF.SETREPLICATION(Primary, ".")` la VM/C stampava `Replication log: .` mentre IL
     * stampava `/.../work/dbf_repl.dbf.repl` (misurato su DbfReplication.bas). */
    if (hh && hh->u.dbf.engine)
    {
        const char* rp = DbfEngine_GetReplicationLogPath(hh->u.dbf.engine);
        if (rp && rp[0]) return gmb_string_alloc(rp);
    }
    return gmb_string_alloc("");
}

/* LOOKUP come espressione (parity IlRuntime.DbfLookup / gmb_bf_446 codice 445). */
gmb_string_t gmb_db_dbf_lookup_str(int64_t h, const char* retField, const char* searchVal, const char* searchField)
{
    char out[512]; out[0] = 0;
    if (gmb_db_dbf_lookup(h, retField, searchVal, searchField, out, (int)sizeof(out)) != GMB_DB_OK) out[0] = 0;
    return gmb_string_alloc(out);
}

/* Versione STRINGA di field_name / relation_key: l'emitter C99Native (e i backend che
 * devono produrre un'espressione) non possono usare le varianti con buffer di uscita. */
gmb_string_t gmb_db_dbf_field_name_str(int64_t h, int idx)
{
    char buf[64]; buf[0] = 0;
    if (gmb_db_dbf_field_name(h, idx, buf, (int)sizeof(buf)) != GMB_DB_OK) buf[0] = 0;
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_db_dbf_relation_key_str(int64_t h)
{
    char buf[512]; buf[0] = 0;
    if (gmb_db_dbf_relation_key(h, buf, (int)sizeof(buf)) != GMB_DB_OK) buf[0] = 0;
    return gmb_string_alloc(buf);
}

gmb_string_t gmb_db_dbf_lupdate_str(int64_t h)
{
    char buf[64]; buf[0] = 0;
    gmb_db_dbf_lupdate(h, buf, sizeof(buf));
    return gmb_string_alloc(buf);
}
