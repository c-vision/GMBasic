/* ============================================================================
 * gmb_dict.c — dizionari (collezioni DICT, chiave stringa) e unwrap val→dict.
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

gmb_dict_t gmb_dict_new(void) {
    gmb_dict_t d;
    d.items = NULL;
    d.count = 0;
    d.capacity = 0;
    return d;
}

/* --- Ownership dei valori OBJECT del dict ---------------------------------------------------
 * Un dict POSSIEDE i valori OBJECT che depositano un riferimento contato: gmb_dict_set li
 * trattiene (retain), gmb_dict_free / clear / remove / sostituzione li rilasciano (release).
 * "Contato" = release_fn != NULL: sono gli oggetti heap della VM (lista/dict condivisi, closure,
 * stringhe VM, ...) con refcount reale. Gli oggetti del transpiler (gmb_obj_new: release_fn NULL,
 * refcount 0) NON hanno ownership e restano "in prestito" come prima — niente retain/release,
 * quindi nessun cambiamento per C99Native. Prima di questo i valori restavano in
 * prestito: se la sorgente moriva prima del dict, il dict puntava a memoria liberata
 * (bug "backends": "" del registry su VM/C). I valori non-OBJECT (numeri, stringhe C) non cambiano.
 * Solo gmb_dict_set deposita valori: qualunque altro writer che copi coppie deve fare retain.
 * Helper condivisi con liste e collezioni: gmb_val_retain/gmb_val_release (gmb_core.c). */
void gmb_dict_free(gmb_dict_t* d) {
    if (!d) return;
    for (size_t i = 0; i < d->count; i++) {
        if (d->items[i].key) free(d->items[i].key);
        gmb_val_release(d->items[i].value);
    }
    if (d->items) free(d->items);
    d->items = NULL; d->count = 0; d->capacity = 0;
}

gmb_int_t gmb_dict_count(gmb_dict_t d) { return (gmb_int_t)d.count; }

/* forward declaration: gmb_dict_get_double (sotto) usa gmb_dict_find, definita più avanti
 * come static — senza questa, il C la tratta come non-static (implicit declaration) e poi
 * "static declaration follows non-static" → errore di compilazione. */
static gmb_pair_t* gmb_dict_find(gmb_dict_t* d, const char* key);

/* Estrae un double da un dict per chiave (per campi come Statistic, PValue, etc.) */
gmb_double_t gmb_dict_get_double(gmb_dict_t d, const char* key) {
    gmb_pair_t* pair = gmb_dict_find(&d, key);
    if (!pair) return 0.0;
    return pair->value.v_double;
}

static gmb_pair_t* gmb_dict_find(gmb_dict_t* d, const char* key) {
    /* Chiavi CASE-INSENSITIVE (parità col Dictionary OrdinalIgnoreCase del managed):
     * il JSON parse conserva la case del documento ("value"), mentre la superficie BASIC
     * scrive `d.VALUE`; con strcmp il membro JSON su C99Native tornava NIL. L'accesso
     * esatto resta il caso normale; il confronto case-insensitive è la semantica canonica
     * delle chiavi (vedi anche gmb_dict_get_str). */
    for (size_t i = 0; i < d->count; i++)
        if (d->items[i].key && strcasecmp(d->items[i].key, key) == 0) return &d->items[i];
    return NULL;
}

void gmb_dict_set(gmb_dict_t* d, const char* key, gmb_val_t v) {
    if (!d || !key) return;
    gmb_pair_t* p = gmb_dict_find(d, key);
    if (p) {
        /* trattieni il nuovo PRIMA di rilasciare il vecchio: set(k, d[k]) non deve liberarlo */
        gmb_val_retain(v);
        gmb_val_t old = p->value;
        p->value = v;
        gmb_val_release(old);
        return;
    }
    if (d->count >= d->capacity) {
        size_t nc = d->capacity == 0 ? 4 : d->capacity * 2;
        gmb_pair_t* ni = (gmb_pair_t*)realloc(d->items, nc * sizeof(gmb_pair_t));
        if (!ni) return;
        d->items = ni;
        d->capacity = nc;
    }
    d->items[d->count].key = strdup(key);
    d->items[d->count].value = v;
    gmb_val_retain(v);
    d->count++;
}

gmb_val_t gmb_dict_get(gmb_dict_t d, const char* key) {
    gmb_pair_t* p = gmb_dict_find(&d, key);
    return p ? p->value : gmb_val_nil();
}

/* Lettura STRINGA di un dict (es. header di risposta): chiave assente → "" (parità col
   Dictionary OrdinalIgnoreCase del managed: chiave assente → stringa vuota, non "<nil>").
   Valore non-stringa → resa canonica (gmb_val_to_string). Unica sorgente (§0bis) per
   l'accessor .Headers.Get(...) di C99Native (bug Headers-Get-Divergenza-Backend). */
gmb_string_t gmb_dict_get_str(gmb_dict_t d, const char* key) {
    gmb_val_t v = gmb_dict_get(d, key);
    if (v.kind == GMB_TYPE_STRING) return v.v_string;
    if (v.kind == GMB_TYPE_NIL) return gmb_string_alloc("");
    return gmb_val_to_string(v);
}

gmb_list_t gmb_dict_keys(gmb_dict_t d) {
    gmb_list_t l = gmb_list_new();
    for (size_t i = 0; i < d.count; i++)
        gmb_list_add(&l, gmb_make_val_string(d.items[i].key));
    return l;
}

/* Chiavi in ordine alfabetico (dizionario SORTED): bubble-sort con confronto
 * numerico-aware ("5" < "10" se entrambe chiavi numeriche). */
gmb_list_t gmb_dict_keys_sorted(gmb_dict_t d) {
    gmb_list_t l = gmb_dict_keys(d);
    for (gmb_int_t i = 0; i < (gmb_int_t)l.count - 1; i++)
        for (gmb_int_t j = 0; j < (gmb_int_t)l.count - 1 - i; j++) {
            const char* a = l.items[j].v_string.data ? l.items[j].v_string.data : "";
            const char* b = l.items[j + 1].v_string.data ? l.items[j + 1].v_string.data : "";
            int cmp;
            char* ea = NULL, * eb = NULL;
            double da = strtod(a, &ea), db = strtod(b, &eb);
            if (ea != a && *ea == 0 && eb != b && *eb == 0) cmp = (da > db) - (da < db);
            else cmp = strcmp(a, b);
            if (cmp > 0) {
                gmb_val_t t = l.items[j]; l.items[j] = l.items[j + 1]; l.items[j + 1] = t;
            }
        }
    return l;
}

/* gmb_dict_t ha SEMPRE chiavi stringa (lo storage sottostante non ha un concetto di chiave
 * numerica: un DICT OF DOUBLE, ... converte la chiave alla sua rappresentazione decimale
 * PRIMA di scrivere, vedi emit_dict_key nel backend C99Native) — gmb_dict_keys/_sorted
 * restituiscono quindi SEMPRE gmb_val_t di kind stringa, anche per una mappa dichiarata a
 * chiave numerica: bug C99Native-Comprehension-DictParam-ChiaviNumeriche (AiWiki), .Keys()
 * su una mappa a chiave numerica restituiva stringhe, "0" a valle di ogni conversione
 * numerica implicita. Queste varianti ricostruiscono il tipo REALE di ogni chiave dalla sua
 * rappresentazione stringa via gmb_dict_key_val (euristico: tutta cifre/decimale → numero,
 * altrimenti stringa — nessun bisogno di conoscere staticamente il tipo dichiarato, e
 * corretto anche per un dizionario a chiave stringa genuina, le cui chiavi non "sembrano"
 * mai numeriche). Stesso helper già riusato da VM/C per lo stesso identico bug
 * (vm_list_fill_keys, native/gmb_vm_c/gmb_vm_core.c) — qui si preserva anche l'ordinamento
 * di gmb_dict_keys_sorted (il confronto numerico-aware lì opera sulla rappresentazione
 * stringa, quindi la conversione di tipo avviene DOPO l'ordinamento, non prima). */
gmb_list_t gmb_dict_keys_typed(gmb_dict_t d) {
    gmb_list_t l = gmb_list_new();
    for (size_t i = 0; i < d.count; i++)
        gmb_list_add(&l, gmb_dict_key_val(d.items[i].key ? d.items[i].key : ""));
    return l;
}
gmb_list_t gmb_dict_keys_typed_sorted(gmb_dict_t d) {
    gmb_list_t src = gmb_dict_keys_sorted(d);
    gmb_list_t l = gmb_list_new();
    for (size_t i = 0; i < src.count; i++)
        gmb_list_add(&l, gmb_dict_key_val(src.items[i].v_string.data ? src.items[i].v_string.data : ""));
    gmb_list_free(&src);
    return l;
}

/* Valori in ordine di inserimento (DICT) — lista di gmb_val_t. */
gmb_list_t gmb_dict_values(gmb_dict_t d) {
    gmb_list_t l = gmb_list_new();
    for (size_t i = 0; i < d.count; i++)
        gmb_list_add(&l, d.items[i].value);
    return l;
}

/* Copia PROFONDA delle chiavi (indipendenza mutazionale: X.ToDict() poi Remove su una
 * copia non tocca l'originale). I valori condividono il valore boxato. */
gmb_dict_t gmb_dict_copy(gmb_dict_t d) {
    gmb_dict_t nd = gmb_dict_new();
    for (size_t i = 0; i < d.count; i++)
        gmb_dict_set(&nd, d.items[i].key, d.items[i].value);
    return nd;
}

/* chiave → val: numerica (tutta cifre/decimale) se possibile, altrimenti stringa.
 * Pubblica (dichiarata in gmb_runtime.h): riusata anche da VM/C. */
gmb_val_t gmb_dict_key_val(const char* key) {
    if (!key || !*key) return gmb_make_val_string("");
    int allnum = 1, dot = 0;
    for (const char* q = key; *q; q++) {
        if (*q == '.' && !dot) { dot = 1; continue; }
        if (*q == '-' && q == key) continue;
        if (!(*q >= '0' && *q <= '9')) { allnum = 0; break; }
    }
    if (allnum) return gmb_make_val_double(atof(key));
    return gmb_make_val_string(key);
}

/* Estremo (min/max per chiave ordinata) come coppia [chiave, valore]; remove=1 lo
 * estrae anche dal dizionario (ExtractMin/ExtractMax). Ordina sempre per chiave. */
