/* native/gmb_vm_c/gmb_mcp_exec.c — MCP nel mondo VM/C (vedi gmb_mcp_exec.h).
 *
 * Contiene:
 *   - gli handler dei builtin MCP (registrazione tool con schema+outputSchema,
 *     SERVE, STOP) che sostituiscono i vecchi no-op silenziosi;
 *   - l'EXECUTOR richiesto dal server MCP del runtime condiviso per invocare la
 *     procedura BASIC di un tool.
 *
 * L'inputSchema (derivato a compile-time dal compilatore P-Code) è usato anche per
 * RICOSTRUIRE nome e tipo degli argomenti: il chunk P-Code non porta i tipi dei
 * parametri, quindi il JSON `arguments` del client viene mappato sullo schema del
 * tool. Argomenti di tipo array non sono supportati: errore ESPLICITO.
 */
#include "gmb_mcp_exec.h"
#include "gmb_adapter.h"
#include "gmb_gpm.h"
#include "gmb_runtime.h"
#include "gmb_vm_value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static gmb_vm_t*   g_vmc_vm = NULL;
static gmb_task_t* g_vmc_serve_task = NULL;
static int         g_vmc_stdio = 1;

/* ---------------------------------------------------------------- args */
#define MCP_MAX_ARGS 16

typedef struct { char name[64]; char type[24]; } mcp_arg_t;

/* Estrae nome+tipo delle proprietà dell'inputSchema (ordine di dichiarazione =
 * ordine dei parametri della FUNCTION). Ritorna il numero di proprietà. */
static int mcp_schema_props(const char* schema, mcp_arg_t* out, int max)
{
    if (!schema || !out || max <= 0) return 0;
    const char* p = strstr(schema, "\"properties\"");
    if (!p) return 0;
    p = strchr(p, '{');
    if (!p) return 0;
    p++;
    int n = 0;
    while (n < max && *p) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (*p != '"') break;
        const char* e = strchr(p + 1, '"');
        if (!e) break;
        size_t len = (size_t)(e - (p + 1));
        if (len > sizeof(out[n].name) - 1) len = sizeof(out[n].name) - 1;
        memcpy(out[n].name, p + 1, len); out[n].name[len] = '\0';

        const char* ob = strchr(e + 1, '{');
        if (!ob) break;
        int d = 1; const char* q = ob + 1;
        while (*q && d > 0) { if (*q == '{') d++; else if (*q == '}') d--; q++; }
        /* cerca "type":"<t>" dentro ob..q */
        out[n].type[0] = '\0';
        const char* tt = strstr(ob, "\"type\"");
        if (tt && tt < q) {
            tt = strchr(tt, ':');
            if (tt && tt < q) {
                tt = strchr(tt, '"');
                if (tt && tt < q) {
                    const char* te = strchr(tt + 1, '"');
                    if (te && te <= q) {
                        size_t tl = (size_t)(te - (tt + 1));
                        if (tl > sizeof(out[n].type) - 1) tl = sizeof(out[n].type) - 1;
                        memcpy(out[n].type, tt + 1, tl); out[n].type[tl] = '\0';
                    }
                }
            }
        }
        p = q;
        n++;
    }
    return n;
}

static void mcp_push_num(gmb_task_t* t, double v) { gmb_task_push_num(t, v, GMB_BT_NUMBER); }

static void mcp_push_str(gmb_task_t* t, const char* s)
{
    gmb_obj_string_t* o = gmb_str_cstr(s ? s : "");
    if (o) { gmb_task_push_ref(t, (gmb_obj_t*)o, GMB_BT_STRING); gmb_str_release(o); }
    else gmb_task_push_num(t, 0, GMB_BT_NUMBER);
}

/* Spinge gli argomenti del tool sullo stack del task, nell'ordine dello schema.
 * Ritorna il numero di argomenti, oppure -1 su argomento non supportato (messaggio
 * in `err`). */
