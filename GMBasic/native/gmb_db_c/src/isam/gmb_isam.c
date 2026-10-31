/* FILE: gmb_isam.c — ABI gmb_db_isam_* delega a Engine (port 1:1) */
#include "gmb_db_internal.h"
#include "gmb_runtime.h"

static int isam_is_text(const IsamFieldSchema* f)
{
    return f && (strcmp(f->DataTypeCode, "StringType") == 0 ||
                 strcmp(f->DataTypeCode, "JsonType") == 0);
}

/* Testo -> payload i64 di un campo NON testuale: DOUBLE/SINGLE portano i bit IEEE (vedi IsamField_*),
 * gli altri numerici l'intero. */
static int64_t isam_num_from_text(const IsamFieldSchema* f, const char* v)
{
    if (IsamField_IsFloat(f)) return IsamField_DoubleToBits(f, v ? atof(v) : 0.0);
    return v ? atoll(v) : 0;
}

static int isam_parse_schema(const char* defs, gmb_isam_t* s)
{
    char parts[GMB_DB_MAX_FIELDS][256];
    int n, i;
    memset(s, 0, sizeof(*s));
    /* Prefisso opzionale "<TypeName>|" (parità C#: il TypeName dello schema è il nome del
     * TYPE, non il placeholder "Rec"). Senza prefisso resta "Rec" (legacy). */
    {
        const char* bar1 = defs ? strchr(defs, '|') : NULL;
        if (bar1 && bar1 != defs && !memchr(defs, ':', (size_t)(bar1 - defs))) {
            const char* bar2 = strchr(bar1 + 1, '|');
            int tl = (int)(bar1 - defs);
            if (tl >= (int)sizeof(s->schema.TypeName)) tl = (int)sizeof(s->schema.TypeName) - 1;
            memcpy(s->schema.TypeName, defs, (size_t)tl);
            s->schema.TypeName[tl] = 0;
            if (bar2) {
                int il = (int)(bar2 - (bar1 + 1));
                if (il >= (int)sizeof(s->idx_field)) il = (int)sizeof(s->idx_field) - 1;
                memcpy(s->idx_field, bar1 + 1, (size_t)il);
                s->idx_field[il] = 0;
                defs = bar2 + 1;
            } else {
                defs = bar1 + 1;
            }
        } else {
            snprintf(s->schema.TypeName, sizeof(s->schema.TypeName), "%s", "Rec");
        }
    }
    n = gmb_db_split_csv(defs, parts, GMB_DB_MAX_FIELDS);
    if (n <= 0) return GMB_DB_ERR;
    for (i = 0; i < n; i++) {
        /* name deve contenere l'INTERO nome campo: con name[12]/ni<10 un nome >10 char
         * ("UltimoOrdine", "Descrizione") veniva troncato lasciando `p` sull'11° carattere,
         * che il parser leggeva come TYPE-CODE ('n'->N, 'e'->E) e il campo collassava a
         * LongType/len=10 -> schema e RECSIZE divergenti dal C#. */
        char name[64];
        char type = 'C';
        int len = 10, dec = 0, ni = 0;
        const char* p = parts[i];
        memset(name, 0, sizeof(name));
        while (*p && *p != ':' && ni < (int)sizeof(name) - 1)
            name[ni++] = *p++;   /* nome campo: conserva il case (parità schema C#) */
        if (*p == ':') p++;
        if (*p) type = (char)toupper((unsigned char)*p++);
        if (*p == ':') p++;
        if (*p) len = atoi(p);
        while (*p && *p != ':') p++;
        if (*p == ':') dec = atoi(p + 1);
        if (len <= 0) len = 10;
        snprintf(s->fields[i].Name, sizeof(s->fields[i].Name), "%s", name);
        if (type == 'C' || type == 'M') {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "StringType");
            s->fields[i].Length = len;
        } else if (type == 'F') {
            /* 'F' = DOUBLE (VM/C e C99 lo emettono per DOUBLE): 8 byte IEEE come il C# (prima collassava su
             * LongType e il valore veniva troncato a intero). 'S' = SINGLE (float 4 byte). */
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "DoubleType");
            (void)dec;
        } else if (type == 'S') {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "SingleType");
            (void)dec;
        } else if (type == 'N' || type == 'I' || type == 'B') {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "LongType");
            (void)dec;
        } else if (type == 'L') {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "BitType");
        } else if (type == 'D') {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "DateType");
        } else if (type == 'Y') {
            /* CurrencyType: sizeof(decimal)=16 nello schema C# (prima il C emitter non emetteva
             * CURRENCY -> lo schema C collassava a LongType e RECSIZE divergeva). */
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "CurrencyType");
        } else if (type == 'J') {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "JsonType");
        } else {
            snprintf(s->fields[i].DataTypeCode, sizeof(s->fields[i].DataTypeCode), "%s", "StringType");
            s->fields[i].Length = len;
        }
    }
    s->schema.Fields = s->fields;
    s->schema.FieldCount = n;
    s->schema.SecondaryKeys = s->secs;
    /* Primary key e indice secondario dall'INDEXED BY (parità engine C#: con INDEXED BY il PK
     * E il secondary sono il campo indicizzato; senza INDEXED BY, PK vuoto e 0 secondary). */
    if (s->idx_field[0]) {
        /* idx_field può contenere PIÙ campi comma-separated (`INDEXED BY ID, Nome, Citta`):
         * il primo è la PRIMARY KEY, TUTTI sono indici secondari (Engine_SetOrder/SEEK USING
         * cercano per nome in SecondaryKeys → senza, gli indici oltre il primo mancavano). */
        char idxbuf[512];
        char* tok; char* save = NULL;
        int ni = 0, first = 1;
        snprintf(idxbuf, sizeof idxbuf, "%s", s->idx_field);
        for (tok = strtok_r(idxbuf, ", ", &save); tok && ni < GMB_DB_MAX_INDEXES;
             tok = strtok_r(NULL, ", ", &save)) {
            int j, kl = 20;
            for (j = 0; j < n; j++)
                if (strcasecmp(s->fields[j].Name, tok) == 0) break;
            if (j < n && isam_is_text(&s->fields[j]))
                kl = s->fields[j].Length > 0 ? s->fields[j].Length : 20;
            else
                kl = 10;
            if (first) {
                snprintf(s->schema.PrimaryKey.Name, sizeof(s->schema.PrimaryKey.Name), "%s", tok);
                snprintf(s->schema.PrimaryKey.KeyExpression, sizeof(s->schema.PrimaryKey.KeyExpression), "%s", tok);
                s->schema.PrimaryKey.KeyLength = kl;
                s->schema.PrimaryKey.IsUnique = 1;
                first = 0;
            }
            snprintf(s->secs[ni].Name, sizeof(s->secs[ni].Name), "%s", tok);
            snprintf(s->secs[ni].KeyExpression, sizeof(s->secs[ni].KeyExpression), "%s", tok);
            s->secs[ni].KeyLength = kl;
            s->secs[ni].IsUnique = 0;
            s->secs[ni].IsUnsafe = 0;
            ni++;
        }
        s->schema.SecondaryKeyCount = ni;
    } else {
        s->schema.PrimaryKey.Name[0] = 0;
        s->schema.PrimaryKey.KeyExpression[0] = 0;
        s->schema.PrimaryKey.KeyLength = 20;
        s->schema.PrimaryKey.IsUnique = 1;
        s->schema.SecondaryKeyCount = 0;
    }
    return GMB_DB_OK;
}

