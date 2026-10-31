/* FILE: gmb_dbf_index.c — ABI index_on/set_order/seek/reindex → DbfEngine */
#include "gmb_db_internal.h"
#include "gmb_db_port.h"

static int field_index(gmb_dbf_t* d, const char* name)
{
    int i;
    for (i = 0; i < d->field_count; i++)
        if (strcasecmp(d->fields[i].name, name) == 0)
            return i;
    return -1;
}

int gmb_db_dbf_index_on(int64_t h, const char* field, const char* index_path, int unique)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int fi, keyLen;
    DbfEngine* e;
    char norm[512];
    const char* expr = field;
    if (!hh || !hh->u.dbf.engine || !field || !index_path) return GMB_DB_ERR;
    /* Normalizza l'espressione: spazi esterni e PARENTESI ESTERNE BILANCIATE.
     * L'AST del compilatore A1 (consumato dalla VM/C) serializza il campo come
     * "(Citta + Nome)"; il lookup dei campi qui sotto (token "(Citta") e il produttore
     * canonico di chiavi (eng_extract_field_key, split su '+') non gestivano le parentesi →
     * "unknown index field", indice COMPOSTO mai creato → SET ORDER/SEEK su quell'ordine
     * fallivano (ClipperDbfLateBinding gamba VM/C: secondo elenco in ordine naturale
     * invece che per Citta+Nome). L'emitter C99 passa il testo senza parentesi: con
     * questa normalizzazione le due gambe producono lo STESSO indice. */
    {
        size_t n = strlen(field), s = 0;
        while (n > 0 && (field[n - 1] == ' ' || field[n - 1] == '\t')) n--;
        while (s < n && (field[s] == ' ' || field[s] == '\t')) s++;
        for (;;) {
            int depth = 0, balanced = 1;
            size_t k;
            if (n - s < 2 || field[s] != '(' || field[n - 1] != ')') break;
            for (k = s; k < n; k++) {
                if (field[k] == '(') depth++;
                else if (field[k] == ')') { depth--; if (depth == 0 && k != n - 1) { balanced = 0; break; } }
            }
            if (!balanced || depth != 0) break;
            s++; n--;
            while (s < n && (field[s] == ' ' || field[s] == '\t')) s++;
            while (n > s && (field[n - 1] == ' ' || field[n - 1] == '\t')) n--;
        }
        if (n > s && n - s < sizeof(norm)) { memcpy(norm, field + s, n - s); norm[n - s] = 0; expr = norm; }
    }
    e = hh->u.dbf.engine;
    fi = field_index(&hh->u.dbf, expr);
    if (fi >= 0) {
        keyLen = hh->u.dbf.fields[fi].length;
    } else {
        /* ESPRESSIONE COMPOSTA (`Citta + Nome`): il produttore di chiavi canonico sa già
         * concatenare i campi (eng_extract_field_key splitta su '+'); qui serve solo la
         * lunghezza totale. Prima `field_index` falliva → "unknown index field" e l'indice
         * non veniva creato (SET ORDER su quel nome falliva → TOTAL/SEEK sbagliati). */
        const char* p = expr;
        keyLen = 0;
        while (*p) {
            const char* q = strchr(p, '+');
            size_t tl = q ? (size_t)(q - p) : strlen(p);
            char tok[64];
            int ti;
            while (tl > 0 && *p == ' ') { p++; tl--; }
            while (tl > 0 && (p[tl - 1] == ' ' || p[tl - 1] == '\t')) tl--;
            if (tl > 0 && tl < sizeof(tok)) {
                memcpy(tok, p, tl); tok[tl] = 0;
                for (ti = 0; ti < hh->u.dbf.field_count; ti++)
                    if (strcasecmp(hh->u.dbf.fields[ti].name, tok) == 0) { keyLen += hh->u.dbf.fields[ti].length; break; }
            }
            if (!q) break;
            p = q + 1;
        }
        if (keyLen <= 0) {
            gmb_db_set_err("unknown index field");
            return GMB_DB_ERR;
        }
    }
    if (DbfEngine_IndexOn(e, index_path, expr, keyLen, unique, NULL, NULL, NULL) != GMB_DB_OK)
        return GMB_DB_ERR;
    hh->u.dbf.index_count = DbfEngine_GetIndexCount(e);
    hh->u.dbf.active_order = hh->u.dbf.index_count;
    if (hh->u.dbf.index_count > 0 && hh->u.dbf.index_count <= GMB_DB_MAX_INDEXES) {
        strncpy(hh->u.dbf.index_path[hh->u.dbf.index_count - 1], index_path, 1023);
        hh->u.dbf.index_key_field[hh->u.dbf.index_count - 1] = fi;
        hh->u.dbf.index_key_len[hh->u.dbf.index_count - 1] = keyLen;
    }
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