gmb_list_t gmb_dict_extreme(gmb_dict_t* d, int want_max, int remove) {
    gmb_list_t r = gmb_list_new();
    if (!d) return r;
    gmb_list_t ks = gmb_dict_keys_sorted(*d);
    if (ks.count == 0) return r;
    gmb_int_t idx = want_max ? (gmb_int_t)ks.count - 1 : 0;
    gmb_val_t kk = ks.items[idx];
    const char* key = kk.v_string.data ? kk.v_string.data : "";
    gmb_list_add(&r, gmb_dict_key_val(key));
    gmb_list_add(&r, gmb_dict_get(*d, key));
    if (remove) gmb_dict_remove(d, key);
    return r;
}

/* ToArray delle mappe ordinate: lista di oggetti {"Key":<k>, "Value":<v>} ordinata. */
gmb_list_t gmb_dict_to_kvp(gmb_dict_t d) {
    gmb_list_t r = gmb_list_new();
    gmb_list_t ks = gmb_dict_keys_sorted(d);
    for (gmb_int_t i = 0; i < (gmb_int_t)ks.count; i++) {
        const char* key = ks.items[i].v_string.data ? ks.items[i].v_string.data : "";
        gmb_dict_t o = gmb_dict_new();
        gmb_dict_set(&o, "Key", gmb_dict_key_val(key));
        gmb_dict_set(&o, "Value", gmb_dict_get(d, key));
        gmb_list_add(&r, gmb_make_val_dict_take(&o));
    }
    return r;
}

/* Valori ordinati per chiave (SORTED) — chiavi alfabetiche, valori allineati. */
gmb_list_t gmb_dict_values_sorted(gmb_dict_t d) {
    gmb_list_t l = gmb_list_new();
    gmb_list_t ks = gmb_dict_keys_sorted(d);
    for (gmb_int_t i = 0; i < (gmb_int_t)ks.count; i++) {
        const char* want = ks.items[i].v_string.data ? ks.items[i].v_string.data : "";
        for (size_t j = 0; j < d.count; j++)
            if (strcmp(d.items[j].key, want) == 0) { gmb_list_add(&l, d.items[j].value); break; }
    }
    return l;
}

gmb_bool_t gmb_dict_contains_key(gmb_dict_t d, const char* key) {
    return gmb_dict_find(&d, key) != NULL;
}

gmb_bool_t gmb_dict_remove(gmb_dict_t* d, const char* key) {
    if (!d) return 0;
    for (size_t i = 0; i < d->count; i++) {
        if (d->items[i].key && strcmp(d->items[i].key, key) == 0) {
            free(d->items[i].key);
            gmb_val_t gone = d->items[i].value;
            for (size_t k = i; k + 1 < d->count; k++) d->items[k] = d->items[k + 1];
            d->count--;
            gmb_val_release(gone);
            return 1;
        }
    }
    return 0;
}

/* --- Unwrap di un gmb_val_t boxed a dict (JSON/object) --- */
gmb_dict_t gmb_val_as_dict(gmb_val_t v) {
    gmb_dict_t r = {NULL, 0, 0};
    /* `DIM x AS JSON = "<stringa JSON>"`: la var resta una STRINGA (parità IL/PCode) e il
     * member access `x.Campo` deve PARSARE la stringa on-access. Un val STRINGA che inizia con
     * '{' è una stringa-JSON → parsa in un object (heap, che resta vivo: nessuno lo libera →
     * leak coerente col modello val, MAI dangling) e restituisci il suo dict. Per stringhe
     * non-JSON il parse non produce un object → resta il dict vuoto (comportamento invariato);
     * l'oggetto parsed è POSSEDUTO (da `gmb_json_parse`), non freed qui. */
    if (v.kind == GMB_TYPE_STRING && v.v_string.data && v.v_string.data[0] == '{') {
        gmb_val_t pv = gmb_json_parse(v.v_string.data);
        if (pv.kind == GMB_TYPE_OBJECT) {
            void* pdata = gmb_obj_data(pv);
            if (pdata) { r = *(gmb_dict_t*)pdata; return r; }
        }
        return r;
    }
    void* data = gmb_obj_data(v);
    if (!data) return r;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    // Solo dict reali: un UDT boxed (gmb_udt_*) NON è un dict → restituisci vuoto
    // (mai reinterpretare una struct UDT come dict: heap overflow).
    if (o && o->type_name && !strstr(o->type_name, "dict")) return r;
    r = *(gmb_dict_t*)data;
    return r;
}

/* Scrittura in-place su un gmb_val_t boxed a dict (oggetto opaco): setta la coppia
   key/value direttamente nel dict contenuto nel gmb_obj_t (niente copia). */
gmb_val_t gmb_val_dict_get(gmb_val_t v, const char* key) {
    if (v.kind != GMB_TYPE_OBJECT) return gmb_val_nil();
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o || !o->data) return gmb_val_nil();
    gmb_dict_t* d = (gmb_dict_t*)o->data;
    return gmb_dict_get(*d, key ? key : "");
}

int gmb_val_dict_set(gmb_val_t v, const char* key, gmb_val_t value) {
    if (v.kind != GMB_TYPE_OBJECT) return -1;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o || !o->data) return -1;
    gmb_dict_t* d = (gmb_dict_t*)o->data;
    gmb_dict_set(d, key, value);
    return 0;
}

int gmb_val_dict_remove(gmb_val_t v, const char* key) {
    if (v.kind != GMB_TYPE_OBJECT) return -1;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o || !o->data) return -1;
    gmb_dict_t* d = (gmb_dict_t*)o->data;
    gmb_dict_remove(d, key ? key : "");
    return 0;
}

/* DICT.Clear() / COOKIEJAR.Clear() — svuota il dict boxato in un gmb_val_t.
 * Sorgente unica condivisa (AGENTS §0bis): VM/C, C99Native chiamano
 * questa; nessun emitter deve reinventare l'estrattore (gmb_val_as_dict copia per
 * valore, quindi "&" su di essa è illegale e "temporanea + gmb_dict_clear" è una
 * frammentazione C). Parità managed (CookieJar.Clear rimuove tutti i cookie). */
int gmb_val_dict_clear(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT) return -1;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (!o || !o->data) return -1;
    gmb_dict_t* d = (gmb_dict_t*)o->data;
    gmb_dict_clear(d);
    return 0;
}

/* Boxing di un dict in gmb_val_t (GMB_TYPE_OBJECT): copia su heap come gmb_make_val_list. */
gmb_val_t gmb_make_val_dict(gmb_dict_t src) {
    gmb_dict_t* p = (gmb_dict_t*)malloc(sizeof(gmb_dict_t));
    if (!p) return gmb_val_nil();
    *p = src;
    p->items = NULL; p->count = 0; p->capacity = 0;
    if (src.count > 0) {
        p->items = (gmb_pair_t*)malloc(src.count * sizeof(gmb_pair_t));
        if (p->items) {
            for (size_t i = 0; i < src.count; i++) {
                p->items[i].key = strdup(src.items[i].key);
                p->items[i].value = src.items[i].value;
                /* la copia e' un secondo possessore: trattiene i valori contati, cosi' liberare il
                 * dict sorgente (gmb_dict_free) non lascia la copia con puntatori pendenti */
                gmb_val_retain(p->items[i].value);
            }
            p->count = src.count; p->capacity = src.count;
        }
    }
    /* anche per dict VUOTO (count 0): boxa il dict (mai gmb_val_nil) */
    gmb_obj_t* box = gmb_obj_new_typed(p, "gmb_dict");
    if (!box)
    {
        gmb_dict_free(p);
        free(p);
        return gmb_val_nil();
    }
    /* NB: il box NON ha release_fn di proposito. Nei mondi transpiler (C99Native) non
     * esiste refcount: gli oggetti sono "immortali" (refcount 0, release_fn NULL) e piu' variabili
     * condividono lo stesso box; dargli un distruttore lo renderebbe un oggetto contato e il primo
     * gmb_dict_free di un dict che lo contiene lo libererebbe sotto i piedi delle altre variabili. */
    return gmb_make_val_obj(box);
}

/* Variante "take" di gmb_make_val_dict: il box ADOTTA chiavi e array delle coppie di `*src` (nessuna copia,
 * nessun retain: la proprieta' dei valori si trasferisce con l'array) e lascia `*src` vuoto. Contratto: `*src`
 * ha storage PROPRIO (gmb_dict_new/derivati), non e' un alias (es. gmb_val_as_dict di un altro box). Dopo la
 * chiamata `gmb_dict_free(src)` e' un no-op. */
gmb_val_t gmb_make_val_dict_take(gmb_dict_t* src) {
    if (!src) return gmb_val_nil();
    gmb_dict_t* p = (gmb_dict_t*)malloc(sizeof(gmb_dict_t));
    if (!p) return gmb_val_nil();
    *p = *src;
    gmb_obj_t* box = gmb_obj_new_typed(p, "gmb_dict");
    if (!box) { free(p); return gmb_val_nil(); }   /* *src resta intatto: il chiamante lo libera */
    src->items = NULL; src->count = 0; src->capacity = 0;
    return gmb_make_val_obj(box);
}

/* Libera un valore temporaneo creato da gmb_make_val_dict / gmb_make_val_dict_take.
 * Quei box sono deliberatamente "immortali" (release_fn NULL, refcount 0) perché nei mondi
 * transpiler i dict sono condivisi fra variabili; ma un percorso PER-RICHIESTA (il seeding
 * BSP: REQ/QUERY/FORM/SESSION) li crea e li abbandona → leak di ~1,7 KB per render, misurato
 * con `leaks` (8.684 leak/300 render su una pagina vuota). Qui si libera SOLO un box "gmb_dict"
 * senza distruttore: qualunque altro oggetto è ignorato (mai liberare roba altrui). */
void gmb_val_free_temp_dict(gmb_val_t v) {
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (o->release_fn || o->dtor) return;                 /* oggetto contato/gestito: non è nostro */
    if (!o->type_name || strcmp(o->type_name, "gmb_dict") != 0) return;
    gmb_dict_t* d = (gmb_dict_t*)o->data;
    if (d) { gmb_dict_free(d); free(d); }
    free(o);
}

gmb_list_t gmb_val_dict_keys(gmb_val_t v) {
    gmb_list_t l = gmb_list_new();
    gmb_dict_t d = gmb_val_as_dict(v);
    for (size_t i = 0; i < d.count; i++)
        gmb_list_add(&l, gmb_make_val_string(d.items[i].key));
    return l;
}

