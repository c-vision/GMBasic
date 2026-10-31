/* gmb_syscall_rdd.c — handler SysCall RDD (range 32-60) estratti dallo switch in
 * gmb_vm_exec_syscall (task 6.5.2). Tabella gmb_rdd_impl; dispatcher per lookup. */
#include "gmb_vm_core.h"
#include "gmb_db.h"   /* AREA ISAM: RDD SET FIELD deve raggiungere il motore dell'area corrente */

static int gmb_rdd_default(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)task; (void)argc;
    char rmsg[64]; snprintf(rmsg, sizeof(rmsg), "RDD syscall %d non implementato", code);
    vm_error(task, rmsg);
    return 0;
}

static int gmb_rdd_32(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
        {
            char fb[1024], db[64], ab[64];
            gmb_stack_entry_t e1 = gmb_task_pop(task); vm_entry_str(e1, ab, sizeof(ab)); if (e1.ref) gmb_obj_release(e1.ref);
            gmb_stack_entry_t e2 = gmb_task_pop(task); vm_entry_str(e2, db, sizeof(db)); if (e2.ref) gmb_obj_release(e2.ref);
            gmb_stack_entry_t e3 = gmb_task_pop(task); vm_entry_str(e3, fb, sizeof(fb)); if (e3.ref) gmb_obj_release(e3.ref);
            int area = gmb_rdd_use(fb, ab, db);
            push_num_entry(task, (double)area, GMB_BT_NUMBER);
            return 0;
        }
}

static int gmb_rdd_33(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
gmb_stack_entry_t e;
        {
            if (task->sp > 0)
            {
                gmb_stack_entry_t e = gmb_task_pop(task);
                if (e.type == 2 && e.ref)
                {
                    char ab[64];
                    vm_entry_str(e, ab, sizeof(ab));
                    gmb_rdd_select_alias(ab);
                }
                else gmb_rdd_select((int)e.num);
                if (e.ref) gmb_obj_release(e.ref);
            }
            /* -1 = TRUE (convenzione di tutti i backend): il PCode spinge `ok ? -1.0 : 0.0`,
             * il runtime C ritorna 1/0 — la conversione va fatta qui. */
            push_num_entry(task, (code == 33 ? gmb_rdd_rlock() : gmb_rdd_flock()) ? -1.0 : 0.0, GMB_BT_NUMBER);
            return 0;
        }
}

/* 35 FLOCK — vedi gmb_rdd_33 (condiviso con 33/34). */

/* 36 UNLOCK: rilascia il lock dell'area; il PCode spinge null → push_nil. */
static int gmb_rdd_36(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    if (task->sp > 0)
    {
        gmb_stack_entry_t e = gmb_task_pop(task);
        if (e.type == 2 && e.ref)
        {
            char ab[64];
            vm_entry_str(e, ab, sizeof(ab));
            gmb_rdd_select_alias(ab);
        }
        else gmb_rdd_select((int)e.num);
        if (e.ref) gmb_obj_release(e.ref);
    }
    gmb_rdd_unlock();
    push_nil(task);
    return 0;
}

/* 37 SET LOCK TIMEOUT: nessun valore di ritorno (come il PCode). */
static int gmb_rdd_37(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    int ms = task->sp > 0 ? (int)gmb_task_pop_num(task) : 0;
    gmb_rdd_set_lock_timeout(ms);
    return 0;
}

/* Entry → testo del VALORE: numeri formattati (%.14g, come le celle del motore),
 * stringhe copiate. vm_entry_str da sola restituiva "" sui numeri → `RDD SET FIELD
 * area, campo, <numero>` scriveva vuota (misurato: rb_write val=""). */
static void vm_entry_value_str(gmb_stack_entry_t e, char* buf, int cap)
{
    if (!buf || cap <= 0) return;
    buf[0] = 0;
    if (e.type == 1) { snprintf(buf, (size_t)cap, "%.14g", e.num); return; }
    vm_entry_str(e, buf, cap);
}

