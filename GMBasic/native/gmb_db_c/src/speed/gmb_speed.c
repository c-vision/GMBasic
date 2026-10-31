/* FILE: gmb_speed.c — ABI gmb_db_speed_* delega a Database (port 1:1) */
#include "gmb_db_internal.h"
#include "gmb_runtime.h"

/* forward declarations for filter functions (defined below) */
static int  speed_filter_pass(int64_t h, int tableId);
static void speed_filter_advance(int64_t h, int tableId, int direction);

int64_t gmb_db_speed_open(const char* dir)
{
    int64_t h;
    gmb_handle_t* hh;
    if (!dir) {
        gmb_db_set_err("null speed path");
        return -1;
    }
    h = gmb_db_alloc(GMB_H_SPEED);
    if (h < 0) return -1;
    hh = &g_handles[h];
    hh->u.speed.db = Database_Open(dir);
    if (!hh->u.speed.db) {
        hh->kind = GMB_H_FREE;
        gmb_db_set_err("cannot open speed db");
        return -1;
    }
    /* L'area APERTA diventa quella CORRENTE (stesso fix dei rami DBF/ISAM/LEDGER):
     * senza, gmb_db_current_speed() restava sull'handle precedente e ogni chiamata
     * diretta (C99Native) lavorava su un'area inesistente. */
    gmb_db_set_current_speed(h);
    return h;
}

int gmb_db_speed_close(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh) return GMB_DB_ERR;
    if (hh->u.speed.db) {
        Database_Dispose(hh->u.speed.db);
        hh->u.speed.db = NULL;
    }
    hh->kind = GMB_H_FREE;
    return GMB_DB_OK;
}

int gmb_db_speed_set(int64_t h, int table_id, const void* key, int klen, const void* val, int vlen)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db || !key || klen < 0 || vlen < 0) return GMB_DB_ERR;
    if (Database_Set(hh->u.speed.db, table_id, (const uint8_t*)key, klen,
                     (const uint8_t*)val, vlen) != GMB_DB_OK) {
        gmb_db_set_err("speed set failed");
        return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int gmb_db_speed_get(int64_t h, int table_id, const void* key, int klen, void* val, int* vlen)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    uint8_t* got = NULL;
    int glen = 0;
    if (!hh || !hh->u.speed.db || !key || !vlen) return GMB_DB_ERR;
    if (!Database_Get(hh->u.speed.db, table_id, (const uint8_t*)key, klen, &got, &glen))
        return GMB_DB_NOTFOUND;
    if (val && *vlen >= glen && glen > 0)
        memcpy(val, got, (size_t)glen);
    *vlen = glen;
    free(got);
    return GMB_DB_OK;
}

int gmb_db_speed_delete(int64_t h, int table_id, const void* key, int klen)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    uint8_t* got = NULL;
    int glen = 0;
    if (!hh || !hh->u.speed.db || !key) return GMB_DB_ERR;
    if (!Database_Get(hh->u.speed.db, table_id, (const uint8_t*)key, klen, &got, &glen))
        return GMB_DB_NOTFOUND;
    free(got);
    if (Database_Delete(hh->u.speed.db, table_id, (const uint8_t*)key, klen) != GMB_DB_OK)
        return GMB_DB_ERR;
    return GMB_DB_OK;
}

int gmb_db_speed_compact(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    if (Database_CompactAll(hh->u.speed.db) < 0) return GMB_DB_ERR;
    return GMB_DB_OK;
}


/* --- FASE 3: ABI SPEED estesa (parita' con i syscall C#) --- */

/* ── SPEED SET RELATION: registry figlio->padre (la chiave del padre = key di seek
 * del figlio). Sync su navigazione del padre. ── */
#define GMB_SPEED_MAX_RELATIONS 16
static struct { int64_t child; int64_t parent; int childTable; int parentTable; } g_speed_relations[GMB_SPEED_MAX_RELATIONS];
static int g_speed_relations_n = 0;

/* ── Tabella alias -> tableId (popolato da SPEED USE/SELECT). ── */
#define GMB_SPEED_MAX_ALIAS 32
static struct { char alias[64]; int tableId; } g_speed_table_alias[GMB_SPEED_MAX_ALIAS];
static int g_speed_table_alias_n = 0;

