/* ============================================================================
 * gmb_adapter.c — Implementazione dell'adapter VM/C <-> runtime condiviso.
 * ============================================================================ */
#include "gmb_adapter.h"
#include <string.h>
#include <stdlib.h>

gmb_val_t gmb_adapter_num(double n)
{
    /* il runtime transpiler distingue int/double; qui usiamo double (coerente col C#) */
    return gmb_make_val_double(n);
}

int gmb_adapter_obj_is_container(const gmb_obj_t* o)
{
    if (!o) return 0;
    if (o->kind == GMB_OBJ_LIST || o->kind == GMB_OBJ_DICT) return 1;
    if (o->type_name
        && (strcmp(o->type_name, "gmb_list") == 0 || strcmp(o->type_name, "gmb_dict") == 0))
        return 1;
    return 0;
}

/* Retain condizionale di stack_to_val. retain=1 (gmb_adapter_stack_to_val): il valore ritornato porta
 * un riferimento in piu'. retain=0 (gmb_adapter_stack_to_val_borrowed): vista in PRESTITO dell'entry,
 * valida finche' l'entry vive, da passare a un contenitore (gmb_list_add/set/insert, gmb_dict_set) che
 * trattiene da se' — evita il doppio conteggio (il ref temporaneo non verrebbe mai rilasciato).
 * Gli oggetti NON contati (release_fn NULL: box runtime) mantengono il comportamento storico (retain). */
static void stv_retain(gmb_obj_t* o, int retain)
{
    if (retain || !o->release_fn) gmb_obj_retain(o);
}

static gmb_val_t stack_to_val_impl(gmb_stack_entry_t s, int retain)
{
    if (s.type == 1)
    {
        /* Entry NUMERICO con tag BOOL (OP_PUSH_TRUE/FALSE, risultati di confronto): il valore
         * gmb_val_t deve essere BOOL, così JSON.Stringify emette `true`/`false` (canone
         * il/pcode) invece di -1. `gmb_print_val`/`gmb_double_to_string` restano "-1"/"0". */
        if (s.basic_type == GMB_BT_BOOL) return gmb_make_val_bool(s.num != 0.0 ? -1 : 0);
        /* DATETIME (NOW()/TODAY()): kind distinto -> JSON ISO-8601 (canone data/ora). */
        if (s.basic_type == GMB_BT_DATE) return gmb_make_val_datetime(s.num);
        /* CURRENCY (campo DBF N/F): numerico con scala -> resa "100.00". */
        if (s.basic_type == GMB_BT_CURRENCY) return gmb_make_val_currency(s.num, s.scale);
        return gmb_make_val_double(s.num);
    }
    if (s.type == 2 && s.ref)
    {
        /* La discriminazione passa PRIMA per i contenitori: un box runtime-wrapped ha
         * kind 0 == GMB_OBJ_STRING e senza questo ordine finiva nel ramo stringa sotto,
         * che ne leggeva ->data come char* (NON e' un gmb_obj_string_t). */
        if (gmb_adapter_obj_is_container(s.ref))
        {
            stv_retain(s.ref, retain);
            return gmb_make_val_obj(s.ref);
        }
        if (s.ref->kind == GMB_OBJ_STRING && !s.ref->type_name)
        {
            /* Stringa CONTATA (rc 0, fluttuante): add/set/insert del contenitore la trattengono e la liberano
             * con lui; se resta temporanea la scarta il chiamante (gmb_val_discard) — vedi gmb_runtime.h. */
            gmb_obj_string_t* str = (gmb_obj_string_t*)s.ref;
            return gmb_make_val_string_counted_n(str->data, str->length);
        }
        /* lista/dict VM: NON convertibili direttamente a gmb_list_t a valore —
           l'opcode che le gestisce lavora direttamente sugli oggetti VM. */
        if (s.ref->kind == GMB_OBJ_CLOSURE || s.ref->kind == GMB_OBJ_BYREF)
        {
            /* Mantiene il riferimento all'oggetto VM come gmb_val_t OBJECT (es. per liste
               annidate / ParamArray che avvolgono una lista, un handler EVENT che
               attraversa .Add()/.IndexOf() su un registro handler, o un box BYREF scritto in
               un campo __ClosureCtx via MakeByRefExpr/OP_MAKE_BYREF — piano
               Closure-ByRef-Box-Linker-Definitivo, Fase 2/3: senza questo caso il box
               diventava silenziosamente NIL in OP_STORE_MEMBER, esattamente come il bug già
               trovato per CLOSURE — vedi il commento gemello in gmb_adapter_val_to_stack).
               Il chiamante lavora direttamente su gmb_obj_shared_val_t/gmb_obj_closure_t/
               gmb_obj_byref_t via v_ptr. */
            stv_retain(s.ref, retain);
            gmb_val_t v = gmb_make_val_obj(s.ref);
            return v;
        }
    }
    return gmb_val_nil();
}