static int isam_load_schema(gmb_isam_t* s, const char* path)
{
    FILE* f;
    IsamFileHeader h;
    uint8_t* buf;
    int rc;
    f = fopen(path, "rb");
    if (!f) return GMB_DB_ERR;
    if (IsamFileHeader_Read(f, &h) != GMB_DB_OK) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (h.SchemaLength <= 0 || h.SchemaLength > 1 << 20) {
        fclose(f);
        return GMB_DB_ERR;
    }
    buf = (uint8_t*)malloc((size_t)h.SchemaLength);
    if (!buf) {
        fclose(f);
        return GMB_DB_ERR;
    }
    if (fseek(f, (long)h.SchemaOffset, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)h.SchemaLength, f) != (size_t)h.SchemaLength) {
        free(buf);
        fclose(f);
        return GMB_DB_ERR;
    }
    fclose(f);
    rc = SchemaManager_DeserializeSchema(buf, (int)h.SchemaLength, &s->schema,
                                         s->fields, GMB_DB_MAX_FIELDS,
                                         s->secs, GMB_DB_MAX_INDEXES);
    free(buf);
    return rc;
}

int gmb_db_isam_create(const char* path, const char* schema)
{
    gmb_isam_t tmp;
    if (!path || isam_parse_schema(schema, &tmp) != GMB_DB_OK) {
        gmb_db_set_err("invalid isam schema");
        return GMB_DB_ERR;
    }
    if (IsamFileHandle_CreateNew(path, &tmp.schema) != GMB_DB_OK) {
        gmb_db_set_err("cannot create isam");
        return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

void gmb_db_install_isam_rdd_hook(void);

int64_t gmb_db_isam_open(const char* path)
{
    int64_t h;
    gmb_handle_t* hh;
    gmb_db_install_isam_rdd_hook(); /* espone il motore ISAM al row-buffer RDD condiviso */
    if (!path) {
        gmb_db_set_err("null path");
        return -1;
    }
    h = gmb_db_alloc(GMB_H_ISAM);
    if (h < 0) return -1;
    hh = &g_handles[h];
    strncpy(hh->u.isam.path, path, sizeof(hh->u.isam.path) - 1);
    if (isam_load_schema(&hh->u.isam, path) != GMB_DB_OK) {
        hh->kind = GMB_H_FREE;
        gmb_db_set_err("cannot read isam schema");
        return -1;
    }
    hh->u.isam.engine = Engine_Open(path, &hh->u.isam.schema);
    if (!hh->u.isam.engine) {
        hh->kind = GMB_H_FREE;
        gmb_db_set_err("cannot open isam");
        return -1;
    }
    Engine_Go(hh->u.isam.engine, CursorMoveOperation_Top);
    /* L'area APERTA diventa quella CORRENTE (stesso fix del ramo DBF): senza,
     * gmb_db_current_isam() restava sull'handle precedente/chiuso e ogni funzione ISAM
     * (INTEGRITY, SETREPLICATION, …) lavorava su un'area inesistente
     * (§ MiniTests/dbf/isam_replication.bas: "setrepl: no", "integrity:" vuoto). */
    gmb_db_set_current_isam(h);
    return h;
}

int gmb_db_isam_close(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh) return GMB_DB_ERR;
    if (hh->u.isam.engine) {
        Engine_Dispose(hh->u.isam.engine);
        hh->u.isam.engine = NULL;
    }
    hh->kind = GMB_H_FREE;
    return GMB_DB_OK;
}

/* Registra l'alias dell'area ISAM h (per ISAM SELECT / risoluzione per nome). */
void gmb_db_isam_set_alias(int64_t h, const char* alias)
{
    gmb_handle_t* hh = h > 0 ? gmb_db_get(h, GMB_H_ISAM) : NULL;
    if (!hh) return;
    if (!alias || !alias[0]) { hh->u.isam.alias[0] = 0; return; }
    snprintf(hh->u.isam.alias, sizeof(hh->u.isam.alias), "%s", alias);
}

/* ISAM SELECT <alias>: rende corrente l'area ISAM registrata con quell'alias.
 * Ritorna l'handle selezionato (>0), 0 se l'alias non è aperto. */
/* Come gmb_db_isam_select ma READ-ONLY (non cambia l'area corrente): handle dell'alias
 * ISAM, o 0. Usato da RECSIZE(alias)/funzioni che devono solo risolvere l'area. */
int64_t gmb_db_isam_handle_for(const char* alias)
{
    if (!alias || !alias[0]) return 0;
    for (int i = 1; i < GMB_DB_MAX_HANDLES; i++) {
        if (g_handles[i].kind != GMB_H_ISAM) continue;
        if (strcasecmp(g_handles[i].u.isam.alias, alias) == 0) return i;
    }
    return 0;
}

int64_t gmb_db_isam_select(const char* alias)
{
    if (!alias || !alias[0]) return 0;
    for (int i = 1; i < GMB_DB_MAX_HANDLES; i++) {
        if (g_handles[i].kind != GMB_H_ISAM) continue;
        if (strcasecmp(g_handles[i].u.isam.alias, alias) == 0) {
            gmb_db_set_current_isam(i);
            return i;
        }
    }
    return 0;
}

/* Come gmb_db_isam_select ma READ-ONLY (nessun side effect sull'area corrente):
 * numero d'area 1-based dell'alias ISAM, o 0. Usato da ISAM.SELECTED(alias). */
int64_t gmb_db_isam_selected(const char* alias)
{
    if (!alias || !alias[0]) return 0;
    for (int i = 1; i < GMB_DB_MAX_HANDLES; i++) {
        if (g_handles[i].kind != GMB_H_ISAM) continue;
        if (strcasecmp(g_handles[i].u.isam.alias, alias) == 0) return i;
    }
    return 0;
}

int gmb_db_isam_insert(int64_t h, const char* values_csv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    IsamFieldValue vals[GMB_DB_MAX_FIELDS];
    char parts[GMB_DB_MAX_FIELDS][256];
    int n, i;
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    n = values_csv ? gmb_db_split_csv(values_csv, parts, GMB_DB_MAX_FIELDS) : 0;
    memset(vals, 0, sizeof(vals));
    for (i = 0; i < hh->u.isam.schema.FieldCount; i++) {
        const char* v = i < n ? parts[i] : "";
        if (isam_is_text(&hh->u.isam.fields[i]))
            vals[i].text = v;
        else if (strcmp(hh->u.isam.fields[i].DataTypeCode, "CurrencyType") == 0)
            vals[i].i64 = (int64_t)llround(atof(v) * 10000.0);   /* CURRENCY: i64 scalato ×10^4 */
        else
            vals[i].i64 = isam_num_from_text(&hh->u.isam.fields[i], v);
    }
    if (Engine_Append(hh->u.isam.engine, vals, hh->u.isam.schema.FieldCount) != GMB_DB_OK) {
        gmb_db_set_err("isam insert failed");
        return GMB_DB_ERR;
    }
    return GMB_DB_OK;
}

int gmb_db_isam_go(int64_t h, int recno)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    if (Engine_PositionAtRecord(hh->u.isam.engine, recno) != GMB_DB_OK)
        return GMB_DB_ERR;
        gmb_db_isam_sync_relations(h);
return GMB_DB_OK;
}