/* 39/40 SET FIELD (il bytecode reale del comando RDD SET FIELD è 39, misurato con
 * trace; il gestito enum direbbe 40): stack valore, fieldIdx, area. fieldIdx è
 * 1-based (Clipper) → convertito qui al confine; la scrittura passa dal runtime
 * condiviso (`gmb_rdd_replace_field`) che devia nel row-buffer se attivo (§0bis). */
static int gmb_rdd_set_field_common(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    gmb_stack_entry_t e3 = gmb_task_pop(task); char vb[256]; vm_entry_value_str(e3, vb, sizeof(vb)); if (e3.ref) gmb_obj_release(e3.ref);
    gmb_stack_entry_t e2 = gmb_task_pop(task);
    char fbn[64] = "";
    if (e2.type == 2) vm_entry_str(e2, fbn, sizeof(fbn));
    if (e2.ref) gmb_obj_release(e2.ref);
    gmb_stack_entry_t e1 = gmb_task_pop(task);
    if (e1.type == 2 && e1.ref) { char ab[64]; vm_entry_str(e1, ab, sizeof(ab)); gmb_rdd_select_alias(ab); }
    else gmb_rdd_select(e1.type == 1 ? (int)e1.num : 0);
    if (e1.ref) gmb_obj_release(e1.ref);
    /* RISOLUZIONE DOPO la select: field_index guarda l'area corrente (cell o hook ISAM). */
    int fidx = (fbn[0]) ? gmb_rdd_field_index(fbn) : -1;
    if (!fbn[0]) fidx = 0;   /* forma numerica: -1 è gestito a valle come campo assente */
    if (e2.type == 1) fidx = (int)e2.num - 1;
    push_num_entry(task, (double)gmb_rdd_replace_field(fidx, vb), GMB_BT_NUMBER);
    return 0;
}

static int gmb_rdd_39(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) { return gmb_rdd_set_field_common(vm, task, code, argc); }

static int gmb_rdd_58(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
        {
            int on = task->sp > 0 ? (int)gmb_task_pop_num(task) : 0;
            push_num_entry(task, (double)gmb_rdd_set_deleted(on), GMB_BT_NUMBER);
            return 0;
        }
}


/* ── Scope dei comandi RDD (DELETE/RECALL/REPLACE … ALL|NEXT n|REST|RECORD n|FOR|WHILE) ──
 * Il bytecode pusha 4 argomenti di scope: hasScope, scopeCode, count, condizione
 * ("condizione (For/While): stringa TESTO o double codeblock-ip", vedi
 * PCodeProcessor.PopScopeClause). Codici: 3=ALL, 2=NEXT n, 4=RECORD n, 5=REST, 0=FOR, 1=WHILE.
 * Gli scope CONDIZIONALI richiedono di eseguire il bytecode della condizione per record: sulla
 * VM/C non è implementato, quindi si produce un ERRORE ESPLICITO (§0bis: mai un risultato
 * silenzioso) invece di applicare la scrittura a tutti i record. */
static void gmb_rdd_scope_pop(gmb_task_t* task, int* has, int* code, int* n, const char** cond)
{
    gmb_stack_entry_t c = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    gmb_stack_entry_t nv = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    double sc = (task->sp > 0) ? gmb_task_pop_num(task) : 0;
    double hs = (task->sp > 0) ? gmb_task_pop_num(task) : 0;
    *has = (hs != 0);
    *code = (int)sc;
    *n = (nv.type == 1) ? (int)nv.num : 1;
    *cond = (c.type == 2 && c.ref && c.ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)c.ref)->data : NULL;
    if (c.ref) gmb_obj_release(c.ref);
    if (nv.ref) gmb_obj_release(nv.ref);
}

/* Applica una funzione a ogni record dello scope. Ritorna il numero di record toccati, oppure
 * -1 se lo scope non è supportato (il chiamante emette l'errore esplicito). */
