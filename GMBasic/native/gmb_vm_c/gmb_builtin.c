/* gmb_builtin.c — handler builtin (func_code) estratti dallo switch monolitico di
 * gmb_vm_exec_builtin (task 6.5.2/6.5.3). Ogni case → gmb_bf_<code>() col contratto
 * (vm, args, argc, func_code, gmb_builtin_out_t*, task). Tabella gmb_bf_impl. */


#include "gmb_vm_core.h"
#include "gmb_vm_events.h"
#include "gmb_mcp_exec.h"
#include "gmb_graphics.h"
#include "gmb_ai_server.h"
#include "gmb_model_io.h"
#include "gmb_moestream.h"
#include "llmmodelio_model.h"
#include "llmengine_plugin.h"
#include "gmb_finetune.h"
#include "gmb_distill.h"   /* Stream D: AI DISTILL */
#include <stdlib.h>   /* atof per gli argomenti numerici di FONTS */
#include "gmb_fonts.h"  /* namespace FONTS: runtime condiviso, nessuna logica qui */
#include "gmb_runtime.h"
#include "gmb_llm_tok.h"
#include "gmb_builtin_registry.h"
#include "gmb_grep.h"
#include "gmb_msg.h"        /* MESSAGE.DELIVERY: contatori di consegna (gmb_msg_delivery_stats) */
#include "gmb_doc.h"
/* Hook OPZIONALE di stato (agenti/team/workflow/memoria): registrato dal client managed
 * quando la VM/C è ospitata (motore BSP in C chiamato dal C# via P/Invoke). Se nessuno lo
 * registra (C99Native, gmb_vm_host) ogni getter usa il registro C — comportamento invariato. */
#include "gmb_state_hook.h"

/* ── Hook di stato ospite: helpers condivisi dai getter di agenti/team/workflow/memoria ──
 * Ritornano 1 se l'hook ha risposto (bout valorizzato e handler da chiudere), 0 se la
 * superficie non è gestita (l'handler prosegue sul registro C). */
static int bsp_hook_str(const char* surface, const char* a, const char* b, gmb_builtin_out_t* bout)
{
    char* r = gmb_state_hook_query(surface, a, b);
    if (!r) return 0;
    gmb_obj_string_t* os = gmb_str_cstr(r);
    free(r);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)os;
    bout->result_ref_bt = GMB_BT_STRING;
    return 1;
}

static int bsp_hook_list(const char* surface, const char* a, const char* b, gmb_builtin_out_t* bout)
{
    char* r = gmb_state_hook_query(surface, a, b);
    if (!r) return 0;
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (!out) { free(r); bout->is_num = 0; bout->result_ref = NULL; return 1; }
    char* line = r;
    while (line && *line) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (*line) {
            gmb_obj_string_t* s = gmb_str_cstr(line);
            gmb_list_add(out->list, gmb_make_val_string_counted(s->data));
            gmb_str_release(s);
        }
        line = nl ? nl + 1 : NULL;
    }
    free(r);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)out;
    bout->result_ref_bt = GMB_BT_OBJECT;
    return 1;
}

static int bsp_hook_str3(const char* surface, const char* a, const char* b, const char* c, gmb_builtin_out_t* bout)
{
    char* r = gmb_state_hook_query3(surface, a, b, c);
    if (!r) return 0;
    gmb_obj_string_t* os = gmb_str_cstr(r);
    free(r);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)os;
    bout->result_ref_bt = GMB_BT_STRING;
    return 1;
}

#if !defined(GMB_VM_MIN) && !defined(GMB_VM_SPECIALIZED)
/* Registra l'estrattore PDF per la ricerca trasparente del grep (gmb_vm linka gmb_doc).
 * Hook opzionale: il grep lo usa solo se registrato (nessuna dipendenza a link-time). */
static char* gmbvm_pdf_text(const char* path) {
    void* h = gmb_doc_pdf_open(path);
    if (!h) return NULL;
    int pages = gmb_doc_pdf_page_count(h);
    char* all = NULL; size_t total = 0, cap = 0;
    for (int p = 0; p < pages; p++) {
        char* t = gmb_doc_pdf_extract_text(h, p);
        if (!t) continue;
        size_t tl = strlen(t);
        if (total + tl + 2 > cap) { cap = (total + tl + 2) * 2; char* nb = (char*)realloc(all, cap); if (!nb) { free(t); break; } all = nb; }
        memcpy(all + total, t, tl); total += tl; all[total++] = '\n';
        free(t);
    }
    gmb_doc_pdf_close(h);
    if (!all) return NULL;
    all[total] = 0;
    return all;
}
__attribute__((constructor)) static void gmbvm_register_pdf_grep(void) {
    gmb_grep_set_pdf_extractor(gmbvm_pdf_text);
}
#endif
#include "gmb_module.h"
#include "gmb_ml.h"
#include "gmb_torch.h"
#include "gmb_doc.h"
#include "gmb_db.h"
#include "gmb_strbuf.h"
#include "gmb_adapter.h"
#include "gmb_collections.h"
#include "gmb_graph.h"
#include "gmb_finance.h"
#include "gmb_md.h"
#include <dirent.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include "gmb_commonlib.h"
#include "gmb_matrix.h"
#include "gmb_network.h"
#include "gmb_connector.h"
#include "tool_registry.h"
#include "gmb_cua_runtime.h"
#include "gmb_observation.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_app.h"
#include "gmb_codegen.h"
#include "action_resolver.h"
#include "gmb_browser_cdp.h"
#include "gmb_computer_provider.h"
#include "gmb_agent_loop.h"
#include "gmb_context.h"
#include "gmb_worker.h"

/* GML_VM_REQUIRE in contesto funzione: return al posto di break (nello switch era break). */
#define GML_VM_REQUIRE() if (gmb_ml_load_lib() != 0) { vm_error(task, "TORCH non caricato: esegui gmb ml setup"); return; }

static double vm_num(gmb_stack_entry_t* a) { if (!a) return 0; if (a->type == 1) return a->num; if (a->type == 2 && a->ref && a->ref->kind == GMB_OBJ_STRING) return gmb_parse_num(((gmb_obj_string_t*)a->ref)->data, &(int){0}); return 0; }
static void gmb_bf_0(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { bout->result = args[0].type == 1 ? gmb_int(gmb_round(args[0].num, 0)) : 0; }
}
static void gmb_bf_1(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Argomento NUMERICO (es. valore macro &/EVAL coercizzato dal compilatore
             * con VAL per il target intero): pass-through — parità PCode
             * (StringHelpers.Val(ToString()) → il numero resta il numero). Prima
             * args[0].type != 2 → s="" → gmb_parse_num("") = 0: `z = &("X")` leggeva
             * 10 ma lo store con VAL lo azzerava (bug ordine ClipperMacroCanary). */
            if (!(args[0].type == 2 && args[0].ref)) {
                bout->result = args[0].num;
                return;
            }
            const char* s = ((gmb_obj_string_t*)args[0].ref)->data;
            int ok = 0;
            bout->result = gmb_parse_num(s, &ok);
        }
}
static void gmb_bf_2(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            long t = (long)args[0].num;
            if (t < 0) t = 0;
            if (t > 255) t = 255;
            bout->result = (double)(unsigned char)t;
        }
}
static void gmb_bf_5(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* `bout` e' RIUSATO fra le chiamate: se il ramo che produce il risultato non scrive tutti
             * i campi, restano i valori della chiamata PRECEDENTE (es. result_ref = la lista argomento
             * -> la stampa mostrava "[a, b, c]" invece di "a"). Inizializzazione esplicita. */
            bout->is_num = 0; bout->result = 0; bout->result_ref = NULL; bout->result_ref_bt = 0;
            /* CHOOSE VARIADICO: CHOOSE(i, v1, v2, ...) — l'oracolo e' Choose(index, object?[]),
             * cioe' il compilatore impacchetta i valori. Questo handler copriva SOLO la forma
             * CHOOSE(i, <lista>): con argomenti variadici cadeva nel ramo sbagliato e ritornava
             * "[0]" invece dell'elemento (bug trovato dal sample SystemMiscellaneous: IL dava
             * 'a', la VM/C '[0]'). Selezione 1-based; indice fuori intervallo -> null. */
            if (argc >= 2 && !(args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST))
            {
                int _ci = (int)args[0].num;
                if (_ci >= 1 && _ci < argc)
                {
                    gmb_stack_entry_t v = args[_ci];
                    if (v.type == 1) { bout->is_num = 1; bout->result = v.num; }
                    else if (v.type == 2 && v.ref && v.ref->kind == GMB_OBJ_STRING)
                    {
                        gmb_obj_string_t* _cs = (gmb_obj_string_t*)v.ref;
                        bout->is_num = 0;
                        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(_cs->data ? _cs->data : "");
                        bout->result_ref_bt = GMB_BT_STRING;
                    }
                    else if (v.type == 2 && v.ref)
                    {
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)v.ref;
                        bout->result_ref_bt = GMB_BT_OBJECT;
                    }
                }
                return;
            }
            
            gmb_stack_entry_t idxE = args[0];
            gmb_stack_entry_t arrE = argc >= 2 ? args[1] : args[0];
            if (arrE.type == 2 && arrE.ref && arrE.ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)arrE.ref;
                int n = (int)sv->list->count;
                gmb_list_t* items = sv->list;
                
                /* FLATTEN dei contenitori annidati: il packing del compilatore avvolge la lista
                 * dei valori in contenitori con UN elemento (catena). Profondita' OSSERVATA:
                 * 10 livelli nel sample SystemMiscellaneous, 2 nei casi semplici -> un unwrap
                 * singolo (o un limite piccolo) lasciava selezionare un CONTENITORE invece del
                 * valore, e la VM/C stampava "[a, b, c]" invece di "a".
                 * Guardie: max 64 livelli e stop sul ciclo (stessa lista). */
                for (int _g = 0; _g < 64; _g++)
                {
                    if (!(n == 1 && items->items[0].kind == GMB_TYPE_OBJECT && items->items[0].v_ptr &&
                          ((gmb_obj_t*)items->items[0].v_ptr)->kind == GMB_OBJ_LIST)) break;
                    gmb_obj_shared_val_t* inner = (gmb_obj_shared_val_t*)items->items[0].v_ptr;
                    gmb_list_t* _il = inner->list;
                    if (!_il || _il == items) break;
                    items = _il;
                    n = (int)_il->count;
                }
                int idx = (int)idxE.num - 1;
                if (idx >= 0 && idx < n)
                {
                    gmb_val_t v = items->items[idx];
                    if (v.kind == GMB_TYPE_DOUBLE) { bout->is_num = 1; bout->result = v.v_double; }
                    if (v.kind == GMB_TYPE_INT) { bout->is_num = 1; bout->result = (double)v.v_int; }
                    if (v.kind == GMB_TYPE_BOOL) { bout->is_num = 1; bout->result = v.v_bool ? -1.0 : 0.0; }
                    if (v.kind == GMB_TYPE_STRING && v.v_string.data)
                    {
                        gmb_obj_string_t* s = gmb_str_cstr(v.v_string.data);
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
                    }
                    if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
                    {
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)v.v_ptr;
                        bout->result_ref_bt = GMB_BT_OBJECT;
                    }
                }
            }
            /* (rimosso: sovrascriveva il risultato numerico impostato sopra) */
        }
}
static void gmb_bf_7(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                /* CSTR(stringa) = la stringa stessa (parità ConversionUtils.CStr: nessuno
                 * spazio iniziale). Prima si prendeva args[0].num (0 per una stringa) e si
                 * rendeva " 0": TUTTI i `CONVERSIONS.CSTR(campo stringa)` davano 0
                 * (§4 "Nome: 0", §5/§6/§8, ecc.). */
                const char* sv = ((gmb_obj_string_t*)args[0].ref)->data ? ((gmb_obj_string_t*)args[0].ref)->data : "";
                gmb_obj_string_t* s = gmb_str_cstr(sv);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (args[0].basic_type == GMB_BT_BOOL)
            {
                /* CSTR(booleano) = "True"/"False" (parità ConversionUtils.CStr); il rendering
                 * generico dei bool nel VM-C dà -1/0. */
                gmb_obj_string_t* s = gmb_str_cstr(args[0].num != 0 ? "True" : "False");
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (args[0].basic_type == GMB_BT_CURRENCY)
            {
                /* CSTR(campo DBF N/F) = " 100.00": spazio iniziale (come gmb_str_basic) + la
                 * SCALA del campo (il decimal PCode/IL mantiene i decimali: " 100.00", non " 100"). */
                char cb[80]; int n = 0;
                if (args[0].num >= 0) cb[n++] = ' ';
                snprintf(cb + n, sizeof(cb) - (size_t)n, "%.*f", (int)args[0].scale, args[0].num);
                gmb_obj_string_t* s = gmb_str_cstr(cb);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
            else
            {
                gmb_string_t gs = gmb_val_str(args[0].type == 1 ? args[0].num : 0.0);
                if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
                else bout->is_num = 0;
            }
        }
}
static void gmb_bf_9(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double x = args[0].type == 1 ? args[0].num : 0;
            if (argc >= 2)
            {
                int dec = (int)args[1].num;
                bout->result = gmb_round(x, dec);
            }
            else bout->result = gmb_round(x, 0);
        }
}
static void gmb_bf_clng(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* CLNG: arrotondamento half-to-even (parity IL Convert.ToInt64 / canonical .NET),
     * NON il round() libc (half-away). Usa gmb_round(x, 0) — MidpointRounding.ToEven. */
    bout->result = args[0].type == 1 ? gmb_round(args[0].num, 0)
        : (args[0].type == 2 && args[0].ref) ? gmb_round(gmb_parse_num(((gmb_obj_string_t*)args[0].ref)->data, &(int){0}), 0) : 0;
}
static void gmb_bf_fix(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = gmb_fix(vm_num(&args[0]));
}
static void gmb_bf_int(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = gmb_int(vm_num(&args[0]));
}
static void gmb_bf_floor(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = gmb_floor(vm_num(&args[0]));
}
static void gmb_bf_trunc(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = gmb_trunc(vm_num(&args[0]));
}
static void gmb_bf_ceiling(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = gmb_ceiling(vm_num(&args[0]));
}
/* ATN(v) = arctan(v); ATN(y, x) = atan2(y, x) — 2° argomento OPZIONALE (arity 1..2 dalla
 * registry canonica: un solo builtin, nessun token "ATN2"). Parity: IlRuntime.Atn/Atn2,
 * PCodeProcessor.Builtins[ATN], C99Native MATH_TBL (righe arity 1 e 2 su GMB_TOK_Atn). */
static void gmb_bf_atan(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    if (argc >= 2)
        bout->result = atan2(vm_num(&args[0]), vm_num(&args[1]));
    else
        bout->result = atan(vm_num(&args[0]));
}
static void gmb_bf_cos(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = cos(vm_num(&args[0]));
}
static void gmb_bf_exp(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = exp(vm_num(&args[0]));
}
static void gmb_bf_log(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = log(vm_num(&args[0]));
}
static void gmb_bf_rnd(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* RND non accetta argomenti: genera un numero in [0,1). Ignorarli in silenzio (era
     * cosi') accettava codice non valido — RND(1) e RND() davano lo stesso risultato senza
     * nessun avviso. Ora e' un errore esplicito, come su IL (che era l'unico corretto:
     * RND(n) -> 1..n e' la semantica di Clipper, non di GMBasic). */
    if (argc > 0) { vm_error(task, "RND non accetta argomenti"); return; }
    (void)args;
    bout->result = gmb_rnd();
}
static void gmb_bf_sgn(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = args[0].type == 1 ? (args[0].num > 0 ? 1.0 : args[0].num < 0 ? -1.0 : 0.0) : 0;
}
static void gmb_bf_sin(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = sin(vm_num(&args[0]));
}
static void gmb_bf_sqr(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = gmb_sqr(vm_num(&args[0]));
}
static void gmb_bf_abs(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = args[0].type == 1 ? gmb_abs(args[0].num)
        : (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            ? gmb_abs(gmb_parse_num(((gmb_obj_string_t*)args[0].ref)->data, &(int){0}))
            : 0;
}
static void gmb_bf_tan(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = tan(vm_num(&args[0]));
}
static void gmb_bf_log10(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = log10(vm_num(&args[0]));
}
static void gmb_bf_asin(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = asin(vm_num(&args[0]));
}
static void gmb_bf_acos(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = acos(vm_num(&args[0]));
}
static void gmb_bf_sinh(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = sinh(vm_num(&args[0]));
}
static void gmb_bf_cosh(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    bout->result = cosh(vm_num(&args[0]));
}
static void gmb_bf_13(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t gs = gmb_num_to_hex(args[0].num);
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else bout->is_num = 0;
        }
}
static void gmb_bf_16(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t gs = gmb_num_to_oct(args[0].num);
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else bout->is_num = 0;
        }
}
static void gmb_bf_17(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            long v = (long)args[0].num;
            char buf[80]; int p = 0;
            if (v == 0) { buf[p++] = '0'; }
            else while (v > 0) { buf[p++] = (char)('0' + (v & 1)); v >>= 1; }
            
            for (int i = 0; i < p / 2; i++) { char t = buf[i]; buf[i] = buf[p-1-i]; buf[p-1-i] = t; }
            buf[p] = '\0';
            gmb_obj_string_t* s = gmb_str_cstr(buf);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_18(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 1) bout->result = 1.0;
            else if (args[0].type == 2 && args[0].ref)
                bout->result = gmb_is_numeric(((gmb_obj_string_t*)args[0].ref)->data) ? -1.0 : 0.0;
            else bout->result = 0.0;
        }
}
static void gmb_bf_19(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_fact(args[0].num);
        }
}
static void gmb_bf_55(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_comb(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_56(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_perm(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_57(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mean = argc >= 2 ? args[1].num : 0, sd = argc >= 3 ? args[2].num : 1;
            bout->result = func_code == 57 ? gmb_dist_normal_pdf(mean, sd, args[0].num)
                         : func_code == 1042 ? 1.0 - gmb_dist_normal_cdf(mean, sd, args[0].num)
                         : gmb_dist_normal_cdf(mean, sd, args[0].num);
        }
}
static void gmb_bf_61(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double df = argc >= 2 ? args[1].num : 1;
            if (func_code == 61) bout->result = gmb_dist_chi_pdf(df, args[0].num);
            else if (func_code == 63) bout->result = gmb_dist_chi2_pdf(df, args[0].num);
            else if (func_code == 64) bout->result = gmb_dist_chi2_cdf(df, args[0].num);
            else if (func_code == 1039) bout->result = 1.0 - gmb_dist_chi2_cdf(df, args[0].num);
            else if (func_code == 1051) bout->result = 1.0 - gmb_dist_chi_cdf(df, args[0].num);
            else bout->result = gmb_dist_chi_cdf(df, args[0].num);
        }
}
static void gmb_bf_65(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = func_code == 65 ? gmb_dist_beta_pdf(args[1].num, args[2].num, args[0].num)
                         : func_code == 1050 ? 1.0 - gmb_dist_beta_cdf(args[1].num, args[2].num, args[0].num)
                         : gmb_dist_beta_cdf(args[1].num, args[2].num, args[0].num);
        }
}
static void gmb_bf_67(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = func_code == 67 ? gmb_dist_student_t_pdf(args[1].num, args[0].num)
                         : func_code == 1038 ? 1.0 - gmb_dist_student_t_cdf(args[1].num, args[0].num)
                         : gmb_dist_student_t_cdf(args[1].num, args[0].num);
        }
}
static void gmb_bf_69(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = func_code == 69 ? gmb_dist_weibull_pdf(args[1].num, args[2].num, args[0].num)
                         : func_code == 1040 ? 1.0 - gmb_dist_weibull_cdf(args[1].num, args[2].num, args[0].num)
                         : gmb_dist_weibull_cdf(args[1].num, args[2].num, args[0].num);
        }
}
static void gmb_bf_73(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = func_code == 73 ? gmb_dist_exp_pdf(args[1].num, args[0].num)
                         : func_code == 1037 ? 1.0 - gmb_dist_exp_cdf(args[1].num, args[0].num)
                         : gmb_dist_exp_cdf(args[1].num, args[0].num);
        }
}
static void gmb_bf_59(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mu = argc >= 2 ? args[1].num : 0, sigma = argc >= 3 ? args[2].num : 1;
            bout->result = func_code == 59 ? gmb_dist_lognormal_pdf(mu, sigma, args[0].num)
                         : func_code == 1043 ? 1.0 - gmb_dist_lognormal_cdf(mu, sigma, args[0].num)
                         : gmb_dist_lognormal_cdf(mu, sigma, args[0].num);
        }
}
static void gmb_bf_71(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = func_code == 71 ? gmb_dist_f_pdf(args[1].num, args[2].num, args[0].num)
                         : func_code == 1041 ? 1.0 - gmb_dist_f_cdf(args[1].num, args[2].num, args[0].num)
                         : gmb_dist_f_cdf(args[1].num, args[2].num, args[0].num);
        }
}
static void gmb_bf_75(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (func_code == 75) bout->result = gmb_dist_pareto_pdf(args[1].num, args[2].num, args[0].num);
            else bout->result = gmb_dist_logistic_pdf(args[1].num, args[2].num, args[0].num);
        }
}
static void gmb_bf_77(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mn = argc >= 2 ? args[1].num : 0, md = argc >= 3 ? args[2].num : 0.5, mx = argc >= 4 ? args[3].num : 1;
            /* gmb_dist_triangular_pdf vuole (min, MAX, MODE, x): il registro BASIC è (x, min,
             * mode, max). Prima passava (mn, md, mx) → pdf sbagliata (TRIANGULARPDF(0.5)=0). */
            bout->result = gmb_dist_triangular_pdf(mn, mx, md, args[0].num);
        }
}
static void gmb_bf_78(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_uniform_pdf(args[1].num, args[2].num, args[0].num);
        }
}
static void gmb_bf_79(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mean = argc >= 2 ? args[1].num : 0, sd = argc >= 3 ? args[2].num : 1, skew = argc >= 4 ? args[3].num : 0;
            bout->result = gmb_dist_skewnormal_pdf(args[0].num, mean, sd, skew);
        }
        
}
static void gmb_bf_870(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mu = argc >= 2 ? args[1].num : 0, sd = argc >= 3 ? args[2].num : 1;
            bout->result = (func_code == 870) ? gmb_dist_normal_ppf(mu, sd, args[0].num) : gmb_dist_normal_isf(mu, sd, args[0].num);
        }
}
static void gmb_bf_872(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mu = argc >= 2 ? args[1].num : 0, sg = argc >= 3 ? args[2].num : 1;
            bout->result = gmb_dist_lognormal_ppf(mu, sg, args[0].num);
        }
}
static void gmb_bf_873(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double df = argc >= 2 ? args[1].num : 1;
            bout->result = (func_code == 873) ? gmb_dist_chi2_ppf(df, args[0].num) : gmb_dist_chi2_isf(df, args[0].num);
        }
}
static void gmb_bf_875(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double df = argc >= 2 ? args[1].num : 1;
            bout->result = gmb_dist_chi_ppf(df, args[0].num);
        }
}
static void gmb_bf_876(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double a = argc >= 2 ? args[1].num : 1, b = argc >= 3 ? args[2].num : 1;
            bout->result = gmb_dist_beta_ppf(a, b, args[0].num);
        }
}
static void gmb_bf_877(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double d1 = argc >= 2 ? args[1].num : 1, d2 = argc >= 3 ? args[2].num : 1;
            bout->result = (func_code == 877) ? gmb_dist_f_ppf(d1, d2, args[0].num) : gmb_dist_f_isf(d1, d2, args[0].num);
        }
}
static void gmb_bf_879(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double df = argc >= 2 ? args[1].num : 1;
            bout->result = (func_code == 879) ? gmb_dist_student_t_ppf(df, args[0].num) : gmb_dist_student_t_isf(df, args[0].num);
        }
}
static void gmb_bf_881(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double rate = argc >= 2 ? args[1].num : 1;
            bout->result = (func_code == 881) ? gmb_dist_exp_ppf(rate, args[0].num) : gmb_dist_exp_isf(rate, args[0].num);
        }
}
static void gmb_bf_883(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double sh = argc >= 2 ? args[1].num : 1, sc = argc >= 3 ? args[2].num : 1;
            bout->result = gmb_dist_weibull_ppf(sh, sc, args[0].num);
        }
}
static void gmb_bf_884(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double loc = argc >= 2 ? args[1].num : 0, sc = argc >= 3 ? args[2].num : 1;
            bout->result = gmb_dist_logistic_ppf(loc, sc, args[0].num);
        }
}
static void gmb_bf_885(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double a = argc >= 2 ? args[1].num : 0, b = argc >= 3 ? args[2].num : 1;
            bout->result = gmb_dist_uniform_ppf(a, b, args[0].num);
        }
}
static void gmb_bf_886(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double sc = argc >= 2 ? args[1].num : 1, sh = argc >= 3 ? args[2].num : 1;
            bout->result = gmb_dist_pareto_ppf(sc, sh, args[0].num);
        }
}
static void gmb_bf_887(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double mn = argc >= 2 ? args[1].num : 0, md = argc >= 3 ? args[2].num : 0.5, mx = argc >= 4 ? args[3].num : 1;
            bout->result = gmb_dist_triangular_ppf(mn, mx, md, args[0].num);
        }
        
}
static void gmb_bf_1000_c1000(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t xv = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t yv = gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_pearsonr(xv, yv);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1064(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_chi_isf(argc >= 2 ? args[1].num : 1, args[0].num);
}
static void gmb_bf_1081(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double a = argc >= 2 ? args[1].num : 1, b = argc >= 3 ? args[2].num : 1, nc = argc >= 4 ? args[3].num : 0;
        bout->result = func_code == 1081 ? gmb_dist_nc_beta_pdf(a, b, nc, args[0].num)
                      : func_code == 1082 ? gmb_dist_nc_beta_cdf(a, b, nc, args[0].num)
                      : gmb_dist_nc_beta_sf(a, b, nc, args[0].num);
}
static void gmb_bf_1087(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double d1 = argc >= 2 ? args[1].num : 1, d2 = argc >= 3 ? args[2].num : 1, nc = argc >= 4 ? args[3].num : 0;
        bout->result = func_code == 1087 ? gmb_dist_nc_f_pdf(d1, d2, nc, args[0].num)
                      : func_code == 1088 ? gmb_dist_nc_f_cdf(d1, d2, nc, args[0].num)
                      : gmb_dist_nc_f_sf(d1, d2, nc, args[0].num);
}
static void gmb_bf_1090(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double df = argc >= 2 ? args[1].num : 1, nc = argc >= 3 ? args[2].num : 0;
        bout->result = func_code == 1090 ? gmb_dist_nc_t_pdf(df, nc, args[0].num)
                      : func_code == 1091 ? gmb_dist_nc_t_cdf(df, nc, args[0].num)
                      : gmb_dist_nc_t_sf(df, nc, args[0].num);
}
static void gmb_bf_3876(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double a0 = argc > 0 ? args[0].num : 0, a1 = argc > 1 ? args[1].num : 0;
        double a2 = argc > 2 ? args[2].num : 0, a3 = argc > 3 ? args[3].num : 0;
        switch (func_code) {
        case 3876: bout->result = gmb_mv_bo_ei(a0, a1, a2, a3); break;
        case 3877: bout->result = gmb_mv_bo_pi(a0, a1, a2, a3); break;
        case 3878: bout->result = gmb_mv_bo_ucb(a0, a1, a2); break;
        case 3880: bout->result = gmb_mv_gambler_prob(a0, a1, a2); break;
        case 3881: bout->result = gmb_mv_gambler_duration(a0, a1, a2); break;
        default:   bout->result = 0.0; break;
        }
}
static void gmb_bf_3879(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int nx = 0, ny = 0, nc = 0;
        double* x = gmb_vm_list_to_dbl(&args[0], &nx);
        double* y = gmb_vm_list_to_dbl(&args[1], &ny);
        double* cd = gmb_vm_list_to_dbl(&args[2], &nc);
        gmb_val_t r = gmb_mv_bo_next(x, y, nx, cd, nc,
                argc >= 4 ? args[3].num : 1.0, argc >= 5 ? args[4].num : 0.01,
                argc >= 6 ? args[5].num : 2.0);
        free(x); free(y); free(cd);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3883(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int nt = 0, ny = 0, np = 0;
        double* t = gmb_vm_list_to_dbl(&args[0], &nt);
        double* y = gmb_vm_list_to_dbl(&args[1], &ny);
        double* ps = gmb_vm_list_to_dbl(&args[2], &np);
        bout->result = (func_code == 3882) ? gmb_mv_ate_ipw(t, y, ps, nt) : gmb_mv_ate_matching(t, y, ps, nt);
        free(t); free(y); free(ps);
}
typedef struct { gmb_closure_ctx_t cc; int n; } gmb_tpe_vm_ctx_t;
static void gmb_bf_3885(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double a = argc > 0 ? args[0].num : 0, b = argc > 1 ? args[1].num : 0;
        double s = argc > 1 ? args[1].num : 0;
        double t = argc > 2 ? args[2].num : 0;
        switch (func_code) {
        case 3885: { double v = 0; gmb_vector_get((int64_t)a, (int)b, &v); bout->result = v; break; }
        case 3886: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 0); break;
        case 3887: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 1); break;
        case 3888: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 2); break;
        case 3889: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 3); break;
        case 3890: bout->result = (double)gmb_vector_op((int64_t)a, -1, s, 0, 4); break;
        case 3891: bout->result = (double)gmb_vector_op((int64_t)a, -1, 0, 0, 5); break;
        case 3892: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, t, 6); break;
        case 3893: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 7); break;
        case 3894: bout->result = gmb_vector_angle_h((int64_t)a, (int64_t)b); break;
        case 3895: bout->result = gmb_vector_distance_h((int64_t)a, (int64_t)b); break;
        case 3896: bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 8); break;
        default:   bout->result = (double)gmb_vector_op((int64_t)a, (int64_t)b, 0, 0, 9); break;
        }
}
static void gmb_bf_3859(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* x = gmb_vm_list_to_dbl(&args[0], &n);
        gmb_val_t r = gmb_mv_gmm_em_1d(x, n, (int)(argc >= 2 ? args[1].num : 1), (int)(argc >= 3 ? args[2].num : 50));
        free(x);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3857(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0, d = 0, ny = 0;
        double* flat = NULL; double* y = gmb_vm_list_to_dbl(&args[1], &ny);
        if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
        {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2) {
                        n = (int)sv->dim_upper[0] + 1; d = (int)sv->dim_upper[1] + 1;
                        int cnt = (int)sv->list->count;
                        flat = (double*)malloc((size_t)cnt * sizeof(double));
                        if (flat) for (int i = 0; i < cnt; i++) { gmb_val_t v = sv->list->items[i]; flat[i] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                } else if (sv->list->count > 0) {
                        n = (int)sv->list->count;
                        for (int i = 0; i < n; i++) { gmb_val_t rv = sv->list->items[i]; if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) { d = (int)((gmb_obj_shared_val_t*)rv.v_ptr)->list->count; break; } }
                        if (d > 0) {
                                flat = (double*)malloc((size_t)n * d * sizeof(double));
                                if (flat) for (int i = 0; i < n; i++) {
                                        gmb_val_t rv = sv->list->items[i];
                                        if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) {
                                                gmb_obj_shared_val_t* rl = (gmb_obj_shared_val_t*)rv.v_ptr;
                                                for (int q = 0; q < d && q < (int)rl->list->count; q++) { gmb_val_t v = rl->list->items[q]; flat[i*d+q] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                                        }
                                }
                        }
                }
        }
        gmb_val_t r = gmb_mv_svm_fit_dict(flat, y, n, d, (int)(argc >= 3 ? args[2].num : 2),
                                          argc >= 4 ? args[3].num : 1.0, (int)(argc >= 5 ? args[4].num : 100));
        free(flat); free(y);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3858(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        int nw = 0, nx = 0;
        double* W = gmb_vm_list_to_dbl(&args[0], &nw);
        double* x = gmb_vm_list_to_dbl(&args[1], &nx);
        (void)nw;
        bout->result = (double)gmb_mv_svm_predict(W, x, (int)(argc >= 3 ? args[2].num : nx), (int)(argc >= 4 ? args[3].num : 1));
        free(W); free(x);
}
static void gmb_bf_3856(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int nx = 0, ny = 0, ns = 0;
        double* x = gmb_vm_list_to_dbl(&args[0], &nx);
        double* y = gmb_vm_list_to_dbl(&args[1], &ny);
        double* xs = gmb_vm_list_to_dbl(&args[2], &ns);
        gmb_val_t r = gmb_mv_gp_regress(x, y, nx, xs, ns,
                                        argc >= 4 ? args[3].num : 1.0, argc >= 5 ? args[4].num : 0.01);
        free(x); free(y); free(xs);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3849(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        switch (func_code) {
        case 3849: bout->result = gmb_mv_rmt_lambda_plus(argc >= 1 ? (int)args[0].num : 1, argc >= 2 ? (int)args[1].num : 1, argc >= 3 ? args[2].num : 1.0); break;
        case 3850: bout->result = gmb_mv_rmt_lambda_minus(argc >= 1 ? (int)args[0].num : 1, argc >= 2 ? (int)args[1].num : 1, argc >= 3 ? args[2].num : 1.0); break;
        case 3851: { int n = 0; double* v = gmb_vm_list_to_dbl(&args[0], &n); bout->result = gmb_mv_rmt_coherence(v, n); free(v); break; }
        case 3852: { int n = 0; double* A = gmb_vm_list_to_dbl(&args[0], &n); bout->result = gmb_mv_rmt_spectral_norm(A, (int)(argc >= 2 ? args[1].num : 1)); free(A); break; }
        case 3853: bout->result = gmb_mv_rmt_davis_kahan(args[0].num, argc >= 2 ? args[1].num : 0.0); break;
        case 3854: { int nu = 0, nv = 0; double* u = gmb_vm_list_to_dbl(&args[0], &nu); double* v = gmb_vm_list_to_dbl(&args[1], &nv); bout->result = gmb_mv_rmt_principal_angle(u, v, nu < nv ? nu : nv); free(u); free(v); break; }
        default:   bout->result = gmb_mv_rmt_tightness(args[0].num, argc >= 2 ? args[1].num : 0.0); break;
        }
}
static void gmb_bf_1095(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        int nx = 0, nl = 0, nc = 0;
        double* x = gmb_vm_list_to_dbl(&args[0], &nx);
        double* lo = gmb_vm_list_to_dbl(&args[1], &nl);
        double* cv = gmb_vm_list_to_dbl(&args[2], &nc);
        (void)nl; (void)nc;
        bout->result = func_code == 1095 ? gmb_mv_gauss_pdf(nx, x, lo, cv)
                      : gmb_mv_student_pdf(nx, x, lo, cv, argc >= 4 ? args[3].num : 1.0);
        free(x); free(lo); free(cv);
}
static void gmb_bf_1097(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double u = args[0].num, v = argc >= 2 ? args[1].num : 0, th = argc >= 3 ? args[2].num : 1;
        bout->result = func_code == 1097 ? gmb_copula_clayton_cdf(u, v, th)
                      : func_code == 1098 ? gmb_copula_gumbel_cdf(u, v, th)
                      : gmb_copula_frank_cdf(u, v, th);
}
static void gmb_bf_1093(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double lambda = argc >= 2 ? args[1].num : 1;
        bout->result = func_code == 1093 ? gmb_dist_ztpoisson_pmf(lambda, (int)args[0].num)
                                        : gmb_dist_ztpoisson_cdf(lambda, (int)args[0].num);
}
static void gmb_bf_1084(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double shape = argc >= 2 ? args[1].num : 1, nc = argc >= 3 ? args[2].num : 0;
        bout->result = func_code == 1084 ? gmb_dist_nc_gamma_pdf(shape, nc, args[0].num)
                      : func_code == 1085 ? gmb_dist_nc_gamma_cdf(shape, nc, args[0].num)
                      : gmb_dist_nc_gamma_sf(shape, nc, args[0].num);
}
static void gmb_bf_1078(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double df = argc >= 2 ? args[1].num : 1, nc = argc >= 3 ? args[2].num : 0;
        double v = func_code == 1078 ? gmb_dist_nc_chi2_pdf(df, nc, args[0].num)
                 : func_code == 1079 ? gmb_dist_nc_chi2_cdf(df, nc, args[0].num)
                 : gmb_dist_nc_chi2_sf(df, nc, args[0].num);
        bout->result = v;
}
static void gmb_bf_1077(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        bout->result = gmb_ts_lyapunov(a, n, (int)args[1].num, (int)args[2].num);
        free(a);
}
static void gmb_bf_3848(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0, k = 0;
        double* flat = NULL;
        if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
        {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2) {
                        n = (int)sv->dim_upper[0] + 1; k = (int)sv->dim_upper[1] + 1;
                        int cnt = (int)sv->list->count;
                        flat = (double*)malloc((size_t)cnt * sizeof(double));
                        if (flat) for (int i = 0; i < cnt; i++) { gmb_val_t v = sv->list->items[i]; flat[i] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                } else if (sv->list->count > 0) {
                        n = (int)sv->list->count;
                        for (int i = 0; i < n; i++) { gmb_val_t rv = sv->list->items[i]; if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) { k = (int)((gmb_obj_shared_val_t*)rv.v_ptr)->list->count; break; } }
                        if (k > 0) {
                                flat = (double*)malloc((size_t)n * k * sizeof(double));
                                if (flat) for (int i = 0; i < n; i++) {
                                        gmb_val_t rv = sv->list->items[i];
                                        if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) {
                                                gmb_obj_shared_val_t* rl = (gmb_obj_shared_val_t*)rv.v_ptr;
                                                for (int q = 0; q < k && q < (int)rl->list->count; q++) { gmb_val_t v = rl->list->items[q]; flat[i*k+q] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                                        }
                                }
                        }
                }
        }
        gmb_val_t r = gmb_ts_johansen_flat(flat, n, k, (int)(argc >= 2 ? args[1].num : 1));
        free(flat);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3846(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        gmb_val_t r = gmb_ts_dfgls_flat(a, n, (int)(argc >= 2 ? args[1].num : 1), (int)(argc >= 3 ? args[2].num : 0));
        free(a);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3847(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0, k = 0;
        double* flat = NULL;
        if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
        {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2) {
                        n = (int)sv->dim_upper[0] + 1; k = (int)sv->dim_upper[1] + 1;
                        int cnt = (int)sv->list->count;
                        flat = (double*)malloc((size_t)cnt * sizeof(double));
                        if (flat) for (int i = 0; i < cnt; i++) { gmb_val_t v = sv->list->items[i]; flat[i] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                } else if (sv->list->count > 0) {
                        n = (int)sv->list->count;
                        for (int i = 0; i < n; i++) { gmb_val_t rv = sv->list->items[i]; if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) { k = (int)((gmb_obj_shared_val_t*)rv.v_ptr)->list->count; break; } }
                        if (k > 0) {
                                flat = (double*)malloc((size_t)n * k * sizeof(double));
                                if (flat) for (int i = 0; i < n; i++) {
                                        gmb_val_t rv = sv->list->items[i];
                                        if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) {
                                                gmb_obj_shared_val_t* rl = (gmb_obj_shared_val_t*)rv.v_ptr;
                                                for (int q = 0; q < k && q < (int)rl->list->count; q++) { gmb_val_t v = rl->list->items[q]; flat[i*k+q] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                                        }
                                }
                        }
                }
        }
        gmb_val_t r = gmb_ts_engle_granger_flat(flat, n, k, (int)(argc >= 2 ? args[1].num : 0),
                                                (int)(argc >= 3 ? args[2].num : 1), (int)(argc >= 4 ? args[3].num : 1));
        free(flat);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3844(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        gmb_val_t r = gmb_ts_kpss_flat(a, n, (int)(argc >= 2 ? args[1].num : -1), (int)(argc >= 3 ? args[2].num : 0));
        free(a);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3845(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[1], &n);
        gmb_val_t r = gmb_ts_qlr_flat(a, n, args[0].num, argc >= 3 ? args[2].num : 0.0);
        free(a);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_3823(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double p = args[0].num;
        switch (func_code) {
        case 3823: bout->result = gmb_dist_nc_chi2_ppf(argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0, p); break;
        case 3824: bout->result = gmb_dist_nc_chi2_isf(argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0, p); break;
        case 3827: bout->result = gmb_dist_nc_gamma_ppf(argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0, p); break;
        case 3828: bout->result = gmb_dist_nc_gamma_isf(argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0, p); break;
        case 3831: bout->result = gmb_dist_nc_t_ppf(argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0, p); break;
        default:   bout->result = gmb_dist_nc_t_isf(argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0, p); break;
        }
}
static void gmb_bf_3825(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double p = args[0].num;
        double a = argc >= 2 ? args[1].num : 1, b = argc >= 3 ? args[2].num : 1, nc = argc >= 4 ? args[3].num : 0;
        bout->result = func_code == 3825 ? gmb_dist_nc_beta_ppf(a, b, nc, p) : gmb_dist_nc_beta_isf(a, b, nc, p);
}
static void gmb_bf_3829(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double p = args[0].num;
        double d1 = argc >= 2 ? args[1].num : 1, d2 = argc >= 3 ? args[2].num : 1, nc = argc >= 4 ? args[3].num : 0;
        bout->result = func_code == 3829 ? gmb_dist_nc_f_ppf(d1, d2, nc, p) : gmb_dist_nc_f_isf(d1, d2, nc, p);
}
static void gmb_bf_3820(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        int np = 0, na = 0, ne = 0;
        double* p = gmb_vm_list_to_dbl(&args[0], &np);
        double* A = gmb_vm_list_to_dbl(&args[1], &na);
        double* E = gmb_vm_list_to_dbl(&args[2], &ne);
        int n = np > 0 ? ne / np : 0;
        (void)na;
        if (func_code == 3820) {
                bout->result = gmb_ts_hmm_forward(p, A, E, np, n);
        } else {
                int outn = 0;
                double* path = gmb_ts_hmm_viterbi(p, A, E, np, n, &outn);
                gmb_vm_set_result_list(path, outn, &bout->is_num, &bout->result_ref);
        }
        free(p); free(A); free(E);
}
static void gmb_bf_3822(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int K = argc >= 2 ? (int)args[1].num : 1;
        int it = argc >= 3 ? (int)args[2].num : 100;
        gmb_val_t r = gmb_ts_hmm_fit(NULL, 0, 0, 0, 0);
        if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
        {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2)
                {
                        int n = (int)sv->dim_upper[0] + 1, d = (int)sv->dim_upper[1] + 1;
                        int cnt = (int)sv->list->count;
                        double* flat = (double*)malloc((size_t)cnt * sizeof(double));
                        if (flat) {
                                for (int i = 0; i < cnt; i++) { gmb_val_t v = sv->list->items[i]; flat[i] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                                r = gmb_ts_hmm_fit(flat, n, d, K, it);
                                free(flat);
                        }
                }
                else if (sv->list->count > 0)
                {
                        int n = (int)sv->list->count, d = 0;
                        for (int i = 0; i < n; i++) {
                                gmb_val_t rv = sv->list->items[i];
                                if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) { d = (int)((gmb_obj_shared_val_t*)rv.v_ptr)->list->count; break; }
                        }
                        if (d > 0) {
                                double* flat = (double*)malloc((size_t)n * d * sizeof(double));
                                if (flat) {
                                        for (int i = 0; i < n; i++) {
                                                gmb_val_t rv = sv->list->items[i];
                                                if (rv.kind == GMB_TYPE_OBJECT && rv.v_ptr && ((gmb_obj_t*)rv.v_ptr)->kind == GMB_OBJ_LIST) {
                                                        gmb_obj_shared_val_t* rl = (gmb_obj_shared_val_t*)rv.v_ptr;
                                                        for (int q = 0; q < d && q < (int)rl->list->count; q++) { gmb_val_t v = rl->list->items[q]; flat[i*d+q] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                                                }
                                        }
                                        r = gmb_ts_hmm_fit(flat, n, d, K, it);
                                        free(flat);
                                }
                        }
                }
        }
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1076(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        int outn = 0;
        double* out = gmb_ts_bma_weights(a, n, &outn);
        free(a);
        gmb_vm_set_result_list(out, outn, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1075(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        bout->result = gmb_ts_lz_complexity(a, n);
        free(a);
}
static void gmb_bf_1074(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        gmb_val_t r = gmb_ts_ou_fit_flat(a, n, argc >= 2 ? args[1].num : 1.0);
        free(a);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1073(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        bout->result = gmb_ts_perm_entropy(a, n, (int)args[1].num, (int)args[2].num);
        free(a);
}
static void gmb_bf_1072(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        bout->result = gmb_ts_approx_entropy(a, n, (int)args[1].num, args[2].num);
        free(a);
}
static void gmb_bf_1071(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        bout->result = gmb_ts_sample_entropy(a, n, (int)args[1].num, args[2].num);
        free(a);
}
static void gmb_bf_1070(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* x = gmb_vm_list_to_dbl(&args[0], &n);
        int outn = 0;
        double* out = gmb_ts_stationary_bootstrap(x, n, (int)args[1].num, (int)args[2].num, &outn);
        free(x);
        gmb_vm_set_result_list(out, outn, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1069(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* z = gmb_vm_list_to_dbl(&args[0], &n);
        int outn = 0;
        double* out = gmb_ts_kalman1d(z, n, argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 1,
                                     argc >= 4 ? args[3].num : 0.01, argc >= 5 ? args[4].num : 1, &outn);
        free(z);
        gmb_vm_set_result_list(out, outn, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1068(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        gmb_val_t r = gmb_ts_garch11_flat(a, n);
        free(a);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1067(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int nx = 0, ny = 0;
        double* x = gmb_vm_list_to_dbl(&args[0], &nx);
        double* y = gmb_vm_list_to_dbl(&args[1], &ny);
        gmb_val_t r = gmb_ts_granger_flat(x, nx, y, ny, (int)(argc >= 3 ? args[2].num : 1));
        free(x); free(y);
        bout->result = 0;
        gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1066(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        int outn = 0;
        double* out = gmb_ts_fracdiff(a, n, argc >= 2 ? args[1].num : 0.0, &outn);
        free(a);
        gmb_vm_set_result_list(out, outn, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1065(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        int n = 0;
        double* a = gmb_vm_list_to_dbl(&args[0], &n);
        bout->result = gmb_hurst_dfa(a, n, 4);
        free(a);
}
static void gmb_bf_1063(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* KRUSKAL su gruppi: matrice rettangolare (flat row-major) o lista di liste.
     * `gmb_kruskal_flat` è il canone condiviso (stessa semantica del C99/IL). */
    int k = 0;
    int* sizes = NULL;
    double* g = argc >= 1 ? gmb_vm_groups_to_flat(&args[0], &k, &sizes) : NULL;
    gmb_val_t r = g ? gmb_kruskal_flat(g, sizes, k) : gmb_val_nil();
    free(g); free(sizes);
    bout->result = 0;
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1059(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_beta_isf(args[1].num, args[2].num, args[0].num);
}
static void gmb_bf_1060(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_logistic_isf(args[1].num, args[2].num, args[0].num);
}
static void gmb_bf_1061(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_lognormal_isf(argc >= 2 ? args[1].num : 0, argc >= 3 ? args[2].num : 1, args[0].num);
}
static void gmb_bf_1062(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_triangular_isf(argc >= 2 ? args[1].num : 0, argc >= 4 ? args[3].num : 1, argc >= 3 ? args[2].num : 0.5, args[0].num);
}
static void gmb_bf_1056(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_weibull_isf(args[1].num, argc >= 3 ? args[2].num : 1, args[0].num);
}
static void gmb_bf_1057(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_uniform_isf(argc >= 2 ? args[1].num : 0, argc >= 3 ? args[2].num : 1, args[0].num);
}
static void gmb_bf_1058(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_dist_pareto_isf(args[1].num, argc >= 3 ? args[2].num : 1, args[0].num);
}
static void gmb_bf_1054(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double mn = argc >= 2 ? args[1].num : 0, md = argc >= 3 ? args[2].num : 0.5, mx = argc >= 4 ? args[3].num : 1;
        double c = gmb_dist_triangular_cdf(mn, mx, md, args[0].num);
        bout->result = func_code == 1055 ? 1.0 - c : c;
}
static void gmb_bf_1052(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double mu = argc >= 2 ? args[1].num : 0, sc = argc >= 3 ? args[2].num : 1;
        double c = gmb_dist_logistic_cdf(mu, sc, args[0].num);
        bout->result = func_code == 1053 ? 1.0 - c : c;
}
static void gmb_bf_1048(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    int nx = 0, ny = 0;
    double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nx) : NULL;
    double* y = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &ny) : NULL;
    gmb_val_t r = gmb_kendalltau_flat(x, nx, y, ny);
    bout->result = 0;
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
    free(x); free(y);
}
static void gmb_bf_1049(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    int nx = 0, ny = 0;
    double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nx) : NULL;
    double* y = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &ny) : NULL;
    gmb_val_t r = gmb_ftest_flat(x, nx, y, ny);
    bout->result = 0;
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
    free(x); free(y);
}
static void gmb_bf_1046(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double a = argc >= 2 ? args[1].num : 0, b = argc >= 3 ? args[2].num : 1;
        double c = gmb_dist_uniform_cdf(a, b, args[0].num);
        bout->result = func_code == 1047 ? 1.0 - c : c;
}
static void gmb_bf_1044(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task;
        double sc = argc >= 2 ? args[1].num : 1, sh = argc >= 3 ? args[2].num : 1;
        double c = gmb_dist_pareto_cdf(sc, sh, args[0].num);
        bout->result = func_code == 1045 ? 1.0 - c : c;
}
static void gmb_bf_1036(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        bout->result = gmb_gamma_sf(args[0].num, argc >= 2 ? args[1].num : 1, argc >= 3 ? args[2].num : 0);
}
static void gmb_bf_1000_c1001(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t xv = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t yv = gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_spearmanr(xv, yv);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1000_c1002(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t xv = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t yv = gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_linregress(xv, yv);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1000_c1004(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t xv = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t yv = gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_mannwhitney(xv, yv);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1000_c1005(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t xv = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t yv = gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_wilcoxon(xv, yv);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_772_c772(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t b = (func_code == 778 || func_code == 779) ? gmb_vm_arg_list(NULL) : gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_ttest(a, b);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_772_c773(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t b = (func_code == 778 || func_code == 779) ? gmb_vm_arg_list(NULL) : gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_ttestpaired(a, b);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_772_c776(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t b = (func_code == 778 || func_code == 779) ? gmb_vm_arg_list(NULL) : gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_chi2(a, b);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_772_c778(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t b = (func_code == 778 || func_code == 779) ? gmb_vm_arg_list(NULL) : gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_shapiro(a);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_772_c779(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t b = (func_code == 778 || func_code == 779) ? gmb_vm_arg_list(NULL) : gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_normaltest(a);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_772_c781(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
    gmb_val_t b = (func_code == 778 || func_code == 779) ? gmb_vm_arg_list(NULL) : gmb_vm_arg_list(argc >= 2 ? &args[1] : NULL);
    gmb_val_t r = gmb_ks(a, b);
    gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_774(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
            gmb_vm_set_dict_result_owned(gmb_ttestonesample(a, gmb_make_val_double(argc >= 2 ? args[1].num : 0)),
                &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
        }
}
static void gmb_bf_775(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int k = 0;
            int* sizes = NULL;
            double* g = argc >= 1 ? gmb_vm_groups_to_flat(&args[0], &k, &sizes) : NULL;
            gmb_val_t av = gmb_val_nil();
            if (g) av = gmb_anova_flat(g, sizes, k);
            gmb_vm_set_dict_result_owned(av, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
            free(g); free(sizes);
        }
}
static void gmb_bf_777(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_vm_set_dict_result_owned(gmb_fisher((gmb_int_t)(argc >= 1 ? args[0].num : 0),
                (gmb_int_t)(argc >= 2 ? args[1].num : 0), (gmb_int_t)(argc >= 3 ? args[2].num : 0),
                (gmb_int_t)(argc >= 4 ? args[3].num : 0)), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
        }
}
static void gmb_bf_780(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t a = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
            gmb_vm_set_dict_result_owned(gmb_ztest(a, gmb_make_val_double(argc >= 2 ? args[1].num : 0),
                gmb_make_val_double(argc >= 3 ? args[2].num : 1)), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
        }
}
static void gmb_bf_1003(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nr = 0, nc = 0;
            double* tf = argc >= 1 ? gmb_vm_jagged_to_flat(&args[0], &nr, &nc) : NULL;
            gmb_vm_set_dict_result_owned(gmb_chi2con_flat(tf, nr, nc), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
            free(tf);
        }
}
static void gmb_bf_1010(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t dv = gmb_vm_arg_list(argc >= 1 ? &args[0] : NULL);
            gmb_val_t r = func_code == 1010 ? gmb_skewtest(dv) : func_code == 1011 ? gmb_kurtosistest(dv)
                : func_code == 1012 ? gmb_jarquebera(dv) : gmb_describe(dv);
            gmb_vm_set_dict_result_owned(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
        }
}
static void gmb_bf_1006(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double a = argc >= 1 ? args[0].num : 0, s = argc >= 2 ? args[1].num : 1, x = argc >= 3 ? args[2].num : 1;
            bout->result = func_code == 1006 ? gmb_gamma_pdf(a, s, x) : func_code == 1007 ? gmb_gamma_cdf(a, s, x)
                : func_code == 1008 ? gmb_gamma_ppf(a, s, x) : gmb_gamma_isf(a, s, x);
        }
}
static void gmb_bf_1013(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = 0;
            double* d = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &n) : NULL;
            if (!d) { bout->result = 0; }
            bout->result = func_code == 1013 ? gmb_mode(d, n) : func_code == 1014 ? gmb_moment(d, n, (int)(argc >= 2 ? args[1].num : 2)) : gmb_iqr(d, n);
            free(d);
        }
}
static int64_t vm_matrix_handle_arg(const gmb_stack_entry_t* a)
{
    if (a && a->type == 2 && a->ref && a->ref->kind == GMB_OBJ_LIST) {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)a->ref;
        if (sv->ndims >= 2 && sv->list) {
            int rows = (int)sv->dim_upper[0] + 1;
            int cols = (int)sv->dim_upper[1] + 1;
            if (rows <= 0 || cols <= 0) return -1;
            size_t need = (size_t)rows * (size_t)cols;
            double* flat = (double*)malloc(sizeof(double) * need);
            if (!flat) return -1;
            size_t n = sv->list->count < need ? sv->list->count : need;
            for (size_t i = 0; i < need; i++) {
                if (i < n) {
                    gmb_val_t v = sv->list->items[i];
                    flat[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double
                            : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0.0;
                } else flat[i] = 0.0;
            }
            int64_t h = gmb_matrix_create_from(rows, cols, flat);
            free(flat);
            return h;
        }
    }
    return (int64_t)(a ? a->num : -1);
}

/* Pubblica un handle matrice come VALORE rank-2 (lista flat + ndims/dim_upper).
 * Usato dalle funzioni F2 che restituiscono matrici (TRANSPOSE/SLICE/RESHAPE/CLONE). */
static void vm_matrix_set_value_result(int64_t h, gmb_builtin_out_t* bout)
{
    int rows = gmb_matrix_rows(h), cols = gmb_matrix_cols(h);
    if (rows <= 0 || cols <= 0) { bout->is_num = 0; bout->result_ref = NULL; return; }
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (!out) { bout->is_num = 0; bout->result_ref = NULL; return; }
    out->is_array = 1; out->ndims = 2;
    out->dim_upper[0] = rows - 1; out->dim_upper[1] = cols - 1;
    gmb_list_resize(out->list, (size_t)rows * (size_t)cols, 0);
    for (int i = 0; i < rows * cols; i++)
        out->list->items[i] = gmb_make_val_double(gmb_matrix_get(h, i / cols, i % cols));
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
}
static void gmb_bf_823(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            bout->result = (double)(func_code == 823 ? gmb_matrix_rows(h) : gmb_matrix_cols(h));
        }
        
}
static void gmb_bf_1020(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = 0;
            double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &n) : NULL;
            int np = (int)(argc >= 2 ? args[1].num : 256);
            double fs = argc >= 3 ? args[2].num : 1.0;
            int rows = 0;
            double* w = x ? gmb_welch(x, n, np, fs, &rows) : NULL;
            free(x);
            if (!w || rows <= 0) { free(w); bout->is_num = 0; bout->result_ref = NULL; }
            int64_t mh = gmb_matrix_create(rows, 2);
            for (int r = 0; r < rows; r++) { gmb_matrix_set(mh, r, 0, w[r * 2]); gmb_matrix_set(mh, r, 1, w[r * 2 + 1]); }
            free(w);
            bout->result = (double)mh;
        }
}
static void gmb_bf_1024(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nx = 0, ny = 0;
            double* xs = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nx) : NULL;
            double* ys = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &ny) : NULL;
            int n = nx < ny ? nx : ny;
            gmb_int_t outn = 0;
            double* r = xs && ys ? gmb_splrep(xs, ys, n, &outn) : NULL;
            free(xs); free(ys);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_1025(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int sn = 0, nq = 0;
            double* sp = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &sn) : NULL;
            double* xq = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &nq) : NULL;
            gmb_int_t outn = 0;
            double* r = sp && xq ? gmb_splev(sp, sn, xq, nq, &outn) : NULL;
            free(sp); free(xq);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_1029(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = 0;
            double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &n) : NULL;
            gmb_int_t m = 0;
            double* r = x ? gmb_rfft(x, n, &m) : NULL;
            free(x);
            if (!r || m <= 0) { free(r); bout->is_num = 0; bout->result_ref = NULL; }
            int64_t mh = gmb_matrix_create(m, 2);
            for (int k = 0; k < m; k++) { gmb_matrix_set(mh, k, 0, r[k * 2]); gmb_matrix_set(mh, k, 1, r[k * 2 + 1]); }
            free(r);
            bout->result = (double)mh;
        }
}
static void gmb_bf_1030(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nr = 0, ni = 0;
            double* re = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nr) : NULL;
            double* im = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &ni) : NULL;
            int m = nr < ni ? nr : ni;
            gmb_int_t outn = 0;
            double* r = re && im ? gmb_irfft(re, im, m, &outn) : NULL;
            free(re); free(im);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
        
}
static void gmb_bf_91(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
            if (!sx || !sy || !sx->list || !sy->list) { bout->is_num = 0; bout->result_ref = NULL; }
            int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
            double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
            double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
            if (!xd || !yd) { free(xd); free(yd); bout->is_num = 0; bout->result_ref = NULL; }
            for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            double slope, intercept, r2;
            gmb_analysis_linear_regression(xd, yd, n, &slope, &intercept, &r2);
            free(xd); free(yd);
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                gmb_dict_set(out->dict, "Slope", gmb_make_val_double(slope));
                gmb_dict_set(out->dict, "Intercept", gmb_make_val_double(intercept));
                gmb_dict_set(out->dict, "R2", gmb_make_val_double(r2));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_92(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            if (!sv || !sv->list) { bout->is_num = 0; bout->result_ref = NULL; }
            int n = (int)sv->list->count;
            int k = argc >= 2 ? (int)args[1].num : 2;
            if (k < 1) k = 1;
            double* dd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
            if (!dd) { bout->is_num = 0; bout->result_ref = NULL; }
            for (int i = 0; i < n; i++) { gmb_val_t v = sv->list->items[i]; dd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            
            if (n >= 100000)
            {
                gmb_cpu_job_t* j = (gmb_cpu_job_t*)calloc(1, sizeof(gmb_cpu_job_t));
                if (j)
                {
                    j->kind = 5; j->a = dd; j->na = n; j->p1 = k;
                    if (vm_try_park_cpu(vm, task, j)) { bout->is_num = -1; }
                    free(j);
                }
            }
            double* cent = (double*)calloc(k > 0 ? k : 1, sizeof(double));
            if (!cent) { free(dd); bout->is_num = 0; bout->result_ref = NULL; }
            gmb_kmeans(dd, n, k, cent);
            free(dd);
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                gmb_obj_shared_val_t* cl = gmb_shared_list_new();
                if (cl) { for (int i = 0; i < k; i++) gmb_list_add(cl->list, gmb_make_val_double(cent[i])); }
                if (cl) gmb_dict_set(out->dict, "Centroids", gmb_make_val_obj((gmb_obj_t*)cl));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
            free(cent);
        }
}
/* Etichetta una lista PIATTA row-major come MATRICE rows x 2 (re,im).
 * Senza i metadati ndims/dim_upper l'indicizzazione f(i,j) cade nel fallback
 * isqrt(count) di gmb_multidim_target e legge celle sbagliate: era il caso di
 * NUMERICAL.Fft/FFTCOMPLEX su VM/C (picco di frequenza su un bin inesistente,
 * mentre IL/PCode/C99 restituivano la matrice n x 2). */
static void gmb_mark_rows2(gmb_obj_shared_val_t* sv, int rows)
{
    if (!sv) return;
    sv->ndims = 2;
    sv->dim_upper[0] = rows - 1;
    sv->dim_upper[1] = 1;
}
static void gmb_bf_105(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            if (!sv || !sv->list || sv->list->count == 0) { bout->is_num = 0; bout->result_ref = NULL; }
            int n = (int)sv->list->count;
            
            {
                int park_thr = (func_code == 106 && n >= 2 && n % 2 == 0) ? 8192 : 4096;
                if (n >= park_thr)
                {
                    double* flat = (double*)malloc((size_t)n * sizeof(double));
                    gmb_cpu_job_t* j = flat ? (gmb_cpu_job_t*)calloc(1, sizeof(gmb_cpu_job_t)) : NULL;
                    if (j)
                    {
                        for (int i = 0; i < n; i++) { gmb_val_t v = sv->list->items[i]; flat[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
                        j->kind = (func_code == 105) ? 2 : 3; j->a = flat; j->na = n;
                        if (vm_try_park_cpu(vm, task, j)) { bout->is_num = -1; }
                        free(j);
                    }
                    free(flat);
                }
            }
            if (func_code == 106 && n >= 2 && n % 2 == 0)
            {
                /* IFFT di una lista di COPPIE (re/im interleaved) = output flat di FFT
                 * (round-trip FFT→IFFT di AnalysisTests.bas). half = n/2 bin complessi.
                 * gmb_fft(inv) divide per half → scala MathNet simmetrica: ×sqrt(half).
                 * RETURN: il branch generico sotto NON deve sovrascrivere questo output. */
                int half = n / 2;
                double* re = (double*)malloc(half * sizeof(double));
                double* im = (double*)malloc(half * sizeof(double));
                if (!re || !im) { free(re); free(im); bout->is_num = 0; bout->result_ref = NULL; return; }
                for (int i = 0; i < half; i++)
                {
                    gmb_val_t rv = sv->list->items[2*i];
                    gmb_val_t iv = sv->list->items[2*i + 1];
                    re[i] = rv.kind == GMB_TYPE_DOUBLE ? rv.v_double : rv.kind == GMB_TYPE_INT ? (double)rv.v_int : 0;
                    im[i] = iv.kind == GMB_TYPE_DOUBLE ? iv.v_double : iv.kind == GMB_TYPE_INT ? (double)iv.v_int : 0;
                }
                gmb_fft(re, im, half, 1);
                if (half > 1) { double sh = sqrt((double)half); for (int i = 0; i < half; i++) { re[i] *= sh; im[i] *= sh; } }
                gmb_obj_shared_val_t* out = gmb_shared_list_new();
                if (out)
                {
                    for (int i = 0; i < half; i++) { gmb_list_add(out->list, gmb_make_val_double(re[i])); gmb_list_add(out->list, gmb_make_val_double(im[i])); }
                    gmb_mark_rows2(out, half);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                free(re); free(im);
                if (!out) { bout->is_num = 0; bout->result_ref = NULL; }
                return;
            }
            {
                /* NON zero-paddare a potenza di 2: MathNet (canone IL) e gli emitter C usano
                 * la lunghezza REALE n (gmb_fft è corretto per ogni n via Bluestein). Il
                 * zero-pad a sz=next_pow2(n) dava bin0 = somma/sqrt(sz) invece di /sqrt(n)
                 * (AnalysisTests: 1.0 vs 1.3 con n=9→16) e una matrice n×2 di dimensione errata. */
                int sz = n;
                double* re = (double*)calloc((size_t)(sz > 0 ? sz : 1), sizeof(double));
                double* im = (double*)calloc(sz, sizeof(double));
                if (!re || !im) { free(re); free(im); bout->is_num = 0; bout->result_ref = NULL; }
                for (int i = 0; i < n; i++) { gmb_val_t v = sv->list->items[i]; re[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
                gmb_fft(re, im, sz, func_code == 106);
                /* Scala MathNet simmetrica 1/sqrt(N) (canone IL/PCode). gmb_fft (scipy)
                 * forward non scala e inverse divide per sz: forward ×1/sqrt(sz);
                 * inverse (branch generico, input reale) ×sqrt(sz) per ottenere 1/sqrt. */
                if (sz > 1) {
                    double s = (func_code == 106) ? sqrt((double)sz) : 1.0 / sqrt((double)sz);
                    for (int i = 0; i < sz; i++) { re[i] *= s; im[i] *= s; }
                }
                gmb_obj_shared_val_t* out = gmb_shared_list_new();
                if (out)
                {
                    for (int i = 0; i < sz; i++) { gmb_list_add(out->list, gmb_make_val_double(re[i])); gmb_list_add(out->list, gmb_make_val_double(im[i])); }
                    gmb_mark_rows2(out, sz);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
                free(re); free(im);
            }
        }
}
static void gmb_bf_96_c96(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
    gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
    double tx = argc >= 3 ? args[2].num : 0;
    if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
    int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
    double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    bout->result = gmb_interp_linear(xd, yd, n, tx);
    free(xd); free(yd);
}
static void gmb_bf_96_c97(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
    gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
    double tx = argc >= 3 ? args[2].num : 0;
    if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
    int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
    double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    bout->result = gmb_interp_akima(xd, yd, n, tx);
    free(xd); free(yd);
}
static void gmb_bf_96_c98(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
    gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
    double tx = argc >= 3 ? args[2].num : 0;
    if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
    int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
    double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    bout->result = gmb_interp_polynomial(xd, yd, n, tx);
    free(xd); free(yd);
}
static void gmb_bf_96_c99(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
    gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
    double tx = argc >= 3 ? args[2].num : 0;
    if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
    int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
    double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    bout->result = gmb_interp_pchip(xd, yd, n, tx);
    free(xd); free(yd);
}
static void gmb_bf_96_c100(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
    gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
    double tx = argc >= 3 ? args[2].num : 0;
    if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
    int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
    double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    bout->result = gmb_interp_step(xd, yd, n, tx);
    free(xd); free(yd);
}
static void gmb_bf_96_c101(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_obj_shared_val_t* sx = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
    gmb_obj_shared_val_t* sy = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
    double tx = argc >= 3 ? args[2].num : 0;
    if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
    int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
    double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
    if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
    bout->result = gmb_interp_clamp(xd, yd, n, tx, gmb_interp_akima);
    free(xd); free(yd);
}

static void gmb_bf_102(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* type = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "linear";
            gmb_obj_shared_val_t* sx = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
            gmb_obj_shared_val_t* sy = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[2].ref : NULL;
            double tx = argc >= 4 ? args[3].num : 0;
            if (!sx || !sy || !sx->list || !sy->list || sx->list->count < 2) { bout->result = 0; }
            int n = (int)(sx->list->count < sy->list->count ? sx->list->count : sy->list->count);
            double* xd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
            double* yd = (double*)malloc((n > 0 ? n : 1) * sizeof(double));
            if (!xd || !yd) { free(xd); free(yd); bout->result = 0; }
            for (int i = 0; i < n; i++) { gmb_val_t v = sx->list->items[i]; xd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            for (int i = 0; i < n; i++) { gmb_val_t v = sy->list->items[i]; yd[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            if (strcasecmp(type, "spline") == 0) bout->result = gmb_interp_akima(xd, yd, n, tx);
            else if (strcasecmp(type, "poly") == 0 || strcasecmp(type, "polynomial") == 0) bout->result = gmb_interp_polynomial(xd, yd, n, tx);
            else if (strcasecmp(type, "pchip") == 0) bout->result = gmb_interp_pchip(xd, yd, n, tx);
            else if (strcasecmp(type, "step") == 0) bout->result = gmb_interp_step(xd, yd, n, tx);
            else if (strcasecmp(type, "clamp") == 0) bout->result = gmb_interp_clamp(xd, yd, n, tx, gmb_interp_akima);
            else bout->result = gmb_interp_linear(xd, yd, n, tx);
            free(xd); free(yd);
        }
}
static void gmb_bf_103(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_closure_t* cl = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[0].ref : NULL;
            if (!cl) { bout->result = 0; }
            double a = argc >= 2 ? args[1].num : 0, b = argc >= 3 ? args[2].num : 1;
            gmb_closure_ctx_t ctx = { vm, task, cl };
            bout->result = gmb_quad_gk(gmb_vm_closure_f, &ctx, a, b);
        }
}
static void gmb_bf_104(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_closure_t* cl = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[0].ref : NULL;
            if (!cl) { bout->result = 0; }
            double x0 = argc >= 2 ? args[1].num : 0;
            /* Parità IL/PCode/C99Native (IlRuntime.Differentiate / an_differentiate):
             * derivata centrale con h=1e-6. Prima h=1e-5 → ultime cifre diverse
             * (12.0000000007892 vs 12.0000000002118, ClipperCodeBlocks). */
            const double h = 1e-6;
            double fxp = gmb_vm_call_closure_sync(vm, task, cl, x0 + h);
            double fxm = gmb_vm_call_closure_sync(vm, task, cl, x0 - h);
            bout->result = (fxp - fxm) / (2 * h);
        }
}
static void gmb_bf_850(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_closure_t* cl = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[0].ref : NULL;
            if (!cl) { bout->result = 0; }
            double a = argc >= 2 ? args[1].num : 0, b = argc >= 3 ? args[2].num : 1;
            int n = (int)(argc >= 4 ? args[3].num : 256);
            gmb_closure_ctx_t ctx = { vm, task, cl };
            bout->result = gmb_simpson_ctx(gmb_vm_closure_f, &ctx, a, b, n);
        }
}
static void gmb_bf_851(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int ny = 0, nx = 0;
            double* y = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &ny) : NULL;
            double* x = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &nx) : NULL;
            bout->result = gmb_trapezoid(y, x, ny < nx ? ny : nx);
            free(y); free(x);
        }
}
static void gmb_bf_852(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int ny = 0, nx = 0;
            double* y = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &ny) : NULL;
            double* x = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &nx) : NULL;
            gmb_int_t outn = 0;
            double* r = gmb_cumtrapezoid(y, x, ny < nx ? ny : nx, &outn);
            free(y); free(x);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_853(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_closure_t* cl = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[0].ref : NULL;
            if (!cl) { bout->result = 0; }
            double y0 = argc >= 2 ? args[1].num : 0, t0 = argc >= 3 ? args[2].num : 0;
            double t1 = argc >= 4 ? args[3].num : 1;
            int steps = (int)(argc >= 5 ? args[4].num : 10);
            gmb_closure_ctx_t ctx = { vm, task, cl };
            gmb_int_t outn = 0;
            double* r = gmb_solve_ode_ctx(gmb_vm_closure_f2, &ctx, y0, t0, t1, steps, &outn);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_1019(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)func_code;
        {
            gmb_obj_closure_t* cl = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[0].ref : NULL;
            int n = 0;
            double* y0 = (argc >= 2) ? gmb_vm_list_to_dbl(&args[1], &n) : NULL;
            double t0 = argc >= 3 ? args[2].num : 0, t1 = argc >= 4 ? args[3].num : 1;
            if (!cl || !y0 || n <= 0) { free(y0); bout->result = 0; return; }
            gmb_closure_ctx_t ctx = { vm, task, cl };
            int rows = 0;
            double* traj = gmb_solve_ivp_rk45(gmb_vm_ode_fn, &ctx, y0, n, t0, t1, 1e-6, 1e-9, &rows);
            free(y0);
            if (!traj || rows <= 0) { free(traj); bout->result = 0; return; }
            int cols = n + 1;
            int64_t h = gmb_matrix_create(rows, cols);
            for (int i = 0; i < rows; i++)
                for (int j = 0; j < cols; j++)
                    gmb_matrix_set(h, i, j, traj[i * cols + j]);
            free(traj);
            bout->result = (double)h;
        }
}
static void gmb_bf_854(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nx = 0, ny = 0;
            double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nx) : NULL;
            double* y = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &ny) : NULL;
            double tx = argc >= 3 ? args[2].num : 0;
            bout->result = gmb_cubic_spline(x, y, nx < ny ? nx : ny, tx);
            free(x); free(y);
        }
}
static void gmb_bf_855(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = (int)(argc >= 1 ? args[0].num : 0);
            double d = argc >= 2 ? args[1].num : 1.0;
            gmb_int_t outn = 0;
            double* r = gmb_fftfreq(n, d, &outn);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_856(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = 0;
            double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &n) : NULL;
            gmb_int_t outn = 0;
            double* r = (func_code == 856) ? gmb_fftshift(x, n, &outn) : gmb_ifftshift(x, n, &outn);
            free(x);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_133(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_bernoulli_pmf(args[1].num, (int)args[0].num);
        }
}
static void gmb_bf_134(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_binomial_pmf(args[2].num, (int)args[1].num, (int)args[0].num);
        }
}
static void gmb_bf_135(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_poisson_pmf(args[1].num, (int)args[0].num);
        }
}
static void gmb_bf_136(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_geometric_pmf(args[1].num, (int)args[0].num);
        }
}
static void gmb_bf_137(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_negbinom_pmf(args[1].num, args[2].num, (int)args[0].num);
        }
}
static void gmb_bf_138(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist_hypergeom_pmf((int)args[1].num, (int)args[2].num, (int)args[3].num, (int)args[0].num);
        }
}
static void gmb_bf_50(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_polarx(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_51(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_polary(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_52(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_angle2d(args[0].num, args[1].num, argc >= 3 ? args[2].num : 0, argc >= 4 ? args[3].num : 0);
        }
}
static void gmb_bf_53(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_pythagoras(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_20(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            bout->result = (args[0].type == 2 && args[0].ref && ((gmb_obj_string_t*)args[0].ref)->length > 0)
                ? (double)(unsigned char)((gmb_obj_string_t*)args[0].ref)->data[0] : 0;
}
static void gmb_bf_21(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char buf[4]; buf[0] = (char)(int)args[0].num; buf[1] = '\0';
            gmb_obj_string_t* s = gmb_str_cstr(buf);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_23(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 2 && args[0].ref)
            {
                /* BYTES boxed (gmb_make_val_bytes, es. dict("privateKey") di BLOCKCHAIN.DERIVEKEY):
                 * layout [len][bytes...] con `kind == GMB_OBJ_STRING` e type_name "gmb_bytes".
                 * LEN = numero di BYTE del buffer (parità IL/PCode/C99Native): prima cadeva nel
                 * ramo stringa e leggeva il campo `size_t len` come testo → 0. */
                if (args[0].ref->type_name && strcmp(args[0].ref->type_name, "gmb_bytes") == 0)
                {
                    gmb_val_t bv; bv.kind = GMB_TYPE_OBJECT; bv.v_ptr = args[0].ref;
                    size_t blen = 0;
                    uint8_t* bd = gmb_val_as_bytes(bv, &blen);
                    if (bd) free(bd);
                    bout->result = (double)blen;
                }
                /* LEN di una STRINGA: CARATTERI (code point Unicode), non byte né unità UTF-16.
                 * Per i BYTE c'è LENB (gmb_bf_3816). Vedi Bugs/(solved) - LEN-UTF8-Byte-VS-Char-Gambe-C. */
                else if (args[0].ref->kind == GMB_OBJ_STRING) bout->result = (double)gmb_chars_len(((gmb_obj_string_t*)args[0].ref)->data);
                else if (args[0].ref->kind == GMB_OBJ_LIST && ((gmb_obj_shared_val_t*)args[0].ref)->list) bout->result = (double)((gmb_obj_shared_val_t*)args[0].ref)->list->count;
                else if (args[0].ref->kind == GMB_OBJ_DICT && ((gmb_obj_shared_val_t*)args[0].ref)->dict) bout->result = (double)((gmb_obj_shared_val_t*)args[0].ref)->dict->count;
                else bout->result = 0;
            }
            else bout->result = 0;
        }
}
/* LENB(s) — codice 3816 (BuiltInFunction.LENB): BYTE della codifica UTF-8 della stringa.
 * LEN conta i CARATTERI (code point), LENB i byte: servono entrambi (limiti di buffer/file
 * vs conteggio visibile). PARITÀ DEI PARAMETRI: per un argomento NON stringa il risultato è
 * -1 su tutte e 4 le gambe (prima: errore C in compilazione su C99Native, IL invalido su IL,
 * 0 su VM/C — tre esiti diversi per lo stesso programma). */
static void gmb_bf_3816(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
    {
        gmb_string_t s;
        gmb_obj_string_t* o = (gmb_obj_string_t*)args[0].ref;
        s.data = o->data; s.length = o->length; s.ref_count = 1;
        bout->result = (double)gmb_str_bytes(s);
    }
    else bout->result = -1;
}
/* VECTOR.MANHATTAN/CHEBYSHEV/COSINE/MINKOWSKI (913-916): metriche handle-based dal
 * runtime condiviso (gmb_matrix.c). Prima cadevano in gmb_bf_900 (matrice) → lista vuota. */
static void gmb_bf_vector_dist(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    int64_t a = (argc >= 1 && args[0].type == 1) ? (int64_t)args[0].num : -1;
    int64_t b = (argc >= 2 && args[1].type == 1) ? (int64_t)args[1].num : -1;
    double r = 0;
    switch (func_code) {
        case 913: r = gmb_vector_manhattan(a, b); break;
        case 914: r = gmb_vector_chebyshev(a, b); break;
        case 915: r = gmb_vector_cosine(a, b); break;
        case 916: r = gmb_vector_minkowski(a, b, (argc >= 3 && args[2].type == 1) ? args[2].num : 2.0); break;
    }
    bout->result = r;
}
static void gmb_bf_928(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = gmb_matrix_expm(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
            bout->result = (double)h;
        }
        
}
static void gmb_bf_860(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nx = 0;
            double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nx) : NULL;
            double wn = argc >= 2 ? args[1].num : 0.3;
            gmb_int_t n = (gmb_int_t)(argc >= 3 ? args[2].num : 2);
            gmb_int_t outn = 0;
            double* r = (func_code == 860)
                ? gmb_filter_lowpass(x, nx, wn, n, &outn)
                : gmb_filter_highpass(x, nx, wn, n, &outn);
            free(x);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_862(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nx = 0;
            double* x = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nx) : NULL;
            double w1 = argc >= 2 ? args[1].num : 0.2;
            double w2 = argc >= 3 ? args[2].num : 0.5;
            gmb_int_t n = (gmb_int_t)(argc >= 4 ? args[3].num : 2);
            gmb_int_t outn = 0;
            double* r = (func_code == 862)
                ? gmb_filter_bandpass(x, nx, w1, w2, n, &outn)
                : gmb_filter_bandstop(x, nx, w1, w2, n, &outn);
            free(x);
            gmb_vm_set_result_list(r, (int)outn, &bout->is_num, &bout->result_ref);
        }
}
static void gmb_bf_938(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            bout->result = func_code == 938 ? (double)gmb_matrix_rank(h) : func_code == 939 ? gmb_matrix_trace(h) : gmb_matrix_norm(h);
        }
}
static void gmb_bf_894(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            int64_t out = func_code == 894 ? gmb_matrix_sqrtm(h) : func_code == 895 ? gmb_matrix_logm(h) : gmb_matrix_hessenberg(h);
            bout->result = (double)out;
        }
}
static void gmb_bf_897(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task; (void)func_code;
            int64_t hs[2];
            hs[0] = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            hs[1] = vm_matrix_handle_arg(argc >= 2 ? &args[1] : NULL);
            int64_t __v = gmb_matrix_block_diag(hs, 2);
            vm_matrix_set_value_result(__v, bout);
            gmb_matrix_free(__v);
        }
}
static void gmb_bf_898(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            bout->result = (double)gmb_matrix_null_space(h);
        }
}
static void gmb_bf_899(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_int_t r = (gmb_int_t)(argc >= 2 ? args[1].num : 0);
            gmb_int_t c = (gmb_int_t)(argc >= 3 ? args[2].num : 0);
            double v = argc >= 4 ? args[3].num : 0;
            /* Valore MATRIX OF (lista FLAT rank-2): mutazione diretta e valore restituito
             * (F1: `m = MATRIX.SETVALUE(m, r, c, v)`). */
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2 && sv->list) {
                    int rows = (int)sv->dim_upper[0] + 1;
                    int cols = (int)sv->dim_upper[1] + 1;
                    if (r >= 0 && r < rows && c >= 0 && c < cols) {
                        size_t idx = (size_t)r * (size_t)cols + (size_t)c;
                        if (idx < sv->list->count) sv->list->items[idx] = gmb_make_val_double(v);
                    }
                    gmb_obj_retain(args[0].ref);
                    bout->is_num = 0; bout->result_ref = args[0].ref; bout->result_ref_bt = GMB_BT_OBJECT;
                    return;
                }
            }
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            gmb_matrix_set(h, r, c, v);
            /* handle → valore (il contratto del linguaggio è MATRIX OF). */
            int rows = gmb_matrix_rows(h), cols = gmb_matrix_cols(h);
            if (rows <= 0 || cols <= 0) { bout->is_num = 0; bout->result_ref = NULL; return; }
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (!out) { bout->is_num = 0; bout->result_ref = NULL; return; }
            out->is_array = 1; out->ndims = 2;
            out->dim_upper[0] = rows - 1; out->dim_upper[1] = cols - 1;
            gmb_list_resize(out->list, (size_t)rows * (size_t)cols, 0);
            for (int i = 0; i < rows * cols; i++)
                out->list->items[i] = gmb_make_val_double(gmb_matrix_get(h, i / cols, i % cols));
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
static void gmb_bf_267(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_int_t r = (gmb_int_t)(argc >= 2 ? args[1].num : 0);
            gmb_int_t c = (gmb_int_t)(argc >= 3 ? args[2].num : 0);
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2 && sv->list) {
                    int rows = (int)sv->dim_upper[0] + 1;
                    int cols = (int)sv->dim_upper[1] + 1;
                    if (r >= 0 && r < rows && c >= 0 && c < cols) {
                        size_t idx = (size_t)r * (size_t)cols + (size_t)c;
                        if (idx < sv->list->count) {
                            gmb_val_t it = sv->list->items[idx];
                            bout->result = it.kind == GMB_TYPE_DOUBLE ? it.v_double : it.kind == GMB_TYPE_INT ? (double)it.v_int : 0.0;
                            return;
                        }
                    }
                    bout->result = 0;
                    return;
                }
            }
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            bout->result = gmb_matrix_get(h, r, c);
        }
}
static void gmb_bf_941(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            int64_t out = func_code == 941 ? gmb_matrix_cosm(h)
                : func_code == 942 ? gmb_matrix_sinm(h)
                : func_code == 943 ? gmb_matrix_tanm(h)
                : func_code == 944 ? gmb_matrix_signm(h)
                : gmb_matrix_orth(h);
            bout->result = (double)out;
        }
}
static void gmb_bf_945(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            double p = argc >= 2 ? args[1].num : 0.5;
            bout->result = (double)gmb_matrix_fractional_power(h, p);
        }
}
static void gmb_bf_947(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h1 = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            int64_t h2 = vm_matrix_handle_arg(argc >= 2 ? &args[1] : NULL);
            double* angles = NULL;
            int na = gmb_matrix_subspace_angles(h1, h2, &angles);
            if (na < 0) { bout->is_num = 0; bout->result_ref = NULL; return; }
            gmb_vm_set_result_list(angles, na, &bout->is_num, &bout->result_ref); 
            bout->result = 0;
        }
}
static void gmb_bf_948(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            int64_t out = func_code == 948 ? gmb_matrix_polar(h)
                : func_code == 955 ? gmb_matrix_eig(h)
                : gmb_matrix_schur(h);
            bout->result = (double)out;
        }
}
static void gmb_bf_949(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            bout->result = func_code == 949 ? gmb_matrix_cond(h) : gmb_matrix_nuclear_norm(h);
        }
}
static void gmb_bf_951(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nc = 0, nr = 0;
            double* c = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nc) : NULL;
            double* r = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &nr) : NULL;
            int64_t out = func_code == 951
                ? gmb_matrix_toeplitz(c, r, nc, nr)
                : gmb_matrix_hankel(c, r, nc, nr);
            free(c);
            free(r);
            vm_matrix_set_value_result(out, bout);
            gmb_matrix_free(out);
        }
}
static void gmb_bf_953(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int nc = 0;
            double* c = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &nc) : NULL;
            int64_t __v = gmb_matrix_circulant(c, nc);
            free(c);
            vm_matrix_set_value_result(__v, bout);
            gmb_matrix_free(__v);
        }
}
static void gmb_bf_954(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t hA = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
            int nb = 0;
            double* b = argc >= 2 ? gmb_vm_list_to_dbl(&args[1], &nb) : NULL;
            int lower = argc >= 3 ? (args[2].num != 0) : 0;
            int64_t sol = gmb_matrix_solve_triangular(hA, b, nb, lower);
            free(b);
            if (sol <= 0) { bout->is_num = 0; bout->result_ref = NULL; return; }
            double* out = (double*)malloc(sizeof(double) * (size_t)nb);
            if (!out) { bout->is_num = 0; bout->result_ref = NULL; return; }
            for (int i = 0; i < nb; i++) out[i] = gmb_matrix_get(sol, i, 0);
            gmb_vm_set_result_list(out, nb, &bout->is_num, &bout->result_ref); 
            bout->result = 0;
        }
}
static void gmb_bf_929(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task; (void)func_code;
            int64_t h = gmb_matrix_kron(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), vm_matrix_handle_arg(argc >= 2 ? &args[1] : NULL));
            vm_matrix_set_value_result(h, bout);
            gmb_matrix_free(h);
        }

}
static void gmb_bf_1100(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char buf[32];
            if (gmb_ml_device_name(gmb_ml_active_device(), buf, (int)sizeof(buf)))
            {
                gmb_obj_string_t* s = gmb_str_cstr(buf);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
        }
}
static void gmb_bf_1101(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char buf[1024];
            if (gmb_ml_lib_path(buf, (int)sizeof(buf)))
            {
                gmb_obj_string_t* s = gmb_str_cstr(buf);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
        }
}
static void gmb_bf_1102(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            bout->result = (double)gmb_ml_loaded();
}
static void gmb_bf_1103(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            bout->result = (double)gmb_ml_tensor_create((int)args[0].num, (int)(argc > 1 ? args[1].num : 0), gmb_ml_active_device());
}
static void gmb_bf_1104(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = 0;
            double* data = argc >= 1 ? gmb_vm_list_to_dbl(&args[0], &n) : NULL;
            bout->result = (double)gmb_ml_tensor_from(1, n, data ? data : NULL, gmb_ml_active_device());
            free(data);
        }
}
static void gmb_bf_1105(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    GML_VM_REQUIRE();
    /* TORCH_ADD */ bout->result = (double)gmb_ml_tensor_add((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1106(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    GML_VM_REQUIRE();
    /* TORCH_SUB */ bout->result = (double)gmb_ml_tensor_sub((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1107(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    GML_VM_REQUIRE();
    /* TORCH_MUL */ bout->result = (double)gmb_ml_tensor_mul((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1108(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    GML_VM_REQUIRE();
    /* TORCH_DIV */ bout->result = (double)gmb_ml_tensor_div((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1109(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    GML_VM_REQUIRE();
    /* TORCH_MATMUL */ bout->result = (double)gmb_ml_tensor_matmul((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1110(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    GML_VM_REQUIRE();
    /* TORCH_RELU */ bout->result = (double)gmb_ml_tensor_relu((int64_t)args[0].num);
}
static void gmb_bf_1111(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            bout->result = gmb_ml_tensor_get((int64_t)args[0].num, (int)args[1].num, (int)args[2].num);
}
static void gmb_bf_1112(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ml_tensor_set((int64_t)args[0].num, (int)args[1].num, (int)args[2].num, args[3].num);
            bout->result = args[3].num;
}
static void gmb_bf_1113(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = (int64_t)(argc >= 1 ? args[0].num : -1);
            int r = gmb_ml_tensor_rows(h), c = gmb_ml_tensor_cols(h);
            if (r > 0 && c > 0)
            {
                int n = r * c;
                double* out = (double*)malloc(sizeof(double) * (size_t)n);
                if (out)
                {
                    int got = gmb_ml_tensor_toarray(h, out, n);
                    if (got >= 0) gmb_vm_set_result_list(out, got, &bout->is_num, &bout->result_ref);
                    else free(out);
                }
            }
        }
}
static void gmb_bf_1114(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = (int64_t)(argc >= 1 ? args[0].num : -1);
            int r = gmb_ml_tensor_rows(h), c = gmb_ml_tensor_cols(h);
            if (r >= 0 && c >= 0)
            {
                double* out = (double*)malloc(sizeof(double) * 2);
                if (out) { out[0] = r; out[1] = c; gmb_vm_set_result_list(out, 2, &bout->is_num, &bout->result_ref); }
            }
        }
}
static void gmb_bf_1115(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ml_tensor_free((int64_t)(argc >= 1 ? args[0].num : -1));
            bout->result = 0;
}
static void gmb_bf_1116(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            /* TORCH.ALLCLOSE → bool BASIC: true = -1 (parità IL/StdLib.Torch boolType e
             * C99Native tg_allclose "!= 0 ? -1 : 0"). Prima ritornava 1. */
            bout->result = gmb_ml_tensor_allclose((int64_t)args[0].num, (int64_t)args[1].num, args[2].num) ? -1.0 : 0.0;
}
static void gmb_bf_1117(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_SUM */ bout->result = gmb_ml_tensor_sum((int64_t)args[0].num);
}
static void gmb_bf_1118(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MEAN */ bout->result = gmb_ml_tensor_mean((int64_t)args[0].num);
}
static void gmb_bf_1119(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_ZEROS */ bout->result = (double)gmb_ml_tensor_zeros((int)args[0].num, (int)args[1].num, gmb_ml_active_device());
}
static void gmb_bf_1120(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_ONES */ bout->result = (double)gmb_ml_tensor_ones((int)args[0].num, (int)args[1].num, gmb_ml_active_device());
}
static void gmb_bf_1121(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_EYE */ bout->result = (double)gmb_ml_tensor_eye((int)args[0].num, gmb_ml_active_device());
}
static void gmb_bf_1122(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_RANDN */ bout->result = (double)gmb_ml_tensor_randn((int)args[0].num, (int)args[1].num, gmb_ml_active_device());
}
static void gmb_bf_1123(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_TRANSPOSE */ bout->result = (double)gmb_ml_tensor_transpose((int64_t)args[0].num);
}
static void gmb_bf_1124(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_RESHAPE */ bout->result = (double)gmb_ml_tensor_reshape((int64_t)args[0].num, (int)args[1].num, (int)args[2].num);
}
static void gmb_bf_1125(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_SIGMOID */ bout->result = (double)gmb_ml_tensor_sigmoid((int64_t)args[0].num);
}
static void gmb_bf_1126(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_TANH */ bout->result = (double)gmb_ml_tensor_tanh((int64_t)args[0].num);
}
static void gmb_bf_1127(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_SOFTMAX */ bout->result = (double)gmb_ml_tensor_softmax((int64_t)args[0].num);
}
static void gmb_bf_1128(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_EXP */ bout->result = (double)gmb_ml_tensor_exp((int64_t)args[0].num);
}
static void gmb_bf_1129(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_LOG */ bout->result = (double)gmb_ml_tensor_log((int64_t)args[0].num);
}
static void gmb_bf_1130(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_SQRT */ bout->result = (double)gmb_ml_tensor_sqrt((int64_t)args[0].num);
}
static void gmb_bf_1131(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_POW */ bout->result = (double)gmb_ml_tensor_pow((int64_t)args[0].num, args[1].num);
}
static void gmb_bf_1132(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_ABS */ bout->result = (double)gmb_ml_tensor_abs((int64_t)args[0].num);
}
static void gmb_bf_1133(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MAX */ bout->result = gmb_ml_tensor_max((int64_t)args[0].num);
}
static void gmb_bf_1134(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MIN */ bout->result = gmb_ml_tensor_min((int64_t)args[0].num);
}
static void gmb_bf_1135(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* arch = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "linear";
            const char* cfg = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "{}";
            bout->result = (double)gmb_ml_model_create(arch, cfg);
        }
}
static void gmb_bf_1136(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            
            const char* opt = (argc >= 4 && args[3].type == 2 && args[3].ref) ? ((gmb_obj_string_t*)args[3].ref)->data : "adam";
            const char* loss = (argc >= 5 && args[4].type == 2 && args[4].ref) ? ((gmb_obj_string_t*)args[4].ref)->data : "mse";
            /* L2: 7o arg opts$ opzionale (JSON scheduler/warmup/accum/clip/resume). */
            if (argc >= 7 && args[6].type == 2 && args[6].ref)
                bout->result = (double)gmb_ml_model_train_step_ex((int64_t)args[0].num, (int64_t)args[1].num, (int64_t)args[2].num, opt, loss, args[5].num, ((gmb_obj_string_t*)args[6].ref)->data);
            else
                bout->result = (double)gmb_ml_model_train_step((int64_t)args[0].num, (int64_t)args[1].num, (int64_t)args[2].num, opt, loss, args[5].num);
        }
}
/* L6 pipeline dati testuale toy (char-level): TEXTENCODE 1169, TEXTDECODE 1170, DATASETLOAD 1171. */
/* L9 export (1172): h, path$, format$ opzionale. */
static void gmb_bf_1172(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          const char* pth = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
          const char* fmt = (argc >= 3 && args[2].type == 2 && args[2].ref) ? ((gmb_obj_string_t*)args[2].ref)->data : "flat";
          bout->result = (double)gmb_ml_model_export((int64_t)args[0].num, pth, fmt);
        }
}
/* L11 imatrix (1173/1174): lettore sensibilita' + mappa rank. */
static void gmb_bf_1173(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          const char* pth = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
          bout->result = (double)gmb_ml_imatrix_load_t(pth);
        }
}
static void gmb_bf_1174(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          const char* pth = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
          int lo = (argc >= 2) ? (int)args[1].num : 1, hi = (argc >= 3) ? (int)args[2].num : lo;
          bout->result = (double)gmb_ml_imatrix_ranks_t(pth, lo, hi);
        }
}
static void gmb_bf_1169(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          const char* t = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
          bout->result = (double)gmb_ml_text_encode(t);
        }
}
static void gmb_bf_1170(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          gmb_string_t r = gmb_ml_text_decode_s((int64_t)args[0].num);
          gmb_obj_string_t* s = gmb_str_cstr(r.data ? r.data : "");
          bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
}
static void gmb_bf_1171(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
          const char* opts = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "{}";
          bout->result = (double)gmb_ml_dataset_load_text(path, opts);
        }
}
/* L5 DPO/ORPO (TORCH.MODELDPOSTEP, codice 1168): ms, mr (0 = ORPO), x, c, r handle;
 * opts$ JSON opzionale. Ritorna loss double. */
static void gmb_bf_1168(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* opts = (argc >= 6 && args[5].type == 2 && args[5].ref) ? ((gmb_obj_string_t*)args[5].ref)->data : "{}";
            bout->result = gmb_ml_model_dpo_step((int64_t)args[0].num, (int64_t)args[1].num, (int64_t)args[2].num, (int64_t)args[3].num, (int64_t)args[4].num, opts);
        }
}
static void gmb_bf_1137(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MODELLOSS */ bout->result = gmb_ml_model_loss((int64_t)args[0].num, (int64_t)args[1].num, (int64_t)args[2].num);
}
static void gmb_bf_1138(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MODELFORWARD */ bout->result = (double)gmb_ml_model_forward((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1139(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MODELPARAMETERS */ bout->result = (double)gmb_ml_model_parameters((int64_t)args[0].num);
}
static void gmb_bf_1140(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char buf[512];
            if (gmb_ml_model_state((int64_t)args[0].num, buf, (int)sizeof(buf)) == 0)
            {
                gmb_obj_string_t* s = gmb_str_cstr(buf);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
        }
}
static void gmb_bf_1141(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MODELFREE */ gmb_ml_model_free((int64_t)args[0].num); bout->result = 0;
}
static void gmb_bf_1142(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_GELU */ bout->result = (double)gmb_ml_tensor_gelu((int64_t)args[0].num);
}
static void gmb_bf_1143(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_PERMUTE */ bout->result = (double)gmb_ml_tensor_permute((int64_t)args[0].num, (int)args[1].num, (int)args[2].num);
}
static void gmb_bf_1144(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_SLICE */ bout->result = (double)gmb_ml_tensor_slice((int64_t)args[0].num, (int)args[1].num, (int)args[2].num, (int)args[3].num);
}
static void gmb_bf_1145(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_CONCAT */ bout->result = (double)gmb_ml_tensor_concat((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1146(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_STACK */ bout->result = (double)gmb_ml_tensor_stack((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1147(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_SPLIT */ bout->result = (double)gmb_ml_tensor_split((int64_t)args[0].num, (int)args[1].num, (int)args[2].num);
}
static void gmb_bf_1148(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        int n = 0; double* out = gmb_ml_tensor_topk_alloc((int64_t)args[0].num, (int)args[1].num, &n); if (out) gmb_vm_set_result_list(out, n, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1149(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        int n = 0; double* out = gmb_ml_tensor_argsort_alloc((int64_t)args[0].num, &n); if (out) gmb_vm_set_result_list(out, n, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1150(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        int n = 0; double* out = gmb_ml_tensor_onehot_alloc((int)args[0].num, (int)args[1].num, &n); if (out) gmb_vm_set_result_list(out, n, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1151(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        int n = 0; double* out = gmb_ml_tensor_clamp_alloc((int64_t)args[0].num, args[1].num, args[2].num, &n); if (out) gmb_vm_set_result_list(out, n, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1152(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        int n = 0; double* out = gmb_ml_tensor_normalize_alloc((int64_t)args[0].num, &n); if (out) gmb_vm_set_result_list(out, n, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1153(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        int n = 0; double* out = gmb_ml_tensor_standardize_alloc((int64_t)args[0].num, &n); if (out) gmb_vm_set_result_list(out, n, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_1190(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          if (gmb_ml_load_lib() != 0) { vm_error(task, "TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'"); }
          gmb_string_t r = gmb_ml_tensor_dtype_s((int64_t)args[0].num);
          gmb_obj_string_t* s = gmb_str_cstr(r.data ? r.data : "");
          bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
}
static void gmb_bf_1191(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)args; (void)argc; (void)func_code;
          gmb_string_t r = gmb_ml_available_s();
          gmb_obj_string_t* s = gmb_str_cstr(r.data ? r.data : "");
          bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
}
static void gmb_bf_1192(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        { (void)vm; (void)func_code;
          if (gmb_ml_load_lib() != 0) { vm_error(task, "TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'"); }
          gmb_string_t r = gmb_ml_tensor_device_s((int64_t)args[0].num);
          gmb_obj_string_t* s = gmb_str_cstr(r.data ? r.data : "");
          bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
}
static void gmb_bf_1193(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (gmb_ml_load_lib() != 0) { vm_error(task, "TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'"); }
            const char* path = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            bout->result = (double)gmb_ml_model_save((int64_t)args[0].num, path);
        }
}
static void gmb_bf_1194(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (gmb_ml_load_lib() != 0) { vm_error(task, "TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'"); }
            bout->result = (double)gmb_ml_model_reset((int64_t)args[0].num);
        }
}
/* TORCH.MODELLOAD(h, path): ricarica i pesi (formato flat di MODELSAVE). */
static void gmb_bf_3898(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (gmb_ml_load_lib() != 0) { vm_error(task, "TORCH non caricato: esegui 'gmb ml setup' o 'ML SETUP'"); }
            const char* path = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            bout->result = (double)gmb_ml_model_load((int64_t)args[0].num, path);
        }
}
static void gmb_bf_1154(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* metric = (argc >= 3 && args[2].type == 2 && args[2].ref) ? ((gmb_obj_string_t*)args[2].ref)->data : "euclidean";
            bout->result = gmb_ml_similarity((int64_t)args[0].num, (int64_t)args[1].num, metric);
        }
}
static void gmb_bf_1195(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_LOSSMSE */ bout->result = gmb_ml_loss((int64_t)args[0].num, (int64_t)args[1].num, "mse");
}
static void gmb_bf_1196(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_LOSSCROSSENTROPY */ bout->result = gmb_ml_loss((int64_t)args[0].num, (int64_t)args[1].num, "crossentropy");
}
static void gmb_bf_1197(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_LOSSBINARY */ bout->result = gmb_ml_loss((int64_t)args[0].num, (int64_t)args[1].num, "binary");
}
static void gmb_bf_1198(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_ACCURACY */ bout->result = gmb_ml_accuracy((int64_t)args[0].num, (int64_t)args[1].num);
}
static void gmb_bf_1199(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MODELINPUT */ bout->result = (double)gmb_ml_model_input((int64_t)args[0].num);
}
static void gmb_bf_1200(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_MODELOUTPUT */ bout->result = (double)gmb_ml_model_output((int64_t)args[0].num);
}
static void gmb_bf_1990(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    GML_VM_REQUIRE();
        /* TORCH_ARGMAX */ bout->result = (double)gmb_ml_tensor_argmax((int64_t)args[0].num);
}
static void gmb_bf_917(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t gs = gmb_val_str(args[0].type == 1 ? args[0].num : 0.0);
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else bout->is_num = 0;
        }
}
static double gmb_bound_of_shared(gmb_task_t* task, gmb_obj_shared_val_t* sv, int dim, int func_code)
{
    if (!sv) return func_code == 33 ? 0.0 : -1.0;
    /* Array creato da CREATE_ARRAY (DIM A(...)): bound per la dimensione richiesta
     * (1-based). Prima si ritornava SEMPRE count-1 (lista piatta), quindi UBOUND di una
     * matrice 2D dava il totale degli elementi -1 (es. DIM A(1,1) → 3,3).
     * Dimensione fuori range: errore runtime come IL/PCode (GetUpperBound(-1/ndims)
     * lancia IndexOutOfRange). Prima si ricadeva sul count della lista, quindi
     * UBOUND(v, 2) su un array 1-D restituiva n-1 e su un 3-D UBOUND(g, 4) il totale-1. */
    if (dim < 1 || dim > (sv->ndims > 0 ? sv->ndims : 1)) {
        vm_error(task, "LBOUND/UBOUND: dimensione fuori range");
        return func_code == 33 ? 0.0 : -1.0;
    }
    if (sv->ndims > 0)
        return func_code == 33 ? (double)sv->dim_lower[dim - 1] : (double)sv->dim_upper[dim - 1];
    double n = (double)gmb_list_count(*sv->list);
    return func_code == 33 ? 0.0 : (n > 0 ? n - 1 : -1.0);
}
static void gmb_bf_32(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                int dim = (argc >= 2 && args[1].type == 1) ? (int)args[1].num : 1;
                bout->result = gmb_bound_of_shared(task, sv, dim, func_code);
            }
            else if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
                int dim = (args[0].type == 1) ? (int)args[0].num : 1;
                bout->result = gmb_bound_of_shared(task, sv, dim, func_code);
            }
            else if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                /* LBOUND/UBOUND su STRINGA: 0 / length-1 (parità PCode/IL: UBOUND("abc")==2).
                 * Prima cadeva nel fallback → UBOUND=-1 → il FOR EACH su stringa emesso dal
                 * compilatore C# (UBOUND + OP_LOAD_ELEMENT_POSITIONAL, visitForEach) iterava
                 * 0 volte SENZA errore (sample ForEachJsonEdgeCases.bas: `string FAIL`). */
                gmb_obj_string_t* sobj = (gmb_obj_string_t*)args[0].ref;
                bout->result = func_code == 33 ? 0.0 : (double)((gmb_int_t)sobj->length - 1);
            }
            else if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                /* LBOUND/UBOUND su una COLLEZIONE a handle (SET/LIST-like e map-like:
                 * SORTED/TREE/AVL/REDBLACK/TRIPLE/BAG/HEAP/...): 0 / count-1 (parità
                 * IL/PCode). Senza questo ramo UBOUND valeva -1 → il FOR EACH emesso dal
                 * compilatore (UBOUND + LOAD_ELEMENT_POSITIONAL) iterava ZERO volte in
                 * silenzio (sample CollectionsDictLists/CollectionsLinked su VM/C). */
                int64_t cn = gmb_coll_count(((gmb_obj_collection_t*)args[0].ref)->handle);
                bout->result = func_code == 33 ? 0.0 : (cn > 0 ? (double)(cn - 1) : -1.0);
            }
            else if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                /* LBOUND/UBOUND su DICT: 0 / count-1 (parità IL/PCode). Il FOR EACH/comprehension
                 * usa il contatore posizionale (0..count-1); prima UBOUND(dict) cadeva nel
                 * fallback -1 → 0 iterazioni (`ks=0` su FOR EACH k,v IN dict, anche dict reali). */
                gmb_obj_shared_val_t* dsv = (gmb_obj_shared_val_t*)args[0].ref;
                double dn = dsv->dict ? (double)gmb_dict_count(*dsv->dict) : 0.0;
                bout->result = func_code == 33 ? 0.0 : (dn > 0 ? dn - 1.0 : -1.0);
            }
            else bout->result = func_code == 33 ? 0.0 : -1.0;
        }
}
static void gmb_bf_299_c299(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_left(s, (gmb_int_t)(argc > 1 ? args[1].num : 0));
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c300(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_right(s, (gmb_int_t)(argc > 1 ? args[1].num : 0));
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c301(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    /* 2-arg (MID senza lunghezza): len GRANDE -> gmb_str_mid lo clampa ad `avail` (resto).
     * len<=0 (esplicito) -> "" (canone C#/IL/PCode). Prima passava 0 per il 2-arg. */
    out = gmb_str_mid(s, (gmb_int_t)(argc > 1 ? args[1].num : 0), (gmb_int_t)(argc > 2 ? args[2].num : 0x7fffffff));
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c307(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_upper(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c306(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_lower(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c314(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_trim(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c315(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_ltrim(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
/* REPLACE(s, old, new) — sostituisce TUTTE le occorrenze (parità C# String.Replace):
 * servita da gmb_bf_317 (lo stesso func_code). La copia inline `gmb_bf_299_c317`
 * portata il 2026-11-02 era un SECONDO binding sullo stesso codice: duplicato
 * segnalato da check_enum_ids e vietato da AGENTS §0bis (una sola implementazione,
 * in `gmb_str_replace` del runtime). Rimossa; la voce in tabella resta una. */

static void gmb_bf_299_c316(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_rtrim(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c308(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_pascal(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c309(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_camel(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c310(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_snake(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c311(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_kebab(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c312(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_title(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); gmb_string_free(&out);   /* out e' una stringa runtime appena allocata: copiata nella stringa VM, va liberata */
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_299_c313(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    if (argc < 1 || args[0].type != 2 || !args[0].ref)
    { bout->is_num = 0; bout->result_ref = NULL; return; }
     gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
     gmb_string_t s;
     s.data = src->data; s.length = src->length; s.ref_count = 1;
     gmb_string_t out = {0};
    out = gmb_str_sentence(s);
    if (out.data)
    {
        gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
    }
    else { bout->is_num = 0; bout->result_ref = NULL; }

}
static void gmb_bf_302(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 1 && args[1].type == 2 && args[2].type == 2)
            {
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[1].ref;
                gmb_obj_string_t* s1 = (gmb_obj_string_t*)args[2].ref;
                gmb_string_t a = { s0->data, s0->length, 1 };
                gmb_string_t b = { s1->data, s1->length, 1 };
                bout->result = (double)gmb_str_instr(a, b, (gmb_int_t)args[0].num);
            }
            else if (argc >= 2 && args[0].type == 2 && args[1].type == 2)
            {
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[0].ref;
                gmb_obj_string_t* s1 = (gmb_obj_string_t*)args[1].ref;
                gmb_string_t a = { s0->data, s0->length, 1 };
                gmb_string_t b = { s1->data, s1->length, 1 };
                gmb_int_t start = argc > 2 ? (gmb_int_t)args[2].num : 1;
                bout->result = (double)gmb_str_instr(a, b, start);
            }
            else bout->result = 0;
        }
}
static void gmb_bf_621(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Array di UDT/CLASS (`DIM Arr(n) AS Udt`): il compilatore pusha l'indice del
             * descriptor come PRIMO argomento, PRIMA del dimCount (Compiler.Assign.cs,
             * extraArgs=1) — stesso protocollo del PCodeProcessor C# (case CREATE_ARRAY), che
             * disambigua controllando se args[0] è un indice valido verso un dict con
             * "FieldNames". Senza questo controllo il VM in C leggeva SEMPRE args[0] come
             * dimCount — un array di UDT veniva dimensionato in modo errato e NESSUN elemento
             * riceveva mai un'istanza vera (`Arr(i).Campo` sempre vuoto — vedi
             * Classi-Distruttore-Ereditarieta-CrossFile-Bugs, 2026-09-02). */
            int dimStart = 0;
            int udtDescIdx = -1;
            if (argc > 1 && args[0].type == 1)
            {
                int cdIdx = (int)args[0].num;
                if (cdIdx >= 0 && cdIdx < (int)vm->chunk->consts_len &&
                    vm->chunk->constants[cdIdx].type == GMB_CONST_DICT && vm->chunk->constants[cdIdx].dict)
                {
                    const gmb_chunk_dict_t* cdChunk = vm->chunk->constants[cdIdx].dict;
                    for (uint32_t k = 0; k < cdChunk->count; k++)
                        if (!cdChunk->entries[k].key_is_int && cdChunk->entries[k].key_str &&
                            strcasecmp(cdChunk->entries[k].key_str, "FieldNames") == 0)
                        { udtDescIdx = cdIdx; dimStart = 1; break; }
                }
            }

            /* Bug (open) Arrays-Sweep-Residui-Minori #2: con UN SOLO argomento numerico oltre
             * l'eventuale descriptor UDT (nessun dimCount-prefix — "legacy path", lo stesso
             * usato da REDIM C = {...} senza dimensioni esplicite, Compiler.Arrays.cs:23:
             * EmitConstant(lit.Elements.Length - 1.0)), quel valore è l'UPPER BOUND (0-based),
             * NON il conteggio elementi — stessa convenzione già usata dal path multi-dim sotto
             * (`total = hi - lo + 1`) e dal "legacy path" di PCodeProcessor.Builtins.cs
             * (`remaining2 == 1: upper = args[dimStart]; size = upper + 1;`). Qui mancava il
             * +1: REDIM C = {100,200,300} (Length-1=2 pushato) creava un array di 2 elementi
             * invece di 3, troncando l'ultimo valore dell'inizializzatore (UBOUND(C) restava a
             * 1 invece di 2, disallineato da PCode). */
            double size = (argc - dimStart) > 0 ? (args[dimStart].type == 1 ? args[dimStart].num + 1 : 0) : 0;

            /* Dimensioni dell'array (per UBOUND(A, d)): numDims + limite superiore per dim.
             * numDims resta 0 se non c'è una dimensione reale (es. CREATE_ARRAY(0) di un
             * `DIM x$()`): così UBOUND ricade sul conteggio della lista (aggiornato da AADD). */
            int numDims = 0;
            int dimUpper[8] = {0};
            if ((argc - dimStart) >= 2 && args[dimStart].type == 1 && args[dimStart].num >= 1 &&
                args[dimStart].num <= 10 && (argc - dimStart - 1) >= (int)args[dimStart].num)
            {
                numDims = (int)args[dimStart].num;
                int remaining = argc - dimStart - 1;
                double total = 1;
                int isBounds = (remaining == numDims * 2);
                for (int d = 0; d < numDims; d++)
                {
                    double lo = isBounds ? args[dimStart + 1 + d*2].num : 0;
                    double hi = isBounds ? args[dimStart + 1 + d*2 + 1].num : args[dimStart + 1 + d].num;
                    total *= (hi - lo + 1);
                    if (d < 8) dimUpper[d] = (int)hi;
                }
                size = total;
                /* Canone array 1-based 2026-09-30: il modello nativo indicizza diretto
                 * (slot == indice utente, con auto-grow), quindi un `DIM A(1 TO 3)` deve
                 * avere lo slot upper+1 allocato (spreca 0..lower-1) invece dei soli
                 * hi-lo+1 slot: altrimenti le letture posizionali/iterazione oltre
                 * hi-lo cadono fuori lista (FOR EACH leggeva vuoto oltre il pieno).
                 * Solo 1D: le matrici restano row-major esatte (non toccare). */
                if (numDims == 1) size = (double)(dimUpper[0] + 1);
            }
            gmb_obj_shared_val_t* arr = gmb_shared_list_new();
            if (!arr) { vm_error(task, "CREATE_ARRAY: memoria"); return; }
            arr->ndims = numDims;
            /* Marcatore di ARRAY: anche un array DINAMICO (DIM d() = [...] / DIM d()) ha
             * numDims=0 ma è un array, non una LISTA → TYPENAME "ARRAY" (parità IL). Le
             * liste vere nascono da gmb_shared_list_new()/ctor COLLECTION, non da qui. */
            arr->is_array = 1;
            for (int d = 0; d < numDims && d < 8; d++) arr->dim_upper[d] = dimUpper[d];
            /* Canone LBOUND 2026-09-30: persiste anche il lower dichiarato (prima scartato →
               LBOUND sempre 0 e FOR EACH iterava 0..UBOUND invece di LBOUND..UBOUND). */
            if (numDims > 0 && (argc - dimStart - 1) == numDims * 2) {
                for (int d = 0; d < numDims && d < 8; d++)
                    arr->dim_lower[d] = (int)args[dimStart + 1 + d*2].num;
            }
            for (double i = 0; i < size; i++)
            {
                if (udtDescIdx >= 0)
                {
                    gmb_obj_shared_val_t* inst = vm_make_default_instance(vm, udtDescIdx);
                    gmb_list_add(arr->list, inst ? gmb_make_val_obj((gmb_obj_t*)inst) : gmb_val_nil());
                    if (inst) gmb_obj_release((gmb_obj_t*)inst);   /* la lista ha il suo riferimento */
                }
                else gmb_list_add(arr->list, gmb_val_nil());
            }
            bout->is_num = 0;
            bout->result_ref = (gmb_obj_t*)arr;
            bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
static void gmb_bf_625(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_list_add(sv->list, gmb_adapter_stack_to_val_borrowed(args[1]));
            }
            else if (argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                /* STACK/QUEUE come collezione del registry (OP_NEW typeCode 4/5): Push = add in testa, Enqueue = in coda.
                 * Prima il builtin gestiva solo GMB_OBJ_LIST → Push era un no-op (Count restava 0). */
                gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                gmb_val_t cv = gmb_adapter_stack_to_val_borrowed(args[1]);
                if (func_code == 625) gmb_coll_enqueue(col->handle, cv);
                else gmb_coll_add(col->handle, cv);
            }
            bout->result = 0;
        }
}
static void gmb_bf_626(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t pv = gmb_val_nil();
            int have = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->list && sv->list->count > 0)
                {
                    pv = func_code == 626 ? gmb_list_dequeue(sv->list) : gmb_list_pop(sv->list);
                    have = 1;
                }
            }
            else if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                /* STACK/QUEUE come collezione del registry: Pop/Dequeue = testa (stesso path del metodo .Pop()/.Dequeue()). */
                pv = gmb_coll_dequeue(((gmb_obj_collection_t*)args[0].ref)->handle);
                have = 1;
            }
            if (have)
            {
                gmb_stack_entry_t s = gmb_adapter_val_to_stack(pv);   /* entry POSSEDUTA */
                gmb_val_release(pv);                                   /* pop/dequeue cedono il riferimento della lista */
                if (s.type == 2 && s.ref) { bout->is_num = 0; bout->result_ref = s.ref; bout->result_ref_bt = s.basic_type; }
                else if (s.type == 1) { bout->is_num = 1; bout->result = s.num; bout->num_bt = s.basic_type; }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_629(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                const char* k = (args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                gmb_dict_set(sv->dict, k, gmb_adapter_stack_to_val_borrowed(args[2]));
            }
            bout->result = 0;
        }
}
static void gmb_bf_630(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                int tck = col->type_code;
                if (tck == 16) 
                {
                    int n = 0;
                    char** keys = gmb_coll_cache_keys(col->handle, &n);
                    gmb_obj_shared_val_t* out = gmb_shared_list_new();
                    if (out)
                    {
                        if (keys)
                            for (int i = 0; i < n; i++)
                            {
                                gmb_obj_string_t* s = gmb_str_cstr(keys[i]);
                                gmb_list_add(out->list, gmb_make_val_string_counted(s->data));
                                gmb_str_release(s);
                                free(keys[i]);
                            }
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                    }
                    free(keys);
                }
                if (tck == 6 || tck == 7 || tck == 8 || tck == 9 || tck == 26 || tck == 18 || tck == 19)
                {
                    int n = 0;
                    gmb_val_t* keys = gmb_coll_kv_keys(col->handle, &n);
                    gmb_obj_shared_val_t* out = gmb_shared_list_new();
                    if (out)
                    {
                        if (keys)
                            for (int i = 0; i < n; i++) gmb_list_add(out->list, keys[i]);
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                    }
                    free(keys);
                }
                /* NIENTE fallthrough: il reset finale di questa funzione
                 * (`bout->is_num = 0; bout->result_ref = NULL;`) azzerava il risultato
                 * appena costruito → TREE/AVL/REDBLACK/CACHE .Keys() stampavano "null".
                 * Il ramo DICT qui sotto ha già il proprio `return`; per un tipo non
                 * chiave→valore si esce con NIL esplicito. */
                if (!bout->result_ref) bout->is_num = 0;
                return;
            }
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                /* d.Keys() compila a OP_CALL_BUILTIN DICT_KEYS (func_code 630), NON a
                 * OP_CALL_MEMBER/property "Keys" — percorso distinto da vm_list_fill_keys
                 * (gmb_vm_core.c). gmb_dict_keys() (gmb_dict.c) forza sempre stringa: qui
                 * ricostruiamo il tipo originale della chiave via gmb_dict_key_val, stessa
                 * logica di vm_list_fill_keys, per parità con PCode su DICT OF DOUBLE,... */
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* keys = gmb_shared_list_new();
                if (keys)
                {
                    for (size_t i = 0; i < sv->dict->count; i++)
                        vm_list_add_fresh(keys->list, gmb_dict_key_val(sv->dict->items[i].key ? sv->dict->items[i].key : ""));
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)keys; bout->result_ref_bt = GMB_OBJ_LIST;
                }
                return;
            }
            /* Argomento NON dict/collection (es. una stringa): KEYS non è applicabile →
             * NIL esplicito (parità IL/PCode: JSON.STRINGIFY(JSON.KEYS("...")) == "null").
             * Prima `bout` restava non inizializzato → output vuoto/garbage. */
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_631(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                int tcv = col->type_code;
                if (tcv == 6 || tcv == 7 || tcv == 8 || tcv == 9 || tcv == 26 || tcv == 18 || tcv == 19)
                {
                    int n = 0;
                    gmb_val_t* pairs = gmb_coll_kv_pairs(col->handle, &n);
                    gmb_obj_shared_val_t* out = gmb_shared_list_new();
                    if (out)
                    {
                        if (pairs)
                            for (int i = 1; i < n; i += 2) gmb_list_add(out->list, pairs[i]);
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                    }
                    free(pairs);
                }
            }
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* vals = gmb_shared_list_new();
                if (vals)
                {
                    for (size_t i = 0; i < sv->dict->count; i++)
                        gmb_list_add(vals->list, sv->dict->items[i].value);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)vals; bout->result_ref_bt = GMB_BT_OBJECT;
                }
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_632(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                /* Contenitore CHIAVE→VALORE (TREE/AVL/REDBLACK/BPlusTree/Timeline/
                 * HashRing): ToArray() è un array di dict {Key,Value} (parità P-Code/IL:
                 * JSON [{"Key":"a","Value":true},…]). Prima usciva solo la lista dei
                 * VALORI ([true,false,false]) perché si passava sempre da
                 * gmb_coll_to_array. Materializzazione dal runtime condiviso (§0bis). */
                int is_kv = 0;
                gmb_list_t kvl = gmb_coll_kv_entries_to_list(col->handle, &is_kv);
                /* SOLO per un contenitore CHIAVE→VALORE si restituiscono le coppie. Per gli
                 * altri (LIST/QUEUE/STACK/HASHSET/HEAP...) kvl è VUOTA e is_kv=0: prima si
                 * usciva comunque con quella lista vuota → `hs.ToArray()` = [] (e
                 * JSON.Stringify(hs.ToArray()) = []), mentre Count era corretto
                 * (sample Collections/CollectionsFluent, gamba VM/C). */
                if (is_kv)
                {
                    gmb_obj_shared_val_t* outkv = gmb_shared_list_new();
                    if (outkv)
                    {
                        for (size_t i = 0; i < kvl.count; i++)
                            gmb_list_add(outkv->list, kvl.items[i]);
                        gmb_list_free(&kvl);   /* gmb_list_add trattiene: si rilasciano gli originali */
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)outkv; bout->result_ref_bt = GMB_BT_OBJECT;
                        return;   /* NON azzerare result_ref (il reset finale lo annullava → ToArray=nil) */
                    }
                }
                gmb_list_free(&kvl);
                int n = 0;
                gmb_val_t* arr = gmb_coll_to_array(col->handle, &n);
                gmb_obj_shared_val_t* out = gmb_shared_list_new();
                if (out)
                {
                    if (arr)
                        for (int i = 0; i < n; i++) gmb_list_add(out->list, arr[i]);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                free(arr);
                return;   /* NON azzerare result_ref (il reset finale lo annullava → ToArray=nil) */
            }
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                (args[0].ref->kind == GMB_OBJ_LIST || args[0].ref->kind == GMB_OBJ_DICT))
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* out = gmb_shared_list_new();
                if (out)
                {
                    if (args[0].ref->kind == GMB_OBJ_LIST)
                        for (size_t i = 0; i < sv->list->count; i++) gmb_list_add(out->list, sv->list->items[i]);
                    else
                        for (size_t i = 0; i < sv->dict->count; i++) gmb_list_add(out->list, sv->dict->items[i].value);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                return;   /* idem: senza return il reset finale annullava il risultato */
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_634(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* out = gmb_shared_dict_new();
                if (out)
                {
                    for (size_t i = 0; i < sv->dict->count; i++)
                        gmb_dict_set(out->dict, sv->dict->items[i].key, sv->dict->items[i].value);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                    /* Senza il return, il reset finale azzerava il risultato: `d.ToDict()`
                     * valeva NIL e un successivo `c.Remove(...)` andava in errore 91
                     * (sample Collections/CollectionsDictLists, gamba VM/C). Stesso
                     * pattern già corretto in gmb_bf_632 per ToArray. */
                    return;
                }
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_635(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_list_sort(sv->list);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_636(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_val_t v = gmb_adapter_stack_to_val_borrowed(args[2]);
                gmb_list_insert(sv->list, (gmb_int_t)args[1].num, v);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_637(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST &&
                args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* d = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* s = (gmb_obj_shared_val_t*)args[1].ref;
                gmb_list_add_range(d->list, *s->list);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_638(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_list_reverse(sv->list);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_639(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_val_t v;
                if (sv->list->count == 0) { bout->is_num = 0; bout->result_ref = NULL; }
                if (func_code == 638) v = gmb_list_first(*sv->list);
                else if (func_code == 641) v = gmb_list_last(*sv->list);
                else v = gmb_list_peek(*sv->list);
                gmb_stack_entry_t s = gmb_adapter_val_to_stack(v);
                if (s.type == 2 && s.ref) { bout->is_num = 0; bout->result_ref = s.ref; bout->result_ref_bt = s.basic_type; }
                else if (s.type == 1) { bout->result = s.num; }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_640(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_LIST) gmb_list_clear(((gmb_obj_shared_val_t*)args[0].ref)->list);
                else if (args[0].ref->kind == GMB_OBJ_DICT) gmb_dict_clear(((gmb_obj_shared_val_t*)args[0].ref)->dict);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_641(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_list_remove_at(sv->list, (gmb_int_t)args[1].num);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_643(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_val_t v = gmb_adapter_stack_to_val(args[1]);
                bout->result = (double)gmb_list_index_of(*sv->list, v);
            }
            bout->result = -1;
        }
}
static void gmb_bf_644(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                int start = (int)args[1].num, count = (int)args[2].num;
                if (start >= 0 && count > 0 && start + count <= (int)sv->list->count)
                {
                    gmb_obj_shared_val_t* out = gmb_shared_list_new();
                    if (out)
                    {
                        for (int j = 0; j < count; j++) gmb_list_add(out->list, sv->list->items[start + j]);
                        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                    }
                }
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_645(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_DICT)
                {
                    const char* k = (args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                    gmb_val_t v = gmb_dict_get(*((gmb_obj_shared_val_t*)args[0].ref)->dict, k);
                    gmb_stack_entry_t s = gmb_adapter_val_to_stack(v);
                    if (s.type == 2 && s.ref) { bout->is_num = 0; bout->result_ref = s.ref; bout->result_ref_bt = s.basic_type; }
                    else if (s.type == 1) { bout->result = s.num; }
                    else { bout->is_num = 0; bout->result_ref = NULL; }
                }
                else if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    int idx = (int)args[1].num;
                    if (idx >= 0 && idx < (int)sv->list->count)
                    {
                        gmb_val_t v = gmb_list_get(*sv->list, idx);
                        gmb_stack_entry_t s = gmb_adapter_val_to_stack(v);
                        if (s.type == 2 && s.ref) { bout->is_num = 0; bout->result_ref = s.ref; bout->result_ref_bt = s.basic_type; }
                        else if (s.type == 1) { bout->result = s.num; }
                        else { bout->is_num = 0; bout->result_ref = NULL; }
                    }
                }
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_646(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_DICT)
                {
                    const char* k = (args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                    gmb_dict_set(((gmb_obj_shared_val_t*)args[0].ref)->dict, k, gmb_adapter_stack_to_val_borrowed(args[2]));
                }
                else if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    int idx = (int)args[1].num;
                    if (idx >= 0 && idx < (int)sv->list->count)
                        gmb_list_set(sv->list, idx, gmb_adapter_stack_to_val_borrowed(args[2]));
                }
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_647(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            
            if (argc >= 1)
            {
                if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                {
                    const char* s = ((gmb_obj_string_t*)args[0].ref)->data;
                    char* end = NULL;
                    double num = strtod(s, &end);
                    if (end && *end == '\0' && end != s)
                    {
                        bout->result = num;
                    }
                    bout->is_num = 0; bout->result_ref = NULL;
                }
                if (args[0].type == 1) { bout->result = args[0].num; }
                bout->is_num = 0; gmb_obj_retain(args[0].ref); bout->result_ref = (gmb_obj_t*)args[0].ref; bout->result_ref_bt = args[0].basic_type;
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_760(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* NIL (nessun argomento o argomento vuoto) → "null" (parità IL/PCode:
             * JSON.STRINGIFY(NIL) == "null"; prima usciva stringa vuota). */
            if (argc < 1 || args[0].type == 0 || (args[0].type == 2 && !args[0].ref))
            {
                gmb_obj_string_t* rn = gmb_str_cstr("null");
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)rn; bout->result_ref_bt = GMB_BT_STRING;
                return;
            }
            if (argc >= 1 && args[0].type == 2)
            {
                gmb_obj_t* o = args[0].ref;
                gmb_obj_string_t* r = gmb_vm_json_stringify(o);
                if (r)
                {
                    bout->is_num = 0;
                    bout->result_ref = (gmb_obj_t*)r;
                    bout->result_ref_bt = GMB_BT_STRING;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else if (argc >= 1 && args[0].type == 1)
            {
                /* SCALARE numerico/bool: mancava il ramo, quindi JSON.STRINGIFY(5) e
                 * JSON.STRINGIFY(TRUE) uscivano STRINGA VUOTA (parità IL/PCode/C99Native
                 * rotta). Serializzazione dal runtime condiviso (§0bis).
                 * Il BOOL si distingue dal numero perché OP_PUSH_TRUE/FALSE pusha -1.0/0.0 con
                 * basic_type GMB_BT_BOOL: senza il flag `TRUE` diventerebbe `-1` invece di
                 * `true` (IL/PCode rendono true). */
                gmb_val_t sv = (args[0].basic_type == GMB_BT_BOOL)
                    ? gmb_make_val_bool(args[0].num != 0.0 ? -1 : 0)
                    : gmb_make_val_double(args[0].num);
                gmb_string_t js = gmb_val_to_json(sv);
                gmb_obj_string_t* rs = gmb_str_cstr(js.data ? js.data : "null");
                gmb_string_free(&js);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)rs; bout->result_ref_bt = GMB_BT_STRING;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
void gmb_bf_325(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            /* LEDGER.COUNT/LASTREC usano i token CONDIVISI Count/Lastrec: se l'argomento è
             * l'handle del ledger corrente si ritorna il numero di transazioni (prima questi
             * codici cadevano nei rami DBF/collezioni → 0). § MiniTests/ledger/ledger_ops.bas. */
            /* Il ledger deve essere APERTO (handle > 0): senza ledger gmb_db_current_ledger()
             * vale 0, e un argomento che e' un IDENTIFICATORE (alias d'area, es. `ISAM.LASTREC(X)`)
             * si compila a 0 → il confronto 0==0 faceva entrare l'area ISAM nel ramo LEDGER e
             * LASTREC/COUNT ritornavano gmb_db_ledger_count(0) = 0 invece del conteggio d'area
             * (misurato su VM/C: ISAM.LASTREC(X)=0 contro 3, § minitests/dbf/isam_commands.bas). */
            int64_t lbq = gmb_db_current_ledger();
            if ((func_code == 328 || func_code == 469) && lbq > 0 && argc >= 1 && args[0].type == 1
                && (int64_t)args[0].num == lbq)
            {
                bout->result = (double)gmb_db_ledger_count(lbq);
                return;
            }

        {
            
            if (argc == 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_GRAPH)
                {
                    gmb_obj_graph_t* g = (gmb_obj_graph_t*)args[0].ref;
                    bout->result = (double)gmb_graph_node_count(g->handle);
                }
                if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    bout->result = (double)gmb_list_count(*sv->list);
                }
                if (args[0].ref->kind == GMB_OBJ_DICT && func_code == 328)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    bout->result = (double)gmb_dict_count(*sv->dict);
                }
                if (args[0].ref->kind == GMB_OBJ_COLLECTION)
                {
                    gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                    bout->result = (double)gmb_coll_count(col->handle);
                }
            }
            /* CONTAINS(lista, valore) con needle NON-ref (numerico): il ramo sotto richiede
             * args[1] come REF (stringa/oggetto), quindi un needle numerico lo saltava e
             * CONTAINS tornava 0 → nessun dedup. La set comprehension ({x FOR x IN c}) usa
             * proprio CONTAINS con needle numerico (ArraysComprehensionFull.bas: il set
             * conteneva i duplicati). gmb_list_contains accetta qualunque gmb_val_t. */
            if (argc >= 2 && func_code == 325 && args[0].type == 2 && args[0].ref
                && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_val_t v = gmb_adapter_stack_to_val(args[1]);
                bout->result = gmb_list_contains(*sv->list, v) ? 1.0 : 0.0;
                return;
            }
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[1].type == 2 && args[1].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_COLLECTION && func_code == 328)
                {
                    
                    gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                    gmb_val_t needle = gmb_adapter_stack_to_val(args[1]);
                    bout->result = 0;
                    int n = 0;
                    gmb_val_t* arr = gmb_coll_to_array(col->handle, &n);
                    if (arr)
                    {
                        for (int i = 0; i < n; i++)
                            if (gmb_val_equals(arr[i], needle)) bout->result += 1;
                        free(arr);
                    }
                }
                if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    if (func_code == 325) {
                        gmb_val_t v = gmb_adapter_stack_to_val(args[1]);
                        bout->result = gmb_list_contains(*sv->list, v) ? 1.0 : 0.0;
                    } else if (func_code == 328) {
                        bout->result = (double)gmb_list_count(*sv->list);
                    } else bout->result = 0;
                }
                if (args[0].ref->kind == GMB_OBJ_DICT && func_code == 325)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    const char* k = ((gmb_obj_string_t*)args[1].ref)->data;
                    bout->result = gmb_dict_contains_key(*sv->dict, k) ? -1.0 : 0.0;  /* TRUE BASIC = -1 (parità IL/PCode) */
                }
                /* Collezione: il conteggio è GIÀ in bout->result (scan di gmb_coll_to_array
                 * sopra, che copre anche il BAG come multiset). Va solo tenuto: prima qui si
                 * azzerava tutto → COLL.Count(item) usciva NIL (PRINT vuoto). */
                if (args[0].ref->kind == GMB_OBJ_COLLECTION) return;
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[0].ref;
                gmb_obj_string_t* s1 = (gmb_obj_string_t*)args[1].ref;
                if (s0->data && s1->data)
                {
                    gmb_string_t a = { s0->data, s0->length, 1 };
                    gmb_string_t b = { s1->data, s1->length, 1 };
                    if (func_code == 325) bout->result = gmb_str_contains(a, b) ? -1.0 : 0.0;
                    else if (func_code == 326) bout->result = gmb_str_startswith(a, b) ? -1.0 : 0.0;
                    else if (func_code == 327) bout->result = gmb_str_endswith(a, b) ? -1.0 : 0.0;
                    else bout->result = (double)gmb_str_count(a, b);
                }
                else bout->result = 0;
            }
        }
}
static void gmb_bf_304(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t out = gmb_str_space((gmb_int_t)args[0].num);
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_303(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int count = (int)args[0].num;
            if (count < 0) count = 0;
            /* DUP(n, ch): n copie del PRIMO carattere UTF-8 di `ch` (stringa) oppure di
             * CHR(codice) se numerico — stessa semantica di IL (IlRuntime.Dup), PCode e
             * C99Native (gmb_dup_str/gmb_dup_cp). Prima qui si copiava fino a 7 BYTE della
             * stringa: DUP(3, "ab") dava "ababab" su VM/C e "aaa" sulle altre gambe. */
            gmb_string_t out;
            if (args[1].type == 2 && args[1].ref)
            {
                gmb_obj_string_t* sc = (gmb_obj_string_t*)args[1].ref;
                out = gmb_dup_str((gmb_int_t)count, sc->data ? sc->data : "");
            }
            else
            {
                out = gmb_dup_cp((gmb_int_t)count, (int)args[1].num);
            }
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_317(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 2 && args[0].ref && args[1].type == 2 && args[1].ref && args[2].type == 2 && args[2].ref)
            {
                gmb_obj_string_t* a = (gmb_obj_string_t*)args[0].ref;
                gmb_obj_string_t* b = (gmb_obj_string_t*)args[1].ref;
                gmb_obj_string_t* c = (gmb_obj_string_t*)args[2].ref;
                gmb_string_t ga = { a->data, a->length, 1 };
                gmb_string_t gb = { b->data, b->length, 1 };
                gmb_string_t gc = { c->data, c->length, 1 };
                gmb_string_t out = gmb_str_replace(ga, gb, gc);
                if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_329(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[1].type == 2 && args[1].ref)
            {
                gmb_obj_string_t* a = (gmb_obj_string_t*)args[0].ref;
                gmb_obj_string_t* b = (gmb_obj_string_t*)args[1].ref;
                gmb_string_t ga = { a->data, a->length, 1 };
                gmb_string_t gb = { b->data, b->length, 1 };
                gmb_string_t gempty = { "", 0, 1 };
                gmb_string_t out = gmb_str_replace(ga, gb, gempty);
                if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_323(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[1].type == 2 && args[1].ref)
            {
                gmb_obj_string_t* h = (gmb_obj_string_t*)args[0].ref;
                gmb_obj_string_t* n = (gmb_obj_string_t*)args[1].ref;
                gmb_string_t a = { h->data, h->length, 1 };
                gmb_string_t b = { n->data, n->length, 1 };
                bout->result = gmb_str_search(a, b) ? -1.0 : 0.0;
            }
            else bout->result = -1;
        }
}
static void gmb_bf_324(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                bout->is_num = 0;
                bout->result_ref = args[0].ref;
                bout->result_ref_bt = GMB_BT_STRING;
                gmb_obj_retain(bout->result_ref);
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_320(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[0].ref;
                gmb_string_t a = { s0->data, s0->length, 1 };
                gmb_int_t w = argc > 1 ? (gmb_int_t)args[1].num : 80;
                gmb_int_t al = argc > 2 ? (gmb_int_t)args[2].num : 0;
                gmb_string_t out = gmb_str_wrap(a, w, al);
                if (out.data)
                {
                    gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_333(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_string_t* s = gmb_str_cstr("\t");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_330(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[0].ref;
                int w = (int)args[1].num;
                char pc = argc >= 3 && args[2].type == 2 && args[2].ref
                    ? ((gmb_obj_string_t*)args[2].ref)->data[0] : ' ';
                int len = (int)s0->length;
                char* out = (char*)calloc((size_t)(w + 1), 1);
                if (out)
                {
                    if (func_code == 330) 
                    {
                        int pad = w - len; for (int i = 0; i < pad && i < w; i++) out[i] = pc;
                        for (int i = 0; i < len && pad + i < w; i++) out[pad + i] = s0->data[i];
                    }
                    else if (func_code == 331) 
                    {
                        for (int i = 0; i < len && i < w; i++) out[i] = s0->data[i];
                        for (int i = len; i < w; i++) out[i] = pc;
                    }
                    else 
                    {
                        int pad = w - len; int left = pad / 2;
                        for (int i = 0; i < left && i < w; i++) out[i] = pc;
                        for (int i = 0; i < len && left + i < w; i++) out[left + i] = s0->data[i];
                        for (int i = left + len; i < w; i++) out[i] = pc;
                    }
                    gmb_obj_string_t* r = gmb_str_cstr(out);
                    free(out);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_318(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Canone 2026-09-30: REVERSE su LISTA ritorna un clone invertito (parità PCode);
             * prima il ref veniva letto come stringa senza controllo kind → garbage/vuoto e
             * FOR EACH su REVERSE non iterava. Su STRINGA: caratteri invertiti (invariato). */
            if (argc >= 1 && args[0].type == 2 && args[0].ref
                && args[0].ref->kind == GMB_OBJ_LIST
                && ((gmb_obj_shared_val_t*)args[0].ref)->list)
            {
                gmb_obj_shared_val_t* src = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* r = gmb_shared_list_new();
                if (r && r->list) {
                    gmb_list_t* sl = src->list;
                    for (long i = (long)sl->count - 1; i >= 0; i--) {
                        gmb_val_t v = gmb_list_get(*sl, (gmb_int_t)i);
                        gmb_list_add(r->list, v);   /* add trattiene da se' */
                    }
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
                return;
            }
            if (argc >= 1 && args[0].type == 2 && args[0].ref
                && args[0].ref->kind == GMB_OBJ_STRING)
            {
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[0].ref;
                char* out = (char*)calloc((size_t)s0->length + 1, 1);
                if (out)
                {
                    for (int i = 0; i < (int)s0->length; i++)
                        out[i] = s0->data[s0->length - 1 - i];
                    gmb_obj_string_t* r = gmb_str_cstr(out);
                    free(out);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_322(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                const char* sep = (argc >= 2 && args[1].type == 2 && args[1].ref)
                    ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                gmb_string_t gs = gmb_str_join_list(sv->list->items, (int)sv->list->count, sep);
                if (gs.data) { gmb_obj_string_t* r = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
                else bout->is_num = 0;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_619(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double total = 0;
            int n = 0;
            double* data = argc >= 1 && args[0].type == 2 && args[0].ref
                ? gmb_vm_flatten_dbl(&args[0], &n)
                : NULL;
            if (data)
            {
                for (int i = 0; i < n; i++) total += data[i];
                free(data);
                bout->result = total;
            }
            else
            {
                for (int i = 0; i < argc; i++) total += args[i].num;
                bout->result = total;
            }
        }
}
static void gmb_bf_620(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double total = 0;
            int n = 0;
            double* data = argc >= 1 && args[0].type == 2 && args[0].ref
                ? gmb_vm_flatten_dbl(&args[0], &n)
                : NULL;
            if (data)
            {
                for (int i = 0; i < n; i++) total += data[i];
                free(data);
                bout->result = n > 0 ? total / n : 0;
            }
            else
            {
                for (int i = 0; i < argc; i++) total += args[i].num;
                bout->result = argc > 0 ? total / argc : 0;
            }
        }
}
static void gmb_bf_321(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[1].type == 2 && args[1].ref)
            {
                gmb_obj_string_t* s0 = (gmb_obj_string_t*)args[0].ref;
                gmb_obj_string_t* s1 = (gmb_obj_string_t*)args[1].ref;
                gmb_obj_shared_val_t* out = gmb_shared_list_new();
                if (out)
                {
                    int n = 0;
                    gmb_val_t* parts = gmb_str_split_list(s0->data, s1->data, &n);
                    for (int i = 0; i < n; i++)
                    {
                        /* parts[i] e' una stringa FRESCA e non condivisa: la lista ne riceve una copia contata (liberata
                         * con la lista) e il buffer originale si libera qui. */
                        gmb_list_add(out->list, gmb_make_val_string_counted_n(parts[i].v_string.data, parts[i].v_string.length));
                        gmb_string_free(&parts[i].v_string);
                    }
                    free(parts);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_336(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const uint8_t* data = NULL;
            size_t len = 0;
            
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    len = sv->list->count;
                    static uint8_t bin[8192];
                    size_t cl = len < sizeof(bin) ? len : sizeof(bin);
                    for (size_t i = 0; i < cl; i++)
                    {
                        gmb_val_t v = sv->list->items[i];
                        bin[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0);
                    }
                    data = bin;
                    len = cl;
                }
                else
                {
                    gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                    data = (const uint8_t*)s->data;
                    len = s->length;
                }
            }
            uint8_t out[64] = {0};
            size_t outlen = 0;
            if (func_code == 334 || func_code == 343) { gmb_md5(data, len, out); outlen = 16; }
            else if (func_code == 335 || func_code == 344) { gmb_sha1(data, len, out); outlen = 20; }
            else if (func_code == 337 || func_code == 346) { gmb_sha512(data, len, out); outlen = 64; }
            else if (func_code == 336 || func_code == 345) { gmb_sha256(data, len, out); outlen = 32; }
            
            if (func_code == 343 || func_code == 344 || func_code == 345 || func_code == 346)
            {
                char hex[130];
                gmb_hex_encode(out, outlen, hex);
                hex[outlen * 2] = '\0';   /* gmb_hex_encode NON termina: senza NUL gmb_str_cstr legge oltre (garbage) */
                gmb_obj_string_t* r = gmb_str_cstr(hex);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            else
            {
                gmb_obj_shared_val_t* res = gmb_shared_list_new();
                if (res)
                {
                    for (size_t i = 0; i < outlen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
        }
}
static void gmb_bf_356(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* pw = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* h = gmb_password_hash(pw);
            if (h) { gmb_obj_string_t* r = gmb_str_cstr(h); free(h); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_357(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* pw = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* st = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            /* Parità con l'oracolo (IlRuntime.PasswordVerify -> bool): vero = 1, falso = 0.
             * Prima ritornava -1 quando la password era CORRETTA (stessa classe del bug
             * VERIFYNOTE: valore inventato che divergeva da IL/PCode). */
            bout->result = gmb_password_verify(pw, st) ? -1.0 : 0.0;
        }
}
static void gmb_bf_342(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* algo = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[1].ref)->data : "SHA256";
            uint8_t buf[65536];
            FILE* fp = fopen(path, "rb");
            char outhex[160] = "";
            if (fp)
            {
                uint8_t h[64];
                if (strcasecmp(algo, "MD5") == 0) { gmb_md5(NULL, 0, h); }
                
                size_t total = 0;
                uint8_t* all = (uint8_t*)malloc(4 * 1024 * 1024);
                if (all)
                {
                    size_t rd = fread(all, 1, 4 * 1024 * 1024, fp);
                    total = rd;
                    if (strcasecmp(algo, "MD5") == 0) gmb_md5(all, total, h);
                    else if (strcasecmp(algo, "SHA1") == 0) gmb_sha1(all, total, h);
                    else gmb_sha256(all, total, h);
                    free(all);
                }
                fclose(fp);
                size_t hlen = strcasecmp(algo, "MD5") == 0 ? 16 : strcasecmp(algo, "SHA1") == 0 ? 20 : 32;
                /* HASHFILE ritorna byte[] (StdLib byteArrayType): lista di byte. */
                gmb_obj_shared_val_t* rl = gmb_shared_list_new();
                if (rl) for (size_t i = 0; i < hlen; i++) gmb_list_add(rl->list, gmb_make_val_double(h[i]));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)rl; bout->result_ref_bt = GMB_BT_OBJECT;
                return;
            }
            (void)buf;
            gmb_obj_shared_val_t* empty = gmb_shared_list_new();
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)empty; bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
/* Espande ricorsivamente un paramarray variadico (lista, anche annidata) in
 * args numerici/stringa per ABIENCODE (parità IL ExtractVariadicValues). */
static void gmb_abi_flatten_list(gmb_obj_shared_val_t* lv, double* argsN,
                                 const char** strargs, int* stridx, int* n, int max)
{
    if (!lv || !lv->list) return;
    for (size_t k = 0; k < lv->list->count && *n < max; k++)
    {
        gmb_val_t ev = lv->list->items[k];
        if (ev.kind == GMB_TYPE_OBJECT && ev.v_ptr && ((gmb_obj_t*)ev.v_ptr)->kind == GMB_OBJ_LIST)
            gmb_abi_flatten_list((gmb_obj_shared_val_t*)ev.v_ptr, argsN, strargs, stridx, n, max);
        else if (ev.kind == GMB_TYPE_STRING && ev.v_string.data)
        { strargs[*n] = ev.v_string.data; stridx[*n] = 1; (*n)++; }
        else { argsN[*n] = gmb_val_to_double(ev); (*n)++; }
    }
}
static void gmb_bf_352(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t data[4096], key[64], nonce[16];
            size_t dlen = 0, klen = 0, nlen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                dlen = sv->list->count < sizeof(data) ? sv->list->count : sizeof(data);
                for (size_t i = 0; i < dlen; i++) { gmb_val_t v = sv->list->items[i]; data[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                dlen = s->length < sizeof(data) ? s->length : sizeof(data);
                memcpy(data, s->data, dlen);
            }
            if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
                klen = sv->list->count < sizeof(key) ? sv->list->count : sizeof(key);
                for (size_t i = 0; i < klen; i++) { gmb_val_t v = sv->list->items[i]; key[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[2].ref;
                nlen = sv->list->count < sizeof(nonce) ? sv->list->count : sizeof(nonce);
                for (size_t i = 0; i < nlen; i++) { gmb_val_t v = sv->list->items[i]; nonce[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            uint8_t* out = (uint8_t*)malloc(dlen ? dlen : 1);
            if (!out) { bout->is_num = 0; bout->result_ref = NULL; }
            if (klen >= 32 && nlen >= 12) gmb_chacha20_xor(key, nonce, data, dlen, out);
            else memcpy(out, data, dlen);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res)
            {
                for (size_t i = 0; i < dlen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                free(out);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_350(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t data[8192];
            size_t dlen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                dlen = sv->list->count < sizeof(data) ? sv->list->count : sizeof(data);
                for (size_t i = 0; i < dlen; i++) { gmb_val_t v = sv->list->items[i]; data[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                /* AESENCRYPT(data$, key$): data come stringa (CryptoBytes lato IL). */
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                dlen = s->length < sizeof(data) ? s->length : sizeof(data);
                memcpy(data, s->data, dlen);
            }
            const char* pw = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            size_t olen = 0;
            uint8_t* out = gmb_aes_encrypt_password(data, dlen, pw, &olen);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res && out)
            {
                for (size_t i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                free(out);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_351(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t enc[16384];
            size_t elen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                elen = sv->list->count < sizeof(enc) ? sv->list->count : sizeof(enc);
                for (size_t i = 0; i < elen; i++) { gmb_val_t v = sv->list->items[i]; enc[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                elen = s->length < sizeof(enc) ? s->length : sizeof(enc);
                memcpy(enc, s->data, elen);
            }
            const char* pw = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            size_t olen = 0;
            uint8_t* out = gmb_aes_decrypt_password(enc, elen, pw, &olen);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res && out)
            {
                for (size_t i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                free(out);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_354(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t pt[8192], key[64], nonce[16], aad[1024];
            size_t plen = 0, klen = 0, nlen = 0, alen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                plen = sv->list->count < sizeof(pt) ? sv->list->count : sizeof(pt);
                for (size_t i = 0; i < plen; i++) { gmb_val_t v = sv->list->items[i]; pt[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                plen = s->length < sizeof(pt) ? s->length : sizeof(pt);
                memcpy(pt, s->data, plen);
            }
            if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
                klen = sv->list->count < sizeof(key) ? sv->list->count : sizeof(key);
                for (size_t i = 0; i < klen; i++) { gmb_val_t v = sv->list->items[i]; key[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[2].ref;
                nlen = sv->list->count < sizeof(nonce) ? sv->list->count : sizeof(nonce);
                for (size_t i = 0; i < nlen; i++) { gmb_val_t v = sv->list->items[i]; nonce[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[3].ref;
                alen = sv->list->count < sizeof(aad) ? sv->list->count : sizeof(aad);
                for (size_t i = 0; i < alen; i++) { gmb_val_t v = sv->list->items[i]; aad[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            uint8_t* ct = (uint8_t*)malloc(plen ? plen : 1);
            uint8_t tag[16];
            if (!ct) { bout->is_num = 0; bout->result_ref = NULL; }
            if (klen >= 32 && nlen >= 12) gmb_chacha20poly1305_encrypt(key, nonce, aad, alen, pt, plen, ct, tag);
            else { memcpy(ct, pt, plen); memset(tag, 0, 16); }
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                gmb_obj_shared_val_t* ctl = gmb_shared_list_new();
                if (ctl) { for (size_t i = 0; i < plen; i++) gmb_list_add(ctl->list, gmb_make_val_double(ct[i])); }
                gmb_obj_shared_val_t* tagl = gmb_shared_list_new();
                if (tagl) { for (int i = 0; i < 16; i++) gmb_list_add(tagl->list, gmb_make_val_double(tag[i])); }
                if (ctl) gmb_dict_set(out->dict, "ciphertext", gmb_make_val_obj((gmb_obj_t*)ctl));
                if (tagl) gmb_dict_set(out->dict, "tag", gmb_make_val_obj((gmb_obj_t*)tagl));
                free(ct);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(ct); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_355(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t ct[8192], tag[16], key[64], nonce[16], aad[1024];
            size_t clen = 0, tlen = 0, klen = 0, nlen = 0, alen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                clen = sv->list->count < sizeof(ct) ? sv->list->count : sizeof(ct);
                for (size_t i = 0; i < clen; i++) { gmb_val_t v = sv->list->items[i]; ct[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
                tlen = sv->list->count < 16 ? sv->list->count : 16;
                for (size_t i = 0; i < tlen; i++) { gmb_val_t v = sv->list->items[i]; tag[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[2].ref;
                klen = sv->list->count < sizeof(key) ? sv->list->count : sizeof(key);
                for (size_t i = 0; i < klen; i++) { gmb_val_t v = sv->list->items[i]; key[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[3].ref;
                nlen = sv->list->count < sizeof(nonce) ? sv->list->count : sizeof(nonce);
                for (size_t i = 0; i < nlen; i++) { gmb_val_t v = sv->list->items[i]; nonce[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            if (argc >= 5 && args[4].type == 2 && args[4].ref && args[4].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[4].ref;
                alen = sv->list->count < sizeof(aad) ? sv->list->count : sizeof(aad);
                for (size_t i = 0; i < alen; i++) { gmb_val_t v = sv->list->items[i]; aad[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            uint8_t* pt = (uint8_t*)malloc(clen ? clen : 1);
            if (!pt) { bout->is_num = 0; bout->result_ref = NULL; return; }
            int rc = -1;
            if (klen >= 32 && nlen >= 12 && tlen >= 16) rc = gmb_chacha20poly1305_decrypt(key, nonce, aad, alen, ct, clen, tag, pt);
            if (rc != 0) {
                /* fallimento/input invalido: lista vuota (parità IL Array.Empty<byte>) */
                free(pt);
                gmb_obj_shared_val_t* empty = gmb_shared_list_new();
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)empty; bout->result_ref_bt = GMB_BT_OBJECT;
                return;
            }
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res)
            {
                for (size_t i = 0; i < clen; i++) gmb_list_add(res->list, gmb_make_val_double(pt[i]));
                free(pt);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(pt); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_362(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t data[8192];
            size_t dlen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                dlen = sv->list->count < sizeof(data) ? sv->list->count : sizeof(data);
                for (size_t i = 0; i < dlen; i++) { gmb_val_t v = sv->list->items[i]; data[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                dlen = s->length < sizeof(data) ? s->length : sizeof(data);
                memcpy(data, s->data, dlen);
            }
            int handle = (int)args[1].num;
            size_t slen = 0;
            uint8_t* sig = gmb_keys_sign(handle, data, dlen, &slen);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res && sig)
            {
                for (size_t i = 0; i < slen; i++) gmb_list_add(res->list, gmb_make_val_double(sig[i]));
                free(sig);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(sig); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_363(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* VERIFY(signature, originalData, publicKeyHandle): args[0]=firma, args[1]=dati. */
            uint8_t sig[8192], data[8192];
            size_t slen = 0, dlen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                slen = sv->list->count < sizeof(sig) ? sv->list->count : sizeof(sig);
                for (size_t i = 0; i < slen; i++) { gmb_val_t v = sv->list->items[i]; sig[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                slen = s->length < sizeof(sig) ? s->length : sizeof(sig);
                memcpy(sig, s->data, slen);
            }
            if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
                dlen = sv->list->count < sizeof(data) ? sv->list->count : sizeof(data);
                for (size_t i = 0; i < dlen; i++) { gmb_val_t v = sv->list->items[i]; data[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[1].type == 2 && args[1].ref)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[1].ref;
                dlen = s->length < sizeof(data) ? s->length : sizeof(data);
                memcpy(data, s->data, dlen);
            }
            int handle = (int)args[2].num;
            bout->result = gmb_keys_verify(handle, data, dlen, sig, slen) ? -1.0 : 0.0;
        }
}
static void gmb_bf_364(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            uint8_t data[8192];
            size_t dlen = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                dlen = sv->list->count < sizeof(data) ? sv->list->count : sizeof(data);
                for (size_t i = 0; i < dlen; i++) { gmb_val_t v = sv->list->items[i]; data[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
            }
            else if (args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                dlen = s->length < sizeof(data) ? s->length : sizeof(data);
                memcpy(data, s->data, dlen);
            }
            int handle = (int)args[1].num;
            size_t olen = 0;
            uint8_t* out = func_code == 364
                ? gmb_keys_encrypt(handle, data, dlen, &olen)
                : gmb_keys_decrypt(handle, data, dlen, &olen);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res && out)
            {
                for (size_t i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                free(out);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_358(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* pem = gmb_keys_export_pem((int)args[0].num);
            gmb_obj_string_t* r = gmb_str_cstr(pem ? pem : "");
            if (pem) free(pem);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_359(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* pem = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            int h = gmb_keys_import_pem(pem);
            bout->result = (double)h;
        }
}
static void gmb_bf_360(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_keys_close((int)args[0].num);
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_366(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int count = (int)args[0].num;
            uint8_t* out = gmb_rand_bytes(count);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res && out)
            {
                for (int i = 0; i < count; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                free(out);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_371(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_rand_int(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_372(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_rand_double(args[0].num, argc >= 2 ? args[1].num : 0);
        }
}
static void gmb_bf_373(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_rand_normal(args[0].num, argc >= 2 ? args[1].num : 1);
        }
}
static void gmb_bf_368(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            size_t olen = 0;
            uint8_t* out = NULL;
            if (func_code == 368) out = gmb_base64_decode(s, &olen);
            else out = gmb_hex_decode(s, &olen);
            gmb_obj_shared_val_t* res = gmb_shared_list_new();
            if (res && out)
            {
                for (size_t i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                free(out);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_367(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const uint8_t* data = NULL;
            size_t len = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    static uint8_t bin[8192];
                    size_t cl = sv->list->count < sizeof(bin) ? sv->list->count : sizeof(bin);
                    for (size_t i = 0; i < cl; i++) { gmb_val_t v = sv->list->items[i]; bin[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0); }
                    data = bin; len = cl;
                }
                else
                {
                    gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                    data = (const uint8_t*)s->data; len = s->length;
                }
            }
            char* out = gmb_base64_encode(data, len);
            gmb_obj_string_t* r = gmb_str_cstr(out ? out : "");
            if (out) free(out);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1404(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const uint8_t* data = NULL; size_t dlen = 0;
    const uint8_t* key = NULL;  size_t klen = 0;
    static uint8_t dbuf[8192], kbuf[8192];
    if (argc >= 1 && args[0].type == 2 && args[0].ref)
    {
        if (args[0].ref->kind == GMB_OBJ_LIST)
        {
            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
            size_t cl = sv->list->count < sizeof(dbuf) ? sv->list->count : sizeof(dbuf);
            for (size_t i = 0; i < cl; i++) { gmb_val_t v = sv->list->items[i]; dbuf[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0); }
            data = dbuf; dlen = cl;
        }
        else { gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref; data = (const uint8_t*)s->data; dlen = s->length; }
    }
    if (argc >= 2 && args[1].type == 2 && args[1].ref)
    {
        if (args[1].ref->kind == GMB_OBJ_LIST)
        {
            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
            size_t cl = sv->list->count < sizeof(kbuf) ? sv->list->count : sizeof(kbuf);
            for (size_t i = 0; i < cl; i++) { gmb_val_t v = sv->list->items[i]; kbuf[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0); }
            key = kbuf; klen = cl;
        }
        else { gmb_obj_string_t* s = (gmb_obj_string_t*)args[1].ref; key = (const uint8_t*)s->data; klen = s->length; }
    }
    uint8_t out[64] = {0};
    size_t outlen = 0;
    if (func_code == 1404) { gmb_hmac_sha256(key, klen, data, dlen, out); outlen = 32; }
    else if (func_code == 1405) { gmb_hmac_sha384(key, klen, data, dlen, out); outlen = 48; }
    else if (func_code == 1406) { gmb_hmac_sha512(key, klen, data, dlen, out); outlen = 64; }
    char hex[130];
    gmb_hex_encode(out, outlen, hex);
    hex[outlen * 2] = '\0';   /* gmb_hex_encode NON termina: senza NUL la stringa è sporca */
    gmb_obj_string_t* r = gmb_str_cstr(hex);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1407(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const uint8_t* data = NULL;
    size_t len = 0;
    if (argc >= 1 && args[0].type == 2 && args[0].ref)
    {
        if (args[0].ref->kind == GMB_OBJ_LIST)
        {
            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
            static uint8_t bin[8192];
            size_t cl = sv->list->count < sizeof(bin) ? sv->list->count : sizeof(bin);
            for (size_t i = 0; i < cl; i++) { gmb_val_t v = sv->list->items[i]; bin[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0); }
            data = bin; len = cl;
        }
        else { gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref; data = (const uint8_t*)s->data; len = s->length; }
    }
    char* out = gmb_base64url_encode(data, len);
    gmb_obj_string_t* r = gmb_str_cstr(out ? out : "");
    if (out) free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1408(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    size_t olen = 0;
    uint8_t* out = gmb_base64url_decode(s, &olen);
    gmb_obj_shared_val_t* res = gmb_shared_list_new();
    if (res && out)
    {
        for (size_t i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
        free(out);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
    }
    else { free(out); bout->is_num = 0; bout->result_ref = NULL; }
}
static void gmb_bf_1415(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    char* out = gmb_upload_safe_name(name);
    gmb_obj_string_t* r = gmb_str_cstr(out ? out : "");
    if (out) free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1416(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const uint8_t* data = NULL;
    size_t len = 0;
    if (argc >= 1 && args[0].type == 2 && args[0].ref)
    {
        if (args[0].ref->kind == GMB_OBJ_LIST)
        {
            gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
            static uint8_t bin[8192];
            size_t cl = sv->list->count < sizeof(bin) ? sv->list->count : sizeof(bin);
            for (size_t i = 0; i < cl; i++) { gmb_val_t v = sv->list->items[i]; bin[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0); }
            data = bin; len = cl;
        }
        else { gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref; data = (const uint8_t*)s->data; len = s->length; }
    }
    const char* type = gmb_upload_detect_type(data, len);
    gmb_obj_string_t* r = gmb_str_cstr(type ? type : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1417(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* user = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* role = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    char* cookie = gmb_session_create(user, role);
    gmb_obj_string_t* r = gmb_str_cstr(cookie ? cookie : "");
    if (cookie) free(cookie);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1418(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    int ok = func_code == 1418 ? gmb_validation_required(s)
           : func_code == 1419 ? gmb_validation_email(s)
           : func_code == 1420 ? gmb_validation_url(s)
           : gmb_validation_credit_card(s);
    bout->is_num = 1; bout->result = ok ? -1.0 : 0.0;
}
static void gmb_bf_1422(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    int minv = argc >= 2 ? (int)args[1].num : 0;
    int maxv = argc >= 3 ? (int)args[2].num : -1;
    bout->is_num = 1;
    bout->result = gmb_validation_length(s, minv, maxv) ? -1.0 : 0.0;
}
static void gmb_bf_1409(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    char* out = func_code == 1409 ? gmb_html_encode(s)
              : func_code == 1410 ? gmb_url_encode(s)
              : gmb_js_encode(s);
    gmb_obj_string_t* r = gmb_str_cstr(out ? out : "");
    if (out) free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_563(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* url = (argc >= 1 && args[0].type == 2 && args[0].ref)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";

            
            const char* method = NULL;
            const char* req_body = NULL;
            const char** hnames = NULL;
            const char** hvals = NULL;
            int hcount = 0;
            long timeout_ms = 0;
            gmb_net_http_options_t opts;
            memset(&opts, 0, sizeof(opts));
            gmb_dict_t* od = NULL;
            if (argc >= 2 && args[1].type == 2 && args[1].ref &&
                args[1].ref->kind == GMB_OBJ_DICT && ((gmb_obj_shared_val_t*)args[1].ref)->dict)
            {
                od = ((gmb_obj_shared_val_t*)args[1].ref)->dict;
                gmb_val_t mv = gmb_dict_get(*od, "Method");
                if (mv.kind == GMB_TYPE_STRING && mv.v_string.data && mv.v_string.data[0]) method = mv.v_string.data;
                gmb_val_t bv = gmb_dict_get(*od, "Body");
                if (bv.kind == GMB_TYPE_STRING && bv.v_string.data) req_body = bv.v_string.data;
                gmb_val_t tv = gmb_dict_get(*od, "Timeout");
                if (tv.kind == GMB_TYPE_DOUBLE && tv.v_double > 0) timeout_ms = (long)tv.v_double;
                else if (tv.kind == GMB_TYPE_INT && tv.v_int > 0) timeout_ms = (long)tv.v_int;
                gmb_val_t hv = gmb_dict_get(*od, "Headers");
                gmb_dict_t* hd = NULL;
                if (hv.kind == GMB_TYPE_OBJECT && hv.v_ptr &&
                    ((gmb_obj_t*)hv.v_ptr)->kind == GMB_OBJ_DICT)
                    hd = ((gmb_obj_shared_val_t*)hv.v_ptr)->dict;
                if (hd && hd->count > 0 && hd->count <= 64)
                {
                    hnames = (const char**)malloc(hd->count * sizeof(char*));
                    hvals = (const char**)malloc(hd->count * sizeof(char*));
                    if (hnames && hvals)
                        for (size_t hi = 0; hi < hd->count; hi++)
                        {
                            hnames[hcount] = hd->items[hi].key;
                            hvals[hcount] = (hd->items[hi].value.kind == GMB_TYPE_STRING &&
                                             hd->items[hi].value.v_string.data)
                                ? hd->items[hi].value.v_string.data : "";
                            hcount++;
                        }
                    else { free(hnames); free(hvals); hnames = NULL; hvals = NULL; hcount = 0; }
                }
            }
            opts.method = method;
            opts.body = req_body;
            opts.body_len = req_body ? strlen(req_body) : 0;
            opts.header_names = hnames;
            opts.header_values = hvals;
            opts.header_count = hcount;
            opts.timeout_ms = timeout_ms;
            opts.allow_redirects = 1;

            int status = 0;
            char* rbody = NULL; size_t rbody_len = 0;
            char* rerr = NULL; char* rheaders = NULL;
            gmb_net_http_request(url, &opts, &status, &rbody, &rbody_len, &rerr, &rheaders);
            free(hnames); free(hvals);

            gmb_obj_shared_val_t* res = gmb_shared_dict_new();
            if (res)
            {
                gmb_dict_set(res->dict, "StatusCode", gmb_make_val_double(status));
                char* body_z = (char*)malloc(rbody_len + 1);
                if (body_z)
                {
                    if (rbody_len) memcpy(body_z, rbody, rbody_len);
                    body_z[rbody_len] = '\0';
                    gmb_dict_set(res->dict, "Body", gmb_make_val_string_counted(body_z));
                    free(body_z);
                }
                gmb_dict_set(res->dict, "Url", gmb_make_val_string_counted(url));
                
                gmb_obj_shared_val_t* hh = gmb_shared_dict_new();
                if (hh)
                {
                    if (rheaders)
                    {
                        char* hc = strdup(rheaders);
                        if (hc)
                        {
                            char* save = NULL;
                            for (char* line = strtok_r(hc, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
                            {
                                char* col = strchr(line, ':');
                                if (!col) continue;
                                *col = '\0';
                                char* v = col + 1;
                                while (*v == ' ') v++;
                                size_t vl = strlen(v);
                                while (vl > 0 && (v[vl - 1] == '\r' || v[vl - 1] == ' ')) v[--vl] = '\0';
                                gmb_dict_set(hh->dict, line, gmb_make_val_string_counted(v));
                            }
                            free(hc);
                        }
                    }
                    gmb_dict_set(res->dict, "Headers", gmb_make_val_obj((gmb_obj_t*)hh));
                }
            }
            if (rbody) free(rbody);
            if (rerr) free(rerr);
            if (rheaders) free(rheaders);
            if (res) { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT; }
            else { bout->is_num = 0; bout->result_ref = NULL; bout->result_ref_bt = GMB_BT_NULL; }
        }
}
static void gmb_bf_608(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int rows = (int)(argc >= 1 ? args[0].num : 4);
            int cols = (int)(argc >= 2 ? args[1].num : rows);
            /* MATRIX.CREATE a VALORE (F1): lista FLAT row-major con ndims/dim_upper,
             * identità sui primi min(rows,cols). Niente handle nel registry. */
            gmb_obj_shared_val_t* arr = gmb_shared_list_new();
            if (!arr || rows <= 0 || cols <= 0) { bout->is_num = 0; bout->result_ref = NULL; return; }
            arr->is_array = 1; arr->ndims = 2;
            arr->dim_upper[0] = rows - 1; arr->dim_upper[1] = cols - 1;
            gmb_list_resize(arr->list, (size_t)rows * (size_t)cols, 0);
            for (int i = 0; i < rows * cols; i++) arr->list->items[i] = gmb_make_val_double(0.0);
            int d = rows < cols ? rows : cols;
            for (int i = 0; i < d; i++) arr->list->items[(size_t)i * (size_t)cols + (size_t)i] = gmb_make_val_double(1.0);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)arr; bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
static void gmb_bf_610(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = gmb_matrix_transpose(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
            vm_matrix_set_value_result(h, bout);
            gmb_matrix_free(h);
        }
}
static void gmb_bf_611(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = gmb_matrix_inverse(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
            bout->result = (double)h;
        }
}
static void gmb_bf_612(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task;
            int64_t h = gmb_matrix_multiply(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL),
                                            vm_matrix_handle_arg(argc >= 2 ? &args[1] : NULL));
            /* 612 = LINALGMULTIPLYFUNC, 1269 = MATRIX.MULTIPLY (F4): restituiscono un
             * VALORE matrice rank-2. Gli altri eventuali codici restano handle. */
            if (func_code == 612 || func_code == 1269) { vm_matrix_set_value_result(h, bout); gmb_matrix_free(h); }
            else bout->result = (double)h;
        }
}
static void gmb_bf_609(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t h = gmb_vector_create(argc >= 1 ? args[0].num : 0,
                                          argc >= 2 ? args[1].num : 0,
                                          argc >= 3 ? args[2].num : 0,
                                          argc >= 4 ? args[3].num : 0);
            bout->result = (double)h;
        }
}
static void gmb_bf_268(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                double s = 0;
                if (sv->list) for (int i = 0; i < (int)sv->list->count; i++) { gmb_val_t v = sv->list->items[i]; double d = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; s += d*d; }
                bout->result = sqrt(s);
            }
            else bout->result = gmb_vector_norm((int64_t)(argc >= 1 ? args[0].num : -1));
        }
}
static void gmb_bf_269(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST &&
                args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sa = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_obj_shared_val_t* sb = (gmb_obj_shared_val_t*)args[1].ref;
                double s = 0;
                if (sa->list && sb->list)
                {
                    int n = sa->list->count < sb->list->count ? (int)sa->list->count : (int)sb->list->count;
                    for (int i = 0; i < n; i++)
                    {
                        gmb_val_t va = sa->list->items[i], vb = sb->list->items[i];
                        double da = va.kind == GMB_TYPE_DOUBLE ? va.v_double : va.kind == GMB_TYPE_INT ? (double)va.v_int : 0;
                        double db = vb.kind == GMB_TYPE_DOUBLE ? vb.v_double : vb.kind == GMB_TYPE_INT ? (double)vb.v_int : 0;
                        s += da * db;
                    }
                }
                bout->result = s;
            }
            else bout->result = gmb_vector_dot((int64_t)(argc >= 1 ? args[0].num : -1), (int64_t)(argc >= 2 ? args[1].num : -1));
        }
}
static void gmb_bf_616_c616(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t src = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    bout->result = (double)gmb_matrix_evd(src);
}
static void gmb_bf_616_c613(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t src = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    bout->result = (double)gmb_matrix_lu(src);
}
static void gmb_bf_616_c614(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t src = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    bout->result = (double)gmb_matrix_qr(src);
}
static void gmb_bf_616_c615(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t src = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    bout->result = (double)gmb_matrix_svd(src);
}
static void gmb_bf_616_c617(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t src = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    bout->result = (double)gmb_matrix_cholesky(src);
}
static void gmb_bf_618(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t d = (int64_t)(argc >= 1 ? args[0].num : -1);
            if (argc >= 2 && args[1].type == 2 && args[1].basic_type == GMB_BT_STRING)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[1].ref;
                char nm[64];
                size_t n = s->length < 63 ? s->length : 63;
                memcpy(nm, s->data, n);
                nm[n] = '\0';
                bout->result = (double)gmb_matrix_decomp_get_factor_named(d, nm);
            }
            else
            {
                int idx = (int)(argc >= 2 ? args[1].num : 0);
                bout->result = (double)gmb_matrix_decomp_get_factor(d, idx);
            }
        }
}
static void gmb_bf_6(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 1) bout->result = args[0].num;
            else if (args[0].type == 2 && args[0].ref)
            {
                const char* s = ((gmb_obj_string_t*)args[0].ref)->data;
                int ok = 0;
                bout->result = gmb_parse_num(s ? s : "", &ok);
            }
            else bout->result = 0;
        }
}
static void gmb_bf_46(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_clamp(args[0].num, argc >= 2 ? args[1].num : 0, argc >= 3 ? args[2].num : 0);
        }
}
static void gmb_bf_40(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = tanh(vm_num(&args[0]));
        }
}
static void gmb_bf_41(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = args[0].num * 180.0 / M_PI;
        }
}
static void gmb_bf_42(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = args[0].num * M_PI / 180.0;
        }
}
static void gmb_bf_43(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = M_PI;
        }
}
static void gmb_bf_44(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double best = 0;
            int n = 0;
            double* data = NULL;
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                
                data = gmb_vm_flatten_dbl(&args[0], &n);
            }
            else
            {
                n = argc;
                data = (double*)malloc(sizeof(double) * (size_t)(n ? n : 1));
                if (data) for (int i = 0; i < n; i++) data[i] = args[i].num;
            }
            if (data && n > 0) best = func_code == 44 ? gmb_min_arr(data, n) : gmb_max_arr(data, n);
            free(data);
            bout->result = best;
        }
}
static void gmb_bf_47(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = hypot(args[0].num, argc > 1 ? args[1].num : 0);
        }
}
static void gmb_bf_48(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist2d(args[0].num, args[1].num, argc > 2 ? args[2].num : 0, argc > 3 ? args[3].num : 0);
        }
}
static void gmb_bf_49(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dist3d(args[0].num, args[1].num, args[2].num,
                                argc > 3 ? args[3].num : 0, argc > 4 ? args[4].num : 0, argc > 5 ? args[5].num : 0);
        }
}
static void gmb_bf_80(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double* data = NULL;
            int cnt = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                
                data = gmb_vm_flatten_dbl(&args[0], &cnt);
            }
            else
            {
                
                cnt = argc;
                if (cnt > 0)
                {
                    data = (double*)malloc(sizeof(double) * (size_t)cnt);
                    if (data) for (int i = 0; i < cnt; i++) data[i] = args[i].num;
                }
            }
            if (!data || cnt == 0) { free(data); bout->result = 0; }
            if (func_code == 80) bout->result = gmb_mean(data, cnt);
            else if (func_code == 81) bout->result = gmb_median(data, cnt);
            else if (func_code == 82) bout->result = gmb_variance(data, cnt);
            else if (func_code == 83) bout->result = gmb_stddev(data, cnt);
            else if (func_code == 84) bout->result = gmb_skewness(data, cnt);
            else if (func_code == 85) bout->result = gmb_kurtosis(data, cnt);
            else if (func_code == 88) bout->result = gmb_mad(data, cnt);
            free(data);
        }
}
void gmb_bf_583(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* text = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = (double)gmb_gfx_text_width(text);
        }
}
void gmb_bf_584(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_gfx_text_height();
        }
}
static void gmb_bf_587(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* CELLSTR(row,col): headless → "" (parità IL IlRuntime.CellStr), nessun errore. */
            gmb_obj_string_t* s = gmb_str_cstr("");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_349(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* algo = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "SHA256";
            char outhex[160] = "";
            FILE* f = fopen(path, "rb");
            if (f)
            {
                uint8_t h[64];
                size_t total = 0;
                uint8_t* all = (uint8_t*)malloc(4 * 1024 * 1024);
                if (all)
                {
                    total = fread(all, 1, 4 * 1024 * 1024, f);
                    if (strcasecmp(algo, "MD5") == 0) gmb_md5(all, total, h);
                    else if (strcasecmp(algo, "SHA1") == 0) gmb_sha1(all, total, h);
                    else gmb_sha256(all, total, h);
                    free(all);
                }
                fclose(f);
                size_t hlen = strcasecmp(algo, "MD5") == 0 ? 16 : strcasecmp(algo, "SHA1") == 0 ? 20 : 32;
                gmb_hex_encode(h, hlen, outhex);
            }
            gmb_obj_string_t* r = gmb_str_cstr(outhex);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_109(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_ts_clear_error();
            double* data = NULL;
            int cnt = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                cnt = (int)sv->list->count;
                data = (double*)malloc(sizeof(double) * (size_t)(cnt ? cnt : 1));
                if (data)
                {
                    for (int i = 0; i < cnt; i++)
                    {
                        gmb_val_t v = sv->list->items[i];
                        data[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0;
                    }
                }
            }
            else if (argc >= 1) { cnt = 1; data = (double*)malloc(sizeof(double)); if (data) data[0] = args[0].num; }
            if (!data) { bout->result = 0; }

            if (func_code == 122) { bout->result = gmb_ts_volatility(data, cnt, 0); free(data); data = NULL; }
            if (func_code == 123) { bout->result = gmb_ts_autocorrelation(data, cnt, argc >= 2 ? (int)args[1].num : 1); free(data); data = NULL; }
            if (func_code == 122 || func_code == 123)
            {
                bout->is_num = 1;
                { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
                return;
            }

            int olen = 0;
            double* out = NULL;
            if (func_code == 109) out = gmb_ts_moving_average(data, cnt, argc >= 2 ? (int)args[1].num : 0, &olen);
            else if (func_code == 110)
            {
                /* TSEMA espone il PERIODO (Long); il runtime vuole ALPHA = 2/(period+1),
                 * come StdLib.Runtime.TimeSeriesFunctions.Ema e il backend C99Native.
                 * Prima si passava il periodo come alpha → "Alpha deve essere in (0,1]"
                 * (gen_TIMESERIES_TSEMA sulla gamba VM/C). */
                double period = argc >= 2 ? args[1].num : 0.0;
                out = gmb_ts_ema(data, cnt, period > 0.0 ? 2.0 / (period + 1.0) : period, &olen);
            }
            else if (func_code == 111) out = gmb_ts_rolling_stddev(data, cnt, argc >= 2 ? (int)args[1].num : 0, &olen);
            else if (func_code == 113) out = gmb_ts_rolling_zscore(data, cnt, argc >= 2 ? (int)args[1].num : 0, &olen);
            else if (func_code == 118) out = gmb_ts_rolldelta(data, cnt, argc >= 2 ? (int)args[1].num : 0, &olen);
            else if (func_code == 120) out = gmb_ts_difference(data, cnt, argc >= 2 ? (int)args[1].num : 1, &olen);
            else if (func_code == 121) out = gmb_ts_returns(data, cnt, 0, &olen);
            else if (func_code == 125) out = gmb_ts_deseasonalize(data, cnt, argc >= 2 ? (int)args[1].num : 0, &olen);
            free(data);
            if (out)
            {
                gmb_obj_shared_val_t* res = gmb_shared_list_new();
                if (res)
                {
                    for (int i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                free(out);
            }
            else if (!(gmb_ts_last_error() && *gmb_ts_last_error()))
            {
                /* Array vuoto (es. TSReturns su serie troppo corta): il C# ritorna [] (non
                 * un errore) — produce una LISTA vuota, non NULL (parità PCode "[]"). */
                gmb_obj_shared_val_t* res = gmb_shared_list_new();
                if (res) { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
        }
}
static void gmb_bf_115(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_ts_clear_error();
            double* x = NULL, * y = NULL;
            int xn = 0, yn = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                xn = (int)sv->list->count;
                x = (double*)malloc(sizeof(double) * (size_t)(xn ? xn : 1));
                if (x) for (int i = 0; i < xn; i++) { gmb_val_t v = sv->list->items[i]; x[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            }
            if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
                yn = (int)sv->list->count;
                y = (double*)malloc(sizeof(double) * (size_t)(yn ? yn : 1));
                if (y) for (int i = 0; i < yn; i++) { gmb_val_t v = sv->list->items[i]; y[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            }
            if (!x || !y || xn != yn) { free(x); free(y); bout->is_num = 0; bout->result_ref = NULL; }
            int olen = 0;
            double* out = func_code == 115
                ? gmb_ts_rolling_cov(x, y, xn, argc >= 3 ? (int)args[2].num : 0, &olen)
                : gmb_ts_rolling_corr(x, y, xn, argc >= 3 ? (int)args[2].num : 0, &olen);
            free(x); free(y);
            if (out)
            {
                gmb_obj_shared_val_t* res = gmb_shared_list_new();
                if (res)
                {
                    for (int i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                free(out);
            }
            else if (!(gmb_ts_last_error() && *gmb_ts_last_error()))
            {
                /* Risultato vuoto ma NON errore (parità PCode "[]"): lista vuota, non NULL. */
                gmb_obj_shared_val_t* res = gmb_shared_list_new();
                if (res) { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
        }
}
static void gmb_bf_119(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double* data = NULL;
            int cnt = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
            gmb_ts_clear_error();
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                cnt = (int)sv->list->count;
                data = (double*)malloc(sizeof(double) * (size_t)(cnt ? cnt : 1));
                if (data) for (int i = 0; i < cnt; i++) { gmb_val_t v = sv->list->items[i]; data[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0; }
            }
            if (!data) { bout->is_num = 0; bout->result_ref = NULL; }
            int olen = 0;
            double* out = gmb_ts_outliers(data, cnt, argc >= 2 ? args[1].num : 3.0, &olen);
            free(data);
            if (out)
            {
                gmb_obj_shared_val_t* res = gmb_shared_list_new();
                if (res)
                {
                    for (int i = 0; i < olen; i++) gmb_list_add(res->list, gmb_make_val_double(out[i]));
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)res; bout->result_ref_bt = GMB_BT_OBJECT;
                    free(out);
                    return;
                }
            }
            free(out);
            bout->is_num = 0; bout->result_ref = NULL;
        }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_112(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ts_clear_error();
        {
            int cnt = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cnt);
            int olen = 0;
            double* out = gmb_ts_rolling_mad(data, cnt, argc >= 2 ? (int)args[1].num : 3, &olen);
            free(data);
            gmb_vm_set_result_list(out, olen, &bout->is_num, &bout->result_ref);   /* consuma out */
        }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_114(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ts_clear_error();
        {
            int cnt = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cnt);
            int olen = 0;
            double* out = gmb_ts_rolling_autocorr(data, cnt, argc >= 2 ? (int)args[1].num : 3, argc >= 3 ? (int)args[2].num : 1, &olen);
            free(data);
            gmb_vm_set_result_list(out, olen, &bout->is_num, &bout->result_ref);   /* consuma out */
        }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
/* Costruisce una MATRICE BASIC (lista di righe-lista) da un flat double[] row-major con
 * `cols` colonne e la restituisce via result_ref (parità oracolo: jagged array).
 * Consuma `r` (free). */
static void gmb_vm_set_result_matrix(double* r, int n, int cols, int* is_num, gmb_obj_t** result_ref)
{
    *is_num = 0;
    if (!r || cols <= 0) { free(r); *result_ref = NULL; return; }
    int rows = n / cols;
    gmb_obj_shared_val_t* m = gmb_shared_list_new();
    if (m) {
        for (int i = 0; i < rows; i++) {
            gmb_obj_shared_val_t* row = gmb_shared_list_new();
            if (!row) continue;
            for (int j = 0; j < cols; j++) gmb_list_add(row->list, gmb_make_val_double(r[i * cols + j]));
            gmb_list_add(m->list, gmb_make_val_obj((gmb_obj_t*)row));
        }
    }
    free(r);
    *result_ref = (gmb_obj_t*)m;
}
static void gmb_bf_117(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ts_clear_error();
        {
            (void)vm; (void)task; (void)func_code;
            double* x = NULL, * y = NULL; int xn = 0, yn = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            { gmb_obj_shared_val_t* sv=(gmb_obj_shared_val_t*)args[0].ref; xn=(int)sv->list->count; x=(double*)malloc(sizeof(double)*(size_t)(xn?xn:1)); if(x) for(int i=0;i<xn;i++){gmb_val_t v=sv->list->items[i];x[i]=v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;} }
            if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            { gmb_obj_shared_val_t* sv=(gmb_obj_shared_val_t*)args[1].ref; yn=(int)sv->list->count; y=(double*)malloc(sizeof(double)*(size_t)(yn?yn:1)); if(y) for(int i=0;i<yn;i++){gmb_val_t v=sv->list->items[i];y[i]=v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;} }
            if (!x || !y) { free(x); free(y); bout->is_num = 0; bout->result_ref = NULL; return; }
            int olen = 0;
            double* out = gmb_ts_rolling_regression(x, y, xn < yn ? xn : yn, argc >= 3 ? (int)args[2].num : 3, &olen);
            free(x); free(y);
            gmb_vm_set_result_matrix(out, olen, 2, &bout->is_num, &bout->result_ref);
        }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_126(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ts_clear_error();
        {
            (void)vm; (void)task; (void)func_code;
            double* data = NULL; int cnt = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            { gmb_obj_shared_val_t* sv=(gmb_obj_shared_val_t*)args[0].ref; cnt=(int)sv->list->count; data=(double*)malloc(sizeof(double)*(size_t)(cnt?cnt:1)); if(data) for(int i=0;i<cnt;i++){gmb_val_t v=sv->list->items[i];data[i]=v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;} }
            int period = argc >= 2 ? (int)args[1].num : 1;
            int olen = 0;
            double* out = gmb_ts_decompose_matrix(data, cnt, period, &olen);
            free(data);
            /* 3 componenti (trend/seasonal/residual) ciascuna di n = olen/3 → matrice 3×n
             * (parità oracolo IlRuntime.Decompose → double[3][n]). */
            if (!out || olen <= 0) { free(out); bout->is_num = 0; bout->result_ref = NULL; return; }
            gmb_vm_set_result_matrix(out, olen, olen / 3, &bout->is_num, &bout->result_ref);
        }
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_305(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                int n = (int)args[1].num;
                const char* sep = (argc >= 3 && args[2].type == 2 && args[2].ref) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
                gmb_string_t inp = { s->data, s->length, 1 };
                gmb_string_t sep_s = { (char*)sep, sep ? strlen(sep) : 0, 1 };
                gmb_string_t gs = gmb_str_repeat(inp, n, sep_s);
                if (gs.data) { gmb_obj_string_t* r = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_319(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            
            double start = 0, end = 0, step = 1;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                /* unwrap ParamArray annidato a profondità variabile (parity IlRuntime.Range) */
                while (sv && sv->list && sv->list->count == 1
                       && sv->list->items[0].kind == GMB_TYPE_OBJECT && sv->list->items[0].v_ptr
                       && ((gmb_obj_t*)sv->list->items[0].v_ptr)->kind == GMB_OBJ_LIST)
                    sv = (gmb_obj_shared_val_t*)sv->list->items[0].v_ptr;
                double vals[8]; int cnt = 0;
                for (size_t i = 0; i < sv->list->count && cnt < 8; i++)
                {
                    gmb_val_t v = sv->list->items[i];
                    vals[cnt++] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0;
                }
                start = cnt > 0 ? vals[0] : 0;
                end = cnt > 1 ? vals[1] : start;
                step = cnt > 2 ? vals[2] : 1;
            }
            else
            {
                start = args[0].num; end = argc > 1 ? args[1].num : start; step = argc > 2 ? args[2].num : 1;
            }
            if (step == 0) step = 1;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                if (step > 0)
                    for (double v = start; v <= end; v += step) gmb_list_add(out->list, gmb_make_val_double(v));
                else
                    for (double v = start; v >= end; v += step) gmb_list_add(out->list, gmb_make_val_double(v));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_369(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                const uint8_t* data = NULL;
                size_t len = 0;
                static uint8_t bin[8192];
                if (args[0].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                    size_t cl = sv->list->count < sizeof(bin) ? sv->list->count : sizeof(bin);
                    for (size_t i = 0; i < cl; i++) { gmb_val_t v = sv->list->items[i]; bin[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0); }
                    data = bin; len = cl;
                }
                else if (args[0].ref->type_name && strcmp(args[0].ref->type_name, "gmb_bytes") == 0)
                {
                    /* bytes boxed (gmb_make_val_bytes, es. BLOCKCHAIN.DERIVEKEY): layout
                     * [len][bytes...], NON una gmb_obj_string_t. Estrai con gmb_val_as_bytes
                     * (parità IL/C99Native) invece di leggere s->data/s->length (garbage). */
                    gmb_val_t bv; bv.kind = GMB_TYPE_OBJECT; bv.v_ptr = args[0].ref;
                    size_t blen = 0;
                    uint8_t* bd = gmb_val_as_bytes(bv, &blen);
                    if (bd)
                    {
                        size_t cl = blen < sizeof(bin) ? blen : sizeof(bin);
                        memcpy(bin, bd, cl);
                        data = bin; len = cl;
                        free(bd);
                    }
                }
                else
                {
                    gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                    data = (const uint8_t*)s->data; len = s->length;
                }
                char* h = gmb_hex_encode_string(data, len);
                gmb_obj_string_t* r = gmb_str_cstr(h ? h : "");
                if (h) free(h);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                return;   /* fix: senza return si cadeva nel path numerico che sovrascriveva il risultato con hex(args[0].num) -> "0" */
            }
            gmb_string_t gs = gmb_num_to_hex(args[0].num);
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else bout->is_num = 0;
        }
}
static void gmb_bf_572(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : ".";
            
            /* DIR: usa l'UNICA implementazione del runtime condiviso (§0bis), la stessa
             * che C99/C99Native emettono (gmb_fs_dir), che gestisce la maschera completa
             * (cartella + pattern). Prima il VMC faceva da sé il parsing della maschera:
             * con un percorso SENZA '*' usava il FILE come directory (opendir falliva →
             * lista vuota) e con '*' prendeva tutto ciò che precede l'asterisco come
             * directory (sbagliato). Trovato da MiniTests/io/io_arrays.bas: IO.DIR di un
             * nome esatto dava IL/PCode 1 e VMC 0. */
            gmb_string_t mask = { (char*)path, strlen(path), 1 };
            gmb_list_t found = gmb_fs_dir(mask);
            gmb_obj_shared_val_t* lst = gmb_shared_list_new();
            if (lst)
            {
                for (size_t i = 0; i < found.count; i++)
                {
                    gmb_string_t n = gmb_val_to_string(found.items[i]);
                    if (n.data) gmb_list_add(lst->list, gmb_make_val_string_counted(n.data));
                }
            }
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)lst; bout->result_ref_bt = GMB_OBJ_LIST;
        }
}
static void gmb_bf_648(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* tn = "UNKNOWN";
            char tnbuf[128]; tnbuf[0] = 0;
            if (args[0].type == 1)
                tn = args[0].basic_type == GMB_BT_BOOL ? "BIT"
                   : args[0].basic_type == GMB_BT_CURRENCY ? "CURRENCY" : "DOUBLE";
            else if (args[0].type == 2 && args[0].ref)
            {
                switch (args[0].ref->kind)
                {
                    case GMB_OBJ_STRING: tn = "STRING"; break;
                    /* Nomi CLR delle collezioni (parità IlRuntime.TypeName/IL/golden C99:
                     * `Dictionary<,>` → "DICTIONARY`2", `List<>` → "LIST`1"). */
                    /* Array nativo (DIM A(...) → CREATE_ARRAY, ndims>=1) → "ARRAY"; lista
                     * semplice → "LIST`1" (parità IlRuntime.TypeName/IL/golden C99). */
                    case GMB_OBJ_LIST: {
                        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                        tn = (sv->is_array || sv->ndims >= 1) ? "ARRAY" : "LIST`1";
                        break;
                    }
                    /* Collezioni generiche (QUEUE/STACK/SET/SORTED/TREE/AVL/...): nome
                     * canonico dal typeCode (fonte unica gmb_collection_type_name). */
                    case GMB_OBJ_COLLECTION:
                        tn = gmb_collection_type_name(((gmb_obj_collection_t*)args[0].ref)->type_code);
                        break;
                    case GMB_OBJ_GRAPH: tn = "GRAPH`1"; break;
                    case GMB_OBJ_DICT:
                    {
                        /* Istanza UDT/CLASS: `__class__.Name` → nome del TYPE (parità
                         * IlRuntime.TypeName / PCode). Dict nudo → "DICTIONARY`2". */
                        tn = "DICTIONARY`2";
                        gmb_obj_shared_val_t* dd = (gmb_obj_shared_val_t*)args[0].ref;
                        if (dd->dict) {
                            /* REGEX/wrapper: oggetto con marcatore `__type` (es. "REGEXWRAPPER"),
                             * parità IlRuntime.TypeName/PCode/C99. */
                            gmb_val_t tyv = gmb_dict_get(*dd->dict, "__type");
                            if (tyv.kind == GMB_TYPE_STRING && tyv.v_string.data && tyv.v_string.data[0]) {
                                snprintf(tnbuf, sizeof(tnbuf), "%s", tyv.v_string.data);
                                tn = tnbuf;
                                break;
                            }
                            gmb_val_t cv = gmb_dict_get(*dd->dict, "__class__");
                            if (cv.kind == GMB_TYPE_OBJECT && cv.v_ptr && ((gmb_obj_t*)cv.v_ptr)->kind == GMB_OBJ_DICT) {
                                gmb_obj_shared_val_t* cd = (gmb_obj_shared_val_t*)cv.v_ptr;
                                if (cd->dict) {
                                    gmb_val_t nv = gmb_dict_get(*cd->dict, "Name");
                                    if (nv.kind == GMB_TYPE_STRING && nv.v_string.data && nv.v_string.data[0]) {
                                        snprintf(tnbuf, sizeof(tnbuf), "%s", nv.v_string.data);
                                        tn = tnbuf;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    default: tn = "OBJECT";
                }
            }
            else tn = "NIL";
            gmb_obj_string_t* s = gmb_str_cstr(tn);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
/* ── JSON "documento impostazioni" in-memory (JSON WRITEVALUE/WRITESECTION e
 * ReadValue/ReadSection): default "app.settings.json" (modello legacy). Stato
 * globale del documento (single-program), mai ponte verso altri mondi. */
static gmb_obj_shared_val_t* g_vm_json_doc = NULL;

static gmb_obj_shared_val_t* vm_json_doc(void)
{
    if (!g_vm_json_doc) g_vm_json_doc = gmb_shared_dict_new();
    return g_vm_json_doc;
}

static void vm_json_copy_dict(gmb_obj_shared_val_t* dst, gmb_dict_t* src)
{
    for (size_t i = 0; i < src->count; i++) {
        gmb_val_t v = src->items[i].value;
        if (v.kind == GMB_TYPE_STRING)
            gmb_dict_set(dst->dict, src->items[i].key, gmb_make_val_string_counted(v.v_string.data ? v.v_string.data : ""));
        else if (v.kind == GMB_TYPE_OBJECT && v.v_ptr) gmb_dict_set(dst->dict, src->items[i].key, v);   /* set trattiene da se' */
        else gmb_dict_set(dst->dict, src->items[i].key, v);
    }
}

static void gmb_bf_3432(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* RANGE(start [, end [, step]]) → lista di numeri (OBJ_LIST). Se un solo arg è
     * "end" (0..n-1) come in Python? Qui seguiamo la semantica GMBasic (inclusiva):
     * RANGE(a,b) → a..b con passo 1, RANGE(a,b,s) passo s. */
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 0; bout->result_ref = NULL;
    double a = argc >= 1 && args[0].type == 1 ? args[0].num : 0;
    double b = argc >= 2 && args[1].type == 1 ? args[1].num : 0;
    double st = argc >= 3 && args[2].type == 1 ? args[2].num : 1.0;
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (!out) return;
    if (st == 0) st = 1.0;
    if (argc == 1) { double v = 0; for (; v < a; v += 1) gmb_list_add(out->list, gmb_make_val_double(v)); }
    else if (st > 0) { for (double v = a; v <= b; v += st) gmb_list_add(out->list, gmb_make_val_double(v)); }
    else { for (double v = a; v >= b; v += st) gmb_list_add(out->list, gmb_make_val_double(v)); }
    bout->result_ref = (gmb_obj_t*)out;
    bout->result_ref_bt = GMB_BT_OBJECT;
}

static void gmb_bf_3431(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* CSTR(x): conversione a stringa (numeri, stringhe, oggetti→Stringify). */
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 0; bout->result_ref = NULL;
    if (argc >= 1)
    {
        char buf[64];
        gmb_obj_string_t* out = NULL;
        if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        {
            out = gmb_str_cstr(((gmb_obj_string_t*)args[0].ref)->data);
        }
        else if (args[0].type == 1)
        {
            snprintf(buf, sizeof(buf), "%.15g", args[0].num);
            out = gmb_str_cstr(buf);
        }
        else if (args[0].type == 2 && args[0].ref)
        {
            gmb_obj_string_t* js = gmb_vm_json_stringify(args[0].ref);
            if (js) out = js; else out = gmb_str_cstr("");
        }
        if (!out) out = gmb_str_cstr("");
        bout->result_ref = (gmb_obj_t*)out;
        bout->result_ref_bt = GMB_BT_STRING;
    }
    else { gmb_obj_string_t* z = gmb_str_cstr(""); bout->result_ref = (gmb_obj_t*)z; bout->result_ref_bt = GMB_BT_STRING; }
}

static void gmb_bf_3420(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* DIRECTCAST(x, T): identità del valore (cast a runtime = no-op sui tipi dinamici VM). */
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 0; bout->result_ref = NULL; bout->result = 0;
    if (argc >= 1)
    {
        if (args[0].type == 2 && args[0].ref)
        {
            gmb_obj_retain(args[0].ref);
            bout->result_ref = args[0].ref;
            bout->result_ref_bt = args[0].basic_type;
        }
        else if (args[0].type == 1) { bout->is_num = 1; bout->result = args[0].num; }
    }
}

static void gmb_bf_3410(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* STRINGBUILDER: dict con campo interno __str (builder mutabile). */
    gmb_obj_shared_val_t* b = gmb_shared_dict_new();
    bout->is_num = 0; bout->result_ref = NULL;
    if (b)
    {
        gmb_dict_set(b->dict, "__str", gmb_make_val_string_counted(""));
        bout->result_ref = (gmb_obj_t*)b;
        bout->result_ref_bt = GMB_BT_OBJECT;
    }
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
}

static void gmb_bf_3400(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    gmb_obj_shared_val_t* doc = vm_json_doc();
    if (!doc) return;
    (void)vm; (void)func_code; (void)task;
    if (argc >= 2)
    {
        const char* k = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        gmb_val_t v;
        if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
            v = gmb_make_val_string(((gmb_obj_string_t*)args[1].ref)->data);
        else if (args[1].type == 1) v = gmb_make_val_double(args[1].num);
        else v = gmb_val_nil();
        gmb_dict_set(doc->dict, k, v);
    }
}

static void gmb_bf_3401(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    gmb_obj_shared_val_t* doc = vm_json_doc();
    if (!doc) return;
    (void)vm; (void)func_code; (void)task;
    if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_DICT)
    {
        const char* name = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        gmb_obj_shared_val_t* sec = gmb_shared_dict_new();
        if (sec)
        {
            gmb_obj_shared_val_t* src = (gmb_obj_shared_val_t*)args[1].ref;
            vm_json_copy_dict(sec, src->dict);
            gmb_obj_retain((gmb_obj_t*)sec);
            gmb_dict_set(doc->dict, name, gmb_make_val_obj((gmb_obj_t*)sec));
        }
    }
}

static void gmb_bf_3402(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    gmb_obj_shared_val_t* doc = vm_json_doc();
    if (!doc) return;
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 0; bout->result_ref = NULL; bout->result = 0;
    if (argc >= 1)
    {
        const char* k = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        gmb_val_t v = gmb_dict_get(*doc->dict, k);
        if (v.kind == GMB_TYPE_STRING) { gmb_obj_string_t* s = gmb_str_cstr(v.v_string.data ? v.v_string.data : ""); bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
        else if (v.kind == GMB_TYPE_DOUBLE) { bout->is_num = 1; bout->result = v.v_double; }
        else if (v.kind == GMB_TYPE_INT) { bout->is_num = 1; bout->result = (double)v.v_int; }
    }
}

static void gmb_bf_3403(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    gmb_obj_shared_val_t* doc = vm_json_doc();
    bout->is_num = 0; bout->result_ref = NULL;
    if (!doc) return;
    (void)vm; (void)func_code; (void)task;
    if (argc >= 1)
    {
        const char* name = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        /* Canone IlRuntime.ReadSection(sezione$): la sezione viene dal FILE
         * app.settings.json (CWD). Prima si leggeva SOLO il doc in-memory popolato da
         * JSON WRITESECTION → un file scritto da IO non era visibile (divergenza da
         * IL/PCode; minitests/json/readsection_json.bas: def= vuoto). Se il file non
         * esiste/è vuoto si ricade sul doc in-memory (comportamento storico). */
        gmb_val_t fv = gmb_ini_section_to_dict_val("app.settings.json", name);
        if (fv.kind == GMB_TYPE_OBJECT && fv.v_ptr)
        {
            gmb_dict_t* fd = (gmb_dict_t*)((gmb_obj_t*)fv.v_ptr)->data;
            if (fd && fd->count > 0)
            {
                gmb_obj_shared_val_t* secf = gmb_shared_dict_new();
                if (secf)
                {
                    vm_json_copy_dict(secf, fd);
                    bout->result_ref = (gmb_obj_t*)secf;
                    bout->result_ref_bt = GMB_BT_OBJECT;
                    return;
                }
            }
        }
        gmb_val_t v = gmb_dict_get(*doc->dict, name);
        if (v.kind == GMB_TYPE_OBJECT && v.v_ptr && ((gmb_obj_t*)v.v_ptr)->kind == GMB_OBJ_DICT)
        {
            gmb_obj_shared_val_t* sec = gmb_shared_dict_new();
            if (sec)
            {
                vm_json_copy_dict(sec, ((gmb_obj_shared_val_t*)v.v_ptr)->dict);
                bout->result_ref = (gmb_obj_t*)sec;
                bout->result_ref_bt = GMB_BT_OBJECT;
            }
        }
    }
}

static void gmb_bf_3404(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    bout->is_num = 0; bout->result_ref = NULL;
    if (!out) return;
    (void)vm; (void)func_code; (void)task;
    if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
    {
        gmb_dict_t* d = ((gmb_obj_shared_val_t*)args[0].ref)->dict;
        for (size_t i = 0; i < d->count; i++)
            gmb_list_add(out->list, gmb_make_val_string_counted(d->items[i].key ? d->items[i].key : ""));
    }
    bout->result_ref = (gmb_obj_t*)out;
    bout->result_ref_bt = GMB_BT_OBJECT;
}

gmb_obj_t* vm_json_plain_obj(gmb_obj_t* src)   /* §0quater: usata anche da gmb_vm_core (JSON WRITESECTION) */
{
    if (!src) return NULL;
    if (src->kind == GMB_OBJ_DICT)
    {
        gmb_obj_shared_val_t* nd = gmb_shared_dict_new();
        if (!nd) return NULL;
        gmb_dict_t* sd = ((gmb_obj_shared_val_t*)src)->dict;
        for (size_t i = 0; i < sd->count; i++)
        {
            if (sd->items[i].key && !strncmp(sd->items[i].key, "__", 2)) continue;
            gmb_val_t v = sd->items[i].value;
            if (v.kind == GMB_TYPE_OBJECT)
            {
                gmb_obj_t* sub = vm_json_plain_obj((gmb_obj_t*)v.v_ptr);
                if (sub) { gmb_dict_set(nd->dict, sd->items[i].key, gmb_make_val_obj(sub)); gmb_obj_release(sub); }   /* sub e' un ref posseduto (fresco o retain di src): il dict ne prende uno suo, si cede quello del creatore */
            }
            else if (v.kind == GMB_TYPE_STRING)
                gmb_dict_set(nd->dict, sd->items[i].key, gmb_make_val_string_counted(v.v_string.data ? v.v_string.data : ""));
            else gmb_dict_set(nd->dict, sd->items[i].key, v);
        }
        return (gmb_obj_t*)nd;
    }
    if (src->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* nl = gmb_shared_list_new();
        if (!nl) return NULL;
        gmb_list_t* sl = ((gmb_obj_shared_val_t*)src)->list;
        for (size_t i = 0; i < sl->count; i++)
        {
            gmb_val_t v = sl->items[i];
            if (v.kind == GMB_TYPE_OBJECT) { gmb_obj_t* sub = vm_json_plain_obj((gmb_obj_t*)v.v_ptr); if (sub) { gmb_list_add(nl->list, gmb_make_val_obj(sub)); gmb_obj_release(sub); }   /* idem */ }
            else if (v.kind == GMB_TYPE_STRING) gmb_list_add(nl->list, gmb_make_val_string_counted(v.v_string.data ? v.v_string.data : ""));
            else gmb_list_add(nl->list, v);
        }
        return (gmb_obj_t*)nl;
    }
    gmb_obj_retain(src);
    return src;
}

static void gmb_bf_3405(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* JSON.LOAD(path) — legge file e parsa in oggetto */
    bout->is_num = 0; bout->result_ref = NULL;
    (void)vm; (void)func_code; (void)task;
    if (argc < 1 || args[0].type != 2 || !args[0].ref || args[0].ref->kind != GMB_OBJ_STRING) return;
    const char* path = ((gmb_obj_string_t*)args[0].ref)->data;
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)sz, f); buf[got] = 0; fclose(f);
    gmb_val_t full;
    /* JSON.LOAD: radice OGGETTO **o ARRAY** (parità IL/PCode: una lista JSON caricata si
     * indicizza con [i]/EXTRACT("[i]")). Prima il check richiedeva GMB_TYPE_OBJECT → un file
     * con radice array tornava vuoto (`JSON SAVE lista` + LOAD + EXTRACT("[1]")). */
    if (vm_json_full_parse(buf, &full) && full.v_ptr)
    {
        gmb_obj_t* o = (gmb_obj_t*)full.v_ptr;
        if (o->kind == GMB_OBJ_LIST)
        {
            /* lista: nessun `__class__` da togliere → direttamente il risultato
             * (vm_json_plain_obj su una lista ritornava un dict vuoto: str={}). */
            gmb_obj_retain(o);
            bout->result_ref = o; bout->result_ref_bt = GMB_BT_OBJECT;
        }
        else
        {
            gmb_obj_t* p = vm_json_plain_obj(o);
            if (p) { bout->result_ref = p; bout->result_ref_bt = GMB_BT_OBJECT; }
        }
    }
    free(buf);
}

static void gmb_bf_3406(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* JSON.ToJson(obj) — JSON dinamico "pulito" (senza __class__) */
    bout->is_num = 0; bout->result_ref = NULL;
    (void)vm; (void)func_code; (void)task;
    if (argc >= 1 && args[0].type == 2 && args[0].ref)
    {
        gmb_obj_t* p = vm_json_plain_obj(args[0].ref);
        if (p) { bout->result_ref = p; bout->result_ref_bt = GMB_BT_OBJECT; }
    }
}

static void gmb_bf_3407(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* JSON.EXTRACT(obj, "path[0].field") */
    bout->is_num = 0; bout->result_ref = NULL; bout->result = 0;
    (void)vm; (void)func_code; (void)task;
    if (argc < 2 || args[0].type != 2 || !args[0].ref) return;
    const char* path = (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    gmb_obj_t* cur = args[0].ref;
    char seg[128]; int si = 0;
    const char* q = path;
    while (*q && cur)
    {
        if (*q == '.') { q++; continue; }
        si = 0;
        while (*q && *q != '.' && *q != '[') { if (si < 126) seg[si++] = *q; q++; }
        seg[si] = 0;
        if (cur->kind == GMB_OBJ_DICT && si > 0)
        {
            gmb_val_t v = gmb_dict_get(*((gmb_obj_shared_val_t*)cur)->dict, seg);
            cur = (v.kind == GMB_TYPE_OBJECT) ? (gmb_obj_t*)v.v_ptr : NULL;
            if (v.kind == GMB_TYPE_STRING) { gmb_obj_string_t* st = gmb_str_cstr(v.v_string.data ? v.v_string.data : ""); bout->result_ref = (gmb_obj_t*)st; bout->result_ref_bt = GMB_BT_STRING; return; }
            if (v.kind == GMB_TYPE_DOUBLE) { gmb_string_t _ts = gmb_double_to_string(v.v_double); gmb_obj_string_t* _st = gmb_str_cstr(_ts.data ? _ts.data : ""); bout->result_ref = (gmb_obj_t*)_st; bout->result_ref_bt = GMB_BT_STRING; return; }
            if (v.kind == GMB_TYPE_INT) { gmb_string_t _ts = gmb_double_to_string((double)v.v_int); gmb_obj_string_t* _st = gmb_str_cstr(_ts.data ? _ts.data : ""); bout->result_ref = (gmb_obj_t*)_st; bout->result_ref_bt = GMB_BT_STRING; return; }
            if (v.kind == GMB_TYPE_BOOL) { gmb_obj_string_t* _st = gmb_str_cstr(v.v_bool ? "true" : "false"); bout->result_ref = (gmb_obj_t*)_st; bout->result_ref_bt = GMB_BT_STRING; return; }
        }
        while (*q == '[')
        {
            q++; long idx = 0; int has = 0;
            while (*q >= '0' && *q <= '9') { idx = idx * 10 + (*q - '0'); q++; has = 1; }
            if (*q == ']') q++;
            if (cur && cur->kind == GMB_OBJ_LIST && has)
            {
                gmb_list_t* l = ((gmb_obj_shared_val_t*)cur)->list;
                if (idx >= 0 && (size_t)idx < l->count)
                {
                    gmb_val_t v = l->items[idx];
                    cur = (v.kind == GMB_TYPE_OBJECT) ? (gmb_obj_t*)v.v_ptr : NULL;
                    if (v.kind == GMB_TYPE_STRING) { gmb_obj_string_t* st = gmb_str_cstr(v.v_string.data ? v.v_string.data : ""); bout->result_ref = (gmb_obj_t*)st; bout->result_ref_bt = GMB_BT_STRING; return; }
                    if (v.kind == GMB_TYPE_DOUBLE) { gmb_string_t _ts = gmb_double_to_string(v.v_double); gmb_obj_string_t* _st = gmb_str_cstr(_ts.data ? _ts.data : ""); bout->result_ref = (gmb_obj_t*)_st; bout->result_ref_bt = GMB_BT_STRING; return; }
                    if (v.kind == GMB_TYPE_INT) { gmb_string_t _ts = gmb_double_to_string((double)v.v_int); gmb_obj_string_t* _st = gmb_str_cstr(_ts.data ? _ts.data : ""); bout->result_ref = (gmb_obj_t*)_st; bout->result_ref_bt = GMB_BT_STRING; return; }
                    if (v.kind == GMB_TYPE_BOOL) { gmb_obj_string_t* _st = gmb_str_cstr(v.v_bool ? "true" : "false"); bout->result_ref = (gmb_obj_t*)_st; bout->result_ref_bt = GMB_BT_STRING; return; }
                }
                else cur = NULL;
            }
            else cur = NULL;
        }
    }
    if (cur) { gmb_obj_retain(cur); bout->result_ref = cur; bout->result_ref_bt = GMB_BT_OBJECT; }
}

static void gmb_bf_3408(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* JSON.ToObject(Type, jsonObj) — JSON dinamico → dict "pulito" (il target tipizzato
     * lo clona nei suoi campi; Type come primo arg è un simbolo di tipo, ignorato). */
    bout->is_num = 0; bout->result_ref = NULL;
    (void)vm; (void)func_code; (void)task;
    int objarg = (argc >= 2) ? 1 : 0;
    if (argc >= 1 && args[objarg].type == 2 && args[objarg].ref)
    {
        gmb_obj_t* p = vm_json_plain_obj(args[objarg].ref);
        if (p) { bout->result_ref = p; bout->result_ref_bt = GMB_BT_OBJECT; }
    }
}

static void gmb_bf_758(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* js = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            gmb_val_t rv = gmb_json_parse(js ? js : "null");
            if (rv.kind == GMB_TYPE_STRING) { gmb_obj_string_t* st = gmb_str_cstr(rv.v_string.data ? rv.v_string.data : ""); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)st; bout->result_ref_bt = GMB_BT_STRING; return; }
            if (rv.kind == GMB_TYPE_DOUBLE) { bout->is_num = 1; bout->result = rv.v_double; return; }
            if (rv.kind == GMB_TYPE_INT) { bout->is_num = 1; bout->result = (double)rv.v_int; return; }
            if (rv.kind == GMB_TYPE_BOOL) { bout->is_num = 1; bout->result = rv.v_bool ? -1.0 : 0.0; return; }
            if (rv.kind == GMB_TYPE_NIL) { bout->is_num = 0; bout->result_ref = NULL; return; }
            gmb_val_t full;
            if (js && vm_json_full_parse(js, &full) && full.kind == GMB_TYPE_OBJECT)
            {
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)full.v_ptr; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else if (js && vm_json_full_parse(js, &full) && full.kind == GMB_TYPE_STRING)
            {
                gmb_obj_string_t* st = gmb_str_cstr(full.v_string.data ? full.v_string.data : "");
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)st; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (js && vm_json_full_parse(js, &full) && (full.kind == GMB_TYPE_DOUBLE || full.kind == GMB_TYPE_INT))
            {
                bout->is_num = 1; bout->result = full.kind == GMB_TYPE_INT ? (double)full.v_int : full.v_double;
            }
            else if (js && vm_json_full_parse(js, &full) && full.kind == GMB_TYPE_NIL)
            {
                bout->is_num = 0; bout->result_ref = NULL;
            }
            else
            {
                gmb_obj_shared_val_t* out = gmb_shared_dict_new();
                if (out)
                {
                    gmb_json_parse_to_dict(js, out->dict);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                }
                else { bout->is_num = 0; bout->result_ref = NULL; }
            }
        }
}
static void gmb_bf_761(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {

            
            const char* js = "";
            for (int i = argc - 1; i >= 0; i--)
            {
                if (args[i].type == 2 && args[i].ref && args[i].ref->kind == GMB_OBJ_STRING) { js = ((gmb_obj_string_t*)args[i].ref)->data; }
            }
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                gmb_json_parse_to_dict(js, out->dict);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_762(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* F2: il result NULL di default va impostato PRIMA — il vecchio corpo
         * azzerava result_ref DOPO aver riempito bout (output sempre vuoto). */
        bout->is_num = 0; bout->result_ref = NULL;
        if (argc >= 1 && args[0].type == 2 && args[0].ref)
        {
            gmb_obj_string_t* r = gmb_vm_json_stringify(args[0].ref);
            if (r) { bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
        }
}
static void gmb_bf_681(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* fmt = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "md";
            char* r = (path[0] && fmt[0]) ? gmb_doc_convert(path, fmt) : NULL;
            gmb_obj_string_t* out = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1210(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* r = path[0] ? gmb_doc_convert(path, "html") : NULL;
            gmb_obj_string_t* out = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1211(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* r = path[0] ? gmb_doc_convert(path, "text") : NULL;
            gmb_obj_string_t* out = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1212(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* r = gmb_doc_formats_s();
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
        
}
static void gmb_bf_1240(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* r = path[0] ? gmb_doc_extract(path) : NULL;
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1241(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* r = path[0] ? gmb_doc_extract_engine(path) : NULL;
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1242(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* dest = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* src = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_doc_extract_to(src, dest);
        }
}
static void gmb_bf_1243(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* dest = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* src = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_doc_render_to(src, dest);
        }
        
}
void gmb_bf_1244(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* text = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            gmb_val_t v = gmb_md_parse(text);
            /* BUG SIGSEGV-heisenbug (VMC-Indexing-Lista-Markdown-AST-Crash): qui si flippava
             * SOLO ->kind a GMB_OBJ_DICT sul gmb_obj_t "piccolo" (allocato da gmb_obj_new,
             * sizeof(gmb_obj_t): refcount/kind/release_fn/data/type_name/dtor) restituito da
             * gmb_md_parse, e lo si passava alla VM COSÌ COM'È — mai un vero gmb_obj_shared_val_t
             * (header + val/list/dict/ndims/dim_upper). Decine di siti (vm_class_desc_of,
             * gmb_adapter_val_to_stack, OP_LOAD_ELEMENT/STORE_MEMBER...) quando vedono
             * kind==GMB_OBJ_DICT fanno (gmb_obj_shared_val_t*)obj e leggono ->dict — campo che
             * nel vero gmb_obj_shared_val_t sta DOPO l'header ma qui cade fuori dall'allocazione
             * (sizeof(gmb_obj_t) << sizeof(gmb_obj_shared_val_t)): lettura heap-buffer-overflow
             * di un puntatore spazzatura, poi dereferenziato da gmb_dict_get → strcmp su un
             * char* a caso → SIGSEGV. Non deterministico (da qui l'heisenbug: il valore letto
             * oltre il buffer dipende dal layout dell'heap, che fprintf/ASan alterano) —
             * confermato con lldb: crash reale in gmb_dict_get ← vm_class_desc_of ← gmb_vm_tick,
             * invocato da OP_FINALIZE_SCOPE quando "d" esce di scope (controllo __class__/
             * Destructor su QUALSIASI locale con kind==GMB_OBJ_DICT).
             * NB: md_to_vm (usato da HEADINGS/LINKS/QUERY sotto) qui NON va bene — converte
             * ricorsivamente anche le stringhe in oggetti GMB_OBJ_STRING, ma gmb_md.c naviga il
             * documento con gli accessor "stile runtime" (md_str/md_dict_of si aspettano
             * gmb_val_t crudi con kind GMB_TYPE_STRING, non wrappati) e li scambierebbe per
             * stringhe vuote (HEADINGS/LINKS non troverebbero più nulla). Fix corretto:
             * gmb_vm_set_dict_result (stesso helper usato da PEARSONR/SPEARMANR/LINREGRESS ecc.,
             * gmb_vm_core.c) fa una copia SHALLOW del dict di primo livello in un vero
             * gmb_obj_shared_val_t (kind/->dict coerenti, niente più overflow su vm_class_desc_of)
             * lasciando i VALORI annidati (blocks/level/text) nella rappresentazione runtime
             * grezza — esattamente quella che gmb_md_headings/links/... si aspettano in input. */
            gmb_vm_set_dict_result(v, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
        }
}
void gmb_bf_1245(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t dv; memset(&dv, 0, sizeof(dv));
            if (argc >= 1 && args[0].type == 2 && args[0].ref) { dv.kind = GMB_TYPE_OBJECT; dv.v_ptr = args[0].ref; }
            gmb_val_t v = gmb_md_headings(dv, argc >= 2 ? (int)args[1].num : 0);
            if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
            {
                gmb_obj_t* conv = md_to_vm(v);
                if (conv) { bout->result_ref = conv; bout->result_ref_bt = GMB_OBJ_LIST; bout->is_num = 0; return; }
            }
            bout->result = 0; bout->is_num = 1;
        }
}
void gmb_bf_1246(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t dv; memset(&dv, 0, sizeof(dv));
            if (argc >= 1 && args[0].type == 2 && args[0].ref) { dv.kind = GMB_TYPE_OBJECT; dv.v_ptr = args[0].ref; }
            gmb_val_t v = func_code == 1246 ? gmb_md_links(dv) : func_code == 1247 ? gmb_md_images(dv)
                       : func_code == 1248 ? gmb_md_tables(dv) : gmb_md_codeblocks(dv);
            if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
            {
                gmb_obj_t* conv = md_to_vm(v);
                if (conv) { bout->result_ref = conv; bout->result_ref_bt = GMB_OBJ_LIST; bout->is_num = 0; return; }
            }
            bout->result = 0; bout->is_num = 1;
        }
}
void gmb_bf_1250(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t dv; memset(&dv, 0, sizeof(dv));
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_STRING)
                {
                    const char* txt = ((gmb_obj_string_t*)args[0].ref)->data;
                    dv = gmb_md_parse(txt ? txt : "");
                }
                else { dv.kind = GMB_TYPE_OBJECT; dv.v_ptr = args[0].ref; }
            }
            char* r = func_code == 1250 ? gmb_md_tree(dv) : func_code == 1251 ? gmb_md_to_html(dv)
                    : func_code == 1252 ? gmb_md_to_text(dv) : gmb_md_format(dv);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
void gmb_bf_1253(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* text = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* r = func_code == 1253 ? gmb_md_validate(text) : gmb_md_strip(text);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
void gmb_bf_1254(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_val_t dv; memset(&dv, 0, sizeof(dv));
            if (argc >= 1 && args[0].type == 2 && args[0].ref) { dv.kind = GMB_TYPE_OBJECT; dv.v_ptr = args[0].ref; }
            const char* sel = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            gmb_val_t v = gmb_md_query(dv, sel);
            if (v.kind == GMB_TYPE_OBJECT && v.v_ptr)
            {
                gmb_obj_t* conv = md_to_vm(v);
                if (conv) { bout->result_ref = conv; bout->result_ref_bt = GMB_OBJ_LIST; bout->is_num = 0; return; }
            }
            bout->result = 0; bout->is_num = 1;
        }
}
static void gmb_bf_1230(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = path[0] ? gmb_doc_open(path) : 0;
        }
}
static void gmb_bf_1231(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
            if (argc >= 1) gmb_doc_close((int)args[0].num);
        }
}
static void gmb_bf_1232(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = argc >= 1 ? gmb_doc_page_count((int)args[0].num) : 0;
        }
}
static void gmb_bf_1233(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* r = gmb_doc_extract_text((int)args[0].num, argc >= 2 ? (int)args[1].num : -1);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1234(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* r = gmb_doc_extract_markdown((int)args[0].num, argc >= 2 ? (int)args[1].num : -1);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1235(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* r = gmb_doc_extract_html((int)args[0].num, argc >= 2 ? (int)args[1].num : -1);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1236(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* r = gmb_doc_metadata((int)args[0].num);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1237(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* pattern = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            char* r = gmb_doc_search((int)args[0].num, pattern);
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1238(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* dir = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            bout->result = gmb_doc_extract_images((int)args[0].num, argc >= 3 ? (int)args[1].num : 0, dir);
        }
}
static void gmb_bf_1239(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* out = (argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[3].ref)->data : "";
            bout->result = gmb_doc_render_png((int)args[0].num, (int)args[1].num, argc >= 4 ? (int)args[2].num : 150, out);
        }
}
static void gmb_bf_275(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t s = { "", 0, 1 };
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
                s.data = src->data; s.length = src->length;
            }
            bout->result = gmb_dt_datevalue(s);
        }
}
static void gmb_bf_276(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dt_dateserial((int)args[0].num, (int)args[1].num, (int)args[2].num);
        }
}
static void gmb_bf_278(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_year(args[0].num);
        }
}
static void gmb_bf_279(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_month(args[0].num);
        }
}
static void gmb_bf_280(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_day(args[0].num);
        }
}
static void gmb_bf_281(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_hour(args[0].num);
        }
}
static void gmb_bf_282(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_minute(args[0].num);
        }
}
static void gmb_bf_283(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_second(args[0].num);
        }
}
static void gmb_bf_287(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_daysinmonth(args[0].num);
        }
}
static void gmb_bf_288(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dt_isleapyear(args[0].num) ? -1.0 : 0.0;
        }
}
static void gmb_bf_289(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dt_firstdayofmonth(args[0].num);
        }
}
static void gmb_bf_290(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_isoweek(args[0].num);
        }
}
static void gmb_bf_291(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dt_datefromunix(args[0].num);
        }
}
static void gmb_bf_292(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dt_unix(argc >= 1 ? args[0].num : 0, argc >= 1);
        }
}
static void gmb_bf_270(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* STRTIME()/TIME$ senza argomento = ora UTC corrente (canone data/ora); con argomento formatta il seriale. */
            gmb_string_t out = gmb_dt_timedollar(argc >= 1 ? args[0].num : gmb_dt_now());
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_538(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Una data JSON arriva come STRINGA ISO ("YYYY-MM-DD") (campo di un report
             * LEDGER): va PARSA (gmb_dt_datevalue), non letta come .num (che su una stringa
             * è spazzatura → 18991230). Parità PCode/IL/C99. */
            gmb_double_t _oa = args[0].num;
            if (argc >= 1 && args[0].type == 2 && args[0].ref
                && args[0].ref->kind == GMB_OBJ_STRING) {
                const char* _sd = ((gmb_obj_string_t*)args[0].ref)->data;
                if (_sd && (strchr(_sd, '-') || strchr(_sd, '/'))) {
                    gmb_string_t _gs = gmb_string_alloc(_sd);
                    _oa = gmb_dt_datevalue(_gs);
                    gmb_string_free(&_gs);
                }
            }
            gmb_string_t out = gmb_dt_dtos(_oa);
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_539(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_dow(args[0].num);
        }
}
static void gmb_bf_540(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t out = gmb_dt_cdow(args[0].num);
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_541(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t out = gmb_dt_cmonth(args[0].num);
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_542(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t s = { "", 0, 1 };
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                gmb_obj_string_t* src = (gmb_obj_string_t*)args[0].ref;
                s.data = src->data; s.length = src->length;
            }
            bout->result = func_code == 541 ? gmb_dt_ctod(s) : gmb_dt_stod(s);
        }
}
static void gmb_bf_543(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t out = gmb_dt_dtoc(args[0].num);
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_460(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
        }
}
static void gmb_bf_271(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    const char* interval = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "d";
    bout->result = gmb_dt_dateadd(interval, argc >= 3 ? args[2].num : 0, args[1].num);
}
static void gmb_bf_273(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* EOMONTH(oa [, months]): il secondo argomento è OPZIONALE. Prima si leggeva
             * args[1] senza guardia: con UNA sola chiamata `gmb_dt_eomonth(46039, <spazzatura>)`
             * restituiva un mese arbitrario (EOMONTH(2026-01-15) → 46081 = 28/02 invece di
             * 46053 = 31/01). Divergenza trovata confrontando le 4 gambe. */
            bout->result = gmb_dt_eomonth(args[0].num, argc >= 2 ? (int)args[1].num : 0);
        }
}
static void gmb_bf_272(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* interval = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "d";
            bout->result = gmb_dt_datediff(interval, args[1].num, args[2].num);
        }
}
static void gmb_bf_274(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_weekday(args[0].num);
        }
}
static void gmb_bf_284(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_dt_isdate(s) ? -1.0 : 0.0;
        }
}
static void gmb_bf_285(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* NOW(): datetime (canone: JSON ISO-8601, PRINT leggibile). num_bt=GMB_BT_DATE ->
         * vm_entry_to_val produce un gmb_val_t DATETIME. */
        { bout->result = gmb_dt_now(); bout->num_bt = GMB_BT_DATE; }
}
static void gmb_bf_286(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* TODAY(): data -> datetime (stesso canone di NOW()). */
        { bout->result = gmb_dt_today(); bout->num_bt = GMB_BT_DATE; }
}
static void gmb_bf_294(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* TIME(): seriale data+ora UTC (canone data/ora). */
        { bout->result = gmb_dt_now(); }
}
static void gmb_bf_293(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* DATE(): seriale del giorno UTC (canone data/ora). */
        { bout->result = gmb_dt_dayserial(); }
}
/* STRDATE()/DATE$ (BuiltInFunction.STRDATE = 3760): "dd/MM/yyyy" UTC. */
static void gmb_bf_strdate_c3760(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t gs = gmb_dt_date_string();
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_277(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = gmb_dt_timeserial((int)args[0].num, (int)args[1].num, (int)args[2].num);
        }
}
static void gmb_bf_295(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double val = argc >= 1 ? args[0].num : 0;
            gmb_string_t fmt = { NULL, 0, 1 };
            char empty[1] = "";
            char* fmtbuf = empty;
            if (argc >= 2 && args[1].type == 2 && args[1].ref)
                fmtbuf = ((gmb_obj_string_t*)args[1].ref)->data;
            fmt.data = fmtbuf;
            fmt.length = strlen(fmtbuf);
            gmb_string_t out = gmb_format(val, fmt);
            if (out.data)
            {
                gmb_obj_string_t* s = gmb_str_alloc(out.data, (uint32_t)out.length);
                free(out.data);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}



static void gmb_bf_107(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double re[256] = {0}, im[256] = {0};
            int n = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            { gmb_obj_shared_val_t* sv=(gmb_obj_shared_val_t*)args[0].ref; n=(int)sv->list->count; for(int i=0;i<n&&i<256;i++){gmb_val_t v=sv->list->items[i];re[i]=v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;} }
            if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
            { gmb_obj_shared_val_t* sv=(gmb_obj_shared_val_t*)args[1].ref; for(int i=0;i<n&&i<256;i++){gmb_val_t v=sv->list->items[i];im[i]=v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;} }
            if (n == 0) { bout->is_num = 0; bout->result_ref = NULL; }
            int sz = 1; while (sz < n) sz <<= 1;
            double re2[256] = {0}, im2[256] = {0};
            for (int i = 0; i < n; i++) { re2[i] = re[i]; im2[i] = im[i]; }
            gmb_fft(re2, im2, sz, func_code == 108);
            /* Scala MathNet simmetrica 1/sqrt(N) (canone IL/PCode): forward ×1/sqrt(sz),
             * inverse ×sqrt(sz) (gmb_fft ha già diviso per sz). */
            if (sz > 1) {
                double s = (func_code == 108) ? sqrt((double)sz) : 1.0 / sqrt((double)sz);
                for (int i = 0; i < sz; i++) { re2[i] *= s; im2[i] *= s; }
            }
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                for (int i = 0; i < sz; i++) { gmb_list_add(out->list, gmb_make_val_double(re2[i])); gmb_list_add(out->list, gmb_make_val_double(im2[i])); }
                gmb_mark_rows2(out, sz);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_95(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double data[256] = {0};
            int m = 0, n = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                if (sv->ndims >= 2)
                {
                    /* Matrice 2D NATIVA del C VM: lista PIATTA row-major con ndims/dim_upper
                     * (DIM d(m-1,n-1)), NON una lista-di-righe. rows=dim_upper[0]+1,
                     * cols=dim_upper[1]+1. Senza questo ramo la matrice piatta veniva iterata
                     * come righe → n=0 → gmb_analysis_pca(data,m,0) → EigenValues 0,0
                     * (bug MULTIVARIATE.Pca: VMC 0,0 vs IL 8.3333,0). */
                    m = (int)sv->dim_upper[0] + 1;
                    n = (int)sv->dim_upper[1] + 1;
                    for (int i = 0; i < m; i++)
                        for (int j = 0; j < n; j++)
                        {
                            int idx = i * n + j;
                            if (idx >= (int)sv->list->count || idx >= 256) continue;
                            gmb_val_t v = sv->list->items[idx];
                            data[idx] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;
                        }
                }
                else
                {
                    m = (int)sv->list->count;
                    for (int r = 0; r < m; r++)
                    {
                        gmb_val_t rowv = sv->list->items[r];
                        if (rowv.kind != GMB_TYPE_OBJECT || !rowv.v_ptr || ((gmb_obj_t*)rowv.v_ptr)->kind != GMB_OBJ_LIST) continue;
                        gmb_obj_shared_val_t* rl = (gmb_obj_shared_val_t*)rowv.v_ptr;
                        n = (int)rl->list->count;
                        for (int c = 0; c < n && c < 8; c++) { gmb_val_t v = rl->list->items[c]; data[r*n+c] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                    }
                }
            }
            if (m == 0 || n == 0) { bout->is_num = 0; bout->result_ref = NULL; return; }
            if (m > 8) m = 8;   /* gmb_analysis_pca: cov/centered[64] → m·8, n·8 ≤ 64 */
            if (n > 8) n = 8;
            double ev[8] = {0}, evec[64] = {0};
            gmb_analysis_pca(data, m, n, ev, evec);
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                /* Canone MULTIVARIATE.PcaResult (IlRuntime.Pca): dict {EigenValues, EigenVectors}
                 * — prima era una List posizionale [evl, evec2] e i membri .EigenValues/.EigenVectors
                 * restavano vuoti (VMC stampava 0 vs IL 1.6266). EigenVectors = lista di n righe. */
                gmb_obj_shared_val_t* evl = gmb_shared_list_new();
                if (evl) { for (int i = 0; i < n; i++) gmb_list_add(evl->list, gmb_make_val_double(ev[i])); }
                if (evl) gmb_dict_set(out->dict, "EigenValues", gmb_make_val_obj((gmb_obj_t*)evl));
                gmb_obj_shared_val_t* evl2 = gmb_shared_list_new();
                if (evl2)
                {
                    for (int i = 0; i < n; i++)
                    {
                        gmb_obj_shared_val_t* row = gmb_shared_list_new();
                        if (row) { for (int j = 0; j < n; j++) gmb_list_add(row->list, gmb_make_val_double(evec[i*8+j])); gmb_list_add(evl2->list, gmb_make_val_obj((gmb_obj_t*)row)); }
                    }
                }
                if (evl2) gmb_dict_set(out->dict, "EigenVectors", gmb_make_val_obj((gmb_obj_t*)evl2));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_89(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            double groups[256] = {0}; int sizes[8] = {0};
            int k = 0;
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                k = (int)sv->list->count;
                int off = 0;
                for (int gi = 0; gi < k && gi < 8; gi++)
                {
                    gmb_val_t gv = sv->list->items[gi];
                    if (gv.kind != GMB_TYPE_OBJECT || !gv.v_ptr || ((gmb_obj_t*)gv.v_ptr)->kind != GMB_OBJ_LIST) continue;
                    gmb_obj_shared_val_t* gl = (gmb_obj_shared_val_t*)gv.v_ptr;
                    sizes[gi] = (int)gl->list->count;
                    for (int i = 0; i < sizes[gi] && off + i < 256; i++) { gmb_val_t v = gl->list->items[i]; groups[off+i] = v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0; }
                    off += sizes[gi];
                }
            }
            double f, p, ssb, ssw;
            gmb_analysis_anova(groups, sizes, k, &f, &p, &ssb, &ssw);
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                /* Canone STATS.AnovaResult (IlRuntime.Anova): dict ESTESO FValue/PValue/
                 * DFBetween/DFWithin/SSBetween/SSWithin/MSBetween/MSWithin — NON una lista
                 * posizionale [f,p,ssb,ssw] (i membri .FValue etc. restavano vuoti). */
                int dfb = k - 1;
                int tot = 0; for (int gi = 0; gi < k; gi++) tot += sizes[gi];
                int dfw = tot - k;
                gmb_dict_set(out->dict, "FValue", gmb_make_val_double(f));
                gmb_dict_set(out->dict, "PValue", gmb_make_val_double(p));
                gmb_dict_set(out->dict, "DFBetween", gmb_make_val_double((double)dfb));
                gmb_dict_set(out->dict, "DFWithin", gmb_make_val_double((double)dfw));
                gmb_dict_set(out->dict, "SSBetween", gmb_make_val_double(ssb));
                gmb_dict_set(out->dict, "SSWithin", gmb_make_val_double(ssw));
                gmb_dict_set(out->dict, "MSBetween", gmb_make_val_double(dfb > 0 ? ssb / (double)dfb : 0.0));
                gmb_dict_set(out->dict, "MSWithin", gmb_make_val_double(dfw > 0 ? ssw / (double)dfw : 0.0));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}

static void gmb_bf_186(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_fin_clear_error();
        {
            double* d = NULL; int n = 0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            { gmb_obj_shared_val_t* sv=(gmb_obj_shared_val_t*)args[0].ref; n=(int)sv->list->count; d=(double*)malloc(sizeof(double)*(size_t)(n?n:1)); if(d) for(int i=0;i<n;i++){gmb_val_t v=sv->list->items[i];d[i]=v.kind==GMB_TYPE_DOUBLE?v.v_double:v.kind==GMB_TYPE_INT?(double)v.v_int:0;} }
            bout->result = (d && n>0) ? gmb_tm_min_semivariance(d, n, argc >= 2 ? args[1].num : 0) : 0;
            free(d);
        }
            { const char* fe = gmb_fin_last_error(); if (fe && *fe) vm_error(task, fe); }
}

static void gmb_bf_544(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)gmb_dt_seconds();
        }
}


static void gmb_bf_677(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc == 0)
            {
                
                task->force_yield = 1;
                return;
            }
            /* Il ramo bond-yield (YIELD con argomenti) è stato rimosso con la migrazione
             * BONDS a FFI: resta solo lo YIELD() di cooperazione del GPM. */
            bout->result = 0.0;
        }
}
static void gmb_bf_3802(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* GREP.MATCHES(3802)/COUNT(3803)/FILES(3804) — motore C condiviso gmb_grep (§0bis).
             * MATCHES/FILES ritornano un ARRAY di stringhe (shared list). */
            const char* pattern = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* path = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            if (func_code == 3803) {
                bout->is_num = 1; bout->result = (double)gmb_grep_count_path(pattern, path);
            } else {
                gmb_int_t n = 0;
                gmb_string_t* a = (func_code == 3804) ? gmb_grep_files_arr(pattern, path, &n)
                                                    : gmb_grep_matches_arr(pattern, path, &n);
                gmb_obj_shared_val_t* out = gmb_shared_list_new();
                if (out && a) {
                    for (gmb_int_t i = 0; i < n; i++) {
                        const char* sd = a[i].data ? a[i].data : "";
                        gmb_list_add(out->list, gmb_make_val_string_counted_n(sd, strlen(sd)));
                    }
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
                } else { bout->is_num = 0; bout->result_ref = NULL; }
                if (a) { for (gmb_int_t i = 0; i < n; i++) free(a[i].data); free(a); }
            }
        }
}
static void gmb_bf_3815(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* MESSAGE.DELIVERY(bus) — contatori di consegna del bus.
             * Su VM/C l'handle del bus è il NOME della variabile (contratto di CONNECT/SUBSCRIBE/
             * PUBLISH/PING/CLOSE, vedi EmitMessageOp/HandleNameExpr): qui il nome si risolve
             * nell'id del runtime C condiviso (gmb_vm_msg_bus_id) e il testo arriva da
             * gmb_msg_delivery_stats — stessa sorgente della gamba C99Native (§0bis). */
            (void)vm; (void)func_code; (void)task;
            {
                /* Su VM/C l'argomento arriva come NOME del bus (stringa, dal Compiler): un
                 * argomento NUMERICO (es. `MESSAGE.DELIVERY(1)`) non identifica un bus di questa
                 * gamba — gli zeri sono la risposta coerente, come per un bus inesistente. */
                const char* h = (argc >= 1 && args[0].type == 2 && args[0].ref) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
                char buf[192];
                gmb_msg_delivery_stats(gmb_vm_msg_bus_id(h), buf, sizeof(buf));
                {
                    gmb_obj_string_t* r = gmb_str_alloc(buf, (uint32_t)strlen(buf));
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                }
            }
        }
}
static void gmb_bf_127(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                int total = (int)sv->list->count;           
                int m = 0, n = 0;
                int bcount = 0;
                if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
                    bcount = (int)((gmb_obj_shared_val_t*)args[1].ref)->list->count;
                
                if (total > 0 && (sv->list->items[0].kind == GMB_TYPE_OBJECT || sv->list->items[0].kind == GMB_TYPE_ARRAY))
                {
                    m = total;
                    gmb_obj_shared_val_t* r0 = (gmb_obj_shared_val_t*)sv->list->items[0].v_ptr;
                    if (r0 && r0->list) n = (int)r0->list->count;
                }
                else
                {
                    
                    m = bcount;
                    if (bcount > 0) n = total / bcount;
                }
                if (m > 0 && n > 0)
                {
                    int64_t A = gmb_matrix_create(m, n);
                    double* b = (double*)calloc((size_t)(bcount > 0 ? bcount : 1), sizeof(double));
                    if (A > 0 && b)
                    {
                        for (int r = 0; r < m; r++)
                        {
                            if (sv->list->items[r].kind == GMB_TYPE_OBJECT || sv->list->items[r].kind == GMB_TYPE_ARRAY)
                            {
                                gmb_obj_shared_val_t* row = (gmb_obj_shared_val_t*)sv->list->items[r].v_ptr;
                                if (row && row->list)
                                    for (int c = 0; c < n && c < (int)row->list->count; c++)
                                    {
                                        gmb_val_t v = row->list->items[c];
                                        gmb_matrix_set(A, r, c, v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0);
                                    }
                            }
                            else
                            {
                                
                                for (int c = 0; c < n; c++)
                                {
                                    int idx = r * n + c;
                                    if (idx < total)
                                    {
                                        gmb_val_t v = sv->list->items[idx];
                                        gmb_matrix_set(A, r, c, v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0);
                                    }
                                }
                            }
                        }
                        for (int r = 0; r < m && r < (int)((gmb_obj_shared_val_t*)args[1].ref)->list->count; r++)
                        {
                            if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
                            {
                                gmb_val_t v = ((gmb_obj_shared_val_t*)args[1].ref)->list->items[r];
                                b[r] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0;
                            }
                        }
                        
                        double* x = NULL;
                        int nx = n;
                        if (n > 0 && n <= 8 && m > 0)
                        {
                            /* Equazioni normali via Cholesky in-house (gmb_matrix_* inaffidabili):
                             * AtA * x = At * b. m==n → sistema quadrato AtA=A. */
                            double AtA[64] = {0}, Atb[8] = {0};
                            for (int i = 0; i < n; i++)
                            {
                                for (int j = 0; j < n; j++)
                                {
                                    double s = 0;
                                    for (int k = 0; k < m; k++) s += gmb_matrix_get(A, k, i) * gmb_matrix_get(A, k, j);
                                    AtA[i * n + j] = s;
                                }
                                double s = 0;
                                for (int k = 0; k < m; k++) s += gmb_matrix_get(A, k, i) * b[k];
                                Atb[i] = s;
                            }
                            x = (double*)calloc((size_t)n, sizeof(double));
                            if (x && !gmb_cholesky_solve(AtA, Atb, n, x)) { free(x); x = NULL; }
                        }
                        if (x)
                        {
                            for (int r = 0; r < nx; r++) gmb_list_add(out->list, gmb_make_val_double(x[r]));
                            free(x);
                        }
                        gmb_matrix_free(A);
                        free(b);
                    }
                    else { free(b); }
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else if (out && argc >= 2 && args[0].type != 2)
            {
                /* LINALG.SOLVE(handle, array): matrice come HANDLE + vettore raw array.
                 * Costruisce il vettore b come matrice n×1, risolve con gmb_matrix_lu_solve
                 * e ritorna la colonna soluzione come lista (parità IL/C99Native). */
                int64_t hA = (int64_t)args[0].num;
                int nb = 0;
                double* bvals = NULL;
                if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
                {
                    gmb_obj_shared_val_t* svb = (gmb_obj_shared_val_t*)args[1].ref;
                    nb = (int)svb->list->count;
                    bvals = (double*)calloc((size_t)(nb > 0 ? nb : 1), sizeof(double));
                    if (bvals)
                        for (int i = 0; i < nb; i++)
                        {
                            gmb_val_t v = svb->list->items[i];
                            bvals[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0;
                        }
                }
                if (bvals && nb > 0)
                {
                    int64_t bh = gmb_matrix_create(nb, 1);
                    for (int i = 0; i < nb; i++) gmb_matrix_set(bh, i, 0, bvals[i]);
                    int64_t sol = gmb_matrix_lu_solve(hA, bh);
                    gmb_matrix_free(bh);
                    if (sol > 0)
                    {
                        int nrows = gmb_matrix_rows(sol);
                        for (int i = 0; i < nrows; i++)
                            gmb_list_add(out->list, gmb_make_val_double(gmb_matrix_get(sol, i, 0)));
                    }
                }
                free(bvals);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else
            {
                if (out) gmb_obj_release((gmb_obj_t*)out);
                gmb_obj_shared_val_t* empty = gmb_shared_list_new();
                bout->is_num = 0; bout->result_ref = empty ? (gmb_obj_t*)empty : NULL; bout->result_ref_bt = GMB_BT_OBJECT;
            }
        }
}
static void gmb_bf_515(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_closure_t* c = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[1].ref : NULL;
            if (!sv || !sv->list || !c) { bout->is_num = 0; bout->result_ref = NULL; return; }
            /* Bug corretto (2026-08-31, audit lambda in builtin): questa funzione gestisce
             * AMAP(515)/AFILTER(516)/ACOUNT(519) — ma i due branch che avrebbero dovuto
             * distinguerli controllavano func_code==514 (ASCAN, mai instradato qui, gestito
             * da gmb_bf_514) e func_code==518 (AUNIQUE, mai instradato qui, gestito da
             * gmb_bf_518): condizioni SEMPRE false per QUALSIASI valore di func_code
             * realmente ricevuto da questa funzione. Il risultato: AMAP e ACOUNT
             * eseguivano sempre la sola logica "else" (semantica di FILTER) invece della
             * propria — AMAP restituiva una lista filtrata sugli elementi originali invece
             * dei valori trasformati, ACOUNT restituiva una LISTA invece del conteggio
             * numerico. Mai osservato empiricamente finché AMAP/AFILTER/ACOUNT non erano
             * nemmeno raggiungibili dal compilatore nativo (vedi g_cc_builtins in
             * gmb_compiler.c — questi nomi non erano affatto registrati, ogni chiamata
             * restituiva NIL per compile-parity: bug distinto, corretto insieme a questo). */
            if (func_code == 519) {
                /* ACOUNT: conta gli elementi per cui il predicato è vero (nessuna lista in
                   output, solo il conteggio numerico). Passa l'elemento con _sync_entry
                   (preserva OBJECT/UDT; _sync_val lo troncava a 0 → predicati su array-UDT
                   sempre falsi). */
                int cnt = 0;
                for (int i = 0; i < (int)sv->list->count; i++)
                {
                    gmb_vm_call_closure_sync_entry(vm, task, c, sv->list->items[i]);
                    int truthy = task->sync_result_type == 1 ? (task->sync_result != 0)
                        : (task->sync_result_type == 2 && task->sync_result_ref ? 1 : 0);
                    if (truthy) cnt++;
                }
                bout->is_num = 1; bout->result = (double)cnt;
                return;
            }
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                for (int i = 0; i < (int)sv->list->count; i++)
                {
                    gmb_val_t it = sv->list->items[i];
                    if (func_code == 515) {
                        /* AMAP: la lambda può restituire una stringa/oggetto, non solo un
                         * numero — _sync_val tronca a double (0.0 per i non-numerici),
                         * risultato SEMPRE 0 per lambda tipo FN(s)=>UPPER(s). _sync_entry
                         * preserva il tipo reale via sync_result_ref/sync_result_type,
                         * stesso schema già usato da OP_NEW_GENERATOR_EXPR; il giro tramite
                         * gmb_adapter_stack_to_val (non gmb_make_val_obj diretto) è quello
                         * che distingue davvero una STRINGA (GMB_TYPE_STRING, con v_string
                         * copiato a valore) da un oggetto lista/dict (GMB_TYPE_OBJECT) —
                         * altrimenti una stringa di ritorno finirebbe boxata come OBJECT e
                         * consumatori come JSON.Stringify la vedrebbero come "null". */
                        gmb_vm_call_closure_sync_entry(vm, task, c, it);
                        gmb_stack_entry_t rse; memset(&rse, 0, sizeof(rse));
                        rse.type = task->sync_result_type;
                        rse.ref = task->sync_result_ref;
                        rse.num = task->sync_result;
                        gmb_list_add(out->list, gmb_adapter_stack_to_val_borrowed(rse));
                    } else {
                        /* AFILTER (516): il predicato riceve l'elemento REALE. Bug corretto
                         * (2026-09-07): _sync_val troncava a double l'elemento OBJECT/UDT
                         * (0.0) → `o.ID > 6` su un array di UDT era sempre falso e AFILTER
                         * restituiva SEMPRE la lista vuota. _sync_entry preserva il tipo;
                         * il risultato numerico del predicato resta in task->sync_result. */
                        gmb_vm_call_closure_sync_entry(vm, task, c, it);
                        double r = task->sync_result;
                        if (task->sync_result_type != 1)
                            r = (task->sync_result_type == 2 && task->sync_result_ref) ? 1.0 : 0.0;
                        if (r != 0) gmb_list_add(out->list, it); /* AFILTER (516) */
                    }
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_517(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_closure_t* c = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[1].ref : NULL;
            if (!sv || !sv->list || !c) { bout->is_num = 0; bout->result_ref = NULL; return; }
            /* Parita IL (ArrayHelpers.AReduce/AReduceNoInitial): lista VUOTA con init →
             * restituisce init; senza init → 0 (default numerico), non piu' NIL. */
            if (sv->list->count == 0) {
                if (argc >= 3) {
                    gmb_val_t initv = gmb_adapter_stack_to_val(args[2]);
                    bout->result = initv.kind == GMB_TYPE_DOUBLE ? initv.v_double
                        : initv.kind == GMB_TYPE_INT ? (double)initv.v_int : 0;
                } else {
                    bout->result = 0.0;
                }
                return;
            }

            gmb_val_t acc;
            int start_idx = 0;
            if (argc >= 3) { acc = gmb_adapter_stack_to_val(args[2]); start_idx = 0; }
            else { acc = sv->list->items[0]; start_idx = 1; }
            /* Bug corretto (2026-09-06): accumulatore ed elemento venivano forzati a
             * DOUBLE prima di chiamare la closure (gmb_vm_call_closure_sync2, solo
             * numeri) — un elemento oggetto/UDT diventava sempre 0, causando un crash a
             * valle se la callback provava a leggerne un campo (es. P.Prezzo su un
             * accumulatore/elemento che in realtà era 0.0). Parità IL
             * (ArrayHelpers.AReduce<T,TAcc>, generico su entrambi i tipi via CallClosureSync
             * con gli oggetti reali, non un double troncato): usa
             * gmb_vm_call_closure_sync2_entry per passare/ricevere valori reali. */
            for (int i = start_idx; i < (int)sv->list->count; i++)
            {
                gmb_vm_call_closure_sync2_entry(vm, task, c, acc, sv->list->items[i]);
                gmb_stack_entry_t rse; memset(&rse, 0, sizeof(rse));
                rse.type = task->sync_result_type;
                rse.ref = task->sync_result_ref;
                rse.num = task->sync_result;
                acc = gmb_adapter_stack_to_val(rse);
            }
            gmb_stack_entry_t out_se = gmb_adapter_val_to_stack(acc);
            if (out_se.type == 2 && out_se.ref) {
                bout->is_num = 0; bout->result_ref = out_se.ref; bout->result_ref_bt = out_se.basic_type;
            } else {
                bout->result = out_se.num;
            }
        }
}
static void gmb_bf_518(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && sv && sv->list)
            {
                for (int i = 0; i < (int)sv->list->count; i++)
                {
                    int dup = 0;
                    for (int j = 0; j < (int)out->list->count; j++)
                        if (gmb_val_equals(out->list->items[j], sv->list->items[i])) { dup = 1; }
                    if (!dup) gmb_list_add(out->list, sv->list->items[i]);
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_521(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* AREVERSE — parita IL (ArrayHelpers: clone-then-reverse): ritorna UNA NUOVA
             * lista rovesciata e NON muta MAI la sorgente (il valore si usa dal ritorno).
             * Prima rovesciava la lista dell'argomento in-place e ritornava lo stesso
             * oggetto (semantica aReverse di Clipper) — divergenza da IL. */
            (void)vm; (void)argc; (void)func_code; (void)task;
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && sv && sv->list) {
                for (int i = (int)sv->list->count - 1; i >= 0; i--)
                    gmb_list_add(out->list, sv->list->items[i]);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            } else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_522(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* ABINARYSEARCH — parita IL (CommonLib.ArrayUtils.BinarySearch): VERA ricerca
             * binaria su lista gia' ordinata; indice 0-based o -1. gmb_list_binary_search
             * e' ora una vera binaria (confronto esatto), vedi gmb_list.c. */
            (void)vm; (void)argc; (void)func_code; (void)task;
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            double target = args[1].num;
            int found = -1;
            if (sv && sv->list)
                found = (int)gmb_list_binary_search(*sv->list, target);
            bout->result = (double)found;
        }
}
static void gmb_bf_523(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            const char* sep = (argc >= 2 && args[1].type == 2 && args[1].ref) ? ((gmb_obj_string_t*)args[1].ref)->data : ",";
            gmb_string_t out = gmb_str_join_list(sv && sv->list ? sv->list->items : NULL, sv && sv->list ? (int)sv->list->count : 0, sep);
            if (out.data) { gmb_obj_string_t* r = gmb_str_alloc(out.data, (uint32_t)out.length); free(out.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_525(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && sv && sv->list)
            {
                for (int i = 0; i < (int)sv->list->count; i++)
                {
                    gmb_val_t it = sv->list->items[i];
                    if (it.kind == GMB_TYPE_OBJECT && it.v_ptr)
                    {
                        gmb_obj_shared_val_t* sub = (gmb_obj_shared_val_t*)it.v_ptr;
                        if (sub->list)
                            for (int j = 0; j < (int)sub->list->count; j++) gmb_list_add(out->list, sub->list->items[j]);
                        else gmb_list_add(out->list, it);
                    }
                    else gmb_list_add(out->list, it);
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_513(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && sv && sv->list) { for (int i=0;i<(int)sv->list->count;i++) gmb_list_add(out->list, sv->list->items[i]); bout->is_num=0; bout->result_ref=(gmb_obj_t*)out; bout->result_ref_bt=GMB_BT_OBJECT; }
            else { bout->is_num=0; bout->result_ref=NULL; }
        }
}
static void gmb_bf_514(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            int found = -1;
            if (sv && sv->list)
            {
                if (args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_CLOSURE)
                {
                    gmb_obj_closure_t* c = (gmb_obj_closure_t*)args[1].ref;
                    for (int i = 0; i < (int)sv->list->count; i++)
                        if (gmb_vm_call_closure_sync_val(vm, task, c, sv->list->items[i]) != 0) { found = i; break; }
                }
                else
                {
                    /* Forma valore — confronto ESATTO (==, parita IL EqualityComparer<double>),
                     * non fabs<1e-12; primo match con break. */
                    double target = args[1].num;
                    for (int i=0;i<(int)sv->list->count;i++){ gmb_val_t it=sv->list->items[i]; double d=it.kind==GMB_TYPE_DOUBLE?it.v_double:it.kind==GMB_TYPE_INT?(double)it.v_int:0; if (d==target){found=i; break;} }
                }
            }
            bout->result = (double)found;
        }
}
static void gmb_bf_520(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            if (sv && sv->list) {
                for (int i = (int)sv->list->count - 1; i > 0; i--) {
                    int j = (int)(gmb_rnd() * (i + 1));
                    gmb_val_t t = sv->list->items[i]; sv->list->items[i] = sv->list->items[j]; sv->list->items[j] = t;
                }
                bout->is_num=0; gmb_obj_retain((gmb_obj_t*)sv); bout->result_ref=(gmb_obj_t*)sv; bout->result_ref_bt=GMB_BT_OBJECT;
            } else { bout->is_num=0; bout->result_ref=NULL; }
        }
}
static void gmb_bf_524(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            int cs = (int)args[1].num;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && sv && sv->list && cs > 0) {
                for (int ci = 0; ci < (int)sv->list->count; ci += cs) {
                    gmb_obj_shared_val_t* chunk = gmb_shared_list_new();
                    if (chunk) { for (int j=ci;j<(int)sv->list->count && j<ci+cs;j++) gmb_list_add(chunk->list, sv->list->items[j]); gmb_list_add(out->list, gmb_make_val_obj((gmb_obj_t*)chunk)); }
                }
                bout->is_num=0; bout->result_ref=(gmb_obj_t*)out; bout->result_ref_bt=GMB_BT_OBJECT;
            } else { bout->is_num=0; bout->result_ref=NULL; }
        }
}
static void gmb_bf_526(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            int newLen = (int)args[1].num;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && sv && sv->list) {
                for (int i=0;i<(int)sv->list->count && i<newLen;i++) gmb_list_add(out->list, sv->list->items[i]);
                gmb_val_t padval = argc >= 3 ? gmb_adapter_stack_to_val_borrowed(args[2]) : gmb_make_val_double(0);
                while ((int)out->list->count < newLen) gmb_list_add(out->list, padval);
                bout->is_num=0; bout->result_ref=(gmb_obj_t*)out; bout->result_ref_bt=GMB_BT_OBJECT;
            } else { bout->is_num=0; bout->result_ref=NULL; }
        }
}
static void gmb_bf_530(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            if (sv && sv->list) {
                int pos = (int)args[1].num;
                /* AINS array, pos (senza valore): lo slot nuovo è il default del TIPO
                 * elemento (parity IL EmitAInsStatement: newarr è zero-inizializzato →
                 * 0 per array numerici, nil per stringhe). Prima inseriva sempre nil →
                 * su un array numerico VMC stampava vuoto dove IL/PCode stampano 0
                 * (ArraysMiniTest.bas: ains= 10 0 20 30). */
                gmb_val_t v;
                if (argc >= 3) v = gmb_adapter_stack_to_val(args[2]);
                else {
                    gmb_val_t first = sv->list->count > 0 ? sv->list->items[0] : gmb_val_nil();
                    v = (first.kind == GMB_TYPE_INT || first.kind == GMB_TYPE_DOUBLE)
                        ? gmb_make_val_double(0.0) : gmb_val_nil();
                }
                gmb_list_insert(sv->list, pos, v);
            }
            bout->is_num=0; bout->result_ref=NULL;
        }
}
static void gmb_bf_531(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            if (sv && sv->list) gmb_list_remove_at(sv->list, (int)args[1].num);
            bout->is_num=0; bout->result_ref=NULL;
        }
}
static void gmb_bf_532(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_closure_t* c = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_CLOSURE)
                ? (gmb_obj_closure_t*)args[1].ref : NULL;
            /* Clipper AEVAL: il block riceve (elemento, indice) — parità IL
             * (ArrayHelpers.AEvalIndexed) e C99* (gmb_array_aeval). L'ARITÀ del block arriva
             * come 3° argomento dal compilatore: il binding lega gli argomenti in ordine
             * INVERSO, quindi a un block a 1 parametro va passato SOLO l'elemento
             * (altrimenti l'indice finirebbe nel primo parametro — regressione arrays18). */
            int aeval_arity = (argc >= 3 && args[2].type != 0) ? (int)args[2].num : 2;
            if (sv && sv->list && c) { for (int i=0;i<(int)sv->list->count;i++){ gmb_val_t it=sv->list->items[i]; if (aeval_arity >= 2) gmb_vm_call_closure_sync2_entry(vm,task,c,it,gmb_make_val_double((double)i)); else gmb_vm_call_closure_sync_val(vm,task,c,it); } }
            bout->is_num=0; bout->result_ref=NULL;
        }
}
static void gmb_bf_533(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            if (sv && sv->list) {
                gmb_val_t v = gmb_adapter_stack_to_val(args[1]);
                int start = (argc >= 3 && args[2].type == 0) ? 0 : (argc >= 3 ? (int)args[2].num : 0);
                int count = (argc >= 4 && args[3].type == 0) ? (int)sv->list->count - start : (argc >= 4 ? (int)args[3].num : (int)sv->list->count - start);
                if (count <= 0) count = (int)sv->list->count - start;
                for (int i=start;i<(int)sv->list->count && i<start+count;i++) gmb_list_set(sv->list, i, v);
            }
            bout->is_num=0; bout->result_ref=NULL;
        }
}
static void gmb_bf_534(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* src = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* dst = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
            if (src && src->list && dst && dst->list) {
                int srcStart = (argc >= 3 && args[2].type == 0) ? 0 : (argc >= 3 ? (int)args[2].num : 0);
                int count = (argc >= 4 && args[3].type == 0) ? (int)src->list->count - srcStart : (argc >= 4 ? (int)args[3].num : (int)src->list->count - srcStart);
                if (count <= 0) count = (int)src->list->count - srcStart;
                int dstStart = (argc >= 5 && args[4].type == 0) ? 0 : (argc >= 5 ? (int)args[4].num : 0);
                for (int i=0;i<count && srcStart+i<(int)src->list->count;i++){ while (dstStart+i >= (int)dst->list->count) gmb_list_add(dst->list, gmb_val_nil()); gmb_list_set(dst->list, dstStart+i, src->list->items[srcStart+i]); }
            }
            bout->is_num=0; bout->result_ref=NULL;
        }
}
static void gmb_bf_529(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_val_t v = gmb_adapter_stack_to_val_borrowed(args[1]);
                gmb_list_add(sv->list, v);
            }
            bout->is_num = 0; bout->result_ref = NULL;
        }
}
static void gmb_bf_535(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            int descending = 0;
            if (argc >= 3 && args[2].type == 2 && args[2].ref)
            {
                const char* ord = ((gmb_obj_string_t*)args[2].ref)->data;
                if (ord && (ord[0] == 'D' || ord[0] == 'd')) descending = 1;
            }
            if (sv && sv->list)
            {
                if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_CLOSURE)
                {
                    /* Bug corretto (2026-09-02): il comparatore BASIC ("RETURN a > b") è un
                     * PREDICATO booleano, non un confronto a tre vie — restituisce -1
                     * (TRUE, convenzione BASIC) o 0 (FALSE), mai un valore "negativo per
                     * <, positivo per >". Il confronto `cmp > 0`/`cmp < 0` qui sotto non
                     * intercettava MAI un TRUE booleano (-1): lo swap non scattava mai,
                     * l'array restava sempre invariato qualunque fosse il comparatore. */
                    gmb_obj_closure_t* c = (gmb_obj_closure_t*)args[1].ref;
                    int n = (int)sv->list->count;
                    /* Insertion sort STABILE (parity IL ArrayHelpers.ASortWithComparer /
                     * PCode / C99 gmb_array_sort_*_cmp): sposta a sinistra se
                     * compareFunc(key, prev) < 0. Gestisce SIA il 3-vie (segno: <0 = prima)
                     * SIA il predicato booleano BASIC (TRUE = -1 = "prima"; FALSE = 0 = "non
                     * prima"). Il precedente bubble con `swap_needed = cmp != 0` dava un
                     * ordine ASCENDENTE per il predicato `a > b` (opposto al sample) e
                     * sbagliava il comparatore a 3 vie (`2 1 3`). */
                    for (int i = 1; i < n; i++)
                    {
                        gmb_val_t key = sv->list->items[i];
                        int j = i - 1;
                        while (j >= 0)
                        {
                            double cmp = gmb_vm_call_closure_sync2_val(vm, task, c, key, sv->list->items[j]);
                            if (!(cmp < 0)) break;
                            sv->list->items[j+1] = sv->list->items[j];
                            j--;
                        }
                        sv->list->items[j+1] = key;
                    }
                    if (descending) gmb_list_reverse(sv->list);
                }
                else
                {
                    gmb_list_sort(sv->list);
                    if (descending) gmb_list_reverse(sv->list);
                }
                bout->is_num = 0; gmb_obj_retain((gmb_obj_t*)sv); bout->result_ref = (gmb_obj_t*)sv; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_266(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            
            if (args[0].type == 1)
            {
                int64_t h = (int64_t)args[0].num;
                bout->result = gmb_matrix_determinant(h);
                return;
            }
            gmb_obj_shared_val_t* sv = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
                ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            int n = 0, total = 0;
            double vals[128] = {0};
            if (sv && sv->list)
            {
                total = (int)sv->list->count;
                if (total > 0 && (sv->list->items[0].kind == GMB_TYPE_OBJECT || sv->list->items[0].kind == GMB_TYPE_ARRAY))
                {
                    
                    n = (int)sv->list->count;
                    int c = 0;
                    for (int r = 0; r < n && c < 128; r++)
                    {
                        gmb_obj_shared_val_t* row = (gmb_obj_shared_val_t*)sv->list->items[r].v_ptr;
                        if (row && row->list)
                            for (int k = 0; k < (int)row->list->count && c < 128; k++)
                            {
                                gmb_val_t v = row->list->items[k];
                                vals[c++] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0;
                            }
                    }
                    n = (int)sqrt((double)c);
                    if (n * n != c) n = (int)sqrt((double)total);
                }
                else
                {
                    
                    n = (int)sqrt((double)total);
                    for (int i = 0; i < total && i < 128; i++)
                    {
                        gmb_val_t v = sv->list->items[i];
                        vals[i] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0;
                    }
                }
            }
            if (n < 1 || n > 12) { bout->result = 0; }
            int64_t A = gmb_matrix_create(n, n);
            if (A <= 0) { bout->result = 0; }
            for (int r = 0; r < n; r++)
                for (int c = 0; c < n; c++)
                    gmb_matrix_set(A, r, c, vals[r * n + c]);
            if (func_code == 266) bout->result = gmb_matrix_determinant(A);
            else bout->result = gmb_matrix_determinant(A); 
            gmb_matrix_free(A);
        }
}
static void gmb_bf_655(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_GENERATOR)
            {
                gmb_obj_generator_t* gen = (gmb_obj_generator_t*)args[0].ref;
                gmb_stack_entry_t out = {0};
                if (gmb_vm_next_generator(vm, gen, &out) == 0)
                {
                    if (out.type == 2 && out.ref) { bout->is_num = 0; bout->result_ref = out.ref; bout->result_ref_bt = out.basic_type; }
                    else if (out.type == 1) { bout->result = out.num; }
                    else { bout->is_num = 0; bout->result_ref = NULL; }
                }
                else
                {
                    
                    task->stop_iteration = 1;
                    bout->is_num = 0; bout->result_ref = NULL;
                }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_537(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3)
            {
                if (args[0].type == 1 && args[0].num != 0)
                {
                    if (args[1].type == 2 && args[1].ref) { bout->is_num = 0; gmb_obj_retain(args[1].ref); bout->result_ref = args[1].ref; bout->result_ref_bt = args[1].basic_type; }
                    else if (args[1].type == 1) { bout->result = args[1].num; }
                    else { bout->is_num = 0; bout->result_ref = NULL; }
                }
                else
                {
                    if (args[2].type == 2 && args[2].ref) { bout->is_num = 0; gmb_obj_retain(args[2].ref); bout->result_ref = args[2].ref; bout->result_ref_bt = args[2].basic_type; }
                    else if (args[2].type == 1) { bout->result = args[2].num; }
                    else { bout->is_num = 0; bout->result_ref = NULL; }
                }
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_536(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
            if (argc < 1 || args[0].type == 0) bout->result = -1;
            else if (args[0].type == 1) bout->result = (args[0].num == 0) ? -1 : 0;
            else if (args[0].type == 2 && args[0].ref == NULL) bout->result = -1;
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                gmb_obj_string_t* s = (gmb_obj_string_t*)args[0].ref;
                gmb_string_t gs = { s->data ? s->data : "", s->length, 1 };
                bout->result = gmb_str_isblank(gs) ? -1 : 0;
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST && ((gmb_obj_shared_val_t*)args[0].ref)->list->count == 0) bout->result = -1;
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT && ((gmb_obj_shared_val_t*)args[0].ref)->dict->count == 0) bout->result = -1;
        }
}
/* SIZEOF — canone: dimensione in byte del TIPO STATICO (unica sorgente TypeLayout, lato C#
 * in common/Ast/TypeLayout.cs). Gli argomenti il cui tipo è noto a compile-time NON arrivano
 * qui: il compilatore sostituisce la chiamata con la costante (fold in
 * common/Compiler/Compiler.SizeOf.cs). Restano: il NOME DI TIPO, che il compilatore spinge
 * come descrittore di classe — la cui chiave "Size" è già la dimensione calcolata — e i valori
 * senza tipo statico: 8 per un numero (a runtime un numerico GMBasic è sempre un double), -1
 * per tutto il resto (stringa dinamica, lista, dict, oggetto).
 * PRIMA di questa riscrittura lo switch non aveva NESSUN break: GMB_OBJ_STRING cadeva in
 * GMB_OBJ_LIST leggendo ->list su un oggetto stringa (SIGSEGV reale, exit 139, su
 * SIZEOF(<stringa>)) e GMB_OBJ_DICT cadeva in `default:`, che AZZERAVA il totale appena
 * calcolato — SIZEOF di qualunque UDT dava 0 su VM/C. La tabella delle larghezze dei campi che
 * stava in questo handler è ELIMINATA: era una copia divergente del canone (SINGLE 8,
 * LONG/QUAD 4, nessuna consapevolezza dei PACKED TYPE). */
static double gmb_sizeof_number(gmb_val_t v)
{
    switch (v.kind)
    {
        case GMB_TYPE_INT: return (double)v.v_int;
        case GMB_TYPE_DOUBLE: return (double)v.v_double;
        default: return -1;
    }
}
static gmb_dict_t* gmb_sizeof_as_dict(gmb_val_t v)
{
    if (v.kind != GMB_TYPE_OBJECT || !v.v_ptr) return NULL;
    gmb_obj_t* o = (gmb_obj_t*)v.v_ptr;
    if (o->kind == GMB_OBJ_DICT && ((gmb_obj_shared_val_t*)o)->dict)
        return ((gmb_obj_shared_val_t*)o)->dict;
    return o->data ? (gmb_dict_t*)o->data : NULL;
}
/* "Size" del descrittore, -1 se `d` non è un descrittore di classe/UDT. */
static double gmb_sizeof_dict_bytes(gmb_dict_t* d)
{
    if (!d) return -1;
    /* Un DICT utente con una chiave "Size" omonima NON è un tipo: i descrittori hanno sempre
     * FieldNames/FieldTypes (tipo dichiarato) o __class__ (istanza). */
    int isDescriptor = gmb_dict_get(*d, "FieldNames").kind != GMB_TYPE_NIL
        || gmb_dict_get(*d, "FieldTypes").kind != GMB_TYPE_NIL
        || gmb_dict_get(*d, "__class__").kind != GMB_TYPE_NIL;
    if (!isDescriptor) return -1;
    gmb_val_t sz = gmb_dict_get(*d, "Size");
    if (sz.kind == GMB_TYPE_INT || sz.kind == GMB_TYPE_DOUBLE) return gmb_sizeof_number(sz);
    /* Istanza: il descrittore del tipo è appeso a __class__. */
    return gmb_sizeof_dict_bytes(gmb_sizeof_as_dict(gmb_dict_get(*d, "__class__")));
}
static void gmb_bf_594(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = -1;
            if (argc >= 1 && args[0].type == 1) bout->result = 8;
            else if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                     args[0].ref->kind == GMB_OBJ_DICT)
                bout->result = gmb_sizeof_dict_bytes(((gmb_obj_shared_val_t*)args[0].ref)->dict);
        }
}
/* Tabella handle per VARPTR/STRPTR su scalari (canone 2026-09-30): gli scalari
 * viaggiano per valore (type==1) quindi non hanno un indirizzo heap stabile; invece di
 * restituire 0 si mappa (type, basic_type, bit del double) su un handle opaco mai-0
 * nello stesso spazio dei backend gestiti (parte da 0x100000). Stabile per contenuto,
 * distinto per contenuti diversi. NON è un indirizzo dereferenziabile. */
#define GMB_VARPTR_TABLE_CAP 1024
typedef struct { uint8_t type; uint8_t basic_type; uint64_t bits; long handle; } gmb_varptr_row_t;
static gmb_varptr_row_t g_varptr_table[GMB_VARPTR_TABLE_CAP];
static size_t g_varptr_used = 0;
static long g_varptr_counter = 0x100000;
static pthread_mutex_t g_varptr_lock = PTHREAD_MUTEX_INITIALIZER;
static long gmb_varptr_scalar_handle(uint8_t type, uint8_t basic_type, double num)
{
    uint64_t bits = 0;
    memcpy(&bits, &num, sizeof(bits));
    if (num != num) bits = UINT64_C(0x7FF8000000000000); /* NaN canonico */
    pthread_mutex_lock(&g_varptr_lock);
    for (size_t i = 0; i < g_varptr_used; i++) {
        if (g_varptr_table[i].type == type && g_varptr_table[i].basic_type == basic_type
            && g_varptr_table[i].bits == bits) {
            long h = g_varptr_table[i].handle;
            pthread_mutex_unlock(&g_varptr_lock);
            return h;
        }
    }
    long h = ++g_varptr_counter;
    if (g_varptr_used < GMB_VARPTR_TABLE_CAP) {
        g_varptr_table[g_varptr_used].type = type;
        g_varptr_table[g_varptr_used].basic_type = basic_type;
        g_varptr_table[g_varptr_used].bits = bits;
        g_varptr_table[g_varptr_used].handle = h;
        g_varptr_used++;
    }
    pthread_mutex_unlock(&g_varptr_lock);
    return h;
}
static void gmb_bf_595(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
                bout->result = (double)(uintptr_t)(args[0].ref);
            else if (argc >= 1)
                bout->result = (double)gmb_varptr_scalar_handle(args[0].type, args[0].basic_type, args[0].num);
            else bout->result = 0;
        }
}
static void gmb_bf_591(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Canone 2026-09-30: SYSTEM.TI() = millisecondi interi (stessa unità di
             * C99Native gmb_timer() e dei backend gestiti). Unica sorgente:
             * il runtime condiviso gmb_timer() (ms dalla mezzanotte); prima erano
             * secondi double (gmb_now_ms()/1000.0). Solo i delta sono portabili. */
            (void)argc;
            bout->result = (double)gmb_timer();
        }
}
static void gmb_bf_592(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t gs = gmb_time_string();
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); free(gs.data); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_593(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        /* SYSTEM.SYS: runtime condiviso gmb_sys_info (OS/ARCH/UI/VERSION/RUNTIME/CPUCOUNT/
         * MACHINE/USER/CWD/SETTINGSPATH + MEMAVAILABLE/MEMTOTAL/MEMUSED/...). Niente logica
         * inline duplicata (AGENTS §0bis). */
        const char* prop = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        const char* v = gmb_sys_info(prop ? prop : "");
        gmb_obj_string_t* s = gmb_str_cstr(v ? v : "");
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_597(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                gmb_rgb_fill(out->dict, args[0].num, args[1].num, args[2].num, argc >= 4 ? args[3].num : 255.0);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_297(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* t = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            int rr = (int)(argc >= 2 ? args[1].num : 0), gg = (int)(argc >= 3 ? args[2].num : 0), bb = (int)(argc >= 4 ? args[3].num : 0);
            if (rr < 0) rr = 0; if (rr > 255) rr = 255;
            if (gg < 0) gg = 0; if (gg > 255) gg = 255;
            if (bb < 0) bb = 0; if (bb > 255) bb = 255;
            /* §0bis: FGRGB/BGRGB usano il runtime condiviso gmb_ansi_fg/bg (unica fonte di
             * verità, rispetta GMBASIC_HEADLESS/isatty). Prima questa funzione costruiva
             * SEMPRE l'ANSI inline, ignorando l'headless → la gamba VM-C divergeva da
             * IL/PCode/C99Native (bug ricorrente). */
            char* colored = NULL;
            const char* out = t;
            if (argc >= 4) {
                colored = (func_code == 297) ? gmb_ansi_fg(t, rr, gg, bb) : gmb_ansi_bg(t, rr, gg, bb);
                if (colored) out = colored;
            }
            gmb_obj_string_t* s = gmb_str_cstr(out);
            free(colored);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_586(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* CELL(row,col,mode): attributo headless — FG=7 (mode<1) / BG=0 (mode>=1),
             * parità IL IlRuntime.CellValue. */
            bout->result = (argc >= 3 && args[2].num >= 1.0) ? 0.0 : 7.0;
        }
}
static void gmb_bf_588(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 1.0;
        }
}
static void gmb_bf_589(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                /* Ordine di inserimento = quello del dict IL/PCode (ModeID,Width,Height,
                 * Bpp,Colors,Active,Description): la stampa di un dict BASIC itera l'ordine
                 * di inserimento, quindi l'ordine VMC precedente rompeva la parità. */
                gmb_dict_set(out->dict, "ModeID", gmb_make_val_double(0));
                gmb_dict_set(out->dict, "Width", gmb_make_val_double(80));
                gmb_dict_set(out->dict, "Height", gmb_make_val_double(25));
                gmb_dict_set(out->dict, "Bpp", gmb_make_val_double(0));
                gmb_dict_set(out->dict, "Colors", gmb_make_val_double(16));
                gmb_dict_set(out->dict, "Active", gmb_make_val_double(-1));
                gmb_dict_set(out->dict, "Description", gmb_make_val_string_counted("Text 80x25"));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_590(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
        }
}
static void gmb_bf_393(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)task;
    if (func_code == 397) {
        /* MOUSE.CLICK(): flag; MOUSE.CLICK(x1,y1,x2,y2): hit-test area (Clipper). */
        bout->result = (argc >= 4)
            ? gmb_mouse_click_area((int)args[0].num, (int)args[1].num, (int)args[2].num, (int)args[3].num)
            : gmb_mouse_click();
        return;
    }
    /* MOUSE getters (MOUSE.X/Y/BUTTON/WHEEL/DBLCLICK/PRESENT/MIDDLE): stato mouse TUI
     * corrente (0 in headless), parità ClipperMouse/oracolo. Tabella di lookup (no switch). */
    static const struct { int code; gmb_double_t (*fn)(void); } t[] = {
        /* 2026-11-06: X/Y mancavano dalla tabella → MOUSE.X/Y non risolte sul VM/C
         * (il C99 e IL/PCode le avevano già). Pixel, 0 in headless. */
        { 393, gmb_mouse_x }, { 394, gmb_mouse_y },
        { 395, gmb_mouse_button }, { 396, gmb_mouse_wheel }, { 398, gmb_mouse_dblclick },
        { 400, gmb_mouse_present }, { 403, gmb_mouse_middle },
        /* 2026-09-22: LEFT/RIGHT/MOVE mancavano → cadevano a 0 (headless coincide, ma
         * MOUSE.LEFT/RIGHT/MOVE mai letti da stato reale). Booleani canonici -1/0. */
        { 399, gmb_mouse_move }, { 401, gmb_mouse_left }, { 402, gmb_mouse_right },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++)
        if (t[i].code == func_code) { bout->result = t[i].fn(); return; }
    bout->result = 0;
}
static void gmb_bf_680(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t k = gmb_console_inkey();
            /* Dispatch eventi ON KEY alla lettura (parita' IL/PCode DispatchKeyEvent):
             * dopo la dequeue il keycode lettura fire l'handler Vchiave (task spawnato). */
            if (vm && task && k.data && k.data[0])
            {
                gmb_vm_onkey_fire(vm, task, (unsigned char)k.data[0]);
            }
            gmb_obj_string_t* s = gmb_str_cstr(k.data ? k.data : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_697(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                bout->result = (double)gmb_dict_count(*((gmb_obj_shared_val_t*)args[0].ref)->dict);
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST)
            {
                bout->result = (double)gmb_list_count(*((gmb_obj_shared_val_t*)args[0].ref)->list);
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_COLLECTION)
            {
                gmb_obj_collection_t* col = (gmb_obj_collection_t*)args[0].ref;
                bout->result = (double)gmb_coll_bag_unique_count(col->handle);
            }
            else bout->result = 0;
        }
}
void gmb_bf_698(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* NET.RECV(handle) → byte disponibili (parità IL IlRuntime.NetRecv = Available). */
            (void)vm; (void)func_code; (void)task;
            int64_t h = (int64_t)(argc >= 1 ? args[0].num : 0);
            bout->result = (double)gmb_net_tcp_available(h);
        }
}
static void gmb_bf_699(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* ALERT(msg): headless → 0 (parità IlRuntime/AlertBox.Show). Prima ritornava 1. */
            (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
            bout->result = 0;
        }
}
static void gmb_bf_700(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* CONFIRM(msg): headless → 0 (parità IlRuntime/AlertBox.Show). */
            (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
            bout->result = 0;
        }
}
static void gmb_bf_671(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* COMPRESSION.ZIP(sources, dest): sources puo' essere un ARRAY di percorsi
             * (forma funzione) o una STRINGA con percorsi separati da ';' (forma comando/
             * statement, che e' quella documentata: "sources: stringa con ';' o array").
             * Prima accettava SOLO l'array: con una stringa restava ns=0 e produceva un
             * archivio VUOTO restituendo SUCCESSO — i pacchetti scaricati dal registry
             * (PackageServer.bas gira sul backend VM/C) arrivavano al client con 0 byte
             * ("Central Directory corrupt"). */
            char* sources[64];
            char* joined = 0;
            int ns = 0;
            int is_list = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST);
            if (is_list)
            {
                gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
                for (size_t i = 0; i < sv->list->count && ns < 64; i++)
                {
                    gmb_val_t v = sv->list->items[i];
                    if (v.kind == GMB_TYPE_STRING && v.v_string.data) sources[ns++] = v.v_string.data;
                }
            }
            else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
            {
                joined = ((gmb_obj_string_t*)args[0].ref)->data;
            }
            const char* dest = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            if (!dest || !*dest || ((!is_list || ns == 0) && !(joined && *joined)))
            {
                /* nessuna sorgente / nessuna destinazione: errore esplicito, non un
                 * archivio vuoto dato per riuscito (era -1 = TRUE a vuoto) */
                bout->result = 0;
            }
            else if (joined)
            {
                bout->result = gmb_zip_files_semicolon(joined, dest) == 0 ? -1 : 0;
            }
            else
            {
                bout->result = gmb_zip_create(dest, sources, ns) == 0 ? -1 : 0;
            }
        }
}
static void gmb_bf_672(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* arch = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* dir = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            bout->result = gmb_zip_extract(arch, dir) == 0 ? -1 : 0;
        }
}
static void gmb_bf_673(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
        }
}
static void gmb_bf_685(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* AI.TRANSCRIBE(audioPath): delega al runtime C condiviso gmb_ai_audio_transcribe
         * (gmb_runtime_c/gmb_ai.c), che usa il backend OCR/audio nativo registrato da
         * gmb_vm_host (gmb_ai_register_ocr_backend) — niente ponte C#. */
        const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        gmb_obj_string_t* s = gmb_str_cstr(gmb_ai_audio_transcribe(path, NULL, 0));
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_686(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char path[1024];
            gmb_paths_providers_json(path, sizeof(path));
            FILE* f = fopen(path, "r");
            gmb_obj_string_t* s;
            if (f)
            {
                char buf[8192];
                size_t n = fread(buf, 1, sizeof(buf) - 1, f);
                buf[n] = '\0';
                fclose(f);
                s = gmb_str_cstr(buf);
            }
            else s = gmb_str_cstr("(no providers)");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_687(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task;
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            /* 687 = AI.GETKEY, 688 = AI.GETMODEL: leggono la config (parita' C# AiGetKey/AiGetModel). */
            const char* v = gmb_ai_get_config(name, func_code == 687 ? "apiKey" : "model");
            gmb_obj_string_t* s = gmb_str_cstr(v ? v : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_ai_setoption(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        /* 3700 = AISETOPTION: scrive una voce del registro opzioni generico (gmb_runtime). */
        const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        const char* val  = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
        gmb_opt_set_persist(name, val);
        gmb_obj_string_t* s = gmb_str_cstr(val ? val : "");
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_ai_option(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        /* 3701 = AIOPTION: legge una voce del registro opzioni generico. */
        const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        char buf[1024]; buf[0] = '\0';
        gmb_opt_get(name, buf, (int)sizeof buf);
        gmb_obj_string_t* s = gmb_str_cstr(buf);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_ai_listoptions(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
        /* 3714 = AILISTOPTIONS: testo delle voci dichiarate del registro opzioni (gmb_runtime). */
        gmb_obj_string_t* s = gmb_str_cstr(gmb_opt_list_text());
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_689(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task;
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            /* 689 = AI.GETTEMP, 690 = AI.GETBASE: leggono la config del provider (parita' C#). */
            const char* key = (func_code == 690) ? "baseUrl" : "temp";
            const char* v = gmb_ai_get_config(name, key);
            gmb_obj_string_t* s = gmb_str_cstr(v ? v : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_691(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* nm = vm_builtin_sarg(args, argc, 0);
            char* info = gmb_ai_info_fmt(nm ? nm : "");
            gmb_obj_string_t* s = gmb_str_cstr(info ? info : "");
            if (info) free(info);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_692(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_string_t* s = gmb_str_cstr("OK");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_694(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task; (void)args; (void)argc;
            /* AI.GETDEFAULT(): default provider del ruolo completion (parita' C# AiGetDefault). */
            const char* d = gmb_ai_get_default("completion");
            gmb_obj_string_t* s = gmb_str_cstr(d ? d : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_481(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* TEAMLIST: se la VM/C è ospitata da un client managed (motore BSP in C) lo stato
             * vero è il suo → l'hook risponde e il registro C non c'entra. */
            if (bsp_hook_str("team.list", NULL, NULL, bout)) return;
            /* TEAMLIST: parità canone C# (string.Join("\n", ...)) — usa gmb_team_list_str,
             * non un join locale con "," (divergenza storica VM/C vs IL/PCode). */
            char* joined = gmb_team_list_str();
            gmb_obj_string_t* s = gmb_str_cstr(joined ? joined : "");
            if (joined) free(joined);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_482(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* TEAMAGENTS(team): l'argomento arriva in args[0] (NON sulla task stack —
             * il pop da task->sp leggeva il vuoto → sempre "" su VM/C). Parità gmb_bf_1260. */
            const char* teamName = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            /* team.agents: formato gestito = stringa unica (virgole) → si passa tal quale. */
            if (bsp_hook_str("team.agents", teamName, NULL, bout)) return;
            char* joined = gmb_team_agents_str(teamName);
            gmb_obj_string_t* s = gmb_str_cstr(joined ? joined : "");
            if (joined) free(joined);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1260(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* teamName = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* json = (func_code == 1260) ? gmb_team_train_status_str(teamName)
                      : (func_code == 1261) ? gmb_team_link_info_str(teamName)
                                            : gmb_team_runtime_stats_str(teamName);
            gmb_obj_string_t* s = gmb_str_cstr(json ? json : "{}");
            if (json) free(json);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_1262(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* teamName = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_team_get_recursion(teamName);
            bout->is_num = 1;
        }
}
static void gmb_bf_1263(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* teamName = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            gmb_obj_string_t* s = gmb_str_cstr(gmb_team_get_latent(teamName));
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_483(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_string_t s;
            if (func_code == 483)
            {
                if (bsp_hook_str("workflow.list", NULL, NULL, bout)) return;
                s = gmb_workflow_list();
            }
            else
            {
                const char* wfName = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                    ? ((gmb_obj_string_t*)args[0].ref)->data : "";
                if (bsp_hook_str(func_code == 484 ? "workflow.status" : "workflow.steps", wfName, NULL, bout)) return;
                s = (func_code == 484) ? gmb_workflow_status(wfName) : gmb_workflow_steps(wfName);
            }
            gmb_obj_string_t* os = gmb_str_cstr(s.data);
            gmb_string_free(&s);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)os; bout->result_ref_bt = GMB_BT_STRING;
        }
}
void gmb_bf_486(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = (func_code == 486 ? gmb_agent_create(name) : gmb_agent_delete(name)) ? -1 : 0;
        }
}
void gmb_bf_488(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* prompt = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            /* L'hook va PRIMA del controllo sul registro C: quando la VM/C è ospitata gli
             * agenti stanno nel runtime managed e il registro C è vuoto (bug visto: AGENTASK
             * rispondeva "not registered" pur essendo l'agente creato). */
            if (bsp_hook_str("agent.ask", name, prompt, bout)) return;
            if (!gmb_agent_exists(name)) {
                char _m[160]; snprintf(_m, sizeof(_m), "ERROR: Agent '%s' is not registered.", name ? name : "");
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(_m); bout->result_ref_bt = GMB_BT_STRING;
                return;
            }
            char* ans = gmb_agent_ask(name, prompt);
            gmb_obj_string_t* r = gmb_str_cstr(ans ? ans : "");
            free(ans);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
void gmb_bf_489(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* key = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            /* Il valore puo' essere stringa OPPURE numero (es. macro &expr che rivaluta
               "0.42" a double): converto il numero a stringa con lo stesso formato usato
               altrove in gmb_builtin.c ("%.15g"). Prima un argomento numerico veniva
               ignorato -> AI AGENT SET n,key,<num> scriveva "" (parita' rotta con IL/PCodeVM,
               bug 2026-09-02). */
            char nbuf[40];
            const char* val = "";
            if (argc >= 3) {
                if (args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING)
                    val = ((gmb_obj_string_t*)args[2].ref)->data;
                else if (args[2].type == 1) {
                    snprintf(nbuf, sizeof(nbuf), "%.15g", args[2].num);
                    val = nbuf;
                }
            }
            bout->result = gmb_agent_set(name, key, val) ? -1 : 0;
        }
}
void gmb_bf_490(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* key = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            if (bsp_hook_str("agent.get", name, key, bout)) return;
            if (!gmb_agent_exists(name)) {
                char _m[160]; snprintf(_m, sizeof(_m), "ERROR: Agent '%s' is not registered.", name ? name : "");
                gmb_obj_string_t* r = gmb_str_cstr(_m);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                return;
            }
            const char* v = gmb_agent_get(name, key);
            gmb_obj_string_t* r = gmb_str_cstr(v ? v : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
/* AI.AGENTRUNGRAPH (1394) / AI.AGENTRUNRAG (1395): il loop graph/rag è in C#; il backend
 * VM/C delega al runtime condiviso (parita' del caso "agente non registrato" col C#). */
void gmb_bf_agent_run_graph_c1394(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* name = vm_builtin_sarg(args, argc, 0);
    const char* prompt = vm_builtin_sarg(args, argc, 1);
    int rounds = (argc >= 3 && args[2].type == 1) ? (int)args[2].num : 5;
    char io[16];
    snprintf(io, sizeof io, "%d", rounds);
    /* AI.AGENTRUNGRAPH(name, prompt[, rounds]) = loop agentico sui tool graph.* — prima qui
     * arrivava SOLO il nome (prompt/rounds ignorati) e il runtime rispondeva sempre
     * "non supportato": firma allineata alla dichiarazione StdLib e stato delegato quando la
     * VM/C è ospitata. */
    if (bsp_hook_str3(func_code == 1395 ? "agent.runrag" : "agent.rungraph", name, prompt, io, bout)) return;
    char* out = gmb_agent_run_graph_json(name, prompt, rounds);
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* AI.AGENTRUNCODE (3744) — loop agentico sul grafo del CODICE (tool code.*). Il loop reale
 * (`gmb_agent_run_code`, runtime agenti C) è agganciato al runtime condiviso via seam;
 * il tool code.* usa `gmb_semquery_file` via seam (host registrato in gmb_vm_host). */
void gmb_bf_agent_run_code_c3744(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* prompt = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    int rounds = (argc >= 3 && args[2].type == 1) ? (int)args[2].num : 5;
    { char io[16]; snprintf(io, sizeof io, "%d", rounds);
      if (bsp_hook_str3("agent.runcode", name, prompt, io, bout)) return; }
    char* out = gmb_agent_run_code_json(name, prompt, rounds);
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* CacheStats(832)/PrefillStats(837)/DecodeStats(838)/AgentFix(1435)/TeamRunWait(1502)/
 * TeamOnStream(1503): parità dei messaggi col C# (nessuna implementazione C equivalente). */
void gmb_bf_stats_c832(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char m[320];
    char* heap = NULL;
    /* Superfici che vivevano nel solo managed (KV cache dell'agente, AI.AGENTFIX): con la VM/C
     * ospitata si delega allo stesso runtime, così la pagina si comporta identicamente; senza
     * client l'hook tace e restano le implementazioni/errori espliciti del motore C. */
    if (func_code == 832) { if (bsp_hook_str("agent.cachestats", n, NULL, bout)) return; }
    else if (func_code == 837) { if (bsp_hook_str("agent.prefillstats", n, NULL, bout)) return; }
    else if (func_code == 838) { if (bsp_hook_str("agent.decodestats", n, NULL, bout)) return; }
    else if (func_code == 1435) { if (bsp_hook_str("agent.fix", n, vm_builtin_sarg(args, argc, 1), bout)) return; }
    if (func_code == 832) snprintf(m, sizeof(m), "{\"error\":\"'%s' non risolve a un contesto KV con completer cache-manager (agente non registrato o fixture)\"}", n ? n : "");
    else if (func_code == 837) heap = gmb_agent_prefill_stats_json(n);
    else if (func_code == 838) heap = gmb_agent_decode_stats_json(n);
    /* TEAMRUNSTART (1501) / TEAMRUNWAIT (1502) / TEAMONSTREAM (1503): prima erano testi
     * FISSI ("Team non trovato" / "handle sconosciuto") — un run asincrono non partiva MAI su
     * VM/C e la Wait rispondeva sempre errore (stub, §0bis). Ora delegano alle funzioni del
     * runtime condiviso, che quando la VM/C è ospitata passano dall'hook di stato (team.run_start
     * / team.run_wait sul runtime managed). */
    else if (func_code == 1501) heap = gmb_team_run_start(n, vm_builtin_sarg(args, argc, 1));
    else if (func_code == 1502) heap = gmb_team_run_wait(n);
    else if (func_code == 1503) snprintf(m, sizeof(m), gmb_team_on_stream(n) ? "1" : "");
    else if (func_code == 1435) snprintf(m, sizeof(m), "Error: Nessun provider LLM configurato (AI SET ACTIVE)");
    else m[0] = 0;
    gmb_obj_string_t* s = gmb_str_cstr(heap ? heap : m);
    free(heap);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_491(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* path = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            bout->result = (func_code == 491 ? gmb_agent_save(name, path) : gmb_agent_load(name, path)) ? -1 : 0;
        }
}
static void gmb_bf_1228(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* prompt = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            char* ans = gmb_ai_analyze_image(path, prompt);
            gmb_obj_string_t* r = gmb_str_cstr(ans ? ans : "");
            free(ans);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
void gmb_bf_1229(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        const char* a0 = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        const char* a1 = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
        const char* a2 = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
        bout->result = (argc == 1) ? gmb_agent_permission_default(a0)
                    : (argc == 3) ? gmb_agent_tool_policy(a0, a1, a2)
                    : gmb_agent_permission(a0, a1) ? -1 : 0;
}
static void gmb_bf_493(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* AI.AGENTREGISTERTOOL (493): era uno STUB che ritornava sempre 0 — il tool binding non
         * veniva mai registrato e l'errore esplicito del runtime ("MCP tool ... not registered")
         * non arrivava mai. Ora delega al runtime condiviso (che su VM/C ospitata passa
         * dall'hook → AgentServer.RegisterTool, con l'eccezione propagata come errore BASIC). */
        (void)vm; (void)func_code;
        const char* rname = vm_builtin_sarg(args, argc, 0);
        const char* rtool = vm_builtin_sarg(args, argc, 1);
        bout->result = gmb_agent_register_tool(rname, rtool) ? -1 : 0;
}
void gmb_bf_494(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* AIAGENTSERVE (494) / AIAGENTSTOPSERVER (495): server HTTP webapp/agent (runtime
         * condiviso gmb_agent_http.c). Prima era uno stub (result=0) → il server non partiva
         * e ogni FETCH in-process verso 127.0.0.1:<port> restituiva 0 (parità 5-backend del
         * sample WebAgentWorkflowsDemo). */
        (void)vm; (void)task;
        if (func_code == 495) {
            bout->result = gmb_agent_http_stop() ? -1 : 0;
            return;
        }
        int port = 8766;
        if (argc >= 1 && args[0].type == 1) port = (int)args[0].num;
        bout->result = gmb_agent_http_start(port) ? -1 : 0;
}
void gmb_bf_496(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (bsp_hook_list("agent.list", NULL, NULL, bout)) return;
            char names[64][64];
            int n = gmb_agent_list(names, 64);
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                for (int i = 0; i < n; i++)
                {
                    gmb_obj_string_t* s = gmb_str_cstr(names[i]);
                    gmb_list_add(out->list, gmb_make_val_string_counted(s->data));
                    gmb_str_release(s);
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
void gmb_bf_497(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            if (bsp_hook_str("agent.status", name, NULL, bout)) return;
            if (!gmb_agent_exists(name)) {
                char _m[160]; snprintf(_m, sizeof(_m), "ERROR: Agent '%s' is not registered.", name ? name : "");
                gmb_obj_string_t* r = gmb_str_cstr(_m);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                return;
            }
            const char* st = gmb_agent_status(name);
            gmb_obj_string_t* r = gmb_str_cstr(st ? st : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
void gmb_bf_498(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* AI AGENT TRACE <ON|OFF|VERBOSE> (parità AgentEventLog.SetMode): prima era uno
             * stub no-op → il trace su stderr non si attivava mai sul VM/C. */
            (void)vm; (void)func_code; (void)task;
            const char* mode = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_agent_trace(mode) ? -1 : 0;
        }
}
void gmb_bf_499(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (bsp_hook_list("agent.events", NULL, NULL, bout)) return;
            char events[64][256];
            int n = gmb_agent_events(events, 64);
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                for (int i = 0; i < n; i++)
                {
                    gmb_obj_string_t* s = gmb_str_cstr(events[i]);
                    gmb_list_add(out->list, gmb_make_val_string_counted(s->data));
                    gmb_str_release(s);
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_409(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* name = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* kind = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            const char* cfg = (argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[3].ref)->data : "";
            /* Forma funzione AI.REGISTER(role, name, kind, config) → parità C#
             * ProviderServer.Register (qualunque kind valido): la validazione sorgente
             * stretta vale solo per la forma statement (c1510). Vedi gmb_ai.c. */
            if (!gmb_provider_register_kind(role, name, kind, cfg))
                vm_error(task, "AI REGISTER: provider non registrato (per i modelli locali usa AI MODEL LOAD)");
            bout->result = -1;
        }
}
static void gmb_bf_410(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        /* 410 = REGISTERLOCAL(path): registra un provider dal path locale con auto-detect
           (runtime C gmb_provider_register_local). Era uno stub che ritornava sempre "OK". */
            const char* p = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            int ok = gmb_provider_register_local(p ? p : "");
            gmb_obj_string_t* s = gmb_str_cstr(ok ? "OK" : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_411(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 411 = PROVIDERCONFIGURE(name, key, value) — parità C# ProviderServer.Configure
       (AI.CONFIGURE); prima usava gmb_provider_config(role,name,kind,cfg) → provider
       cercato per "role"=nome → sempre 0 su AI.CONFIGURE(name,k,v). */
    const char* cname = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* ckey  = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    const char* cval  = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
    bout->result = gmb_provider_configure_name(cname, ckey, cval) ? 1.0 : 0.0;
}
static void gmb_bf_412(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 412 = PROVIDERSETACTIVE(role, name) — era scambiato con CONFIGURE.
     * Il ruolo può essere una STRINGA ("completion") oppure la CAPABILITY NUMERICA
     * (%CHAT=0/%OCR=1/%AUDIO=2/%EMBEDDING=3/%RERANKER=4/%TTS=5): il bytecode A1 pusha il
     * valore numerico del constant, non la stringa. Prima si leggeva solo la stringa →
     * `AI SET ACTIVE %EMBEDDING, "x"` perdeva il ruolo (e il flag embedding esplicito),
     * divergendo da IL/PCode/C99. */
    static const char* g_cap_roles[] = { "completion", "ocr", "audio", "embedding", "reranker", "tts", "decide" };
    const char* role;
    if (argc >= 1 && args[0].type == GMB_BT_STRING && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        role = ((gmb_obj_string_t*)args[0].ref)->data;
    else if (argc >= 1 && args[0].type == GMB_BT_NUMBER && args[0].num >= 0 && args[0].num <= 6)
        role = g_cap_roles[(int)args[0].num];
    else
        role = "";
    const char* name = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    bout->result = gmb_provider_set_active_explicit(role, name) ? 1.0 : 0.0;
}
static void gmb_bf_413(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* n = gmb_provider_rotate(role);
            gmb_obj_string_t* r = gmb_str_cstr(n ? n : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_414(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* ruolo: stringa ("completion") oppure capability numerica (%CHAT=0 … %DECIDE=6), come in 412 */
            static const char* g_cap_roles414[] = { "completion", "ocr", "audio", "embedding", "reranker", "tts", "decide" };
            const char* role = "";
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                role = ((gmb_obj_string_t*)args[0].ref)->data;
            else if (argc >= 1 && args[0].type == GMB_BT_NUMBER && args[0].num >= 0 && args[0].num <= 6)
                role = g_cap_roles414[(int)args[0].num];
            const char* n = gmb_provider_active(role);
            gmb_obj_string_t* r = gmb_str_cstr(n ? n : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_415(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            /* AI.LIST(role) -> LISTA vera (parità string[] C#), non la stringa "[a,b]":
             * con la stringa `DIM x() = AI.LIST(..)` + `x(0)` leggeva "["/91 (AiDbAdmin).
             * Stesso schema di AI.PLUGINLIST (1224). */
            { gmb_list_t pl = gmb_provider_list_names(role); gmb_obj_shared_val_t* plo = gmb_shared_list_new(); if (plo) { for (size_t pi = 0; pi < pl.count; pi++) { gmb_string_t st = gmb_val_to_string(pl.items[pi]); gmb_list_add(plo->list, gmb_make_val_string_counted(st.data)); } } bout->is_num = 0; bout->result_ref = (gmb_obj_t*)plo; bout->result_ref_bt = GMB_BT_OBJECT; }
        }
}
static void gmb_bf_416(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)task; (void)func_code;
        {
            /* 416 = PROVIDERUNREGISTER(name): firma BASIC a 1 arg (ProviderUnregister C#). */
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_provider_unregister_name(name) ? 1.0 : 0.0;
        }
}
static void gmb_bf_417(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 417 = PROVIDERSAVE(path): scrive il registro provider su file (parità C# Save). */
    const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    bout->result = gmb_provider_save_registry(path) ? 1.0 : 0.0;
}
static void gmb_bf_418(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 418 = PROVIDERLOAD(path): SOSTITUISCE il registro col contenuto del file (parità C# Load). */
    const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    bout->result = gmb_provider_load_registry(path) ? 1.0 : 0.0;
}
static void gmb_bf_419(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 419 = PROVIDERTEST(role, name): testa il provider e ritorna una stringa di stato. */
    const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* name = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    char* t = gmb_provider_test(role, name);
    gmb_obj_string_t* r = gmb_str_cstr(t ? t : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_420_c420(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 420 = PROVIDERSETPRIORITY(name, priority): firma BASIC a 2 arg (il valore e' numerico).
       Runtime per-nome gmb_provider_set_priority_name (parita' IL ProviderSetPriority). */
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    int pri = (argc >= 2) ? (int)args[1].num : 0;
    bout->result = gmb_provider_set_priority_name(name, pri) ? 1.0 : 0.0;
}
static void gmb_bf_420_c421(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    /* PRIORITY(name): legge la priorita' del provider per nome (parita' emitter C#
       gmb_provider_priority_name / IL ProviderPriority). Era uno stub vm_error. */
    bout->result = gmb_provider_priority_name(name);
}
static void gmb_bf_420_c422(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 422 = PROVIDERSETWEIGHT(name, weight): firma BASIC a 2 arg. Runtime per-nome. */
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    int w = (argc >= 2) ? (int)args[1].num : 0;
    bout->result = gmb_provider_set_weight_name(name, w) ? 1.0 : 0.0;
}
static void gmb_bf_420_c423(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    /* WEIGHT(name): legge il peso del provider per nome (parita' gmb_provider_weight_name).
       Era uno stub vm_error. */
    bout->result = gmb_provider_weight_name(name);
}
static void gmb_bf_420_c424(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 424 = PROVIDERFAIL(name): firma BASIC a 1 arg — runtime per-nome. */
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    bout->result = gmb_provider_fail_name(name) ? 1.0 : 0.0;
}
static void gmb_bf_420_c425(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 425 = PROVIDERRESETFAILED(name): firma BASIC a 1 arg — runtime per-nome. */
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    bout->result = gmb_provider_reset_failed_name(name) ? 1.0 : 0.0;
}
static void gmb_bf_420_c426(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 426 = PROVIDERSETSTATUS(name, status): firma BASIC (name, status-stringa). */
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* status = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    bout->result = gmb_provider_set_status_name(name, status) ? 1.0 : 0.0;
}
static void gmb_bf_420_c427(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 427 = PROVIDERSTATUS(name): ritorna STRINGA di stato (parita' ProviderStatus C#). */
    const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* st = gmb_provider_status_name(name);
    gmb_obj_string_t* r = gmb_str_cstr(st ? st : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_420_c428(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 428 = STARTHEALTHCHECK(intervalMinutes) — parità C# ProviderStartHealthCheck:
     * imposta l'intervallo del monitor e ritorna 1.0. La forma storica (role,name) resta
     * accettata quando il 1° arg è una stringa. */
    if (argc >= 1 && args[0].type != 2) {
        gmb_provider_set_healthcheck_interval("", "", (int)args[0].num);
        bout->result = 1.0;
        return;
    }
    const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
     const char* name = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    bout->result = gmb_provider_start_healthcheck(role, name) ? 1.0 : 0.0;
}
static void gmb_bf_420_c429(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 429 = STOPHEALTHCHECK(role, name) — runtime reale. */
    const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
     const char* name = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    bout->result = gmb_provider_stop_healthcheck(role, name) ? 1.0 : 0.0;
}
static void gmb_bf_420_c430(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 430 = HEALTHCHECKINTERVAL(role, name) — runtime reale. */
    const char* role = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
     const char* name = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    bout->result = (double)gmb_provider_healthcheck_interval(role, name);
}
static void gmb_bf_420_c431(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code; (void)args; (void)argc;
    /* 431 = RUNHEALTHCHECK() — runtime reale. */
    bout->result = (double)gmb_provider_run_healthcheck();
}
/* func_code 432 (MCPREGISTERTOOL): handler in gmb_mcp_exec.c (vmc_mcp_bf_register_tool)
 * — la firma legacy a 1 argomento è deprecata: la registrazione ora porta
 * proc/desc/readOnly/inputSchema/outputSchema (parità IlRuntime.McpRegisterTool). */
static void gmb_bf_433(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_mcp_unregister_tool(name) ? -1 : 0;
        }
}
static void gmb_bf_434(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* s = gmb_mcp_list_tools();
            gmb_obj_string_t* r = gmb_str_cstr((s && *s) ? s : "[]");
            free(s);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_435(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (func_code == 437 || func_code == 438)
            {
                char* s = func_code == 437 ? gmb_mcp_resources() : gmb_mcp_prompts();
                gmb_obj_string_t* r = gmb_str_cstr(s ? s : "");
                if (s) free(s);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            bout->result = 0;
        }
}
static void gmb_bf_439(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_mcp_register_agent(name) ? -1 : 0;
        }
}
void gmb_bf_440(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t rs = (int64_t)(argc >= 1 ? args[0].num : 0);
            bout->result = (double)gmb_sql_fetch_next(rs);
        }
}
void gmb_bf_441(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int64_t rs = (int64_t)(argc >= 1 ? args[0].num : 0);
            bout->result = (double)(gmb_sql_close_rs(rs) == 0 ? -1 : 0);
        }
}
void gmb_bf_442(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* alias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* sql = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            int pc = argc - 2; if (pc > 13) pc = 13;
            const char* params[13]; char pbuf[13][256];
            for (int i = 0; i < pc; i++)
            {
                char tmp[256];
                gmb_stack_entry_t pe = args[2 + i];
                if (pe.type == 2 && pe.ref) { gmb_obj_string_t* ms = (gmb_obj_string_t*)pe.ref; size_t nn = ms->length < 255 ? ms->length : 255; memcpy(tmp, ms->data, nn); tmp[nn] = 0; }
                else if (pe.type == 1) snprintf(tmp, sizeof(tmp), "%.17g", pe.num);
                else tmp[0] = 0;
                snprintf(pbuf[i], sizeof(pbuf[i]), "%s", tmp); params[i] = pbuf[i];
            }
            bout->result = (double)(gmb_sql_exec_params(alias, sql, pc > 0 ? params : NULL, pc) == 0 ? -1 : 0);
        }
}
void gmb_bf_443(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* alias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = (double)gmb_sql_last_rowid(alias);
        }
}
void gmb_bf_444(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* alias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = (double)gmb_sql_changes(alias);
        }
}
/* G10 (2026-10-09): SQL.DESCRIBE / TOCSV / FROMCSV (ergonomia DataFrame). */
void gmb_bf_sql_describe(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        char buf[8192];
        gmb_sql_describe((int64_t)(argc >= 1 ? args[0].num : 0), buf, (int)sizeof(buf));
        char* s = (char*)malloc(strlen(buf) + 1);
        if (s) strcpy(s, buf);
        vm_builtin_rstr(s, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
void gmb_bf_sql_tocsv(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        bout->result = (double)gmb_sql_to_csv((int64_t)(argc >= 1 ? args[0].num : 0), vm_builtin_sarg(args, argc, 1));
}
void gmb_bf_sql_fromcsv(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        bout->result = (double)gmb_sql_from_csv(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), vm_builtin_sarg(args, argc, 2));
}
/* G10: SQL.COL(rs, col): intera colonna → lista BASIC di stringhe. */
void gmb_bf_sql_col(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        int64_t rs = (int64_t)(argc >= 1 ? args[0].num : 0);
        const char* col = vm_builtin_sarg(args, argc, 1);
        int ncol = gmb_sql_col_count(rs), rows = gmb_sql_row_count(rs), ci = -1, i;
        gmb_obj_shared_val_t* out = gmb_shared_list_new();
        for (i = 0; i < ncol; i++) { char nm[256]; if (gmb_sql_col_name(rs, i, nm, sizeof(nm)) == 0 && strcasecmp(nm, col) == 0) { ci = i; break; } }
        if (out) {
            if (ci >= 0 && rows > 0) for (i = 0; i < rows; i++) {
                gmb_string_t sv = gmb_val_to_string(gmb_sql_val_at(rs, i, ci));
                gmb_list_add(out->list, gmb_make_val_string(sv.data));
            }
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
void gmb_bf_446(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
#ifdef GMB_NATIVE_DB
            if (func_code == 445)
            {
                /* LOOKUP(returnField, searchExpr, searchField [, alias]) = valore di
                 * returnField nel primo record dove searchField = searchExpr (scan del layer
                 * RDD). Prima il codice 445 cadeva nel ramo "speed flush" e ritornava 0
                 * (§ MiniTests/dbf/dbf_lock_filter.bas). */
                const char* retField = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
                const char* searchVal = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                const char* searchField = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
                char luOut[256];
                luOut[0] = 0;
                rdd_select_area(args, argc);
                /* gmb_rdd_locate ritorna 1 = TROVATO (non 0): è così che si posiziona sul
                 * record giusto, poi si legge il campo richiesto. */
                {
                    /* Il layer DBF (aree aperte con DBF USE) non è visibile a gmb_rdd_locate:
                     * si prova prima il runtime DBF condiviso, poi il layer RDD. */
                    int _ok = 0;
                    int64_t _lh = gmb_db_current_dbf();
                    if (_lh > 0 && retField[0] && searchField[0]
                        && gmb_db_dbf_lookup(_lh, retField, searchVal, searchField, luOut, (int)sizeof(luOut)) == GMB_DB_OK)
                        _ok = 1;
                    else if (retField[0] && searchField[0] && gmb_rdd_locate(searchField, searchVal) > 0
                             && gmb_rdd_get_field_by_name(retField, luOut, (int)sizeof(luOut)) == 0)
                        _ok = 1;
                    if (!_ok) luOut[0] = 0;
                }
                gmb_obj_string_t* lur = gmb_str_cstr(luOut);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)lur; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (func_code == 446)
                bout->result = gmb_db_dbf_flush_durability(gmb_db_current_dbf()) == GMB_DB_OK ? -1 : 0;
            else if (func_code == 447 || func_code == 448)
            {
                /* REPLICATION SHIP/RECV via TCP (runtime condiviso gmb_db_c).
                 * StdLib: SHIP(host, port) e SHIP(fileNumber, host, port); RECV(port) e
                 * RECV(fileNumber, port). Il primo argomento può quindi essere l'area:
                 * si prendono gli ULTIMI due (host, port) / l'ULTIMO (port).
                 * Prima questi codici cadevano nel ramo finale (speed flush) → 0. */
                const char* host = "";
                int port = 0;
                if (func_code == 447)
                {
                    int hi = argc >= 2 ? argc - 2 : 0;
                    host = (args[hi].type == 2 && args[hi].ref && args[hi].ref->kind == GMB_OBJ_STRING)
                               ? ((gmb_obj_string_t*)args[hi].ref)->data : "";
                    port = argc >= 2 ? (int)args[argc - 1].num : 0;
                    bout->result = gmb_db_dbf_replication_ship(gmb_db_current_dbf(), host, port);
                }
                else
                {
                    port = argc >= 1 ? (int)args[argc - 1].num : 0;
                    bout->result = gmb_db_dbf_replication_recv(gmb_db_current_dbf(), port);
                }
            }
            else if (func_code == 458)
            {
                const char* mode = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                bout->result = gmb_db_speed_set_durability(gmb_db_current_speed(), mode) == GMB_DB_OK ? -1 : 0;
            }
            else
                bout->result = gmb_db_speed_flush_durability(gmb_db_current_speed()) == GMB_DB_OK ? -1 : 0;
#else
            bout->result = -1; 
#endif
        }
}
static void gmb_bf_459(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* SPEED.VALIDATE([alias][, full]) — BuiltInFunction 459. Integrità del DB SPEED
     * corrente con il runtime condiviso `gmb_db_speed_validate` (§0bis): Bit canonico
     * -1 valido / 0 non valido. Il flag `full` è informativo (la libreria fa sempre
     * l'integrità completa); l'alias è accettato per compatibilità di firma.
     * Prima era uno stub `bout->result = 0` (valor sempre falso). */
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1;
    bout->result = gmb_db_speed_validate(gmb_db_current_speed(), NULL, 0) == GMB_DB_OK ? -1.0 : 0.0;
}
void gmb_bf_449(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* SELECTED(): numero d'area 1-based, come IL/PCode. Le aree DBF della VM/C
             * vivono nel layer gmb_db_* (NON nel RDD, che i comandi DBF non alimentano):
             * area corrente valida con campi → area 1. Prima ritornava -1/0 (un flag RDD).
             * § MiniTests/dbf/dbf_state.bas. */
            const char* alias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            if (argc >= 1 && args[0].type == 1)
            {
                /* SELECTED(<numero>): numero d'area 1-based, 0 se non aperta. Prima il
                 * ramo numerico veniva ignorato e tornava l'area corrente (2 sempre). */
                int an = (int)args[0].num;
                bout->result = (an > 0 && gmb_db_dbf_find_handle_by_area(an) > 0) ? (double)an : 0.0;
                return;
            }
            if (alias && alias[0])
            {
                /* Alias esplicito: numero d'area 1-based via gmb_db_isam_selected (ISAM) o
                 * gmb_selected (DBF/RDD). Prima l'alias era IGNORATO → ISAM.SELECTED(area)=0. */
                int64_t ih = gmb_db_isam_selected(alias);
                int ah = gmb_db_area_of_alias(alias);
                bout->result = ih > 0 ? (double)ih : (ah > 0 ? (double)ah : (double)gmb_selected(alias));
                return;
            }
            int ca = gmb_db_current_area();
            /* Numero d'area 1-based (parità IL/PCode): area DBF assegnata all'apertura,
             * altrimenti area ISAM; senza area aperta → 0. */
            bout->result = ca > 0 ? (double)ca
                         : (gmb_db_current_isam() > 0 ? 1.0 : 0.0);
        }
}
void gmb_bf_450(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (func_code == 449 || func_code == 456)
            {
                const char* alias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
                gmb_obj_string_t* r = gmb_str_cstr(alias ? alias : "");
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
                return;
            }
            if (func_code == 452 || func_code == 453)
            {
                /* ISAM REPLICATION SHIP/RECV via TCP (runtime condiviso gmb_db_c).
                 * Ultimi argomenti = (host, port) / (port): vedi il commento del gruppo DBF.
                 * Prima questi codici ritornavano 0 dal ramo finale. */
                const char* host = "";
                int port = 0;
                if (func_code == 452)
                {
                    int hi = argc >= 2 ? argc - 2 : 0;
                    host = (args[hi].type == 2 && args[hi].ref && args[hi].ref->kind == GMB_OBJ_STRING)
                               ? ((gmb_obj_string_t*)args[hi].ref)->data : "";
                    port = argc >= 2 ? (int)args[argc - 1].num : 0;
                    bout->result = gmb_db_isam_replication_ship(gmb_db_current_isam(), host, port);
                }
                else
                {
                    port = argc >= 1 ? (int)args[argc - 1].num : 0;
                    bout->result = gmb_db_isam_replication_recv(gmb_db_current_isam(), port);
                }
                return;
            }
            bout->result = 0;
        }
}
void gmb_bf_466(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            /* LEDGER.COUNT/LASTREC usano i token CONDIVISI Count/Lastrec: se l'argomento è
             * l'handle del ledger corrente si ritorna il numero di transazioni (prima questi
             * codici cadevano nei rami DBF/collezioni → 0). § MiniTests/ledger/ledger_ops.bas. */
            /* Il ledger deve essere APERTO (handle > 0): senza ledger gmb_db_current_ledger()
             * vale 0, e un argomento che e' un IDENTIFICATORE (alias d'area, es. `ISAM.LASTREC(X)`)
             * si compila a 0 → il confronto 0==0 faceva entrare l'area ISAM nel ramo LEDGER e
             * LASTREC/COUNT ritornavano gmb_db_ledger_count(0) = 0 invece del conteggio d'area
             * (misurato su VM/C: ISAM.LASTREC(X)=0 contro 3, § minitests/dbf/isam_commands.bas). */
            int64_t lbq = gmb_db_current_ledger();
            if ((func_code == 328 || func_code == 469) && lbq > 0 && argc >= 1 && args[0].type == 1
                && (int64_t)args[0].num == lbq)
            {
                bout->result = (double)gmb_db_ledger_count(lbq);
                return;
            }
            /* BOF(467)/FOUND(468) con l'handle del ledger corrente → cursore del journal
             * (stessa classe di COUNT/LASTREC sopra): prima cadevano nei rami DBF → 0,
             * mentre IL ritorna -1/0. */
            /* Stessa guardia del ledger: handle 0 (nessun ledger) non deve combaciare con un
             * argomento identificatore compilato a 0 (BOF(X)/FOUND(X) su area ISAM). */
            int64_t lbo = gmb_db_current_ledger();
            if ((func_code == 467 || func_code == 468) && lbo > 0 && argc >= 1 && args[0].type == 1
                && (int64_t)args[0].num == lbo)
            {
                bout->result = (func_code == 467 ? (gmb_db_ledger_bof(lbo) ? -1 : 0)
                                                 : (gmb_db_ledger_found(lbo) ? -1 : 0));
                return;
            }

        {
            /* Codici dall'enum BuiltInFunction C# (la tabella li instrada QUI):
             * 468 FOUND · 467 BOF · 472 USED · 477 DELETED · 479 RECSIZE (+ EOF/LASTREC sotto).
             * PRIMA i rami erano disallineati (471 per USED, 476 per DELETED — instradati
             * altrove) e BOF/RECSIZE mancavano: l'area DBF restava muta, USED() ritornava 0 con
             * un'area aperta e DELETED() sempre 0 (§ MiniTests/dbf/dbf_state.bas). */
            const char* dbfAlias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            /* FOUND/BOF: come RECNO/EOF, se non c'è un'area DBF ma un'area RDD è aperta si
             * legge QUELLA (i comandi DBF GO/SKIP navigano l'area RDD quando il layer gmb_db
             * non ha handle: senza questo l'area RDD risultava "non trovata"/"a BOF" per sempre). */
            if (func_code == 468)
                /* FOUND: se l'area corrente e' ISAM (B+Tree), l'esito del SEEK va letto dal
                 * motore ISAM — non dal work-area DBF (0 → sempre false, il GET post-SEEK
                 * non veniva eseguito). Parita' IL/PCode. */
                bout->result = (gmb_db_current_isam() > 0) ? (gmb_db_isam_found(gmb_db_current_isam()) ? -1 : 0)
                    : ((gmb_db_current_dbf() > 0) ? (gmb_db_dbf_found(gmb_db_current_dbf()) ? -1 : 0)
                        : ((gmb_rdd_used()) ? (gmb_rdd_found() ? -1 : 0)
                                            : (gmb_dbf_found() ? -1 : 0)));
            else if (func_code == 467)
                bout->result = (gmb_db_current_dbf() > 0) ? (gmb_db_dbf_bof(gmb_db_current_dbf()) ? -1 : 0)
                    : ((gmb_rdd_used()) ? (gmb_rdd_bof() ? -1 : 0)
                                        : (gmb_dbf_bof() ? -1 : 0));
            else if (func_code == 472)
            {
                /* USED(): c'e' un'area DBF aperta e utilizzabile (campo presente)? Lo stato
                 * delle aree nella VM/C vive nel layer gmb_db_*, non nel RDD ne' nella
                 * work-area condivisa (gmb_dbf_used() chiama wa_or_new, che CREA l'area:
                 * ritornava sempre vero). § MiniTests/dbf/dbf_state.bas. */
                int64_t usedh = gmb_db_current_dbf();
                if (dbfAlias && dbfAlias[0])
                    /* USED(alias): l'area è usata se l'alias è un DBF O un ISAM aperto.
                     * Prima l'alias ISAM era ignorato → ISAM.USED("X")=0 contro -1 di IL. */
                    bout->result = (gmb_db_dbf_find_handle_by_alias(dbfAlias) > 0
                                    || gmb_db_isam_handle_for(dbfAlias) > 0) ? -1 : 0;
                else if (usedh > 0)
                    bout->result = gmb_db_dbf_field_count(usedh) > 0 ? -1 : 0;
                else if (gmb_db_current_isam() > 0)
                    /* ISAM.USED(): area B+Tree corrente aperta (il layer gmb_db non ha DBF). */
                    bout->result = -1;
                else
                    /* RDD.USED() (e DBF.USED() su un'area aperta via RDD USE): l'area del driver
                     * RDD. `RDD.USED()` ritornava 0 con l'area aperta (misurato con un probe
                     * minimo: IL -1, VM/C 0). */
                    bout->result = gmb_rdd_used() ? -1 : 0;
            }
            else if (func_code == 477) bout->result = (gmb_db_current_dbf() <= 0 && gmb_rdd_used())   /* area RDD: marcatore RDD (RDD DELETE) */
                ? (gmb_rdd_deleted() ? -1 : 0)
                : (gmb_db_current_dbf() > 0)
                ? (gmb_db_dbf_deleted(gmb_db_current_dbf()) ? -1 : 0)
                : (gmb_dbf_deleted(dbfAlias) ? -1 : 0);
            else if (func_code == 479)
            {
                /* RECSIZE([alias]): se l'alias è un'area ISAM usa la lunghezza record del motore
                 * B+Tree (gmb_db_isam_recsize = schema byte size, incl. metadata); altrimenti DBF.
                 * Prima usava solo gmb_db_dbf_record_size -> ISAM.RECSIZE = -1. */
                int64_t _rh = (dbfAlias && dbfAlias[0]) ? gmb_db_isam_handle_for(dbfAlias) : gmb_db_current_isam();
                if (_rh > 0) bout->result = (double)gmb_db_isam_recsize(_rh);
                else bout->result = (double)gmb_db_dbf_record_size(gmb_db_current_dbf());
            }
            else if (func_code == 466)
            {
                /* EOF è condiviso tra DBF/ISAM/RDD (EOF dell'AREA) e IO (EOF di un FILE):
                 * decide l'ARGOMENTO. Numerico = handle di FOpen/FCreate (o n° di file
                 * legacy) -> gmb_file_eof, la funzione canonica del runtime condiviso
                 * (la stessa usata da C99/C99Native, §0bis). Stringa o assente = area
                 * DBF/ISAM -> gmb_dbf_eof().
                 * PRIMA qualunque argomento cadeva su gmb_dbf_eof(): IO.EOF(handle)
                 * ritornava l'EOF dell'AREA DBF invece di quello del file, quindi era
                 * sempre true e divergeva da IL (Position >= Length). Trovato da
                 * MiniTests/io/io_handles.bas.
                 * NOTA CODICE: EOF è **466** (BuiltInFunction.EOFFUNC). Prima il ramo era
                 * scritto su 465, che la tabella instrada a gmb_bf_462 (che ritorna 0): il
                 * ramo era CODICE MORTO e DBF.EOF() valeva sempre 0 → i cicli
                 * `DO WHILE NOT DBF.EOF()` non terminavano (misurato: RDD_ReplaceRealData.bas
                 * stampava 1.048.579 righe e poi "operand stack overflow"). */
                if (argc >= 1 && args[0].type == 1)
                {
                    /* `EOF(n)`: n è il FILE NUMBER (indice di g_vm_file_handles, come PRINT#/
                     * INPUT# gestiti da gmb_syscall_fileio). Va mappato all'HANDLE prima di
                     * gmb_file_eof: senza, gmb_file_eof(n) usava il runtime gmb_file_handles[n-1]
                     * (lo SLOT del runtime, diverso dal file number) → EOF=true PRIMA della
                     * lettura (VM/C eof0=-1; il/c99n danno 0). */
                    int eofIdx = (int)args[0].num;
                    int eofH = (eofIdx >= 0 && eofIdx < 64 && g_vm_file_handles[eofIdx] > 0)
                        ? (int)g_vm_file_handles[eofIdx] : eofIdx;
                    bout->result = gmb_file_eof((gmb_int_t)eofH) ? -1 : 0;
                }
                else if (dbfAlias[0] && gmb_db_speed_table_by_alias(dbfAlias) >= 0)
                    /* SPEED.EOF(alias): cursore dell'area SPEED (stesso registro alias di
                     * SPEED PUT/GET). Prima cadeva su gmb_dbf_eof() → sempre 0 → i loop
                     * `DO WHILE NOT SPEED.EOF(T)` non finivano mai. */
                    bout->result = gmb_db_speed_eof(gmb_db_current_speed(), gmb_db_speed_table_by_alias(dbfAlias)) ? -1 : 0;
                else if (gmb_db_current_isam() > 0)
                    /* EOF dell'area ISAM corrente (motore B+Tree). */
                    bout->result = gmb_db_isam_eof(gmb_db_current_isam()) ? -1 : 0;
                else if (gmb_db_current_dbf() > 0)
                    /* EOF dell'area DBF corrente (motore gmb_db): `gmb_dbf_eof()` legge il
                     * work-area RDD legacy (g_wa), che per un'area DBF è vuoto → sempre 0 →
                     * i cicli `DO WHILE NOT DBF.EOF()` non terminavano (1M righe "0" + operand
                     * stack overflow). */
                    bout->result = gmb_db_dbf_eof(gmb_db_current_dbf()) ? -1 : 0;
                else if (gmb_rdd_used())
                    /* Navigazione su un'area RDD (RDD USE): GO/SKIP muovono l'area RDD (vedi
                     * vm_sys_goskip_128/129). */
                    bout->result = gmb_rdd_eof() ? -1 : 0;
                else
                    bout->result = gmb_dbf_eof() ? -1 : 0;
            }
            else if (func_code == 467) bout->result = gmb_dbf_found() ? -1 : 0;
            else if (func_code == 469) 
            {
                /* LASTREC([alias]): l'area va risolta dall'ARGOMENTO quando c'e', come fa IL
                 * (IlRuntime.DbfLastrec -> DbfHandleFor/IsamHandleFor): PRIMA l'alias era
                 * ignorato e si leggeva sempre l'area CORRENTE, quindi `ISAM.LASTREC(A)` su
                 * due aree aperte dava il conteggio di quella corrente. Alias sconosciuto →
                 * fallback all'area corrente (stesso comportamento di IsamHandleFor).
                 * Se l'area corrente e' ISAM (motore B+Tree, gmb_db_isam_*), il conteggio va
                 * letto dal motore ISAM, NON dal cell-store RDD generico (gmb_rdd_lastrec, che
                 * per un'area ISAM e' sempre 0). Parita' IL/PCode. */
                if (dbfAlias && dbfAlias[0])
                {
                    char aq[80];
                    int qn = 0;
                    const char* ap = dbfAlias;
                    while (*ap == ' ' || *ap == '"' || *ap == '\'') ap++;
                    while (*ap && qn < (int)sizeof(aq) - 1) aq[qn++] = *ap++;
                    while (qn > 0 && (aq[qn - 1] == ' ' || aq[qn - 1] == '"' || aq[qn - 1] == '\'')) qn--;
                    aq[qn] = 0;
                    int64_t adh = aq[0] ? gmb_db_dbf_find_handle_by_alias(aq) : 0;
                    int64_t aih = (adh > 0 || !aq[0]) ? 0 : gmb_db_isam_handle_for(aq);
                    if (adh > 0) { bout->result = (double)gmb_db_dbf_lastrec(adh); return; }
                    if (aih > 0) { bout->result = (double)gmb_db_isam_count(aih); return; }
                }
                if (gmb_db_current_isam() > 0)
                    bout->result = (double)gmb_db_isam_count(gmb_db_current_isam());
                else if (gmb_db_current_dbf() > 0)
                    /* Area DBF aperta col motore gmb_db: il conteggio sta nell'engine
                     * (gmb_rdd_lastrec è il cell-store legacy e per un'area DBF è sempre 0 →
                     * `Totale record nel database: 0` pur avendo 7 APPEND, ClipperDbfLateBinding
                     * gamba VM/C). */
                    bout->result = (double)gmb_db_dbf_lastrec(gmb_db_current_dbf());
                else
                {
                    rdd_select_area(args, argc);
                    bout->result = (double)gmb_rdd_lastrec();
                }
            }
            else bout->result = 0;
        }
}
/* Alias dell'ultima area DBF aperta (handler DBF USE, gmb_syscall_db.c). */
const char* gmb_vm_dbf_current_alias(void);

void gmb_bf_473(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Stringhe DBF (la tabella instrada qui 473 DBFILTER · 474 DBRELATION · 475 INDEXKEY
             * · 478 FIELDNAME · 480 ALIAS). PRIMA questo handler gestiva solo i codici 477/479,
             * che la tabella instrada ALTROVE: FIELDNAME/DBFILTER/DBRELATION/ALIAS ritornavano
             * sempre "" (§ MiniTests/dbf/dbf_state.bas). */
            const char* alias = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            static char fbuf[256];
            const char* res = "";
            int64_t dh = gmb_db_current_dbf();
            if (func_code == 478)
            {
                /* FIELDNAME(fieldNum [, area]): 1-BASED come in Clipper; la API C e' 0-based. */
                int fidx = (argc >= 1 && args[0].type == 1) ? (int)args[0].num - 1 : -1;
                if (fidx >= 0 && gmb_db_dbf_field_name(dh, fidx, fbuf, (int)sizeof(fbuf)) == GMB_DB_OK) res = fbuf;
            }
            else if (func_code == 473)
            {
                gmb_string_t fl = gmb_db_dbf_get_filter_str(dh);
                snprintf(fbuf, sizeof(fbuf), "%s", fl.data ? fl.data : "");
                res = fbuf;
            }
            else if (func_code == 474) { gmb_db_dbf_relation_key(dh, fbuf, (int)sizeof(fbuf)); res = fbuf; }
            else if (func_code == 480)
            {
                /* ALIAS(): alias dell'AREA CORRENTE. Prima la VM/C usava solo l'ultima
                 * area aperta con DBF USE → dopo `DBF SELECT Clienti` continuava a
                 * rispondere "Ordini" (divergenza IL/PCode/C99, sample SelectAreas). Il
                 * layer DBF conosce l'alias per handle; se vuoto (USE senza ALIAS / area
                 * RDD) si ripiega sul registro storico. */
                gmb_string_t al = gmb_db_dbf_get_alias_str(dh);
                if (al.data && al.data[0]) snprintf(fbuf, sizeof(fbuf), "%s", al.data);
                else {
                    const char* va = gmb_vm_dbf_current_alias();
                    snprintf(fbuf, sizeof(fbuf), "%s", va ? va : "");
                }
                if (al.data) gmb_string_free(&al);
                res = fbuf;
            }
            /* 475 INDEXKEY: campo chiave dell'ordine attivo (vuoto se nessun ordine). */
            else if (func_code == 475)
            {
                gmb_string_t ik = gmb_indexkey(0);
                snprintf(fbuf, sizeof(fbuf), "%s", ik.data ? ik.data : "");
                res = fbuf;
            }
            gmb_obj_string_t* r = gmb_str_cstr(res);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_476(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
        {
            /* INDEXORD(): numero d'ordine attivo dell'area DBF corrente (0 = ordine fisico),
             * letto dal runtime DBF condiviso (stesso stato di DBF SET ORDER). */
            bout->result = gmb_indexord_dbl(0);
        }
}
static void gmb_bf_623(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
        }
}
static void gmb_bf_500(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* F1-followup (2026-09-11): il codice 500 è AI.EMBED (AIVECTOREMBED), NON
         * EnsureCollection (501): la branch 500 eseguiva gmb_rag_ensure_collection →
         * AI.EMBED restituiva 1/0 invece dell'embedding (fallback silenzioso). Ora
         * costruisce la lista double come IL/PCode/C99 (gmb_aiv_embed ha fallback
         * deterministico, nessun backend richiesto). */
        if (func_code == 500)
        {
            const char* text = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            int dims = (argc >= 2 && args[1].type == 1) ? (int)args[1].num : 0;
            gmb_int_t n = 0;
            double* e = gmb_aiv_embed(text, dims, &n);
            if (!e)
            {
                /* Nessun provider embedding reale → errore ESPLICITO (parità IL:
                 * InvalidOperationException "Operazione AI RAG richiede un provider
                 * embedding attivo (AI SET ACTIVE %EMBEDDING, alias)"). Prima costruiva
                 * una lista VUOTA (fallback silenzioso) e il programma proseguiva —
                 * divergenza da IL/C99 (SqlVecSearch). */
                vm_error(task, "Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)");
                return;
            }
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && e) { for (gmb_int_t i = 0; i < n; i++) gmb_list_add(out->list, gmb_make_val_double(e[i])); }
            free(e);
            bout->is_num = 0;
            bout->result_ref = out ? (gmb_obj_t*)out : NULL;
            bout->result_ref_bt = GMB_BT_OBJECT;
            return;
        }
        {
            const char* db = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* coll = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* src = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            const char* text = (argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[3].ref)->data : "";
            /* P0f (todo Tokenizer-WordPiece 2026-09-12): 503 INDEX META ora usa la superficie
             * gmb_rag_* con source_id e metadata (prima: no-op ramificato a 0). */
            if (func_code == 503)
            {
                const char* meta = (argc >= 5 && args[4].type == 2 && args[4].ref && args[4].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[4].ref)->data : NULL;
                /* §0bis: usa la superficie condivisa gmb_aiv_index_meta, che mappa ai codici C#
                 * IndexTextOutcome (Indexed=0, Updated=1, Skipped=2). Prima r>0?r:... produceva
                 * 1/2/0 (invertito) → divergenza da IL/PCode/C99. */
                bout->result = (double)gmb_aiv_index_meta(db, coll, src, text, meta);
                return;
            }
            if (func_code == 501) bout->result = (double)gmb_rag_ensure_collection_fmt(db, coll, argc >= 3 ? (int)args[2].num : 0, "cosine", NULL, argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_STRING ? ((gmb_obj_string_t*)args[3].ref)->data : NULL);
            else if (func_code == 502) bout->result = (double)gmb_aiv_index_source(db, coll, src, text);
            /* P0f: 505 PURGE DEPRECATED ora chiama gmb_rag_purge_deprecated (prima: no-op → 0). */
            else if (func_code == 505) bout->result = (double)gmb_rag_purge_deprecated(db, coll);
            else if (func_code == 504) { gmb_aiv_remove_source(db, coll, src); bout->result = 1.0; }   /* C# AiVectorRemoveSource: 1.0 */
            else
            {
                /* P0f: mai più risultato silenzioso — stesso modello di gmb_vec_default. */
                char rmsg[96];
                snprintf(rmsg, sizeof(rmsg), "builtin %d non implementato su VM/C", func_code);
                vm_error(task, rmsg);
            }
        }
}
static void gmb_bf_506(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* db = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* coll = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            bout->result = (double)gmb_rag_count(db, coll);
        }
}
static void gmb_bf_507(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* db = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* coll = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* q = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            int topk = argc >= 4 && args[3].type == 1 ? (int)args[3].num : 5;
            int hybrid = argc >= 5 && args[4].type == 1 ? (int)args[4].num : 1;
            const char* filter = (argc >= 6 && args[5].type == 2 && args[5].ref && args[5].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[5].ref)->data : NULL;
            int rerank = argc >= 7 && args[6].type == 1 ? (int)args[6].num : 1;
            double mev = argc >= 8 && args[7].type == 1 ? args[7].num : (double)NAN;
            int explain = argc >= 9 && args[8].type == 1 ? (int)args[8].num : 0;
            char* s = gmb_rag_search_sel(db, coll, q, topk, hybrid, filter, rerank, mev, explain);
            gmb_obj_string_t* r = gmb_str_cstr(s ? s : "");
            free(s);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_509(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* db = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* coll = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* q = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            int topk = argc >= 4 && args[3].type == 1 ? (int)args[3].num : 5;
            int rerank = argc >= 5 && args[4].type == 1 ? (int)args[4].num : 1;
            double mev = argc >= 6 && args[5].type == 1 ? args[5].num : (double)NAN;
            char* s = gmb_rag_ask_sel(db, coll, q, topk, rerank, mev);
            gmb_obj_string_t* r = gmb_str_cstr(s ? s : "");
            free(s);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_ragcontext_508(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)func_code;
            const char* db = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* coll = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* q = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            int topk = argc >= 4 && args[3].type == 1 ? (int)args[3].num : 5;
            int rerank = argc >= 5 && args[4].type == 1 ? (int)args[4].num : 1;
            double mev = argc >= 6 && args[5].type == 1 ? args[5].num : (double)NAN;
            char* s = gmb_rag_context_sel(db, coll, q, topk, rerank, mev);
            gmb_obj_string_t* r = gmb_str_cstr(s ? s : "");
            free(s);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_510(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* db = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            if (func_code == 510) {
                /* LOADDOCUMENTS(db, coll, path, glob) -> conteggio documenti caricati */
                const char* coll = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
                const char* path = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
                const char* glob = (argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[3].ref)->data : NULL;
                bout->result = (double)gmb_aiv_load_documents(db, coll, path, glob);
                return;
            }
            char* s = gmb_rag_list_collections(db);
            gmb_obj_string_t* r = gmb_str_cstr(s ? s : "");
            free(s);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_512(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)task; (void)func_code;
            /* AI.RERANK(query, json, topK): rescoring keyword + shape PascalCase via
             * gmb_ai_rerank (parità C#). Prima: passthrough del json (query ignorata). */
            const char* q = vm_builtin_sarg(args, argc, 0);
            const char* json = vm_builtin_sarg(args, argc, 1);
            int topk = (argc >= 3 && args[2].type == 1) ? (int)args[2].num : 5;
            char* r = gmb_ai_rerank(q ? q : "", json ? json : "", topk);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
            free(r);
        }
}
void gmb_bf_556(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (func_code == 559)
            {
                
                char ipbuf[64] = "";
                gmb_net_local_host(ipbuf, sizeof(ipbuf));
                gmb_obj_string_t* s = gmb_str_cstr(ipbuf);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
            if (func_code == 557)
            {
                
                int64_t h = (int64_t)(argc >= 1 ? args[0].num : 0);
                char ipbuf[64] = "";
                gmb_net_remote_ip(h, ipbuf, sizeof(ipbuf));
                gmb_obj_string_t* s = gmb_str_cstr(ipbuf);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
            }
            if (func_code == 558)
            {
                
                int64_t h = (int64_t)(argc >= 1 ? args[0].num : 0);
                bout->result = gmb_net_remote_port(h);
            }
            if (func_code == 560)
            {
                
                int64_t h = (int64_t)(argc >= 1 ? args[0].num : 0);
                bout->result = gmb_net_local_port(h);
            }
            if (func_code == 556)
            {
                /* CONN(h): booleano connessione. Non deve sovrascrivere bout->result
                   di REMOTEPORT(558)/LOCALPORT(560), che condividono l'handler. */
                int64_t hc = (int64_t)(argc >= 1 ? args[0].num : 0);
                bout->result = gmb_net_conn(hc) ? 1 : 0;
            }
        }
}
static void gmb_bf_565(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
        }
}
void gmb_bf_585(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* POINT(x,y): indice colore pixel dalla pagina software C; -1 se schermo off. */
            (void)vm; (void)task; (void)func_code;
            if (argc >= 2 && gmb_gfx_screen_on())
                bout->result = (double)gmb_gfx_point((int)args[0].num, (int)args[1].num);
            else
                bout->result = -1.0;
        }
}
static void gmb_bf_361(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* KEY.MAKE(key, mod) → intero (parità IL IlRuntime.KeyMake). */
            bout->result = argc >= 1 ? (double)(int)args[0].num : 0.0;
        }
}
void gmb_bf_464(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int jid = (argc >= 1) ? (int)args[0].num : 0;
            const char* nm = gmb_joy_get_name(jid);
            gmb_obj_string_t* s = gmb_str_cstr(nm ? nm : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_462(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = 0;
        }
}
void gmb_bf_461(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* COLLIDEBATCH(xArr, yArr, w, h) → lista piatta [i0,j0,i1,j1,...] (parità
         * IL/PCode/C99: prima era una lista VUOTA → nessuna collisione nella gamba VM/C). */
        (void)vm; (void)func_code; (void)task;
        gmb_obj_shared_val_t* out = gmb_shared_list_new();
        if (!out) { bout->is_num = 0; bout->result_ref = NULL; return; }
        {
            gmb_obj_shared_val_t* ax = (argc >= 1 && args[0].type == 2) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* ay = (argc >= 2 && args[1].type == 2) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
            int nx = (ax && ax->list) ? (int)gmb_list_count(*ax->list) : 0;
            int ny = (ay && ay->list) ? (int)gmb_list_count(*ay->list) : 0;
            double* xs = (nx > 0) ? (double*)malloc((size_t)nx * sizeof(double)) : NULL;
            double* ys = (ny > 0) ? (double*)malloc((size_t)ny * sizeof(double)) : NULL;
            if (xs) for (int i = 0; i < nx; i++) xs[i] = gmb_val_to_double(gmb_list_get(*ax->list, i));
            if (ys) for (int i = 0; i < ny; i++) ys[i] = gmb_val_to_double(gmb_list_get(*ay->list, i));
            double dw = (argc >= 3) ? args[2].num : 0.0;
            double dh = (argc >= 4) ? args[3].num : 0.0;
            int nout = 0;
            double* pairs = gmb_gfx_collide_batch(xs, nx, ys, ny, dw, dh, &nout);
            for (int k = 0; k < nout; k++) gmb_list_add(out->list, gmb_make_val_double(pairs[k]));
            free(xs); free(ys);
        }
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
}
static void gmb_bf_674(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                gmb_list_add(out->list, gmb_make_val_double(0));
                gmb_list_add(out->list, gmb_make_val_double(0));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_675(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                gmb_list_add(out->list, gmb_make_val_double(80));
                gmb_list_add(out->list, gmb_make_val_double(24));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_676(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* GETPALETTECOLOR(index) → [r,g,b] dalla palette VGA16 (parità IlRuntime.GetPaletteColor).
             * Prima ritornava sempre [0,0,0] ignorando l'indice. */
            (void)vm; (void)func_code; (void)task;
            int idx = (argc >= 1) ? (int)args[0].num : 0;
            int r = 0, g = 0, b = 0;
            gmb_console_palette_color(idx, &r, &g, &b);
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                gmb_list_add(out->list, gmb_make_val_double((double)r));
                gmb_list_add(out->list, gmb_make_val_double((double)g));
                gmb_list_add(out->list, gmb_make_val_double((double)b));
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_598(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
                bout->result = (double)((gmb_obj_shared_val_t*)args[0].ref)->dict->count;
            else bout->result = 0;
        }
}
static void gmb_bf_650(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char tnbuf[128]; tnbuf[0] = 0;
            /* NIL VM/C = (type 0 "vuoto") oppure (type 2 con ref NULL). Parità
             * IlRuntime.TypeInfo(null) → "NIL" (prima ritornava "NULL"). */
            int is_nil = (argc < 1) || (args[0].type == 0) || (args[0].type == 2 && args[0].ref == NULL);
            const char* tn = is_nil ? "NIL" : "NULL";
            if (!is_nil && args[0].type == 1)
                tn = args[0].basic_type == GMB_BT_BOOL ? "BIT"
                   : args[0].basic_type == GMB_BT_CURRENCY ? "CURRENCY" : "DOUBLE";
            else if (!is_nil && args[0].type == 2 && args[0].ref)
            {
                switch (args[0].ref->kind)
                {
                    case GMB_OBJ_STRING: tn = "STRING"; break;
                    case GMB_OBJ_LIST: {
                        /* TYPEINFO (≠ TYPENAME): lista/array → "LIST OF <elem>"
                         * ("LIST OF ANY" se vuota), parità IlRuntime.TypeInfo. */
                        gmb_string_t ts = gmb_typeinfo_val(gmb_make_val_obj(args[0].ref));
                        snprintf(tnbuf, sizeof(tnbuf), "%s", ts.data ? ts.data : "OBJECT");
                        gmb_string_free(&ts);
                        tn = tnbuf;
                        break;
                    }
                    case GMB_OBJ_COLLECTION: {
                        gmb_obj_collection_t* cc = (gmb_obj_collection_t*)args[0].ref;
                        int tc = cc->type_code;
                        if (tc == 2 || tc == 6 || tc == 7 || tc == 8 || tc == 9 || tc == 10
                            || tc == 15 || tc == 26) tn = "DICT";
                        else if (tc == 1 || tc == 13 || tc == 14 || tc == 27) {
                            gmb_val_t e0 = gmb_coll_first(cc->handle);
                            if (e0.kind == GMB_TYPE_NIL) snprintf(tnbuf, sizeof(tnbuf), "LIST OF ANY");
                            else {
                                gmb_string_t es = gmb_typeinfo_val(e0);
                                snprintf(tnbuf, sizeof(tnbuf), "LIST OF %s", es.data ? es.data : "ANY");
                                gmb_string_free(&es);
                            }
                            tn = tnbuf;
                        } else tn = gmb_collection_type_name(tc);
                        break;
                    }
                    case GMB_OBJ_GRAPH: tn = "GRAPH`1"; break;
                    case GMB_OBJ_DICT:
                    {
                        /* Istanza UDT/CLASS: `__class__.Name` → nome del TYPE (parità
                         * IlRuntime.TypeInfo → "Person"). Prima ritornava sempre "DICT". */
                        tn = "DICT";
                        gmb_obj_shared_val_t* dd = (gmb_obj_shared_val_t*)args[0].ref;
                        if (dd->dict) {
                            gmb_val_t cv = gmb_dict_get(*dd->dict, "__class__");
                            if (cv.kind == GMB_TYPE_OBJECT && cv.v_ptr && ((gmb_obj_t*)cv.v_ptr)->kind == GMB_OBJ_DICT) {
                                gmb_obj_shared_val_t* cd = (gmb_obj_shared_val_t*)cv.v_ptr;
                                if (cd->dict) {
                                    gmb_val_t nv = gmb_dict_get(*cd->dict, "Name");
                                    if (nv.kind == GMB_TYPE_STRING && nv.v_string.data && nv.v_string.data[0]) {
                                        snprintf(tnbuf, sizeof(tnbuf), "%s", nv.v_string.data);
                                        tn = tnbuf;
                                    }
                                }
                            }
                        }
                        break;
                    }
                    case GMB_OBJ_GENERATOR: tn = "GENERATOR"; break;
                    default: tn = "OBJECT";
                }
            }
            gmb_obj_string_t* s = gmb_str_cstr(tn);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_651(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Parità IlRuntime.TypeInfoHas: `value != null ? 1 : 0`. Un NIL su VM/C è
             * (type 0) oppure (type 2 con ref NULL) — NON basta `type != 0` (un NIL è
             * type 2/ref NULL), né `ref != NULL` (un numerico è type 1/ref NULL). */
            int is_nil = (argc < 1) || (args[0].type == 0) || (args[0].type == 2 && args[0].ref == NULL);
            bout->result = is_nil ? 0.0 : 1.0;
        }
}
static void gmb_bf_652(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* METHODINFO/PROPERTYINFO: parità `IlRuntime.TypeInfoJson` per un tipo NON nel
             * TypeSchemaRegistry C# (il caso comune) → `{}`. Prima ritornava sempre "[]". */
            gmb_obj_string_t* s = gmb_str_cstr("{}");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_654(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
            {
                const char* tn = ((gmb_obj_string_t*)args[1].ref)->data;
                if (strcasecmp(tn, "STRING") == 0)
                {
                    char buf[64];
                    if (args[0].type == 1) snprintf(buf, sizeof(buf), "%.15g", args[0].num);
                    else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) { bout->is_num = 0; gmb_obj_retain(args[0].ref); bout->result_ref = args[0].ref; bout->result_ref_bt = GMB_BT_STRING; }
                    else snprintf(buf, sizeof(buf), "");
                    gmb_obj_string_t* s = gmb_str_cstr(buf);
                    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
                }
                else if (strcasecmp(tn, "INTEGER") == 0 || strcasecmp(tn, "INT") == 0 || strcasecmp(tn, "LONG") == 0)
                {
                    double sv = 0;
                    if (args[0].type == 1) sv = args[0].num;
                    else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) {
                        gmb_obj_string_t* ss = (gmb_obj_string_t*)args[0].ref;
                        gmb_string_t gs = { ss->data, ss->length, 1 };
                        sv = gmb_string_to_double(gs);
                    }
                    bout->result = (double)(long)sv;
                }
                else if (strcasecmp(tn, "DOUBLE") == 0 || strcasecmp(tn, "SINGLE") == 0 || strcasecmp(tn, "FLOAT") == 0)
                {
                    double sv = 0;
                    if (args[0].type == 1) sv = args[0].num;
                    else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) {
                        gmb_obj_string_t* ss = (gmb_obj_string_t*)args[0].ref;
                        gmb_string_t gs = { ss->data, ss->length, 1 };
                        sv = gmb_string_to_double(gs);
                    }
                    bout->result = sv;
                }
                else if (strcasecmp(tn, "BOOLEAN") == 0 || strcasecmp(tn, "BIT") == 0)
                {
                    double sv = 0;
                    if (args[0].type == 1) sv = args[0].num;
                    else if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) {
                        gmb_obj_string_t* ss = (gmb_obj_string_t*)args[0].ref;
                        gmb_string_t gs = { ss->data, ss->length, 1 };
                        sv = gmb_string_to_double(gs);
                    }
                    bout->result = sv != 0 ? -1 : 0;
                }
                else { bout->result = args[0].type == 1 ? args[0].num : 0; }
            }
            else bout->result = 0;
        }
}
static void gmb_bf_649(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* t = "U";
            if (argc >= 1 && args[0].type == 1)
                t = args[0].basic_type == GMB_BT_BOOL ? "L" : "N";
            else if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_STRING) t = "C";
                else if (args[0].ref->kind == GMB_OBJ_LIST) t = "A";
                else if (args[0].ref->kind == GMB_OBJ_COLLECTION)
                {
                    /* VALTYPE dal typeCode (fonte unica; es. HashSet/SparseSet → "O",
                     * Queue/Stack/Sorted/Tree/... → "A"). */
                    gmb_obj_collection_t* cc = (gmb_obj_collection_t*)args[0].ref;
                    t = gmb_collection_valtype_letter(cc->type_code);
                }
                else if (args[0].ref->kind == GMB_OBJ_GRAPH) t = "O";
                else if (args[0].ref->kind == GMB_OBJ_DICT)
                {
                    /* Istanza UDT/CLASS (dict con __class__): OGGETTO → "O". Dict nudo
                     * (ICollection nel C#) → "A" (parità ClipperStrings.ValType/IL/PCode;
                     * prima dava "M", outlier). */
                    gmb_obj_shared_val_t* vd = (gmb_obj_shared_val_t*)args[0].ref;
                    t = "A";
                    if (vd->dict)
                    {
                        gmb_val_t cv = gmb_dict_get(*vd->dict, "__class__");
                        if (cv.kind == GMB_TYPE_OBJECT && cv.v_ptr && ((gmb_obj_t*)cv.v_ptr)->kind == GMB_OBJ_DICT)
                            t = "O";
                    }
                }
                else t = "O";
            }
            gmb_obj_string_t* s = gmb_str_cstr(t);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_657(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            long idx = (long)args[0].num;
            /* COMMAND(1) = primo argomento utente (1-based, come C99/COMMANDCOUNT). */
            const char* r = (idx >= 1 && idx <= g_vm_host_argc) ? g_vm_host_argv[idx - 1] : "";
            gmb_obj_string_t* s = gmb_str_cstr(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_3818(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)func_code; (void)task;
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            gmb_obj_string_t* s = gmb_str_cstr(gmb_env_get(name));
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_3819(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)func_code; (void)task;
            const char* name = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = gmb_env_has(name) ? -1.0 : 0.0;
        }
}
static void gmb_bf_679(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* MESSAGE(text?): getter/setter (parità IlRuntime.MessageCmd). Prima stampava
             * su stdout e ritornava 0 → `PRINT CONSOLE.MESSAGE("x")` dava "x\n0" invece di "x". */
            (void)vm; (void)func_code; (void)task;
            const char* v = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
                ? ((gmb_obj_string_t*)args[0].ref)->data : NULL;
            gmb_string_t m = gmb_console_message(v);
            gmb_obj_string_t* s = gmb_str_cstr(m.data ? m.data : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_555(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* FINPUT(count, handle): legge `count` BYTE dall'handle (parita' IlRuntime.FileInput,
             * che legge count byte e ne ritorna il testo). Prima leggeva una RIGA e subito dopo
             * il risultato veniva SOVRASCRITTO con "" → ritornava sempre stringa vuota. */
            (void)vm; (void)func_code; (void)task;
            gmb_int_t handle = (gmb_int_t)args[1].num;
            gmb_int_t count = (gmb_int_t)args[0].num;
            gmb_string_t r = gmb_file_read_n(handle, count);
            gmb_obj_string_t* s = gmb_str_cstr(r.data ? r.data : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_656(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->result = (double)g_vm_host_argc;
        }
}
static void gmb_bf_683(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* url = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* r = url[0] ? gmb_doc_web(url, "md") : NULL;
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_684(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* AI.OCR(imagePath): delega al runtime C condiviso gmb_ai_ocr_read
         * (gmb_runtime_c/gmb_ai.c), che usa il backend OCR nativo (mtmd/vision) registrato da
         * gmb_vm_host — niente ponte C#. Se nessun modello OCR è attivo ritorna il messaggio
         * d'errore del runtime invece di un vm_error generico. */
        const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
        gmb_obj_string_t* s = gmb_str_cstr(gmb_ai_ocr_read(path, NULL, 0));
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* F3 multimodale (2026-09-11): AI.IMAGEINFO/RESIZE/CROP/CONVERT — delega al runtime C
 * condiviso gmb_image.c (stessa semantica di IL/PCodeVM/C99/C99Native, solo PNG). */
static const char* vm_image_str(gmb_stack_entry_t* args, int argc, int i)
{
        return (argc > i && args[i].type == 2 && args[i].ref && args[i].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[i].ref)->data : "";
}
static void gmb_bf_2115(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        char out[64]; char fmt[16] = "?"; int w = 0, h = 0;
        gmb_obj_string_t* s;
        if (!gmb_image_info(vm_image_str(args, argc, 0), &w, &h, fmt))
                s = gmb_str_cstr(gmb_image_error());
        else { snprintf(out, sizeof(out), "%d,%d,%s", w, h, fmt); s = gmb_str_cstr(out); }
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_2116(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        int w = (argc >= 3 && args[2].type == 1) ? (int)args[2].num : 0;
        int h = (argc >= 4 && args[3].type == 1) ? (int)args[3].num : 0;
        bout->is_num = 1;
        bout->result = gmb_image_resize(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1), w, h) ? 1.0 : 0.0;
}
static void gmb_bf_2117(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        int x = (argc >= 3 && args[2].type == 1) ? (int)args[2].num : 0;
        int y = (argc >= 4 && args[3].type == 1) ? (int)args[3].num : 0;
        int w = (argc >= 5 && args[4].type == 1) ? (int)args[4].num : 0;
        int h = (argc >= 6 && args[5].type == 1) ? (int)args[5].num : 0;
        bout->is_num = 1;
        bout->result = gmb_image_crop(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1), x, y, w, h) ? 1.0 : 0.0;
}
static void gmb_bf_2118(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        bout->is_num = 1;
        bout->result = gmb_image_convert(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1)) ? 1.0 : 0.0;
}
/* F2 multimodale (2026-09-11): AI.DOCFIELDS/DOCEXTRACT — runtime condiviso gmb_docfields.c
 * (stessa semantica di IL/PCodeVM/C99/C99Native). Stringhe copiate, nessun free (buffer
 * statico come gmb_ai_ocr_read). */
static void gmb_bf_2190(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        gmb_obj_string_t* s = gmb_str_cstr(gmb_docfields_json(vm_image_str(args, argc, 0)));
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_2191(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        gmb_obj_string_t* s = gmb_str_cstr(gmb_docextract_json(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1)));
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* F4 multimodale (2026-09-11): AI.SYNTHESIZE(text, output) → TTS via motore di sistema
 * (runtime condiviso gmb_tts.c). Ritorna 1 (ok) / 0 (errore). */
static void gmb_bf_2192(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        bout->is_num = 1;
        bout->result = gmb_tts_synthesize(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1)) ? 1.0 : 0.0;
}
/* F5 multimodale (2026-09-11): AI.IMAGEEMBED (lista 64), IMAGECOMPARE (0..100), IMAGEDETECT/
 * IMAGECLASSIFY (stringa via provider vision). Runtime condiviso gmb_image.c/gmb_ai.c. */
static void gmb_bf_2193(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        gmb_list_t l;
        gmb_obj_shared_val_t* out;
        gmb_int_t i, n;
        (void)vm; (void)func_code; (void)task;
        l = gmb_image_embed_list(vm_image_str(args, argc, 0));
        out = gmb_shared_list_new();
        n = gmb_list_count(l);
        if (out) for (i = 0; i < n; i++) gmb_list_add(out->list, gmb_list_get(l, i));
        bout->is_num = 0; bout->result_ref = out ? (gmb_obj_t*)out : NULL; bout->result_ref_bt = GMB_BT_OBJECT;
}
static void gmb_bf_2194(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        bout->is_num = 1;
        bout->result = (double)gmb_image_compare(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1));
}
static void gmb_bf_2195(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        char* ans;
        gmb_obj_string_t* s;
        (void)vm; (void)func_code; (void)task;
        ans = (char*)gmb_image_detect(vm_image_str(args, argc, 0));
        s = gmb_str_cstr(ans ? ans : "");
        free(ans);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_2196(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        char* ans;
        gmb_obj_string_t* s;
        (void)vm; (void)func_code; (void)task;
        ans = (char*)gmb_image_classify(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1));
        s = gmb_str_cstr(ans ? ans : "");
        free(ans);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* F6 multimodale (2026-09-11): AI.AGENTASKFILE(name, path [, question]) — input multimodale
 * instradato a vision/STT/documento (runtime condiviso gmb_agent.c gmb_agent_ask_file). */
void gmb_bf_2198(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        char* ans;
        gmb_obj_string_t* s;
        (void)vm; (void)func_code; (void)task;
        ans = gmb_agent_ask_file(vm_image_str(args, argc, 0), vm_image_str(args, argc, 1), vm_image_str(args, argc, 2));
        s = gmb_str_cstr(ans ? ans : "");
        free(ans);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* F7 multimodale (2026-09-11): AI.VIDEOINFO/VIDEOFRAME/VIDEOSNAPSHOT (Y4M nativo, gmb_video.c). */
static void gmb_bf_2199(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        gmb_obj_string_t* s;
        (void)vm; (void)func_code; (void)task;
        s = gmb_str_cstr(gmb_video_info_str(vm_image_str(args, argc, 0)));
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_2200(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        int idx = (argc >= 2 && args[1].type == 1) ? (int)args[1].num : 0;
        (void)vm; (void)func_code; (void)task;
        bout->is_num = 1;
        bout->result = gmb_video_frame(vm_image_str(args, argc, 0), idx, vm_image_str(args, argc, 2)) ? 1.0 : 0.0;
}
static void gmb_bf_2201(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)func_code; (void)task;
        bout->is_num = 1;
        bout->result = gmb_video_frame(vm_image_str(args, argc, 0), 0, vm_image_str(args, argc, 1)) ? 1.0 : 0.0;
}
void gmb_bf_470(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)args; (void)argc; (void)task;
        {
            /* RLOCK (470) / FLOCK (471): lock in-process di record/file dal runtime condiviso
             * (§0bis). Prima era uno STUB `bout->result = 0` → la VM diceva "non bloccato"
             * mentre IL/PCode dicevano "bloccato" (§ MiniTests/dbf/dbf_lock_filter.bas).
             * -1 = acquisito, come le altre funzioni DBF. */
            int64_t lh = gmb_db_current_dbf();
            int64_t ih = gmb_db_current_isam();
            int rc;
            /* Area ISAM (B+Tree): lock del record corrente / del file dal motore ISAM;
             * l'handle DBF è 0 per un'area ISAM → gmb_db_dbf_rlock(0) falliva sempre
             * (§5 "Lock record ID 1 FALLITO con RLOCK()"). */
            if (ih > 0) rc = (func_code == 470) ? gmb_db_isam_lock(ih) : gmb_db_isam_flock(ih);
            else        rc = (func_code == 470) ? gmb_db_dbf_rlock(lh) : gmb_db_dbf_flock(lh);
            /* gmb_db_lock_file ritorna l'FD (>=0) su successo, non GMB_DB_OK(0): il check
             * `== GMB_DB_OK` classificava il FLOCK riuscito come fallito (§5). Valore = 1
             * (parità IL: ISAM.RLOCK/FLOCK ritornano 1). */
            bout->result = (rc >= 0) ? 1.0 : 0.0;
        }
}
static void gmb_bf_527(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_obj_shared_val_t* l1 = (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[0].ref : NULL;
            gmb_obj_shared_val_t* l2 = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST) ? (gmb_obj_shared_val_t*)args[1].ref : NULL;
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && l1 && l1->list && l2 && l2->list)
            {
                int n = l1->list->count < l2->list->count ? (int)l1->list->count : (int)l2->list->count;
                for (int i = 0; i < n; i++)
                {
                    gmb_obj_shared_val_t* pair = gmb_shared_list_new();
                    if (pair)
                    {
                        gmb_list_add(pair->list, l1->list->items[i]);
                        gmb_list_add(pair->list, l2->list->items[i]);
                        gmb_list_add(out->list, gmb_make_val_obj((gmb_obj_t*)pair));
                    }
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { if (out) gmb_obj_release((gmb_obj_t*)out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_528(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* s = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* delims = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : " ";
            if (strlen(delims) == 0) delims = " ";
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out)
            {
                char buf[8192];
                strncpy(buf, s, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                char* tok = strtok(buf, delims);
                while (tok)
                {
                    gmb_obj_string_t* ts = gmb_str_cstr(tok);
                    gmb_list_add(out->list, gmb_make_val_obj((gmb_obj_t*)ts));
                    tok = strtok(NULL, delims);
                }
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_546(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* mode = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "r";
            gmb_string_t p = { (char*)path, strlen(path), 1 };
            gmb_int_t handle = 0;
            int rc = gmb_file_open_auto(p, mode, &handle);
            bout->result = rc == 0 ? (double)handle : -1;
        }
}
static void gmb_bf_553(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* LOF(n): n può essere un FILE NUMBER legacy (USING/OPEN ... AS #n) — mappa
             * all'handle in g_vm_file_handles, come EOF (gmb_bf EOF sopra) — oppure un
             * handle IO diretto. Prima: gmb_file_length(file number) → 0 su VM/C
             * (divergenza LOF nella catena USING). */
            int idx = (int)args[0].num;
            gmb_int_t handle = (idx >= 0 && idx < 64 && g_vm_file_handles[idx] > 0)
                ? g_vm_file_handles[idx] : (gmb_int_t)args[0].num;
            long len = gmb_file_length(handle);
            bout->result = len >= 0 ? (double)len : 0;
        }
}
static void gmb_bf_554(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
        {
            /* FREEFILE (func_code 554): primo numero di file libero, dal runtime condiviso
             * (§0bis). Prima era la COSTANTE 1 — stub vietato: con più file aperti il
             * programma riusava un numero già occupato. */
            bout->result = (double)gmb_file_free_file();
        }
}
static void gmb_bf_547(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            /* FCREATE: prima faceva solo fopen+fclose e ritornava 0 → handle INVALIDO, quindi
             * ogni FWRITE/FSEEK/FCLOSE successivo falliva (-1). Ora apre via runtime condiviso e
             * ritorna l'handle reale (1-based, come PCode/C99/C99Native). */
            gmb_string_t p = { (char*)path, strlen(path), 1 };
            gmb_int_t fh = 0;
            int rc = gmb_file_open_auto(p, "w+", &fh);
            bout->result = rc == 0 ? (double)fh : -1;
        }
}
static void gmb_bf_548(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* FREAD(handle [, count]) — FUNZIONE PURA (AGENTS §1/§3): ritorna i dati letti
             * (una riga se count e' omesso, altrimenti count byte). Prima leggeva TUTTO il
             * file concatenando le righe. La scrittura in un buffer e' il COMANDO
             * `FREAD handle [, count] INTO buffer` (desugara in assegnazione). */
            (void)vm; (void)func_code; (void)task;
            gmb_int_t fhandle = (gmb_int_t)args[0].num;
            gmb_string_t fr = (argc >= 2) ? gmb_file_read_n(fhandle, (gmb_int_t)args[1].num)
                                          : gmb_file_read_line_str(fhandle);
            gmb_obj_string_t* s = gmb_str_cstr(fr.data ? fr.data : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_549(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_int_t handle = (gmb_int_t)args[0].num;
            const char* data = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            gmb_string_t s = { (char*)data, strlen(data), 1 };
            /* byte scritti (parita' IlRuntime.FWrite); prima forzava 0. */
            bout->result = (double)gmb_file_write_string(handle, s);
        }
}
static void gmb_bf_550(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            gmb_int_t handle = (gmb_int_t)args[0].num;
            if (argc >= 2) { gmb_file_seek(handle, (long)args[1].num, 0); bout->result = (double)gmb_file_tell(handle); }
            else bout->result = (double)gmb_file_tell(handle);
        }
}
static void gmb_bf_551(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* risultato reale del close (0=ok, -1=handle invalido): parita' IlRuntime.FClose. */
            bout->result = (double)gmb_file_close((gmb_int_t)args[0].num);
        }
}
static void gmb_bf_552(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* FERROR: era uno STUB (sempre 0). Ora lo stato reale dal runtime condiviso. */
            (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
            bout->result = (double)gmb_file_error();
        }
}
void gmb_bf_564(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* DBF.LUPDATE(): data di ultimo aggiornamento dell'area corrente, stringa
             * "YYYY-MM-DD" dal runtime condiviso — lo stesso testo che PCode stampa via GbDate
             * e che il backend IL ritorna. Prima era un passthrough di args[0].num: senza
             * argomenti valeva 0 (§ MiniTests/dbf/dbf_replication.bas: VMC "lupdate: no"). */
            (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
            gmb_string_t ls = gmb_db_dbf_lupdate_str(gmb_db_current_dbf());
            if (ls.data)
            {
                gmb_obj_string_t* lso = gmb_str_cstr(ls.data);
                free(ls.data);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)lso; bout->result_ref_bt = GMB_BT_STRING;
            }
            else bout->is_num = 0;
        }
}
static void gmb_bf_566(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* c = getcwd(NULL, 0);
            gmb_obj_string_t* s = gmb_str_cstr(c ? c : "");
            free(c);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_567(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = (path && *path && access(path, F_OK) == 0) ? -1 : 0;
        }
}
static void gmb_bf_568(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            struct stat st;
            bout->result = (path && *path && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? -1 : 0;
        }
}
static void gmb_bf_569(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* folder = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            int max_depth = argc >= 2 ? (int)args[1].num : 10;
            char* rp = gmb_path_find_folder(folder, max_depth);
            gmb_obj_string_t* s = gmb_str_cstr(rp ? rp : "");
            free(rp);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_570(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* NORMALIZEPATH: prima usava realpath(), che RICHIEDE che il path esista e in caso
             * di fallimento restituisce l'input invariato → su un path inesistente (es.
             * "/a/b/../c") VMC non normalizzava nulla, divergendo da IL/PCode/C99 (che usano
             * gmb_fs_normalize_path, "parity Path.GetFullPath"). Ora chiama lo STESSO runtime
             * condiviso (§0bis: una sola implementazione). Bug trovato dal MiniTest
             * minitests/io/io_normalizepath.bas. */
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            gmb_string_t nsin = { (char*)path, strlen(path), 1 };
            gmb_string_t ns = gmb_fs_normalize_path(nsin);
            gmb_obj_string_t* s = gmb_str_cstr(ns.data ? ns.data : "");
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_571(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* FOLDERSIZE(path): runtime condiviso gmb_fs_folder_size (file → dimensione,
               directory → somma ricorsiva), parità IL/C99Native (§0bis). Prima:
               opendir inline → su un FILE opendir falliva e il risultato era 0 (HttpFetch:
               "Dimensione: 0" vs IL 5969) e su directory non ricorreva. */
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : ".";
            gmb_string_t pin = { (char*)path, strlen(path), 1 };
            bout->result = gmb_fs_folder_size(pin);
        }
}
void gmb_bf_561(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* host = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char ipbuf[64] = "";
            gmb_net_resolve_host(host, ipbuf, sizeof(ipbuf));
            gmb_obj_string_t* s = gmb_str_cstr(ipbuf);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_682(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* dirp = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* fmt = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "md";
            int rec = argc >= 3 ? (int)(args[2].num != 0) : 0;
            char* r = (dirp[0] && fmt[0]) ? gmb_doc_batch(dirp, fmt, rec) : NULL;
            gmb_obj_string_t* s = gmb_str_cstr(r ? r : "");
            if (r) free(r);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_759(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* JSON.LOAD(file$): parse GENERICO del contenuto — radice OGGETTO (dict condiviso)
             * o ARRAY (lista condivisa). Prima usava gmb_json_parse_to_dict (dict-only): un
             * file con radice array tornava un dict VUOTO (`JSON SAVE lista` + LOAD → str={}). */
            (void)vm; (void)func_code; (void)task;
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            char* buf = NULL;
            FILE* f = fopen(path, "r");
            if (f)
            {
                size_t cap = 0, len = 0;
                char tmp[4096];
                size_t rd;
                while ((rd = fread(tmp, 1, sizeof(tmp), f)) > 0)
                {
                    if (len + rd + 1 > cap)
                    {
                        cap = cap ? cap * 2 : 8192;
                        if (cap < len + rd + 1) cap = len + rd + 1;
                        buf = (char*)realloc(buf, cap);
                    }
                    memcpy(buf + len, tmp, rd);
                    len += rd;
                }
                fclose(f);
                if (buf) buf[len] = '\0';
            }
            gmb_obj_shared_val_t* out = NULL;
            if (buf)
            {
                gmb_val_t full;
                if (vm_json_full_parse(buf, &full) && full.v_ptr)
                {
                    gmb_obj_t* o = (gmb_obj_t*)full.v_ptr;   /* già shared (dict o lista) */
                    gmb_obj_retain(o);
                    out = (gmb_obj_shared_val_t*)o;
                }
                free(buf);
            }
            if (!out) out = gmb_shared_dict_new();           /* file assente/vuoto: dict vuoto */
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
static void gmb_bf_575(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            if (func_code == 575)
            {
                char* d = gmb_path_search_paths();
                gmb_obj_string_t* r = gmb_str_cstr(d ? d : "");
                free(d);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (func_code == 576)
            {
                char* d = gmb_path_bin_dir();
                gmb_obj_string_t* r = gmb_str_cstr(d ? d : "");
                free(d);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (func_code == 577)
            {
                char* d = gmb_path_current_file_dir();
                gmb_obj_string_t* r = gmb_str_cstr(d ? d : "");
                free(d);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (func_code == 574)
            {
                char* d = gmb_path_temp_dir();
                gmb_obj_string_t* r = gmb_str_cstr(d ? d : "");
                free(d);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            else if (func_code == 573)
            {
                char* d = gmb_path_temp_dir();
                gmb_obj_string_t* r = gmb_str_cstr(d ? d : "");
                free(d);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
            /* COUNTLINES (581) ritorna un NUMERO di righe, non una stringa: prima finiva nel
             * tronco stringa qui sotto chiamando gmb_path_resolve (= realpath) → COUNT abusava
             * di realpath e restituiva un percorso. Ora chiama il runtime condiviso
             * gmb_count_lines (§0bis). */
            else if (func_code == 581)
            {
                bout->result = (double)gmb_count_lines(path);
            }
            else
            {
            char* out = func_code == 582 ? gmb_path_resolve_file(path)
                       : gmb_path_parent(path);
            gmb_obj_string_t* r = gmb_str_cstr(out ? out : "");
            free(out);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
            }
        }
}
static void gmb_bf_573(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* f = gmb_path_temp_file();
            gmb_obj_string_t* r = gmb_str_cstr(f ? f : "");
            free(f);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_579(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            size_t len = 0;
            uint8_t* data = gmb_read_all_bytes(path, &len);
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && data)
            {
                for (size_t i = 0; i < len; i++) gmb_list_add(out->list, gmb_make_val_double(data[i]));
                free(data);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(data); if (out) gmb_obj_release((gmb_obj_t*)out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
/* ---- namespace FONTS: TrueType/OpenType (FUNZIONI) ----------------------
 * Nessuna logica qui: si delega a gmb_fonts_tt_*_list del runtime condiviso (§0bis).
 * Argomenti: path (stringa), size (numerico), codepoint (numerico). */
static double gmb_bf_fonts_num(gmb_stack_entry_t* args, int argc, int i, double def)
{
    if (i < 0 || i >= argc) return def;
    if (args[i].type == 1) return args[i].num;
    if (args[i].type == 2 && args[i].ref && args[i].ref->kind == GMB_OBJ_STRING)
    {
        const char* s = ((gmb_obj_string_t*)args[i].ref)->data;
        if (s && *s) return atof(s);
    }
    return def;
}
static const char* gmb_bf_fonts_path(gmb_stack_entry_t* args, int argc)
{
    if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        return ((gmb_obj_string_t*)args[0].ref)->data;
    return "";
}
static void gmb_bf_fonts_emit_list(gmb_list_t l, gmb_builtin_out_t* bout)
{
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (!out) { bout->is_num = 0; bout->result_ref = NULL; return; }
    {
        int n = (int)gmb_list_count(l);
        for (int i = 0; i < n; i++) gmb_list_add(out->list, gmb_list_get(l, i));
    }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
}
/* TTRENDER(path, size, codepoint) -> lista di byte (alpha 0..255). */
static void gmb_bf_605(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    gmb_bf_fonts_emit_list(
        gmb_fonts_tt_render_list(gmb_bf_fonts_path(args, argc),
                                 gmb_bf_fonts_num(args, argc, 1, 16.0),
                                 (int)gmb_bf_fonts_num(args, argc, 2, 0.0)),
        bout);
}
/* TTMETRICS(path, size, codepoint) -> lista di 8 numeri. */
static void gmb_bf_606(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    gmb_bf_fonts_emit_list(
        gmb_fonts_tt_metrics_list(gmb_bf_fonts_path(args, argc),
                                  gmb_bf_fonts_num(args, argc, 1, 16.0),
                                  (int)gmb_bf_fonts_num(args, argc, 2, 0.0)),
        bout);
}
/* TTGLYPHS(path) -> lista di codepoint coperti dal cmap. */
static void gmb_bf_607(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    gmb_bf_fonts_emit_list(gmb_fonts_tt_glyphs_list(gmb_bf_fonts_path(args, argc)), bout);
}

/* WRITEBYTES(path, bytes) — IO.WRITEBYTES: array di byte -> file, byte-esatto (§0bis:
 * l'implementazione è una sola, gmb_write_all_bytes_list del runtime condiviso). */
static void gmb_bf_604(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    int rc = -1;
    if (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
        rc = gmb_write_all_bytes_list(path, *sv->list);
    }
    else
    {
        /* array assente/vuoto: il file viene comunque creato (0 byte), come WriteAllBytes. */
        rc = gmb_write_all_bytes(path, NULL, 0);
    }
    bout->is_num = 1; bout->result = (double)rc;
}
static void gmb_bf_3806(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            (void)vm; (void)func_code;
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            gmb_string_t r = gmb_read_file_text(path);
            gmb_obj_string_t* s = gmb_str_cstr(r.data ? r.data : "");
            free(r.data);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_580(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            int n = 0;
            char** lines = gmb_read_all_lines(path, &n);
            gmb_obj_shared_val_t* out = gmb_shared_list_new();
            if (out && lines)
            {
                for (int i = 0; i < n; i++)
                {
                    gmb_obj_string_t* s = gmb_str_cstr(lines[i]);
                    gmb_list_add(out->list, gmb_make_val_obj((gmb_obj_t*)s));
                    free(lines[i]);
                }
                free(lines);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(lines); if (out) gmb_obj_release((gmb_obj_t*)out); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_581(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            const char* path = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            bout->result = (double)gmb_count_lines(path);
        }
}
static void gmb_bf_763(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Arity canoniche (IlRuntime.ReadValue / PCodeProcessor.Builtins):
             *   (file, section, key[, def]) → file indicato
             *   (key)                      → app.settings.json, sezione "", def ""
             * Prima la 1-arg trattava l'argomento come FILE (divergenza da IL/PCode). */
            const char* a0 = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* a1 = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* a2 = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[2].ref)->data : "";
            const char* a3 = (argc >= 4 && args[3].type == 2 && args[3].ref && args[3].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[3].ref)->data : "";
            const char* file = (argc >= 3) ? a0 : "app.settings.json";
            const char* sec  = (argc >= 3) ? a1 : "";
            const char* key  = (argc >= 3) ? a2 : a0;
            const char* def  = (argc >= 4) ? a3 : "";
            { FILE* d=fopen("/tmp/rv.log","a"); if(d){fprintf(d,"763 argc=%d file=[%s] sec=[%s] key=[%s] def=[%s]\n",argc,file,sec,key,def);fclose(d);} }
            char* v = gmb_ini_read_value(file, sec, key, def);
            { FILE* d=fopen("/tmp/rv.log","a"); if(d){fprintf(d,"763 -> [%s]\n", v?v:"(null)");fclose(d);} }
            gmb_obj_string_t* s = gmb_str_cstr(v ? v : "");
            free(v);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        }
}
static void gmb_bf_764(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* Due firme canoniche (IlRuntime.ReadSection): (file, sezione) e
             * (sezione) → sezione dal file di default app.settings.json. Prima la 1-arg
             * leggeva il file con sezione "" (divergenza da IL/PCode). */
            const char* a0 = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[0].ref)->data : "";
            const char* a1 = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING) ? ((gmb_obj_string_t*)args[1].ref)->data : "";
            const char* file = (argc >= 2) ? a0 : "app.settings.json";
            const char* sec  = (argc >= 2) ? a1 : a0;
            /* Sezione INI/JSON -> DICT (chiavi k=v): l'accesso `sec.host` richiede un dict,
             * non la lista delle righe (prima: lista -> campi vuoti, bug reale
             * json/readsection.bas). Stessa sorgente di parsing dei backend C99
             * (gmb_ini_read_section, §0bis). */
            int cnt = 0;
            char** lines = gmb_ini_read_section(file, sec, &cnt);
            gmb_obj_shared_val_t* out = gmb_shared_dict_new();
            if (out)
            {
                for (int i = 0; i < cnt; i++)
                {
                    char* eq = strchr(lines[i], '=');
                    if (eq) { *eq = 0; gmb_dict_set(out->dict, lines[i], gmb_ini_coerce_val(eq + 1)); }
                    else { gmb_dict_set(out->dict, lines[i], gmb_make_val_string_counted("")); }
                    free(lines[i]);
                }
                free(lines);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
            }
            else { free(lines); bout->is_num = 0; bout->result_ref = NULL; }
        }
}
static void gmb_bf_765(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        /* F2 (§0bis, una sola implementazione): JSON.EXTRACT la gestisce il walk del
         * path (segmenti .k / [i]) di gmb_bf_3407 — il vecchio corpo (gmb_dict_get con
         * la chiave INTERA, senza walk: "user.name" → miss) divergeva da IL/PCode. */
        gmb_bf_3407(vm, args, argc, func_code, bout, task);
}

static void gmb_bf_1155(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int cur;
            pthread_mutex_lock(&vm->worker_lock);
            cur = vm->worker_target;
            pthread_mutex_unlock(&vm->worker_lock);
            if (argc == 0) { bout->result = cur; return; }
            int target = (int)args[0].num;
            if (target <= 0) { bout->result = cur; return; }   /* 0 = sola query (Go runtime.GOMAXPROCS(0)) */
            if (target > 32) target = 32;
            int spawn_n = 0;
            pthread_mutex_lock(&vm->worker_lock);
            int prev = vm->worker_target;
            vm->worker_target = target;
            
            vm->retire_pending = 0;
            int live = vm->worker_live;
            if (target > live) spawn_n = target - live;
            else if (target < live) vm->retire_pending = live - target;
            pthread_mutex_unlock(&vm->worker_lock);
            for (int i = 0; i < spawn_n; i++) gmb_vm_spawn_worker(vm);
            bout->result = prev;
        }
        
}
static void gmb_bf_1156(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_ATOMIC)
                bout->result = (double)gmb_atomic_add((gmb_atomic_t*)args[0].ref, (long long)args[1].num);
        }
}
static void gmb_bf_1157(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_ATOMIC)
                bout->result = (double)gmb_atomic_load((gmb_atomic_t*)args[0].ref);
        }
}
static void gmb_bf_1158(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_ATOMIC)
                gmb_atomic_store((gmb_atomic_t*)args[0].ref, (long long)args[1].num);
        }
}
static void gmb_bf_1159(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 3 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_ATOMIC)
                bout->result = gmb_atomic_cas((gmb_atomic_t*)args[0].ref,
                                        (long long)args[1].num, (long long)args[2].num) ? 1.0 : 0.0;
        }
}
static void gmb_bf_1165(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_ATOMIC)
                bout->result = (double)gmb_atomic_swap((gmb_atomic_t*)args[0].ref, (long long)args[1].num);
        }
        
}
static void gmb_bf_1160(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->is_num = 0;
            bout->result_ref = (gmb_obj_t*)gmb_context_create(NULL);
            bout->result_ref_bt = GMB_BT_OBJECT;
        }
}
static void gmb_bf_1161(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->is_num = 0; bout->result_ref = NULL;
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_CONTEXT)
            {
                bout->result_ref = (gmb_obj_t*)gmb_context_create((gmb_context_t*)args[0].ref);
                bout->result_ref_bt = GMB_BT_OBJECT;
            }
        }
}
static void gmb_bf_1162(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_CONTEXT)
                gmb_context_cancel((gmb_context_t*)args[0].ref, vm->scheduler);
        }
}
static void gmb_bf_1163(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            bout->is_num = 0; bout->result_ref = NULL;
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_CONTEXT)
            {
                gmb_wait_entry_t* wr = NULL; gmb_wait_entry_t* ws = NULL;
                int nr = 0, ns = 0;
                gmb_channel_t* ch = gmb_context_done((gmb_context_t*)args[0].ref,
                                                     &wr, &nr, &ws, &ns);
                for (int i = 0; i < nr; i++)
                {
                    gmb_wait_entry_t e = wr[i];
                    if (!e.sel) gmb_sched_enqueue(vm->scheduler, e.task);
                    else
                    {
                        if (gmb_select_claim(e.sel, e.case_idx))
                            gmb_sched_enqueue(vm->scheduler, e.task);
                        gmb_select_release(e.sel);
                    }
                }
                for (int i = 0; i < ns; i++)
                {
                    gmb_wait_entry_t e = ws[i];
                    if (!e.sel) gmb_sched_enqueue(vm->scheduler, e.task);
                    else
                    {
                        if (gmb_select_claim(e.sel, e.case_idx))
                            gmb_sched_enqueue(vm->scheduler, e.task);
                        gmb_select_release(e.sel);
                    }
                }
                free(wr); free(ws);
                if (ch) { bout->result_ref = (gmb_obj_t*)ch; bout->result_ref_bt = GMB_BT_OBJECT; }
            }
        }
}
static void gmb_bf_1164(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_CONTEXT)
                bout->result = gmb_context_is_cancelled((gmb_context_t*)args[0].ref) ? 1.0 : 0.0;
        }
        
}
static void gmb_bf_1201(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_POOL)
            {
                gmb_stack_entry_t e = gmb_pool_get((gmb_pool_t*)args[0].ref);
                if (e.type == 2 && e.ref)
                {
                    /* Il valore esce dal pool POSSEDUTO (+1): lo prende il dispatcher via bout. Spingerlo
                     * qui lasciava anche il push_nil del dispatcher in cima → POOL.GET sempre 0/nil. */
                    bout->is_num = 0; bout->result_ref = e.ref; bout->result_ref_bt = e.basic_type;
                    return;
                }
                if (e.type == 1) { bout->is_num = 1; bout->result = e.num; bout->num_bt = e.basic_type; return; }
            }
            bout->is_num = 0; bout->result_ref = NULL;   /* pool vuoto → NIL (il dispatcher lo pusha) */
            return;
        }
}
static void gmb_bf_1202(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 2 && args[0].type == 2 && args[0].ref &&
                args[0].ref->kind == GMB_OBJ_POOL)
            {
                gmb_pool_put((gmb_pool_t*)args[0].ref, args[1]);
                args[1].ref = NULL;   /* il pool ha PRESO il riferimento: il dispatcher non deve rilasciarlo (use-after-free → stringa vuota) */
            }
        }
        
}
/* ISAM.LOG([area|alias]) → riepilogo WAL (builtin 451: TokenKind.IsamLog →
 * BuiltInFunction.ISAMLOG). Stessa fonte unica del C99 (gmb_db_isam_log_str). */
static void gmb_bf_451(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    int64_t h;
    gmb_string_t s;
    gmb_obj_string_t* o;
    (void)vm; (void)func_code; (void)task;
    h = gmb_db_current_isam();
    if (argc >= 1)
    {
        if (args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        {
            const char* an = ((gmb_obj_string_t*)args[0].ref)->data;
            if (an && an[0]) h = gmb_db_isam_handle_for(an);
        }
        else if (args[0].type == 1 && args[0].num > 0)
        {
            h = (int64_t)args[0].num;
        }
    }
    s = gmb_db_isam_log_str(h);
    o = gmb_str_cstr(s.data ? s.data : "");
    if (s.data) free(s.data);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)o; bout->result_ref_bt = GMB_BT_STRING;
}

static void gmb_bf_1203(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* SYNCMAP.STORE(m,k,v): gmb_concurrentmap_store ADOTTA chiave e valore. Il dispatch
     * rilascia gli argomenti a fine chiamata → vanno azzerati qui (contratto ownership),
     * altrimenti double-free (SIGBUS) e LOAD su memoria liberata. Il RICEVENTE non si
     * tocca: è della variabile. */
    (void)vm; (void)func_code; (void)task;
    if (argc >= 3 && args[0].type == 2 && args[0].ref &&
        args[0].ref->kind == GMB_OBJ_CONCURRENTMAP)
    {
        gmb_concurrentmap_store((gmb_concurrentmap_t*)args[0].ref, args[1], args[2]);
        args[1].ref = NULL; args[1].type = 0;
        args[2].ref = NULL; args[2].type = 0;
    }
    bout->is_num = 1; bout->result = 0;
}

static void gmb_bf_1204(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* SYNCMAP.LOAD: risultato via bout (come i metodi DICT/LIST), non push sul task stack.
     * La entry è un riferimento POSSEDUTO (retain della mappa) → out.result_ref. */
    (void)vm; (void)func_code; (void)task;
    if (argc >= 2 && args[0].type == 2 && args[0].ref &&
        args[0].ref->kind == GMB_OBJ_CONCURRENTMAP)
    {
        gmb_stack_entry_t e = gmb_concurrentmap_load((gmb_concurrentmap_t*)args[0].ref, args[1]);
        if (e.type == 2 && e.ref) { bout->is_num = 0; bout->result_ref = e.ref; bout->result_ref_bt = e.basic_type; return; }
        if (e.type == 1) { bout->is_num = 1; bout->result = e.num; return; }
    }
    bout->is_num = 0; bout->result_ref = NULL;
}

static void gmb_bf_1205(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* SYNCMAP.DELETE: la mappa CEDE il valore (move) → riferimento posseduto per il chiamante. */
    (void)vm; (void)func_code; (void)task;
    if (argc >= 2 && args[0].type == 2 && args[0].ref &&
        args[0].ref->kind == GMB_OBJ_CONCURRENTMAP)
    {
        gmb_stack_entry_t e = gmb_concurrentmap_delete((gmb_concurrentmap_t*)args[0].ref, args[1]);
        if (e.type == 2 && e.ref) { bout->is_num = 0; bout->result_ref = e.ref; bout->result_ref_bt = e.basic_type; return; }
        if (e.type == 1) { bout->is_num = 1; bout->result = e.num; return; }
    }
    bout->is_num = 0; bout->result_ref = NULL;
}

static void gmb_bf_1206(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* SYNCMAP.LOADORSTORE: la mappa consuma SEMPRE chiave e valore proposti (li rilascia
     * se la chiave esiste, li adotta se la inserisce) → azzerarli per il dispatch. */
    (void)vm; (void)func_code; (void)task;
    if (argc >= 3 && args[0].type == 2 && args[0].ref &&
        args[0].ref->kind == GMB_OBJ_CONCURRENTMAP)
    {
        gmb_stack_entry_t e = gmb_concurrentmap_load_or_store(
            (gmb_concurrentmap_t*)args[0].ref, args[1], args[2]);
        args[1].ref = NULL; args[1].type = 0;
        args[2].ref = NULL; args[2].type = 0;
        if (e.type == 2 && e.ref) { bout->is_num = 0; bout->result_ref = e.ref; bout->result_ref_bt = e.basic_type; return; }
        if (e.type == 1) { bout->is_num = 1; bout->result = e.num; return; }
    }
    bout->is_num = 0; bout->result_ref = NULL;
}

static void gmb_bf_1207(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            if (argc >= 1 && args[0].type == 2 && args[0].ref)
            {
                if (args[0].ref->kind == GMB_OBJ_MUTEX)
                    bout->result = gmb_mutex_try_lock_nb((gmb_mutex_t*)args[0].ref) ? -1.0 : 0.0;
                else if (args[0].ref->kind == GMB_OBJ_RWMUTEX)
                    bout->result = gmb_rwlock_try_lock_nb((gmb_rwlock_t*)args[0].ref) ? -1.0 : 0.0;
                { gmb_obj_release(args[0].ref); args[0].ref = NULL; args[0].type = 0; }
            }
        }
}
static void gmb_bf_1208(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            int n = 0;
            gmb_scheduler_t* ns = vm->scheduler;
            if (ns)
            {
                pthread_mutex_lock(&ns->all_lock);
                for (int i = 0; i < ns->all_count; i++)
                    if (ns->all_tasks[i]->state != GMB_TS_FINISHED) n++;
                pthread_mutex_unlock(&ns->all_lock);
            }
            bout->result = (double)n;
        }
}
static void gmb_bf_1209(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
            bout->result = ncpu > 0 ? (double)ncpu : 1.0;
        }
        

}
void gmb_bf_600_c600(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* C2PX(col, size) = col*size */ bout->result = gmb_gfx_c2px(args[0].num, argc >= 2 ? args[1].num : 1.0);
}
void gmb_bf_600_c601(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* C2PY(row, size) = row*size */ bout->result = gmb_gfx_c2py(args[0].num, argc >= 2 ? args[1].num : 1.0);
}
void gmb_bf_600_c602(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* P2CX(x, size) = x/size */ bout->result = gmb_gfx_p2cx(args[0].num, argc >= 2 ? args[1].num : 1.0);
}
void gmb_bf_600_c603(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* P2CY(y, size) = y/size */ bout->result = gmb_gfx_p2cy(args[0].num, argc >= 2 ? args[1].num : 1.0);
}
static void gmb_bf_702_setcredits(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* PROVIDER.SETCREDITS(name, credits) */ bout->result = (double)gmb_ai_set_credits(vm_builtin_sarg(args, argc, 0), argc >= 2 ? args[1].num : 0);
}
static void gmb_bf_702_credits(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* PROVIDER.CREDITS(name) */ bout->result = gmb_ai_get_credits(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c704(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PATH.MODELADD(path) */ bout->result = (double)gmb_ai_path_model_add(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c705(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PATH.MODELREMOVE(path) */ bout->result = (double)gmb_ai_path_model_remove(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c706(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PATH.SKILLADD(path) */ bout->result = (double)gmb_ai_path_skill_add(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c707(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PATH.SKILLREMOVE(path) */ bout->result = (double)gmb_ai_path_skill_remove(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c708(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.SKILLS.ADD(path) */ bout->result = (double)gmb_ai_skill_add(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c709(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.SKILLS.REMOVE(name) */ bout->result = (double)gmb_ai_skill_remove(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c710(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.SKILLS.REFRESH() */ bout->result = (double)gmb_ai_skill_refresh();
}
static void gmb_bf_704_c711(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.SKILLS.SETINDEX(path) */ bout->result = (double)gmb_ai_skill_set_index(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_704_c712(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.SKILLS.MARKETPLACE(url, dest?) — git clone */ bout->result = (double)gmb_ai_skills_marketplace(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
}
static void gmb_bf_713(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        char* sp = gmb_ai_skills_prompt(); vm_builtin_rstr(sp, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_714_c714(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AISKILLSLIST: parità IlRuntime.AiSkillsList -> string[]: riempie una LISTA BASIC
       (shared list di gmb_val_t stringhe, come SPLIT/DICT_KEYS), NON una CSV stringa. */
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (!out) return;
    gmb_int_t n = 0;
    gmb_string_t* names = gmb_ai_skills_list(&n);
    for (gmb_int_t i = 0; i < n; i++)   /* C# AiSkillsList: ordinata (OrderBy) */
        for (gmb_int_t j = i + 1; j < n; j++)
            if (names[i].data && names[j].data && strcasecmp(names[i].data, names[j].data) > 0) {
                gmb_string_t tmp = names[i]; names[i] = names[j]; names[j] = tmp;
            }
    for (gmb_int_t i = 0; i < n; i++)
        gmb_list_add(out->list, gmb_make_val_string_counted(names[i].data));
    if (names) { for (gmb_int_t i = 0; i < n; i++) gmb_string_free(&names[i]); free(names); }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
}
static void gmb_bf_714_c715(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AISKILLSPATHS: parità IlRuntime.AiSkillsPaths -> string[] (tutti i path configurati,
       NON il solo index path come prima). */
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (!out) return;
    gmb_int_t n = 0;
    gmb_string_t* paths = gmb_ai_skills_paths(&n);
    for (gmb_int_t i = 0; i < n; i++)
        gmb_list_add(out->list, gmb_make_val_string_counted(paths[i].data));
    if (paths) { for (gmb_int_t i = 0; i < n; i++) gmb_string_free(&paths[i]); free(paths); }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
}
static void gmb_bf_714_c716(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_ai_set_log_path(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_714_c717(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_ai_log_write(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), 0);
}
static void gmb_bf_714_c718(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_ai_log_write_line(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
}
void gmb_bf_714_c719(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    /* AI AGENT ASK STREAM <nome>,<prompt> INTO <Sub>: completion → callback SUB per-TOKEN.
     * Parità FixtureLlmCompleter.CompleteStream: split su ' ' con spazio finale per parola. */
    const char* cb = (argc >= 3) ? vm_builtin_sarg(args, argc, 2) : "";
    char* ar = gmb_agent_ask(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
    if (ar && cb && *cb) {
        const char* start = ar;
        for (const char* p = ar; ; p++) {
            if (*p == ' ' || *p == '\0') {
                size_t len = (size_t)(p - start);
                char* tok = (char*)malloc(len + 2);
                if (tok) {
                    memcpy(tok, start, len);
                    tok[len] = ' '; tok[len + 1] = '\0';
                    gmb_vm_call_proc_sync_str(vm, task, cb, tok);
                    free(tok);
                }
                if (*p == '\0') break;
                start = p + 1;
            }
        }
    }
    vm_builtin_rstr(ar, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_714_c720(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_mcp_server_add(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), vm_builtin_sarg(args, argc, 2));
}
static void gmb_bf_714_c721(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_mcp_server_remove(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_714_c722(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_mcp_server_set_enabled(vm_builtin_sarg(args, argc, 0), 1);
}
static void gmb_bf_714_c723(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_mcp_server_set_enabled(vm_builtin_sarg(args, argc, 0), 0);
}
static void gmb_bf_714_c724(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_mcp_server_refresh();
}
static void gmb_bf_725_c725(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* sl = gmb_mcp_server_list(); vm_builtin_rstr(sl, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_725_c726(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* ss = gmb_mcp_server_status(vm_builtin_sarg(args, argc, 0)); vm_builtin_rstr(ss, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_725_c727(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* mc = gmb_mcp_call(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), vm_builtin_sarg(args, argc, 2)); vm_builtin_rstr(mc, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_725_c728(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_ai_save_config();
}
static void gmb_bf_725_c729(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_ai_load_config();
}
static void gmb_bf_725_c730(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* mpl = gmb_mcp_server_marketplace_list(); vm_builtin_rstr(mpl, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_725_c731(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* mg = gmb_mcp_server_marketplace_get(vm_builtin_sarg(args, argc, 0)); vm_builtin_rstr(mg, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_732(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        char* lts = gmb_mcp_list_tools_server(vm_builtin_sarg(args, argc, 0)); vm_builtin_rstr(lts, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_782_c782(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
            gmb_ts_clear_error();
    (void)vm; (void)task; (void)func_code;
    /* TSCOVARIANCEMATRIX(data): data = lista di osservazioni (lista di lista) →
     * matrice m×m di covarianza (Bessel), parità CommonLib.Numerics.CovarianceMatrix. */
    if (argc < 1 || args[0].type != 2 || !args[0].ref || args[0].ref->kind != GMB_OBJ_LIST) {
        bout->is_num = 0; bout->result_ref = NULL; return;
    }
    gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[0].ref;
    int n_obs = (int)sv->list->count;
    if (n_obs < 1) { bout->is_num = 0; bout->result_ref = NULL; return; }
    double** obs = (double**)calloc((size_t)n_obs, sizeof(double*));
    if (!obs) { bout->is_num = 0; bout->result_ref = NULL; return; }
    int n_vars = 0;
    for (int i = 0; i < n_obs; i++) {
        gmb_val_t it = sv->list->items[i];
        gmb_obj_shared_val_t* rl = NULL;
        if ((it.kind == GMB_TYPE_OBJECT || it.kind == GMB_TYPE_ARRAY) && it.v_ptr)
            rl = (gmb_obj_shared_val_t*)it.v_ptr;
        if (!rl || !rl->list) continue;
        int rn = (int)rl->list->count;
        if (i == 0) n_vars = rn;
        obs[i] = (double*)calloc((size_t)(rn > 0 ? rn : 1), sizeof(double));
        for (int j = 0; j < rn; j++) {
            gmb_val_t v = rl->list->items[j];
            obs[i][j] = v.kind == GMB_TYPE_DOUBLE ? v.v_double : v.kind == GMB_TYPE_INT ? (double)v.v_int : 0.0;
        }
    }
    if (n_vars <= 0) { for (int i=0;i<n_obs;i++) free(obs[i]); free(obs); bout->is_num = 0; bout->result_ref = NULL; return; }
    gmb_double_t** cov = gmb_ts_covariance_obs((const double* const*)obs, n_obs, n_vars);
    for (int i = 0; i < n_obs; i++) free(obs[i]);
    free(obs);
    if (!cov) { bout->is_num = 0; bout->result_ref = NULL; return; }
    double* flat = (double*)malloc((size_t)n_vars * n_vars * sizeof(double));
    if (flat) for (int i = 0; i < n_vars; i++) { for (int j = 0; j < n_vars; j++) flat[i*n_vars+j] = cov[i][j]; free(cov[i]); }
    free(cov);
    gmb_vm_set_result_matrix(flat, n_vars*n_vars, n_vars, &bout->is_num, &bout->result_ref);
            { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c783(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_minmax(data, cn, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c784(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_mean(data, cn, argc >= 2 ? (int)args[1].num : 0, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c785(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_skewness(data, cn, argc >= 2 ? (int)args[1].num : 0, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c786(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_kurtosis(data, cn, argc >= 2 ? (int)args[1].num : 0, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c787(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_median(data, cn, argc >= 2 ? (int)args[1].num : 0, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c788(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_quantile(data, cn, argc >= 2 ? (int)args[1].num : 0, argc >= 3 ? args[2].num : 0.5, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c789(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_sharpe(data, cn, argc >= 2 ? (int)args[1].num : 0, argc >= 3 ? args[2].num : 0.0, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c790(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_sortino(data, cn, argc >= 2 ? (int)args[1].num : 0, argc >= 3 ? args[2].num : 0.0, argc >= 4 ? args[3].num : 0.0, &on); free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c791(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_minmax(data, cn, argc >= 2 ? (int)args[1].num : 0, &on); free(data);
    /* Matrice [finestre][2] = [min,max] per finestra (parità oracolo). */
    gmb_vm_set_result_matrix(out, (int)on, 2, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c792(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    gmb_ts_clear_error();
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = gmb_ts_rolling_regression_single(data, cn, argc >= 2 ? (int)args[1].num : 0, &on); free(data);
    /* Matrice [finestre][2] (parità oracolo: doppia serie → array di righe). */
    gmb_vm_set_result_matrix(out, (int)on, 2, &bout->is_num, &bout->result_ref);
    { const char* tse = gmb_ts_last_error(); if (tse && *tse) vm_error(task, tse); }
}
static void gmb_bf_782_c794(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int cn = 0; double* data = gmb_vm_list_to_dbl(&args[0], &cn); gmb_int_t on = 0; double* out = data ? gmb_ma_coefficients(data, cn, argc >= 2 ? (int)args[1].num : 0, &on) : NULL; free(data); gmb_vm_set_result_list(out, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_782_c795(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int an = 0, bn = 0; double* a = gmb_vm_list_to_dbl(&args[0], &an); double* b = gmb_vm_list_to_dbl(&args[1], &bn); int n = an < bn ? an : bn; bout->result = (a && b) ? gmb_euclidean_distance_squared(a, b, n) : 0.0; free(a); free(b);
}

static void gmb_bf_796_c796(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_row_sums_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c797(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_col_sums_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c798(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_row_means_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c799(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_col_means_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c800(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_row_variances_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c801(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_col_variances_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c802(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = gmb_matrix_frobenius_norm(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
}
static void gmb_bf_796_c803(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = gmb_matrix_one_norm(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
}
static void gmb_bf_796_c804(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = gmb_matrix_infinity_norm(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
}
static void gmb_bf_796_c805(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_matrix_is_symmetric(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL)) ? -1 : 0);
}
static void gmb_bf_796_c806(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_matrix_is_orthogonal(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL)) ? -1 : 0);
}
static void gmb_bf_796_c807(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_matrix_is_upper_triangular(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL)) ? -1 : 0);
}
static void gmb_bf_796_c808(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_matrix_is_lower_triangular(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL)) ? -1 : 0);
}
static void gmb_bf_796_c809(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_argmax_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c810(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; double* r = gmb_matrix_argmin_alloc(mh, &on); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c811(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __ms = gmb_matrix_slice(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), argc >= 2 ? (int)args[1].num : 0, argc >= 3 ? (int)args[2].num : 0, argc >= 4 ? (int)args[3].num : 0, argc >= 5 ? (int)args[4].num : 0);
    vm_matrix_set_value_result(__ms, bout);
    gmb_matrix_free(__ms);
}
static void gmb_bf_796_c812(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __mr = gmb_matrix_reshape(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), argc >= 2 ? (int)args[1].num : 0, argc >= 3 ? (int)args[2].num : 0);
    vm_matrix_set_value_result(__mr, bout);
    gmb_matrix_free(__mr);
}
/* MATRIX F2b (2026-11-09): ROW/COL restituiscono una matrice rank-2 1×cols / rows×1;
 * ISCONTIGUOUS è il predicato di contiguità (convenzione Bit -1/0). */
static void gmb_bf_3900(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    int __r = argc >= 2 ? (int)args[1].num : 0;
    int64_t __v = gmb_matrix_slice(__h, __r, __r + 1, 0, gmb_matrix_cols(__h));
    vm_matrix_set_value_result(__v, bout);
    gmb_matrix_free(__v);
}
static void gmb_bf_3901(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    int __c = argc >= 2 ? (int)args[1].num : 0;
    int64_t __v = gmb_matrix_slice(__h, 0, gmb_matrix_rows(__h), __c, __c + 1);
    vm_matrix_set_value_result(__v, bout);
    gmb_matrix_free(__v);
}
static void gmb_bf_3902(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    bout->result = gmb_matrix_is_contiguous_h(__h) ? -1.0 : 0.0;
}
static void gmb_bf_796_c813(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __v = gmb_matrix_concat_rows(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), vm_matrix_handle_arg(argc >= 2 ? &args[1] : NULL));
    vm_matrix_set_value_result(__v, bout);
    gmb_matrix_free(__v);
}
static void gmb_bf_796_c814(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __v = gmb_matrix_concat_cols(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), vm_matrix_handle_arg(argc >= 2 ? &args[1] : NULL));
    vm_matrix_set_value_result(__v, bout);
    gmb_matrix_free(__v);
}
static void gmb_bf_796_c815(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int vn = 0; double* v = gmb_vm_list_to_dbl(&args[1], &vn); bout->result = (double)gmb_matrix_add_row_vector(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), v, vn); free(v);
}
static void gmb_bf_796_c816(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int vn = 0; double* v = gmb_vm_list_to_dbl(&args[1], &vn); bout->result = (double)gmb_matrix_add_col_vector(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), v, vn); free(v);
}
static void gmb_bf_796_c817(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = gmb_matrix_max_value(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
}
static void gmb_bf_796_c818(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = gmb_matrix_min_value(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
}
static void gmb_bf_796_c819(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; bool* bm = gmb_matrix_above_mask_alloc(mh, argc >= 2 ? args[1].num : 0, &on); if (bm) { double* bd = (double*)malloc(sizeof(double) * (size_t)on); if (bd) { for (gmb_int_t i = 0; i < on; i++) bd[i] = bm[i] ? -1.0 : 0.0; } free(bm); gmb_vm_set_result_list(bd, (int)on, &bout->is_num, &bout->result_ref); }
}
static void gmb_bf_796_c820(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t mh = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL); gmb_int_t on = 0; bool* bm = gmb_matrix_above_matrix_alloc(mh, argc >= 2 ? args[1].num : 0, &on); if (bm) { double* bd = (double*)malloc(sizeof(double) * (size_t)on); if (bd) { for (gmb_int_t i = 0; i < on; i++) bd[i] = bm[i] ? -1.0 : 0.0; } free(bm); gmb_vm_set_result_list(bd, (int)on, &bout->is_num, &bout->result_ref); }
}
static void gmb_bf_796_c821(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* cs = gmb_matrix_as_csv(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL)); vm_builtin_rstr(cs, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_796_c822(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* js = gmb_matrix_as_json(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL)); vm_builtin_rstr(js, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_796_c825(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int64_t __mc = gmb_matrix_clone(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
    vm_matrix_set_value_result(__mc, bout);
    gmb_matrix_free(__mc);
}
static void gmb_bf_796_c826(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    int vn = 0; double* v = gmb_vm_list_to_dbl(&args[1], &vn); gmb_int_t on = 0; double* r = v ? gmb_matrix_multiply_vector_alloc(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL), v, vn, &on) : NULL; free(v); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c827(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* StdLib: MULTIPLYLEFTVECTOR(matrix, values) → args[0]=handle, args[1]=vettore.
     * Prima leggeva il vettore da args[0] (handle) → vuoto. */
    int64_t mlv_h = vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL);
    int vn = 0; double* v = (argc >= 2) ? gmb_vm_list_to_dbl(&args[1], &vn) : NULL; gmb_int_t on = 0; double* r = v ? gmb_matrix_multiply_left_vector_alloc(v, vn, mlv_h, &on) : NULL; free(v); gmb_vm_set_result_list(r, (int)on, &bout->is_num, &bout->result_ref);
}
static void gmb_bf_796_c828(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_matrix_load_csv(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_796_c829(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_matrix_load_json(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_796_c937(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_matrix_pseudo_inverse(vm_matrix_handle_arg(argc >= 1 ? &args[0] : NULL));
}
void gmb_bf_840_c840(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* url = vm_builtin_sarg(args, argc, 0); char* b = gmb_net_http_body("GET", url, NULL); vm_builtin_rstr(b, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
void gmb_bf_840_c841(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* url = vm_builtin_sarg(args, argc, 0); const char* body = vm_builtin_sarg(args, argc, 1); char* rb = gmb_net_http_body("POST", url, body); vm_builtin_rstr(rb, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
void gmb_bf_840_c842(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* url = vm_builtin_sarg(args, argc, 0); const char* body = vm_builtin_sarg(args, argc, 1); char* rb = gmb_net_http_body("PUT", url, body); vm_builtin_rstr(rb, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
void gmb_bf_840_c843(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* url = vm_builtin_sarg(args, argc, 0); char* rb = gmb_net_http_body("DELETE", url, NULL); vm_builtin_rstr(rb, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_740(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char fb[256] = "";
            rdd_select_area(args, argc);
            if (argc >= 2 && args[1].type == 2)
                gmb_rdd_get_field_by_name(vm_builtin_sarg(args, argc, 1), fb, sizeof(fb));
            else
                gmb_rdd_get_field(argc >= 2 ? (int)args[1].num - 1 : 0, fb, sizeof(fb));
            /* Valore numerico → NUMBER (display con il padding Clipper del gestito); testo → STRING. */
            { double xv = 0; if (!gmb_rdd_area_is_text() && gmb_is_numeric(fb) && fb[0]) { xv = strtod(fb, NULL); bout->is_num = 1; bout->result = xv; }
              else { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(fb); bout->result_ref_bt = GMB_BT_STRING; } }
        }
}
static void gmb_bf_741_c741(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc);
    { int si = (argc >= 2 && args[1].type == 2) ? gmb_rdd_field_index(vm_builtin_sarg(args, argc, 1)) : (argc >= 2 ? (int)args[1].num - 1 : 0);
      bout->result = (double)gmb_rdd_get_field_state(si); }
}
static void gmb_bf_741_c742(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* TODAY() e' una DATA: il mondo gestito la espone come NUMERO (DateType → seriale OA,
     * vedi IlRuntime), non come stringa. Ritornando una stringa, il confronto `> 0` dei sample
     * dava falso e la VM/C divergeva (§ MiniTests/dbf/rdd_fields.bas). */
    (void)args; (void)argc;
    bout->is_num = 1;
    bout->result = (double)gmb_dt_today();
}
static void gmb_bf_741_c743(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* RDD.CTOD ritorna una DATA: nel mondo gestito e' un valore data/seriale OA (un DateType assegnato a una variabile
     * numerica ne conserva il seriale). Come stringa ISO, `d2 = RDD.CTOD("2026-12-25")` su variabile di tipo data/numerico
     * dava 2026-12-25 = 1989 come numero e RDD.DTOC(d2) 1905-07-18. Seriale OA come IL/C99/C99Native. */
    { const char* cs = vm_builtin_sarg(args, argc, 0); gmb_string_t gs = {0}; gs.data = (char*)(cs ? cs : ""); gs.length = strlen(gs.data);
      bout->is_num = 1; bout->result = (cs && *cs) ? (double)gmb_dt_ctod(gs) : 0.0; }
}
static void gmb_bf_741_c744(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* DATA come NUMERO (seriale OA: variabile non tipizzata/DOUBLE che ha ricevuto RDD.CTOD) -> ISO; come stringa ISO -> normalizzata. */
    if (argc >= 1 && args[0].type == 1 && args[0].num > 0) {
        gmb_string_t is = gmb_dt_dtoc_iso(args[0].num);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(is.data ? is.data : ""); bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    const char* dc = gmb_rdd_dtoc(vm_builtin_sarg(args, argc, 0)); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(dc); bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_741_c745(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = argc >= 1 ? args[0].num : 0;
}
static void gmb_bf_741_c746(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = argc >= 1 ? args[0].num : 0;
}
static void gmb_bf_741_c747(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char vb[32] = ""; rdd_select_area(args, argc); gmb_rdd_driver(vb, sizeof(vb)); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(vb); bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_741_c748(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* RDDBUFFER: prima l'arm chiamava gmb_rdd_used() — un bit "area aperta" che rendeva
     * BUFFER() ininfluente. Ora invoca il row-buffer del runtime condiviso (ADR §2). */
    bout->result = (double)gmb_rdd_buffer(argc >= 1 ? (int)args[0].num : 0, argc >= 2 ? (int)args[1].num : 0);
}
static void gmb_bf_741_c749(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc); bout->result = (double)gmb_rdd_table_revert();
}
static void gmb_bf_741_c750(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc); bout->result = (double)gmb_rdd_table_update();
}
static void gmb_bf_741_c751(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc);
    { int oi = (argc >= 2 && args[1].type == 2) ? gmb_rdd_field_index(vm_builtin_sarg(args, argc, 1)) : (argc >= 2 ? (int)args[1].num - 1 : 0);
      const char* ov = gmb_rdd_oldval(oi);
      double xv = 0;
      if (gmb_is_numeric(ov) && ov[0]) { xv = strtod(ov, NULL); bout->is_num = 1; bout->result = xv; }
      else { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(ov); bout->result_ref_bt = GMB_BT_STRING; } }
}
void gmb_bf_741_c752(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc); bout->result = gmb_db_current_isam() > 0 ? 1.0 : (double)gmb_rdd_rlock();
}
void gmb_bf_741_c753(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc); bout->result = gmb_db_current_isam() > 0 ? 1.0 : (double)gmb_rdd_flock();
}
static void gmb_bf_741_c754(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* -1 = successo (convenzione del mondo gestito): gmb_rdd_set_deleted ritorna 0 = ok
     * (convenzione C) → il valore esposto va tradotto (§ MiniTests/dbf/rdd_fields.bas). */
    rdd_select_area(args, argc);
    bout->result = gmb_rdd_set_deleted(argc >= 2 ? (int)args[1].num : 0) == 0 ? -1.0 : 0.0;
}
static void gmb_bf_741_c755(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* DELETE(...) ritorna il NUMERO di record cancellati (0 se non c'è un record corrente),
     * come IL/PCode — prima si ritornava il valore grezzo del runtime, che è **-1 = fallito**,
     * letto come TRUE dal programma (§ sample RDD_Omonime: IL 0 e VM/C -1). */
    rdd_select_area(args, argc); bout->result = gmb_rdd_delete() == 0 ? 1.0 : 0.0;
}
static void gmb_bf_741_c756(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* RECALL(scope, expr): il mondo gestito ritorna il NUMERO di record recallati (2 nello
     * sample), non 0 = ok (§ MiniTests/dbf/rdd_fields.bas). */
    const char* rcScope = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    rdd_select_area(args, argc);
    bout->result = (double)gmb_rdd_recall_scoped(rcScope);
}
static void gmb_bf_741_c757(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    rdd_select_area(args, argc); int f = (argc >= 2 && args[1].type == 2) ? gmb_rdd_field_index(vm_builtin_sarg(args, argc, 1)) : (argc >= 2 ? (int)args[1].num - 1 : 0);
    /* REPLACE(...): conteggio dei campi sostituiti (0 se fallito) — stessa conversione di DELETE. */
    bout->result = gmb_rdd_replace_field(f, vm_builtin_sarg(args, argc, 2)) == 0 ? 1.0 : 0.0;
}
static void gmb_bf_701(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            char* pd = gmb_provider_discover(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), vm_builtin_sarg(args, argc, 2), vm_builtin_sarg(args, argc, 3), argc >= 5 ? (int)args[4].num : 0);
            vm_builtin_rstr(pd, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
        }

        
}
void gmb_bf_733_c733(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_brain_add_watch(vm_builtin_sarg(args, argc, 0));
}
void gmb_bf_733_c734(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_brain_import_file(vm_builtin_sarg(args, argc, 0), 0) > 0);
}
void gmb_bf_733_c735(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_brain_add(vm_builtin_sarg(args, argc, 0), "Note", NULL, 0) > 0);
}
void gmb_bf_733_c736(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_brain_refresh(0);
}
void gmb_bf_733_c737(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)(gmb_brain_add(vm_builtin_sarg(args, argc, 0), "Fact", NULL, 0) > 0);
}
void gmb_bf_733_c738(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    bout->result = (double)gmb_brain_forget((long)(argc >= 1 ? args[0].num : 0));
}
void gmb_bf_733_c739(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    /* MEMORY.ASK (739) = brain del runtime OSPITE quando c'è (IlRuntime.MemoryAsk): stessa
     * risposta dei motori managed. MEMORY.SAVE/FORGET (737/738) passano da gmb_mem_store/delete,
     * già ponte. */
    if (func_code == 739)
    {
        char tk[16];
        snprintf(tk, sizeof tk, "%d", argc >= 2 && args[1].type == 1 ? (int)args[1].num : 5);
        if (bsp_hook_str("memory.ask", vm_builtin_sarg(args, argc, 0), tk, bout)) return;
    }
    char* bs = gmb_brain_search(vm_builtin_sarg(args, argc, 0), argc >= 2 ? (int)args[1].num : 5, 0); vm_builtin_rstr(bs, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1213_c1213(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.ADD(path) — parità IlRuntime.AiPluginsAdd (C#): copia plugin da
     * percorso sorgente nell'archivio (directory di plugin o singolo PLUGIN.md). */
    bout->result = (double)llmengine_plugin_add(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_1213_c1220(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.MARKETPLACE(url, dest?) — git clone */ bout->result = (double)gmb_ai_plugins_marketplace(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
}
static void gmb_bf_1218_c1217(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.LIST() -> lista stringhe ordinata (parita' PluginIndex.Scan C#) */
    static char names[256][64]; int n = gmb_plugins_scan(names, 256);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (strcasecmp(names[i], names[j]) > 0) { char tmp[64]; memcpy(tmp, names[i], 64); memcpy(names[i], names[j], 64); memcpy(names[j], tmp, 64); }
    gmb_obj_shared_val_t* out = gmb_shared_list_new();
    if (out) for (int i = 0; i < n; i++) gmb_list_add(out->list, gmb_make_val_string_counted(gmb_string_alloc(names[i]).data));
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)out; bout->result_ref_bt = GMB_BT_OBJECT;
}
static void gmb_bf_1218_c1218(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.ENABLE(name) */ bout->result = (double)llmengine_plugin_set_enabled(vm_builtin_sarg(args, argc, 0), 1);
}
static void gmb_bf_1218_c1219(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.DISABLE(name) */ bout->result = (double)llmengine_plugin_set_enabled(vm_builtin_sarg(args, argc, 0), 0);
}
static void gmb_bf_1218_c1221(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.CREATE(name, desc) → scheletro COMPLETO (PLUGIN.md + tool.json),
     * parità PluginRegistry.Create (C#): su VM/C l'archivio è la cartella, non il
     * registro in-process (llmengine_plugin_register NON scrive file). */
    bout->result = (double)llmengine_plugin_create(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
}
static int g_plugin_call_hook_installed = 0;
static char* llmengine_plugin_call_invoke(const char* name, const char* arg, void* ctx);
/* Contesto dell'hook: vm + task corrente (il builtin conosce entrambi). */
typedef struct { gmb_vm_t* vm; gmb_task_t* task; } llmengine_plugin_call_ctx_t;
static llmengine_plugin_call_ctx_t g_plugin_call_ctx;
static void gmb_bf_1218_c1222(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    /* AI.PLUGINS.CALL(name, arg) -> string. Il runtime C non conosce la VM: qui (dove
     * vm+task ci sono) installiamo l'hook di invocazione reale — risolve la procedura
     * del plugin con gmb_vm_proc_addr e la invoca in sync sul task corrente (stesso
     * schema dell'executor MCP stdio in gmb_mcp_exec.c: frame sync + gmb_vm_tick). */
    if (vm && task) {
        if (!g_plugin_call_hook_installed) { g_plugin_call_hook_installed = 1; llmengine_plugin_set_call_hook(llmengine_plugin_call_invoke, &g_plugin_call_ctx); }
        g_plugin_call_ctx.vm = vm; g_plugin_call_ctx.task = task;   /* task corrente, sempre */
    }
    char* pc = llmengine_plugin_call(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
    vm_builtin_rstr(pc, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
/* Hook: invoca la procedura <name> del plugin sul task corrente (frame sync). Il nome
 * della procedura = MakeProcName(name) (PascalCase, parità PluginRegistry.MakeProcName). */
static char* llmengine_plugin_call_invoke(const char* name, const char* arg, void* ctx)
{
    llmengine_plugin_call_ctx_t* c = (llmengine_plugin_call_ctx_t*)ctx;
    gmb_vm_t* vm = c ? c->vm : NULL;
    gmb_task_t* task = c ? c->task : NULL;
    if (!vm || !task || !vm->chunk || !name || !*name) return NULL;
    /* parità PluginRegistry.Call (C#): `Get(name)` = registro in-process OPPURE scansione del
     * disco (PluginIndex.Scan) → NON serve `AI PLUGINS ENABLE`, e la procedura da invocare è
     * `procedures[0]` di PLUGIN.md, NON il nome del plugin convertito in CamelCase (per
     * `report` è `ReportGenerate`, non `Report`; per `web-fetch` coincide, ed è per questo che
     * la vecchia euristica sembrava corretta). Divergenza chiusa 2026-10-31. */
    char* procname = gmb_plugin_procedure(name);
    if (!procname || !*procname) { free(procname); return NULL; }
    char proc[128];
    snprintf(proc, sizeof(proc), "%s", procname);
    free(procname);
    int addr = gmb_vm_proc_addr(vm, proc);
    if (addr < 0) return NULL;   /* procedura assente → fallback dichiarativo */
    /* Quanti parametri ha la procedura? Il chunk ha `arity_addrs`/`arity_vals` in lockstep con
     * `proc_addrs` (gmb_compiler.c `cc_proc`): senza ARITY i parametri successivi al primo non
     * vengono riempiti e finiscono su quello sbagliato (osservato: param2 = argomento, param1 = 0
     * per `FUNCTION ReportGenerate(rows, title)`). */
    int nargs = 1;
    for (uint32_t i = 0; i < vm->chunk->arity_count; i++)
        if ((int)vm->chunk->arity_addrs[i] == addr) { nargs = (int)vm->chunk->arity_vals[i]; break; }
    if (nargs < 1) nargs = 1;
    /* Argomento: stessa regola di `mcp_push_args` per un tipo NON dichiarato — se il testo è un
     * numero lo si impila come NUMERO, altrimenti come STRINGA (la VM converte numero→stringa
     * quando il parametro è STRING: `F(s AS STRING)` con "42" riceve "42"). Gli altri parametri
     * restano al default: STRINGA VUOTA (un parametro numerico la converte in 0, come C#). */
    {
        if (arg && *arg) {
            char* endp = NULL;
            double d = strtod(arg, &endp);
            if (endp && endp != arg && *endp == '\0') gmb_task_push_num(task, d, GMB_BT_NUMBER);
            else {
                gmb_obj_string_t* o = gmb_str_cstr(arg);
                if (o) { gmb_task_push_ref(task, (gmb_obj_t*)o, GMB_BT_STRING); gmb_str_release(o); }
                else gmb_task_push_num(task, 0, GMB_BT_NUMBER);
            }
        } else {
            gmb_task_push_num(task, 0, GMB_BT_NUMBER);
        }
        for (int k = 1; k < nargs; k++) {
            gmb_obj_string_t* e = gmb_str_cstr("");
            if (e) { gmb_task_push_ref(task, (gmb_obj_t*)e, GMB_BT_STRING); gmb_str_release(e); }
            else gmb_task_push_num(task, 0, GMB_BT_NUMBER);
        }
    }
    /* frame sync + tick (schema gmb_mcp_exec.c stdio) */
    int saved_ip = task->ip, saved_sync = task->sync_active, saved_sip = task->sync_saved_ip;
    double saved_sr = task->sync_result;
    gmb_obj_t* saved_sref = task->sync_result_ref;
    uint8_t saved_st = task->sync_result_type;
    if (gmb_vm_push_sync_frame(task, nargs) != 0) return NULL;
    task->sync_saved_ip = saved_ip; task->sync_active = 1; task->sync_result = 0;
    task->sync_result_ref = NULL; task->sync_result_type = 0;
    task->ip = addr;
    int guard = 0;
    while (task->sync_active && guard++ < 4000000) { if (gmb_vm_tick(vm, task) < 0) break; }
    /* risultato: usa lo stesso schema di mcp_result_text (adapter stack→val→stringa) */
    char* out = NULL;
    {
        gmb_stack_entry_t rv; memset(&rv, 0, sizeof(rv));
        rv.type = task->sync_result_type; rv.num = task->sync_result; rv.ref = task->sync_result_ref;
        rv.basic_type = (task->sync_result_type == 2) ? GMB_BT_STRING : GMB_BT_NUMBER;
        gmb_val_t v = gmb_adapter_stack_to_val(rv);
        gmb_string_t s = gmb_val_to_string(v);
        out = strdup((s.data && *s.data) ? s.data : "");
        gmb_string_free(&s);
    }
    task->ip = saved_ip; task->sync_active = saved_sync; task->sync_saved_ip = saved_sip;
    task->sync_result = saved_sr; task->sync_result_ref = saved_sref; task->sync_result_type = saved_st;
    return out;
}
static void gmb_bf_1218_c1223(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.INFO(name) -> string */ { char* pi = llmengine_plugin_info(vm_builtin_sarg(args, argc, 0)); vm_builtin_rstr(pi, &bout->is_num, &bout->result_ref, &bout->result_ref_bt); }
}
static void gmb_bf_1218_c1224(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINLIST() -> il REGISTRO in-process (parità PluginRegistry.List C#),
     * NON la scansione disco (quella è AI.PLUGINSLIST = llmengine_plugin_list_names). */
    { gmb_list_t pl = llmengine_plugin_list_registry(); gmb_obj_shared_val_t* plo = gmb_shared_list_new(); if (plo) { for (size_t pi = 0; pi < pl.count; pi++) { gmb_string_t st = gmb_val_to_string(pl.items[pi]); gmb_list_add(plo->list, gmb_make_val_string_counted(st.data)); } } bout->is_num = 0; bout->result_ref = (gmb_obj_t*)plo; bout->result_ref_bt = GMB_BT_OBJECT; }
}
static void gmb_bf_766(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        {
            /* SYSTEM.UDTPACKED(udt) = numero di campi. Il conteggio è MEMORIZZATO una volta
             * nel descriptor __class__.FieldNames (dict offset→nome): size O(1), nessun
             * conteggio dei campi né del dict-istanza. Parità IL/PCode (registro) e C99
             * (costante compile-time). Prima contava le entry del dict istanza (= nfields+1,
             * includeva __class__). */
            bout->result = 0.0;
            if (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_DICT)
            {
                gmb_obj_shared_val_t* inst = (gmb_obj_shared_val_t*)args[0].ref;
                gmb_val_t cv = gmb_dict_get(*inst->dict, "__class__");
                if (cv.kind == GMB_TYPE_OBJECT && cv.v_ptr && ((gmb_obj_t*)cv.v_ptr)->kind == GMB_OBJ_DICT)
                {
                    gmb_val_t fnv = gmb_dict_get(*((gmb_obj_shared_val_t*)cv.v_ptr)->dict, "FieldNames");
                    if (fnv.kind == GMB_TYPE_OBJECT && fnv.v_ptr && ((gmb_obj_t*)fnv.v_ptr)->kind == GMB_OBJ_DICT)
                    {
                        bout->result = (double)((gmb_obj_shared_val_t*)fnv.v_ptr)->dict->count;
                        return;
                    }
                }
                bout->result = (double)inst->dict->count;
            }
        }
}
static void gmb_bf_599(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* SYSTEM.UDTALIGNED(udt) = 1. Prima la tabella mappava 599 sul handler 598
     * (dict->count) → ritornava nfields+1. Parità IL/PCode/C99. */
    bout->result = 1.0;
}
static void gmb_bf_1214_c1214(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.REMOVE(name) */ bout->result = (double)llmengine_plugin_remove(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_1214_c1215(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.REFRESH() */ bout->result = (double)llmengine_plugin_refresh();
}
static void gmb_bf_1214_c1216(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PLUGINS.SETINDEX(name) */ bout->result = (double)llmengine_plugin_set_index(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_1214_c1225(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PATH.PLUGINADD(path) */ bout->result = (double)gmb_ai_path_model_add(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_1214_c1226(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.PATH.PLUGINREMOVE(path) */ bout->result = (double)gmb_ai_path_model_remove(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_768_c768(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* WINDOWS.X(): posizione X della finestra attiva (headless -1). */
    bout->result = gmb_window_x();
}
static void gmb_bf_768_c769(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* WINDOWS.Y(): posizione Y della finestra attiva (headless -1). */
    bout->result = gmb_window_y();
}
static void gmb_bf_768_c770(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* WINDOWS.WIDTH(): larghezza finestra attiva (runtime condiviso). */
    bout->result = gmb_window_width();
}
static void gmb_bf_768_c771(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* WINDOWS.HEIGHT(): altezza finestra attiva (runtime condiviso). Prima era uno stub -1. */
    bout->result = gmb_window_height();
}
static void gmb_bf_768_c830(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* WINDOWS.THEME(): tema corrente — nome canonico dal registro tema del runtime. */
    gmb_obj_string_t* s = gmb_str_cstr(gmb_window_theme());
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_768_c831(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* WINDOWS.DESKTOP(): desktop corrente (default 1). */
    bout->result = gmb_window_desktop();
}
static void gmb_bf_1227(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
        char* pd2 = gmb_process_document(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1)); vm_builtin_rstr(pd2, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1265_c1265(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* hf = gmb_ai_set_hf_token(vm_builtin_sarg(args, argc, 0));
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(hf ? hf : ""); bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1265_c1266(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* hf = gmb_ai_set_hf_repo(vm_builtin_sarg(args, argc, 0));
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(hf ? hf : ""); bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_1265_c1267(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* hf = gmb_ai_hf_repo();
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(hf ? hf : ""); bout->result_ref_bt = GMB_BT_STRING;
}


 // ── PROVENANCE (Second Brain §2): firma/verifica HMAC-SHA256 con chiave condivisa ──
 // 1327 = SIGNNOTE(content, key) → firma hex | 1328 = VERIFYNOTE(content, key, sig) → 1/0.
 // Il runtime C condiviso (gmb_provenance_sign/verify in gmb_crypto.c) è l'unica
 // implementazione; la semantica vault/nota (risoluzione path) resta C#-side.
 static void gmb_bf_provenance(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
 {
     if (func_code == 1327)
     {
         const char* content = vm_builtin_sarg(args, argc, 0);
         const char* key = vm_builtin_sarg(args, argc, 1);
         if (!content || !key) { vm_error(task, "SIGNNOTE richiede content e key"); return; }
         char* sig = gmb_provenance_sign((const uint8_t*)content, strlen(content), key, strlen(key));
         bout->is_num = 0;
         bout->result_ref = (gmb_obj_t*)gmb_str_cstr(sig ? sig : "");
         bout->result_ref_bt = GMB_BT_STRING;
         if (sig) free(sig);
         return;
     }
     if (func_code == 1328)
     {
         const char* content = vm_builtin_sarg(args, argc, 0);
         const char* key = vm_builtin_sarg(args, argc, 1);
         const char* sig = vm_builtin_sarg(args, argc, 2);
         if (!content || !key || !sig) { vm_error(task, "VERIFYNOTE richiede content, key e sig"); return; }
         int ok = gmb_provenance_verify((const uint8_t*)content, strlen(content), key, strlen(key), sig);
         bout->is_num = 1;
         /* Parità con l'oracolo (IlRuntime.VerifyNote -> ProvenanceCrypto.Verify ? 1.0 : 0.0):
          * firma AUTENTICA = 1, altrimenti 0. Prima ritornava -1 quando autentica (valore
          * inventato: faceva divergere il sample Provenance: IL/PCode 1 vs VM/C -1). */
         bout->result = ok ? 1 : 0;
         bout->result_ref_bt = GMB_BT_NUMBER;
         return;
     }
     vm_error(task, "builtin provenance non implementato");
 }

 // ── PROVENANCE share (F7 P2): AES-256-GCM con password, base64 (parità C#) ──
 // 1337 = SHAREENCRYPT(content, key) → base64 | 1338 = SHAREDECRYPT(cipher_b64, key) → testo.
 // Formato [salt16|nonce12|tag16|ct] identico a C# CoreAesEncrypt.
 static void gmb_bf_share(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
 {
     if (func_code == 1337)
     {
         const char* content = vm_builtin_sarg(args, argc, 0);
         const char* key = vm_builtin_sarg(args, argc, 1);
         if (!content || !key) { vm_error(task, "SHAREENCRYPT richiede content e key"); return; }
         size_t olen = 0;
         uint8_t* enc = gmb_aes_encrypt_password((const uint8_t*)content, strlen(content), key, &olen);
         char* b64 = enc ? gmb_base64_encode(enc, olen) : NULL;
         if (enc) free(enc);
         bout->is_num = 0;
         bout->result_ref = (gmb_obj_t*)gmb_str_cstr(b64 ? b64 : "");
         bout->result_ref_bt = GMB_BT_STRING;
         if (b64) free(b64);
         return;
     }
     if (func_code == 1338)
     {
         const char* b64 = vm_builtin_sarg(args, argc, 0);
         const char* key = vm_builtin_sarg(args, argc, 1);
         if (!b64 || !key) { vm_error(task, "SHAREDECRYPT richiede cipher e key"); return; }
         size_t elen = 0;
         uint8_t* enc = gmb_base64_decode(b64, &elen);
         if (!enc) {
             bout->is_num = 0;
             bout->result_ref = (gmb_obj_t*)gmb_str_cstr("ERROR: The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
             bout->result_ref_bt = GMB_BT_STRING;
             return;
         }
         size_t olen = 0;
         uint8_t* out = gmb_aes_decrypt_password(enc, elen, key, &olen);
         free(enc);
         if (!out) {
             bout->is_num = 0;
             bout->result_ref = (gmb_obj_t*)gmb_str_cstr("ERROR: The input is not a valid Base-64 string as it contains a non-base 64 character, more than two padding characters, or an illegal character among the padding characters.");
             bout->result_ref_bt = GMB_BT_STRING;
             return;
         }
         char* nul = (char*)malloc(olen + 1);
         if (nul) { memcpy(nul, out, olen); nul[olen] = '\0'; }
         free(out);
         bout->is_num = 0;
         bout->result_ref = (gmb_obj_t*)gmb_str_cstr(nul ? nul : "");
         bout->result_ref_bt = GMB_BT_STRING;
         if (nul) free(nul);
         return;
     }
     vm_error(task, "builtin share non implementato");
 }

 // ── SECOND BRAIN gap (F7 P3): folder props / link manuali / sync / web scrape ──
 // 1341-1343 VAULT ADD/REMOVE/SET FOLDER; 1344-1346 NOTE LINK/UNLINK/SET ALIAS;
 // 1347 SYNC FOLDER; 1348 INGEST WEB. Logica nel runtime condiviso gmb_secondbrain.c.
 static void gmb_bf_secondbrain_c1341(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* folder = vm_builtin_sarg(args, argc, 1);
        const char* idx = vm_builtin_sarg(args, argc, 2);
        int indexed = idx && strcmp(idx, "0") != 0;
        if (!name || !folder) { vm_error(task, "VAULT.ADDFOLDER richiede name e folder"); return; }
        bout->is_num = 1; bout->result = (double)gmb_vault_add_folder_bool(name, folder, indexed, NULL);
}
static void gmb_bf_secondbrain_c1342(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* folder = vm_builtin_sarg(args, argc, 1);
        if (!name || !folder) { vm_error(task, "VAULT.REMOVEFOLDER richiede name e folder"); return; }
        bout->is_num = 1; bout->result = (double)gmb_vault_folder_remove(name, folder);
}
static void gmb_bf_secondbrain_c1343(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* folder = vm_builtin_sarg(args, argc, 1);
        const char* idx = vm_builtin_sarg(args, argc, 2);
        if (!name || !folder) { vm_error(task, "VAULT.SETFOLDERPROPS richiede name e folder"); return; }
        int idxv = idx && strcmp(idx, "0") != 0;
        bout->is_num = 1; bout->result = (double)gmb_vault_folder_set(name, folder, &idxv, NULL);
}
static void gmb_bf_secondbrain_c1344(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* from = vm_builtin_sarg(args, argc, 1);
        const char* to = vm_builtin_sarg(args, argc, 2);
        const char* alias = vm_builtin_sarg(args, argc, 3);
        if (!name || !from || !to) { vm_error(task, "NOTE.LINK richiede vault/from/to"); return; }
        bout->is_num = 1; bout->result = (double)gmb_note_link(name, from, to, alias);
}
static void gmb_bf_secondbrain_c1345(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* from = vm_builtin_sarg(args, argc, 1);
        const char* to = vm_builtin_sarg(args, argc, 2);
        if (!name || !from || !to) { vm_error(task, "NOTE.UNLINK richiede vault/from/to"); return; }
        bout->is_num = 1; bout->result = (double)gmb_note_unlink(name, from, to);
}
static void gmb_bf_secondbrain_c1346(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* from = vm_builtin_sarg(args, argc, 1);
        const char* to = vm_builtin_sarg(args, argc, 2);
        const char* alias = vm_builtin_sarg(args, argc, 3);
        if (!name || !from || !to || !alias) { vm_error(task, "NOTE.SETALIAS richiede vault/from/to/alias"); return; }
        bout->is_num = 1; bout->result = (double)gmb_note_set_alias(name, from, to, alias);
}
static void gmb_bf_secondbrain_c1347(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* src = vm_builtin_sarg(args, argc, 1);
        const char* dir = vm_builtin_sarg(args, argc, 2);
        const char* conflict = vm_builtin_sarg(args, argc, 3);
        if (!name || !src) { vm_error(task, "SYNC.FOLDER richiede vault e src"); return; }
        char* r = gmb_sync_folder(name, src, dir, conflict);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1348(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
        const char* url = vm_builtin_sarg(args, argc, 1);
        const char* rel = vm_builtin_sarg(args, argc, 2);
        if (!name || !url) { vm_error(task, "INGEST.WEB richiede vault e url"); return; }
        char* r = gmb_web_scrape(name, url, rel);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
/* ── INGEST FILE/FOLDER (1316/1317, porting Ingestor — §0bis): il converter binario
   (BinaryTextExtractor C#) è gmb_doc_convert, linkato qui (gmb_builtin.c include gmb_doc.h).
   Il chunk A1 porta gli args del parser = [source, rel]/[source, preserve] SENZA vault
   (come PCode, che antepone "" a dispatch): il runtime risolve "" al brain attivo. */
static int g_ingest_converter_registered = 0;
static void gmb_ingest_ensure_converter(void)
{
    if (g_ingest_converter_registered) return;
    gmb_ingest_register_converter(gmb_doc_convert);
    g_ingest_converter_registered = 1;
}
static void gmb_bf_secondbrain_c1316(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* source = vm_builtin_sarg(args, argc, 0);
        const char* rel = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        if (!source) { vm_error(task, "INGEST.FILE richiede source"); return; }
        gmb_ingest_ensure_converter();
        char* r = gmb_ingest_import_file("", source, (rel && *rel) ? rel : NULL, 1, 1);
        bout->is_num = 1; bout->result = r ? 1.0 : 0.0;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1317(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* source = vm_builtin_sarg(args, argc, 0);
        int preserve = argc > 1 ? (int)args[1].num : 1;
        if (!source) { vm_error(task, "INGEST.FOLDER richiede source"); return; }
        gmb_ingest_ensure_converter();
        long long r = gmb_ingest_import_folder("", source, preserve, 1, 1);
        bout->is_num = 1; bout->result = (double)r;
}
static void gmb_bf_secondbrain_c1288(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* 1288 = GRAPH.QUERYMULTI(query, brains_csv) — prima chiamava STATS (mis-mapping).
     * brains è una CSV (parità IL SplitNames), NON un array. */
    const char* query = vm_builtin_sarg(args, argc, 0);
    const char* brains = argc > 1 ? vm_builtin_sarg(args, argc, 1) : "";
    char* r = gmb_graph_query_multi(query ? query : "", brains ? brains : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}

/* GRAPH BUILD/UPDATE <wikiDir> [, outputDir] (1320/1321): scansiona il vault e scrive
 * <outputDir>/<basename>.json (runtime condiviso gmb_graph_build). */
static void gmb_bf_graph_build(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code;
    const char* dir = vm_builtin_sarg(args, argc, 0);
    const char* out = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
    if (!dir || !*dir) { vm_error(task, "GRAPH BUILD richiede la cartella del vault"); return; }
    int64_t n = gmb_graph_build(dir, out);
    bout->is_num = 1; bout->result = (double)n;
}
/* GRAPH FIND PATH <from> TO <to> (1322): path più corto nel grafo attivo. */
static void gmb_bf_graph_findpath(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code;
    const char* from = vm_builtin_sarg(args, argc, 0);
    const char* to = vm_builtin_sarg(args, argc, 1);
    if (!from || !to) { vm_error(task, "GRAPH FIND PATH richiede from e to"); return; }
    char* r = gmb_graph_brain_query(gmb_graph_active_name(), "PATH", from, to, 0);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
/* GRAPH RDD <MEMORY|JSON|SQLITE> (1323): seleziona il backend (ISAM/DBF → errore esplicito). */
static void gmb_bf_graph_rdd(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code;
    const char* backend = vm_builtin_sarg(args, argc, 0);
    if (gmb_graph_rdd_set(backend) != 0) { vm_error(task, "GRAPH RDD backend non disponibile (usa MEMORY, JSON o SQLITE)"); return; }
    bout->is_num = 1; bout->result = 0;
}
static void gmb_bf_secondbrain_c1289(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "NEIGHBORS", a1, a2, k);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1290(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "PATH", a1, a2, k);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1293(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "STATS", a1, a2, k);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
/* BRAINS.ADJACENCY (3899): nodi + archi pesati del cervello — ponte verso GRAPH/FLOW.
 * Il JSON lo costruisce gmb_graph_brain_query (op "ADJACENCY"), ordine canonico condiviso
 * con il mondo C# (GraphMultiQuery.Adjacency). */
static void gmb_bf_secondbrain_c3899(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
    if (!brain) { vm_error(task, "BRAINS.ADJACENCY richiede il nome del cervello"); return; }
    char* r = gmb_graph_brain_query(brain, "ADJACENCY", NULL, NULL, 0);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_secondbrain_c1332(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "DOT", a1, a2, k);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1334(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "BYTAG", a1, a2, k);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1335(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "BYFOLDER", a1, a2, k);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1336(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* brain = vm_builtin_sarg(args, argc, 0);
        if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
        const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
        int k = argc > 3 ? (int)args[3].num : 0;
        char* r = gmb_graph_brain_query(brain, "ISVALID", a1, a2, k);
        /* ISVALID ritorna un numero (IL/PCode: DoubleType → gmb_print_double col padding). */
        bout->is_num = 1; bout->result = r ? atof(r) : 0.0;
        if (r) free(r);
}
void gmb_bf_secondbrain_c1282(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* r = gmb_brain_registry_list();
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
void gmb_bf_secondbrain_c1284(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    char* r = gmb_brain_used_csv_json();
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
void gmb_bf_secondbrain_c1283(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* n = vm_builtin_sarg(args, argc, 0);
        char* r = n ? gmb_brain_registry_info(n) : NULL;
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "{}"); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1324(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* s = vm_builtin_sarg(args, argc, 0);
        const char* p = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        { const char* _pv = gmb_provider_active("completion");
          if (!_pv || !*_pv || strcmp(gmb_provider_kind(_pv), "fixture") == 0) {
              bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("Error: nessun provider LLM configurato (AI SET ACTIVE)"); bout->result_ref_bt = GMB_BT_STRING;
              return;
          } }
        char* r = gmb_llm_complete(s ? s : "", p ? p : "");
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1325(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* s = vm_builtin_sarg(args, argc, 0);
        const char* brain = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        int window = argc > 2 ? (int)args[2].num : 3;
        /* Sorgente UNICA della logica (ordine dei controlli di Enricher.QaAnswer) nel runtime
         * condiviso: prima qui c'era una scorciatoia che trattava QUALSIASI brain come non
         * disponibile (§0bis: una sola implementazione, niente varianti locali). */
        char* r = gmb_qa_answer(s ? s : "", brain ? brain : "", window);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1329(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    /* AI.BRAINVAULTBUILD(outputDir$): logica SORGENTE UNICA nel runtime condiviso
     * (gmb_brainvaultbuild, §0bis); prima qui c'era il corpo di Write/Query
     * (gmb_llm_complete) — assegnazione sbagliata trovata dal minitest BVB. */
    const char* s = vm_builtin_sarg(args, argc, 0);
    /* -2 = nessun provider embedding reale (parità oracolo: eccezione esplicita, non un
     * valore silenzioso). */
    double r = gmb_brainvaultbuild(s ? s : "");
    if (r == -2) { vm_error(task, gmb_brainvaultbuild_error()); return; }
    bout->is_num = 1;
    bout->result = r;
}
static void gmb_bf_secondbrain_c1330(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code; (void)args; (void)argc;
    /* AI.BRAINVAULTQUERY richiede un provider embedding attivo (parità BrainVault.Query C#):
     * errore ESPLICITO, mai silenzioso. Prima chiamava gmb_llm_complete (risultato vuoto). */
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr("ERROR: Operazione AI RAG richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)");
    bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_secondbrain_c1331(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* s = vm_builtin_sarg(args, argc, 0);
        const char* p = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
        { const char* _pv = gmb_provider_active("completion");
          if (!_pv || !*_pv || strcmp(gmb_provider_kind(_pv), "fixture") == 0) {
              bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("Error: nessun provider LLM configurato (AI SET ACTIVE)"); bout->result_ref_bt = GMB_BT_STRING;
              return;
          } }
        char* r = gmb_llm_complete(s ? s : "", p ? p : "");
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}
static void gmb_bf_secondbrain_c1326(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task; (void)func_code;
    const char* t = vm_builtin_sarg(args, argc, 0);
        char* r = gmb_llm_complete("Suggerisci tag", t ? t : "");
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
        if (r) free(r);
}

/* GRAPH.DIJKSTRA/HASCYCLE/ECCENTRICITY/DFS/BFS (1389-1393): il runtime condiviso
 * (gmb_graph_brain_query) implementa gli op, ma i codici NON erano nel dispatch → exit 6
 * "builtin non implementato" (gen_GRAPH_*). Stessa forma degli altri GRAPH.* (1288-1300). */
static void gmb_bf_graph_ops(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm;
    const char* brain = vm_builtin_sarg(args, argc, 0);
    if (!brain) { vm_error(task, "GRAPH richiede brain"); return; }
    const char* a1 = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
    const char* a2 = argc > 2 ? vm_builtin_sarg(args, argc, 2) : NULL;
    int k = argc > 3 ? (int)args[3].num : 0;
    const char* op = func_code == 1389 ? "DIJKSTRA"
                   : func_code == 1390 ? "HASCYCLE"
                   : func_code == 1391 ? "ECCENTRICITY"
                   : func_code == 1392 ? "DFS" : "BFS";
    char* r = gmb_graph_brain_query(brain, op, a1, a2, k);
    /* HASCYCLE ritorna un numero (IL/PCode: DoubleType → gmb_print_double col padding). */
    if (func_code == 1390) { bout->is_num = 1; bout->result = r ? atof(r) : 0.0; if (r) free(r); return; }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}


/* SYSTEM.SHELL (1401) / SHELLOUT (1402) / SHELLCODE (1403) — delega al runtime C condiviso
 * (gmb_shell.c, §0bis: mai inline). Opt-in via SET SHELL ON (gmb_shell_set_enabled). */
static void gmb_bf_shell_c1401(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    const char* cmd = vm_builtin_sarg(args, argc, 0);
    if (!cmd) { vm_error(task, "SHELL richiede un comando"); return; }
    if (!gmb_shell_is_enabled()) { vm_error(task, "SYSTEM.SHELL non abilitato: serve 'SET SHELL ON' (opt-in)."); return; }
    bout->is_num = 1; bout->result = (double)gmb_shell_exec(cmd);
}
static void gmb_bf_shell_c1402(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    const char* cmd = vm_builtin_sarg(args, argc, 0);
    if (!cmd) { vm_error(task, "SHELL richiede un comando"); return; }
    if (!gmb_shell_is_enabled()) { vm_error(task, "SYSTEM.SHELL non abilitato: serve 'SET SHELL ON' (opt-in)."); return; }
    { char* out = gmb_shell_out(cmd);
      if (!out) { vm_error(task, "SYSTEM.SHELLOUT: esecuzione fallita"); return; }
      vm_builtin_rstr(out, &bout->is_num, &bout->result_ref, &bout->result_ref_bt); }
}
static void gmb_bf_shell_c1403(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    const char* cmd = vm_builtin_sarg(args, argc, 0);
    if (!cmd) { vm_error(task, "SHELL richiede un comando"); return; }
    if (!gmb_shell_is_enabled()) { vm_error(task, "SYSTEM.SHELL non abilitato: serve 'SET SHELL ON' (opt-in)."); return; }
    bout->is_num = 1; bout->result = (double)gmb_shell_code(cmd);
}
/* SENDKEY (1408): invia testo al buffer typeahead — usa gmb_console_sendkey (gmb_runtime.h). */
static void gmb_bf_sendkey_c3614(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)task;
    const char* keys = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    gmb_console_sendkey(keys);
    bout->is_num = 1; bout->result = 0;
}
/* TUI controls (1404..1407): widget interattivi headless → ritornano ""/0 (parità emitter C#). */
void gmb_bf_tui_control_c3610(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task; (void)func_code;
    if (argc < 1) { bout->is_num = 1; bout->result = 0; return; }
    gmb_checkbox_t cb;
    int x = argc >= 1 ? (int)args[0].num : 0;
    int y = argc >= 2 ? (int)args[1].num : 0;
    const char* cap = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    gmb_checkbox_init(&cb, x, y, (int)strlen(cap) + 6, cap);
    gmb_tui_run_control_modal(&cb.ctrl);
    bout->is_num = 1; bout->result = cb.ival != 0 ? 1.0 : 0.0;
}
void gmb_bf_tui_toggle_c3611(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task; (void)func_code;
    if (argc < 1) { bout->is_num = 1; bout->result = 0; return; }
    gmb_toggle_button_t tg;
    int x = argc >= 1 ? (int)args[0].num : 0;
    int y = argc >= 2 ? (int)args[1].num : 0;
    const char* cap = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    gmb_toggle_button_init(&tg, x, y, (int)strlen(cap) + 6, cap);
    gmb_tui_run_control_modal(&tg.ctrl);
    bout->is_num = 1; bout->result = tg.ival != 0 ? 1.0 : 0.0;
}
void gmb_bf_tui_menu_c3612(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task; (void)func_code;
    if (argc < 1) { vm_builtin_rstr(strdup(""), &bout->is_num, &bout->result_ref, &bout->result_ref_bt); return; }
    gmb_menu_t mn;
    int x = argc >= 1 ? (int)args[0].num : 0;
    int y = argc >= 2 ? (int)args[1].num : 0;
    gmb_menu_init(&mn, x, y, 20);
    gmb_tui_run_control_modal(&mn.ctrl);
    vm_builtin_rstr(strdup(""), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
void gmb_bf_tui_listbox_c3613(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task; (void)func_code;
    if (argc < 1) { vm_builtin_rstr(strdup(""), &bout->is_num, &bout->result_ref, &bout->result_ref_bt); return; }
    gmb_listbox_t lb;
    int x = argc >= 1 ? (int)args[0].num : 0;
    int y = argc >= 2 ? (int)args[1].num : 0;
    int w = argc >= 3 ? (int)args[2].num : 0;
    int h = argc >= 4 ? (int)args[3].num : 0;
    gmb_listbox_init(&lb, x, y, w, h);
    gmb_tui_run_control_modal(&lb.ctrl);
    vm_builtin_rstr(strdup(""), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}


/* ---- Task 6.6: funzioni canoniche chiuse a BuiltInFunction (PCodeVM/IL 100%). ----
   DBF durabilità/replicazione/recno su area corrente (stesso contratto dei syscall
   DBF in gmb_syscall_db.c), MemoryMap (gmb_mmap_* di gmb_file.c). Runtime C
   condiviso (gmb_db_c / gmb_runtime_c) — mai stub. */
void gmb_bf_dbf_recno_c1361(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* RECNO dell'AREA ATTIVA: DBF se aperto, altrimenti l'area RDD (RDD USE … VIA "CSV").
     * Senza il fallback, `DBF GO TOP`/`SKIP` muovevano l'area RDD (vedi vm_sys_goskip_*) ma
     * RECNO leggeva il layer gmb_db (handle 0) → sempre 0, e i cicli `DO WHILE NOT EOF()`
     * non terminavano (misurato su samples/Databases/RDD_ReplaceRealData.bas). */
    {
        int64_t dh = gmb_db_current_dbf();
        /* Se l'area corrente e' ISAM (B+Tree), RECNO va letto dal motore ISAM: senza questo
         * ISAM.RECNO restava 0 (il layer gmb_db DBF non ha handle → 0), divergenza §3 vmc vs
         * il/pcode/c99. Parita' con i rami ISAM di EOF/LASTREC/FOUND sopra. */
        if (gmb_db_current_isam() > 0) bout->result = (double)gmb_db_isam_recno(gmb_db_current_isam());
        else if (dh > 0) bout->result = (double)gmb_db_dbf_recno(dh);
        /* Area RDD di tipo DBF (RDD USE x.dbf): IL legge il driver DBF corrente -> record corrente dell'area. */
        else if (gmb_rdd_cur_is_dbf()) bout->result = (double)gmb_rdd_recno();
        /* Area RDD: DBF.RECNO() torna 0 (parità IL — DbfRecno senza handle DBF → 0; il
         * recno dell'area RDD si legge con RDD.RECNO()). Prima tornava gmb_rdd_recno()=1
         * → `clienti: 1` vs il 0 (CustomerOrdersReport.bas). */
        else bout->result = 0.0;
    }
}
/* RDD.RECNO() (BuiltInFunction.RDDRECNO = 3745): come DBF.RECNO() quando c'e' un handle DBF/ISAM; SENZA handle ricade sull'area RDD
 * (`RDD USE x.csv`/`RDD USE x.dbf` ... `RDD.RECNO()` = record corrente dell'area RDD). DBF.RECNO() (1361) senza handle resta 0 (parita' IL). */
void gmb_bf_rdd_recno_c3745(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    {
        int64_t dh = gmb_db_current_dbf();
        if (gmb_db_current_isam() > 0) bout->result = (double)gmb_db_isam_recno(gmb_db_current_isam());
        else if (dh > 0) bout->result = (double)gmb_db_dbf_recno(dh);
        else bout->result = (double)gmb_rdd_recno();
    }
}
void gmb_bf_dbf_setdurability_c1350(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    char mode[256] = "";
    if (argc > 0) vm_entry_str(args[0], mode, (int)sizeof(mode));
    /* -1 = SUCCESSO, come IlRuntime.DbfSetDurability (il tipo dichiarato e' -1/0): prima era
     * invertito (0 su successo) e divergeva da IL/PCode (§ dbf_lock_filter.bas). */
    bout->result = gmb_db_dbf_set_durability(gmb_db_current_dbf(), mode) == GMB_DB_OK ? -1.0 : 0.0;
}
void gmb_bf_dbf_begincommit_c1351(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* -1 = SUCCESSO, come IlRuntime.DbfBeginGroupCommit. */
    bout->result = gmb_db_dbf_begin_group_commit(gmb_db_current_dbf()) == GMB_DB_OK ? -1.0 : 0.0;
}
void gmb_bf_dbf_endcommit_c1352(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* -1 = SUCCESSO, come IlRuntime.DbfEndGroupCommit. */
    bout->result = gmb_db_dbf_end_group_commit(gmb_db_current_dbf()) == GMB_DB_OK ? -1.0 : 0.0;
}
/* ── SPEED durability/group-commit/info (BuiltInFunction 454-458) ─────────────
 * Prima assenti: il VM rispondeva "builtin 458 non implementato". Runtime C
 * condiviso gmb_db_speed_* (§0bis). Booleani canonici -1/0, stringhe GMB_BT_STRING. */
void gmb_bf_speed_begingc_c454(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_db_speed_begin_group_commit(gmb_db_current_speed()) == GMB_DB_OK ? -1.0 : 0.0;
}
void gmb_bf_speed_endgc_c455(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_db_speed_end_group_commit(gmb_db_current_speed()) == GMB_DB_OK ? -1.0 : 0.0;
}
void gmb_bf_speed_flushdur_c456(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_db_speed_flush_durability(gmb_db_current_speed()) == GMB_DB_OK ? -1.0 : 0.0;
}
void gmb_bf_speed_info_c457(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    gmb_string_t s = gmb_db_speed_info_str(gmb_db_current_speed());
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(s.data ? s.data : "");
    bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_speed_setdur_c458(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char mode[256] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], mode, (int)sizeof(mode));
    gmb_string_t s = gmb_db_speed_set_durability_str(gmb_db_current_speed(), mode);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(s.data ? s.data : "");
    bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_dbf_setrepl_c1353(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    /* SETREPLICATION ritorna il PATH del log (StringType in StdLib, come IlRuntime.
     * DbfSetReplication) e il path è l'ULTIMO argomento: la StdLib dichiara SETREPLICATION(logPath)
     * e SETREPLICATION(fileNumber|alias, logPath). Prima ritornava 0/-1 (numero) e leggeva il
     * PRIMO argomento → il chiamante stampava una stringa vuota
     * (§ MiniTests/dbf/dbf_replication.bas: VMC "setrepl:"). */
    char logPath[1024] = "";
    int ok = 0, i;
    /* Il path è una STRINGA: si prova ogni argomento (in ordine inverso) finché il motore
     * accetta quello come log di replica. Non si assume la posizione: il primo argomento può
     * essere l'area/alias, che qui arriva come numero o nome. */
    for (i = argc - 1; i >= 0 && !ok; i--)
    {
        char cand[1024] = "";
        vm_entry_str(args[i], cand, (int)sizeof(cand));
        if (!cand[0]) continue;
        /* Si usa la funzione del RUNTIME condiviso, che oltre a impostare la replica ritorna
         * il path RISOLTO: `"."`/`"DEFAULT"` diventano `<percorso tabella>.repl`, come nel
         * motore di riferimento e come fa il backend IL. Prima il builtin ritornalva la stringa
         * in ingresso → `DBF.SETREPLICATION(Primary, ".")` dava `Replication log: .` sulla VM/C
         * contro il path completo su IL (misurato su DbfReplication.bas). */
        gmb_string_t rs = gmb_db_dbf_set_replication_str(gmb_db_current_dbf(), cand);
        if (rs.data && rs.data[0])
        {
            snprintf(logPath, sizeof(logPath), "%s", rs.data);
            ok = 1;
        }
        else if (cand[0] == '.' && (cand[1] == 0)) { /* non risolto: si prosegue col prossimo arg */ }
    }
    gmb_obj_string_t* s = gmb_str_cstr(ok ? logPath : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_isam_setrepl_c2112(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* ISAM.SETREPLICATION(logPath) / (alias, logPath) → path del log (stringa vuota se fallisce). */
    (void)vm; (void)func_code; (void)task;
    char logPath[1024] = "";
    int ok = 0, i;
    for (i = argc - 1; i >= 0 && !ok; i--)
    {
        char cand[1024] = "";
        vm_entry_str(args[i], cand, (int)sizeof(cand));
        if (!cand[0]) continue;
        if (gmb_db_isam_set_replication(gmb_db_current_isam(), cand) == 0)
        {
            snprintf(logPath, sizeof(logPath), "%s", cand);
            ok = 1;
        }
    }
    gmb_obj_string_t* s = gmb_str_cstr(ok ? logPath : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_isam_applyrepl_c2113(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* ISAM.APPLYREPLICATION(logPath) / (alias, logPath) → conteggio righe applicate. */
    (void)vm; (void)func_code; (void)task;
    char logPath[1024] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], logPath, (int)sizeof(logPath));
    bout->result = (double)gmb_db_isam_apply_replication(gmb_db_current_isam(), logPath);
}
void gmb_bf_isam_followrepl_c2114(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* ISAM.FOLLOWREPLICATION(logPath) / (alias, logPath, checkpoint) → conteggio. */
    (void)vm; (void)func_code; (void)task;
    char logPath[1024] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], logPath, (int)sizeof(logPath));
    bout->result = (double)gmb_db_isam_follow_replication(gmb_db_current_isam(), logPath, NULL);
}
void gmb_bf_dbf_applyrepl_c1354(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    char replLog[1024] = "";
    if (argc > 0) vm_entry_str(args[0], replLog, (int)sizeof(replLog));
    /* Come la syscall: il runtime ritorna il CONTEGGIO delle operazioni applicate (0 su errore),
     * non un bit. Trattarlo come booleano dava -1 con 5 operazioni applicate. */
    {
        int arc = gmb_db_dbf_apply_replication(gmb_db_current_dbf(), replLog);
        bout->result = arc > 0 ? (double)arc : 0.0;
    }
}
void gmb_bf_dbf_followrepl_c1355(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    char replLog[1024] = "";
    if (argc > 0) vm_entry_str(args[0], replLog, (int)sizeof(replLog));
    /* FOLLOWREPLICATION ritorna il numero di operazioni seguite (0 su errore), come il
     * riferimento: `== 0 ? 0 : -1` dava -1 con 4 operazioni applicate (misurato su
     * DbfReplication.bas). */
    {
        int frc = gmb_db_dbf_follow_replication(gmb_db_current_dbf(), replLog, NULL);
        bout->result = frc > 0 ? (double)frc : 0.0;
    }
}
static void gmb_bf_memmap_open_c1358(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    char path[1024] = "";
    if (argc > 0) vm_entry_str(args[0], path, (int)sizeof(path));
    bout->result = (double)gmb_mmap_open(path);
}
static void gmb_bf_memmap_peek_c1359(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    double hv = argc > 0 && args[0].type == 1 ? args[0].num : 0;
    double off = argc > 1 && args[1].type == 1 ? args[1].num : 0;
    bout->result = (double)gmb_mmap_peek((long)hv, (long)off);
}
static void gmb_bf_memmap_readbytes_c1360(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)argc; (void)func_code; (void)task;
    double hv = argc > 0 && args[0].type == 1 ? args[0].num : 0;
    double off = argc > 1 && args[1].type == 1 ? args[1].num : 0;
    double cnt = argc > 2 && args[2].type == 1 ? args[2].num : 0;
    int n = (int)cnt; if (n < 0) n = 0; if (n > 65536) n = 65536;
    unsigned char* buf = (unsigned char*)malloc((size_t)n ? (size_t)n : 1);
    int rc = buf ? gmb_mmap_read_bytes((long)hv, (long)off, buf, n) : 0;
    gmb_obj_shared_val_t* l = gmb_shared_list_new();
    if (l && rc) for (int i = 0; i < n; i++) gmb_list_add(l->list, gmb_make_val_double(buf[i]));
    free(buf);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)l; bout->result_ref_bt = GMB_BT_OBJECT;
}

void gmb_bf_isam_integrity_c1356(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char out[4096];
    int rc = gmb_db_isam_integrity(gmb_db_current_isam(), out, (int)sizeof(out));
    gmb_obj_string_t* s = gmb_str_cstr(rc == 0 ? out : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}

void gmb_bf_ledger_open_c1362(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char path[1024] = "";
    if (argc > 0) vm_entry_str(args[0], path, (int)sizeof(path));
    int64_t h = 0;
    gmb_db_apply_syscall(GMB_SYS_LEDGER_OPEN, path, 0, &h);
    bout->result = (double)h;
}
/* LEDGER.EOF(1268): cursore del journal (handle esplicito o ledger corrente), -1/0 come IL.
 * Prima il codice 1268 non era in tabella → "[VM ERROR] builtin 1268 non implementato". */
void gmb_bf_ledger_eof_c1268(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int64_t h = gmb_db_current_ledger();
    if (argc >= 1 && args[0].type == 1) h = (int64_t)args[0].num;
    bout->result = gmb_db_ledger_eof(h) ? -1.0 : 0.0;
}
static void gmb_bf_ledger_edit_c1363(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)bout;
    vm_error(task, "LEDGER.EDIT come funzione non è supportata — usa la forma statement (LEDGER EDIT ... INTO).");
}

/* Il ledger ritorna ARRAY: IL/PCode pushano liste, quindi anche la VM/C deve pushare la LISTA
 * parsando il JSON prodotto dal runtime (prima pushava il TESTO JSON → LEN() contava i
 * caratteri: balance 49, register 194). § MiniTests/ledger/ledger_ops.bas. */
static void gmb_ledger_push_json_list(gmb_builtin_out_t* bout, const char* js)
{
    /* API PUBBLICA del runtime (gmb_val_as_list/gmb_list_count/gmb_list_get): l'oggetto del
     * parser JSON NON è un gmb_obj_shared_val_t del VM (ha `data` = gmb_list_t*), quindi
     * `((gmb_obj_shared_val_t*)v.v_ptr)->list` leggeva un campo inesistente → puntatore
     * spazzatura → segfault intermittente (vedi gmb_syscall_db.c per la misura con lldb).
     * L'uscita invece si costruisce con gmb_shared_list_new (tipo del VM, con ->list), che è
     * ciò che LEN/JOIN si aspettano. Elementi COPIA TI (gmb_list_add non prende ownership). */
    gmb_val_t v = gmb_json_parse(js && *js ? js : "[]");
    gmb_list_t srcList = gmb_val_as_list(v);
    if (gmb_list_count(srcList) == 1)
    {
        gmb_val_t inner = gmb_list_get(srcList, 0);
        if (inner.kind == GMB_TYPE_OBJECT)
        {
            gmb_string_t tn = gmb_val_typename(inner);
            if (tn.data && strstr(tn.data, "LIST"))
            {
                gmb_list_free(&srcList);
                srcList = gmb_val_as_list(inner);
            }
        }
    }
    gmb_obj_shared_val_t* outL = gmb_shared_list_new();
    if (outL)
    {
        gmb_int_t i, n = gmb_list_count(srcList);
        for (i = 0; i < n; i++)
        {
            gmb_val_t it = gmb_list_get(srcList, i);
            if (it.kind == GMB_TYPE_STRING)
                it = gmb_make_val_string(it.v_string.data ? it.v_string.data : "");
            else if (it.kind == GMB_TYPE_OBJECT)
                it = gmb_make_val_string("");
            gmb_list_add(outL->list, it);
        }
        gmb_list_free(&srcList);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)outL; bout->result_ref_bt = GMB_BT_OBJECT;
        return;
    }
    gmb_list_free(&srcList);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("[]"); bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_ledger_register_c1364(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char q[512] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], q, (int)sizeof(q));
    char jbuf[8192] = "";   /* vedi syscall_db: buffer non inizializzato → JSON parse su spazzatura */
    int ok = gmb_db_ledger_register(gmb_db_current_ledger(), q, jbuf, (int)sizeof(jbuf));
    gmb_ledger_push_json_list(bout, ok == 0 ? jbuf : "[]");
}
void gmb_bf_ledger_balance_c1365(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char q[512] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], q, (int)sizeof(q));
    char jbuf[8192] = "";   /* vedi syscall_db: buffer non inizializzato → JSON parse su spazzatura */
    int ok = gmb_db_ledger_balance(gmb_db_current_ledger(), q, jbuf, (int)sizeof(jbuf));
    gmb_ledger_push_json_list(bout, ok == 0 ? jbuf : "[]");
}
void gmb_bf_ledger_accounts_c1366(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char root[512] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], root, (int)sizeof(root));
    char jbuf[8192] = "";   /* vedi syscall_db: buffer non inizializzato → JSON parse su spazzatura */
    int ok = gmb_db_ledger_accounts(gmb_db_current_ledger(), root, jbuf, (int)sizeof(jbuf));
    gmb_ledger_push_json_list(bout, ok == 0 ? jbuf : "[]");
}
void gmb_bf_ledger_plot_c1367(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char q[512] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], q, (int)sizeof(q));
    char labels[64][GMB_DB_LEDGER_NAME];
    double amounts[64];
    int nout = 0;
    /* PLOT ritorna il NUMERO di punti (IntegerType in StdLib; il PCode fa lo stesso): prima
     * pushereva un dict (Labels/Amounts) → il MiniTest non poteva confrontarlo. */
    nout = 0;
    (void)gmb_db_ledger_plot(gmb_db_current_ledger(), q, labels, amounts, 64, &nout);
    bout->result = (double)nout;
}
void gmb_bf_ledger_import_c1368(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char csvp[1024] = ""; char mapb[1024] = "";
    if (argc > 0) vm_entry_str(args[argc - 1], mapb, (int)sizeof(mapb));
    if (argc > 1) vm_entry_str(args[argc - 2], csvp, (int)sizeof(csvp));
    /* Il runtime C ritorna il NUMERO di righe importate (0 su errore), come il servizio C#:
     * non un booleano "rc == 0". Prima si ritornava 1.0 per qualunque import riuscito. */
    int rc = gmb_db_ledger_import(gmb_db_current_ledger(), csvp, mapb);
    bout->result = (double)rc;
}

/* ---- Fase 3/4 CUA: TOOL.SEARCH / UI.FIND / UI.CLICK / UI.FINDALL / UI.TYPE ----
 * Dispatch diretto al runtime C (gmb_agent_runtime): ToolRegistry + ActionResolver.
 * Il modello chiede un tool per nome/query, il runtime restituisce JSON con
 * nome/descrizione/rischio del tool (search) o l'esito dell'azione (find/click). */
void gmb_bf_cua_toolsearch_c1369(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* query = vm_builtin_sarg(args, argc, 0);
    if (!query) query = "";
    char* json = gmb_tool_registry_search_json(query);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(json ? json : "[]"); bout->result_ref_bt = GMB_BT_STRING;
    if (json) free(json);
}
void gmb_bf_cua_toolregister_c1370(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* name = vm_builtin_sarg(args, argc, 0);
    const char* desc = vm_builtin_sarg(args, argc, 1);
    const char* prov = vm_builtin_sarg(args, argc, 2);
    if (!name) name = ""; if (!desc) desc = ""; if (!prov) prov = "native";
    /* Parità IlRuntime.ToolRegister: nome vuoto → 0; successo → 1; errore → 0.
       (register_simple ritorna 0 su successo, -1 su errore: mappare.) */
    double out = 0.0;
    if (name[0]) out = gmb_tool_registry_register_simple(name, desc, prov) == 0 ? 1.0 : 0.0;
    bout->is_num = 1;
    bout->result = out;
}
void gmb_bf_cua_uifind_c1371(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* target = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_action_resolver_find(target);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(json ? json : "{\"found\":false}"); bout->result_ref_bt = GMB_BT_STRING;
    if (json) free(json);
}
void gmb_bf_cua_uiclick_c1372(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* target = vm_builtin_sarg(args, argc, 0);
    /* Tipo dichiarato Bit (parita' C#/C99 gmb_cua_ui_click_ok): 1/0, non JSON. */
    bout->is_num = 1; bout->result = gmb_cua_ui_click_ok(target ? target : "");
}
void gmb_bf_cua_uifindall_c1373(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* target = vm_builtin_sarg(args, argc, 0);
    /* Tipo dichiarato Array (parita' C# UiFindAll): JSON-array, non envelope find. */
    gmb_string_t rs = gmb_cua_ui_findall_arr(target ? target : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rs.data ? rs.data : "[]"); bout->result_ref_bt = GMB_BT_STRING;
    gmb_string_free(&rs);
}
void gmb_bf_cua_uitype_c1374(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* target = vm_builtin_sarg(args, argc, 0);
    const char* text  = vm_builtin_sarg(args, argc, 1);
    /* Tipo dichiarato Bit (parita' C# UiType): 1/0, non JSON. */
    bout->is_num = 1; bout->result = gmb_cua_ui_type_ok(target ? target : "", text ? text : "");
}

/* ---- BROWSER.NAVIGATE/SNAPSHOT/CLICK/FILL/EVALUATE (1375-1379) ----
 * CDP nativo C (gmb_browser_cdp.c nel runtime condiviso): naviga via Chromium/Obscura
 * sulla porta GMBASIC_CDP_PORT (default 9222). La connessione è lazy: si apre al primo
 * comando e resta attiva per i successivi (variabile statica di processo). */
static gmb_cdp_t* gmb_cdp_ctx(void) {
    return gmb_cdp_singleton();
}

void gmb_bf_cua_browser_act(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* sarg0 = vm_builtin_sarg(args, argc, 0);
    const char* sarg1 = vm_builtin_sarg(args, argc, 1);
    if (!sarg0) sarg0 = "";
    if (!sarg1) sarg1 = "";
    gmb_cdp_t* cdp = gmb_cdp_ctx();
    char* out = NULL;
    char src[4096];    /* sorgente JS / url grezzo */
    char esc[8192];    /* versione JSON-escaped (gmb_cdp_json_escape) */
    char body[8192];
    src[0] = 0; esc[0] = 0; body[0] = 0;
    const char* method = NULL;
    int rc = -1;

    if (func_code == 1375) {          /* BROWSER.NAVIGATE url */
        method = "Page.navigate";
        gmb_cdp_json_escape(sarg0, esc, sizeof(esc));
        snprintf(body, sizeof(body), "{\"url\":\"%s\"}", esc);
    } else if (func_code == 1377 && strncmp(sarg0, "@e", 2) == 0) {  /* CLICK ref @eN -> runtime */
        gmb_string_t rs = gmb_cua_browser_click(sarg0);
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rs.data ? rs.data : "");
        bout->result_ref_bt = GMB_BT_STRING;
        gmb_string_free(&rs);
        if (out) free(out);
        return;
    } else if (func_code == 1378 && strncmp(sarg0, "@e", 2) == 0) {  /* FILL ref @eN -> runtime */
        gmb_string_t rs = gmb_cua_browser_fill(sarg0, sarg1);
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rs.data ? rs.data : "");
        bout->result_ref_bt = GMB_BT_STRING;
        gmb_string_free(&rs);
        if (out) free(out);
        return;
    } else if (func_code == 1376) {   /* BROWSER.SNAPSHOT → DOM.getDocument */
        if (cdp) gmb_cdp_command(cdp, "DOM.enable", "", NULL);
        method = "DOM.getDocument";
        snprintf(body, sizeof(body), "{\"depth\":-1,\"pierce\":true}");
    } else if (func_code == 1377) {   /* BROWSER.CLICK selector — runtime condiviso (verify no-op) */
        gmb_string_t rs = gmb_cua_browser_click(sarg0);
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rs.data ? rs.data : "");
        bout->result_ref_bt = GMB_BT_STRING;
        gmb_string_free(&rs);
        return;
    } else if (func_code == 1378) {   /* BROWSER.FILL selector, value — runtime condiviso */
        gmb_string_t rs = gmb_cua_browser_fill(sarg0, sarg1);
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rs.data ? rs.data : "");
        bout->result_ref_bt = GMB_BT_STRING;
        gmb_string_free(&rs);
        return;
    } else if (func_code == 1379) {   /* BROWSER.EVALUATE expression */
        method = "Runtime.evaluate";
        gmb_cdp_json_escape(sarg0, esc, sizeof(esc));
        snprintf(body, sizeof(body), "{\"expression\":\"%s\",\"returnByValue\":true}", esc);
    }
    if (method && cdp) rc = gmb_cdp_command(cdp, method, body, &out);
    if (rc == 0) {
        /* Parità con il client C# (CdpBrowserClient): navigate/click/fill ritornano una
           costante, evaluate il valore interno, snapshot il JSON del documento. */
        const char* ok_const = NULL;
        if (func_code == 1375) ok_const = "navigated";
        else if (func_code == 1377) ok_const = "clicked";
        else if (func_code == 1378) ok_const = "filled";
        if (ok_const) {
            bout->is_num = 0;
            bout->result_ref = (gmb_obj_t*)gmb_str_cstr(ok_const);
            bout->result_ref_bt = GMB_BT_STRING;
            if (out) free(out);
            return;
        }
        if (func_code == 1379 && out) {
            char* val = gmb_cdp_evaluate_value(out);
            bout->is_num = 0;
            bout->result_ref = (gmb_obj_t*)gmb_str_cstr(val ? val : "null");
            bout->result_ref_bt = GMB_BT_STRING;
            if (val) free(val);
            if (out) free(out);
            return;
        }
        if (out) {
            bout->is_num = 0;
            bout->result_ref = (gmb_obj_t*)gmb_str_cstr(out);
            bout->result_ref_bt = GMB_BT_STRING;
            if (out) free(out);
            return;
        }
    }
    {
        char msg[768];
        snprintf(msg, sizeof(msg), "{\"ok\":false,\"error\":\"%s\"}",
            cdp ? gmb_cdp_last_error(cdp) : "CDP non connesso (gmb setup BROWSER)");
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(msg);
        bout->result_ref_bt = GMB_BT_STRING;
    }
    if (out) free(out);
}

void gmb_bf_cua_browser_start(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    /* Su VM/C il launch del browser è responsabilità dell'host (test_vmrun / REPL):
       BROWSER.START verifica/attiva la connessione CDP e riporta lo stato. Profilo per-sessione
       (residuo B/C): 3° arg 'profile' -> GMBASIC_CHROMIUM_PROFILE, letto da launch_chromium.sh
       quando l'host lancia il browser (profilo separato in libs-runtime/chromium/profiles/<nome>). */
    const char* provider = NULL;
    if (argc >= 1 && args[0].type == 2) provider = vm_builtin_sarg(args, argc, 0);
    else if (argc >= 1) provider = gmb_cua_browser_engine((int)args[0].num);
    int port = (argc >= 2) ? (int)args[1].num : 0;
    const char* profile = (argc >= 3) ? vm_builtin_sarg(args, argc, 2) : NULL;
    /* Delega al runtime C condiviso (§0bis): stessa implementazione di C99Native. */
    gmb_string_t s = gmb_cua_browser_start(provider, port, profile);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(s.data ? s.data : "");
    bout->result_ref_bt = GMB_BT_STRING;
    gmb_string_free(&s);
}

void gmb_bf_cua_browser_access_tree(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    /* Parità BROWSER.ACCESSTREE: delega al runtime condiviso (gmb_cua_browser_access_tree),
       unica implementazione (Accessibility.getFullAXTree -> snapshot @eN). */
    gmb_string_t s = gmb_cua_browser_access_tree();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(s.data ? s.data : "");
    bout->result_ref_bt = GMB_BT_STRING;
    gmb_string_free(&s);
}

void gmb_bf_cua_browser_screenshot(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* path = (argc > 0) ? vm_builtin_sarg(args, argc, 0) : "";
    gmb_string_t s = gmb_cua_browser_screenshot(path);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(s.data ? s.data : "");
    bout->result_ref_bt = GMB_BT_STRING;
    gmb_string_free(&s);
}
void gmb_bf_cua_browser_get_cookies(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)args;(void)argc;(void)func_code;(void)task; gmb_string_t s=gmb_cua_browser_get_cookies(); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(s.data?s.data:"[]"); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&s); }
void gmb_bf_cua_browser_set_cookie(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)func_code;(void)task; const char* n=vm_builtin_sarg(args,argc,0); const char* v=vm_builtin_sarg(args,argc,1); const char* u=vm_builtin_sarg(args,argc,2); gmb_string_t s=gmb_cua_browser_set_cookie(n?n:"",v?v:"",u?u:""); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(s.data?s.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&s); }
void gmb_bf_cua_browser_clear_cookies(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)args;(void)argc;(void)func_code;(void)task; gmb_string_t s=gmb_cua_browser_clear_cookies(); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(s.data?s.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&s); }
void gmb_bf_cua_browser_set_header(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)func_code;(void)task; const char* n=vm_builtin_sarg(args,argc,0); const char* v=vm_builtin_sarg(args,argc,1); gmb_string_t s=gmb_cua_browser_set_header(n?n:"",v?v:""); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(s.data?s.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&s); }
void gmb_bf_cua_browser_set_user_agent(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)func_code;(void)task; const char* u=vm_builtin_sarg(args,argc,0); gmb_string_t s=gmb_cua_browser_set_user_agent(u?u:""); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(s.data?s.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&s); }
void gmb_bf_cua_browser_human_click(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)func_code;(void)task; const char* s=vm_builtin_sarg(args,argc,0); gmb_string_t r=gmb_cua_browser_human_click(s?s:""); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(r.data?r.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&r); }
void gmb_bf_cua_browser_human_fill(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)func_code;(void)task; const char* s=vm_builtin_sarg(args,argc,0); const char* v=vm_builtin_sarg(args,argc,1); gmb_string_t r=gmb_cua_browser_human_fill(s?s:"",v?v:""); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(r.data?r.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&r); }
void gmb_bf_cua_browser_human_scroll(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) { (void)vm;(void)func_code;(void)task; int dy=(argc>0 && args[0].type==1)?(int)args[0].num:0; gmb_string_t r=gmb_cua_browser_human_scroll(dy); bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(r.data?r.data:""); bout->result_ref_bt=GMB_BT_STRING; gmb_string_free(&r); }
/* G1-G9: BROWSER.OBSERVE/FIND/NETLOG/CONSOLE/AUTH — richiedono client CDP C#,
 * non disponibile su backend C. Errore esplicito (stesso testo IL/PCode senza
 * CDP per parità MiniTest). Mai stub/crash.
 * AUTH(list) funziona senza CDP — storage file in work/browser-auth/. */
static void gmb_bf_cua_browser_unavailable(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)task;
  if (func_code == 1525 && argc >= 2) {
    const char* op = vm_builtin_sarg(args, argc, 0);
    if (op && strcmp(op, "list") == 0) {
      const char* dir = "work/browser-auth";
      char buf[512];
      /* costruisce la directory listing */
      char cmd[1024]; snprintf(cmd, sizeof cmd, "ls -1 \"%s\" 2>/dev/null || echo '(nessun auth profile)'", dir);
      FILE* p = popen(cmd, "r");
      if (!p) { snprintf(buf, sizeof buf, "(nessun auth profile)"); }
      else {
        size_t n = fread(buf, 1, sizeof buf - 1, p);
        buf[n < sizeof buf ? n : sizeof buf - 1] = 0;
        char* nl = strchr(buf, '\n'); if (nl) *nl = 0;
        pclose(p);
        if (!buf[0]) snprintf(buf, sizeof buf, "(nessun auth profile)");
      }
      bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(buf); bout->result_ref_bt=GMB_BT_STRING;
      return;
    }
  }
  (void)args;(void)argc;
  char b[256];
  snprintf(b, sizeof b, "error: CDP connect fallita su 127.0.0.1:9222 (provider=auto) — avvia Chromium/Obscura (launch_chromium.sh 9222)");
  bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr(b); bout->result_ref_bt=GMB_BT_STRING;
}

void gmb_bf_computer_act(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    static gmb_computer_provider_t* prov = NULL;
    if (!prov) {
        prov = gmb_computer_provider_resolve("auto");
        if (!prov) {
            vm_error(task, "COMPUTER: nessun provider disponibile");
            return;
        }
    }
    void* impl = prov->impl;
    /* 2024-2027 SCREENRECORD* → JSON dal bridge C condiviso (unica fonte dei testi, §0bis;
     * E7 chiuso 2026-09-13: prima i codici ricadevano nel dispatch ignoto → 0 silenzioso). */
    if (func_code >= 2024 && func_code <= 2027) {
        const char* rp = vm_builtin_sarg(args, argc, 0);
        double rfps = argc > 1 ? args[1].num : 10;
        gmb_string_t rs = func_code == 2024 ? gmb_cua_computer_screenrecord(rp ? rp : "", rfps)
                      : func_code == 2025 ? gmb_cua_computer_screenrecord_start(rp ? rp : "", rfps)
                      : func_code == 2026 ? gmb_cua_computer_screenrecord_stop()
                      : gmb_cua_computer_screenrecord_status();
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rs.data ? rs.data : "");
        bout->result_ref_bt = GMB_BT_STRING;
        gmb_string_free(&rs);
        return;
    }
    /* 1381 SCREENSHOT([path]) → stringa path salvato / errore */
    if (func_code == 1381) {
        const char* path = vm_builtin_sarg(args, argc, 0);
        char def[256] = "work/screenshot.png";
        if (!path || !*path) path = def;
        int rc = prov->vt->screenshot(impl, path);
        if (rc == 0) { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(path); bout->result_ref_bt = GMB_BT_STRING; }
        else { char m[512]; snprintf(m, sizeof(m), "error: screenshot %s fallito (Screen Recording permission?)", path); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(m); bout->result_ref_bt = GMB_BT_STRING; }
        return;
    }
    /* 1385 COMPUTER.WINDOWS() → JSON string */
    if (func_code == 1385) {
        char buf[16384] = "";
        int rc = prov->vt->windows(impl, buf, sizeof(buf));
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(rc == 0 ? buf : "[]");
        bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    /* numerici: CLICK x,y / MOVE x,y / TYPE text / KEY name */
    double x = argc > 0 ? args[0].num : 0;
    double y = argc > 1 ? args[1].num : 0;
    const char* s = vm_builtin_sarg(args, argc, 0);
    int rc = -1;
    switch (func_code) {
        case 1382: rc = prov->vt->click(impl, x, y); break;
        case 1386: rc = prov->vt->move(impl, x, y); break;
        case 1383: rc = prov->vt->type(impl, s ? s : ""); break;
        case 1384: rc = prov->vt->key(impl, s ? s : ""); break;
        case 3510: rc = prov->vt->double_click ? prov->vt->double_click(impl, x, y) : -1; break;
        case 3511: rc = prov->vt->hotkey ? prov->vt->hotkey(impl, s ? s : "") : -1; break;
        default: break;
    }
    bout->is_num = 1;
    bout->result = rc == 0 ? 0.0 : -1.0;
}

void gmb_bf_agent_code_c1439(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    /* AI.AGENTCODE: CodeGen loop nativo (Fase 2) — task -> brain -> .bas -> exec -> fix */
    const char* task_s = vm_builtin_sarg(args, argc, 0);
    if (!task_s || !*task_s) { bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr("AGENTCODE: task mancante"); bout->result_ref_bt=GMB_BT_STRING; return; }
    const char* outp = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
    {   /* C# CodeGenLoop: senza completer reale -> 'Nessun provider LLM configurato'. */
        const char* prov = gmb_provider_active("completion");
        if (!prov || !*prov || strcmp(gmb_provider_kind(prov), "fixture") == 0) {
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("Error: Nessun provider LLM configurato (AI SET ACTIVE)"); bout->result_ref_bt = GMB_BT_STRING;
            return;
        }
    }
    gmb_codegen_result_t r;
    int rc = gmb_codegen_run(task_s, (outp && *outp) ? outp : NULL, 3, &r);
    char* res = NULL;
    if (rc == 0 && r.ok) { asprintf(&res, "%s", r.output); }
    else { asprintf(&res, "Error: %s", r.diag[0] ? r.diag : "codegen fallita"); }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (res) free(res);
}
void gmb_bf_agent_fix_c1440(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    /* AI.AGENTFIX: fissa un errore -> loop codegen con prompt "Fix error: ..." */
    const char* err_s = vm_builtin_sarg(args, argc, 0);
    if (!err_s || !*err_s) { bout->is_num=0; bout->result_ref=(gmb_obj_t*)gmb_str_cstr("AGENTFIX: errore mancante"); bout->result_ref_bt=GMB_BT_STRING; return; }
    const char* outp = argc > 1 ? vm_builtin_sarg(args, argc, 1) : NULL;
    {   /* Parità C# CodeGenLoop (come 1439 AGENTCODE): senza completer reale. */
        const char* prov = gmb_provider_active("completion");
        if (!prov || !*prov || strcmp(gmb_provider_kind(prov), "fixture") == 0) {
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("Error: Nessun provider LLM configurato (AI SET ACTIVE)"); bout->result_ref_bt = GMB_BT_STRING;
            return;
        }
    }
    char prompt[4096]; snprintf(prompt, sizeof(prompt), "Fix error: %s", err_s);
    gmb_codegen_result_t r;
    int rc = gmb_codegen_run(prompt, (outp && *outp) ? outp : NULL, 3, &r);
    char* res = NULL;
    if (rc == 0 && r.ok) { asprintf(&res, "%s", r.output); }
    else { asprintf(&res, "Error: %s", r.diag[0] ? r.diag : "codegen fallita"); }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (res) free(res);
}
void gmb_bf_agent_run_c1387(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* goal = vm_builtin_sarg(args, argc, 0);
    if (!goal || !*goal) {
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr("AGENT.RUN: goal mancante");
        bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    int steps = argc > 1 && args[1].type == 1 ? (int)args[1].num : 8;
    if (steps <= 0) steps = 1;
    size_t cap = 65536;
    char* log = (char*)malloc(cap);
    if (!log) { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("AGENT.RUN: memoria"); bout->result_ref_bt = GMB_BT_STRING; return; }
    int rc = gmb_agent_run_goal("", goal, steps, log, cap);
    (void)rc;
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(log);
    bout->result_ref_bt = GMB_BT_STRING;
    free(log);
}

/* AI.AGENTRUN(name, goal[, rounds]) — bug 2026-09-08: mai portata al mondo C nativo (a
 * differenza di AGENT.RUN sopra, che non ha nome agente). Stesso loop (gmb_agent_run_goal),
 * ma sull'agente named — vedi gmb_agent_run_named in gmb_agent_loop.c. */
void gmb_bf_agent_run_named_c1441(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* name = vm_builtin_sarg(args, argc, 0);
    const char* goal = vm_builtin_sarg(args, argc, 1);
    int steps = argc > 2 && args[2].type == 1 ? (int)args[2].num : 5;
    if (func_code == 1388) {
        char io[16]; snprintf(io, sizeof io, "%d", steps);
        if (bsp_hook_str3("agent.runcua", name, goal, io, bout)) return;
    }
    if (func_code == 1388 && !gmb_agent_exists(name)) {
        char _m[160]; snprintf(_m, sizeof(_m), "ERROR: Agent '%s' is not registered.", name ? name : "");
        gmb_obj_string_t* r = gmb_str_cstr(_m);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)r; bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    char* res = gmb_agent_run_named(name, goal, steps);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* AI.BLACKBOARDWRITE(key, value) — 1504 (BuiltInFunction C#; fascia liberata 2026-09-09
 * dalla collisione coi BROWSER*, vedi vault Bugs/(open) - BuiltInFunction-Numerazione-
 * Collidente-Blackboard-Browser). Parita' Basic.Agents.Blackboard.Write: 1 = ok, 0 = err. */
static void gmb_bf_blackboard_write_c1504(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* key = vm_builtin_sarg(args, argc, 0);
    const char* val = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1;
    bout->result = (double)gmb_blackboard_write(key ? key : "", val ? val : "");
}

/* AI.BLACKBOARDREAD(key) — 1505: ritorna il valore corrente ("" se assente). */
static void gmb_bf_blackboard_read_c1505(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* key = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_blackboard_read(key ? key : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* AI.BLACKBOARDWAIT(key[, timeoutMs]) — 1506: attende un valore fino a timeout (default 10000).
 * F1 (parita' IL/PCodeVM, Strada C, 2026-09-11): se chiamata da un task GPM (task!=NULL),
 * il lavoro bloccante (pthread_cond_timedwait) va su un thread dedicato via vm_try_park_str
 * — il worker NON si blocca, il task riprende al completamento (o al timeout). Fuori da un
 * task GPM (task==NULL, es. REPL/flusso principale) resta sincrona. */
static void gmb_bf_blackboard_wait_c1506(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    const char* key = vm_builtin_sarg(args, argc, 0);
    int timeout = argc > 1 && args[1].type == 1 ? (int)args[1].num : 10000;
    if (task && vm)
    {
        gmb_str_job_t* j = (gmb_str_job_t*)calloc(1, sizeof(gmb_str_job_t));
        if (j)
        {
            j->kind = 0;
            j->key = key ? strdup(key) : NULL;
            j->timeout_ms = timeout;
            if (vm_try_park_str(vm, task, j)) return; /* parcheggiato: nessun push qui */
            free(j->key); free(j);
        }
    }
    char* res = gmb_blackboard_wait(key ? key : "", timeout);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* AI.AGENTSWARM(n, prefix, task[, maxConcurrency]) — 1507 (BuiltInFunction C#). Parita'
 * IlRuntime.AiAgentSwarm: crea prefix0..N-1 (se mancano), chiede a ciascuno, join '\n'.
 * F1 (parita' IL/PCodeVM, Strada C, 2026-09-11): stesso park di BLACKBOARDWAIT — lo swarm
 * (N pthread_join interni, potenzialmente secondi con LLM reali) gira su un thread dedicato
 * se chiamata da un task GPM, cosi' il worker resta libero per le altre goroutine. */
void gmb_bf_agent_swarm_c1507(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    double n = argc > 0 && args[0].type == 1 ? args[0].num : 0;
    const char* prefix = vm_builtin_sarg(args, argc, 1);
    const char* tsk = vm_builtin_sarg(args, argc, 2);
    int cc = argc > 3 && args[3].type == 1 ? (int)args[3].num : 8;
    if (task && vm)
    {
        gmb_str_job_t* j = (gmb_str_job_t*)calloc(1, sizeof(gmb_str_job_t));
        if (j)
        {
            j->kind = 1;
            j->n = n;
            j->prefix = prefix ? strdup(prefix) : NULL;
            j->task_str = tsk ? strdup(tsk) : NULL;
            j->cc = cc;
            if (vm_try_park_str(vm, task, j)) return; /* parcheggiato: nessun push qui */
            free(j->prefix); free(j->task_str); free(j);
        }
    }
    char* res = gmb_agent_swarm(n, prefix, tsk, cc);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Orchestrazione agenti (A1): categorie di task → provider/modello (runtime gmb_category.c;
 * parita' Basic.AI.Providers.CategoryRegistry C#). Codici 2112-2116 (BuiltInFunction C#). */
static void gmb_bf_category_set_c2112(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* cat = vm_builtin_sarg(args, argc, 0);
    const char* prov = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1;
    bout->result = (double)gmb_category_set(cat ? cat : "", prov ? prov : "");
}
static void gmb_bf_category_fallback_c2113(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* cat = vm_builtin_sarg(args, argc, 0);
    const char* prov = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1;
    bout->result = (double)gmb_category_fallback(cat ? cat : "", prov ? prov : "");
}
static void gmb_bf_category_auto_c2114(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int flag = argc > 0 && args[0].type == 1 ? (int)args[0].num : 0;
    bout->is_num = 1;
    bout->result = (double)gmb_category_auto(flag);
}
static void gmb_bf_category_pick_c2115(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* cat = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_category_pick(cat ? cat : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_category_list_c2116(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_category_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Orchestrazione agenti (B1): Task Graph con dipendenze (gmb_taskgraph.c; codici 2117-2122). */
static void gmb_bf_task_define_c2117(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* id = vm_builtin_sarg(args, argc, 0);
    const char* desc = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_task_define(id ? id : "", desc ? desc : "");
}
static void gmb_bf_task_depends_c2118(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* id = vm_builtin_sarg(args, argc, 0);
    const char* dep = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_task_depends(id ? id : "", dep ? dep : "");
}
static void gmb_bf_task_assign_c2119(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* id = vm_builtin_sarg(args, argc, 0);
    const char* agent = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_task_assign(id ? id : "", agent ? agent : "");
}
static void gmb_bf_task_status_c2120(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* id = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_task_status(id ? id : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_task_list_c2121(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_task_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_task_run_c2122(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_task_run();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Orchestrazione agenti (A2): Planner — piani con approvazione (gmb_plan.c; codici 2123-2128). */
static void gmb_bf_plan_define_c2123(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* g = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_plan_define(n ? n : "", g ? g : "");
}
static void gmb_bf_plan_add_c2124(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* t = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_plan_add(n ? n : "", t ? t : "");
}
static void gmb_bf_plan_approve_c2125(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_plan_approve(n ? n : "");
}
static void gmb_bf_plan_status_c2126(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_plan_status(n ? n : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_plan_list_c2127(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_plan_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_plan_run_c2128(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_plan_run(n ? n : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Orchestrazione agenti (A3): lifecycle hooks (gmb_hook.c; codici 2129-2133). */
static void gmb_bf_hook_register_c2129(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* ev = vm_builtin_sarg(args, argc, 0);
    const char* h = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_hook_register(ev ? ev : "", h ? h : "");
}
static void gmb_bf_hook_unregister_c2130(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* ev = vm_builtin_sarg(args, argc, 0);
    const char* h = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_hook_unregister(ev ? ev : "", h ? h : "");
}
static void gmb_bf_hook_list_c2131(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_hook_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_hook_events_c2132(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_hook_events();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_hook_fire_c2133(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* ev = vm_builtin_sarg(args, argc, 0);
    const char* pl = vm_builtin_sarg(args, argc, 1);
    char* res = gmb_hook_fire(ev ? ev : "", pl ? pl : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Nomi dei locali della procedura corrente (chunk->ploc_*), per il REQUIRE di AI VERIFY:
 * parità C# PCodeProcessor.ResolveProcLocalNames (proc con start addr <= ip). */
static const char** gmb_vm_aiverify_local_names(gmb_vm_t* vm, int ip, int* out_n)
{
    *out_n = 0;
    const gmb_chunk_t* ch = vm->chunk;
    if (!ch || ch->ploc_count == 0) return NULL;
    int best = -1;
    for (uint32_t i = 0; i < ch->ploc_count; i++)
        if ((int)ch->ploc_addrs[i] <= ip && (best < 0 || ch->ploc_addrs[i] > ch->ploc_addrs[best]))
            best = (int)i;
    if (best < 0) return NULL;
    *out_n = (int)ch->ploc_counts[best];
    return (const char**)ch->ploc_names[best];
}

/* AI VERIFY <attempt> REQUIRE <require> — 1992 (BuiltInFunction C#). Verifier/retry (B2):
 * runtime condiviso gmb_verify_run (COMPILE via compilatore C, REQUIRE via gmb_expr_eval_ex).
 * Il REQUIRE vede i LOCALI DEL CHIAMANTE (parità C# MacroEvalExprWithLocals): prima l'env
 * esponeva solo RESULT$ → REQUIRE con una locale (es. `LEN(RESULT$) > n - 8`) falso sul C. */
static void gmb_bf_aiverify_c1992(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    const char* act = vm_builtin_sarg(args, argc, 0);
    const char* req = vm_builtin_sarg(args, argc, 1);
    int nn = 0;
    const char** names = gmb_vm_aiverify_local_names(vm, task->ip, &nn);
    gmb_dict_t lv = gmb_dict_new();
    for (int i = 0; i < nn; i++)
    {
        if (!names[i] || !*names[i]) continue;
        gmb_stack_entry_t e = gmb_task_load_local(task, i);
        gmb_dict_set(&lv, names[i], gmb_adapter_stack_to_val_borrowed(e));
    }
    char* res = gmb_verify_run_ex(act ? act : "", req ? req : "", 3, &lv);
    gmb_dict_free(&lv);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Orchestrazione agenti (A6): Artifact Store (gmb_artifact.c; codici 2134-2138). */
static void gmb_bf_artifact_put_c2134(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_artifact_put(n ? n : "", p ? p : "");
}
static void gmb_bf_artifact_link_c2135(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* u = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_artifact_link(n ? n : "", u ? u : "");
}
static void gmb_bf_artifact_get_c2136(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_artifact_get(n ? n : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_artifact_list_c2137(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_artifact_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_artifact_remove_c2138(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_artifact_remove(n ? n : "");
}

/* Orchestrazione agenti (A7): Resource Manager (gmb_resource.c; codici 2139-2143). */
static void gmb_bf_resource_set_c2139(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* k = vm_builtin_sarg(args, argc, 0);
    const char* v = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_resource_set(k ? k : "", v ? v : "");
}
static void gmb_bf_resource_get_c2140(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* k = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_resource_get(k ? k : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_resource_list_c2141(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_resource_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_resource_available_c2142(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* k = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_resource_available(k ? k : "");
}
static void gmb_bf_resource_swarmsize_c2143(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    double n = argc > 0 && args[0].type == 1 ? args[0].num : 0;
    bout->is_num = 1; bout->result = gmb_resource_swarm_size(n);
}

/* Orchestrazione agenti (A9): Handoff (gmb_handoff.c; codici 2144-2148). */
static void gmb_bf_handoff_c2144(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* t = vm_builtin_sarg(args, argc, 0);
    const char* a = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_handoff_set(t ? t : "", a ? a : "");
}
static void gmb_bf_handoff_complete_c2145(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* t = vm_builtin_sarg(args, argc, 0);
    const char* r = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_handoff_complete(t ? t : "", r ? r : "");
}
static void gmb_bf_handoff_status_c2146(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* t = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_handoff_status(t ? t : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_handoff_list_c2147(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_handoff_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_handoff_clear_c2148(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_handoff_clear();
}

/* Orchestrazione agenti (A5): Rules injection gerarchica (gmb_rules.c; codici 2149-2153). */
static void gmb_bf_rules_add_c2149(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    const char* c = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_rules_add(s ? s : "", c ? c : "");
}
static void gmb_bf_rules_for_c2150(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_rules_for(p ? p : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_rules_list_c2151(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_rules_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_rules_remove_c2152(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_rules_remove(s ? s : "");
}
static void gmb_bf_rules_clear_c2153(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_rules_clear();
}

/* Orchestrazione agenti (A4): Goal + idle continuation (gmb_goal.c; codici 2154-2158). */
static void gmb_bf_goal_set_c2154(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* o = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_goal_set(o ? o : "");
}
static void gmb_bf_goal_get_c2155(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_goal_get();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_goal_status_c2156(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_goal_status();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_goal_tick_c2157(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = gmb_goal_tick();
}
static void gmb_bf_goal_clear_c2158(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_goal_clear();
}






/* Orchestrazione agenti (A8): Session Replay (gmb_replay.c; codici 2159-2164). */
static void gmb_bf_replay_start_c2159(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_replay_start(s ? s : "");
}
static void gmb_bf_replay_record_c2160(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* e = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_replay_record(e ? e : "");
}
static void gmb_bf_replay_stop_c2161(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_replay_stop();
}
static void gmb_bf_replay_get_c2162(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_replay_get(s ? s : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_replay_list_c2163(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_replay_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_replay_clear_c2164(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_replay_clear();
}
static void gmb_bf_replay_run_c2181(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    const char* t = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_replay_run(s ? s : "", t ? t : "");
}
static void gmb_bf_goal_agent_c2182(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_goal_agent(n ? n : "");
}
/* AI EVAL VALIDATE (2001): UAT via vault (oracolo a termini) → gmb_eval_validate. */
void gmb_bf_eval_validate_c2001(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* r = vm_builtin_sarg(args, argc, 0);
    const char* f = vm_builtin_sarg(args, argc, 1);
    char* res = gmb_eval_validate(r ? r : "", f ? f : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
/* AI DECIDE (3817): decision layer JEV/SystemOne v0 (Rules, JSON) → gmb_ai_decide_run_json. */
void gmb_bf_decide_c3817(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    if (argc >= 6) {
        const char* state = vm_builtin_sarg(args, argc, 0);
        const char* kind = vm_builtin_sarg(args, argc, 1);
        const char* providers = vm_builtin_sarg(args, argc, 2);
        char* vres = gmb_ai_decide_verify_json(state ? state : "", kind ? kind : "",
                                               providers ? providers : "");
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(vres ? vres : "");
        bout->result_ref_bt = GMB_BT_STRING;
        free(vres);
        return;
    }
    if (argc >= 5) {
        const char* state = vm_builtin_sarg(args, argc, 0);
        const char* names = vm_builtin_sarg(args, argc, 1);
        const char* kinds = vm_builtin_sarg(args, argc, 2);
        const char* provider = vm_builtin_sarg(args, argc, 3);
        const char* fallback = vm_builtin_sarg(args, argc, 4);
        char* gres = gmb_ai_decide_group_json(state ? state : "", names ? names : "[]",
                                              kinds ? kinds : "[]",
                                              provider ? provider : "", fallback ? fallback : "");
        bout->is_num = 0;
        bout->result_ref = (gmb_obj_t*)gmb_str_cstr(gres ? gres : "");
        bout->result_ref_bt = GMB_BT_STRING;
        free(gres);
        return;
    }
    const char* state = vm_builtin_sarg(args, argc, 0);
    const char* kind = vm_builtin_sarg(args, argc, 1);
    const char* provider = vm_builtin_sarg(args, argc, 2);
    const char* fallback = vm_builtin_sarg(args, argc, 3);
    char* res = (fallback && *fallback)
        ? gmb_ai_decide_run_json_chain(state ? state : "", kind ? kind : "",
                                       provider ? provider : "", fallback)
        : gmb_ai_decide_run_json(state ? state : "", kind ? kind : "",
                                 provider ? provider : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
/* AI ASKCHAIN (2002): anello di ragionamento concatenato via Blackboard → gmb_agent_ask_chain. */
/* CONNECTOR (codici 2003-2023) — runtime condiviso gmb_connector_* (parità IL/C99). */
static void gmb_bf_conn_str(gmb_builtin_out_t* bout, char* res)
{
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "[]");
    bout->result_ref_bt = GMB_BT_STRING;
}
static void gmb_bf_conn_num(gmb_builtin_out_t* bout, int r)
{
    bout->is_num = 1;
    bout->result = (double)r;
}
static void gmb_bf_c2003(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_load(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2004(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_register(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1), argc>=3?vm_builtin_sarg(args,argc,2):NULL)); }
static void gmb_bf_c2005(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_unregister(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2006(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)args;(void)argc;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_list()); }
static void gmb_bf_c2007(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_enable(vm_builtin_sarg(args,argc,0), 1)); }
static void gmb_bf_c2008(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_enable(vm_builtin_sarg(args,argc,0), 0)); }
static void gmb_bf_c2009(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_call(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1), vm_builtin_sarg(args,argc,2))); }
static void gmb_bf_c2010(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_status(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2011(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_resources(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2012(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_prompts(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2013(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)args;(void)argc;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_marketplace_list()); }
static void gmb_bf_c2014(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_marketplace_get(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2015(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_policy_set(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1), vm_builtin_sarg(args,argc,2))); }
static void gmb_bf_c2016(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_policy_get(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1))); }
static void gmb_bf_c2017(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_secret_set(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1), vm_builtin_sarg(args,argc,2))); }
static void gmb_bf_c2018(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_secret_get(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1))); }
static void gmb_bf_c2019(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_auth(vm_builtin_sarg(args,argc,0), vm_builtin_sarg(args,argc,1))); }
static void gmb_bf_c2020(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)args;(void)argc;(void)fc;(void)task; gmb_bf_conn_num(bout, 0); }
static void gmb_bf_c2021(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_search(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2022(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_str(bout, gmb_connector_tools(vm_builtin_sarg(args,argc,0))); }
static void gmb_bf_c2023(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int fc, gmb_builtin_out_t* bout, gmb_task_t* task)
{ (void)vm;(void)fc;(void)task; gmb_bf_conn_num(bout, gmb_connector_refresh(vm_builtin_sarg(args,argc,0))); }
void gmb_bf_askchain_c2002(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    const char* x = vm_builtin_sarg(args, argc, 2);
    if (bsp_hook_str("agent.askchain", n, p, bout)) return;
    if (!gmb_agent_exists(n)) {
        char _m[160]; snprintf(_m, sizeof(_m), "ERROR: Agent '%s' is not registered.", n ? n : "");
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(_m); bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    char* res = gmb_agent_ask_chain(n ? n : "", p ? p : "", x ? x : "chain");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}

/* Orchestrazione agenti (B6): metriche fleet-level (gmb_metrics.c; codici 2165-2169). */
static void gmb_bf_metric_record_c2165(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* a = vm_builtin_sarg(args, argc, 0);
    const char* k = vm_builtin_sarg(args, argc, 1);
    double v = argc > 2 && args[2].type == 1 ? args[2].num : 0;
    bout->is_num = 1; bout->result = (double)gmb_metric_record(a ? a : "", k ? k : "", v);
}
static void gmb_bf_metric_get_c2166(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* a = vm_builtin_sarg(args, argc, 0);
    const char* k = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = gmb_metric_get(a ? a : "", k ? k : "");
}
static void gmb_bf_metric_list_c2167(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_metric_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_fleet_stats_c2168(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_fleet_stats();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_metric_reset_c2169(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_metric_reset();
}

/* Orchestrazione agenti (A10): Hashline (gmb_hashline.c; codici 2170-2172). */
static void gmb_bf_hashline_c2170(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* c = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_hashline_tag(c ? c : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_hash_edit_c2171(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* c = vm_builtin_sarg(args, argc, 0);
    const char* r = vm_builtin_sarg(args, argc, 1);
    const char* t = vm_builtin_sarg(args, argc, 2);
    char* res = gmb_hashline_edit(c ? c : "", r ? r : "", t ? t : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_hash_verify_c2172(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* c = vm_builtin_sarg(args, argc, 0);
    const char* r = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_hashline_verify(c ? c : "", r ? r : "");
}
/* Orchestrazione agenti (B7): checkpoint/recovery (gmb_checkpoint.c; codici 2173-2176). */
static void gmb_bf_checkpoint_c2173(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    const char* st = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1; bout->result = (double)gmb_checkpoint_save(s ? s : "", st ? st : "");
}
static void gmb_bf_resume_c2174(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    char* res = gmb_checkpoint_resume(s ? s : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_checkpoint_list_c2175(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* res = gmb_checkpoint_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(res ? res : "");
    bout->result_ref_bt = GMB_BT_STRING;
    free(res);
}
static void gmb_bf_checkpoint_clear_c2176(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_checkpoint_clear();
}

/* Persistenza orchestrazione su disco (gmb_orchestration.c; codici 2177-2178). */
static void gmb_bf_orch_save_c2177(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_orch_save(p ? p : "");
}
static void gmb_bf_orch_load_c2178(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    bout->is_num = 1; bout->result = (double)gmb_orch_load(p ? p : "");
}
static void gmb_bf_task_clear_c2179(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    gmb_task_clear(); bout->is_num = 1; bout->result = 0;
}
static void gmb_bf_plan_clear_c2180(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    gmb_plan_clear(); bout->is_num = 1; bout->result = 0;
}

static void gmb_bf_vault_info_c1274(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char* r = gmb_vault_info_json(n ? n : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_vault_folders_c1275(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char* r = gmb_vault_folders_json(n ? n : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "[]"); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_vault_count_c1276(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->result = (double)gmb_vault_count(n ? n : "");
}
static void gmb_bf_vault_list_c1277(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* r = gmb_vault_list_json();
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_vault_create_c1272(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* AI VAULT CREATE name, path [, pattern] — parità C# VaultManager.VaultCreate. */
    (void)vm; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
    const char* root = vm_builtin_sarg(args, argc, 1);
    const char* pat = vm_builtin_sarg(args, argc, 2);
    if (!name || !root) { vm_error(task, "VAULT.CREATE richiede nome e percorso"); return; }
    bout->is_num = 1; bout->result = (double)gmb_vault_create(name, root, pat);
}
static void gmb_bf_vault_index_c1273(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    /* AI VAULT INDEX [name] — senza nome indicizza tutti i vault registrati. */
    (void)vm; (void)func_code;
    const char* name = vm_builtin_sarg(args, argc, 0);
    if (name && *name)
    {
        bout->is_num = 1; bout->result = (double)gmb_vault_index(name);
        return;
    }
    /* tutti i vault: lista nomi → somma file indicizzati */
    char* list = gmb_vault_list_json();
    double total = 0;
    if (list)
    {
        const char* p = list;
        while ((p = strchr(p, '"')) != NULL)
        {
            p++;
            char nm[256]; size_t i = 0;
            while (*p && *p != '"' && i < sizeof(nm) - 1) nm[i++] = *p++;
            nm[i] = '\0';
            long long n = gmb_vault_index(nm);
            if (n > 0) total += (double)n;
        }
        free(list);
    }
    bout->is_num = 1; bout->result = total;
}
static void gmb_bf_note_get_c1306(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_note_get(n ? n : "", p ? p : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_note_title_c1307(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_note_title(n ? n : "", p ? p : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_note_tags_c1308(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_note_tags(n ? n : "", p ? p : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_note_wordcount_c1309(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    bout->result = (double)gmb_note_wordcount(n ? n : "", p ? p : "");
}
static void gmb_bf_note_exists_c1310(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    bout->result = gmb_note_exists(n ? n : "", p ? p : "") ? 1.0 : 0.0;
}

static void gmb_bf_note_folder_c1311(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_note_folder_path(n ? n : "", p ? p : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
/* AI NOTE CREATE/UPDATE/APPEND/DELETE/MOVE — comandi (vault attivo ""). Codici interni VM/C
 * 1450-1454 (nessuna collisione con BuiltInFunction C#); il compilatore mappa il domain-stmt. */
static void gmb_bf_note_create_c1301(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    const char* c = vm_builtin_sarg(args, argc, 1);
    const char* t = vm_builtin_sarg(args, argc, 2);
    bout->result = (double)gmb_note_create("", p ? p : "", c, t);
}
static void gmb_bf_note_update_c1302(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    const char* c = vm_builtin_sarg(args, argc, 1);
    bout->result = (double)gmb_note_update("", p ? p : "", c);
}
static void gmb_bf_note_append_c1303(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    const char* c = vm_builtin_sarg(args, argc, 1);
    bout->result = (double)gmb_note_append("", p ? p : "", c);
}
static void gmb_bf_note_delete_c1304(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    bout->result = (double)gmb_note_delete("", p ? p : "");
}
static void gmb_bf_note_move_c1305(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* o = vm_builtin_sarg(args, argc, 0);
    const char* n = vm_builtin_sarg(args, argc, 1);
    bout->result = (double)gmb_note_move("", o ? o : "", n ? n : "");
}

static void gmb_bf_links_out_c1312(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_note_links_out_json(n ? n : "", p ? p : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "[]"); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_backlinks_c1313(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_note_backlinks_json(n ? n : "", p ? p : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "[]"); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_link_dangling_c1314(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->result = (double)gmb_links_dangling_count(n ? n : "");
}
static void gmb_bf_link_json_c1315(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    /* LINKJSON: grafo completo (nodes/edges/dangling) - era gmb_links_all_json (array
     * piatto), shape sbagliata rispetto a IL (LinkManager.ToJson). */
    char* r = gmb_links_json_graph(n ? n : "");
    if (!r || strcmp(r, "[]") == 0) { if (r) free(r); r = strdup("{\"nodes\":[],\"edges\":[],\"dangling\":[]}"); }
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "{\"nodes\":[],\"edges\":[],\"dangling\":[]}"); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}

void gmb_bf_brain_use_c1278(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->result = (double)gmb_brain_use(n ? n : "");
}
void gmb_bf_brain_detach_c1279(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    bout->result = gmb_brain_detach(n ? n : "") ? 1.0 : 0.0;
}
static void gmb_bf_vault_watch_c1339(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    const char* p = vm_builtin_sarg(args, argc, 1);
    bout->result = gmb_vault_watch(n ? n : "", p ? p : "") == 0 ? 0.0 : 1.0;   /* C# AiVaultWatch: 1.0 */
}
static void gmb_bf_vault_policy_c1340(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* n = vm_builtin_sarg(args, argc, 0);
    char* r = gmb_vault_policy(n ? n : "");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "MANUAL"); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}

static void gmb_bf_keygen_c1270(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* algo = vm_builtin_sarg(args, argc, 0);
    int bits = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 2048;
    int pv = 0, pub = 0;
    if (gmb_keys_generate(algo ? algo : "RSA", bits, &pv, &pub) == 0)
    {
        /* KEYGEN ritorna long[] [handle_privato, handle_pubblico] (parità IL). */
        gmb_obj_shared_val_t* rl = gmb_shared_list_new();
        if (rl) { gmb_list_add(rl->list, gmb_make_val_double(pv)); gmb_list_add(rl->list, gmb_make_val_double(pub)); }
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)rl; bout->result_ref_bt = GMB_BT_OBJECT;
    }
    else
    {
        bout->is_num = 0; bout->result_ref = NULL;
    }
}
static void gmb_bf_verifyraw_c3509(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    uint8_t data[8192], sig[8192];
    size_t dlen = 0, slen = 0;
    /* ordine BASIC/C#: VERIFYRAW(signature, data, publicKeyB64) */
    gmb_stack_entry_t sigE = argc > 0 ? args[0] : (gmb_stack_entry_t){0};
    gmb_stack_entry_t datE = argc > 1 ? args[1] : (gmb_stack_entry_t){0};
    if (sigE.type == 2 && sigE.ref && sigE.ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)sigE.ref;
        slen = sv->list->count < sizeof(sig) ? sv->list->count : sizeof(sig);
        for (size_t i = 0; i < slen; i++) { gmb_val_t v = sv->list->items[i]; sig[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
    }
    else if (sigE.type == 2 && sigE.ref)
    {
        gmb_obj_string_t* s = (gmb_obj_string_t*)sigE.ref;
        slen = s->length < sizeof(sig) ? s->length : sizeof(sig);
        memcpy(sig, s->data, slen);
    }
    if (datE.type == 2 && datE.ref && datE.ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)datE.ref;
        dlen = sv->list->count < sizeof(data) ? sv->list->count : sizeof(data);
        for (size_t i = 0; i < dlen; i++) { gmb_val_t v = sv->list->items[i]; data[i] = (uint8_t)(v.kind == GMB_TYPE_DOUBLE ? v.v_double : (double)v.v_int); }
    }
    else if (datE.type == 2 && datE.ref)
    {
        gmb_obj_string_t* s = (gmb_obj_string_t*)datE.ref;
        dlen = s->length < sizeof(data) ? s->length : sizeof(data);
        memcpy(data, s->data, dlen);
    }
    const char* key = vm_builtin_sarg(args, argc, 2);
    bout->result = gmb_keys_verify_raw(data, dlen, sig, slen, key ? key : "") ? -1.0 : 0.0;
}

static void gmb_bf_sleep_c1271(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    double ms = (argc > 0 && args[0].type == 1) ? args[0].num : 0.0;
    if (ms > 0) usleep((useconds_t)(ms * 1000.0));
    gmb_obj_string_t* s = gmb_str_cstr("");
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_dbf_appendmany_c1349(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    (void)argc;
    int rc = 0;
    if (argc > 1 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
        rc = gmb_db_dbf_append_many(gmb_db_current_dbf(), *sv->list);
    }
    bout->result = (double)rc;
}
void gmb_bf_speed_setmany_c1357(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* alias = vm_builtin_sarg(args, argc, 0);
    int rc = 0;
    if (argc > 1 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_LIST)
    {
        gmb_obj_shared_val_t* sv = (gmb_obj_shared_val_t*)args[1].ref;
        rc = gmb_db_speed_set_many(gmb_db_current_speed(), *sv->list, alias ? alias : NULL);
    }
    bout->result = (double)rc;
}

static void gmb_bf_memory_rank_c1318(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* q = vm_builtin_sarg(args, argc, 0);
    int topK = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 5;
    char* r = gmb_memory_rank(q ? q : "", topK);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_memory_consolidate_c1319(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    gmb_memory_consolidate();
    bout->result = 1.0;
}

/* AI MULTI/bench (R7): il VM/C non ha i RagDb vettoriali per-vault né l'AgentServer:
   AISEARCHMULTI/RAG/ASK usano il brain condiviso gmb_brain (ricerca + llm); BENCHRESULT
   risponde come AgentServer.GetBenchResult quando nessun benchmark è registrato (sempre
   il caso: errore esplicito, mai stub). */
void gmb_bf_benchresult_c1258(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char* out = gmb_agent_benchresult_json(vm_builtin_sarg(args, argc, 0));
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* AI.PREFILL/DECODE/CACHECLEAR/CACHETRIM/BENCHMARK — sessione KV dell'agente.
 * Il runtime condiviso (gmb_agent.c) ritorna l'errore esplicito del contratto IL
 * (i backend C non hanno una sessione KV: AgentServer è C#). Mai risultato silenzioso. */
void gmb_bf_prefill_c833(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char* out = gmb_agent_prefill_json(vm_builtin_sarg(args, argc, 0), "", vm_builtin_sarg(args, argc, 1));
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_decode_c834(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int mt = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 0;
    char* out = gmb_agent_decode_json(vm_builtin_sarg(args, argc, 0), mt);
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_cacheclear_c835(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    if (bsp_hook_str("agent.cacheclear", vm_builtin_sarg(args, argc, 0), NULL, bout)) return;
    char* out = gmb_agent_cache_clear_json(vm_builtin_sarg(args, argc, 0));
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_cachetrim_c836(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int drop = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 0;
    { char io[16]; snprintf(io, sizeof io, "%d", drop);
      if (bsp_hook_str3("agent.cachetrim", vm_builtin_sarg(args, argc, 0), NULL, io, bout)) return; }
    char* out = gmb_agent_cache_trim_json(vm_builtin_sarg(args, argc, 0), drop);
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_benchmark_c1257(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int iters = (argc > 2 && args[2].type == 1) ? (int)args[2].num : 1;
    { char io[16]; snprintf(io, sizeof io, "%d", iters);
      if (bsp_hook_str3("agent.benchmark", vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), io, bout)) return; }
    char* out = gmb_agent_benchmark_json(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1), iters, "");
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* AI.BRAININSTALL/AI.BRAINUNINSTALL — il cervello è un DATO: manifest + download on demand
 * (gmbfetch) + estrazione, tutto nel runtime C condiviso (gmb_brains.c). Prima qui c'era un
 * errore esplicito ("richiede BrainInstaller") perché la logica viveva solo nel C#: ora IL,
 * PCodeVM, VM/C e C99Native installano lo STESSO cervello con lo STESSO codice. */
static void gmb_bf_brain_install_c1280(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* name = vm_builtin_sarg(args, argc, 0);
    const char* source = vm_builtin_sarg(args, argc, 1);
    bout->is_num = 1;
    bout->result = (double)gmb_brain_install(name, source);
}
static void gmb_bf_brain_uninstall_c1281(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 1;
    bout->result = (double)gmb_brain_uninstall(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_searchmulti_c1285(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* q = vm_builtin_sarg(args, argc, 0);
    int topK = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 5;
    if (topK <= 0) topK = 5;
    int rerank = (argc > 4 && args[4].type == 1) ? (int)args[4].num : 1;   /* %RERANK_KEYWORD default */
    double mev = (argc > 5 && args[5].type == 1) ? args[5].num : (double)NAN; /* soglia off */
    int explain = (argc > 6 && args[6].type == 1) ? (int)args[6].num : 0;
    if (explain != 0) {
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr("ERROR: SearchMulti: explain non supportato sul merge multi-brain (usare AIVECTOR.SEARCH single-collection)."); bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    char* out = NULL;
    {
        const char* coll = vm_builtin_sarg(args, argc, 2);
        const char* brains = vm_builtin_sarg(args, argc, 3);
        out = gmb_ai_sbm_json_multi(q ? q : "", topK,
                                    coll && *coll ? coll : "wiki",
                                    brains && *brains ? brains : NULL,
                                    rerank, mev, explain);
    }
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_ragcontextmulti_c1286(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* q = vm_builtin_sarg(args, argc, 0);
    int topK = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 5;
    if (topK <= 0) topK = 5;
    int rerank = (argc > 2 && args[2].type == 1) ? (int)args[2].num : 1;
    double mev = (argc > 3 && args[3].type == 1) ? args[3].num : (double)NAN;
    char* out = gmb_ai_sbm_ctx(q ? q : "", topK, 384, rerank, mev);
    /* C# RagContextMulti: "" senza hit. gmb_ai_sbm_ctx ritorna il JSON d'errore OPPURE il
     * messaggio testuale canonico insufficiente (gmb_brain.c) — vanno trattati ENTRAMBI,
     * altrimenti il messaggio trapelava (divergenza vs IL/PCode). */
    if (out && (strncmp(out, "{\"status\":", 10) == 0 || strncmp(out, "[insufficient evidence]", 23) == 0)) { free(out); out = strdup(""); }
    gmb_obj_string_t* s = gmb_str_cstr(out ? out : "");
    free(out);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
/* ── L1 Queue/ACF (VM/C, parity C# AI.AGENTSEND..AI.CHANNEL*, builtin 1423-1433) ──
 * Implementazione reale nel runtime condiviso (gmb_agent_queue.c / gmb_message_bus.c).
 * 1423 SEND(to,task,input[,payment]); 1424 RECEIVE(agent)->json; 1425-1429 pause/resume/
 * stop/stats/clear; 1430-1433 channel create/publish/subscribe/unsubscribe. */
void gmb_bf_qacf_1423(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* to = vm_builtin_sarg(args, argc, 0);
    const char* ts = vm_builtin_sarg(args, argc, 1);
    const char* inp = vm_builtin_sarg(args, argc, 2);
    double pay = (argc > 3 && args[3].type == 1) ? args[3].num : 0.0;
    bout->is_num = 1;
    bout->result = gmb_agent_queue_send(to ? to : "", "user", ts ? ts : "", inp ? inp : "", pay) ? 1.0 : 0.0;   /* C# AiAgentSend: 1.0/0.0 */
}
void gmb_bf_qacf_str(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* agent = vm_builtin_sarg(args, argc, 0);
    char* out = NULL;
    if (func_code == 1424) out = gmb_agent_queue_receive(agent ? agent : "");
    else if (func_code == 1428) out = gmb_agent_queue_stats(agent ? agent : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(out ? out : "");
    bout->result_ref_bt = GMB_BT_STRING;
    if (out) free(out);
}
void gmb_bf_qacf_num(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* agent = vm_builtin_sarg(args, argc, 0);
    int ok = 0;
    if (func_code == 1425) ok = gmb_agent_queue_pause(agent ? agent : "");
    else if (func_code == 1426) ok = gmb_agent_queue_resume(agent ? agent : "");
    else if (func_code == 1427) ok = gmb_agent_queue_stop(agent ? agent : "");
    else if (func_code == 1429) ok = gmb_agent_queue_clear(agent ? agent : "");
    bout->is_num = 1;
    /* Booleano canonico -1/0 (come BOF/EOF/FOUND; parità IlRuntime.AiAgentPause/Resume/
     * Stop/QueueClear). Era 1.0/0.0 → divergenza dopo l'allineamento IL/C# a -1. */
    bout->result = ok ? -1.0 : 0.0;
}
static void gmb_bf_chan_1430(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* ch = vm_builtin_sarg(args, argc, 0);
    int ok = 0;
    if (func_code == 1430) ok = 1;   /* C# AiChannelCreate => 1.0 (ignora il canale) */
    else if (func_code == 1431) { const char* msg = vm_builtin_sarg(args, argc, 1); gmb_bus_channel_publish(ch ? ch : "", msg ? msg : ""); ok = 1; }
    else if (func_code == 1432 || func_code == 1433)
    {
        /* subscribe/unsubscribe: in BASIC il "handler" è un nome agente — la gestione
           a callback non è esposta; parity col managed (no-op) → canale creato, 1. */
        gmb_bus_channel_create(ch ? ch : "");
        ok = 1;
    }
    bout->is_num = 1;
    bout->result = ok ? 1.0 : 0.0;
}

/* AI.KGADDNODE/KGADDEDGE/KGQUERY (1436-1438): parità col grafo C# (KnowledgeGraphRuntime)
 * DELEGANDO al runtime condiviso gmb_kg.c (§0bis: una sola implementazione — prima qui c'era
 * una COPIA inline con handle/grafo SEPARATI e gli stessi bug: path senza truncate su 'a'
 * (from="alice->carol") e 'incident' mancante → KgC99Parity/KnowledgeGraphDemo divergenti
 * dal C99Native). */
static void gmb_bf_kg_1436(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_kg_add_node(vm_builtin_sarg(args, argc, 0));
}
static void gmb_bf_kg_1437(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    bout->is_num = 1; bout->result = (double)gmb_kg_add_edge(vm_builtin_sarg(args, argc, 0), vm_builtin_sarg(args, argc, 1));
}
static void gmb_bf_kg_1438(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char* out = gmb_kg_query(vm_builtin_sarg(args, argc, 0));
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(out ? out : ""); bout->result_ref_bt = GMB_BT_STRING;
    free(out);
}

static void gmb_bf_askmulti_c1287(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* question = vm_builtin_sarg(args, argc, 0);
    int topK = (argc > 1 && args[1].type == 1) ? (int)args[1].num : 5;
    if (topK <= 0) topK = 5;
    int rerank = (argc > 2 && args[2].type == 1) ? (int)args[2].num : 1;
    double mev = (argc > 3 && args[3].type == 1) ? args[3].num : (double)NAN;
    char* ctx = gmb_ai_sbm_ctx(question ? question : "", topK, 384, rerank, mev);
    /* Errore/insufficiente: parita' con C# AskMulti (risposta DIRETTA, nessun LLM). gmb_ai_sbm_ctx
     * ritorna il JSON d'errore OPPURE il messaggio testuale canonico insufficiente (gmb_brain.c). */
    if (ctx && strncmp(ctx, "{\"status\":", 10) == 0)
    {
        free(ctx);
        ctx = strdup("[insufficient evidence] No relevant documents found.");
    }
    if (ctx && strncmp(ctx, "[insufficient evidence]", 23) == 0)
    {
        gmb_obj_string_t* s = gmb_str_cstr(ctx ? ctx : "");
        free(ctx);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
        return;
    }
    char prompt[8192];
    snprintf(prompt, sizeof(prompt), "Domanda: %s\nContesto:\n%s", question ? question : "", ctx ? ctx : "");
    char* ans = gmb_llm_complete("Rispondi alla domanda usando SOLO il contesto fornito.", prompt);
    if (ctx) free(ctx);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(ans ? ans : ""); bout->result_ref_bt = GMB_BT_STRING;
    if (ans) free(ans);
}


/* AI SERVER (3505..3509) — gestione server LLM come processo esterno (runtime condiviso
 * gmb_runtime_c). Statement: risultato scartato (dispatcher pushera' 0.0 e la VM fara' POP);
 * qui conta solo l'effetto. Mai stub: json_* ritorna JSON di errore esplicito. */
static void gmb_bf_aiserver_start_c3505(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* dir = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    int port = argc >= 2 ? (int)args[1].num : 8020;
    if (port < 1 || port > 65535) port = 8020;
    char out[1024]; out[0] = 0;
    if (gmb_ai_server_json_start(dir ? dir : "", port, 0, out, sizeof out) != 0)
        vm_error(task, out[0] ? out : "AI SERVER START fallito");
}
static void gmb_bf_aiserver_stop_c3506(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)args; (void)argc; (void)func_code; (void)bout; (void)task;
    char out[1024]; out[0] = 0;
    if (gmb_ai_server_json_stop(out, sizeof out) != 0)
        vm_error(task, out[0] ? out : "gmb_bf_aiserver_stop_c3506 fallito");
}
static void gmb_bf_aiserver_status_c3507(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)args; (void)argc; (void)func_code; (void)bout; (void)task;
    char out[1024]; out[0] = 0;
    if (gmb_ai_server_json_status(out, sizeof out) != 0)
        vm_error(task, out[0] ? out : "gmb_bf_aiserver_status_c3507 fallito");
}
static void gmb_bf_aiserver_model_c3508(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout; (void)task;
    const char* dir = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    char out[1024]; out[0] = 0;
    if (gmb_ai_server_json_load(dir ? dir : "", out, sizeof out) != 0)
        vm_error(task, out[0] ? out : "AI SERVER MODEL fallito");
}
static void gmb_bf_aiserver_unload_c3509(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)args; (void)argc; (void)func_code; (void)bout; (void)task;
    char out[1024]; out[0] = 0;
    if (gmb_ai_server_json_unload(out, sizeof out) != 0)
        vm_error(task, out[0] ? out : "gmb_bf_aiserver_unload_c3509 fallito");
}

/* LLM MODEL / LLM.STREAMSTATS (M14 MoE out-of-core, GGUF; codici 3521..3526, 3528, 3529) —
 * provider "moestream": dlopen libgmb_ai_moestream (gmb_runtime_c). Ripristinati 2026-09-25
 * (erano stati rimossi per errore insieme al formato .gmoe). */
void gmb_bf_aimodelload_c3521(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* path = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    int resident = 0;
    if (argc >= 3) { const char* f = vm_builtin_sarg(args, argc, 2); if (f && strcasecmp(f, "RESIDENT") == 0) resident = 1; }
    if (gmb_moestream_stmt_load(name ? name : "", path ? path : "", resident) != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL LOAD: modello non valido o non trovato");
}
void gmb_bf_aimodelunload_c3522(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    if (gmb_moestream_stmt_unload(name ? name : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL UNLOAD fallito");
}
void gmb_bf_aimodelcache_c3523(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    int mb = argc >= 2 ? (int)args[1].num : 0;
    if (gmb_moestream_stmt_cache(name ? name : "", mb) != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL CACHE fallito");
}
void gmb_bf_aimodelprefetch_c3524(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    int topn = argc >= 2 ? (int)args[1].num : 0;
    if (gmb_moestream_stmt_prefetch(name ? name : "", topn) != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL PREFETCH fallito");
}
void gmb_bf_aimodeldevice_c3525(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* mode = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (gmb_moestream_stmt_device(name ? name : "", mode ? mode : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL DEVICE non disponibile");
}
void gmb_bf_aistreamstats_c3526(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    char buf[2048]; buf[0] = 0;
    if (gmb_moestream_streamstats_json(name ? name : "", buf, sizeof buf) != 0)
        vm_error(task, buf[0] ? buf : "LLM.STREAMSTATS: modello non trovato");
    gmb_obj_string_t* s = gmb_str_cstr(buf);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING;
}
void gmb_bf_aimodelpromote_c3528(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    if (gmb_moestream_stmt_promote(name ? name : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL PROMOTE fallito");
}
void gmb_bf_aimodeldemote_c3529(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    if (gmb_moestream_stmt_demote(name ? name : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL DEMOTE fallito");
}
static void gmb_bf_aimodelconvert_c3527(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* src = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* dst = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* fmt = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    /* fmt: default GGUF (llmmodelio_model_convert valida il formato). */
    const char* f2 = (fmt && *fmt) ? fmt : "GGUF";
    int rc = llmmodelio_model_convert(src ? src : "", dst ? dst : "", f2);
    if (rc != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL CONVERT: conversione fallita");
}
static void gmb_bf_aimodelinspect_c3543(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* dir = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    if (gmb_model_io_inspect_print(dir ? dir : "") != 0)
        gmb_raise_basic_error(task, 0, "AI MODEL INSPECT fallito");
}
/* EMITTER (2026-09-30, codice 3702): LLM MODEL SAVE <in>, <out> — round-trip/emissione artefatto
 * (writer condiviso llmmodelio_model_save: metadata + byte tensori). Stessa logica di IL/C99. */
static void gmb_bf_aimodelsave_c3702(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_save(a ? a : "", b ? b : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL SAVE fallito");   /* catchable (ON ERROR) */
}
/* MODIFICARE (2026-09-30, codice 3703): LLM MODEL ABLATE <in>, <out>, <pattern> — azzera i tensori
 * che combaciano (llmmodelio_model_ablate). Stessa logica di IL/C99/PCode. */
static void gmb_bf_aimodelablate_c3703(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_ablate(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL ABLATE fallito");   /* catchable (ON ERROR) */
}
/* RIMUOVERE (2026-10-01, codice 3712): LLM MODEL REMOVE <in>, <out>, <layer> — elimina un layer + rinumerazione. */
static const char* vm_builtin_sarg_any(const gmb_stack_entry_t* a, int argc, int i, char* buf, size_t n);
static void gmb_bf_aimodelremove_c3712(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    /* il layer puo' essere un NUMERO (es. 0): vm_builtin_sarg rende "" per i numerici ->
     * usare _any che li formatta (altrimenti layer="" -> -1 -> "fallito"). */
    char cbuf[32]; const char* c = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, cbuf, sizeof cbuf) : "";
    if (llmmodelio_model_remove_layer(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL REMOVE fallito");
}
/* DIFF (2026-10-01, codice 3704): LLM MODEL DIFF <a>, <b>, <out> — report strutturale. */
static void gmb_bf_aimodeldiff_c3704(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_diff(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL DIFF fallito");   /* catchable (ON ERROR) */
}
/* CHECKPOINT/ROLLBACK (2026-10-01, codici 3705/3706) — checkpoint-store. */
static void gmb_bf_aimodelcheckpoint_c3705(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_checkpoint(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL CHECKPOINT fallito");
}
static void gmb_bf_aimodelrollback_c3706(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_rollback(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL ROLLBACK fallito");
}
/* ADD (2026-10-01, codice 3707): LLM MODEL ADD <base>, <module>, <out> — AGGIUNGERE (merge moduli). */
static void gmb_bf_aimodeladd_c3707(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_add(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL ADD fallito");
}
/* ADAPTER (2026-10-01, codice 3708): LLM MODEL ADAPTER <base>, <adapter>, <out>[, <alpha>] — LoRA. */
static void gmb_bf_aimodeladapter_c3708(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    const char* d = argc >= 4 ? vm_builtin_sarg(args, argc, 3) : "1";
    if (llmmodelio_model_adapter(a ? a : "", b ? b : "", c ? c : "", d ? d : "1") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL ADAPTER fallito");
}
/* MODULE (2026-10-01, codice 3709): LLM MODEL MODULE <base>, <gmmod>, <module>, <out> — modulo dichiarato. */
static void gmb_bf_aimodelmodule_c3709(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    const char* d = argc >= 4 ? vm_builtin_sarg(args, argc, 3) : "";
    if (llmmodelio_model_module(a ? a : "", b ? b : "", c ? c : "", d ? d : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL MODULE fallito");
}
/* MODALITY (2026-10-01, codice 3710): LLM MODEL MODALITY <base>, <mmproj>, <out.gmmod>. */
static void gmb_bf_aimodelmodality_c3710(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_modality(a ? a : "", b ? b : "", c ? c : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL MODALITY fallito");
}
/* VALIDATE (2026-10-01, codice 3711): LLM MODEL VALIDATE <path>, <out>. */
static void gmb_bf_aimodelvalidate_c3711(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_validate(a ? a : "", b ? b : "") != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL VALIDATE fallito");
}
/* K1: LLM MODEL TRACE nome, path|OFF — trace esperti `.moetrace` (replay offline moe_sim). */
void gmb_bf_aimodeltrace_c3713(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* name = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* path = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "OFF";
    if (gmb_moestream_stmt_trace(name ? name : "", path ? path : "OFF", 1) != 0)
        gmb_raise_basic_error(task, 0, "LLM MODEL TRACE fallito");
}
static void gmb_bf_aimodelquantize_c3542(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* src = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* dst = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    int bits = argc >= 3 ? (int)args[2].num : 4;
    int group = argc >= 4 ? (int)args[3].num : 64;
    const char* imatrix = argc >= 5 ? vm_builtin_sarg(args, argc, 4) : NULL;
    int expert_bits = argc >= 6 ? (int)args[5].num : 0;
    if (gmb_model_io_quantize(src ? src : "", dst ? dst : "", bits, group, imatrix, expert_bits) != 0)
        gmb_raise_basic_error(task, 0, "AI MODEL QUANTIZE fallito");
}

/* MODEL COMPILER LLM (2026-09-23, codici 3604..3611) — verbi LLM (PLAN/POLICY/QUANT/MERGE/
 * PRUNE/EXTRACT/PROFILE/ARCH) eseguibili a RUNTIME anche nel VM/C. Stesso runtime CONDIVISO
 * gmb_runtime_c usato da C99Native: una sola sorgente, nessuna reimplementazione.
 * POLICY e ARCH dispatchano il sotto-verbo a runtime (llmmodelio_policy_cmd/llmmodelio_arch_cmd). */
static void gmb_bf_llmplan_c3615(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_plan(a ? a : "", b ? b : "") != 0) gmb_raise_basic_error(task, 0, "LLM PLAN fallito");
}
/* vm_builtin_sarg ritorna "" se l'argomento e' NUMERICO (es. `LLM POLICY ITEM 0`): qui gli
 * argomenti del verbo possono essere numeri (indici) -> li stringifichiamo con un buffer locale
 * per ogni argomento. */
static const char* vm_builtin_sarg_any(const gmb_stack_entry_t* a, int argc, int i, char* buf, size_t n) {
    if (i < argc && a[i].type == 2 && a[i].ref && a[i].ref->kind == GMB_OBJ_STRING)
        return ((gmb_obj_string_t*)a[i].ref)->data;
    if (i < argc && a[i].type != 2) { snprintf(buf, n, "%lld", (long long)a[i].num); return buf; }
    return "";
}
static void gmb_bf_gmbmodule_c3629(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    static char lbuf[8192];
    const char* sub = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (strcasecmp(sub, "REGISTER") == 0) { if (gmb_module_register(a, b, 0) != 0) gmb_raise_basic_error(task, 0, "GMB MODULE REGISTER: fallito"); }
    else if (strcasecmp(sub, "PRIORITY") == 0) { char pb[32]; if (gmb_module_priority(a, "", atoi(vm_builtin_sarg_any(args, argc, 2, pb, sizeof pb))) != 0) gmb_raise_basic_error(task, 0, "GMB MODULE PRIORITY: fallito"); }
    else if (strcasecmp(sub, "LIST") == 0) { gmb_module_list(lbuf, sizeof lbuf); gmb_print_string(gmb_string_alloc(lbuf)); gmb_print_newline(); }
    else if (strcasecmp(sub, "SYM") == 0) { if (!gmb_module_sym(a, b)) gmb_raise_basic_error(task, 0, "GMB MODULE SYM: non risolto"); }
    else if (strcasecmp(sub, "CALL") == 0) { lbuf[0] = 0; if (gmb_module_call_to(a, b, lbuf, sizeof lbuf) != 0) gmb_raise_basic_error(task, 0, "GMB MODULE CALL: fallito"); if (lbuf[0]) gmb_print_string(gmb_string_alloc(lbuf)); }
    else gmb_raise_basic_error(task, 0, "GMB MODULE: atteso REGISTER|PRIORITY|LIST|SYM|CALL");
}
static void gmb_bf_llmpolicy_c3616(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)func_code; (void)bout;
    char b0[32] = "", b1[32] = "", b2[32] = "", b3[32] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    /* Livello B (Policy CALLBACK, mondo C/VM-C): la GPM VM/C itera il piano (nativo) e invoca
     * la SUB compilata (closure) per-tensore, nel task corrente. */
    if (s && strcasecmp(s, "CALLBACK") == 0) {
        gmb_obj_closure_t* cb = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_CLOSURE)
            ? (gmb_obj_closure_t*)args[1].ref : NULL;
        if (!cb) { gmb_raise_basic_error(task, 0, "LLM POLICY CALLBACK: callback non valido"); return; }
        int n = llmmodelio_policy_item_count(); int applied = 0;
        static char item[4096];
        for (int i = 0; i < n; i++) {
            if (llmmodelio_policy_item_to(i, item, sizeof item) != 0) continue;
            char* t1 = strchr(item, '\t'); if (!t1) continue; *t1 = 0;
            char* t2 = strchr(t1 + 1, '\t'); if (!t2) continue; *t2 = 0;
            char* t3 = strchr(t2 + 1, '\t'); if (t3) *t3 = 0;
            const char* iname = item; const char* irole = t1 + 1; const char* idflt = t2 + 1;
            gmb_vm_call_closure_sync3_val(vm, task, cb, gmb_make_val_string(iname), gmb_make_val_string(irole), gmb_make_val_string(idflt));
            char fmt[64] = "";
            if (task->sync_result_type == 2 && task->sync_result_ref && task->sync_result_ref->kind == GMB_OBJ_STRING) {
                gmb_obj_string_t* rs = (gmb_obj_string_t*)task->sync_result_ref;
                size_t L = rs->length; if (L > 63) L = 63;
                memcpy(fmt, rs->data, L); fmt[L] = 0;
            }
            if (fmt[0]) { llmmodelio_policy_set_item(iname, fmt); applied++; }
        }
        return;
    }
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    if (llmmodelio_policy_cmd(s ? s : "", a ? a : "", b ? b : "", c ? c : "") != 0) gmb_raise_basic_error(task, 0, "LLM verbo fallito");
}
static void gmb_bf_llmquant_c3617(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    /* LLM QUANT <in>, <out>[, <bpw>]: col 3o arg (numerico > 0) auto-allocazione bit per-tensore
     * (stessa sorgente C del per-ruolo: llmmodelio_model_quantize_auto). */
    double bpw = argc >= 3 ? args[2].num : 0.0;
    int rc = (bpw > 0.0) ? llmmodelio_model_quantize_auto(a ? a : "", b ? b : "", bpw)
                         : llmmodelio_model_quantize(a ? a : "", b ? b : "");
    if (rc != 0) gmb_raise_basic_error(task, 0, "LLM QUANT fallito");
}
static void gmb_bf_llmmerge_c3618(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_model_merge(a ? a : "", b ? b : "", c ? c : "") != 0) gmb_raise_basic_error(task, 0, "LLM MERGE fallito");
}
static void gmb_bf_llmprune_c3619(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    int keep = argc >= 3 ? (int)args[2].num : 0;
    if (llmmodelio_model_prune(a ? a : "", b ? b : "", keep) != 0) gmb_raise_basic_error(task, 0, "LLM PRUNE fallito");
}
static void gmb_bf_llmextract_c3620(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    int layer = argc >= 3 ? (int)args[2].num : 0;
    int expert = argc >= 4 ? (int)args[3].num : 0;
    if (llmmodelio_model_extract(a ? a : "", b ? b : "", layer, expert) != 0) gmb_raise_basic_error(task, 0, "LLM EXTRACT fallito");
}
static void gmb_bf_llmprofile_c3621(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_profile(a ? a : "", b ? b : "") != 0) gmb_raise_basic_error(task, 0, "LLM PROFILE fallito");
}
static void gmb_bf_llmarch_c3622(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* s = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* p = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* o = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_arch_cmd(s ? s : "", p ? p : "", o ? o : "") != 0) gmb_raise_basic_error(task, 0, "LLM verbo fallito");
}
/* TENSOR/KERNEL/MODULE/KV (2026-09-24): stessa superficie .gmbarch dei verbi ARCH/POLICY, un solo
 * punto d'ingresso nel runtime C condiviso (gmb_llm_*_cmd). Sotto-verbo come primo argomento. */
static void gmb_bf_llmtensor_c3623(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    /* NIENTE vm_builtin_sarg: ritorna "" per gli argomenti NUMERICI (es. `LLM TENSOR EXPOLICY
     * <p>, 1` / `MPLAN <p>, 1`). vm_builtin_sarg_any stringifica i numerici -> il budget arriva. */
    char b0[64] = "", b1[512] = "", b2[64] = "", b3[64] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* p = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* o = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* e = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    if (llmmodelio_tensor_cmd(s ? s : "", p ? p : "", o ? o : "", e ? e : "") != 0) gmb_raise_basic_error(task, 0, "LLM verbo fallito");
}
/* B2 — LLM.TOKENCOUNT(model, testo): numero di token (fonte unica llmtoken). */
static void gmb_bf_llmtokencount_c3761(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)task;
    const char* m = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* t = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    bout->is_num = 1;
    bout->result = (double)llmmodelio_token_count(m ? m : "", t ? t : "");
}
/* ── B2 (2026-10-30) — verbi tokenizer con RITORNO come DATO (il comando stampa, la funzione
 * restituisce; `LLM … INTO` assegna senza stampare). Sorgente unica: gmb_runtime_c/gmb_llm_tok.c. */
static void gmb_bf_llmtokens_c3807(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    const char* m = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* t = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    char* r = gmb_llm_tokens(m ? m : "", t ? t : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.TOKENS: tokenizer non disponibile per il modello indicato"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmdetokenize_c3808(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    const char* m = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* i = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    char* r = gmb_llm_detokenize(m ? m : "", i ? i : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.DETOKENIZE: tokenizer non disponibile o id non validi"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmtensorvalue_c3809(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    char b0[64] = "", b1[512] = "", b2[256] = "", b3[256] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    char* r = gmb_llm_tensor_out(s ? s : "", a ? a : "", b ? b : "", c ? c : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.TENSOR: verbo fallito"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmpolicyout_c3810(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    char b0[64] = "", b1[512] = "", b2[256] = "", b3[256] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    char* r = gmb_llm_cmd_out(GMB_LLM_FAM_POLICY, s ? s : "", a ? a : "", b ? b : "", c ? c : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.POLICY: verbo fallito"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmarchout_c3811(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    char b0[64] = "", b1[512] = "", b2[256] = "", b3[256] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    char* r = gmb_llm_cmd_out(GMB_LLM_FAM_ARCH, s ? s : "", a ? a : "", b ? b : "", c ? c : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.ARCH: verbo fallito"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmmoduleout_c3812(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    char b0[64] = "", b1[512] = "", b2[256] = "", b3[256] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    char* r = gmb_llm_cmd_out(GMB_LLM_FAM_MODULE, s ? s : "", a ? a : "", b ? b : "", c ? c : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.MODULE: verbo fallito"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmkernelout_c3813(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    char b0[64] = "", b1[512] = "", b2[256] = "", b3[256] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    char* r = gmb_llm_cmd_out(GMB_LLM_FAM_KERNEL, s ? s : "", a ? a : "", b ? b : "", c ? c : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.KERNEL: verbo fallito"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmkvout_c3814(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code;
    char b0[64] = "", b1[512] = "", b2[256] = "", b3[256] = "";
    const char* s = argc >= 1 ? vm_builtin_sarg_any(args, argc, 0, b0, sizeof b0) : "";
    const char* a = argc >= 2 ? vm_builtin_sarg_any(args, argc, 1, b1, sizeof b1) : "";
    const char* b = argc >= 3 ? vm_builtin_sarg_any(args, argc, 2, b2, sizeof b2) : "";
    const char* c = argc >= 4 ? vm_builtin_sarg_any(args, argc, 3, b3, sizeof b3) : "";
    char* r = gmb_llm_cmd_out(GMB_LLM_FAM_KV, s ? s : "", a ? a : "", b ? b : "", c ? c : "");
    if (r == NULL) { gmb_raise_basic_error(task, 0, "LLM.KV: verbo fallito"); return; }
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
/* ASR — ASR.TRANSCRIBE(model, wav): speech-to-text Qwen3-ASR (engine nativo in gmb_runtime). */
void gmb_bf_asrtranscribe_c3805(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)task;
    const char* m = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* w = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* o = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    char* r = gmb_asr_cmd(m ? m : "", w ? w : "", o ? o : "");
    vm_builtin_rstr(r, &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_llmquality_c3628(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* c = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    const char* d = argc >= 4 ? vm_builtin_sarg(args, argc, 3) : "";
    if (llmmodelio_model_quality(a ? a : "", b ? b : "", c ? c : "", d ? d : "") != 0) gmb_raise_basic_error(task, 0, "LLM QUALITY fallito");
}
static void gmb_bf_llmcompile_c3627(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* a = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_compile(a ? a : "", b ? b : "") != 0) gmb_raise_basic_error(task, 0, "LLM COMPILE fallito");
}
static void gmb_bf_llmkernel_c3624(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* s = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* p = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* o = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_kernel_cmd(s ? s : "", p ? p : "", o ? o : "") != 0) gmb_raise_basic_error(task, 0, "LLM verbo fallito");
}
static void gmb_bf_llmmodule_c3625(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* s = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* p = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* o = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_module_cmd(s ? s : "", p ? p : "", o ? o : "") != 0) gmb_raise_basic_error(task, 0, "LLM verbo fallito");
}
static void gmb_bf_llmkv_c3626(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)bout;
    const char* s = argc >= 1 ? vm_builtin_sarg(args, argc, 0) : "";
    const char* p = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    const char* o = argc >= 3 ? vm_builtin_sarg(args, argc, 2) : "";
    if (llmmodelio_kv_cmd(s ? s : "", p ? p : "", o ? o : "") != 0) gmb_raise_basic_error(task, 0, "LLM verbo fallito");
}

/* AI REGISTER alias, capability, source, param (1510 = AIREGISTERSOURCE) — registra un provider;
 * per un source *GGUF il runtime C carica il modello MoE out-of-core (gmb_provider_register).
 * gmb_provider_register(role, name, kind, config): role=CAPABILITY, name=ALIAS (find_provider/
 * set_active/active in gmb_ai.c cercano per name — l'alias è la chiave di lookup). Prima
 * passava (a0=alias, a1=capability) invertito: AI REGISTER "completion","llama1b",...
 * registrava un provider chiamato "completion" invece di "llama1b" (Bugs/(open) -
 * AI-REGISTER-Ordine-Argomenti-Alias-vs-Capability.md — stesso bug su C99Native, corretto
 * insieme). */
static void gmb_bf_airegister_c1510(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task; (void)func_code;
    const char* a0 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
    /* capability puo' essere un NUMERO (%DECIDE/%CHAT…): vm_builtin_sarg rende "" per i
     * numerici, quindi serve _any (altrimenti il ruolo finisce su "completion"). */
    char rbuf[32];
    const char* a1 = (argc >= 2) ? vm_builtin_sarg_any(args, argc, 1, rbuf, sizeof rbuf) : "";
    const char* a2 = (argc >= 3) ? vm_builtin_sarg(args, argc, 2) : "";
    const char* a3 = (argc >= 4) ? vm_builtin_sarg(args, argc, 3) : "";
    if (!gmb_provider_register(a1 ? a1 : "", a0 ? a0 : "", a2 ? a2 : "", a3 ? a3 : ""))
        vm_error(task, "AI REGISTER: provider non registrato (per i modelli locali usa AI MODEL LOAD)");
    bout->result = -1;
}

/* --- RAG eval (2026-09-14): quartetto metriche retrieval (R1) + giudizi generazione tier-1
 * (R2), codici 3530-3537 — runtime condiviso gmb_rag_eval.c (§0bis). Pure math, risultato
 * scalare: precision/recall/mrr/ndcg su liste CSV di id, faithfulness/relevance/hallucination/
 * completeness su testo. In FONDO (gli id viaggiano nel chunk A1). */
void gmb_bf_rageval(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task;
    const char* a0 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
    const char* a1 = (argc >= 2) ? vm_builtin_sarg(args, argc, 1) : "";
    int k = (argc >= 3 && args[2].type == 1) ? (int)args[2].num : 0;
    switch (func_code) {
    case 3530: bout->result = gmb_rageval_precision_at(a0, a1, k); break;
    case 3531: bout->result = gmb_rageval_recall_at(a0, a1, k); break;
    case 3532: bout->result = gmb_rageval_mrr(a0, a1); break;
    case 3533: bout->result = gmb_rageval_ndcg_at(a0, a1, k); break;
    case 3534: bout->result = gmb_rageval_faithfulness(a0, a1); break;
    case 3535: bout->result = gmb_rageval_relevance(a0, a1); break;
    case 3536: bout->result = gmb_rageval_hallucination(a0, a1); break;
    case 3537: bout->result = gmb_rageval_completeness(a0, a1); break;
    /* L7 Training (2026-09-29): BLEU-4 + ROUGE-L (variante GMBasic, gmb_rag_eval.c).
     * Stesso handler perche' stessa forma (2 stringhe -> double); i codici sono 1166/1167
     * (1155/1156 erano gia' GOMAXPROCS/ATOMICINCREMENT — collisione presa dal gate). */
    case 1166: bout->result = gmb_bleu_score(a0, a1); break;
    case 1167: bout->result = gmb_rouge_l(a0, a1); break;
    /* Self-Consistency / Semantic Entropy (3538-3540): NON sono di questo dominio.
     * Avevano qui 3 case che chiamavano gmb_bf_consistency/gmb_bf_lexical_entropy/
     * gmb_bf_consistency_detail — funzioni MAI definite (build rossa) e comunque
     * IRRAGGIUNGIBILI (nessuna riga di tabella per quei codici su questo handler). Il
     * dominio ha il suo file (§0quater): gmb_uncertainty_exec.c, registrato nella tabella
     * gmb_bf_impl con vmc_bf_uncertainty. */
    default: vm_error(task, "builtin rag-eval non implementato su VM/C"); break;
    }
}

/* --- GenTest (2026-09-15): VECTORDB.GENTEST(collection, n, types) → stringa risultato.
 * 3 argomenti stringa (collection, n, types). Chiama gmb_vectordb_gentest e pusha il risultato. */
void gmb_bf_gentest(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)task;
    const char* coll = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
    const char* n_str = (argc >= 2) ? vm_builtin_sarg(args, argc, 1) : "5";
    const char* types = (argc >= 3) ? vm_builtin_sarg(args, argc, 2) : "simple";
    char* result = gmb_vectordb_gentest(coll, coll, n_str, types);
    gmb_obj_string_t* sr = gmb_str_cstr(result ? result : "ERROR: null result");
    free(result);
    bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
}

/* --- AI OBSERVE / OBSERVATION builtins (3512-3520): handler unico gmb_bf_observe.
 * 3512 = AiObserveFile, 3513 = AiObserveLog, 3514 = AiObserveScreen,
 * 3515 = AiObservationLatest, 3516 = AiObservation, 3517 = AiObservationHash,
 * 3518 = AiObservationCount, 3519 = AiObservationList, 3520 = AiObservationFile. */
void gmb_bf_observe(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task;
    switch (func_code) {
    case 3512: {
        const char* path = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        bout->result = (double)gmb_cua_observe_file(path);
        break;
    }
    case 3513: {
        const char* path = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        double tail = (argc >= 2) ? args[1].num : 200.0;
        bout->result = (double)gmb_cua_observe_log(path, tail);
        break;
    }
    /* A5 (2026-09-16): fonti OBSERVE di fase 2 (3600-3603). Fallimento ESPLICITO (vm_error →
     * TRY/CATCH → ON ERROR → fatale), mai un -1 scartato in silenzio. */
    case 3600: {
        const char* p1 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        double id = gmb_observe_test(p1);
        if (id < 0) { vm_error(task, "AI OBSERVE TEST: il file non è compilabile"); return; }
        bout->result = id;
        break;
    }
    case 3601: {
        const char* p1 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        const char* q1 = (argc >= 2) ? vm_builtin_sarg(args, argc, 1) : "";
        double id = gmb_observe_database(p1, q1);
        if (id < 0) { vm_error(task, "AI OBSERVE DATABASE: database non apribile o query fallita"); return; }
        bout->result = id;
        break;
    }
    case 3602: {
        const char* p1 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        double id = gmb_observe_process(p1);
        if (id < 0) { vm_error(task, "AI OBSERVE PROCESS: processo inesistente"); return; }
        bout->result = id;
        break;
    }
    case 3603: {
        const char* p1 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        double id = gmb_observe_browser(p1);
        if (id < 0) { vm_error(task, "AI OBSERVE BROWSER: browser/CDP non disponibile"); return; }
        bout->result = id;
        break;
    }
    case 3514: {
        const char* outPath = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
        double obs_id = (double)gmb_cua_observe_screen(outPath);
        if (obs_id < 0) {
            /* Fallimento ESPLICITO (mai silenzioso): prima il -1 veniva scartato e l'osservazione
             * mancava senza segnale. Parità con IlRuntime.AiObserveScreen (C#), che ora solleva. */
            char msg[512];
            snprintf(msg, sizeof(msg),
                "AI OBSERVE SCREEN: cattura schermo fallita per '%s' (permessi di registrazione "
                "schermo oppure percorso non scrivibile) — nessuna osservazione registrata",
                outPath ? outPath : "");
            vm_error(task, msg);
            return;
        }
        bout->result = obs_id;
        break;
    }
    case 3515: {
        gmb_string_t rs = gmb_obs_latest_id();
        gmb_obj_string_t* sr = gmb_str_cstr(rs.data ? rs.data : "");
        gmb_string_free(&rs);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3516: {
        double id_num = (argc >= 1) ? args[0].num : 0;
        gmb_string_t rs = gmb_obs_json((int)id_num);
        gmb_obj_string_t* sr = gmb_str_cstr(rs.data ? rs.data : "");
        gmb_string_free(&rs);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3517: {
        double id_num = (argc >= 1) ? args[0].num : 0;
        gmb_string_t rs = gmb_obs_hash((int)id_num);
        gmb_obj_string_t* sr = gmb_str_cstr(rs.data ? rs.data : "");
        gmb_string_free(&rs);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3518: {
        bout->result = (double)gmb_obs_count();
        break;
    }
    case 3519: {
        gmb_string_t rs = gmb_obs_list();
        gmb_obj_string_t* sr = gmb_str_cstr(rs.data ? rs.data : "");
        gmb_string_free(&rs);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3520: {
        double id_num = (argc >= 1) ? args[0].num : 0;
        gmb_string_t rs = gmb_obs_file((int)id_num);
        gmb_obj_string_t* sr = gmb_str_cstr(rs.data ? rs.data : "");
        gmb_string_free(&rs);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    default: vm_error(task, "builtin observation non implementato su VM/C"); break;
    }
}

/* --- VECTORDB eval advanced (2026-09-15): Diagnose, DiagnoseReport, Calibrate, EvalGate,
 * EvalDiff, Baseline, Drift, AbTest, Compare. Handler unico che dispatches per func_code.
 * I char* ritorni sono mallocati (gmb_rageval_*: caller free).
 * NB: Diagnose = BuiltInFunction 3593 (era 3543: collideva con AIMODELINSPECT, la cui voce
 * precedeva in tabella → VECTORDB.DIAGNOSE finiva nell'handler AIMODELINSPECT). */
static void gmb_bf_vectordb_advanced(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)task;
    const char* a0 = (argc >= 1) ? vm_builtin_sarg(args, argc, 0) : "";
    const char* a1 = (argc >= 2) ? vm_builtin_sarg(args, argc, 1) : "";
    switch (func_code) {
    case 3593: {
        char* r = gmb_rageval_diagnose(a0);
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : "");
        free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3544: {
        char* r = gmb_rageval_diagnose_report();
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : "");
        free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3545:
        /* CALIBRATE: combined JSON da kappa/wilson/threshold. */
        { char* r = gmb_rageval_calibrate(a0); gmb_obj_string_t* sr = gmb_str_cstr(r ? r : ""); free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING; }
        break;
    case 3546: {
        char* r = gmb_rageval_eval_gate(a0, a1);
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : "");
        free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3547: {
        char* r = gmb_rageval_eval_diff(a0, a1);
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : "");
        free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3548:
        { char* r = gmb_rageval_baseline(a0); gmb_obj_string_t* sr = gmb_str_cstr(r ? r : ""); free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING; }
        break;
    case 3549:
        bout->result = gmb_rageval_psi(a0, a1);
        break;
    case 3550: {
        /* ABTEST = proporzioni {"n","success"} → JSON {z_score,p_value,effect_size}
         * (gmb_rageval_ab_test). Prima usava gmb_rageval_z_test (array di valori, ritorno
         * double) → output a chiave singola, divergente dal C#/guida. */
        char* r = gmb_rageval_ab_test(a0, a1);
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : "");
        free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    case 3551: {
        char* r = gmb_rageval_compare(a0);
        gmb_obj_string_t* sr = gmb_str_cstr(r ? r : "");
        free(r); bout->is_num = 0; bout->result_ref = (gmb_obj_t*)sr; bout->result_ref_bt = GMB_BT_STRING;
        break;
    }
    default: vm_error(task, "builtin vectordb-advanced non implementato su VM/C"); break;
    }
}

/* --- AI FINETUNE / AI LORA APPLY / stato (surface MLX LoRA, codici 1993-2000) --- */
static char* gmb_ft_dup(const char* s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char* d = (char*)malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}
/* Estrae l'argomento opzioni come stringa JSON: stringa diretta o dict serializzato.
 * OWNERSHIP: ritorna SEMPRE un buffer malloc di proprieta' del CHIAMANTE, da liberare
 * con free(); NULL se assente/non valido. Prima il ramo dict ritornava `js.data` di una
 * gmb_string_t LOCALE (allocata da gmb_dict_to_json) che nessuno liberava -> leak a ogni
 * chiamata con opts-dict. Il ramo stringa era invece un prestito dall'oggetto VM. */
static char* gmb_ft_json_arg(const gmb_stack_entry_t* args, int argc)
{
    if (argc <= 1 || !args[1].ref) return NULL;
    if (args[1].ref->kind == GMB_OBJ_STRING) return gmb_ft_dup(((gmb_obj_string_t*)args[1].ref)->data);
    if (args[1].ref->kind == GMB_OBJ_DICT) {
        gmb_dict_t* d = ((gmb_obj_shared_val_t*)args[1].ref)->dict;
        if (!d) return NULL;
        gmb_string_t js = gmb_dict_to_json(d);
        char* out = gmb_ft_dup(js.data);
        free(js.data);
        return out;
    }
    return NULL;
}
static void gmb_bf_1993(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    char buf[4096];
    const char* model = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    /* B4 (§0ter): il training gira su un processo esterno (popen, puo' durare minuti) →
     * park su thread dedicato se chiamato da un task GPM; il worker NON si blocca. Fallback
     * sincrono se lo spawn fallisce o fuori da un task GPM (REPL/flusso principale). */
    if (task && vm)
    {
        gmb_str_job_t* j = (gmb_str_job_t*)calloc(1, sizeof(gmb_str_job_t));
        if (j)
        {
            j->kind = 2;
            j->model = model ? strdup(model) : NULL;
            j->json = json ? strdup(json) : NULL;
            if (vm_try_park_str(vm, task, j)) { free(json); return; } /* parcheggiato: nessun push qui */
            free(j->model); free(j->json); free(j);
        }
    }
    int rc = gmb_ai_finetune_run_opts(model, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { gmb_raise_basic_error(task, 0, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1994(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code;
    char buf[4096];
    const char* ckpt = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    /* B4 (§0ter): stesso park di gmb_bf_1993 (deploy/verifica adapter, processo esterno). */
    if (task && vm)
    {
        gmb_str_job_t* j = (gmb_str_job_t*)calloc(1, sizeof(gmb_str_job_t));
        if (j)
        {
            j->kind = 3;
            j->model = ckpt ? strdup(ckpt) : NULL;
            j->json = json ? strdup(json) : NULL;
            if (vm_try_park_str(vm, task, j)) { free(json); return; } /* parcheggiato: nessun push qui */
            free(j->model); free(j->json); free(j);
        }
    }
    int rc = gmb_ai_lora_apply_run_opts(ckpt, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { gmb_raise_basic_error(task, 0, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
/* Stream D — AI DISTILL: BUILD (2028) + STATUS (2029). */
static void gmb_bf_2028(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code; (void)vm;
    const char* jsonl = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    char buf[4096];
    int rc = gmb_distill_build_opts(jsonl, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { vm_error(task, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_2029(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    vm_builtin_rstr(gmb_ft_dup(gmb_distill_status()), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
/* Stream D — AI DISTILL SAMPLE (2030) + SAMPLEN (2031) + SAMPLEFAIL (2032). */
static void gmb_bf_2030(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code; (void)vm;
    const char* prompts = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    char buf[8192];
    int rc = gmb_distill_sample_opts(prompts, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { vm_error(task, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_2031(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_distill_samplen();
}
static void gmb_bf_2032(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_distill_samplefail();
}
static void gmb_bf_2215(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_distill_sampledone();
}
static void gmb_bf_2216(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int idx = (argc > 0 && args[0].type == 1) ? (int)args[0].num : 0;
    bout->result = gmb_distill_evalacc(idx);
}
/* Stream D — AI DISTILL EVAL (2033) + EVALTOK (2034) + EVALPASS (2035). */
static void gmb_bf_2033(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code; (void)vm;
    const char* prompts = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    char buf[8192];
    int rc = gmb_distill_eval_opts(prompts, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { vm_error(task, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_2034(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int idx = (argc > 0 && args[0].type == 1) ? (int)args[0].num : 0;
    bout->result = gmb_distill_evaltok(idx);
}
static void gmb_bf_2035(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    int idx = (argc > 0 && args[0].type == 1) ? (int)args[0].num : 0;
    bout->result = gmb_distill_evalpass(idx);
}
/* Stream D — AI DISTILL APPLY (2036) + RESTORE (2037). */
static void gmb_bf_2036(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code; (void)vm;
    const char* ckpt = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    char buf[8192];
    int rc = gmb_distill_apply_opts(ckpt, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { vm_error(task, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_2037(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code; (void)vm;
    const char* backup = vm_builtin_sarg(args, argc, 0);
    char buf[8192];
    int rc = gmb_distill_restore_opts(backup, buf, sizeof buf);
    if (rc != 0) { vm_error(task, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
/* Stream D — AI DISTILL TRAIN (2210) + LOSS (2211) + HOLD (2212) + CKPT (2213) + DONE (2214). */
static void gmb_bf_2210(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)func_code; (void)vm;
    const char* dataset = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    char buf[8192];
    int rc = gmb_distill_train_opts(dataset, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { vm_error(task, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_2211(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_distill_loss();
}
static void gmb_bf_2212(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_distill_hold();
}
static void gmb_bf_2213(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    vm_builtin_rstr(gmb_ft_dup(gmb_distill_ckpt()), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_2214(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_distill_done();
}
static void gmb_bf_1995(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    vm_builtin_rstr(gmb_ft_dup(gmb_ai_finetune_status()), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1996(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_ai_finetune_loss();
}
static void gmb_bf_1997(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = gmb_ai_finetune_hold();
}
static void gmb_bf_1998(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    vm_builtin_rstr(gmb_ft_dup(gmb_ai_finetune_ckpt()), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
static void gmb_bf_1999(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_finetune_done();
}
static void gmb_bf_2000(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_finetune_greedy();
}
/* B3 fine-tune (2026-09-13): misure di apprendimento (baseline e dopo). */
static void gmb_bf_2202(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_finetune_greedy_after();
}
static void gmb_bf_2203(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_finetune_demo_changed();
}
static void gmb_bf_2204(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_finetune_base_train();
}
static void gmb_bf_2205(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_finetune_base_hold();
}
/* --- TRAIN RUNS (F1): query del registro run (parità C# JobRegistry.Json/Count). --- */
static void gmb_bf_2300(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* r = gmb_ai_runs_json();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "{\"count\":0,\"runs\":[]}");
    bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_2301(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_runs_count();
}
static void gmb_bf_2302(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    bout->result = (double)gmb_ai_runs_clear();
}
static void gmb_bf_2303(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* r = gmb_ai_train_config_json();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "");
    bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_2304(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* k = vm_builtin_sarg(args, argc, 0);
    const char* v = vm_builtin_sarg(args, argc, 1);
    char* r = gmb_ai_train_config_set(k ? k : "", v ? v : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "");
    bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_2305(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* s = vm_builtin_sarg(args, argc, 0);
    int id = s ? atoi(s) : 0;
    char* r = gmb_ai_runs_item(id);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "");
    bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_2306(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    char* r = gmb_ai_dataset_stats(p ? p : "");
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "");
    bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_2307(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)args; (void)argc; (void)func_code; (void)task;
    char* r = gmb_ai_lora_list();
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(r ? r : "");
    bout->result_ref_bt = GMB_BT_STRING;
    if (r) free(r);
}
static void gmb_bf_2309(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    /* 1 arg → (out) ultima run; 2 arg → (run, out). */
    const char* a = vm_builtin_sarg(args, argc, 0);
    const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (argc >= 2) bout->result = (double)gmb_ai_train_export_run(b ? b : "", a ? atoi(a) : 0);
    else bout->result = (double)gmb_ai_train_export(a ? a : "");
}
static void gmb_bf_2310(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* p = vm_builtin_sarg(args, argc, 0);
    bout->result = (double)gmb_ai_runs_stop(p ? atoi(p) : 0);
}
/* TRAIN LORA SAVE/LOAD (LLM.LORASAVE/LORALOAD): adapter RAW <-> .safetensors (gmbllm). */
static void gmb_bf_2312(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)task;
    const char* a = vm_builtin_sarg(args, argc, 0); const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_adapter_save(a ? a : "", b ? b : "") != 0) { gmb_raise_basic_error(task, 0, "LLM.LORASAVE: fallito"); return; }
}
static void gmb_bf_2313(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task) {
    (void)vm; (void)func_code; (void)task;
    const char* a = vm_builtin_sarg(args, argc, 0); const char* b = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    if (llmmodelio_model_adapter_load(a ? a : "", b ? b : "") != 0) { gmb_raise_basic_error(task, 0, "LLM.LORALOAD: fallito"); return; }
}
/* TRAIN EVAL GATE → LLM.EVALGATE(ref, cand): verdetto JSON dal runtime gmbllm (gate ppl). */
static void gmb_bf_2311(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    const char* r = vm_builtin_sarg(args, argc, 0);
    const char* c = argc >= 2 ? vm_builtin_sarg(args, argc, 1) : "";
    char g[1024];
    llmmodelio_model_gate(r ? r : "", c ? c : "", "", g, (int)sizeof g);
    bout->is_num = 0;
    bout->result_ref = (gmb_obj_t*)gmb_str_cstr(g);
    bout->result_ref_bt = GMB_BT_STRING;
}
/* TRAIN DPO (F3): preference optimization (model, json_opts) -> gmb_ai_dpo_run_opts. */
static void gmb_bf_2308(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char buf[4096];
    const char* model = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    int rc = gmb_ai_dpo_run_opts(model, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { gmb_raise_basic_error(task, 0, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
/* TRAIN GRPO (G-opt2): RLHF senza critic (model, json_opts) -> gmb_ai_grpo_run_opts. */
static void gmb_bf_2314(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    char buf[4096];
    const char* model = vm_builtin_sarg(args, argc, 0);
    char* json = gmb_ft_json_arg(args, argc);
    int rc = gmb_ai_grpo_run_opts(model, json, buf, sizeof buf);
    free(json);
    if (rc != 0) { gmb_raise_basic_error(task, 0, buf); return; }
    vm_builtin_rstr(gmb_ft_dup(buf), &bout->is_num, &bout->result_ref, &bout->result_ref_bt);
}
void gmb_bf_dbf_validate_c2197(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)func_code; (void)task;
    /* VALIDATE è un token CONDIVISO (DBF/ISAM/LEDGER) che non aveva alcun codice: la VM lo
     * trattava come funzione ignota → 0 (mentre IL/PCode danno -1 per un ledger valido).
     * § MiniTests/ledger/ledger_ops.bas. */
    int64_t hv = (argc >= 1 && args[0].type == 1) ? (int64_t)args[0].num : gmb_db_current_ledger();
    /* VALIDATE è un BOOL (C#/IL): CONVERSIONS.CSTR deve rendere "True"/"False", non "-1"/"0".
     * Senza num_bt il risultato è un NUMBER → CSTR stampa -1 (divergenza § DbLedgerFullTests). */
    bout->num_bt = GMB_BT_BOOL;
    if (gmb_db_current_ledger() > 0 && hv == gmb_db_current_ledger())
    {
        bout->result = gmb_db_ledger_validate(hv) == 0 ? -1.0 : 0.0;
        return;
    }
    bout->result = gmb_db_dbf_validate(hv) == 0 ? -1.0 : 0.0;
}
/* ── Context Engine (P2 AI, codici 3552-3558): dispatch per func_code ── */
static void gmb_bf_context(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* agent = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* val  = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    double num = argc >= 2 ? args[1].num : 0;
    switch (func_code) {
    case 3552: bout->result = gmb_ctx_used(agent); break;
    case 3553: bout->result = gmb_ctx_budget(agent); break;
    case 3554: bout->result = gmb_ctx_overflow(agent); break;
    case 3557: bout->result = gmb_ctx_set_budget(agent, num); break;
    case 3558: bout->result = gmb_ctx_set_view(agent, val); break;
    case 3566: bout->result = gmb_ctx_set_policy(agent, val); break;
    case 3567: bout->result = gmb_ctx_set_selector(agent, val); break;
    /* §3.6 PARSEC: context builder a segmenti (agente). */
    case 3735: bout->result = gmb_ctxseg_create(agent); break;
    case 3736: {
        const char* kind = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
            ? ((gmb_obj_string_t*)args[1].ref)->data : "";
        const char* text = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING)
            ? ((gmb_obj_string_t*)args[2].ref)->data : "";
        bout->result = gmb_ctx_add(agent, kind, text); break;
    }
    case 3737: bout->result = gmb_ctx_map(agent); break;
    case 3738: bout->result = gmb_ctx_trim(agent); break;
    case 3739: bout->result = gmb_ctx_gc(agent); break;
    case 3740: bout->result = gmb_ctx_commit(agent); break;
    /* §3.6 PARSEC: cache/version-hash. */
    case 3741: {
        gmb_string_t gs = gmb_ctx_hash(agent);
        if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); gmb_string_free(&gs);
            bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
        else { bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(""); bout->result_ref_bt = GMB_BT_STRING; }
        break;
    }
    case 3742: bout->result = gmb_ctx_valid(agent, val); break;
    /* §3.6 PARSEC: metriche task (JSON). */
    case 3743: {
        char* j = gmb_task_metrics(agent);
        gmb_obj_string_t* s = gmb_str_cstr(j ? j : "{}"); free(j);
        bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; break;
    }
    case 3555:
    case 3556:
        /* AiContextView / AiContextViewDebug: return gmb_string_t */
        {
            gmb_string_t gs = gmb_ctx_view(agent);
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); gmb_string_free(&gs);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else bout->is_num = 0;
        }
        break;
    default: bout->result = 0; break;
    }
}

/* ── AI WORKER + AI STATE (P3, codici 3559-3565): dispatch per func_code ── */
static void gmb_bf_worker(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    const char* s0 = (argc >= 1 && args[0].type == 2 && args[0].ref && args[0].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[0].ref)->data : "";
    const char* s1 = (argc >= 2 && args[1].type == 2 && args[1].ref && args[1].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[1].ref)->data : "";
    const char* s2 = (argc >= 3 && args[2].type == 2 && args[2].ref && args[2].ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)args[2].ref)->data : "";
    switch (func_code) {
    case 3559: bout->result = gmb_worker_new(s0, s1); break;
    case 3560: bout->result = gmb_worker_run(s0); break;
    case 3561: bout->result = gmb_state_save(s0, s1, s2); break;
    case 3565: bout->result = gmb_worker_spend(); break;
    case 3563: bout->result = gmb_worker_rounds(); break;
    case 3564: bout->result = gmb_worker_rescued(); break;
    case 3568: bout->result = gmb_worker_invoke(s0); break;   /* AI WORKER "<TASK>" */
    case 3592:
        /* AI REPLAY AUTO "<sessione>": args = (session). */
        bout->result = gmb_replay_auto(argc >= 1 ? s0 : "");
        break;
    case 3570:
        /* AI WORKER MANAGE [, cycles] [REQUIRE <expr>]: args = (cycles, require, plan). */
        bout->result = gmb_worker_manage(argc >= 1 ? args[0].num : 0, argc >= 2 ? s1 : "",
                                         argc >= 3 ? s2 : "");
        break;
    case 3569:
        /* LOOP ["<TASK>"[, rounds]][ REQUIRE <expr>]: args = (task, rounds, require). */
        bout->result = gmb_worker_loop(s0, argc >= 2 ? args[1].num : 0, argc >= 3 ? s2 : "");
        break;
    case 3562:
        /* AiStateLoad: return gmb_string_t */
        {
            gmb_string_t gs = gmb_state_load(s0, s1);
            if (gs.data) { gmb_obj_string_t* s = gmb_str_cstr(gs.data); gmb_string_free(&gs);
                bout->is_num = 0; bout->result_ref = (gmb_obj_t*)s; bout->result_ref_bt = GMB_BT_STRING; }
            else bout->is_num = 0;
        }
        break;
    default: bout->result = 0; break;
    }
}
void gmb_bf_tray(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code, gmb_builtin_out_t* bout, gmb_task_t* task);

// Tabella syscall -> handler. Ogni voce passa da GMB_BF_ENTRY: con GMB_VM_MIN i domini
// non-core potranno essere omessi a compile-time (cosi0027 il linker scarta i loro handler).
#ifndef GMB_BF_ENTRY
#define GMB_BF_ENTRY(code, fn) { code, fn },
#endif

#ifndef GMB_VM_SPECIALIZED

static const gmb_bf_impl_t gmb_bf_impl_core[] = {
    { 0, gmb_bf_0 },
    { 1, gmb_bf_1 },
    { 2, gmb_bf_6 },
    { 3, gmb_bf_6 },
    { 4, gmb_bf_2 },
    { 5, gmb_bf_5 },
    { 7, gmb_bf_clng },
    { 8, gmb_bf_7 },
    { 9, gmb_bf_fix },
    { 10, gmb_bf_int },
    { 11, gmb_bf_floor },
    { 12, gmb_bf_9 },
    { 13, gmb_bf_trunc },
    { 14, gmb_bf_ceiling },
    { 15, gmb_bf_13 },
    { 16, gmb_bf_16 },
    { 17, gmb_bf_17 },
    { 18, gmb_bf_18 },
    { 19, gmb_bf_abs },
    { 54, gmb_bf_19 },
    { 55, gmb_bf_55 },
    { 56, gmb_bf_56 },
    { 57, gmb_bf_57 },
    { 58, gmb_bf_57 },
    { 61, gmb_bf_61 },
    { 62, gmb_bf_61 },
    { 63, gmb_bf_61 },
    { 64, gmb_bf_61 },
    { 65, gmb_bf_65 },
    { 66, gmb_bf_65 },
    { 67, gmb_bf_67 },
    { 68, gmb_bf_67 },
    { 69, gmb_bf_69 },
    { 70, gmb_bf_69 },
    { 73, gmb_bf_73 },
    { 74, gmb_bf_73 },
    { 59, gmb_bf_59 },
    { 60, gmb_bf_59 },
    { 71, gmb_bf_71 },
    { 72, gmb_bf_71 },
    { 75, gmb_bf_75 },
    { 76, gmb_bf_75 },
    { 77, gmb_bf_77 },
    { 78, gmb_bf_78 },
    { 79, gmb_bf_79 },
    { 870, gmb_bf_870 },
    { 871, gmb_bf_870 },
    { 872, gmb_bf_872 },
    { 873, gmb_bf_873 },
    { 874, gmb_bf_873 },
    { 875, gmb_bf_875 },
    { 876, gmb_bf_876 },
    { 877, gmb_bf_877 },
    { 878, gmb_bf_877 },
    { 879, gmb_bf_879 },
    { 880, gmb_bf_879 },
    { 881, gmb_bf_881 },
    { 882, gmb_bf_881 },
    { 883, gmb_bf_883 },
    { 884, gmb_bf_884 },
    { 885, gmb_bf_885 },
    { 886, gmb_bf_886 },
    { 887, gmb_bf_887 },
    { 1000, gmb_bf_1000_c1000 },
    { 1001, gmb_bf_1000_c1001 },
    { 1002, gmb_bf_1000_c1002 },
    { 1004, gmb_bf_1000_c1004 },
    { 1005, gmb_bf_1000_c1005 },
    { 772, gmb_bf_772_c772 },
    { 773, gmb_bf_772_c773 },
    { 776, gmb_bf_772_c776 },
    { 778, gmb_bf_772_c778 },
    { 779, gmb_bf_772_c779 },
    { 781, gmb_bf_772_c781 },
    { 774, gmb_bf_774 },
    { 775, gmb_bf_775 },
    { 777, gmb_bf_777 },
    { 780, gmb_bf_780 },
    { 1003, gmb_bf_1003 },
    { 1010, gmb_bf_1010 },
    { 1011, gmb_bf_1010 },
    { 1012, gmb_bf_1010 },
    { 1016, gmb_bf_1010 },
    { 1006, gmb_bf_1006 },
    { 1007, gmb_bf_1006 },
    { 1008, gmb_bf_1006 },
    { 1009, gmb_bf_1006 },
    { 1013, gmb_bf_1013 },
    { 1014, gmb_bf_1013 },
    { 1036, gmb_bf_1036 },
    { 1037, gmb_bf_73 },
    { 1038, gmb_bf_67 },
    { 1039, gmb_bf_61 },
    { 1040, gmb_bf_69 },
    { 1041, gmb_bf_71 },
    { 1042, gmb_bf_57 },
    { 1043, gmb_bf_59 },
    { 1044, gmb_bf_1044 },
    { 1045, gmb_bf_1044 },
    { 1046, gmb_bf_1046 },
    { 1047, gmb_bf_1046 },
    { 1048, gmb_bf_1048 },
    { 1049, gmb_bf_1049 },
    { 1050, gmb_bf_65 },
    { 1051, gmb_bf_61 },
    { 1052, gmb_bf_1052 },
    { 1053, gmb_bf_1052 },
    { 1054, gmb_bf_1054 },
    { 1055, gmb_bf_1054 },
    { 1056, gmb_bf_1056 },
    { 1057, gmb_bf_1057 },
    { 1058, gmb_bf_1058 },
    { 1059, gmb_bf_1059 },
    { 1060, gmb_bf_1060 },
    { 1061, gmb_bf_1061 },
    { 1062, gmb_bf_1062 },
    { 1063, gmb_bf_1063 },
    { 1064, gmb_bf_1064 },
    { 1065, gmb_bf_1065 },
    { 1066, gmb_bf_1066 },
    { 1067, gmb_bf_1067 },
    { 1068, gmb_bf_1068 },
    { 1069, gmb_bf_1069 },
    { 1070, gmb_bf_1070 },
    { 1071, gmb_bf_1071 },
    { 1072, gmb_bf_1072 },
    { 1073, gmb_bf_1073 },
    { 1074, gmb_bf_1074 },
    { 1075, gmb_bf_1075 },
    { 1076, gmb_bf_1076 },
    { 3820, gmb_bf_3820 },
    { 3821, gmb_bf_3820 },
    { 3822, gmb_bf_3822 },
    { 3844, gmb_bf_3844 },
    { 3846, gmb_bf_3846 },
    { 3848, gmb_bf_3848 },
    { 3847, gmb_bf_3847 },
    { 3845, gmb_bf_3845 },
    { 3823, gmb_bf_3823 },
    { 3824, gmb_bf_3823 },
    { 3825, gmb_bf_3825 },
    { 3826, gmb_bf_3825 },
    { 3827, gmb_bf_3823 },
    { 3828, gmb_bf_3823 },
    { 3829, gmb_bf_3829 },
    { 3830, gmb_bf_3829 },
    { 3831, gmb_bf_3823 },
    { 3832, gmb_bf_3823 },
    { 1077, gmb_bf_1077 },
    { 1078, gmb_bf_1078 },
    { 1079, gmb_bf_1078 },
    { 1080, gmb_bf_1078 },
    { 1081, gmb_bf_1081 },
    { 1082, gmb_bf_1081 },
    { 1083, gmb_bf_1081 },
    { 1084, gmb_bf_1084 },
    { 1085, gmb_bf_1084 },
    { 1086, gmb_bf_1084 },
    { 1087, gmb_bf_1087 },
    { 1088, gmb_bf_1087 },
    { 1089, gmb_bf_1087 },
    { 1090, gmb_bf_1090 },
    { 1091, gmb_bf_1090 },
    { 1092, gmb_bf_1090 },
    { 1093, gmb_bf_1093 },
    { 1094, gmb_bf_1093 },
    { 1095, gmb_bf_1095 },
    { 3849, gmb_bf_3849 },
    { 3856, gmb_bf_3856 },
    { 3857, gmb_bf_3857 },
    { 3859, gmb_bf_3859 },
    { 3885, gmb_bf_3885 },
    { 3886, gmb_bf_3885 },
    { 3887, gmb_bf_3885 },
    { 3888, gmb_bf_3885 },
    { 3889, gmb_bf_3885 },
    { 3890, gmb_bf_3885 },
    { 3891, gmb_bf_3885 },
    { 3892, gmb_bf_3885 },
    { 3893, gmb_bf_3885 },
    { 3894, gmb_bf_3885 },
    { 3895, gmb_bf_3885 },
    { 3896, gmb_bf_3885 },
    { 3897, gmb_bf_3885 },
    { 3876, gmb_bf_3876 },
    { 3877, gmb_bf_3876 },
    { 3878, gmb_bf_3876 },
    { 3879, gmb_bf_3879 },
    { 3880, gmb_bf_3876 },
    { 3881, gmb_bf_3876 },
    { 3882, gmb_bf_3883 },
    { 3883, gmb_bf_3883 },
    { 3858, gmb_bf_3858 },
    { 3850, gmb_bf_3849 },
    { 3851, gmb_bf_3849 },
    { 3852, gmb_bf_3849 },
    { 3853, gmb_bf_3849 },
    { 3854, gmb_bf_3849 },
    { 3855, gmb_bf_3849 },
    { 1096, gmb_bf_1095 },
    { 1097, gmb_bf_1097 },
    { 1098, gmb_bf_1097 },
    { 1099, gmb_bf_1097 },
    { 1015, gmb_bf_1013 },
    { 823, gmb_bf_823 },
    { 824, gmb_bf_823 },
    { 1020, gmb_bf_1020 },
    { 1024, gmb_bf_1024 },
    { 1025, gmb_bf_1025 },
    { 1029, gmb_bf_1029 },
    { 1030, gmb_bf_1030 },
    { 91, gmb_bf_91 },
    { 92, gmb_bf_92 },
    { 105, gmb_bf_105 },
    { 106, gmb_bf_105 },
    { 96, gmb_bf_96_c96 },
    { 97, gmb_bf_96_c97 },
    { 98, gmb_bf_96_c98 },
    { 99, gmb_bf_96_c99 },
    { 100, gmb_bf_96_c100 },
    { 101, gmb_bf_96_c101 },
    { 102, gmb_bf_102 },
    { 103, gmb_bf_103 },
    { 104, gmb_bf_104 },
    { 850, gmb_bf_850 },
    { 851, gmb_bf_851 },
    { 852, gmb_bf_852 },
    { 853, gmb_bf_853 },
    { 1019, gmb_bf_1019 },
    { 854, gmb_bf_854 },
    { 855, gmb_bf_855 },
    { 856, gmb_bf_856 },
    { 857, gmb_bf_856 },
    { 133, gmb_bf_133 },
    { 134, gmb_bf_134 },
    { 135, gmb_bf_135 },
    { 136, gmb_bf_136 },
    { 137, gmb_bf_137 },
    { 138, gmb_bf_138 },
    { 50, gmb_bf_50 },
    { 51, gmb_bf_51 },
    { 52, gmb_bf_52 },
    { 53, gmb_bf_53 },
    { 20, gmb_bf_20 },
    { 21, gmb_bf_atan },
    { 22, gmb_bf_21 },
    { 23, gmb_bf_cos },
    { 24, gmb_bf_exp },
    { 25, gmb_bf_23 },
    { 26, gmb_bf_log },
    { 27, gmb_bf_rnd },
    { 28, gmb_bf_sgn },
    { 29, gmb_bf_sin },
    { 30, gmb_bf_sqr },
    { 913, gmb_bf_vector_dist },
    { 914, gmb_bf_vector_dist },
    { 915, gmb_bf_vector_dist },
    { 916, gmb_bf_vector_dist },
    { 928, gmb_bf_928 },
    { 860, gmb_bf_860 },
    { 861, gmb_bf_860 },
    { 862, gmb_bf_862 },
    { 863, gmb_bf_862 },
    { 938, gmb_bf_938 },
    { 939, gmb_bf_938 },
    { 940, gmb_bf_938 },
    { 894, gmb_bf_894 },
    { 895, gmb_bf_894 },
    { 896, gmb_bf_894 },
    { 897, gmb_bf_897 },
    { 898, gmb_bf_898 },
    { 899, gmb_bf_899 },
    { 267, gmb_bf_267 },
    { 941, gmb_bf_941 },
    { 942, gmb_bf_941 },
    { 943, gmb_bf_941 },
    { 944, gmb_bf_941 },
    { 946, gmb_bf_941 },
    { 945, gmb_bf_945 },
    { 947, gmb_bf_947 },
    { 948, gmb_bf_948 },
    { 955, gmb_bf_948 },
    { 956, gmb_bf_948 },
    { 949, gmb_bf_949 },
    { 950, gmb_bf_949 },
    { 951, gmb_bf_951 },
    { 952, gmb_bf_951 },
    { 953, gmb_bf_953 },
    { 954, gmb_bf_954 },
    { 929, gmb_bf_929 },
    { 1100, gmb_bf_1100 },
    { 1101, gmb_bf_1101 },
    { 1102, gmb_bf_1102 },
    { 1103, gmb_bf_1103 },
    { 1104, gmb_bf_1104 },
    { 1105, gmb_bf_1105 },
    { 1106, gmb_bf_1106 },
    { 1107, gmb_bf_1107 },
    { 1108, gmb_bf_1108 },
    { 1109, gmb_bf_1109 },
    { 1110, gmb_bf_1110 },
    { 1111, gmb_bf_1111 },
    { 1112, gmb_bf_1112 },
    { 1113, gmb_bf_1113 },
    { 1114, gmb_bf_1114 },
    { 1115, gmb_bf_1115 },
    { 1116, gmb_bf_1116 },
    { 1117, gmb_bf_1117 },
    { 1118, gmb_bf_1118 },
    { 1119, gmb_bf_1119 },
    { 1120, gmb_bf_1120 },
    { 1121, gmb_bf_1121 },
    { 1122, gmb_bf_1122 },
    { 1123, gmb_bf_1123 },
    { 1124, gmb_bf_1124 },
    { 1125, gmb_bf_1125 },
    { 1126, gmb_bf_1126 },
    { 1127, gmb_bf_1127 },
    { 1128, gmb_bf_1128 },
    { 1129, gmb_bf_1129 },
    { 1130, gmb_bf_1130 },
    { 1131, gmb_bf_1131 },
    { 1132, gmb_bf_1132 },
    { 1133, gmb_bf_1133 },
    { 1134, gmb_bf_1134 },
    { 1135, gmb_bf_1135 },
    { 1136, gmb_bf_1136 },
    { 1168, gmb_bf_1168 },
    { 1169, gmb_bf_1169 },
    { 1137, gmb_bf_1137 },
    { 1138, gmb_bf_1138 },
    { 1139, gmb_bf_1139 },
    { 1140, gmb_bf_1140 },
    { 1141, gmb_bf_1141 },
    { 1142, gmb_bf_1142 },
    { 1143, gmb_bf_1143 },
    { 1144, gmb_bf_1144 },
    { 1145, gmb_bf_1145 },
    { 1146, gmb_bf_1146 },
    { 1147, gmb_bf_1147 },
    { 1148, gmb_bf_1148 },
    { 1149, gmb_bf_1149 },
    { 1150, gmb_bf_1150 },
    { 1151, gmb_bf_1151 },
    { 1152, gmb_bf_1152 },
    { 1153, gmb_bf_1153 },
    { 1190, gmb_bf_1190 },
    { 1191, gmb_bf_1191 },
    { 1192, gmb_bf_1192 },
    { 1193, gmb_bf_1193 },
    { 1194, gmb_bf_1194 },
    { 3898, gmb_bf_3898 },
    { 1195, gmb_bf_1195 },
    { 1196, gmb_bf_1196 },
    { 1197, gmb_bf_1197 },
    { 1198, gmb_bf_1198 },
    { 1199, gmb_bf_1199 },
    { 1200, gmb_bf_1200 },
    { 1154, gmb_bf_1154 },
    { 1990, gmb_bf_1990 },
    { 31, gmb_bf_917 },
    { 32, gmb_bf_tan },
    { 33, gmb_bf_32 },
    { 34, gmb_bf_32 },
    { 35, gmb_bf_log10 },
    { 36, gmb_bf_asin },
    { 37, gmb_bf_acos },
    { 38, gmb_bf_sinh },
    { 39, gmb_bf_cosh },
    { 299, gmb_bf_299_c299 },
    { 300, gmb_bf_299_c300 },
    { 301, gmb_bf_299_c301 },
    { 307, gmb_bf_299_c307 },
    { 306, gmb_bf_299_c306 },
    { 314, gmb_bf_299_c314 },
    { 315, gmb_bf_299_c315 },
    { 316, gmb_bf_299_c316 },
    { 308, gmb_bf_299_c308 },
    { 309, gmb_bf_299_c309 },
    { 310, gmb_bf_299_c310 },
    { 311, gmb_bf_299_c311 },
    { 312, gmb_bf_299_c312 },
    { 313, gmb_bf_299_c313 },
    { 302, gmb_bf_302 },
    { 621, gmb_bf_621 },
    { 625, gmb_bf_625 },
    { 627, gmb_bf_625 },
    { 626, gmb_bf_626 },
    { 628, gmb_bf_626 },
    { 629, gmb_bf_629 },
    { 630, gmb_bf_630 },
    { 631, gmb_bf_631 },
    { 632, gmb_bf_632 },
    { 633, gmb_bf_632 },
    { 634, gmb_bf_634 },
    { 635, gmb_bf_635 },
    { 636, gmb_bf_636 },
    { 637, gmb_bf_637 },
    { 638, gmb_bf_638 },
    { 639, gmb_bf_639 },
    { 642, gmb_bf_639 },
    { 562, gmb_bf_639 },
    { 640, gmb_bf_640 },
    { 641, gmb_bf_641 },
    { 643, gmb_bf_643 },
    { 644, gmb_bf_644 },
    { 645, gmb_bf_645 },
    { 646, gmb_bf_646 },
    { 647, gmb_bf_647 },
    { 760, gmb_bf_760 },
    { 304, gmb_bf_304 },
    { 303, gmb_bf_303 },
    { 317, gmb_bf_317 },
    { 329, gmb_bf_329 },
    { 323, gmb_bf_323 },
    { 324, gmb_bf_324 },
    { 320, gmb_bf_320 },
    { 333, gmb_bf_333 },
    { 330, gmb_bf_330 },
    { 331, gmb_bf_330 },
    { 332, gmb_bf_330 },
    { 318, gmb_bf_318 },
    { 322, gmb_bf_322 },
    { 619, gmb_bf_619 },
    { 620, gmb_bf_620 },
    { 321, gmb_bf_321 },
    { 336, gmb_bf_336 },
    { 345, gmb_bf_336 },
    { 334, gmb_bf_336 },
    { 335, gmb_bf_336 },
    { 337, gmb_bf_336 },
    { 343, gmb_bf_336 },
    { 344, gmb_bf_336 },
    { 346, gmb_bf_336 },
    { 356, gmb_bf_356 },
    { 357, gmb_bf_357 },
    { 342, gmb_bf_342 },
    { 352, gmb_bf_352 },
    { 353, gmb_bf_352 },
    { 350, gmb_bf_350 },
    { 351, gmb_bf_351 },
    { 354, gmb_bf_354 },
    { 355, gmb_bf_355 },
    { 362, gmb_bf_362 },
    { 363, gmb_bf_363 },
    { 364, gmb_bf_364 },
    { 365, gmb_bf_364 },
    { 358, gmb_bf_358 },
    { 359, gmb_bf_359 },
    { 360, gmb_bf_360 },
    { 366, gmb_bf_366 },
    { 374, gmb_bf_366 },
    { 371, gmb_bf_371 },
    { 372, gmb_bf_372 },
    { 373, gmb_bf_373 },
    { 368, gmb_bf_368 },
    { 370, gmb_bf_368 },
    { 367, gmb_bf_367 },
    { 1404, gmb_bf_1404 },
    { 1405, gmb_bf_1404 },
    { 1406, gmb_bf_1404 },
    { 1407, gmb_bf_1407 },
    { 1408, gmb_bf_1408 },
    { 1409, gmb_bf_1409 },
    { 1410, gmb_bf_1409 },
    { 1411, gmb_bf_1409 },
    { 1415, gmb_bf_1415 },
    { 1416, gmb_bf_1416 },
    { 1417, gmb_bf_1417 },
    { 1418, gmb_bf_1418 },
    { 1419, gmb_bf_1418 },
    { 1420, gmb_bf_1418 },
    { 1421, gmb_bf_1418 },
    { 1422, gmb_bf_1422 },
    { 1430, gmb_bf_chan_1430 },
    { 1431, gmb_bf_chan_1430 },
    { 1432, gmb_bf_chan_1430 },
    { 1433, gmb_bf_chan_1430 },
    { 1436, gmb_bf_kg_1436 },
    { 1437, gmb_bf_kg_1437 },
    { 1438, gmb_bf_kg_1438 },
    { 608, gmb_bf_608 },
    { 610, gmb_bf_610 },
    { 611, gmb_bf_611 },
    { 612, gmb_bf_612 },
    { 1269, gmb_bf_612 },
    { 609, gmb_bf_609 },
    { 268, gmb_bf_268 },
    { 269, gmb_bf_269 },
    { 616, gmb_bf_616_c616 },
    { 613, gmb_bf_616_c613 },
    { 614, gmb_bf_616_c614 },
    { 615, gmb_bf_616_c615 },
    { 617, gmb_bf_616_c617 },
    { 618, gmb_bf_618 },
    { 6, gmb_bf_6 },
    { 46, gmb_bf_46 },
    { 40, gmb_bf_40 },
    { 41, gmb_bf_41 },
    { 42, gmb_bf_42 },
    { 43, gmb_bf_43 },
    { 44, gmb_bf_44 },
    { 45, gmb_bf_44 },
    { 47, gmb_bf_47 },
    { 48, gmb_bf_48 },
    { 49, gmb_bf_49 },
    { 82, gmb_bf_80 },
    { 587, gmb_bf_587 },
    { 349, gmb_bf_349 },
    { 109, gmb_bf_109 },
    { 110, gmb_bf_109 },
    { 111, gmb_bf_109 },
    { 113, gmb_bf_109 },
    { 118, gmb_bf_109 },
    { 120, gmb_bf_109 },
    { 121, gmb_bf_109 },
    { 122, gmb_bf_109 },
    { 123, gmb_bf_109 },
    { 125, gmb_bf_109 },
    { 115, gmb_bf_115 },
    { 116, gmb_bf_115 },
    { 119, gmb_bf_119 },
    { 112, gmb_bf_112 },
    { 114, gmb_bf_114 },
    { 117, gmb_bf_117 },
    { 126, gmb_bf_126 },
    { 305, gmb_bf_305 },
    { 319, gmb_bf_319 },
    { 369, gmb_bf_369 },
    { 572, gmb_bf_572 },
    { 648, gmb_bf_648 },
    { 758, gmb_bf_758 },
    { 3400, gmb_bf_3400 },
    { 3401, gmb_bf_3401 },
    { 3402, gmb_bf_3402 },
    { 3403, gmb_bf_3403 },
    { 3404, gmb_bf_3404 },
    { 3405, gmb_bf_3405 },
    { 3406, gmb_bf_3406 },
    { 3407, gmb_bf_3407 },
    { 3408, gmb_bf_3408 },
    { 3410, gmb_bf_3410 },
    { 3420, gmb_bf_3420 },
    { 3431, gmb_bf_3431 },
    { 3432, gmb_bf_3432 },
    { 761, gmb_bf_761 },
    { 762, gmb_bf_762 },
    { 681, gmb_bf_681 },
    { 1210, gmb_bf_1210 },
    { 1211, gmb_bf_1211 },
    { 1212, gmb_bf_1212 },
    { 1240, gmb_bf_1240 },
    { 1241, gmb_bf_1241 },
    { 1242, gmb_bf_1242 },
    { 1243, gmb_bf_1243 },
    { 1230, gmb_bf_1230 },
    { 1231, gmb_bf_1231 },
    { 1232, gmb_bf_1232 },
    { 1233, gmb_bf_1233 },
    { 1234, gmb_bf_1234 },
    { 1235, gmb_bf_1235 },
    { 1236, gmb_bf_1236 },
    { 1237, gmb_bf_1237 },
    { 1238, gmb_bf_1238 },
    { 1239, gmb_bf_1239 },
    { 275, gmb_bf_275 },
    { 276, gmb_bf_276 },
    { 278, gmb_bf_278 },
    { 279, gmb_bf_279 },
    { 280, gmb_bf_280 },
    { 281, gmb_bf_281 },
    { 282, gmb_bf_282 },
    { 283, gmb_bf_283 },
    { 287, gmb_bf_287 },
    { 288, gmb_bf_288 },
    { 289, gmb_bf_289 },
    { 290, gmb_bf_290 },
    { 291, gmb_bf_291 },
    { 292, gmb_bf_292 },
    { 270, gmb_bf_270 },
    { 538, gmb_bf_538 },
    { 539, gmb_bf_539 },
    { 540, gmb_bf_540 },
    { 541, gmb_bf_541 },
    { 542, gmb_bf_542 },
    { 545, gmb_bf_542 },
    { 543, gmb_bf_543 },
    { 460, gmb_bf_460 },
    { 271, gmb_bf_271 },
    { 273, gmb_bf_273 },
    { 272, gmb_bf_272 },
    { 274, gmb_bf_274 },
    { 284, gmb_bf_284 },
    { 285, gmb_bf_285 },
    { 286, gmb_bf_286 },
    { 294, gmb_bf_294 },
    { 293, gmb_bf_293 },
    { 277, gmb_bf_277 },
    { 295, gmb_bf_295 },
    { 107, gmb_bf_107 },
    { 108, gmb_bf_107 },
    { 95, gmb_bf_95 },
    { 89, gmb_bf_89 },
    { 186, gmb_bf_186 },
    { 544, gmb_bf_544 },
    { 677, gmb_bf_677 },
    { 3802, gmb_bf_3802 },
    { 3803, gmb_bf_3802 },
    { 3804, gmb_bf_3802 },
    { 3815, gmb_bf_3815 },   /* MESSAGE.DELIVERY (BuiltInFunction.MESSAGEDELIVERY) */
    { 3816, gmb_bf_3816 },   /* LENB (BuiltInFunction.LENB): BYTE UTF-8 */
    { 127, gmb_bf_127 },
    { 515, gmb_bf_515 },
    { 516, gmb_bf_515 },
    { 519, gmb_bf_515 },
    { 517, gmb_bf_517 },
    { 518, gmb_bf_518 },
    { 521, gmb_bf_521 },
    { 522, gmb_bf_522 },
    { 523, gmb_bf_523 },
    { 525, gmb_bf_525 },
    { 513, gmb_bf_513 },
    { 514, gmb_bf_514 },
    { 520, gmb_bf_520 },
    { 524, gmb_bf_524 },
    { 526, gmb_bf_526 },
    { 530, gmb_bf_530 },
    { 531, gmb_bf_531 },
    { 532, gmb_bf_532 },
    { 533, gmb_bf_533 },
    { 534, gmb_bf_534 },
    { 529, gmb_bf_529 },
    { 535, gmb_bf_535 },
    { 266, gmb_bf_266 },
    { 128, gmb_bf_266 },
    { 129, gmb_bf_266 },
    { 130, gmb_bf_266 },
    { 131, gmb_bf_266 },
    { 132, gmb_bf_266 },
    { 655, gmb_bf_655 },
    { 537, gmb_bf_537 },
    { 536, gmb_bf_536 },
    { 594, gmb_bf_594 },
    { 595, gmb_bf_595 },
    { 596, gmb_bf_595 },
    { 591, gmb_bf_591 },
    { 592, gmb_bf_592 },
    { 593, gmb_bf_593 },
    { 597, gmb_bf_597 },
    { 297, gmb_bf_297 },
    { 298, gmb_bf_297 },
    { 586, gmb_bf_586 },
    { 588, gmb_bf_588 },
    { 589, gmb_bf_589 },
    { 590, gmb_bf_590 },
    { 393, gmb_bf_393 },
    { 394, gmb_bf_393 },
    { 395, gmb_bf_393 },
    { 396, gmb_bf_393 },
    { 397, gmb_bf_393 },
    { 398, gmb_bf_393 },
    { 399, gmb_bf_393 },
    { 400, gmb_bf_393 },
    { 401, gmb_bf_393 },
    { 402, gmb_bf_393 },
    { 403, gmb_bf_393 },
    { 680, gmb_bf_680 },
    { 697, gmb_bf_697 },
    { 699, gmb_bf_699 },
    { 700, gmb_bf_700 },
    { 671, gmb_bf_671 },
    { 672, gmb_bf_672 },
    { 673, gmb_bf_673 },
    { 685, gmb_bf_685 },
    { 686, gmb_bf_686 },
    { 687, gmb_bf_687 },
    { 688, gmb_bf_687 },
    { 689, gmb_bf_689 },
    { 690, gmb_bf_689 },
    { 691, gmb_bf_691 },
    { 692, gmb_bf_692 },
    { 693, gmb_bf_692 },
    { 695, gmb_bf_692 },
    { 696, gmb_bf_692 },
    { 694, gmb_bf_694 },
    { 481, gmb_bf_481 },
    { 482, gmb_bf_482 },
    { 1260, gmb_bf_1260 },
    { 1261, gmb_bf_1260 },
    { 1264, gmb_bf_1260 },
    { 1262, gmb_bf_1262 },
    { 1263, gmb_bf_1263 },
    { 483, gmb_bf_483 },
    { 484, gmb_bf_483 },
    { 485, gmb_bf_483 },
    { 3760, gmb_bf_strdate_c3760 },
    { 1228, gmb_bf_1228 },
    { 493, gmb_bf_493 },
    { 409, gmb_bf_409 },
    { 410, gmb_bf_410 },
    { 411, gmb_bf_411 },
    { 412, gmb_bf_412 },
    { 413, gmb_bf_413 },
    { 414, gmb_bf_414 },
    { 415, gmb_bf_415 },
    { 416, gmb_bf_416 },
    { 417, gmb_bf_417 },
    { 418, gmb_bf_418 },
    { 419, gmb_bf_419 },
    { 420, gmb_bf_420_c420 },
    { 421, gmb_bf_420_c421 },
    { 422, gmb_bf_420_c422 },
    { 423, gmb_bf_420_c423 },
    { 424, gmb_bf_420_c424 },
    { 425, gmb_bf_420_c425 },
    { 426, gmb_bf_420_c426 },
    { 427, gmb_bf_420_c427 },
    { 428, gmb_bf_420_c428 },
    { 429, gmb_bf_420_c429 },
    { 430, gmb_bf_420_c430 },
    { 431, gmb_bf_420_c431 },
    { 433, gmb_bf_433 },
    { 434, gmb_bf_434 },
    { 437, gmb_bf_435 },
    { 438, gmb_bf_435 },
    { 439, gmb_bf_439 },
    { 459, gmb_bf_459 },
    { 476, gmb_bf_476 },
    { 623, gmb_bf_623 },
    { 500, gmb_bf_500 },
    { 501, gmb_bf_500 },
    { 502, gmb_bf_500 },
    { 503, gmb_bf_500 },
    { 504, gmb_bf_500 },
    { 505, gmb_bf_500 },
    { 506, gmb_bf_506 },
    { 507, gmb_bf_507 },
    { 508, gmb_bf_ragcontext_508 },
    { 509, gmb_bf_509 },
    { 510, gmb_bf_510 },
    { 511, gmb_bf_510 },
    { 512, gmb_bf_512 },
    { 565, gmb_bf_565 },
    { 361, gmb_bf_361 },
    { 462, gmb_bf_462 },
    { 463, gmb_bf_462 },
    { 465, gmb_bf_462 },
    { 674, gmb_bf_674 },
    { 675, gmb_bf_675 },
    { 676, gmb_bf_676 },
    { 598, gmb_bf_598 },
    { 599, gmb_bf_599 },
    { 650, gmb_bf_650 },
    { 651, gmb_bf_651 },
    { 652, gmb_bf_652 },
    { 653, gmb_bf_652 },
    { 654, gmb_bf_654 },
    { 649, gmb_bf_649 },
    { 657, gmb_bf_657 },
    { 679, gmb_bf_679 },
    { 555, gmb_bf_555 },
    { 656, gmb_bf_656 },
    { 3818, gmb_bf_3818 },
    { 3819, gmb_bf_3819 },
    { 683, gmb_bf_683 },
    { 684, gmb_bf_684 },
    { 2186, gmb_bf_2115 },
    { 2187, gmb_bf_2116 },
    { 2188, gmb_bf_2117 },
    { 2189, gmb_bf_2118 },
    { 2190, gmb_bf_2190 },
    { 2191, gmb_bf_2191 },
    { 2192, gmb_bf_2192 },
    { 2193, gmb_bf_2193 },
    { 2194, gmb_bf_2194 },
    { 2195, gmb_bf_2195 },
    { 2199, gmb_bf_2199 },
    { 2200, gmb_bf_2200 },
    { 2201, gmb_bf_2201 },
    { 527, gmb_bf_527 },
    { 528, gmb_bf_528 },
    { 546, gmb_bf_546 },
    { 553, gmb_bf_553 },
    { 554, gmb_bf_554 },
    { 547, gmb_bf_547 },
    { 548, gmb_bf_548 },
    { 549, gmb_bf_549 },
    { 550, gmb_bf_550 },
    { 551, gmb_bf_551 },
    { 552, gmb_bf_552 },
    { 566, gmb_bf_566 },
    { 567, gmb_bf_567 },
    { 568, gmb_bf_568 },
    { 569, gmb_bf_569 },
    { 570, gmb_bf_570 },
    { 571, gmb_bf_571 },
    { 682, gmb_bf_682 },
    { 759, gmb_bf_759 },
    { 575, gmb_bf_575 },
    { 576, gmb_bf_575 },
    { 577, gmb_bf_575 },
    { 574, gmb_bf_575 },
    { 582, gmb_bf_575 },
    { 578, gmb_bf_575 },
    { 573, gmb_bf_573 },
    { 579, gmb_bf_579 },
    { 604, gmb_bf_604 },   /* IO.WRITEBYTES */
    { 605, gmb_bf_605 },   /* FONTS.TTRENDER */
    { 606, gmb_bf_606 },   /* FONTS.TTMETRICS */
    { 607, gmb_bf_607 },   /* FONTS.TTGLYPHS */
    { 580, gmb_bf_580 },
    { 581, gmb_bf_581 },
    { 3806, gmb_bf_3806 },
    { 763, gmb_bf_763 },
    { 764, gmb_bf_764 },
    { 765, gmb_bf_765 },
    { 1155, gmb_bf_1155 },
    { 1156, gmb_bf_1156 },
    { 1157, gmb_bf_1157 },
    { 1158, gmb_bf_1158 },
    { 1159, gmb_bf_1159 },
    { 1165, gmb_bf_1165 },
    { 1160, gmb_bf_1160 },
    { 1161, gmb_bf_1161 },
    { 1162, gmb_bf_1162 },
    { 1163, gmb_bf_1163 },
    { 1164, gmb_bf_1164 },
    { 1201, gmb_bf_1201 },
    { 1202, gmb_bf_1202 },
    { 451, gmb_bf_451 },   /* ISAM.LOG */
    { 1203, gmb_bf_1203 },
    { 1204, gmb_bf_1204 },
    { 1205, gmb_bf_1205 },
    { 1206, gmb_bf_1206 },
    { 1207, gmb_bf_1207 },
    { 1208, gmb_bf_1208 },
    { 1209, gmb_bf_1209 },
    { 702, gmb_bf_702_setcredits },
    { 703, gmb_bf_702_credits },
    { 704, gmb_bf_704_c704 },
    { 705, gmb_bf_704_c705 },
    { 706, gmb_bf_704_c706 },
    { 707, gmb_bf_704_c707 },
    { 708, gmb_bf_704_c708 },
    { 709, gmb_bf_704_c709 },
    { 710, gmb_bf_704_c710 },
    { 711, gmb_bf_704_c711 },
    { 712, gmb_bf_704_c712 },
    { 713, gmb_bf_713 },
    { 714, gmb_bf_714_c714 },
    { 715, gmb_bf_714_c715 },
    { 716, gmb_bf_714_c716 },
    { 717, gmb_bf_714_c717 },
    { 718, gmb_bf_714_c718 },
    { 720, gmb_bf_714_c720 },
    { 721, gmb_bf_714_c721 },
    { 722, gmb_bf_714_c722 },
    { 723, gmb_bf_714_c723 },
    { 724, gmb_bf_714_c724 },
    { 725, gmb_bf_725_c725 },
    { 726, gmb_bf_725_c726 },
    { 727, gmb_bf_725_c727 },
    { 728, gmb_bf_725_c728 },
    { 729, gmb_bf_725_c729 },
    { 730, gmb_bf_725_c730 },
    { 731, gmb_bf_725_c731 },
    { 732, gmb_bf_732 },
    { 782, gmb_bf_782_c782 },
    { 783, gmb_bf_782_c783 },
    { 784, gmb_bf_782_c784 },
    { 785, gmb_bf_782_c785 },
    { 786, gmb_bf_782_c786 },
    { 787, gmb_bf_782_c787 },
    { 788, gmb_bf_782_c788 },
    { 789, gmb_bf_782_c789 },
    { 790, gmb_bf_782_c790 },
    { 791, gmb_bf_782_c791 },
    { 792, gmb_bf_782_c792 },
    { 794, gmb_bf_782_c794 },
    { 795, gmb_bf_782_c795 },
    { 796, gmb_bf_796_c796 },
    { 797, gmb_bf_796_c797 },
    { 798, gmb_bf_796_c798 },
    { 799, gmb_bf_796_c799 },
    { 800, gmb_bf_796_c800 },
    { 801, gmb_bf_796_c801 },
    { 802, gmb_bf_796_c802 },
    { 803, gmb_bf_796_c803 },
    { 804, gmb_bf_796_c804 },
    { 805, gmb_bf_796_c805 },
    { 806, gmb_bf_796_c806 },
    { 807, gmb_bf_796_c807 },
    { 808, gmb_bf_796_c808 },
    { 809, gmb_bf_796_c809 },
    { 810, gmb_bf_796_c810 },
    { 811, gmb_bf_796_c811 },
    { 812, gmb_bf_796_c812 },
    { 813, gmb_bf_796_c813 },
    { 814, gmb_bf_796_c814 },
    { 815, gmb_bf_796_c815 },
    { 816, gmb_bf_796_c816 },
    { 817, gmb_bf_796_c817 },
    { 818, gmb_bf_796_c818 },
    { 819, gmb_bf_796_c819 },
    { 820, gmb_bf_796_c820 },
    { 821, gmb_bf_796_c821 },
    { 822, gmb_bf_796_c822 },
    { 825, gmb_bf_796_c825 },
    /* MATRIX F2b (2026-11-09): ROW/COL/ISCONTIGUOUS */
    { 3900, gmb_bf_3900 },
    { 3901, gmb_bf_3901 },
    { 3902, gmb_bf_3902 },
    { 826, gmb_bf_796_c826 },
    { 827, gmb_bf_796_c827 },
    { 828, gmb_bf_796_c828 },
    { 829, gmb_bf_796_c829 },
    { 937, gmb_bf_796_c937 },
    { 740, gmb_bf_740 },
    { 741, gmb_bf_741_c741 },
    { 742, gmb_bf_741_c742 },
    { 743, gmb_bf_741_c743 },
    { 744, gmb_bf_741_c744 },
    { 745, gmb_bf_741_c745 },
    { 746, gmb_bf_741_c746 },
    { 747, gmb_bf_741_c747 },
    { 748, gmb_bf_741_c748 },
    { 749, gmb_bf_741_c749 },
    { 750, gmb_bf_741_c750 },
    { 751, gmb_bf_741_c751 },
    { 754, gmb_bf_741_c754 },
    { 755, gmb_bf_741_c755 },
    { 756, gmb_bf_741_c756 },
    { 757, gmb_bf_741_c757 },
    { 701, gmb_bf_701 },
    { 1213, gmb_bf_1213_c1213 },
    { 1220, gmb_bf_1213_c1220 },
    { 1218, gmb_bf_1218_c1218 },
    { 1219, gmb_bf_1218_c1219 },
    { 1221, gmb_bf_1218_c1221 },
    { 1222, gmb_bf_1218_c1222 },
    { 1223, gmb_bf_1218_c1223 },
    { 1217, gmb_bf_1218_c1217 },
    { 1224, gmb_bf_1218_c1224 },
    { 766, gmb_bf_766 },
    { 1214, gmb_bf_1214_c1214 },
    { 1215, gmb_bf_1214_c1215 },
    { 1216, gmb_bf_1214_c1216 },
    { 1225, gmb_bf_1214_c1225 },
    { 1226, gmb_bf_1214_c1226 },
    { 768, gmb_bf_768_c768 },
    { 769, gmb_bf_768_c769 },
    { 770, gmb_bf_768_c770 },
    { 771, gmb_bf_768_c771 },
    { 830, gmb_bf_768_c830 },
    { 831, gmb_bf_768_c831 },
    { 1227, gmb_bf_1227 },
    { 1265, gmb_bf_1265_c1265 },
    { 1266, gmb_bf_1265_c1266 },
    { 1267, gmb_bf_1265_c1267 },
    { 1327, gmb_bf_provenance },
    { 1328, gmb_bf_provenance },
    { 1337, gmb_bf_share },
    { 1338, gmb_bf_share },
    { 1341, gmb_bf_secondbrain_c1341 },
    { 1342, gmb_bf_secondbrain_c1342 },
    { 1343, gmb_bf_secondbrain_c1343 },
    { 1344, gmb_bf_secondbrain_c1344 },
    { 1345, gmb_bf_secondbrain_c1345 },
    { 1346, gmb_bf_secondbrain_c1346 },
    { 1347, gmb_bf_secondbrain_c1347 },
    { 1348, gmb_bf_secondbrain_c1348 },
    { 1316, gmb_bf_secondbrain_c1316 },
    { 1317, gmb_bf_secondbrain_c1317 },
    { 1288, gmb_bf_secondbrain_c1288 },
    { 1289, gmb_bf_secondbrain_c1289 },
    { 1290, gmb_bf_secondbrain_c1290 },
    { 1293, gmb_bf_secondbrain_c1293 },
    { 3899, gmb_bf_secondbrain_c3899 },
    { 1332, gmb_bf_secondbrain_c1332 },
    { 1334, gmb_bf_secondbrain_c1334 },
    { 1335, gmb_bf_secondbrain_c1335 },
    { 1336, gmb_bf_secondbrain_c1336 },
    { 1324, gmb_bf_secondbrain_c1324 },
    { 1325, gmb_bf_secondbrain_c1325 },
    { 1326, gmb_bf_secondbrain_c1326 },
    { 1329, gmb_bf_secondbrain_c1329 },
    { 1330, gmb_bf_secondbrain_c1330 },
    { 1331, gmb_bf_secondbrain_c1331 },
    { 1320, gmb_bf_graph_build },
    { 1321, gmb_bf_graph_build },
    { 1322, gmb_bf_graph_findpath },
    { 1323, gmb_bf_graph_rdd },
    { 1389, gmb_bf_graph_ops },
    { 1390, gmb_bf_graph_ops },
    { 1391, gmb_bf_graph_ops },
    { 1392, gmb_bf_graph_ops },
    { 1393, gmb_bf_graph_ops },
    { 1401, gmb_bf_shell_c1401 },
    { 1402, gmb_bf_shell_c1402 },
    { 1403, gmb_bf_shell_c1403 },
    { 3614, gmb_bf_sendkey_c3614 },
    { 1363, gmb_bf_ledger_edit_c1363 },
    { 1521, gmb_bf_cua_browser_unavailable },
    { 1522, gmb_bf_cua_browser_unavailable },
    { 1523, gmb_bf_cua_browser_unavailable },
    { 1524, gmb_bf_cua_browser_unavailable },
    { 1525, gmb_bf_cua_browser_unavailable },
    { 1504, gmb_bf_blackboard_write_c1504 },
    { 1505, gmb_bf_blackboard_read_c1505 },
    { 1506, gmb_bf_blackboard_wait_c1506 },
    { 2112, gmb_bf_category_set_c2112 },
    { 2113, gmb_bf_category_fallback_c2113 },
    { 2114, gmb_bf_category_auto_c2114 },
    { 2115, gmb_bf_category_pick_c2115 },
    { 2116, gmb_bf_category_list_c2116 },
    { 2117, gmb_bf_task_define_c2117 },
    { 2118, gmb_bf_task_depends_c2118 },
    { 2119, gmb_bf_task_assign_c2119 },
    { 2120, gmb_bf_task_status_c2120 },
    { 2121, gmb_bf_task_list_c2121 },
    { 2122, gmb_bf_task_run_c2122 },
    { 2123, gmb_bf_plan_define_c2123 },
    { 2124, gmb_bf_plan_add_c2124 },
    { 2125, gmb_bf_plan_approve_c2125 },
    { 2126, gmb_bf_plan_status_c2126 },
    { 2127, gmb_bf_plan_list_c2127 },
    { 2128, gmb_bf_plan_run_c2128 },
    { 2129, gmb_bf_hook_register_c2129 },
    { 2130, gmb_bf_hook_unregister_c2130 },
    { 2131, gmb_bf_hook_list_c2131 },
    { 2132, gmb_bf_hook_events_c2132 },
    { 2133, gmb_bf_hook_fire_c2133 },
    { 1992, gmb_bf_aiverify_c1992 },
    { 2134, gmb_bf_artifact_put_c2134 },
    { 2135, gmb_bf_artifact_link_c2135 },
    { 2136, gmb_bf_artifact_get_c2136 },
    { 2137, gmb_bf_artifact_list_c2137 },
    { 2138, gmb_bf_artifact_remove_c2138 },
    { 2139, gmb_bf_resource_set_c2139 },
    { 2140, gmb_bf_resource_get_c2140 },
    { 2141, gmb_bf_resource_list_c2141 },
    { 2142, gmb_bf_resource_available_c2142 },
    { 2143, gmb_bf_resource_swarmsize_c2143 },
    { 2144, gmb_bf_handoff_c2144 },
    { 2145, gmb_bf_handoff_complete_c2145 },
    { 2146, gmb_bf_handoff_status_c2146 },
    { 2147, gmb_bf_handoff_list_c2147 },
    { 2148, gmb_bf_handoff_clear_c2148 },
    { 2149, gmb_bf_rules_add_c2149 },
    { 2150, gmb_bf_rules_for_c2150 },
    { 2151, gmb_bf_rules_list_c2151 },
    { 2152, gmb_bf_rules_remove_c2152 },
    { 2153, gmb_bf_rules_clear_c2153 },
    { 2154, gmb_bf_goal_set_c2154 },
    { 2155, gmb_bf_goal_get_c2155 },
    { 2156, gmb_bf_goal_status_c2156 },
    { 2157, gmb_bf_goal_tick_c2157 },
    { 2158, gmb_bf_goal_clear_c2158 },
    { 2159, gmb_bf_replay_start_c2159 },
    { 2160, gmb_bf_replay_record_c2160 },
    { 2161, gmb_bf_replay_stop_c2161 },
    { 2162, gmb_bf_replay_get_c2162 },
    { 2163, gmb_bf_replay_list_c2163 },
    { 2164, gmb_bf_replay_clear_c2164 },
    { 2181, gmb_bf_replay_run_c2181 },
    { 3700, gmb_bf_ai_setoption },
    { 3701, gmb_bf_ai_option },
    { 2182, gmb_bf_goal_agent_c2182 },
    { 2003, gmb_bf_c2003 },
    { 2006, gmb_bf_c2006 },
    { 2009, gmb_bf_c2009 },
    { 2012, gmb_bf_c2012 },
    { 2015, gmb_bf_c2015 },
    { 2018, gmb_bf_c2018 },
    { 2021, gmb_bf_c2021 },
    { 2165, gmb_bf_metric_record_c2165 },
    { 2166, gmb_bf_metric_get_c2166 },
    { 2167, gmb_bf_metric_list_c2167 },
    { 2168, gmb_bf_fleet_stats_c2168 },
    { 2169, gmb_bf_metric_reset_c2169 },
    { 2170, gmb_bf_hashline_c2170 },
    { 2171, gmb_bf_hash_edit_c2171 },
    { 2172, gmb_bf_hash_verify_c2172 },
    { 2173, gmb_bf_checkpoint_c2173 },
    { 2174, gmb_bf_resume_c2174 },
    { 2175, gmb_bf_checkpoint_list_c2175 },
    { 2176, gmb_bf_checkpoint_clear_c2176 },
    { 2177, gmb_bf_orch_save_c2177 },
    { 2178, gmb_bf_orch_load_c2178 },
    { 2179, gmb_bf_task_clear_c2179 },
    { 2180, gmb_bf_plan_clear_c2180 },
    { 1272, gmb_bf_vault_create_c1272 },
    { 1273, gmb_bf_vault_index_c1273 },
    { 1274, gmb_bf_vault_info_c1274 },
    { 454, gmb_bf_speed_begingc_c454 }, { 455, gmb_bf_speed_endgc_c455 },
    { 456, gmb_bf_speed_flushdur_c456 }, { 457, gmb_bf_speed_info_c457 },
    { 458, gmb_bf_speed_setdur_c458 },
    { 1275, gmb_bf_vault_folders_c1275 },
    { 1276, gmb_bf_vault_count_c1276 },
    { 1277, gmb_bf_vault_list_c1277 },
    { 1306, gmb_bf_note_get_c1306 },
    { 1307, gmb_bf_note_title_c1307 },
    { 1308, gmb_bf_note_tags_c1308 },
    { 1309, gmb_bf_note_wordcount_c1309 },
    { 1310, gmb_bf_note_exists_c1310 },
    { 1311, gmb_bf_note_folder_c1311 },
    { 1301, gmb_bf_note_create_c1301 },
    { 1302, gmb_bf_note_update_c1302 },
    { 1303, gmb_bf_note_append_c1303 },
    { 1304, gmb_bf_note_delete_c1304 },
    { 1305, gmb_bf_note_move_c1305 },
    { 1312, gmb_bf_links_out_c1312 },
    { 1313, gmb_bf_backlinks_c1313 },
    { 1314, gmb_bf_link_dangling_c1314 },
    { 1315, gmb_bf_link_json_c1315 },
    { 1339, gmb_bf_vault_watch_c1339 },
    { 1340, gmb_bf_vault_policy_c1340 },
    { 1270, gmb_bf_keygen_c1270 },
    { 1400, gmb_bf_verifyraw_c3509 },
    { 1271, gmb_bf_sleep_c1271 },
    { 1318, gmb_bf_memory_rank_c1318 },
    { 1319, gmb_bf_memory_consolidate_c1319 },
    { 1280, gmb_bf_brain_install_c1280 },
    { 1281, gmb_bf_brain_uninstall_c1281 },
    { 1285, gmb_bf_searchmulti_c1285 },
    { 1287, gmb_bf_askmulti_c1287 },
    { 1358, gmb_bf_memmap_open_c1358 },
    { 1359, gmb_bf_memmap_peek_c1359 },
    { 1360, gmb_bf_memmap_readbytes_c1360 },
    { 1510, gmb_bf_airegister_c1510 },
    { 3505, gmb_bf_aiserver_start_c3505 },
    { 3506, gmb_bf_aiserver_stop_c3506 },
    { 3507, gmb_bf_aiserver_status_c3507 },
    { 3508, gmb_bf_aiserver_model_c3508 },
    { 3509, gmb_bf_aiserver_unload_c3509 },
    { 3615, gmb_bf_llmplan_c3615 },
    { 3616, gmb_bf_llmpolicy_c3616 },
    { 3629, gmb_bf_gmbmodule_c3629 },
    { 3617, gmb_bf_llmquant_c3617 },
    { 3618, gmb_bf_llmmerge_c3618 },
    { 3619, gmb_bf_llmprune_c3619 },
    { 3620, gmb_bf_llmextract_c3620 },
    { 3621, gmb_bf_llmprofile_c3621 },
    { 3622, gmb_bf_llmarch_c3622 },
    { 3623, gmb_bf_llmtensor_c3623 },
    { 3761, gmb_bf_llmtokencount_c3761 },
    { 3807, gmb_bf_llmtokens_c3807 },
    { 3808, gmb_bf_llmdetokenize_c3808 },
    { 3809, gmb_bf_llmtensorvalue_c3809 },
    { 3810, gmb_bf_llmpolicyout_c3810 },
    { 3811, gmb_bf_llmarchout_c3811 },
    { 3812, gmb_bf_llmmoduleout_c3812 },
    { 3813, gmb_bf_llmkernelout_c3813 },
    { 3814, gmb_bf_llmkvout_c3814 },
    { 3627, gmb_bf_llmcompile_c3627 },
    { 3628, gmb_bf_llmquality_c3628 },
    { 3624, gmb_bf_llmkernel_c3624 },
    { 3625, gmb_bf_llmmodule_c3625 },
    { 3626, gmb_bf_llmkv_c3626 },
    { 3527, gmb_bf_aimodelconvert_c3527 },
    { 3542, gmb_bf_aimodelquantize_c3542 },
    { 3543, gmb_bf_aimodelinspect_c3543 },
    { 3702, gmb_bf_aimodelsave_c3702 },
    { 3703, gmb_bf_aimodelablate_c3703 },
    { 3712, gmb_bf_aimodelremove_c3712 },
    { 3704, gmb_bf_aimodeldiff_c3704 },
    { 3705, gmb_bf_aimodelcheckpoint_c3705 },
    { 3706, gmb_bf_aimodelrollback_c3706 },
    { 3707, gmb_bf_aimodeladd_c3707 },
    { 3708, gmb_bf_aimodeladapter_c3708 },
    { 3709, gmb_bf_aimodelmodule_c3709 },
    { 3710, gmb_bf_aimodelmodality_c3710 },
    { 3711, gmb_bf_aimodelvalidate_c3711 },
    { 3714, gmb_bf_ai_listoptions },
    { 3593, gmb_bf_vectordb_advanced },
    { 3544, gmb_bf_vectordb_advanced },
    { 3545, gmb_bf_vectordb_advanced },
    { 3546, gmb_bf_vectordb_advanced },
    { 3547, gmb_bf_vectordb_advanced },
    { 3548, gmb_bf_vectordb_advanced },
    { 3549, gmb_bf_vectordb_advanced },
    { 3550, gmb_bf_vectordb_advanced },
    { 3551, gmb_bf_vectordb_advanced },
    { 1993, gmb_bf_1993 },
    { 2028, gmb_bf_2028 },
    { 2030, gmb_bf_2030 },
    { 2215, gmb_bf_2215 },
    { 2216, gmb_bf_2216 },
    { 2033, gmb_bf_2033 },
    { 2206, gmb_bf_2034 },
    { 2208, gmb_bf_2036 },
    { 2210, gmb_bf_2210 },
    { 2213, gmb_bf_2213 },
    { 1994, gmb_bf_1994 },
    { 1995, gmb_bf_1995 },
    { 1996, gmb_bf_1996 },
    { 1997, gmb_bf_1997 },
    { 1998, gmb_bf_1998 },
    { 1999, gmb_bf_1999 },
    { 2000, gmb_bf_2000 },
    { 2202, gmb_bf_2202 },
    { 2203, gmb_bf_2203 },
    { 2204, gmb_bf_2204 },
    { 2205, gmb_bf_2205 },
    { 2300, gmb_bf_2300 },
    { 2301, gmb_bf_2301 },
    { 2302, gmb_bf_2302 },
    { 2303, gmb_bf_2303 },
    { 2304, gmb_bf_2304 },
    { 2305, gmb_bf_2305 },
    { 2306, gmb_bf_2306 },
    { 2307, gmb_bf_2307 },
    { 2308, gmb_bf_2308 },
    { 2314, gmb_bf_2314 },
    { 2309, gmb_bf_2309 },
    { 2310, gmb_bf_2310 },
    { 2311, gmb_bf_2311 },
    { 2312, gmb_bf_2312 },
    { 2313, gmb_bf_2313 },
    { 3552, gmb_bf_context },
    { 3555, gmb_bf_context },
    { 3558, gmb_bf_context },
    { 3566, gmb_bf_context },
    { 3735, gmb_bf_context },
    { 3738, gmb_bf_context },
    { 3741, gmb_bf_context },
    { 3559, gmb_bf_worker },
    { 3562, gmb_bf_worker },
    { 3565, gmb_bf_worker },
    { 3568, gmb_bf_worker },
    { 3592, gmb_bf_worker },
    { 3720, gmb_bf_tray },
    { 3723, gmb_bf_tray },
    { 3726, gmb_bf_tray },
    { 1170, gmb_bf_1170 },
    { 1171, gmb_bf_1171 },
    { 1172, gmb_bf_1172 },
    { 1173, gmb_bf_1173 },
    { 1174, gmb_bf_1174 },
    { 432, vmc_mcp_bf_register_tool },
    { 435, vmc_mcp_bf_serve },
    { 436, vmc_mcp_bf_stop },
    { 2004, gmb_bf_c2004 },
    { 2005, gmb_bf_c2005 },
    { 2007, gmb_bf_c2007 },
    { 2008, gmb_bf_c2008 },
    { 2010, gmb_bf_c2010 },
    { 2011, gmb_bf_c2011 },
    { 2013, gmb_bf_c2013 },
    { 2014, gmb_bf_c2014 },
    { 2016, gmb_bf_c2016 },
    { 2017, gmb_bf_c2017 },
    { 2019, gmb_bf_c2019 },
    { 2020, gmb_bf_c2020 },
    { 2022, gmb_bf_c2022 },
    { 2023, gmb_bf_c2023 },
    { 3538, vmc_bf_uncertainty },
    { 3539, vmc_bf_uncertainty },
    { 3540, vmc_bf_uncertainty },
    { 2029, gmb_bf_2029 },
    { 2031, gmb_bf_2031 },
    { 2032, gmb_bf_2032 },
    { 2207, gmb_bf_2035 },
    { 2209, gmb_bf_2037 },
    { 2211, gmb_bf_2211 },
    { 2212, gmb_bf_2212 },
    { 2214, gmb_bf_2214 },
    { 3553, gmb_bf_context },
    { 3554, gmb_bf_context },
    { 3556, gmb_bf_context },
    { 3557, gmb_bf_context },
    { 3567, gmb_bf_context },
    { 3736, gmb_bf_context },
    { 3737, gmb_bf_context },
    { 3739, gmb_bf_context },
    { 3740, gmb_bf_context },
    { 3742, gmb_bf_context },
    { 3743, gmb_bf_context },
    { 3560, gmb_bf_worker },
    { 3561, gmb_bf_worker },
    { 3563, gmb_bf_worker },
    { 3564, gmb_bf_worker },
    { 3569, gmb_bf_worker },
    { 3570, gmb_bf_worker },
    { 3721, gmb_bf_tray },
    { 3722, gmb_bf_tray },
    { 3724, gmb_bf_tray },
    { 3725, gmb_bf_tray },
    { 3727, gmb_bf_tray },
    { 3728, gmb_bf_tray },
    { 3729, gmb_bf_tray },

};
#else
/* Tabella specializzata per chunk: generata da tools (solo handler usati).
 * Qui il chunk non usa builtin (PRINT nativo + FFI dinamica) => nessun handler. */
static const gmb_bf_impl_t gmb_bf_impl_core[] = { { -1, 0 } };
#endif


/* Rilascia gli argomenti (riferimenti posseduti estratti dallo stack) e li azzera. */
static void gmb_builtin_release_args(gmb_stack_entry_t* args, int argc)
{
    for (int i = 0; i < argc && i < 16; i++)
        if (args[i].type == 2 && args[i].ref) { gmb_obj_release(args[i].ref); args[i].ref = NULL; args[i].type = 0; }
}

/* Registro dei moduli di dominio (vedi gmb_builtin_registry.h). */
#define GMB_BF_REG_MAX 16
static const gmb_bf_impl_t* g_bf_reg_t[GMB_BF_REG_MAX];
static size_t g_bf_reg_n[GMB_BF_REG_MAX];
static int g_bf_reg_count;
void gmb_builtin_register(const gmb_bf_impl_t* table, size_t count)
{
    if (g_bf_reg_count >= GMB_BF_REG_MAX) return;
    g_bf_reg_t[g_bf_reg_count] = table;
    g_bf_reg_n[g_bf_reg_count] = count;
    g_bf_reg_count++;
}

static const gmb_bf_impl_t* gmb_bf_find(int code)
{
    for (size_t i = 0; i < sizeof(gmb_bf_impl_core) / sizeof(gmb_bf_impl_core[0]); i++)
        if (gmb_bf_impl_core[i].code == code && gmb_bf_impl_core[i].handler) return &gmb_bf_impl_core[i];
    for (int r = 0; r < g_bf_reg_count; r++)
        for (size_t i = 0; i < g_bf_reg_n[r]; i++)
            if (g_bf_reg_t[r][i].code == code) return &g_bf_reg_t[r][i];
    return NULL;
}

void gmb_builtin_dispatch(gmb_vm_t* vm, gmb_task_t* task, int func_code, int argc)
{
    gmb_stack_entry_t args[16];
    for (int i = 0; i < argc && i < 16; i++)
        args[argc - 1 - i] = gmb_task_pop(task);
    const gmb_bf_impl_t* impl = gmb_bf_find(func_code);
    if (impl)
    {
            gmb_builtin_out_t out = { 0.0, 1, NULL, GMB_BT_OBJECT };
            gmb_state_hook_clear_error();   /* nessun errore "stantio" da una chiamata precedente */
            impl->handler(vm, args, argc, func_code, &out, task);
            /* Se il runtime OSPITE (hook di stato) ha segnalato un'eccezione managed, la si
             * solleva come errore BASIC: stessa semantica di vm_error, così l'ON ERROR/TRY
             * della pagina si comporta come sulla gamba managed (mai fallimento silenzioso). */
            if (task->pending_exception == NULL)
            {
                char* hook_err = gmb_state_hook_take_error();
                if (hook_err && *hook_err)
                {
                    if (out.result_ref) { gmb_obj_release(out.result_ref); out.result_ref = NULL; }
                    gmb_builtin_release_args(args, argc);
                    gmb_raise_basic_error(task, 200.0, hook_err);
                    free(hook_err);
                    return;
                }
                free(hook_err);
            }
            if (task->state == GMB_TS_BLOCKED) return;
            if (task->pending_exception != NULL)
            {
                /* Il builtin ha segnalato un errore (vm_error): instradalo nello schema a 3
                 * livelli (TRY/CATCH -> ON ERROR GOTO/GOSUB -> fatale) e NON pushare il valore
                 * di default. Prima l'errore era sempre fatale (ON ERROR non lo catturava) e il
                 * dispatch spingeva comunque 0/nil. Parità con PCodeVM RaiseBasicError / IL. */
                char* emsg = task->pending_exception;
                task->pending_exception = NULL;
                task->state = GMB_TS_RUNNING;
                gmb_raise_basic_error(task, 200.0, emsg);
                free(emsg);
                if (out.result_ref) gmb_obj_release(out.result_ref);   /* risultato posseduto mai pushato */
                gmb_builtin_release_args(args, argc);
                return;
            }
            /* CONTRATTO (ownership): `result_ref` e' un riferimento POSSEDUTO (+1 del creatore o retain del
             * handler se restituisce un oggetto preso in prestito) → lo stack lo PRENDE (push_ref_new, nessun
             * retain). Gli ARGOMENTI estratti dallo stack sono posseduti dal dispatch → rilasciati qui a fine
             * chiamata (gli handler che li consumano da soli li azzerano: ref = NULL). Prima nessuno dei due
             * veniva rilasciato: ogni risultato-oggetto e ogni argomento-oggetto di ogni builtin restava a
             * refcount +1 per sempre (leak di ogni stringa/dict/lista passata o restituita). */
            if (out.is_num == 1) push_num_entry(task, out.result, out.num_bt ? out.num_bt : GMB_BT_NUMBER);
            else if (out.result_ref) push_ref_new(task, out.result_ref, out.result_ref_bt);
            else push_nil(task);
            gmb_builtin_release_args(args, argc);
            return;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "builtin %d non implementato", func_code);
    vm_error(task, msg);
}