int gmb_db_isam_skip(int64_t h, int n)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    int rc;
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    rc = Engine_Skip(hh->u.isam.engine, n);
    /* Sync DOPO il movimento del padre (prima si sincronizzava il figlio sulla posizione
     * VECCHIA del padre → relazione errata per tutti i clienti tranne il primo). */
    gmb_db_isam_sync_relations(h);
    return rc;
}

int gmb_db_isam_seek(int64_t h, const char* key)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine || !key) return GMB_DB_ERR;
    /* Scegli text/i64 dal TIPO DELLA CHIAVE, non dal campo[0]: con un indice secondario
     * testuale (es. `SEEK "Laura Neri" USING "Nome"` su una tabella con PK numerica) il
     * vecchio `atoll("Laura Neri")=0` cercava la chiave sbagliata → mai trovato. */
    {
        const char* p = key;
        int numeric = *p ? 1 : 0;
        if (*p == '+' || *p == '-') p++;
        for (; *p; p++) if (*p < '0' || *p > '9') { numeric = 0; break; }
        if (numeric)
        {
            /* Una chiave "tutta cifre" può essere una PK/secondaria STRINGA (es. "002" su
             * `ID AS STRING * 10`): prova PRIMA il seek TEXT; se non trovato, ripiega su i64. */
            if (Engine_SeekText(hh->u.isam.engine, key) == GMB_DB_OK && Engine_WasFound(hh->u.isam.engine))
                return GMB_DB_OK;
            return Engine_SeekI64(hh->u.isam.engine, atoll(key));
        }
    }
    return Engine_SeekText(hh->u.isam.engine, key);
}

int gmb_db_isam_eof(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    return hh && hh->u.isam.engine ? Engine_IsEof(hh->u.isam.engine) : 1;
}

int gmb_db_isam_count(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    return hh && hh->u.isam.engine ? (int)Engine_GetTotalRecordCount(hh->u.isam.engine) : 0;
}

int gmb_db_isam_recsize(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return 0;
    const IsamSchema* sc = Engine_GetSchema(hh->u.isam.engine);
    return sc ? SchemaManager_CalculateRecordSize(sc) : 0;
}

int gmb_db_isam_found(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    return hh && hh->u.isam.engine ? Engine_WasFound(hh->u.isam.engine) : 0;
}

int gmb_db_isam_flock(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    /* Lock del FILE in-process (Engine_LockFile: stato interno, come il lock manager C#/IL).
     * Prima usava flock(2) su una SECONDA apertura del file → EWOULDBLOCK (l'engine tiene già
     * il file aperto) → FLOCK sempre fallito su ISAM (§5 / protocollo §4.1). */
    return Engine_LockFileInProcess(hh->u.isam.engine);
}

int gmb_db_isam_funlock(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    return hh && hh->u.isam.path[0] ? gmb_db_lock_file(hh->u.isam.path, 0) : 0;
}

