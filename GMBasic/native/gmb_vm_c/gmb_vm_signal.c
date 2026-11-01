/* gmb_vm_signal.c — namespace SIGNAL sulla VM/C (§0bis: il filtro vive nel runtime
 * condiviso `gmb_signal.c`; qui solo l'aggancio bytecode → handler BASIC).
 *
 *   SIGNAL SUBSCRIBE  <types> <asset> <market> <symbols> <timeframe> <id>
 *   SIGNAL EMIT       <type> <symbol> <value> <asset> <market> <timeframe>
 *   SIGNAL UNSUBSCRIBE <id>       SIGNAL PING
 *   ON SIGNAL <id> CALL <handler>
 *
 * La consegna arriva dal runtime (gmb_signal_emit) sulla callback vm_signal_cb*, che
 * risolve il nome dell'handler nel chunk e ACCoda un task GPM con (id,type,symbol,value
 * [,timeframe]) — mai invocato inline su un thread esterno (§0ter). L'arità viene dal
 * metadata ARITYS del chunk: 4 parametri = senza timeframe, 5 = con timeframe.
 */
#include "gmb_vm_core.h"
#include "gmb_gpm.h"
#include "gmb_vm_value.h"

#include "gmb_signal.h"

#include <stdio.h>
#include <string.h>

#define GMB_VSIG_MAX 64

static char g_vsig_id[GMB_VSIG_MAX][40];
static char g_vsig_handler[GMB_VSIG_MAX][128];
static int  g_vsig_n;
/* VM corrente per la risoluzione dell'handler (impostata alla prima operazione SIGNAL). */
static gmb_vm_t* g_sig_vm;

static int sig_find(const char* id)
{
    int i;
    if (!id || !id[0]) return -1;
    for (i = 0; i < g_vsig_n; i++)
        if (g_vsig_id[i][0] && strcmp(g_vsig_id[i], id) == 0) return i;
    return -1;
}

/* Risolve il nome dell'handler a (indirizzo, arità) nel chunk della VM. */
static int sig_resolve(const char* name, uint32_t* addr, uint32_t* arity)
{
    uint32_t p, k;
    if (!g_sig_vm || !name || !name[0]) return 0;
    for (p = 0; p < g_sig_vm->chunk->proc_count; p++)
    {
        if (!g_sig_vm->chunk->proc_names[p]) continue;
        if (strcasecmp(g_sig_vm->chunk->proc_names[p], name) != 0) continue;
        *addr = g_sig_vm->chunk->proc_addrs[p];
        *arity = 4;
        for (k = 0; k < g_sig_vm->chunk->arity_count; k++)
            if (g_sig_vm->chunk->arity_addrs[k] == *addr) { *arity = g_sig_vm->chunk->arity_vals[k]; break; }
        return 1;
    }
    return 0;
}

/* Enqueue dell'handler come task GPM (4 o 5 parametri). */
static void sig_dispatch(const char* id, const char* type, const char* symbol, double value, const char* tf)
{
    int i = sig_find(id);
    uint32_t addr = 0, arity = 0;
    gmb_task_t* t;
    if (i < 0 || !g_sig_vm) return;
    if (!sig_resolve(g_vsig_handler[i], &addr, &arity)) return;
    t = gmb_task_create(g_sig_vm->scheduler, (int)addr);
    if (!t) return;
    t->vm_ctx = g_sig_vm;
    gmb_vm_task_share_main_globals(t, g_sig_vm);
    gmb_task_push_ref(t, (gmb_obj_t*)gmb_str_cstr(id ? id : ""), GMB_BT_STRING);
    gmb_task_push_ref(t, (gmb_obj_t*)gmb_str_cstr(type ? type : ""), GMB_BT_STRING);
    gmb_task_push_ref(t, (gmb_obj_t*)gmb_str_cstr(symbol ? symbol : ""), GMB_BT_STRING);
    gmb_task_push_num(t, value, GMB_BT_NUMBER);
    if (arity >= 5)
        gmb_task_push_ref(t, (gmb_obj_t*)gmb_str_cstr(tf ? tf : ""), GMB_BT_STRING);
    gmb_sched_enqueue(g_sig_vm->scheduler, t);
}