static int mcp_push_args(gmb_task_t* t, const char* tool_name, const char* args_json, char* err, size_t errcap)
{
    mcp_arg_t props[MCP_MAX_ARGS];
    int np = mcp_schema_props(gmb_mcp_tool_schema(tool_name), props, MCP_MAX_ARGS);
    for (int i = 0; i < np; i++) {
        char* v = gmb_json_extract(args_json ? args_json : "{}", props[i].name);
        const char* tv = (v && *v) ? v : NULL;
        if (strcmp(props[i].type, "string") == 0) {
            mcp_push_str(t, tv ? tv : "");
        } else if (strcmp(props[i].type, "boolean") == 0) {
            mcp_push_num(t, (tv && (strcmp(tv, "true") == 0 || strcmp(tv, "TRUE") == 0)) ? -1.0 : 0.0);
        } else if (strcmp(props[i].type, "integer") == 0 || strcmp(props[i].type, "number") == 0) {
            mcp_push_num(t, tv ? atof(tv) : 0.0);
        } else if (strcmp(props[i].type, "array") == 0) {
            snprintf(err, errcap, "argomento array '%s' non supportato dal tool MCP su VM/C", props[i].name);
            free(v);
            return -1;
        } else {
            /* tipo non dichiarato: JSON numerico → numero, altrimenti stringa */
            if (tv) {
                char* endp = NULL;
                double d = strtod(tv, &endp);
                if (endp && *endp == '\0') mcp_push_num(t, d);
                else mcp_push_str(t, tv);
            } else {
                mcp_push_str(t, "");
            }
        }
        free(v);
    }
    return np;
}

/* Converte il valore di ritorno della procedura in testo — stessa resa testuale
 * degli altri backend (`gmb_val_to_string`, già usata dal runtime condiviso). */
static char* mcp_result_text(gmb_task_t* t)
{
    gmb_stack_entry_t rv;
    memset(&rv, 0, sizeof(rv));
    rv.type = t->sync_result_type;
    rv.num = t->sync_result;
    rv.ref = t->sync_result_ref;
    rv.basic_type = (t->sync_result_type == 2) ? GMB_BT_STRING : GMB_BT_NUMBER;
    gmb_val_t v = gmb_adapter_stack_to_val(rv);
    gmb_string_t s = gmb_val_to_string(v);
    char* out = strdup((s.data && *s.data) ? s.data : "");
    gmb_string_free(&s);
    return out;
}

/* ---------------------------------------------------- executor (runtime → VM) */
static int vmc_mcp_exec(void* ctx, const char* tool_name, const char* proc_name,
                        const char* args_json, char** out_text, int* out_is_error)
{
    gmb_vm_t* vm = (gmb_vm_t*)ctx;
    *out_is_error = 0;
    *out_text = NULL;
    if (!vm || !vm->chunk) return 0;
    int addr = gmb_vm_proc_addr(vm, proc_name);
    if (addr < 0) return 0;

    char err[256];
    err[0] = '\0';

    /* --- stdio: chiamata sincrona annidata sul task che esegue AI MCP SERVE --- */
    if (g_vmc_stdio && g_vmc_serve_task) {
        gmb_task_t* task = g_vmc_serve_task;
        int nargs = mcp_push_args(task, tool_name, args_json, err, sizeof(err));
        if (nargs < 0) { *out_is_error = 1; *out_text = strdup(err); return 1; }

        int saved_ip = task->ip;
        int saved_sync = task->sync_active;
        int saved_sip = task->sync_saved_ip;
        double saved_sr = task->sync_result;
        gmb_obj_t* saved_sref = task->sync_result_ref;
        uint8_t saved_st = task->sync_result_type;

        if (gmb_vm_push_sync_frame(task, nargs) != 0) {
            *out_is_error = 1; *out_text = strdup("MCP: impossibile preparare il frame di chiamata");
            return 1;
        }
        task->sync_saved_ip = saved_ip;
        task->sync_active = 1;
        task->sync_result = 0;
        task->sync_result_ref = NULL;
        task->sync_result_type = 0;
        task->ip = addr;
        int guard = 0;
        while (task->sync_active && guard++ < 4000000) {
            if (gmb_vm_tick(vm, task) < 0) break;
        }

        /* Il risultato va catturato PRIMA di ripristinare i campi sync del task. */
        char* exmsg = NULL;
        char* text = NULL;
        if (task->pending_exception) {
            exmsg = strdup(task->pending_exception);
            free(task->pending_exception);
            task->pending_exception = NULL;
        } else {
            text = mcp_result_text(task);
        }

        task->ip = saved_ip;
        task->sync_active = saved_sync;
        task->sync_saved_ip = saved_sip;
        task->sync_result = saved_sr;
        task->sync_result_ref = saved_sref;
        task->sync_result_type = saved_st;

        if (exmsg) { *out_is_error = 1; *out_text = exmsg; free(text); return 1; }
        *out_text = text;
        return 1;
    }

    /* --- http: task nuovo nel GPM (il thread HTTP attende il risultato) --- */
    gmb_task_t* t = gmb_task_create(vm->scheduler, addr);
    if (!t) { *out_is_error = 1; *out_text = strdup("MCP: impossibile creare il task del tool"); return 1; }
    t->vm_ctx = vm;
    if (g_vmc_serve_task && g_vmc_serve_task->globals_nums) {
        t->globals_nums = g_vmc_serve_task->globals_nums;
        t->globals_refs = g_vmc_serve_task->globals_refs;
        t->globals_types = g_vmc_serve_task->globals_types;
        t->globals_basic = g_vmc_serve_task->globals_basic;
        t->globals_count = g_vmc_serve_task->globals_count;
    } else {
        gmb_task_ensure_globals(t, vm->chunk->global_count);
    }
    int nargs = mcp_push_args(t, tool_name, args_json, err, sizeof(err));
    if (nargs < 0 || gmb_vm_push_sync_frame(t, nargs) != 0) {
        gmb_task_free(t);
        *out_is_error = 1;
        *out_text = strdup(err[0] ? err : "MCP: frame di chiamata non preparabile");
        return 1;
    }
    t->sync_active = 1;
    t->sync_result = 0;
    t->sync_result_ref = NULL;
    t->sync_result_type = 0;
    t->ip = addr;
    gmb_sched_enqueue(vm->scheduler, t);

    /* §0ter: attesa SULL'EVENTO della chiusura del frame sync — prima era un poll di 2 ms per
     * 15000 giri (30s). Il tool gira nel GPM (task `t`), il chiamante è il thread HTTP dell'MCP
     * server che deve consegnare la risposta: si dorme sulla condizione del task e ci si sveglia
     * quando il task chiude il frame (o muore), con la stessa deadline di 30s di prima. */
    (void)gmb_sched_wait_task_event(t, GMB_TASK_WAIT_SYNC_CLOSED, 30000);

    if (t->sync_active) {
        t->sync_active = 0;
        *out_is_error = 1;
        *out_text = strdup("MCP: timeout nell'esecuzione del tool");
    } else if (t->pending_exception) {
        *out_is_error = 1;
        *out_text = strdup(t->pending_exception);
        free(t->pending_exception);
        t->pending_exception = NULL;
    } else {
        *out_text = mcp_result_text(t);
    }
    gmb_task_free(t);
    return 1;
}