int gmb_db_dbf_set_order(int64_t h, int order)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_SetActiveOrderNumber(hh->u.dbf.engine, order) != GMB_DB_OK) return GMB_DB_ERR;
    hh->u.dbf.active_order = order;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}

/* `DBF SET ORDER TO "<file>.ntx"` (o al nome senza estensione): risolve l'indice per NOME tra
 * quelli creati/aperti sull'area. Prima gli emitter passavano 0 (nessun ordine attivo) → TOTAL
 * ON (che richiede un ordine) falliva con file vuoto e SEEK cadeva sulla scansione lineare. */
int gmb_db_dbf_set_order_by_name(int64_t h, const char* name)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    int i;
    if (!hh || !hh->u.dbf.engine || !name || !*name) return GMB_DB_ERR;
    for (i = 0; i < hh->u.dbf.index_count; i++) {
        const char* p = hh->u.dbf.index_path[i];
        const char* b = p ? strrchr(p, '/') : NULL;
        const char* base = b ? b + 1 : p;
        char stem[1024];
        size_t sl;
        if (!base || !*base) continue;
        snprintf(stem, sizeof(stem), "%s", base);
        sl = strlen(stem);
        if (sl > 4 && strcasecmp(stem + sl - 4, ".ntx") == 0) stem[sl - 4] = 0;
        if (strcasecmp(name, base) == 0 || strcasecmp(name, stem) == 0) {
            if (DbfEngine_SetActiveOrderNumber(hh->u.dbf.engine, i + 1) != GMB_DB_OK) return GMB_DB_ERR;
            hh->u.dbf.active_order = i + 1;
            gmb_dbf_sync(hh);
            return GMB_DB_OK;
        }
    }
    gmb_db_set_err("index not found");
    return GMB_DB_NOTFOUND;
}

int gmb_db_dbf_seek(int64_t h, const char* key)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    DbfEngine* e;
    const IndexHandle* ih;
    uint8_t padded[256];
    int klen, ncopy;
    if (!hh || !hh->u.dbf.engine || !key) return GMB_DB_ERR;
    e = hh->u.dbf.engine;
    hh->u.dbf.found = 0;
    if (hh->u.dbf.active_order > 0) {
        ih = DbfEngine_GetIndex(e, hh->u.dbf.active_order - 1);
        klen = ih ? IndexHandle_get_KeyLength(ih) : 0;
        if (klen <= 0 || klen > (int)sizeof(padded)) return GMB_DB_ERR;
        memset(padded, ' ', (size_t)klen);
        ncopy = (int)strlen(key);
        if (ncopy > klen) ncopy = klen;
        /* I campi numerici DBF sono RIGHT-aligned (spazi a sinistra): il key di
         * SEEK/INDEX usa lo stesso allineamento, altrimenti "2" non trova "         2". */
        {
            int kf = hh->u.dbf.index_key_field[hh->u.dbf.active_order - 1];
            int numeric = kf >= 0 && kf < hh->u.dbf.field_count
                && hh->u.dbf.fields[kf].type != 'C' && hh->u.dbf.fields[kf].type != 'M';
            if (numeric) memcpy(padded + (klen - ncopy), key, (size_t)ncopy);
            else memcpy(padded, key, (size_t)ncopy);
        }
        if (DbfEngine_Seek(e, padded, klen) != GMB_DB_OK) {
            gmb_dbf_sync(hh);
            return GMB_DB_NOTFOUND;
        }
        gmb_dbf_sync(hh);
        return hh->u.dbf.found ? GMB_DB_OK : GMB_DB_NOTFOUND;
    }
    /* Clipper: senza ordine controllante SEEK fallisce (FOUND()=.F.); il primo indice
     * diventa controllante quando viene aperto/creato, mentre `SET ORDER TO 0` lo azzera.
     * Nessuna scansione lineare del file (il vecchio fallback sul campo 0 confrontava
     * l'ID con un testo). */
    hh->u.dbf.eof = 1;
    hh->u.dbf.recno = (int)hh->u.dbf.rec_count + 1;
    return GMB_DB_NOTFOUND;
}

int gmb_db_dbf_reindex(int64_t h)
{
    gmb_handle_t* hh = gmb_db_get(h, GMB_H_DBF);
    if (!hh || !hh->u.dbf.engine) return GMB_DB_ERR;
    if (DbfEngine_Reindex(hh->u.dbf.engine) != GMB_DB_OK) return GMB_DB_ERR;
    gmb_dbf_sync(hh);
    return GMB_DB_OK;
}