int gmb_db_speed_register_table_alias(const char* alias, int tableId)
{
    int i;
    if (!alias || !*alias) return GMB_DB_ERR;
    for (i = 0; i < g_speed_table_alias_n; i++) {
        if (strcasecmp(g_speed_table_alias[i].alias, alias) == 0) {
            g_speed_table_alias[i].tableId = tableId;
            return GMB_DB_OK;
        }
    }
    if (g_speed_table_alias_n >= GMB_SPEED_MAX_ALIAS) return GMB_DB_ERR;
    snprintf(g_speed_table_alias[g_speed_table_alias_n].alias, 64, "%s", alias);
    g_speed_table_alias[g_speed_table_alias_n].tableId = tableId;
    g_speed_table_alias_n++;
    return GMB_DB_OK;
}

int gmb_db_speed_table_by_alias(const char* alias)
{
    int i;
    if (!alias) return -1;
    for (i = 0; i < g_speed_table_alias_n; i++)
        if (strcasecmp(g_speed_table_alias[i].alias, alias) == 0) return g_speed_table_alias[i].tableId;
    return -1;
}

int gmb_db_speed_set_relation_table(int64_t h, int parentTable, int childTable)
{
    int i;
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (h <= 0 || !hh) return GMB_DB_ERR;
    for (i = 0; i < g_speed_relations_n; i++) {
        if (g_speed_relations[i].child == h && g_speed_relations[i].childTable == childTable) {
            g_speed_relations[i].parent = h;
            g_speed_relations[i].childTable = childTable;
            g_speed_relations[i].parentTable = parentTable;
            return GMB_DB_OK;
        }
    }
    if (g_speed_relations_n >= GMB_SPEED_MAX_RELATIONS) return GMB_DB_ERR;
    g_speed_relations[g_speed_relations_n].child = h;
    g_speed_relations[g_speed_relations_n].parent = h;
    g_speed_relations[g_speed_relations_n].childTable = childTable;
    g_speed_relations[g_speed_relations_n].parentTable = parentTable;
    g_speed_relations_n++;
    return GMB_DB_OK;
}

/* ── VIEW SPEED: registry viewName -> source tableId (snapshot del table sorgente;
 * la query SELECT completa richiederebbe un parser SQL-like — la vista esegue la
 * scansione del table sorgente come la C# SpeedRunViewQuery senza filtri). ── */
#define GMB_SPEED_MAX_VIEWS 32
static struct { char name[64]; int tableId; } g_speed_views[GMB_SPEED_MAX_VIEWS];
static int g_speed_views_n = 0;

int gmb_db_speed_create_view(const char* viewName, int fromTableId)
{
    int i;
    if (!viewName || !*viewName || fromTableId < 0) return GMB_DB_ERR;
    for (i = 0; i < g_speed_views_n; i++) {
        if (strcasecmp(g_speed_views[i].name, viewName) == 0) {
            g_speed_views[i].tableId = fromTableId;
            return GMB_DB_OK;
        }
    }
    if (g_speed_views_n >= GMB_SPEED_MAX_VIEWS) return GMB_DB_ERR;
    snprintf(g_speed_views[g_speed_views_n].name, 64, "%s", viewName);
    g_speed_views[g_speed_views_n].tableId = fromTableId;
    g_speed_views_n++;
    return GMB_DB_OK;
}

int gmb_db_speed_refresh_view(const char* viewName)
{
    int i;
    if (!viewName) return GMB_DB_ERR;
    for (i = 0; i < g_speed_views_n; i++)
        if (strcasecmp(g_speed_views[i].name, viewName) == 0) return GMB_DB_OK;
    return GMB_DB_ERR;
}

int gmb_db_speed_drop_view(const char* viewName)
{
    int i;
    if (!viewName) return GMB_DB_ERR;
    for (i = 0; i < g_speed_views_n; i++) {
        if (strcasecmp(g_speed_views[i].name, viewName) == 0) {
            if (i < g_speed_views_n - 1)
                g_speed_views[i] = g_speed_views[g_speed_views_n - 1];
            g_speed_views_n--;
            return GMB_DB_OK;
        }
    }
    return GMB_DB_ERR;
}

int gmb_db_speed_view_table(const char* viewName)
{
    int i;
    if (!viewName) return -1;
    for (i = 0; i < g_speed_views_n; i++)
        if (strcasecmp(g_speed_views[i].name, viewName) == 0) return g_speed_views[i].tableId;
    return -1;
}