/* ── ISAM SET RELATION: registry figlio->padre+chiave, sync su navigazione del padre.
 * Il seek del figlio usa la chiave primaria (Engine_SeekText/I64). ---- */
#define GMB_ISAM_MAX_RELATIONS 16
static struct { int64_t child; int64_t parent; char key[128]; } g_isam_relations[GMB_ISAM_MAX_RELATIONS];
static int g_isam_relations_n = 0;

int gmb_db_isam_set_relation(int64_t child, int64_t parent, const char* keyField)
{
    int i;
    if (child <= 0 || parent <= 0) return GMB_DB_ERR;
    for (i = 0; i < g_isam_relations_n; i++) {
        if (g_isam_relations[i].child == child) {
            g_isam_relations[i].parent = parent;
            snprintf(g_isam_relations[i].key, sizeof(g_isam_relations[i].key), "%s", keyField ? keyField : "");
            return GMB_DB_OK;
        }
    }
    if (g_isam_relations_n >= GMB_ISAM_MAX_RELATIONS) return GMB_DB_ERR;
    g_isam_relations[g_isam_relations_n].child = child;
    g_isam_relations[g_isam_relations_n].parent = parent;
    snprintf(g_isam_relations[g_isam_relations_n].key, sizeof(g_isam_relations[g_isam_relations_n].key), "%s", keyField ? keyField : "");
    g_isam_relations_n++;
    return GMB_DB_OK;
}

int gmb_db_isam_clear_relation(int64_t child)
{
    int i;
    for (i = 0; i < g_isam_relations_n; i++) {
        if (g_isam_relations[i].child == child) {
            if (i < g_isam_relations_n - 1)
                g_isam_relations[i] = g_isam_relations[g_isam_relations_n - 1];
            g_isam_relations_n--;
            return GMB_DB_OK;
        }
    }
    return GMB_DB_OK;
}

static int isam_field_index_from_fields(const char* fieldsCsv, const char* name)
{
    const char* p = fieldsCsv ? fieldsCsv : "";
    int idx = 0;
    while (*p) {
        char fname[64]; int fi = 0;
        while (*p && *p != ',' && fi < 63) fname[fi++] = *p++;
        fname[fi] = 0;
        if (*p == ',') p++;
        if (strcasecmp(fname, name) == 0) return idx;
        idx++;
    }
    return -1;
}

int gmb_db_isam_sync_relations(int64_t parent)
{
    int i, rc = GMB_DB_OK;
    for (i = 0; i < g_isam_relations_n; i++) {
        if (g_isam_relations[i].parent == parent) {
            gmb_handle_t* ph = gmb_db_get(parent, GMB_H_ISAM);
            gmb_handle_t* ch = gmb_db_get(g_isam_relations[i].child, GMB_H_ISAM);
            char pfields[1024], cfields[1024], keytext[512], ctext[512], cand[160];
            const char* pkey = g_isam_relations[i].key;
            const char* alias;
            int fidx, cidx = -1, l;
            if (!ph || !ch || !ph->u.isam.engine || !ch->u.isam.engine) continue;
            /* Valore del campo chiave del PADRE. */
            gmb_db_isam_fields(parent, pfields, (int)sizeof(pfields));
            fidx = isam_field_index_from_fields(pfields, pkey);
            if (fidx < 0) { rc = GMB_DB_ERR; continue; }
            keytext[0] = 0;
            gmb_db_isam_read(parent, fidx, keytext, (int)sizeof(keytext));
            /* Campo del FIGLIO: la sintassi xBase non lo specifica — stessa euristica dell'IL
             * (IsamDriver.ResolveRelationChildField): prima il nome chiave stesso ("ID"), poi
             * "ID_" + alias del padre ("ID_Clienti"), poi la singolarizzazione italiana
             * ("ID_Cliente")/inglese ("ID_Client"). Prima si faceva un seek del FIGLIO sulla sua
             * PK col valore del padre → mai trovato (divergenza §8 c99n vs IL). */
            gmb_db_isam_fields(g_isam_relations[i].child, cfields, (int)sizeof(cfields));
            alias = ph->u.isam.alias;
            cidx = isam_field_index_from_fields(cfields, pkey);
            if (cidx < 0 && alias && alias[0]) {
                snprintf(cand, sizeof(cand), "ID_%s", alias);
                cidx = isam_field_index_from_fields(cfields, cand);
                l = (int)strlen(alias);
                if (cidx < 0 && l > 1 && (alias[l - 1] == 'i' || alias[l - 1] == 'I')) {
                    snprintf(cand, sizeof(cand), "ID_%.*se", l - 1, alias);
                    cidx = isam_field_index_from_fields(cfields, cand);
                }
                if (cidx < 0 && l > 1 && (alias[l - 1] == 's' || alias[l - 1] == 'S')) {
                    snprintf(cand, sizeof(cand), "ID_%.*s", l - 1, alias);
                    cidx = isam_field_index_from_fields(cfields, cand);
                }
            }
            if (cidx < 0) { Engine_SetFoundFlag(ch->u.isam.engine, 0); continue; }
            /* Scan lineare dal TOP: primo record del figlio col campo == valore chiave. */
            Engine_SetFoundFlag(ch->u.isam.engine, 0);
            Engine_Go(ch->u.isam.engine, CursorMoveOperation_Top);
            while (!Engine_IsEof(ch->u.isam.engine)) {
                int match;
                ctext[0] = 0;
                match = gmb_db_isam_read(g_isam_relations[i].child, cidx, ctext, (int)sizeof(ctext)) == GMB_DB_OK
                        && strcmp(ctext, keytext) == 0;
                if (match) { Engine_SetFoundFlag(ch->u.isam.engine, 1); rc = GMB_DB_OK; break; }
                Engine_Skip(ch->u.isam.engine, 1);
            }
        }
    }
    return rc;
}