static int gmb_rdd_scope_apply(int has, int code, int n, const char* cond, int (*fn)(void*), void* ctx)
{
    /* FOR (0) / WHILE (1): condizione TESTO valutata per record dal runtime condiviso
     * (`campo OP letterale`); se non è valutabile → -1 = errore esplicito. WHILE parte dal
     * record corrente e si ferma al primo non match. */
    if (has && (code == 0 || code == 1))
    {
        int done = 0;
        if (code == 0) gmb_rdd_gotop();
        while (!gmb_rdd_eof())
        {
            int m = gmb_rdd_condition_match(cond);
            if (m < 0) return -1;
            if (!m) { if (code == 1) break; else { gmb_rdd_skip(1); continue; } }
            if (fn(ctx)) done++;
            gmb_rdd_skip(1);
        }
        return done;
    }
    if (!has || code == 0 /*Current*/ ) { return fn(ctx) ? 1 : 0; }
    if (code == 3 /*All*/ || code == 5 /*Rest*/ || code == 2 /*Next n*/)
    {
        int done = 0;
        if (code != 5) gmb_rdd_gotop();
        if (code == 2 && n < 1) n = 1;
        while (!gmb_rdd_eof())
        {
            if (code == 2 && done >= n) break;
            if (fn(ctx)) done++;
            gmb_rdd_skip(1);
        }
        return done;
    }
    if (code == 4 /*Record n*/ ) { if (gmb_rdd_go(n) != 0) return -1; return fn(ctx) ? 1 : 0; }
    return -1;   /* FOR/WHILE: condizione bytecode non valutabile qui */
}

typedef struct { int idx; char val[256]; } rdd_one_t;
static int gmb_rdd_apply_replace(void* cx) { rdd_one_t* o = (rdd_one_t*)cx; return gmb_rdd_replace_field(o->idx, o->val) == 0; }
static int gmb_rdd_apply_delete(void* cx) { (void)cx; return gmb_rdd_delete() == 0; }
static int gmb_rdd_apply_recall(void* cx) { (void)cx; return gmb_rdd_recall() == 0; }

static int gmb_rdd_sys_delete_scoped(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);   /* 63 */
static int gmb_rdd_sys_recall_scoped(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);   /* 64 */
static int gmb_rdd_sys_replace_scoped(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);  /* 65 */

static int gmb_rdd_sys_delete_scoped(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    int has, sc, n; const char* cond;
    gmb_rdd_scope_pop(task, &has, &sc, &n, &cond);
    int done = gmb_rdd_scope_apply(has, sc, n, cond, gmb_rdd_apply_delete, NULL);
    if (done < 0) { vm_error(task, "RDD DELETE ... FOR/WHILE: condizione bytecode non supportata su VM/C"); return 0; }
    push_num_entry(task, (double)done, GMB_BT_NUMBER);
    return 0;
}

static int gmb_rdd_sys_recall_scoped(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    int has, sc, n; const char* cond;
    gmb_rdd_scope_pop(task, &has, &sc, &n, &cond);
    int done = gmb_rdd_scope_apply(has, sc, n, cond, gmb_rdd_apply_recall, NULL);
    if (done < 0) { vm_error(task, "RDD RECALL ... FOR/WHILE: condizione bytecode non supportata su VM/C"); return 0; }
    push_num_entry(task, (double)done, GMB_BT_NUMBER);
    return 0;
}

static int gmb_rdd_sys_replace_scoped(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    int has, sc, n; const char* cond;
    gmb_rdd_scope_pop(task, &has, &sc, &n, &cond);
    int rc = (task->sp > 0) ? (int)gmb_task_pop_num(task) : 0;
    int total = 0, k;
    /* coppie (valore, campo) dal fondo dello stack, come il PCode. */
    rdd_one_t items[8];
    if (rc > 8) rc = 8;
    for (k = 0; k < rc; k++)
    {
        gmb_stack_entry_t val = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        gmb_stack_entry_t fld = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        char fname[64] = "";
        vm_entry_str(fld, fname, (int)sizeof(fname));
        items[k].idx = gmb_rdd_field_index(fname);
        vm_entry_value_str(val, items[k].val, (int)sizeof(items[k].val));
        if (val.ref) gmb_obj_release(val.ref);
        if (fld.ref) gmb_obj_release(fld.ref);
    }
    for (k = 0; k < rc; k++)
    {
        if (items[k].idx < 0) continue;
        int done = gmb_rdd_scope_apply(has, sc, n, cond, gmb_rdd_apply_replace, &items[k]);
        if (done < 0) { vm_error(task, "RDD REPLACE ... FOR/WHILE: condizione bytecode non supportata su VM/C"); return 0; }
        if (done > total) total = done;
    }
    push_num_entry(task, (double)total, GMB_BT_NUMBER);
    return 0;
}