void vmc_mcp_install(gmb_vm_t* vm, gmb_task_t* serve_task, int stdio)
{
    g_vmc_vm = vm;
    g_vmc_serve_task = serve_task;
    g_vmc_stdio = stdio ? 1 : 0;
    gmb_mcp_set_executor(vmc_mcp_exec, vm);
}

/* ------------------------------------------------------------- builtin MCP */
void vmc_mcp_bf_register_tool(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                              gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* tool = vm_builtin_sarg(args, argc, 0);
    const char* proc = vm_builtin_sarg(args, argc, 1);
    const char* desc = vm_builtin_sarg(args, argc, 2);
    int readonly = (argc >= 4 && args[3].type == 1) ? (args[3].num != 0) : 0;
    const char* schema = vm_builtin_sarg(args, argc, 4);      /* inputSchema compile-time */
    const char* oschema = vm_builtin_sarg(args, argc, 5);     /* outputSchema opzionale */
    bout->result = gmb_mcp_register_tool_ex(tool, proc, desc, readonly, schema, oschema) ? -1.0 : 0.0;
}

void vmc_mcp_bf_serve(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                      gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    const char* mode = vm_builtin_sarg(args, argc, 0);
    int port = (argc >= 2 && args[1].type == 1) ? (int)args[1].num : 0;
    int readonly = (argc >= 3 && args[2].type == 1) ? (args[2].num != 0) : 0;
    int stdio = (mode && strcmp(mode, "stdio") == 0) ? 1 : 0;
    /* L'executor va installato PRIMA di servire: senza, ogni tools/call fallirebbe
     * con "No tool executor registered" (errore esplicito, mai silenzioso). */
    vmc_mcp_install(vm, stdio ? task : NULL, stdio);
    bout->result = gmb_mcp_serve(mode, port, readonly) ? -1.0 : 0.0;
}

void vmc_mcp_bf_stop(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                     gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    gmb_mcp_stop();
    bout->result = 1.0;
}