int gmb_db_isam_read(int64_t h, int field_idx, char* buf, int cap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    IsamFieldValue vals[GMB_DB_MAX_FIELDS];
    char text[GMB_DB_MAX_FIELDS][256];
    RecordMetadata meta;
    int i, n;
    if (!hh || !hh->u.isam.engine || !buf || cap <= 0) return GMB_DB_ERR;
    n = hh->u.isam.schema.FieldCount;
    if (field_idx < 0 || field_idx >= n) return GMB_DB_ERR;
    memset(vals, 0, sizeof(vals));
    for (i = 0; i < n; i++) {
        if (!isam_is_text(&hh->u.isam.fields[i])) continue;
        vals[i].textOut = text[i];
        vals[i].textOutCap = (int)sizeof(text[i]);
        text[i][0] = 0;
    }
    if (Engine_GetCurrent(hh->u.isam.engine, vals, n, &meta) != GMB_DB_OK) return GMB_DB_ERR;
    if (isam_is_text(&hh->u.isam.fields[field_idx])) {
        const char* src = vals[field_idx].textOut ? vals[field_idx].textOut : "";
        strncpy(buf, src, (size_t)cap - 1);
        buf[cap - 1] = 0;
        gmb_db_trim_field(buf);
    } else if (strcmp(hh->u.isam.fields[field_idx].DataTypeCode, "CurrencyType") == 0) {
        /* CURRENCY: i64 scalato ×10^4 → testo decimale (trim zeri finali, parità IL/C#). */
        int len;
        snprintf(buf, (size_t)cap, "%.4f", (double)vals[field_idx].i64 / 10000.0);
        len = (int)strlen(buf);
        while (len > 0 && buf[len - 1] == '0') buf[--len] = 0;
        if (len > 0 && buf[len - 1] == '.') buf[--len] = 0;
    } else if (IsamField_IsFloat(&hh->u.isam.fields[field_idx])) {
        IsamField_FormatFloat(&hh->u.isam.fields[field_idx], vals[field_idx].i64, buf, cap);
    } else {
        snprintf(buf, (size_t)cap, "%lld", (long long)vals[field_idx].i64);
    }
    return GMB_DB_OK;
}

/* Tipo del campo ISAM i-esimo, nella stessa convenzione a singolo carattere di
 * gmb_db_dbf_field_type ('N' numerico, 'L' booleano, 'D' data, 'C' testo/default):
 * IsamFieldSchema.DataTypeCode è una stringa ("LongType"/"BitType"/"DateType"/
 * "StringType"/"JsonType"), non il carattere schema originale (N/F/I/B collassano
 * tutti su "LongType" — vedi isam_parse_schema) — qui si rimappa per permettere al
 * chiamante (gmb_syscall_fileio.c, GET_FILE) di scegliere gmb_make_val_double vs
 * gmb_make_val_string con lo stesso identico criterio già usato per i campi DBF
 * (vm_expr_dbf_record_get, gmb_vm_core.c: `t == 'N' || t == 'F'`). */
int gmb_db_isam_field_type(int64_t h, int idx)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    const char* dt;
    if (!hh || idx < 0 || idx >= hh->u.isam.schema.FieldCount) return 0;
    dt = hh->u.isam.fields[idx].DataTypeCode;
    if (strcmp(dt, "LongType") == 0 || strcmp(dt, "DoubleType") == 0 || strcmp(dt, "SingleType") == 0) return 'N';
    if (strcmp(dt, "BitType") == 0) return 'L';
    if (strcmp(dt, "DateType") == 0) return 'D';
    /* CURRENCY: codice 'Y' distinto da 'C' (stringa), così il GET la materializza come
     * NUMERO (non stringa) — la CURRENCY va resa con il padding Clipper come l'IL. */
    if (strcmp(dt, "CurrencyType") == 0) return 'Y';
    return 'C';
}

int gmb_db_isam_pack(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_Pack(hh->u.isam.engine);
}

int gmb_db_isam_reindex(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_Reindex(hh->u.isam.engine);
}

int gmb_db_isam_import_dbf(int64_t h, const char* dbf_path)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    DbfEngine* dbf;
    int n, i;
    if (!hh || !hh->u.isam.engine || !dbf_path) return GMB_DB_ERR;
    dbf = DbfEngine_New(dbf_path, NULL, 0, 1, LockingStrategy_IntraProcess);
    if (!dbf) return GMB_DB_ERR;
    n = hh->u.isam.schema.FieldCount;
    DbfEngine_GoTop(dbf);
    while (!DbfEngine_get_Eof(dbf)) {
        IsamFieldValue vals[GMB_DB_MAX_FIELDS];
        char parts[GMB_DB_MAX_FIELDS][256];
        if (!DbfEngine_IsCurrentRecordDeleted(dbf)) {
            memset(vals, 0, sizeof(vals));
            for (i = 0; i < n; i++) {
                parts[i][0] = 0;
                DbfEngine_GetFieldText(dbf, i, parts[i], (int)sizeof(parts[i]));
                gmb_db_trim_field(parts[i]);
                if (isam_is_text(&hh->u.isam.fields[i]))
                    vals[i].text = parts[i];
                else if (strcmp(hh->u.isam.fields[i].DataTypeCode, "CurrencyType") == 0)
                    vals[i].i64 = (int64_t)llround(atof(parts[i]) * 10000.0);
                else
                    vals[i].i64 = isam_num_from_text(&hh->u.isam.fields[i], parts[i]);
            }
            if (Engine_Append(hh->u.isam.engine, vals, n) != GMB_DB_OK) {
                DbfEngine_Dispose(dbf);
                return GMB_DB_ERR;
            }
        }
        DbfEngine_Skip(dbf, 1);
    }
    DbfEngine_Dispose(dbf);
    return GMB_DB_OK;
}


/* ISAM.INTEGRITY(alias) -> JSON {"valid":...,"message":...}, pattern C# IsamValidateJson.
 * Esegue: free list pagine, puntatori record (stato + LOB JSON) e coerenza conteggio
 * header vs indice primario. Ritorna GMB_DB_OK se valido, GMB_DB_ERR se corrotto. */