int gmb_db_speed_set_relation(int64_t child, int64_t parent)
{
    int i;
    gmb_handle_t* ch = gmb_db_get(child, GMB_H_SPEED);
    if (child <= 0 || parent <= 0 || !ch) return GMB_DB_ERR;
    for (i = 0; i < g_speed_relations_n; i++) {
        if (g_speed_relations[i].child == child) {
            g_speed_relations[i].parent = parent;
            g_speed_relations[i].childTable = ch->u.speed.curTable;
            g_speed_relations[i].parentTable = -1;
            return GMB_DB_OK;
        }
    }
    if (g_speed_relations_n >= GMB_SPEED_MAX_RELATIONS) return GMB_DB_ERR;
    g_speed_relations[g_speed_relations_n].child = child;
    g_speed_relations[g_speed_relations_n].parent = parent;
    g_speed_relations[g_speed_relations_n].childTable = ch->u.speed.curTable;
    g_speed_relations[g_speed_relations_n].parentTable = -1;
    g_speed_relations_n++;
    return GMB_DB_OK;
}

int gmb_db_speed_clear_relation(int64_t child)
{
    int i;
    for (i = 0; i < g_speed_relations_n; i++) {
        if (g_speed_relations[i].child == child) {
            if (i < g_speed_relations_n - 1)
                g_speed_relations[i] = g_speed_relations[g_speed_relations_n - 1];
            g_speed_relations_n--;
            return GMB_DB_OK;
        }
    }
    return GMB_DB_OK;
}

int gmb_db_speed_sync_relations(int64_t parent)
{
    int i, rc = GMB_DB_OK;
    for (i = 0; i < g_speed_relations_n; i++) {
        if (g_speed_relations[i].parent == parent) {
            gmb_handle_t* ph = gmb_db_get(parent, GMB_H_SPEED);
            gmb_handle_t* ch = gmb_db_get(g_speed_relations[i].child, GMB_H_SPEED);
            const uint8_t* key = NULL; int klen = 0;
            int pt = g_speed_relations[i].parentTable >= 0 ? g_speed_relations[i].parentTable : (ph ? ph->u.speed.curTable : -1);
            if (!ph || !ch || !ph->u.speed.db || !ch->u.speed.db) continue;
            if (!Database_GetCurrentKey(ph->u.speed.db, pt, &key, &klen) || !key || klen <= 0)
                { rc = GMB_DB_ERR; continue; }
            if (!Database_Seek(ch->u.speed.db, g_speed_relations[i].childTable, key, klen, NULL))
                rc = GMB_DB_ERR;
        }
    }
    return rc;
}

int gmb_db_speed_set_durability(int64_t h, const char* mode)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh) return GMB_DB_ERR;
    int is_full = (mode && strcasecmp(mode, "FULL") == 0) ? 1 : 0;
    hh->u.speed.durability = is_full ? 0 : 1;
    /* Cabla la durability nel MOTORE (prima si scriveva solo un campo mai letto →
     * fsync per ogni set indipendentemente dalla modalità). */
    if (hh->u.speed.db) Database_SetDurability(hh->u.speed.db, is_full);
    return GMB_DB_OK;
}

int gmb_db_speed_flush_durability(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    Database_FlushDurability(hh->u.speed.db);
    return GMB_DB_OK;
}

int gmb_db_speed_begin_group_commit(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    Database_BeginGroupCommit(hh->u.speed.db);
    return GMB_DB_OK;
}

int gmb_db_speed_end_group_commit(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    return Database_EndGroupCommit(hh->u.speed.db);
}

/* Variante stringa (canone StdLib: SETDURABILITY ritorna la modalita applicata). */
gmb_string_t gmb_db_speed_set_durability_str(int64_t h, const char* mode)
{
    if (gmb_db_speed_set_durability(h, mode) != GMB_DB_OK) return gmb_string_alloc("");
    return gmb_string_alloc((mode && strcasecmp(mode, "FULL") == 0) ? "Full" : "Buffered");
}