// KEYS come array C di gmb_string_t con lunghezza compagna (per `DIM x() AS STRING = KEYS(...)`).
gmb_string_t* gmb_val_dict_keys_array(gmb_val_t v, gmb_int_t* len_out) {
    gmb_dict_t d = gmb_val_as_dict(v);
    size_t n = d.count;
    gmb_string_t* out = (gmb_string_t*)calloc(n ? n : 1, sizeof(gmb_string_t));
    for (size_t i = 0; i < n; i++) out[i] = gmb_string_alloc(d.items[i].key);
    *len_out = (gmb_int_t)n;
    return out;
}

/* ============================================================================
 * Parser JSON — UNA implementazione, tre percorsi (2026-11-01)
 *
 *   gmb_json_parse            → albero completo (gmb_val_t)
 *   gmb_json_extract_val      → ZERO-ALLOC: scansiona e materializza SOLO il valore al path
 *                               (stile `fastjson.c` di Redis: i rami non richiesti si SALTANO)
 *   gmb_json_parse_to_dict    → riversa le coppie di primo livello in un dict esistente
 *
 * Perché è stato riscritto (misure su questa macchina, JSON di 11.891 B con 1000 chiavi
 * piatte): 964 µs/parse, 12 MB/s — il costo era `gmb_dict_set`, che cerca la chiave in modo
 * LINEARE (O(n²) sull'intero oggetto). Ora l'oggetto si costruisce con un indice hash locale
 * (open addressing) → O(1) per chiave. Inoltre:
 *   - stringhe: fast path `memchr` (nessun escape → UNA allocazione esatta, niente crescita
 *     geometrica con realloc);
 *   - numeri: fast path intero con `strtoll` (niente `atof`), altrimenti `strtod`;
 *   - chiavi: confronto in-place sul testo (nessun buffer fisso da 512 B);
 *   - profondità limitata (GMB_JSON_MAX_DEPTH) contro input ostili (prima stack overflow);
 *   - `extract` di un campo non costruisce più l'albero: da ~940 µs a pochi µs sul caso sopra.
 * ============================================================================ */

#define GMB_JSON_MAX_DEPTH 256

static const char* jp_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static int jp_is_num_char(char c) {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E';
}

/* Fine della stringa JSON che inizia su `p` (che punta a '"'). *has_esc = 1 se contiene '\\'.
 * ATTENZIONE: scansione LINEARE sul solo contenuto della stringa (niente strlen sul resto del
 * documento: con quello ogni stringa costava O(documento) → O(n²) sull'intero JSON, misurato
 * 5x più lento su un payload da 200 elementi). */
static const char* jp_str_end(const char* p, int* has_esc) {
    if (has_esc) *has_esc = 0;
    if (*p != '"') return p;
    p++;
    for (;;) {
        while (*p && *p != '"' && *p != '\\') p++;
        if (*p == '\\') { if (has_esc) *has_esc = 1; p++; if (*p) p++; continue; }
        return p;                     /* '"' oppure fine input */
    }
}