int gmb_db_isam_integrity(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    IsamFileHandle* fh;
    BPlusTreeManager* pk;
    char err[512] = "";
    int ok = 1;
    int rc;
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    fh = Engine_GetHandle(hh->u.isam.engine);
    pk = Engine_GetPrimaryIndex(hh->u.isam.engine);
    if (!fh || !pk) return GMB_DB_ERR;
    if (IsamIntegrityChecker_ValidatePageFreeList(fh, err, (int)sizeof(err)) != GMB_DB_OK)
        ok = 0;
    if (ok &&
        IsamIntegrityChecker_ValidateActiveRecordPointers(fh, pk, err, (int)sizeof(err)) != GMB_DB_OK)
        ok = 0;
    if (ok) {
        /* coerenza: conteggio indice primario vs header (port di Engine.ValidateIntegrityCore) */
        int64_t idxCount = 0;
        if (IsamIntegrityChecker_CountRecords(pk, &idxCount) == GMB_DB_OK &&
            idxCount != fh->Header.ActiveRecordCount) {
            snprintf(err, sizeof(err),
                "Errore di coerenza: l'header riporta %lld record attivi, ma l'indice primario ne contiene %lld.",
                (long long)fh->Header.ActiveRecordCount, (long long)idxCount);
            ok = 0;
        }
    }
    /* Messaggio del caso VALIDO: identico a Engine.ValidateIntegrityCore (modules/Database/
     * ISAM/Engine.cs): prima restava vuoto → il JSON differiva da IL/PCode
     * (§ MiniTests/dbf/isam_replication.bas: VMC {"valid":true,"message":""}). */
    if (ok) snprintf(err, sizeof(err), "Validazione completata con successo. Nessun errore trovato.");
    if (out && outCap > 0) {
        char safe[512];
        int i, n = 0;
        for (i = 0; err[i] && n < (int)sizeof(safe) - 2; i++)
            safe[n++] = err[i] == '"' ? '\\' : err[i];
        safe[n] = 0;
        snprintf(out, outCap, "{\"valid\":%s,\"message\":\"%s\"}",
            ok ? "true" : "false", safe);
    }
    rc = ok ? GMB_DB_OK : GMB_DB_ERR;
    return rc;
}


/* --- helper: CSV -> IsamFieldValue[] (riusato da insert/put/appendmany) --- */
static int isam_parse_values(gmb_handle_t* hh, const char* csv, IsamFieldValue* vals)
{
    char parts[GMB_DB_MAX_FIELDS][256];
    int n, i;
    if (!hh || !vals) return GMB_DB_ERR;
    n = csv ? gmb_db_split_csv(csv, parts, GMB_DB_MAX_FIELDS) : 0;
    memset(vals, 0, (size_t)GMB_DB_MAX_FIELDS * sizeof(*vals));
    for (i = 0; i < hh->u.isam.schema.FieldCount; i++) {
        const char* v = i < n ? parts[i] : "";
        if (isam_is_text(&hh->u.isam.fields[i]))
            vals[i].text = v;
        else if (strcmp(hh->u.isam.fields[i].DataTypeCode, "CurrencyType") == 0)
            vals[i].i64 = (int64_t)llround(atof(v) * 10000.0);   /* CURRENCY: i64 scalato ×10^4 */
        else
            vals[i].i64 = isam_num_from_text(&hh->u.isam.fields[i], v);
    }
    return GMB_DB_OK;
}

/* --- NAVIGAZIONE / STATO --- */
int gmb_db_isam_go_top(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    int rc;
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    rc = Engine_Go(hh->u.isam.engine, CursorMoveOperation_Top);
    gmb_db_isam_sync_relations(h);
    return rc;
}

int gmb_db_isam_go_bottom(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    int rc;
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    rc = Engine_Go(hh->u.isam.engine, CursorMoveOperation_Bottom);
    gmb_db_isam_sync_relations(h);
    return rc;
}

int gmb_db_isam_bof(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_IsBof(hh->u.isam.engine) ? 1 : 0;
}

int64_t gmb_db_isam_recno(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return -1;
    return Engine_GetCurrentRecordNumber(hh->u.isam.engine);
}

/* --- SCRITTURA --- */
int gmb_db_isam_put(int64_t h, const char* values_csv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    IsamFieldValue vals[GMB_DB_MAX_FIELDS];
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    if (isam_parse_values(hh, values_csv, vals) != GMB_DB_OK) return GMB_DB_ERR;
    return Engine_Put(hh->u.isam.engine, vals, hh->u.isam.schema.FieldCount);
}

int gmb_db_isam_delete(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_DeleteCurrent(hh->u.isam.engine);
}

int gmb_db_isam_recall(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_RecallCurrent(hh->u.isam.engine);
}