/* SPEED.INFO([alias]) — riepilogo deterministico dello stato del database SPEED. */
gmb_string_t gmb_db_speed_info_str(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    char buf[512];
    int full = 1, gc = 0, table = -1;
    if (hh) {
        table = hh->u.speed.curTable;
        full = hh->u.speed.durability == 0 ? 1 : 0;
        if (hh->u.speed.db) {
            full = Database_GetDurabilityFull(hh->u.speed.db);
            gc = Database_GetGroupCommitDepth(hh->u.speed.db);
        }
    }
    snprintf(buf, sizeof(buf),
        "{\"durability_mode\":\"%s\",\"group_commit\":%s,\"cur_table\":%d}",
        full ? "Full" : "Buffered", gc > 0 ? "true" : "false", table);
    return gmb_string_alloc(buf);
}

int gmb_db_speed_curtable(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    return (hh) ? hh->u.speed.curTable : -1;
}

int gmb_db_speed_use(int64_t h, int tableId)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    hh->u.speed.curTable = tableId;
    return GMB_DB_OK;
}

int gmb_db_speed_create_table(int64_t h, const char* typeName, const char* fieldsCsv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    char parts[GMB_DB_MAX_FIELDS][256];
    const char* names[GMB_DB_MAX_FIELDS];
    const char* types[GMB_DB_MAX_FIELDS];
    int lens[GMB_DB_MAX_FIELDS];
    int n, i;
    if (!hh || !hh->u.speed.db || !typeName) return GMB_DB_ERR;
    n = fieldsCsv ? gmb_db_split_csv(fieldsCsv, parts, GMB_DB_MAX_FIELDS) : 0;
    for (i = 0; i < n; i++) {
        /* formato "Nome:Tipo:Len" per campo; Tipo = codice 1-lettera o nome tipo */
        char* p = parts[i];
        char* colon1 = strchr(p, ':');
        char* colon2 = colon1 ? strchr(colon1 + 1, ':') : NULL;
        static const char* TYPMAP[256] = {0};
        char code;
        if (!TYPMAP['C']) {
            TYPMAP['C'] = "String"; TYPMAP['M'] = "String";
            TYPMAP['N'] = "Long";   TYPMAP['I'] = "Integer";
            TYPMAP['F'] = "Double"; TYPMAP['L'] = "Bit";
            TYPMAP['D'] = "Date";   TYPMAP['J'] = "Json";
        }
        if (!colon1) { names[i] = p; types[i] = "String"; lens[i] = 20; continue; }
        *colon1 = 0;
        names[i] = p;
        if (colon2) { *colon2 = 0; lens[i] = atoi(colon2 + 1); }
        else lens[i] = 20;
        code = colon1[1];
        if (code && TYPMAP[(unsigned char)code] && colon1[2] == 0)
            types[i] = TYPMAP[(unsigned char)code];
        else
            types[i] = colon1[1] ? colon1 + 1 : "String";
    }
    return Database_CreateTableWithFields(hh->u.speed.db, typeName, names, types, lens, n);
}

int gmb_db_speed_drop_table(int64_t h, const char* tableName)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db || !tableName) return GMB_DB_ERR;
    return Database_DropTableInternal(hh->u.speed.db, tableName);
}

int gmb_db_speed_create_index(int64_t h, int tableId, const char* indexName,
                              const char* keyExpression, int keyLength)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    return Database_CreateSecondaryIndex(hh->u.speed.db, tableId, indexName, keyExpression, keyLength) ? GMB_DB_OK : GMB_DB_ERR;
}

int gmb_db_speed_drop_index(int64_t h, int tableId, const char* indexName)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    return Database_DropIndex(hh->u.speed.db, tableId, indexName) ? GMB_DB_OK : GMB_DB_ERR;
}

int gmb_db_speed_set_order(int64_t h, int tableId, const char* indexName)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    return Database_SetOrder(hh->u.speed.db, tableId, indexName) ? GMB_DB_OK : GMB_DB_ERR;
}

int gmb_db_speed_go_top(int64_t h, int tableId)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
        gmb_db_speed_sync_relations(h);
    if (!Database_GoTop(hh->u.speed.db, tableId)) return GMB_DB_ERR;
    speed_filter_advance(h, tableId, 1);
    return GMB_DB_OK;
}

int gmb_db_speed_go_bottom(int64_t h, int tableId)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
        gmb_db_speed_sync_relations(h);
    if (!Database_GoBottom(hh->u.speed.db, tableId)) return GMB_DB_ERR;
    speed_filter_advance(h, tableId, -1);
    return GMB_DB_OK;
}