gmb_val_t gmb_adapter_stack_to_val(gmb_stack_entry_t s) { return stack_to_val_impl(s, 1); }
gmb_val_t gmb_adapter_stack_to_val_borrowed(gmb_stack_entry_t s) { return stack_to_val_impl(s, 0); }

gmb_stack_entry_t gmb_adapter_val_to_stack(gmb_val_t v)
{
    gmb_stack_entry_t s;
    memset(&s, 0, sizeof(s));
    switch (v.kind)
    {
        case GMB_TYPE_INT:
            s.type = 1; s.num = (double)v.v_int; s.basic_type = GMB_BT_NUMBER; break;
        case GMB_TYPE_DOUBLE:
            s.type = 1; s.num = v.v_double; s.basic_type = GMB_BT_NUMBER; break;
        case GMB_TYPE_CURRENCY:
            /* Numerico con scala (campo DBF N/F): sullo stack è numerico; la scala è
             * trasportata sul campo `scale` dell'entry (STAGE B, resa "100.00"). */
            s.type = 1; s.num = v.v_double; s.basic_type = GMB_BT_CURRENCY; s.scale = (uint8_t)v.scale; break;
        case GMB_TYPE_DATETIME:
            /* DATETIME (DATEVALUE/NOW/campo data in un array o dict): l'entry deve restare
             * NUMERICA con tag GMB_BT_DATE, altrimenti cadeva nel default e tornava NIL —
             * `DIM a(3): a(0) = DATEVALUE("2023-01-01") : PRINT a(0)` stampava vuoto su VM/C
             * (IL/PCode/C99 stampano la data formattata). */
            s.type = 1; s.num = v.v_double; s.basic_type = GMB_BT_DATE; break;
        case GMB_TYPE_BOOL:
            s.type = 1; s.num = v.v_bool ? -1.0 : 0.0; s.basic_type = GMB_BT_BOOL; break;
        case GMB_TYPE_STRING:
        {
            gmb_obj_string_t* str = gmb_str_cstr(v.v_string.data ? v.v_string.data : "");
            s.type = 2; s.ref = (gmb_obj_t*)str; s.basic_type = GMB_BT_STRING;
            break;
        }
        case GMB_TYPE_ARRAY:
        {
            /* Lista del runtime condiviso (es. riga di matrice multidim): la incapsula in un
             * gmb_obj_shared_val_t copiando i valori, così lo stack la gestisce con refcount e
             * gli opcode lista (FE_DESTRUCT, LOAD_ELEMENT, LIST_ADD...) la vedono come lista. */
            gmb_list_t src = gmb_val_as_list(v);
            gmb_obj_shared_val_t* sv = gmb_shared_list_new();
            if (sv)
            {
                gmb_list_t* dst = sv->list;
                for (gmb_int_t i = 0; i < gmb_list_count(src); i++)
                    gmb_list_add(dst, gmb_list_get(src, i));
                s.type = 2; s.ref = (gmb_obj_t*)sv; s.basic_type = GMB_BT_OBJECT;
            }
            else { s.type = 0; s.basic_type = GMB_BT_NULL; }
            break;
        }
        case GMB_TYPE_OBJECT:
        {
            gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
            /* CLOSURE (EVENT/ADD HANDLER, 2.9): un handler passato a .Add() attraversa
             * stack_to_val (sotto) → val_to_stack (qui) quando riletto da una lista/dict —
             * a differenza di LIST/DICT non ha "contenuto" da copiare per valore, è un
             * riferimento/handle puro (indirizzo+upvalue). Trattarlo come dict (il ramo
             * sotto, mai raggiunto prima perché stack_to_val restituiva NIL per qualunque
             * OBJECT non STRING/LIST/DICT — bug reale: un handler aggiunto a un registro
             * eventi diventava sempre NIL, RAISE EVENT non invocava mai nulla) sarebbe
             * type confusion (gmb_obj_closure_t non è un gmb_obj_shared_val_t — ->dict
             * leggerebbe campi a caso). Fix: solo retain, nessuna copia. */
            if (o && o->kind == GMB_OBJ_CLOSURE)
            {
                gmb_obj_retain(o);
                s.type = 2; s.ref = o; s.basic_type = GMB_BT_OBJECT;
                break;
            }
            /* BYREF box su campo __ClosureCtx (Closure-ByRef-Box-Linker-Definitivo, Fase 2/3):
             * stesso identico motivo del ramo CLOSURE sopra — un gmb_obj_byref_t non ha
             * "contenuto" da copiare per valore, è un riferimento write-through puro
             * (frame+slot). Trattarlo come dict sotto sarebbe type confusion. Fix: solo
             * retain, nessuna copia — preserva l'identità del box condiviso col chiamante. */
            if (o && o->kind == GMB_OBJ_BYREF)
            {
                gmb_obj_retain(o);
                s.type = 2; s.ref = o; s.basic_type = GMB_BT_OBJECT;
                break;
            }
            /* STRINGA wrappata come OBJECT (es. elementi di lista aggiunti con
             * gmb_make_val_obj((gmb_obj_t*)gmb_str_cstr(...)) — ATOKENS/bf_528, word-split
             * bf a :4851): senza questo ramo cadeva nel ramo dict sotto (gmb_val_as_dict su
             * una stringa = dict vuoto) e l'elemento letto dalla lista era sempre vuoto.
             * Fix: solo retain, come CLOSURE/BYREF — la stringa è immutabile.
             * GUARDIA !o->type_name (bug Lambda-Param-Array-Indicizzato-Cross-Backend,
             * , trovato 2026-09-16): GMB_OBJ_STRING vale 0 (gmb_runtime.h), lo STESSO
             * valore di default lasciato da gmb_obj_new/gmb_obj_new_typed quando ->kind non
             * viene impostato esplicitamente (usato per "boxare" un valore del runtime
             * condiviso in un gmb_obj_t generico, es. gmb_make_val_list → gmb_obj_new_typed
             * (p, "gmb_list")) — quindi QUALUNQUE oggetto runtime-wrapped di questo tipo
             * (kind==0 per coincidenza, non una vera stringa) veniva scambiato per una
             * GMB_OBJ_STRING, "corto-circuitando" il ramo is_list/dict sotto (mai
             * raggiunto). Sintomo osservato: una λ codeblock con parametro ARRAY passata a
             * NUMERICAL.SolveIvp riceveva `y` come oggetto malformato — l'indicizzazione
             * y(0) leggeva sempre NIL (contatore/lunghezza letti dai campi sbagliati di
             * gmb_obj_string_t invece che dal gmb_list_t reale). Le VERE stringhe
             * (gmb_str_alloc, gmb_vm_value.c) non impostano MAI type_name (calloc + nessun
             * assegnamento) — è un discriminante sicuro. */
            if (o && o->kind == GMB_OBJ_STRING && !o->type_name)
            {
                gmb_obj_retain(o);
                s.type = 2; s.ref = o; s.basic_type = GMB_BT_STRING;
                break;
            }
            /* Il valore OBJECT può essere una LISTA (oggetto VM GMB_OBJ_LIST o oggetto
             * runtime type_name "gmb_list") oppure un DICT. Distinguere per non
             * serializzare le liste dentro i dict come {} */
            /* BYTES boxed (gmb_make_val_bytes, type_name "gmb_bytes", kind==0): senza questo
             * ramo cadeva nel ramo dict sotto (gmb_val_as_dict su bytes = dict vuoto) → i
             * byte letti da un dict erano zero (bug: BLOCKCHAIN.DERIVEKEY("privateKey") sul
             * VM/C dava 6 byte zero, mentre C99Native/IL erano corretti). */
            if (o && o->type_name && strcmp(o->type_name, "gmb_bytes") == 0)
            {
                gmb_obj_retain(o);
                s.type = 2; s.ref = o; s.basic_type = GMB_BT_OBJECT;
                break;
            }
            /* discriminazione UNICA (gmb_adapter_obj_is_container): stessa regola di
             * stack_to_val e dello store, così un box runtime-wrapped non viene scambiato
             * per una stringa (kind 0 == GMB_OBJ_STRING). */
            int is_list = 0;
            if (o && gmb_adapter_obj_is_container(o) && o->kind != GMB_OBJ_DICT
                && !(o->type_name && strcmp(o->type_name, "gmb_dict") == 0))
                is_list = 1;
            if (is_list)
            {
                gmb_obj_shared_val_t* sv = gmb_shared_list_new();
                if (sv)
                {
                    gmb_list_t* dst = sv->list;
                    if (o->kind == GMB_OBJ_LIST && ((gmb_obj_shared_val_t*)o)->list)
                    {
                        gmb_list_t* src = ((gmb_obj_shared_val_t*)o)->list;
                        for (size_t i = 0; i < src->count; i++)
                            gmb_list_add(dst, src->items[i]);
                    }
                    else
                    {
                        gmb_list_t src = gmb_val_as_list(v);
                        for (gmb_int_t i = 0; i < gmb_list_count(src); i++)
                            gmb_list_add(dst, gmb_list_get(src, i));
                    }
                    s.type = 2; s.ref = (gmb_obj_t*)sv; s.basic_type = GMB_BT_OBJECT;
                }
                else { s.type = 0; s.basic_type = GMB_BT_NULL; }
            }
            else
            {
                if (o && o->kind == GMB_OBJ_DICT && ((gmb_obj_shared_val_t*)o)->dict)
                {
                    /* Oggetto VM dict (gmb_obj_shared_val_t): header.data è NULL, quindi
                     * gmb_val_as_dict/gmb_obj_data restituirebbero vuoto. Copia direttamente
                     * dal campo dict (es. elementi di HTTPPOSTEDDATA nel FOR EACH). */
                    gmb_obj_shared_val_t* sv = gmb_shared_dict_new();
                    if (sv)
                    {
                        gmb_dict_t* dst = sv->dict;
                        gmb_dict_t* src = ((gmb_obj_shared_val_t*)o)->dict;
                        for (size_t i = 0; i < src->count; i++)
                            gmb_dict_set(dst, src->items[i].key, src->items[i].value);
                        s.type = 2; s.ref = (gmb_obj_t*)sv; s.basic_type = GMB_BT_OBJECT;
                    }
                    else { s.type = 0; s.basic_type = GMB_BT_NULL; }
                }
                else
                {
                    gmb_dict_t src = gmb_val_as_dict(v);
                    gmb_obj_shared_val_t* sv = gmb_shared_dict_new();
                    if (sv)
                    {
                        gmb_dict_t* dst = sv->dict;
                        for (size_t i = 0; i < src.count; i++)
                            gmb_dict_set(dst, src.items[i].key, src.items[i].value);
                        s.type = 2; s.ref = (gmb_obj_t*)sv; s.basic_type = GMB_BT_OBJECT;
                    }
                    else { s.type = 0; s.basic_type = GMB_BT_NULL; }
                }
            }
            break;
        }
        default:
            s.type = 0; s.basic_type = GMB_BT_NULL; break;
    }
    return s;
}

gmb_stack_entry_t gmb_adapter_val_to_stack_ref(gmb_val_t v)
{
    if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
    {
        gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
        if (o->kind == GMB_OBJ_DICT || o->kind == GMB_OBJ_LIST)
        {
            gmb_stack_entry_t s;
            memset(&s, 0, sizeof(s));
            gmb_obj_retain(o);
            s.type = 2; s.ref = o; s.basic_type = GMB_BT_OBJECT;
            return s;
        }
    }
    return gmb_adapter_val_to_stack(v);
}

double gmb_adapter_to_double(gmb_val_t v)
{
    switch (v.kind)
    {
        case GMB_TYPE_INT: return (double)v.v_int;
        case GMB_TYPE_DOUBLE: return v.v_double;
        /* DATETIME = seriale OADate; CURRENCY = numerico con scala. Senza questi rami un
         * valore data/vALuta letto da un contenitore si convertiva in 0.0. */
        case GMB_TYPE_DATETIME: return v.v_double;
        case GMB_TYPE_CURRENCY: return v.v_double;
        case GMB_TYPE_BOOL: return v.v_bool ? -1.0 : 0.0;
        default: return 0.0;
    }
}