int gmb_db_isam_appendmany(int64_t h, const char* records_csv)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    IsamFieldValue* rows;
    char** texts = NULL;
    int ntexts = 0, textcap = 0;
    char tmp[GMB_DB_MAX_FIELDS][256];
    int nrec = 0, r, nf, cap, rc;
    const char* start;
    if (!hh || !hh->u.isam.engine || !records_csv) return GMB_DB_ERR;
    nf = hh->u.isam.schema.FieldCount;
    /* conta le righe per dimensionare il batch (stride = nf, contiguo) */
    cap = 1;
    { const char* q = records_csv; while (*q) { if (*q == '\n') cap++; q++; } }
    if (cap > 4000000) cap = 4000000;
    rows = (IsamFieldValue*)calloc((size_t)cap * (size_t)nf, sizeof(IsamFieldValue));
    if (!rows) return GMB_DB_ERR;
    start = records_csv;
    while (*start && nrec < cap) {
        const char* nl = strchr(start, '\n');
        char line[2048];
        size_t ln;
        if (!nl) nl = start + strlen(start);
        ln = (size_t)(nl - start);
        if (ln >= sizeof(line)) ln = sizeof(line) - 1;
        memcpy(line, start, ln); line[ln] = 0;
        gmb_db_split_csv(line, tmp, GMB_DB_MAX_FIELDS);
        for (r = 0; r < nf && r < GMB_DB_MAX_FIELDS; r++) {
            IsamFieldValue* fv = &rows[(size_t)nrec * (size_t)nf + (size_t)r];
            if (isam_is_text(&hh->u.isam.fields[r])) {
                char* cp = strdup(tmp[r]);
                if (ntexts >= textcap) { textcap = textcap ? textcap * 2 : 64; texts = (char**)realloc(texts, sizeof(char*) * (size_t)textcap); }
                texts[ntexts++] = cp;
                fv->text = cp;
            } else {
                fv->i64 = isam_num_from_text(&hh->u.isam.fields[r], tmp[r]);
            }
        }
        nrec++;
        if (!*nl) break;
        start = nl + 1;
    }
    rc = Engine_AppendMany(hh->u.isam.engine, rows, nrec, nf);
    for (r = 0; r < ntexts; r++) free(texts[r]);
    free(texts);
    free(rows);
    return rc;
}

int gmb_db_isam_zap(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_Zap(hh->u.isam.engine);
}

/* --- FILTRO / ORDINE / INDICI --- */
int gmb_db_isam_set_filter(int64_t h, const char* expression)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_SetFilter(hh->u.isam.engine, expression);
}

int gmb_db_isam_set_order(int64_t h, const char* indexName)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_SetOrder(hh->u.isam.engine, indexName);
}

int gmb_db_isam_create_index(int64_t h, const char* indexName, const char* keyExpression, int keyLength)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_CreateIndex(hh->u.isam.engine, indexName, keyExpression, keyLength);
}

int gmb_db_isam_drop_index(int64_t h, const char* indexName)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_DropIndex(hh->u.isam.engine, indexName);
}

/* --- LOCK / VALIDAZIONE / INFO --- */
int gmb_db_isam_lock(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_LockCurrentRecord(hh->u.isam.engine);
}

int gmb_db_isam_unlock(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    Engine_UnlockCurrentRecord(hh->u.isam.engine);
    return GMB_DB_OK;
}

int gmb_db_isam_validate(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return gmb_db_isam_integrity(h, NULL, 0);
}

int gmb_db_isam_info(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    IsamFileHeader hdr;
    long sz = 0;
    FILE* f;
    char fld[4096];
    int pos = 0, i;
    if (!hh || !hh->u.isam.engine || !out || outCap <= 0) return GMB_DB_ERR;
    memset(&hdr, 0, sizeof(hdr));
    f = fopen(hh->u.isam.path, "rb");
    if (f)
    {
        IsamFileHeader_Read(f, &hdr);
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fclose(f);
    }
    fld[0] = 0;
    for (i = 0; i < hh->u.isam.schema.FieldCount; i++)
    {
        if (pos >= (int)sizeof(fld) - 1) break;
        pos += snprintf(fld + pos, sizeof(fld) - (size_t)pos, "%s\"%s\"",
                        i ? "," : "", hh->u.isam.schema.Fields[i].Name);
    }
    /* JSON d'ispezione completo (parità IlRuntime.IsamInfo): header del file ISAM +
     * schema (TypeName/FieldCount/secondary). Prima ritornava solo active_records. */
    snprintf(out, outCap,
        "{\"file\":\"%s\",\"size_bytes\":%ld,\"format_version\":%d,\"type_name\":\"%s\","
        "\"record_size\":%d,\"active_records\":%lld,\"last_rec_number\":%lld,\"primary_key_root\":%lld,"
        "\"lob_heap_start\":%lld,\"next_available_page\":%lld,\"free_list_head\":%lld,"
        "\"page_free_list_head\":%lld,\"schema_offset\":%lld,\"schema_length\":%lld,"
        "\"secondary_indexes\":%d,\"durability_mode\":\"Full\",\"locking_strategy\":\"IntraProcess\","
        "\"field_names\":[%s]}",
        hh->u.isam.path, sz, hdr.Version, hh->u.isam.schema.TypeName,
        hdr.RecordSize, (long long)hdr.ActiveRecordCount, (long long)hdr.LastRecNumber,
        (long long)hdr.PrimaryKeyRootOffset, (long long)hdr.LobHeapStartOffset,
        (long long)hdr.NextAvailablePageOffset, (long long)hdr.FreeListHeadOffset,
        (long long)hdr.PageFreeListHeadOffset, (long long)hdr.SchemaOffset,
        (long long)hdr.SchemaLength, hh->u.isam.schema.SecondaryKeyCount, fld);
    return GMB_DB_OK;
}

/* Wrapper stringa (parità gmb_db_isam_integrity_str): gli emitter C99 usano gmb_string_alloc. */
gmb_string_t gmb_db_isam_info_str(int64_t h)
{
    char buf[8192];
    buf[0] = 0;
    gmb_db_isam_info(h, buf, sizeof(buf));
    return gmb_string_alloc(buf[0] ? buf : "{}");
}

/* --- BACKUP / REPAIR / COPY / SORT --- */
int gmb_db_isam_backup(int64_t h, const char* destDir)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !destDir) return GMB_DB_ERR;
    return IsamBackupRestore_BackupIncremental(hh->u.isam.path, destDir);
}

int gmb_db_isam_restore(const char* backupDir, const char* targetIsdb)
{
    return IsamBackupRestore_Restore(backupDir, targetIsdb);
}

int gmb_db_isam_repair(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh) return GMB_DB_ERR;
    return IsamRepair_PrepareForOpen(hh->u.isam.path);
}

int gmb_db_isam_copy_to(int64_t h, const char* destPath, const char* forCondition)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine || !destPath) return GMB_DB_ERR;
    return Engine_CopyTo(hh->u.isam.engine, destPath, forCondition);
}