int gmb_db_speed_skip(int64_t h, int tableId, int recordCount)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
        gmb_db_speed_sync_relations(h);
    if (!Database_Skip(hh->u.speed.db, tableId, recordCount)) return GMB_DB_ERR;
    speed_filter_advance(h, tableId, recordCount >= 0 ? 1 : -1);
    return GMB_DB_OK;
}

int gmb_db_speed_seek(int64_t h, int tableId, const void* key, int keyLen, const char* indexName)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db || !key) return GMB_DB_ERR;
    return Database_Seek(hh->u.speed.db, tableId, (const uint8_t*)key, keyLen, indexName) ? GMB_DB_OK : GMB_DB_ERR;
}

int gmb_db_speed_get_current(int64_t h, int tableId, void* val, int* vlen)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    uint8_t* got = NULL;
    int glen = 0;
    if (!hh || !hh->u.speed.db || !vlen) return GMB_DB_ERR;
    if (!Database_GetCurrent(hh->u.speed.db, tableId, &got, &glen)) return GMB_DB_ERR;
    if (val && *vlen >= glen && glen > 0)
        memcpy(val, got, (size_t)glen);
    *vlen = glen;
    free(got);
    return GMB_DB_OK;
}

int gmb_db_speed_eof(int64_t h, int tableId)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return 1;
    return Database_Eof(hh->u.speed.db, tableId) ? 1 : 0;
}

int gmb_db_speed_validate(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    char msg[512] = "";
    int rc;
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    rc = Database_ValidateIntegrity(hh->u.speed.db, msg, (int)sizeof(msg));
    if (out && outCap > 0) {
        char safe[512];
        int i, n = 0;
        for (i = 0; msg[i] && n < (int)sizeof(safe) - 2; i++)
            safe[n++] = msg[i] == '"' ? '\\' : msg[i];
        safe[n] = 0;
        snprintf(out, outCap, "{\"valid\":%s,\"message\":\"%s\"}",
            rc ? "true" : "false", safe);
    }
    return rc ? GMB_DB_OK : GMB_DB_ERR;
}

/* SPEED.SETMANY(alias, list): bulk set da LIST OF UDT boxate. PK = primo campo del
 * layout UDT (INDEXED BY <pk>, ...), valore = JSON dell'intero record, chiave = sortable
 * double (parità SpeedSortableDoubleBytes). */
int gmb_db_speed_set_many(int64_t h, gmb_list_t list, const char* alias)
{
    if (h < 0) return 0;
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return 0;
    int tableId = alias && alias[0] ? gmb_db_speed_table_by_alias(alias) : 0;
    if (tableId < 0) tableId = 0;
    int ok = 0;
    for (size_t i = 0; i < list.count; i++) {
        gmb_val_t v = gmb_list_get(list, (gmb_int_t)i);
        if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) continue;
        gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
        const char* tn = o->type_name;
        if (!tn || !*tn) continue;
        const char* udt = strstr(tn, "gmb_udt_");
        udt = udt ? udt + strlen("gmb_udt_") : tn;
        const uint8_t* base = (const uint8_t*)o->data;
        /* Chiave = primo campo layout (PK), valore = JSON dell'INTERO record (dict). */
        const char* pkName = NULL; long pkOff = 0; int pkType = 0;
        char pkVal[1024]; pkVal[0] = 0;
        if (gmb_type_udt_first_field(udt, &pkName, &pkOff, &pkType))
            gmb_type_udt_read_field(base, pkOff, pkType, pkVal, sizeof(pkVal));
        gmb_dict_t* d = gmb_type_udt_to_dict(udt, base);
        gmb_string_t json = d ? gmb_dict_to_json(d) : gmb_string_alloc("");
        if (d) { free(d); }
        uint8_t keybuf[8];
        gmb_speed_sortable_double_bytes(atof(pkVal), keybuf);
        int rc = gmb_db_speed_set(h, tableId, keybuf, 8, json.data ? json.data : "", (int)json.length);
        if (rc == 0) ok++;
    }
    return ok;
}