/* 40 RDD ON EVENT "name" [, callback] — bytecode code 40 = RDD_ONEVENT (il SET FIELD è il
 * codice 39). Prima era mappato erroneamente su set_field → `RDD ON EVENT` scriveva/leggeva un
 * campo col nome evento (get_field(-1)). Ora registra l'evento nel runtime condiviso
 * (gmb_rdd_onevent), parità IL/PCode (SqlDataSession.Events.On). */
static int gmb_rdd_40(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    gmb_stack_entry_t cb = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    gmb_stack_entry_t ev = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    char nb[64] = "";
    vm_entry_str(ev, nb, sizeof(nb));
    if (ev.ref) gmb_obj_release(ev.ref);
    if (cb.ref) gmb_obj_release(cb.ref);
    gmb_rdd_onevent(nb);
    return 0;
}

/* 51 GETFLDSTATE(area, field): stato del campo (0 = non modificato, 2 = record cancellato).
 * Stessa sorgente del builtin della forma funzione (§0bis), così le due superfici concordano. */
static int gmb_rdd_51(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    int idx = (task->sp > 0) ? (int)gmb_task_pop_num(task) : 0;
    double area = (task->sp > 0) ? gmb_task_pop_num(task) : 0;
    if (area > 0) gmb_rdd_select((int)area);
    if (idx > 0) idx -= 1;          /* 1-based dal bytecode */
    push_num_entry(task, (double)gmb_rdd_get_field_state(idx), GMB_BT_NUMBER);
    return 0;
}

/* 41 SET REPROCESS <ms> (`RDD SET REPROCESS 2000`): codice del BYTECODE misurato con
 * GMB_DUMP_CHUNK (`RDD_SETREPROCESS (1 args)`); l'enum C# lo direbbe 52. Nessun valore di
 * ritorno, come il resto dei SET. */
static int gmb_rdd_41(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    int ms = (task->sp > 0) ? (int)gmb_task_pop_num(task) : 0;
    gmb_rdd_set_reprocess(ms);
    return 0;
}

/* 52 SET ORDER(area, ordine) (`RDD SET ORDER 1, "Name"`): ordine attivo per SEEK/navigazione
 * (nome campo; vuoto = ordine fisico). gmb_rdd_set_order era già implementato in gmb_rdd.c
 * (order_field/rebuild_order/next_visible_ord) ma mai agganciato a un syscall — mancando dalla
 * tabella, ogni RDD SET ORDER falliva con "RDD syscall 52 non implementato" (bloccava
 * RDD_Memory_CRUD.bas). Wrapper diretto, stesso pattern di gmb_rdd_40/51. */
static int gmb_rdd_52(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    gmb_stack_entry_t ord = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    double area = (task->sp > 0) ? gmb_task_pop_num(task) : 0;
    char ob[64] = "";
    vm_entry_str(ord, ob, (int)sizeof(ob));
    if (ord.ref) gmb_obj_release(ord.ref);
    if (area > 0) gmb_rdd_select((int)area);
    gmb_rdd_set_order(ob);
    return 0;
}

/* 55 RDD COPY TO area, file [, driver]: snapshot dell'area su file (gmb_rdd_copy_to).
 * Il bytecode pusha area, file, driver (argc 3). Prima mancava → "RDD syscall 55 non
 * implementato" (bloccava CustomerOrdersReport.bas). */