int gmb_db_isam_sort_to(int64_t h, const char* destPath, const char* fieldName, int descending, const char* forCondition)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine || !destPath) return GMB_DB_ERR;
    return Engine_SortTo(hh->u.isam.engine, destPath, fieldName, descending, forCondition);
}

/* --- TRANSAZIONI --- */
int gmb_db_isam_begin_transaction(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_BeginTransaction(hh->u.isam.engine);
}

int gmb_db_isam_commit(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    return Engine_Commit(hh->u.isam.engine);
}

int gmb_db_isam_rollback(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    Engine_Rollback(hh->u.isam.engine);
    return GMB_DB_OK;
}

int gmb_db_isam_set_replication(int64_t h, const char* logPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine || !logPath) return GMB_DB_ERR;
    return Engine_SetReplication(hh->u.isam.engine, logPath);
}


/* --- FASE 3: ISAM esteso (join, replica) --- */
int gmb_db_isam_join_with(int64_t h1, int64_t h2, const char* destPath,
                          const char* leftField, const char* rightField)
{
    gmb_handle_t* a = gmb_db_get(h1, GMB_H_ISAM);
    gmb_handle_t* b = gmb_db_get(h2, GMB_H_ISAM);
    if (!a || !a->u.isam.engine || !b || !b->u.isam.engine || !destPath) return GMB_DB_ERR;
    return Engine_JoinWith(a->u.isam.engine, b->u.isam.engine, destPath, leftField, rightField);
}

int gmb_db_isam_apply_replication(int64_t h, const char* replLogPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine || !replLogPath) return GMB_DB_ERR;
    return IsamReplicationApplier_Apply(replLogPath, hh->u.isam.engine);
}

int gmb_db_isam_follow_replication(int64_t h, const char* replLogPath, const char* checkpointPath)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine || !replLogPath) return GMB_DB_ERR;
    return IsamReplicationApplier_Follow(replLogPath, hh->u.isam.engine, checkpointPath);
}


/* Nomi campo ISAM (schema) separati da virgola — per il marshalling dict->CSV nel VM/C. */
int gmb_db_isam_fields(int64_t h, char* out, int outCap)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    int i, pos = 0;
    if (!hh || !hh->u.isam.engine || !out || outCap <= 0) return GMB_DB_ERR;
    for (i = 0; i < hh->u.isam.schema.FieldCount; i++) {
        if (pos >= outCap - 1) break;
        pos += snprintf(out + pos, (size_t)(outCap - pos), "%s%s",
            i ? "," : "", hh->u.isam.schema.Fields[i].Name);
    }
    return GMB_DB_OK;
}


/* ISAM.APPENDMANY(list): bulk insert da LIST OF UDT boxate (stesso pattern DBF:
 * layout UDT da gmb_type_udt_*, nomi schema da gmb_db_isam_fields, CSV per riga). */
/* Come gmb_db_dbf_append_many_boxed: array di UDT boxati → inserimento multiplo. */
int gmb_db_isam_append_many_boxed(int64_t h, const gmb_val_t* vals, int n)
{
    gmb_list_t l = gmb_list_new();
    int i, rc = 0;
    if (!vals || n <= 0) return 0;
    for (i = 0; i < n; i++) gmb_list_add(&l, vals[i]);
    rc = (int)gmb_db_isam_append_many(h, l);
    gmb_list_free(&l);
    /* Il runtime ritorna uno STATO (GMB_DB_OK = 0): il builtin, come IlRuntime.*AppendMany,
     * ritorna il NUMERO di record scritti (l'oracolo dei MiniTest è "appendmany: 2"). */
    /* append_many ritorna il NUMERO di record scritti (o <= 0 in errore): è il valore che
     * l'oracolo espone (IlRuntime.*AppendMany → records.Count). */
    return rc > 0 ? rc : 0;
}

int gmb_db_isam_append_many(int64_t h, gmb_list_t list)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_ISAM);
    if (!hh || !hh->u.isam.engine) return GMB_DB_ERR;
    char fieldsBuf[4096];
    if (gmb_db_isam_fields(h, fieldsBuf, sizeof(fieldsBuf)) != GMB_DB_OK) return GMB_DB_ERR;
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
        const char* udt = strstr(tn, "gmb_udt_");
        udt = udt ? udt + strlen("gmb_udt_") : tn;
        const uint8_t* base = (const uint8_t*)o->data;
        char csv[4096]; int pos = 0; csv[0] = 0;
        for (int f = 0; f < nf; f++) {
            long off = 0; int t = 0;
            char val[1024];
            if (gmb_type_udt_lookup(udt, fnames[f], &off, &t))
                gmb_type_udt_read_field(base, off, t, val, sizeof(val));
            else val[0] = 0;
            if (f > 0) { if (pos < (int)sizeof(csv) - 1) csv[pos++] = ','; }
            int w = snprintf(csv + pos, sizeof(csv) - (size_t)pos, "%s", val);
            if (w > 0) pos += w;
            if (pos >= (int)sizeof(csv) - 1) break;
        }
        if (gmb_db_isam_insert(h, csv) == GMB_DB_OK) ok++;
    }
    return ok;
}

/* SETREPLICATION come espressione: path del log (StringType) — parity IlRuntime.IsamSetReplication. */
gmb_string_t gmb_db_isam_set_replication_str(int64_t h, const char* logPath)
{
    if (gmb_db_isam_set_replication(h, logPath) != GMB_DB_OK) return gmb_string_alloc("");
    return gmb_string_alloc(logPath ? logPath : "");
}

gmb_string_t gmb_db_isam_integrity_str(int64_t h)
{
    char buf[4096]; buf[0] = 0;
    gmb_db_isam_integrity(h, buf, sizeof(buf));
    return gmb_string_alloc(buf[0] ? buf : "");
}