/* Decodifica una stringa JSON [start,end) in `out` (cap): ritorna la lunghezza scritta. */
static size_t jp_str_decode(const char* start, const char* end, char* out, size_t cap) {
    size_t n = 0;
    const char* p = start;
    while (p < end && n + 1 < cap) {
        char c = *p++;
        if (c != '\\' || p >= end) { out[n++] = c; continue; }
        char e = *p++;
        switch (e) {
            case 'n': out[n++] = '\n'; break;
            case 't': out[n++] = '\t'; break;
            case 'r': out[n++] = '\r'; break;
            case 'b': out[n++] = '\b'; break;
            case 'f': out[n++] = '\f'; break;
            case 'u': {
                int cp = 0, ok = 1;
                for (int i = 0; i < 4 && ok; i++) {
                    char h = (p + i < end) ? p[i] : 0;
                    if (h >= '0' && h <= '9') cp = (cp << 4) | (h - '0');
                    else if (h >= 'a' && h <= 'f') cp = (cp << 4) | (h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp = (cp << 4) | (h - 'A' + 10);
                    else ok = 0;
                }
                if (ok) {
                    p += 4;
                    if (n + 4 >= cap) break;
                    if (cp < 0x80) out[n++] = (char)cp;
                    else if (cp < 0x800) { out[n++] = (char)(0xC0 | (cp >> 6)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
                    else { out[n++] = (char)(0xE0 | (cp >> 12)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
                } else out[n++] = '?';
                break;
            }
            default: out[n++] = e; break;   /* \" \\ \/ e altro: il carattere stesso */
        }
    }
    out[n] = '\0';
    return n;
}

/* Valore JSON che inizia su `p`: albero completo. */
static gmb_val_t jp_value(const char** pp, int depth);

/* Salta un valore senza allocare (fastjson): ritorna il puntatore dopo il valore. */
static const char* jp_skip_value(const char* p, int depth) {
    p = jp_ws(p);
    if (*p == '"') { const char* e = jp_str_end(p, NULL); return *e == '"' ? e + 1 : e; }
    if (*p == '{' || *p == '[') {
        if (depth > GMB_JSON_MAX_DEPTH) { p++; return p; }
        char close = (*p == '{') ? '}' : ']';
        p = jp_ws(p + 1);
        if (*p == close) return p + 1;
        for (;;) {
            if (*p == '\0') return p;
            if (close == '}') {                       /* chiave: "k" : valore */
                if (*p == '"') { const char* e = jp_str_end(p, NULL); p = (*e == '"') ? e + 1 : e; }
                else p++;                              /* input malformato: avanza (mai loop) */
                p = jp_ws(p);
                if (*p == ':') p = jp_ws(p + 1);
            }
            p = jp_skip_value(p, depth + 1);
            p = jp_ws(p);
            if (*p == ',') { p = jp_ws(p + 1); continue; }
            if (*p == close) return p + 1;
            if (*p == '\0') return p;
            p++;
        }
    }
    if (*p == 't') return p + 4;
    if (*p == 'f') return p + 5;
    if (*p == 'n') return p + 4;
    while (jp_is_num_char(*p)) p++;
    return p;
}

/* Indice hash locale per gli oggetti (open addressing, slot = indice+1 in dict.items).
 * Cresce al 50% di carico; il confronto delle chiavi è sul testo (i dict non hanno hash). */
typedef struct { size_t cap, mask, count; size_t* slot; } jp_hash_t;

static uint64_t jp_hash_bytes(const char* s, size_t n) {
    uint64_t h = 1469598103934665603ULL;          /* FNV-1a 64 */
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)s[i]; h *= 1099511628211ULL; }
    return h;
}

static void jp_hash_reinsert(jp_hash_t* h, gmb_dict_t* d) {
    for (size_t k = 0; k < d->count; k++) {
        const char* key = d->items[k].key;
        if (!key) continue;
        uint64_t hv = jp_hash_bytes(key, strlen(key));
        size_t i = (size_t)hv & h->mask;
        while (h->slot[i]) i = (i + 1) & h->mask;
        h->slot[i] = k + 1;
    }
}

static void jp_hash_grow(jp_hash_t* h, gmb_dict_t* d) {
    size_t ncap = h->cap ? h->cap * 2 : 16;
    free(h->slot);
    h->slot = (size_t*)calloc(ncap, sizeof(size_t));
    if (!h->slot) { h->cap = h->mask = h->count = 0; return; }
    h->cap = ncap; h->mask = ncap - 1;
    jp_hash_reinsert(h, d);
    h->count = d->count;                /* slot occupati: serve al controllo di carico */
}

/* Accoda una coppia al dict SENZA ricerca lineare (l'unicità la garantisce l'hash). */
static gmb_pair_t* jp_dict_append(gmb_dict_t* d, const char* key, size_t klen, gmb_val_t v) {
    if (d->count >= d->capacity) {
        size_t nc = d->capacity == 0 ? 8 : d->capacity * 2;
        gmb_pair_t* ni = (gmb_pair_t*)realloc(d->items, nc * sizeof(gmb_pair_t));
        if (!ni) return NULL;
        d->items = ni; d->capacity = nc;
    }
    gmb_pair_t* pr = &d->items[d->count];
    pr->key = (char*)malloc(klen + 1);
    if (!pr->key) return NULL;
    memcpy(pr->key, key, klen);
    pr->key[klen] = '\0';
    pr->value = v;
    gmb_val_retain(v);
    d->count++;
    return pr;
}

/* Inserisce (o sostituisce, "vince l'ultima") una coppia oggetto.
 * Ibrido: fino a GMB_JSON_HASH_MIN chiavi si usa la scansione LINEARE (gli oggetti piccoli —
 * record, risposte API — sono la norma e l'hash costerebbe un calloc per oggetto); da lì in poi
 * si passa all'indice hash, che elimina l'O(n²) sugli oggetti con molte chiavi (1000 chiavi
 * piatte: 964 µs → ~32 µs). */
#define GMB_JSON_HASH_MIN 12

static void jp_obj_put(jp_hash_t* h, gmb_dict_t* d, const char* key, size_t klen, gmb_val_t v) {
    if (h->cap == 0 && d->count < GMB_JSON_HASH_MIN) {
        for (size_t i = 0; i < d->count; i++) {
            gmb_pair_t* pr = &d->items[i];
            if (pr->key && pr->key[klen] == '\0' && memcmp(pr->key, key, klen) == 0) {
                gmb_val_retain(v);
                gmb_val_t old = pr->value;
                pr->value = v;
                gmb_val_release(old);
                return;
            }
        }
        if (jp_dict_append(d, key, klen, v) && d->count == GMB_JSON_HASH_MIN) jp_hash_grow(h, d);
        return;
    }
    if (h->cap == 0) jp_hash_grow(h, d);
    if ((h->count + 1) * 2 > h->cap) jp_hash_grow(h, d);
    if (!h->cap) {                                     /* OOM: degrado a scansione lineare */
        if (jp_dict_append(d, key, klen, v)) h->count = d->count;
        return;
    }
    uint64_t hv = jp_hash_bytes(key, klen);
    size_t i = (size_t)hv & h->mask;
    while (h->slot[i]) {
        gmb_pair_t* pr = &d->items[h->slot[i] - 1];
        if (pr->key[klen] == '\0' && memcmp(pr->key, key, klen) == 0) {
            gmb_val_retain(v);
            gmb_val_t old = pr->value;
            pr->value = v;
            gmb_val_release(old);
            return;
        }
        i = (i + 1) & h->mask;
    }
    if (jp_dict_append(d, key, klen, v)) { h->slot[i] = d->count; h->count++; }
}

static gmb_val_t jp_number(const char** pp) {
    const char* p = *pp;
    int is_int = 1;
    const char* q = p;
    if (*q == '-') q++;
    for (; jp_is_num_char(*q); q++)
        if (*q == '.' || *q == 'e' || *q == 'E') is_int = 0;
    if (is_int && q > p) {
        char buf[32]; size_t n = (size_t)(q - p);
        if (n < sizeof buf) {
            memcpy(buf, p, n); buf[n] = '\0';
            *pp = q;
            return gmb_make_val_double((double)strtoll(buf, NULL, 10));
        }
    }
    char num[128]; size_t ni = 0;
    while (jp_is_num_char(*p) && ni < sizeof(num) - 1) num[ni++] = *p++;
    num[ni] = '\0';
    *pp = p;
    return gmb_make_val_double(ni ? strtod(num, NULL) : 0.0);
}

static gmb_val_t jp_object(const char** pp, int depth) {
    gmb_dict_t obj = gmb_dict_new();
    jp_hash_t h = { 0, 0, 0, NULL };
    const char* p = jp_ws(*pp + 1);            /* dopo '{' */
    while (*p && *p != '}') {
        p = jp_ws(p);
        if (*p != '"') { if (*p) p++; else break; continue; }
        int esc = 0;
        const char* kstart = p + 1;
        const char* kend = jp_str_end(p, &esc);
        if (*kend != '"') { p = kend; break; }
        char kbuf[512];
        const char* key = kstart;
        size_t klen = (size_t)(kend - kstart);
        if (esc) { klen = jp_str_decode(kstart, kend, kbuf, sizeof kbuf); key = kbuf; }
        p = jp_ws(kend + 1);
        if (*p == ':') p = jp_ws(p + 1);
        gmb_val_t v = jp_value(&p, depth + 1);
        jp_obj_put(&h, &obj, key, klen, v);
        gmb_val_release(v);                     /* la referenza del dict resta */
        p = jp_ws(p);
        if (*p == ',') { p = jp_ws(p + 1); continue; }
        if (*p == '}') break;
        if (*p == '\0') break;
        p++;
    }
    if (*p == '}') p++;
    *pp = p;
    free(h.slot);
    return gmb_make_val_dict_take(&obj);
}

static gmb_val_t jp_array(const char** pp, int depth) {
    gmb_list_t l = gmb_list_new();
    const char* p = jp_ws(*pp + 1);
    if (*p == ']') { *pp = p + 1; return gmb_make_val_list_take(&l); }
    while (*p) {
        gmb_val_t v = jp_value(&p, depth + 1);
        gmb_list_add(&l, v);                    /* trattiene */
        gmb_val_release(v);                     /* rilasciamo la nostra */
        p = jp_ws(p);
        if (*p == ',') { p = jp_ws(p + 1); continue; }
        if (*p == ']') { p++; break; }
        if (*p == '\0') break;
        p++;
    }
    *pp = p;
    return gmb_make_val_list_take(&l);
}

static gmb_val_t jp_value(const char** pp, int depth) {
    if (depth > GMB_JSON_MAX_DEPTH) { *pp = jp_skip_value(*pp, depth); return gmb_val_nil(); }
    const char* p = jp_ws(*pp);
    if (*p == '{') return jp_object(pp, depth);
    if (*p == '[') return jp_array(pp, depth);
    if (*p == '"') {
        int esc = 0;
        const char* start = p + 1;
        const char* end = jp_str_end(p, &esc);
        char* buf;
        size_t n;
        if (!esc) {                            /* fast path: nessun escape, allocazione esatta */
            n = (size_t)(end - start);
            buf = (char*)malloc(n + 1);
            if (!buf) { *pp = (*end == '"') ? end + 1 : end; return gmb_make_val_string(""); }
            memcpy(buf, start, n); buf[n] = '\0';
        } else {
            size_t cap = (size_t)(end - start) + 1;
            buf = (char*)malloc(cap);
            if (!buf) { *pp = (*end == '"') ? end + 1 : end; return gmb_make_val_string(""); }
            n = jp_str_decode(start, end, buf, cap);
            (void)n;
        }
        *pp = (*end == '"') ? end + 1 : end;
        gmb_val_t v = gmb_make_val_string(buf);
        free(buf);
        return v;
    }
    if (*p == 't') { *pp = p + 4; return gmb_make_val_bool(1); }
    if (*p == 'f') { *pp = p + 5; return gmb_make_val_bool(0); }
    if (*p == 'n') { *pp = p + 4; return gmb_val_nil(); }
    return jp_number(pp);
}

gmb_val_t gmb_json_parse(const char* js) {
    if (!js) return gmb_val_nil();
    const char* p = jp_ws(js);
    return jp_value(&p, 0);
}

/* Riversa le coppie di PRIMO livello nel dict `out` (merge: chiavi già presenti sostituite,
 * come gmb_dict_set). Usa lo stesso scanner del parser: stringhe con escape decodificate,
 * numeri con fast path intero. */
void gmb_json_parse_to_dict(const char* js, gmb_dict_t* out) {
    if (!js || !out) return;
    const char* p = jp_ws(js);
    if (*p != '{') return;
    p = jp_ws(p + 1);
    while (*p && *p != '}') {
        if (*p != '"') { if (*p) p++; else break; continue; }
        int esc = 0;
        const char* ks = p + 1;
        const char* ke = jp_str_end(p, &esc);
        if (*ke != '"') break;
        char kbuf[512];
        const char* key = ks;
        size_t kl = (size_t)(ke - ks);
        if (esc) {
            kl = jp_str_decode(ks, ke, kbuf, sizeof kbuf);
            key = kbuf;
        } else {
            if (kl >= sizeof kbuf) kl = sizeof kbuf - 1;
            memcpy(kbuf, key, kl);
            kbuf[kl] = '\0';
            key = kbuf;
        }
        p = jp_ws(ke + 1);
        if (*p == ':') p = jp_ws(p + 1);
        gmb_val_t v = jp_value(&p, 0);
        gmb_dict_set(out, key, v);      /* trattiene */
        gmb_val_release(v);             /* rilasciamo la nostra referenza */
        p = jp_ws(p);
        if (*p == ',') { p = jp_ws(p + 1); continue; }
        break;
    }
}

void gmb_dict_clear(gmb_dict_t* d) {
    if (!d) return;
    for (size_t i = 0; i < d->count; i++) { free(d->items[i].key); gmb_val_release(d->items[i].value); }
    free(d->items);
    d->items = NULL;
    d->count = 0;
    d->capacity = 0;
}

/* ---- STRINGIFY (JSON) su gmb_val_t (C99): serializza lista/dict/val in JSON string. ---- */
static void gmb_json_append_val(gmb_val_t v, char** buf, size_t* cap, size_t* pos);

/* Serializza un gmb_dict_t* GREZZO (non boxato in gmb_obj_t/type_name) come oggetto JSON
 * `{"k":v,...}` — riusata sia dal branch GMB_TYPE_OBJECT di gmb_json_append_val (dict C99
 * boxato in un gmb_obj_t) sia da gmb_dict_to_json (dict grezzo, es. VM/C: gmb_obj_shared_val_t
 * tiene il gmb_dict_t* direttamente in .dict, senza il wrapper gmb_obj_t/type_name del C99).
 * Unica implementazione della serializzazione dict->JSON (§0bis regola 2) — PUBLICA da F2
 * (la VM/C ci delega il builtin STRINGIFY, gmb_builtin_json eliminato). */
static void gmb_json_put_escaped(const char* s, char** buf, size_t* cap, size_t* pos);

void gmb_json_append_dict(gmb_dict_t* d, char** buf, size_t* cap, size_t* pos)
{
    if (!d) { while (*pos + 4 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); } memcpy(*buf + *pos, "null", 4); *pos += 4; return; }
    while (*pos + 2 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
    (*buf)[(*pos)++] = '{';
    int _first = 1;
    for (size_t i = 0; i < d->count; i++)
    {
        const char* key = d->items[i].key ? d->items[i].key : "";
        /* Istanza UDT/CLASS (VM-C usa dict con descriptor): escludi le chiavi di
         * bookkeeping "__class__"/"__closure__" dal JSON utente (parità IL/PCode/C99). */
        if (strcmp(key, "__class__") == 0 || strcmp(key, "__closure__") == 0) continue;
        if (!_first) { while (*pos + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); } (*buf)[(*pos)++] = ','; }
        _first = 0;
        gmb_json_put_escaped(key, buf, cap, pos);
        (*buf)[(*pos)++] = ':';
        gmb_json_append_val(d->items[i].value, buf, cap, pos);
    }
    while (*pos + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
    (*buf)[(*pos)++] = '}';
}

/* Serializza un gmb_list_t* GREZZO come array JSON `[a,b,...]` — F2 (§0bis): unica
 * implementazione della serializzazione list->JSON, PUBLICA perché VM/C e chiamanti
 * handle-based ci delegano (era duplicata in gmb_builtin_json.c, eliminato). */
void gmb_json_append_list(gmb_list_t* l, char** buf, size_t* cap, size_t* pos)
{
    if (!l) { while (*pos + 4 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); } memcpy(*buf + *pos, "null", 4); *pos += 4; return; }
    while (*pos + 2 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
    (*buf)[(*pos)++] = '[';
    for (size_t i = 0; i < l->count; i++)
    {
        if (i > 0) { while (*pos + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); } (*buf)[(*pos)++] = ','; }
        gmb_json_append_val(l->items[i], buf, cap, pos);
    }
    while (*pos + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
    (*buf)[(*pos)++] = ']';
}

/* Scrive una stringa JSON con l'escape ESATTO di System.Text.Json (JavaScriptEncoder.Default,
 * usato da JSON.TOJSON/STRINGIFY dell'IL): " → \u0022, \ → \\, ' → \u0027, + → \u002B,
 * < → \u003C, > → \u003E, & → \u0026, ` → \u0060, DEL → \u007F, C0 → \b \t \n \f \r oppure
 * \u00XX, non-ASCII → \uXXXX (surrogate per cp >= 0x10000), hex MAIUSCOLO. Prima mancavano
 * i caratteri di controllo e i non-ASCII: un tab rendeva il JSON INVALIDO e faceva scartare
 * l'intero array a ParseJsonArray (bug reale: GREP.MATCHES su directory grandi tornava -1). */
static void gmb_json_put_escaped(const char* s, char** buf, size_t* cap, size_t* pos)
{
    static const char* H = "0123456789ABCDEF";
    if (!s) s = "";
    #define JG(n) do { while (*pos + (n) + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); } } while (0)
    JG(8); (*buf)[(*pos)++] = '"';
    for (const unsigned char* p = (const unsigned char*)s; *p; )
    {
        unsigned char c = *p;
        if (c == '"') { JG(6); memcpy(*buf + *pos, "\\u0022", 6); *pos += 6; p++; }
        else if (c == '\\') { JG(2); (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = '\\'; p++; }
        else if (c == '\t') { JG(2); (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 't'; p++; }
        else if (c == '\n') { JG(2); (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'n'; p++; }
        else if (c == '\r') { JG(2); (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'r'; p++; }
        else if (c == '\b') { JG(2); (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'b'; p++; }
        else if (c == '\f') { JG(2); (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'f'; p++; }
        else if (c < 0x20 || c == 0x7F || c == '\'' || c == '+' || c == '<' || c == '>' || c == '&' || c == '`')
        {
            JG(6);
            (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'u'; (*buf)[(*pos)++] = '0'; (*buf)[(*pos)++] = '0';
            (*buf)[(*pos)++] = H[c >> 4]; (*buf)[(*pos)++] = H[c & 15]; p++;
        }
        else if (c < 0x80) { JG(1); (*buf)[(*pos)++] = (char)c; p++; }
        else
        {
            uint32_t cp;
            if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((uint32_t)(c & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
            else if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
            else if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
            else { cp = 0xFFFD; p++; }
            if (cp < 0x10000) {
                JG(6);
                (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'u';
                (*buf)[(*pos)++] = H[(cp >> 12) & 15]; (*buf)[(*pos)++] = H[(cp >> 8) & 15];
                (*buf)[(*pos)++] = H[(cp >> 4) & 15]; (*buf)[(*pos)++] = H[cp & 15];
            } else {
                cp -= 0x10000;
                unsigned hi = 0xD800 + (cp >> 10), lo = 0xDC00 + (cp & 0x3FF);
                JG(12);
                (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'u';
                (*buf)[(*pos)++] = H[(hi >> 12) & 15]; (*buf)[(*pos)++] = H[(hi >> 8) & 15];
                (*buf)[(*pos)++] = H[(hi >> 4) & 15]; (*buf)[(*pos)++] = H[hi & 15];
                (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = 'u';
                (*buf)[(*pos)++] = H[(lo >> 12) & 15]; (*buf)[(*pos)++] = H[(lo >> 8) & 15];
                (*buf)[(*pos)++] = H[(lo >> 4) & 15]; (*buf)[(*pos)++] = H[lo & 15];
            }
        }
    }
    JG(1); (*buf)[(*pos)++] = '"';
    #undef JG
}

static void gmb_json_append_val(gmb_val_t v, char** buf, size_t* cap, size_t* pos)
{
    if (v.kind == GMB_TYPE_STRING && v.v_string.data)
    {
        gmb_json_put_escaped(v.v_string.data, buf, cap, pos);
    }
    else if (v.kind == GMB_TYPE_DATETIME)
    {
        /* Canone datetime: in JSON una stringa ISO-8601 "yyyy-MM-ddTHH:mm:ss" (parità IL/PCode
         * NormalizeForJson). Il valore e' un OADate in v_double. Nessun char da escapare. */
        gmb_string_t iso = gmb_dt_datetime_iso(v.v_double);
        const char* sp = (iso.data && *iso.data) ? iso.data : "";
        size_t n = strlen(sp);
        while (*pos + n + 3 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        (*buf)[(*pos)++] = '"';
        memcpy(*buf + *pos, sp, n); *pos += n;
        (*buf)[(*pos)++] = '"';
    }
    else if (v.kind == GMB_TYPE_INT)
    {
        char tmp[32]; int n = snprintf(tmp, sizeof(tmp), "%lld", (long long)v.v_int);
        while (*pos + n + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *pos, tmp, n); *pos += n;
    }
    else if (v.kind == GMB_TYPE_DOUBLE)
    {
        char tmp[32]; int n;
        /* Shortest round-trip (canone System.Text.Json.JsonSerializer, usato da
         * JSON.Stringify dell'IL): la %.15g troncava (0.35354570729799417 → 0.353545707297994).
         * Prova precisioni crescenti e ferma alla PRIMA che, ri-parsata, dà lo stesso double.
         * Per i valori INTERI (10.0, 1.0, 100.0) usa %lld: `%.1g` sceglierebbe "1e+01" (che
         * round-trippa ma NON è la forma di .NET), mentre .NET stampa "10". */
        double dd = v.v_double;
        if (dd == (double)(long long)dd && fabs(dd) < 1e15)
            n = snprintf(tmp, sizeof(tmp), "%lld", (long long)dd);
        else
            for (int _p = 1; _p <= 17; _p++) {
                n = snprintf(tmp, sizeof(tmp), "%.*g", _p, dd);
                if (strtod(tmp, NULL) == dd) break;
            }
        while (*pos + n + 1 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *pos, tmp, n); *pos += n;
    }
    else if (v.kind == GMB_TYPE_BOOL)
    {
        while (*pos + 5 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        const char* s = v.v_bool ? "true" : "false";
        size_t n = strlen(s);
        memcpy(*buf + *pos, s, n); *pos += n;
    }
    else if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
    {
        /* lista o dict boxed. DUE convenzioni di box (F2, §0bis — unica serializzazione):
         * 1) transpiler/C99: kind=0, type_name "gmb_list"/"gmb_dict..." e data = lista/dict grezzo;
         * 2) VM/C: kind GMB_OBJ_LIST/GMB_OBJ_DICT (enum unificato in gmb_runtime.h) e
         *    data = lista/dict (gmb_shared_list_new/gmb_shared_dict_new lo impostano). */
        gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
        if (o->kind == GMB_OBJ_DICT)
        {
            gmb_json_append_dict((gmb_dict_t*)o->data, buf, cap, pos);
            return;
        }
        if (o->kind == GMB_OBJ_LIST)
        {
            gmb_json_append_list((gmb_list_t*)o->data, buf, cap, pos);
            return;
        }
        if (o->type_name && strstr(o->type_name, "dict"))
        {
            gmb_dict_t* d = (gmb_dict_t*)o->data;
            gmb_json_append_dict(d, buf, cap, pos);
            return;
        }
        /* Lista boxata col type_name "gmb_list" (convenzione gmb_make_val_list/C99): senza
         * questo ramo cadeva nel fallback "lista di byte" → JSON SAVE di una lista produceva
         * spazzatura (variante `JSON SAVE lista, file$`). */
        if (o->type_name && strstr(o->type_name, "list"))
        {
            gmb_json_append_list((gmb_list_t*)o->data, buf, cap, pos);
            return;
        }
        /* UDT boxed (type_name registrato con gmb_type_register_udt/gmb_type_udt_field):
         * serializza come OGGETTO JSON col layout registrato. Senza questo ramo un UDT
         * cadeva nel ramo "lista" sotto e veniva serializzato come array di byte (es. un
         * campo STRING*50 → 50 null): bug reale (JSON.TOJSON(udt) sul C99/VM-C). */
        {
            const char* f0name = NULL; long f0off = 0; int f0ty = 0;
            if (o->type_name && o->data
                && gmb_type_udt_first_field(o->type_name, &f0name, &f0off, &f0ty)) {
                gmb_dict_t* ud = gmb_type_udt_to_dict(o->type_name, (const uint8_t*)o->data);
                if (ud) { gmb_json_append_dict(ud, buf, cap, pos); gmb_dict_free(ud); free(ud); return; }
            }
        }
        /* lista */
        gmb_list_t* l = (gmb_list_t*)o->data;
        gmb_json_append_list(l, buf, cap, pos);
        return;
    }
    else
    {
        while (*pos + 4 >= *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *pos, "null", 4); *pos += 4;
    }
}

/* Walk del path su un JSON: ritorna la val TIPIZZATA (stringa/numero/dict/lista).
 * Unica implementazione del walk; gmb_json_extract è la forma testuale (per gli usi che
 * vogliono una stringa), gmb_json_extract_val quella tipizzata (PRINT: un numero conserva
 * la zona, parità IL/PCode/VM-C). */
/* Cerca la chiave (key,klen) nell'oggetto che inizia su `p` e ritorna il puntatore al
 * VALORE, saltando le coppie che non interessano (zero allocazioni). */
static const char* jp_object_get(const char* p, const char* key, size_t klen) {
    p = jp_ws(p);
    if (*p != '{') return NULL;
    p = jp_ws(p + 1);
    while (*p && *p != '}') {
        if (*p != '"') return NULL;
        int esc = 0;
        const char* ks = p + 1;
        const char* ke = jp_str_end(p, &esc);
        if (*ke != '"') return NULL;
        int match;
        if (!esc) match = ((size_t)(ke - ks) == klen && memcmp(ks, key, klen) == 0);
        else {
            char buf[512];
            size_t n = jp_str_decode(ks, ke, buf, sizeof buf);
            match = (n == klen && memcmp(buf, key, klen) == 0);
        }
        p = jp_ws(ke + 1);
        if (*p == ':') p = jp_ws(p + 1);
        if (match) return p;
        p = jp_skip_value(p, 0);
        p = jp_ws(p);
        if (*p == ',') { p = jp_ws(p + 1); continue; }
        return NULL;
    }
    return NULL;
}

/* Elemento idx dell'array che inizia su `p` (zero allocazioni). */
static const char* jp_array_item(const char* p, int idx) {
    p = jp_ws(p);
    if (*p != '[') return NULL;
    p = jp_ws(p + 1);
    if (*p == ']') return NULL;
    int i = 0;
    while (*p) {
        if (i == idx) return p;
        p = jp_skip_value(p, 0);
        p = jp_ws(p);
        if (*p == ',') { p = jp_ws(p + 1); i++; continue; }
        return NULL;
    }
    return NULL;
}

/* Estrae il valore al path SENZA costruire l'albero (modello `fastjson.c` di Redis): si
 * scansiona il testo saltando i rami non richiesti e si materializza SOLO il valore finale.
 * Semantica del path invariata rispetto alla versione parse-then-walk: `a.b[2].c`, `[0].x`,
 * chiave assente o indice fuori range → nil.
 * Prima: `gmb_json_parse(js)` + walk (costruiva tutto: su un oggetto piatto da 1000 chiavi
 * ~940 µs per estrarre UNA chiave). */
gmb_val_t gmb_json_extract_val(const char* js, const char* path)
{
    if (!js || !path) return gmb_val_nil();
    const char* p = jp_ws(js);
    const char* s = path;
    while (*s) {
        char key[512];
        size_t kl = 0;
        while (*s && *s != '.' && *s != '[' && kl < sizeof(key) - 1) key[kl++] = *s++;
        key[kl] = '\0';
        int idx = -1;
        if (*s == '[') {
            const char* b = s + 1;
            idx = (int)strtol(b, NULL, 10);
            while (*s && *s != ']') s++;
            if (*s == ']') s++;
        }
        if (kl > 0) {
            p = jp_object_get(p, key, kl);
            if (!p) return gmb_val_nil();
        } else if (idx < 0) {
            return gmb_val_nil();               /* segmento vuoto senza indice: path invalido */
        }
        if (idx >= 0) {
            p = jp_array_item(p, idx);
            if (!p) return gmb_val_nil();
        }
        if (*s == '.') s++;
    }
    return jp_value(&p, 0);
}

char* gmb_json_extract(const char* js, const char* path)
{
    gmb_val_t v = gmb_json_extract_val(js, path);
    if (v.kind == GMB_TYPE_NIL) return strdup("");
    if (v.kind == GMB_TYPE_STRING) return strdup(v.v_string.data ? v.v_string.data : "");
    gmb_string_t s = gmb_val_to_json(v);
    return strdup(s.data);
}

gmb_string_t gmb_val_to_json(gmb_val_t v) {
    size_t cap = 256;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("null");
    size_t pos = 0;
    gmb_json_append_val(v, &buf, &cap, &pos);
    buf[pos] = '\0';
    gmb_string_t s = gmb_string_alloc(buf);
    free(buf);
    return s;
}

/* Escape stringa JSON (parità System.Text.Json default, da evidenza: " \' \\ C0,
 * '→\u0027, `→\u0060, <>&→\uXXXX, non-ASCII→\uXXXX con surrogate). Ritorna byte scritti. */
size_t gmb_json_escape(char* o, size_t cap, const char* s) {
    size_t w = 0;
    static const char* hex = "0123456789abcdef";
    if (!o || !s) return 0;
    for (const unsigned char* p = (const unsigned char*)s; *p && w + 12 < cap; ) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { o[w++] = '\\'; o[w++] = (char)c; p++; }
        else if (c == '\n') { o[w++] = '\\'; o[w++] = 'n'; p++; }
        else if (c == '\r') { o[w++] = '\\'; o[w++] = 'r'; p++; }
        else if (c == '\t') { o[w++] = '\\'; o[w++] = 't'; p++; }
        else if (c == '\b') { o[w++] = '\\'; o[w++] = 'b'; p++; }
        else if (c == '\f') { o[w++] = '\\'; o[w++] = 'f'; p++; }
        else if (c < 0x20) { o[w++] = '\\'; o[w++] = 'u'; o[w++] = '0'; o[w++] = '0'; o[w++] = hex[c >> 4]; o[w++] = hex[c & 15]; p++; }
        else if (c == '\'') { memcpy(o + w, "\\u0027", 6); w += 6; p++; }
        else if (c == '`') { memcpy(o + w, "\\u0060", 6); w += 6; p++; }
        else if (c == '<') { memcpy(o + w, "\\u003c", 6); w += 6; p++; }
        else if (c == '>') { memcpy(o + w, "\\u003e", 6); w += 6; p++; }
        else if (c == '&') { memcpy(o + w, "\\u0026", 6); w += 6; p++; }
        else if (c < 0x80) { o[w++] = (char)c; p++; }
        else {
            uint32_t cp;
            if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((uint32_t)(c & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
            else if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
            else if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4; }
            else { cp = 0xFFFD; p++; }
            if (cp < 0x10000) {
                o[w++] = '\\'; o[w++] = 'u';
                o[w++] = hex[(cp >> 12) & 15]; o[w++] = hex[(cp >> 8) & 15];
                o[w++] = hex[(cp >> 4) & 15]; o[w++] = hex[cp & 15];
            } else {
                cp -= 0x10000;
                unsigned hi = 0xD800 + (cp >> 10), lo = 0xDC00 + (cp & 0x3FF);
                o[w++] = '\\'; o[w++] = 'u';
                o[w++] = hex[(hi >> 12) & 15]; o[w++] = hex[(hi >> 8) & 15];
                o[w++] = hex[(hi >> 4) & 15]; o[w++] = hex[hi & 15];
                o[w++] = '\\'; o[w++] = 'u';
                o[w++] = hex[(lo >> 12) & 15]; o[w++] = hex[(lo >> 8) & 15];
                o[w++] = hex[(lo >> 4) & 15]; o[w++] = hex[lo & 15];
            }
        }
    }
    return w;
}

/* Float shortest-round-trip (parità System.Text.Json Single). Ritorna char scritti. */
int gmb_json_float_str(char* out, size_t cap, double v) {
    float sf = (float)v;
    if (sf == sf) {
        for (int pr = 1; pr <= 9; pr++) {
            int w = snprintf(out, cap, "%.*g", pr, (double)sf);
            if (w > 0 && strtof(out, NULL) == sf) return w;
        }
    }
    return snprintf(out, cap, "%.9g", (double)sf);
}

/* Testo JSON di un val: una STRINGA è già testo JSON (il contenuto, MAI virgolettato);
 * per gli altri tipi serializza (dict/lista/UDT → oggetto/array, scalari → numero). */
gmb_string_t gmb_val_json_text(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING) return v.v_string.data ? gmb_string_alloc(v.v_string.data) : gmb_string_alloc("");
    return gmb_val_to_json(v);
}

/* Display di un val per PRINT (parità IL PrintFormatValueCore / VM-C gmb_format_value):
 * dict → [[k, v], ...]; list → [e1, e2, ...]; scalari → testo. Diverso da gmb_val_to_json
 * (che serializza in JSON): serve al C99/C99Native per stampare un dict come i backend
 * gestiti (es. RGB/SCREENINFO). */
static void gmb_display_append(gmb_val_t v, char** buf, size_t* cap, size_t* pos)
{
    char t[64];
    int n = 0;
    if (v.kind == GMB_TYPE_STRING)
    {
        const char* s = v.v_string.data ? v.v_string.data : "";
        size_t l = strlen(s);
        if (*pos + l + 1 > *cap) { *cap = (*pos + l + 1) * 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *pos, s, l); *pos += l; return;
    }
    if (v.kind == GMB_TYPE_INT) n = snprintf(t, sizeof(t), "%lld", (long long)v.v_int);
    else if (v.kind == GMB_TYPE_BOOL) n = snprintf(t, sizeof(t), "%s", v.v_bool ? "-1" : "0");
    else if (v.kind == GMB_TYPE_DOUBLE)
    {
        double d = v.v_double;
        if (d == 0.0) d = 0.0;   /* -0.0 -> "0" (parita con .NET/sample) */
        n = (d == (double)(long long)d) ? snprintf(t, sizeof(t), "%lld", (long long)d)
                                        : snprintf(t, sizeof(t), "%.15g", d);
    }
    else if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
    {
        gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
        const char* tn = o->type_name ? o->type_name : "";
        if (strstr(tn, "dict"))
        {
            gmb_dict_t dd = gmb_val_as_dict(v);
            const char* open = "["; if (*pos + 1 > *cap) { *cap = (*pos + 2) * 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = open[0];
            for (size_t i = 0; i < dd.count; i++)
            {
                if (i) { if (*pos + 2 > *cap) { *cap = (*pos + 3) * 2; *buf = (char*)realloc(*buf, *cap); } memcpy(*buf + *pos, ", ", 2); *pos += 2; }
                if (*pos + 1 > *cap) { *cap = (*pos + 2) * 2; *buf = (char*)realloc(*buf, *cap); }
                (*buf)[(*pos)++] = '[';
                gmb_display_append(gmb_make_val_string(dd.items[i].key), buf, cap, pos);
                if (*pos + 2 > *cap) { *cap = (*pos + 3) * 2; *buf = (char*)realloc(*buf, *cap); }
                memcpy(*buf + *pos, ", ", 2); *pos += 2;
                gmb_display_append(dd.items[i].value, buf, cap, pos);
                if (*pos + 1 > *cap) { *cap = (*pos + 2) * 2; *buf = (char*)realloc(*buf, *cap); }
                (*buf)[(*pos)++] = ']';
            }
            if (*pos + 1 > *cap) { *cap = (*pos + 2) * 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = ']';
            return;
        }
        if (strstr(tn, "list"))
        {
            gmb_list_t ll = gmb_val_as_list(v);
            if (*pos + 1 > *cap) { *cap = (*pos + 2) * 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = '[';
            for (size_t i = 0; i < ll.count; i++)
            {
                if (i) { if (*pos + 2 > *cap) { *cap = (*pos + 3) * 2; *buf = (char*)realloc(*buf, *cap); } memcpy(*buf + *pos, ", ", 2); *pos += 2; }
                gmb_display_append(ll.items[i], buf, cap, pos);
            }
            if (*pos + 1 > *cap) { *cap = (*pos + 2) * 2; *buf = (char*)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = ']';
            return;
        }
        return;
    }
    if (n > 0)
    {
        if (*pos + (size_t)n + 1 > *cap) { *cap = (*pos + (size_t)n + 1) * 2; *buf = (char*)realloc(*buf, *cap); }
        memcpy(*buf + *pos, t, (size_t)n); *pos += (size_t)n;
    }
}

gmb_string_t gmb_val_to_display(gmb_val_t v) {
    size_t cap = 256, pos = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("");
    gmb_display_append(v, &buf, &cap, &pos);
    buf[pos] = '\0';
    gmb_string_t s = gmb_string_alloc(buf);
    free(buf);
    return s;
}

/* JSON.Stringify su array NATIVI C99 (elementi gmb_double_t/gmb_string_t): serializza come
 * array JSON. Parità PCodeVM/IL (System.Text.Json su double[]/string[]) — es. C99Native
 * FunctionsLambda.bas `JSON.Stringify(pari)` (array da AFILTER/AMAP). I numeri usano lo
 * stesso formato di gmb_double_to_string (%.15g: 1.0 → "1", nessuno spazio). */
gmb_string_t gmb_json_double_array(const gmb_double_t* items, gmb_int_t n) {
    if (!items || n < 0) return gmb_string_alloc("[]");
    size_t cap = 64, pos = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("[]");
    buf[pos++] = '[';
    for (gmb_int_t i = 0; i < n; i++) {
        if (i > 0) {
            while (pos + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            buf[pos++] = ',';
        }
        gmb_string_t t = gmb_double_to_string(items[i]);
        const char* td = t.data ? t.data : "";
        size_t tl = strlen(td);
        while (pos + tl + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        memcpy(buf + pos, td, tl); pos += tl;
    }
    while (pos + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
    buf[pos++] = ']';
    buf[pos] = '\0';
    gmb_string_t s = gmb_string_alloc(buf);
    free(buf);
    return s;
}

/* Array nativo di stringhe: ogni elemento virgolettato (senza escape, stesso limite del
 * writer gmb_val_to_json — gli escape interni dei caratteri speciali restano un limite
 * noto del runtime, vedi gmb_json_append_val/GMB_TYPE_STRING). */
/* Elemento di un report LEDGER reso come stringa per l'array INTO: se il valore è una
 * STRINGA (ACCOUNTS: nome conto) → il valore grezzo "Attivita:Banca"; se è un OGGETTO
 * (REGISTER/GET/BALANCE: riga JSON) → il testo JSON. Parità C# (`string[]` di nomi per
 * ACCOUNTS, stringhe-JSON per gli altri). */
gmb_string_t gmb_val_ledger_item(gmb_val_t v) {
    if (v.kind == GMB_TYPE_STRING && v.v_string.data) return gmb_string_alloc(v.v_string.data);
    return gmb_val_to_json(v);
}

gmb_string_t gmb_json_string_array(const gmb_string_t* items, gmb_int_t n) {
    if (!items || n < 0) return gmb_string_alloc("[]");
    size_t cap = 64, pos = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("[]");
    buf[pos++] = '[';
    for (gmb_int_t i = 0; i < n; i++) {
        if (i > 0) {
            while (pos + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            buf[pos++] = ',';
        }
        while (pos + 2 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        buf[pos++] = '"';
        const char* p = items[i].data ? items[i].data : "";
        for (; *p; p++) {
            while (pos + 8 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            /* Escapa `"`→\u0022 e `\`→\\ (parità System.Text.Json/C# JSON.Stringify): senza,
             * una lista di stringhe-JSON (report LEDGER) usciva con le virgolette NON escapate
             * (["{"TransactionId":...] vs ["{\u0022...\u0022}]). § DbLedgerFullTests c99n. */
            if (*p == '"') { memcpy(buf + pos, "\\u0022", 6); pos += 6; }
            else if (*p == '\\') { buf[pos++] = '\\'; buf[pos++] = '\\'; }
            else buf[pos++] = *p;
        }
        buf[pos++] = '"';
    }
    while (pos + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
    buf[pos++] = ']';
    buf[pos] = '\0';
    gmb_string_t s = gmb_string_alloc(buf);
    free(buf);
    return s;
}

/* Serializza un gmb_dict_t* GREZZO come oggetto JSON (record intero, tutti i campi) —
 * parità PCodeVM SpeedRecordToBytes (common/IlBackendRuntime/PCode/PCodeProcessor.Helpers.cs):
 * JsonSerializer.Serialize(Dictionary<string,object?>) sull'INTERO record, non un CSV di un
 * solo campo. Entry point condiviso per VM/C e C99 (SPEED PUT/GET/SEEK/SELECT) — vedi
 * DB-Parity-1-1-TODO.md §3 "SPEED data round-trip". */
gmb_string_t gmb_dict_to_json(gmb_dict_t* d) {
    size_t cap = 256;
    char* buf = (char*)malloc(cap);
    if (!buf) return gmb_string_alloc("null");
    size_t pos = 0;
    gmb_json_append_dict(d, &buf, &cap, &pos);
    buf[pos] = '\0';
    gmb_string_t s = gmb_string_alloc(buf);
    free(buf);
    return s;
}

/* Encoding order-preserving big-endian di un double per chiavi SPEED ordinabili byte-a-byte
 * (parità PCodeVM SpeedSortableDoubleBytes, PCodeProcessor.Helpers.cs): il bit pattern
 * IEEE754 di un double NON preserva l'ordinamento numerico sotto confronto lessicografico
 * di byte grezzi — servono (a) l'inversione del bit di segno per i positivi (li sposta
 * sopra i negativi nell'ordinamento unsigned) o il complemento a uno per i negativi
 * (inverte l'ordinamento, che altrimenti sarebbe rovesciato), e (b) byte MSB-first (il
 * motore SPEED confronta le chiavi byte-a-byte in ordine, non come intero little-endian).
 * `out` deve avere almeno 8 byte. */
void gmb_speed_sortable_double_bytes(gmb_double_t d, uint8_t* out) {
    int64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    bits = (bits >= 0) ? (bits ^ INT64_MIN) : ~bits;
    for (int i = 0; i < 8; i++)
        out[i] = (uint8_t)((uint64_t)bits >> (8 * (7 - i)));
}

/* TUPLE (a, b, ...): boxa gli elementi in un gmb_val_t dict con chiavi "0","1",...
 * (accesso tup(i) → gmb_dict_get con chiave stringa dell'indice). */
gmb_val_t gmb_val_tuple(const gmb_val_t* items, gmb_int_t n) {
    gmb_dict_t d = gmb_dict_new();
    for (gmb_int_t i = 0; i < n; i++) {
        char k[16];
        snprintf(k, sizeof(k), "%lld", (long long)i);
        gmb_dict_set(&d, k, items[i]);
    }
    return gmb_make_val_dict_take(&d);
}

/* Lunghezza di un gmb_val_t: stringa → caratteri; lista → elementi; dict → chiavi; default 0. */
gmb_int_t gmb_val_length(gmb_val_t v)
{
    /* Stringa: lunghezza in UNITÀ UTF-16 (come `string.Length` C#), non in byte. */
    if (v.kind == GMB_TYPE_STRING) return gmb_chars_len(v.v_string.data);
    if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
    {
        gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
        /* DIZIONARIO (JSON/dict boxed): LEN = numero di chiavi (parita' IL/PCode/VM-C). Prima l'oggetto veniva reinterpretato come
         * gmb_list_t* e si leggeva un `count` a caso (o `.v_string.length` di una val non stringa: spazzatura). */
        if (o->data && o->type_name && strstr(o->type_name, "dict"))
            return (gmb_int_t)((gmb_dict_t*)o->data)->count;
        if (o->data)
        {
            gmb_list_t* l = (gmb_list_t*)o->data;
            return (gmb_int_t)l->count;
        }
    }
    return 0;
}

/* RGB(r,g,b[,a]) → dict RGBA {Red,Green,Blue,Alpha} — condivisa VM/C e C99. */
static double rgb_clamp(double v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

gmb_val_t gmb_rgb(double r, double g, double b, double a) {
    gmb_dict_t d = gmb_dict_new();
    gmb_dict_set(&d, "Red", gmb_make_val_double(rgb_clamp(r)));
    gmb_dict_set(&d, "Green", gmb_make_val_double(rgb_clamp(g)));
    gmb_dict_set(&d, "Blue", gmb_make_val_double(rgb_clamp(b)));
    gmb_dict_set(&d, "Alpha", gmb_make_val_double(rgb_clamp(a)));
    return gmb_make_val_dict_take(&d);
}

void gmb_rgb_fill(gmb_dict_t* d, double r, double g, double b, double a) {
    if (!d) return;
    gmb_dict_set(d, "Red", gmb_make_val_double(rgb_clamp(r)));
    gmb_dict_set(d, "Green", gmb_make_val_double(rgb_clamp(g)));
    gmb_dict_set(d, "Blue", gmb_make_val_double(rgb_clamp(b)));
    gmb_dict_set(d, "Alpha", gmb_make_val_double(rgb_clamp(a)));
}

int gmb_rgb_to_int(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_OBJECT)
    {
        gmb_dict_t d = gmb_val_as_dict(v);
        double r = gmb_val_to_double(gmb_dict_get(d, "Red"));
        double g = gmb_val_to_double(gmb_dict_get(d, "Green"));
        double b = gmb_val_to_double(gmb_dict_get(d, "Blue"));
        return ((int)r << 16) | ((int)g << 8) | (int)b;
    }
    return 0;
}

/* Colore da un valore DINAMICO (RGBA dict, oppure scalare/stringa numerica):
 * dict {Red,Green,Blue} → 0x40000000|RGB (RGB diretto per resolve_color), altrimenti
 * il valore numerico (indice palette). Usato da COLOR quando l'operando è gmb_val_t
 * (parità C99Native/VM-C). */
int gmb_color_from_val(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_OBJECT) return gmb_rgb_to_line_color(v);
    return gmb_val_to_int(v);
}

/* Colore per primitive grafiche: 0x40000000|RGB — formato atteso da gmb_gfx_* /
 * gmb_gfx_color (resolve_color). gmb_rgb_to_int (0xRRGGBB, senza flag) sarebbe
 * interpretato come indice palette. Parità IlRuntime.RgbaToLineColor. */
int gmb_rgb_to_line_color(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_OBJECT)
    {
        gmb_dict_t d = gmb_val_as_dict(v);
        double r = gmb_val_to_double(gmb_dict_get(d, "Red"));
        double g = gmb_val_to_double(gmb_dict_get(d, "Green"));
        double b = gmb_val_to_double(gmb_dict_get(d, "Blue"));
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        return 0x40000000 | ((int)r << 16) | ((int)g << 8) | (int)b;
    }
    return 0x40000000;
}

/* ANSI true-color solo su tty reale (o quando non headless). Su stdout redirect/pipe
 * (test, CI, capture) si restituisce il testo senza escape (parita' headless). */
static int gmb_ansi_tty_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char* h = getenv("GMBASIC_HEADLESS");
        cached = (h && h[0] == '1') ? 0 : (isatty(1) ? 1 : 0);
    }
    return cached;
}
static char* gmb_ansi_color(const char* text, int mode, int r, int g, int b)
{
    const char* t = text ? text : "";
    if (!gmb_ansi_tty_enabled()) {
        char* plain = (char*)malloc(strlen(t) + 1);
        if (plain) memcpy(plain, t, strlen(t) + 1);
        return plain;
    }
    char* out = (char*)malloc(strlen(t) + 64);
    if (!out) return NULL;
    snprintf(out, strlen(t) + 64, "\x1b[%d;2;%d;%d;%dm%s\x1b[0m", mode, r, g, b, t);
    return out;
}
char* gmb_ansi_fg(const char* text, int r, int g, int b) { return gmb_ansi_color(text, 38, r, g, b); }
char* gmb_ansi_bg(const char* text, int r, int g, int b) { return gmb_ansi_color(text, 48, r, g, b); }

/* Lettura numerica: NAN se la chiave non esiste (safe-access/IS NIL). */
gmb_double_t gmb_dict_get_or_nan(gmb_dict_t d, const char* key) {
    gmb_pair_t* p = gmb_dict_find(&d, key);
    if (!p) return (gmb_double_t)(0.0 / 0.0);
    gmb_val_t v = p->value;
    if (v.kind == GMB_TYPE_DOUBLE) return v.v_double;
    if (v.kind == GMB_TYPE_INT) return (gmb_double_t)v.v_int;
    return (gmb_double_t)(0.0 / 0.0);
}

/* ── Dominio JSON tipizzato (DIM x AS JSON) ── */
static int gmb_json_is_list(gmb_val_t v) {
    const char* t;
    /* Guardia PRIMA di gmb_obj_type: su un val NON-oggetto (nil/numero/stringa)
     * gmb_obj_type dereferenzia un puntatore non valido (EXC_BAD_ACCESS @0x10). */
    if (v.kind != GMB_TYPE_OBJECT) return 0;
    t = gmb_obj_type(v);
    return t && strstr(t, "list");
}
static gmb_val_t gmb_json_nav(gmb_val_t root, const char* path, int* ok) {
    *ok = 1;
    gmb_val_t cur = root;
    /* `DIM x AS JSON/DECISION` può restare una STRINGA (parità IL/PCode con l'assegnazione
     * non ancora parsata): il member access deve parsarla on-access, come fa
     * `gmb_val_as_dict`. Senza questo, `x.Campo` numerico funzionava (gmb_val_as_dict) ma il
     * confronto testuale `x.Campo = "true"` leggeva "" (gmb_json_path_text) → divergenza C99. */
    if (cur.kind == GMB_TYPE_STRING && cur.v_string.data && cur.v_string.data[0] == '{') {
        gmb_val_t pv = gmb_json_parse(cur.v_string.data);
        if (pv.kind == GMB_TYPE_OBJECT) cur = pv;
    }
    char seg[256];
    const char* p = path;
    while (*p) {
        size_t si = 0;
        while (*p && *p != '.' && si < 255) seg[si++] = *p++;
        if (*p == '.') p++;
        seg[si] = 0;
        char key[200]; int ki = 0;
        for (const char* q = seg; *q && *q != '[' && ki < 199; q++) key[ki++] = *q;
        key[ki] = 0;
        if (cur.kind == GMB_TYPE_OBJECT && *key) {
            cur = gmb_val_dict_get(cur, key);
        } else {
            *ok = 0; return gmb_val_nil();
        }
        char* br = strchr(seg, '[');
        if (br) {
            int idx = atoi(br + 1);
            if (!gmb_json_is_list(cur)) { *ok = 0; return gmb_val_nil(); }
            gmb_list_t l = gmb_val_as_list(cur);
            if (idx < 0 || (size_t)idx >= l.count) { *ok = 0; return gmb_val_nil(); }
            cur = l.items[idx];
        }
    }
    return cur;
}

gmb_string_t gmb_json_path_text(gmb_val_t root, const char* path) {
    int ok = 0;
    gmb_val_t v = gmb_json_nav(root, path, &ok);
    if (!ok) return gmb_string_alloc("");
    if (v.kind == GMB_TYPE_STRING) return gmb_string_alloc(v.v_string.data ? v.v_string.data : "");
    if (v.kind == GMB_TYPE_DOUBLE) return gmb_double_to_string(v.v_double);
    if (v.kind == GMB_TYPE_INT) return gmb_double_to_string((gmb_double_t)v.v_int);
    if (v.kind == GMB_TYPE_BOOL) return gmb_string_alloc(v.v_bool ? "True" : "False");
    return gmb_val_to_json(v);
}

/* Scrittura per percorso (es. "Headers.ContentType"): scende/crea i dict intermedi
 * (parità con la lettura gmb_json_nav e con SetDictPathValue di PCodeVM/IL). I percorsi
 * mono-segmento (write TypesJson top-level) mantengono il comportamento precedente. */
void gmb_json_path_set(gmb_val_t* root, const char* path, gmb_val_t value) {
    if (!root || !path) return;
    gmb_val_t cur = *root;
    for (;;) {
        char seg[256]; size_t si = 0;
        const char* p = path;
        while (*p && *p != '.' && si < 255) seg[si++] = *p++;
        if (*p == '.') p++;
        seg[si] = 0;
        path = p;
        char key[200]; int ki = 0;
        for (const char* q = seg; *q && *q != '[' && ki < 199; q++) key[ki++] = *q;
        key[ki] = 0;
        if (!*key) return;
        if (!*path) { gmb_val_dict_set(cur, key, value); return; }
        gmb_val_t nxt = gmb_val_dict_get(cur, key);
        if (nxt.kind != GMB_TYPE_OBJECT) {
            nxt = gmb_make_val_dict(gmb_dict_new());
            gmb_val_dict_set(cur, key, nxt);
        }
        cur = nxt;
    }
}

gmb_val_t gmb_json_load_file(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return gmb_val_nil();
    char* buf = NULL; long len = 0;
    if (fseek(fp, 0, SEEK_END) == 0) { len = ftell(fp); fseek(fp, 0, SEEK_SET); }
    if (len > 0) { buf = (char*)malloc((size_t)len + 1); if (buf && fread(buf, 1, (size_t)len, fp) == (size_t)len) buf[len] = 0; }
    fclose(fp);
    gmb_val_t v = gmb_val_nil();
    if (buf) { v = gmb_json_parse(buf); free(buf); }
    return v;
}

void gmb_json_save_val(gmb_val_t v, const char* path) {
    FILE* fp = fopen(path, "wb");
    if (!fp) return;
    gmb_string_t s = gmb_val_to_json(v);
    if (s.data) fputs(s.data, fp);
    fclose(fp);
}

/* Accesso MEMBRO di un elemento: `base(idx).member` (base = LISTA JSON) — con fallback a
 * `base.member(idx)` (base = DICT con un membro-array). Il parser fonde ENTRAMBE le forme
 * nella stessa CALL("base.member", [idx]) (l'ordine reale è perso a compile-time): qui si
 * decide a RUNTIME in base al tipo di `root`. */
gmb_string_t gmb_json_index_member_text(gmb_val_t root, gmb_int_t idx, const char* member) {
    int ok = 0;
    gmb_val_t cur;
    if (root.kind == GMB_TYPE_OBJECT && gmb_json_is_list(root)) {
        gmb_list_t l = gmb_val_as_list(root);
        if (idx < 0 || (size_t)idx >= l.count) return gmb_string_alloc("");
        cur = gmb_json_nav(l.items[idx], member, &ok);
        if (!ok) return gmb_string_alloc("");
    } else {
        cur = gmb_json_nav(root, member, &ok);
        if (!ok) return gmb_string_alloc("");
        if (cur.kind == GMB_TYPE_OBJECT && gmb_json_is_list(cur)) {
            gmb_list_t l = gmb_val_as_list(cur);
            if (idx < 0 || (size_t)idx >= l.count) return gmb_string_alloc("");
            cur = l.items[idx];
        } else return gmb_string_alloc("");
    }
    if (cur.kind == GMB_TYPE_STRING) return gmb_string_alloc(cur.v_string.data ? cur.v_string.data : "");
    if (cur.kind == GMB_TYPE_DOUBLE) return gmb_double_to_string(cur.v_double);
    if (cur.kind == GMB_TYPE_INT) return gmb_double_to_string((gmb_double_t)cur.v_int);
    if (cur.kind == GMB_TYPE_BOOL) return gmb_string_alloc(cur.v_bool ? "True" : "False");
    return gmb_val_to_json(cur);
}

/* Variante RAW di gmb_json_index_member_text: ritorna il gmb_val_t (non il testo) per
 * il consumo tipizzato (gmb_cstr_val/gmb_val_to_double) senza perdere il kind. */
gmb_val_t gmb_json_index_member_val(gmb_val_t root, gmb_int_t idx, const char* member) {
    int ok = 0;
    gmb_val_t cur;
    if (root.kind == GMB_TYPE_OBJECT && gmb_json_is_list(root)) {
        gmb_list_t l = gmb_val_as_list(root);
        if (idx < 0 || (size_t)idx >= l.count) return gmb_val_nil();
        cur = gmb_json_nav(l.items[idx], member, &ok);
        if (!ok) return gmb_val_nil();
    } else {
        cur = gmb_json_nav(root, member, &ok);
        if (!ok) return gmb_val_nil();
        if (cur.kind == GMB_TYPE_OBJECT && gmb_json_is_list(cur)) {
            gmb_list_t l = gmb_val_as_list(cur);
            if (idx < 0 || (size_t)idx >= l.count) return gmb_val_nil();
            cur = l.items[idx];
        } else return gmb_val_nil();
    }
    return cur;
}

/* Lettura di un elemento di array JSON: root.member[idx] → testo. */
gmb_string_t gmb_json_path_index_text(gmb_val_t root, const char* member, gmb_int_t idx) {
    int ok = 0;
    gmb_val_t cur = gmb_json_nav(root, member, &ok);
    if (ok && cur.kind == GMB_TYPE_OBJECT && gmb_json_is_list(cur)) {
        gmb_list_t l = gmb_val_as_list(cur);
        if (idx >= 0 && (size_t)idx < l.count) {
            gmb_val_t e = l.items[idx];
            if (e.kind == GMB_TYPE_STRING) return gmb_string_alloc(e.v_string.data ? e.v_string.data : "");
            if (e.kind == GMB_TYPE_DOUBLE) return gmb_double_to_string(e.v_double);
            if (e.kind == GMB_TYPE_INT) return gmb_double_to_string((gmb_double_t)e.v_int);
            if (e.kind == GMB_TYPE_BOOL) return gmb_string_alloc(e.v_bool ? "True" : "False");
            return gmb_val_to_json(e);
        }
    }
    return gmb_string_alloc("");
}

/* Trova una chiave ignorando maiuscole/minuscole (vedi gmb_dict_get_ci nell'header). */
static gmb_pair_t* gmb_dict_find_ci(gmb_dict_t* d, const char* key) {
    for (size_t i = 0; i < d->count; i++)
        if (d->items[i].key && strcasecmp(d->items[i].key, key) == 0) return &d->items[i];
    return NULL;
}
gmb_val_t gmb_dict_get_ci(gmb_dict_t d, const char* key) {
    gmb_pair_t* p = gmb_dict_find_ci(&d, key);
    return p ? p->value : gmb_make_val_string("");
}
gmb_int_t gmb_dict_contains_key_ci(gmb_dict_t d, const char* key) {
    return gmb_dict_find_ci(&d, key) ? 1 : 0;
}