static int gmb_rdd_55(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    gmb_stack_entry_t e3 = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    gmb_stack_entry_t e2 = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    gmb_stack_entry_t e1 = (task->sp > 0) ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
    char fb[512] = "", db[32] = "CSV";
    vm_entry_str(e2, fb, sizeof(fb));
    if (e3.type == 2) vm_entry_str(e3, db, sizeof(db));
    if (e1.type == 2 && e1.ref) { char ab[64]; vm_entry_str(e1, ab, sizeof(ab)); gmb_rdd_select_alias(ab); }
    else gmb_rdd_select(e1.type == 1 ? (int)e1.num : 0);
    if (e1.ref) gmb_obj_release(e1.ref);
    if (e2.ref) gmb_obj_release(e2.ref);
    if (e3.ref) gmb_obj_release(e3.ref);
    push_num_entry(task, (double)gmb_rdd_copy_to(fb, db), GMB_BT_NUMBER);
    return 0;
}

typedef struct { int code; int (*handler)(gmb_vm_t*, gmb_task_t*, int, int); } gmb_rdd_impl_t;
static int gmb_rdd_53(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    /* 53 RDD CREATE INDEX(area, nome, chiave) → imposta l'ordine sulla chiave (effetto REALE:
     * gmb_rdd_set_order). 54 RDD CLOSE INDEX → torna all'ordine fisico. Prima era un no-op
     * silenzioso (push 0 senza lavoro) che il gate anti-stub segnalava. */
    (void)vm;
    char key[256] = "";
    for (int i = 0; i < argc && task->sp > 0; i++) {
        gmb_stack_entry_t e = gmb_task_pop(task);
        if (i == 0 && e.type == 2 && e.ref) vm_entry_str(e, key, sizeof(key));   /* chiave = ultimo arg (top) */
        if (e.ref) gmb_obj_release(e.ref);
    }
    if (code == 53) gmb_rdd_set_order(key);   /* CREATE INDEX: ordine sulla chiave */
    else gmb_rdd_set_order("");               /* CLOSE INDEX: ordine fisico */
    push_num_entry(task, 0.0, GMB_BT_NUMBER);
    return 0;
}

static const gmb_rdd_impl_t gmb_rdd_impl[] = {
/* {label, handler} */
    { 32, gmb_rdd_32 },
    { 33, gmb_rdd_33 },
    { 34, gmb_rdd_33 },
    { 35, gmb_rdd_33 },
    { 36, gmb_rdd_36 },
    { 37, gmb_rdd_37 },
    { 39, gmb_rdd_39 },
    { 40, gmb_rdd_40 },
    { 41, gmb_rdd_41 },
    { 51, gmb_rdd_51 },
    { 52, gmb_rdd_52 },
    { 53, gmb_rdd_53 },   /* CREATE INDEX (no-op) */
    { 54, gmb_rdd_53 },   /* CLOSE INDEX (no-op) */
    { 55, gmb_rdd_55 },   /* RDD COPY TO (l'handler c'era ma mancava dalla tabella) */
    { 58, gmb_rdd_58 },
    /* CODICI REALI del bytecode (misurati con GMB_DEBUG_SYS su probe mirati): 62 DELETE,
     * 63 RECALL, 64 REPLACE — non i valori dell'enum C# SysCall (63/64/65), che differiscono
     * di uno. Prima erano registrati a 63/64/65 e non venivano MAI raggiunti. */
    { 62, gmb_rdd_sys_delete_scoped },
    { 63, gmb_rdd_sys_recall_scoped },
    { 64, gmb_rdd_sys_replace_scoped },
};

int gmb_syscall_rdd(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    for (size_t i = 0; i < sizeof(gmb_rdd_impl) / sizeof(gmb_rdd_impl[0]); i++)
        if (gmb_rdd_impl[i].code == code)
            return gmb_rdd_impl[i].handler(vm, task, code, argc);
    return gmb_rdd_default(vm, task, code, argc);
}