/* ── SPEED SET FILTER: applicazione reale (parità IL/PCode) ──
 * Memoria: RuntimeExpressionEvaluator_Eval(expr, speed_filter_get, ctx) valuta
 * l'espressione contro i campi del record corrente SPEED (via gmb_expr_eval_filter).
 * Il filtro viene applicato navigando: GO/SKIP avanza finché il record corrente
 * soddisfa il predicato o si raggiunge EOF/BOF (stesso pattern di PCode SpeedApplyFilter). */

typedef struct { int64_t h; int tableId; } speed_filter_ctx_t;

static int speed_filter_get(void* ctx, const char* name, char* out, int cap)
{
    /* Legge il campo per nome dal record corrente SPEED.
     * La PUT SPEED (VM/C e C#) serializza il valore come JSON (gmb_db_speed_set con
     * gmb_dict_to_json); la GET lo ri-parsa con gmb_json_parse_to_dict. Quindi qui si
     * legge il campo DAL JSON — NON con SpeedMarshaller_ExtractField, che assume il
     * formato binario UDT e non riconosce il JSON scritto dalla VM/C (era la causa del
     * filtro che non discriminava mai: ExtractField falliva → eval falliva → avanza a EOF). */
    speed_filter_ctx_t* fc = (speed_filter_ctx_t*)ctx;
    gmb_handle_t* hh = fc->h > 0 ? gmb_db_get(fc->h, GMB_H_SPEED) : NULL;
    uint8_t* raw = NULL; int rawLen = 0;
    if (!hh || !hh->u.speed.db || !name || !out || cap <= 0) return GMB_DB_ERR;
    out[0] = 0;
    if (!Database_GetCurrent(hh->u.speed.db, fc->tableId, &raw, &rawLen) || !raw || rawLen <= 0)
        return GMB_DB_ERR;
    {
        char jsbuf[16384];
        int n = rawLen < (int)sizeof(jsbuf) - 1 ? rawLen : (int)sizeof(jsbuf) - 1;
        memcpy(jsbuf, raw, (size_t)n); jsbuf[n] = 0;
        gmb_dict_t d = gmb_dict_new();
        gmb_json_parse_to_dict(jsbuf, &d);
        gmb_val_t v = gmb_dict_get(d, name);
        int r = GMB_DB_ERR;
        if (v.kind == GMB_TYPE_STRING && v.v_string.data) {
            int len = v.v_string.length < cap - 1 ? v.v_string.length : cap - 1;
            memcpy(out, v.v_string.data, (size_t)len);
            out[len] = 0;
            r = GMB_DB_OK;
        } else if (v.kind == GMB_TYPE_INT || v.kind == GMB_TYPE_DOUBLE) {
            snprintf(out, (size_t)cap, "%g", gmb_val_to_double(v));
            r = GMB_DB_OK;
        }
        gmb_dict_free(&d);
        free(raw);
        return r;
    }
}

static int speed_filter_pass(int64_t h, int tableId)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db || !hh->u.speed.filter[0]) return 1; /* nessun filtro → passa */
    speed_filter_ctx_t fc = { h, tableId };
    /* RuntimeExpressionEvaluator_Eval ritorna già un BOOLEANO di verità (0/1, vedi
     * rt_val_truth), non un codice d'errore stile GMB_DB_OK — confrontarlo con GMB_DB_OK
     * (0) invertiva pass/reject (parità con engine_evaluate_filter ISAM/DBF, che
     * ritornano il valore direttamente). */
    return RuntimeExpressionEvaluator_Eval(hh->u.speed.filter, speed_filter_get, &fc);
}

static void speed_filter_advance(int64_t h, int tableId, int direction)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db || !hh->u.speed.filter[0]) return;
    Database* db = hh->u.speed.db;
    for (int guard = 0; guard < 1000000; guard++) {
        int atEnd = direction > 0 ? Database_Eof(db, tableId) : Database_Bof(db, tableId);
        if (atEnd) return;
        if (speed_filter_pass(h, tableId)) return;
        if (!Database_Skip(db, tableId, direction)) return;
    }
}

/* SPEED SET FILTER <condition>: memorizza il predicato e lo applica alla navigazione. */
int gmb_db_speed_set_filter(int64_t h, const char* filterExpr)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_SPEED);
    if (!hh || !hh->u.speed.db) return GMB_DB_ERR;
    if (!filterExpr) filterExpr = "";
    snprintf(hh->u.speed.filter, sizeof(hh->u.speed.filter), "%s", filterExpr);
    return GMB_DB_OK;
}