/* Due forme, una per arità (la scelta avviene in ON SIGNAL, dove il chunk dice quanti
 * parametri ha la SUB): 4 parametri = senza timeframe, 5 = con timeframe. */
static void vm_signal_cb4(const char* id, const char* type, const char* symbol, double value)
{
    sig_dispatch(id, type, symbol, value, "");
}
static void vm_signal_cb5(const char* id, const char* type, const char* symbol, double value, const char* tf)
{
    sig_dispatch(id, type, symbol, value, tf);
}

void vm_signal_subscribe(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    char timeframe[32], symbols[512], market[64], asset[64], types[256], id[40];
    int i;
    (void)code; (void)argc;
    g_sig_vm = vm;
    vm_pop_str(task, timeframe, sizeof(timeframe));
    vm_pop_str(task, symbols, sizeof(symbols));
    vm_pop_str(task, market, sizeof(market));
    vm_pop_str(task, asset, sizeof(asset));
    vm_pop_str(task, types, sizeof(types));
    vm_pop_str(task, id, sizeof(id));
    if (!id[0]) return;
    i = sig_find(id);
    if (i < 0) {
        if (g_vsig_n >= GMB_VSIG_MAX) return;
        i = g_vsig_n++;
        snprintf(g_vsig_id[i], sizeof(g_vsig_id[i]), "%s", id);
        g_vsig_handler[i][0] = 0;
    }
    gmb_signal_subscribe(id, types, asset, market, symbols, timeframe);
}

void vm_signal_on(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    char id[40], handler[128];
    int i;
    (void)code; (void)argc;
    g_sig_vm = vm;
    vm_pop_str(task, id, sizeof(id));
    vm_pop_str(task, handler, sizeof(handler));
    i = sig_find(id);
    if (i < 0) return;
    if (handler[0]) {
        uint32_t addr = 0, arity = 0;
        snprintf(g_vsig_handler[i], sizeof(g_vsig_handler[i]), "%s", handler);
        if (sig_resolve(handler, &addr, &arity) && arity >= 5)
            gmb_signal_on(id, NULL, vm_signal_cb5);
        else
            gmb_signal_on(id, vm_signal_cb4, NULL);
    } else {
        g_vsig_handler[i][0] = 0;
        gmb_signal_on(id, NULL, NULL);
    }
}

void vm_signal_emit(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    char timeframe[32], market[64], asset[64], symbol[128], type[128];
    double value;
    (void)code; (void)argc;
    g_sig_vm = vm;
    vm_pop_str(task, timeframe, sizeof(timeframe));
    vm_pop_str(task, market, sizeof(market));
    vm_pop_str(task, asset, sizeof(asset));
    value = gmb_task_pop_num(task);
    vm_pop_str(task, symbol, sizeof(symbol));
    vm_pop_str(task, type, sizeof(type));
    gmb_signal_emit(type, symbol, value, asset, market, timeframe);
}

void vm_signal_unsubscribe(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    char id[40];
    int i;
    (void)code; (void)argc;
    g_sig_vm = vm;
    vm_pop_str(task, id, sizeof(id));
    gmb_signal_unsubscribe(id);
    if (!id[0]) { for (i = 0; i < g_vsig_n; i++) { g_vsig_id[i][0] = 0; g_vsig_handler[i][0] = 0; } }
    else { i = sig_find(id); if (i >= 0) { g_vsig_id[i][0] = 0; g_vsig_handler[i][0] = 0; } }
}

void vm_signal_ping(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)task; (void)code; (void)argc;
    gmb_signal_ping();
}
