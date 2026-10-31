/* gmb_syscall_sql.c — handler SysCall SQL (197-209) estratti dallo switch in
 * gmb_vm_exec_syscall (task 6.5.2). Tabella gmb_sql_impl; dispatcher per lookup. */
#include "gmb_vm_core.h"

static int gmb_sql_197(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], path[1024];
    vm_pop_str(task, alias, sizeof(alias)); vm_pop_str(task, path, sizeof(path));
    push_num_entry(task, (double)gmb_sql_open(path, alias), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_198(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128];
    vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_close(alias), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_199(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)argc;
    char alias[128], sql[4096];
    gmb_stack_entry_t a[16];
    int n = argc > 16 ? 16 : argc;
    vm_pop_args_ordered(task, a, n);
    vm_entry_str(a[0], alias, sizeof(alias));
    vm_entry_str(a[1], sql, sizeof(sql));
    int pc = a[2].type == 1 ? (int)a[2].num : 0;
    if (pc > 13) pc = 13;
    const char* params[13];
    char pbuf[13][256];
    for (int i = 0; i < pc && i < 13; i++)
    {
        char tmp[256];
        gmb_stack_entry_t pe = a[3 + i];
        if (pe.type == 2 && pe.ref) { gmb_obj_string_t* ms = (gmb_obj_string_t*)pe.ref; size_t nn = ms->length < 255 ? ms->length : 255; memcpy(tmp, ms->data, nn); tmp[nn] = 0; }
        else if (pe.type == 1) snprintf(tmp, sizeof(tmp), "%.17g", pe.num);
        else tmp[0] = 0;
        snprintf(pbuf[i], sizeof(pbuf[i]), "%s", tmp);
        params[i] = pbuf[i];
    }
    /* Coarse-park (regola GPM): una scrittura PESANTE — che riscrive un file tramite le
     * virtual table csv/parquet — gira su un thread dedicato e il task si parcheggia, così
     * il worker non resta bloccato per tutta la riscrittura. Letture e write leggere restano
     * sincrone (parcheggiare il leggero è più lento). */
    if (code == 199 && gmb_sql_stmt_is_heavy_write(alias, sql)
        && vm_try_park_sql_exec(vm, task, alias, sql, pc > 0 ? (const char* const*)params : NULL, pc))
        return 0;   /* parcheggiato: il thread pusherà il risultato e riaccoderà il task */
    int64_t rs;
    if (code == 200) rs = gmb_sql_query_params(alias, sql, pc > 0 ? params : NULL, pc);
    else {
        /* SQL EXEC: la forma FUNZIONE usa il valore di ritorno (changes); il comando lo ignora.
         * Push dei CHANGES (affected rows) su successo, come IL/PCode/C99. */
        int st = gmb_sql_exec_params(alias, sql, pc > 0 ? params : NULL, pc);
        rs = (st == 0) ? gmb_sql_changes(alias) : (int64_t)st;
    }
    push_num_entry(task, (double)rs, GMB_BT_NUMBER);
    return 0;
}

/* SQL.FETCH(rs) → 1/0 e SQL CLOSE RESULTSET (parità SqlDriver/C99: mancavano del tutto). */
static int gmb_sql_201(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    gmb_stack_entry_t a;
    double rs = 0;
    a = gmb_task_pop(task);
    if (a.type == 1) rs = a.num;
    if (a.ref) gmb_obj_release(a.ref);
    push_num_entry(task, (double)gmb_sql_fetch_next((int64_t)rs), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_202(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    gmb_stack_entry_t a;
    double rs = 0;
    a = gmb_task_pop(task);
    if (a.type == 1) rs = a.num;
    if (a.ref) gmb_obj_release(a.ref);
    gmb_sql_close_rs((int64_t)rs);
    push_num_entry(task, 0.0, GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_203(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128];
    vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_begin(alias), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_204(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128];
    vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_commit(alias), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_205(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128];
    vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_rollback(alias), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_206(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], name[128];
    vm_pop_str(task, name, sizeof(name)); vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_savepoint(alias, name), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_207(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], name[128];
    vm_pop_str(task, name, sizeof(name)); vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_release(alias, name), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_208(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], name[128];
    vm_pop_str(task, name, sizeof(name)); vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_rollback_to(alias, name), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_209(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], path[1024], name[128];
    vm_pop_str(task, name, sizeof(name)); vm_pop_str(task, path, sizeof(path)); vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_attach(alias, path, name), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_210(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], name[128];
    vm_pop_str(task, name, sizeof(name)); vm_pop_str(task, alias, sizeof(alias));
    push_num_entry(task, (double)gmb_sql_detach(alias, name), GMB_BT_NUMBER);
    return 0;
}

/* SQL BACKUP alias TO file: VACUUM INTO (parita PCode ExecRaw). */
static int gmb_sql_211(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char alias[128], path[1024], sql[2200];
    vm_pop_str(task, path, sizeof(path)); vm_pop_str(task, alias, sizeof(alias));
    size_t o = 0;
    o += (size_t)snprintf(sql + o, sizeof(sql) - o, "VACUUM INTO '");
    for (const char* p = path[0] ? path : "backup.db"; *p && o + 2 < sizeof(sql); p++)
    {
        if (*p == '\'') sql[o++] = '\'';
        sql[o++] = *p;
    }
    snprintf(sql + o, sizeof(sql) - o, "'");
    push_num_entry(task, (double)gmb_sql_exec(alias, sql), GMB_BT_NUMBER);
    return 0;
}

static int gmb_sql_default(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)argc;
    char rmsg[64]; snprintf(rmsg, sizeof(rmsg), "SQL syscall %d non implementato", code);
    vm_error(task, rmsg);
    return 0;
}

typedef struct { int code; int (*handler)(gmb_vm_t*, gmb_task_t*, int, int); } gmb_sql_impl_t;
static const gmb_sql_impl_t gmb_sql_impl[] = {
    { 197, gmb_sql_197 },
    { 198, gmb_sql_198 },
    { 199, gmb_sql_199 },
    { 200, gmb_sql_199 },
    { 201, gmb_sql_201 },
    { 202, gmb_sql_202 },
    { 203, gmb_sql_203 },
    { 204, gmb_sql_204 },
    { 205, gmb_sql_205 },
    { 206, gmb_sql_206 },
    { 207, gmb_sql_207 },
    { 208, gmb_sql_208 },
    { 209, gmb_sql_209 },
    { 210, gmb_sql_210 },
    { 211, gmb_sql_211 },
};

int gmb_syscall_sql(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    for (size_t i = 0; i < sizeof(gmb_sql_impl) / sizeof(gmb_sql_impl[0]); i++)
        if (gmb_sql_impl[i].code == code)
            return gmb_sql_impl[i].handler(vm, task, code, argc);
    return gmb_sql_default(vm, task, code, argc);
}
