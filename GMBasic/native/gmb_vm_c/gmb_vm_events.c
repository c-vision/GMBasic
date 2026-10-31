/* ============================================================================
 * gmb_vm_events.c — Registry eventi runtime (ON KEY) e dispatch alla lettura tasti.
 * File: native/gmb_vm_c/gmb_vm_events.c
 *
 * Parità col canone PCode/IL (PCodeProcessor.Helpers.FireKeyEvent + IlRuntime
 * TryDequeueInkey::DispatchKeyEvent): OP_ON_KEY registra (keycode -> addr handler);
 * alla LETTURA di un tasto (INKEY, e in futuro WAITKEY/INPUT/GET_KEY) l'handler
 * viene eseguito spawando un task indipendente all'indirizzo (stesso schema OP_GO,
 * senza wg né args — il figlio eredita i globali del task che legge).
 *
 * Limitazione dichiarata: registry statico per-processo (una VM per host) —
 * come il singleton ClipperLib.App.EventDispatcher del backend C#.
 * ============================================================================ */

#include "gmb_vm_events.h"

#define GMB_ONKEY_CAP 64

typedef struct {
    int keycode;      /* codice tasto (byte/ConsoleKey) */
    int target_addr;  /* indirizzo entry handler (GOSUB/SUB) nel chunk */
    int used;
} gmb_onkey_entry_t;

static gmb_onkey_entry_t g_onkey[GMB_ONKEY_CAP];
static int g_onkey_count = 0;

void gmb_vm_onkey_register(int keycode, int action, int target_addr)
{
    if (keycode <= 0)
        return; /* ON KEY STOP "tutti i tasti" (0): nessun target specifico da rimuovere */
    /* STOP (action==0): rimuove gli handler per quel tasto. */
    for (int i = 0; i < g_onkey_count; i++)
    {
        if (g_onkey[i].used && g_onkey[i].keycode == keycode)
        {
            if (action == 0)
            {
                g_onkey[i] = (gmb_onkey_entry_t){0};
                return;
            }
            /* re-registrazione: sostituisce il target */
            g_onkey[i].target_addr = target_addr;
            return;
        }
    }
    if (action == 0 || target_addr < 0)
        return; /* STOP su tasto non registrato: no-op */
    if (g_onkey_count >= GMB_ONKEY_CAP)
        return; /* registry pieno: come il dispatcher C#, l'ultima registrazione vince
                   a parità di slot; CAP 64 è ampio per i programmi reali */
    g_onkey[g_onkey_count++] = (gmb_onkey_entry_t){ keycode, target_addr, 1 };
}

/* Fire alla lettura di un tasto (byte letto). Specchio FireKeyEvent: spawna un task
 * indipendente all'indirizzo handler con i globali del task che legge. */
void gmb_vm_onkey_fire(gmb_vm_t* vm, gmb_task_t* task, int keycode)
{
    if (vm == NULL || task == NULL || vm->scheduler == NULL || keycode <= 0)
        return;
    for (int i = 0; i < g_onkey_count; i++)
    {
        if (g_onkey[i].used && g_onkey[i].keycode == keycode)
        {
            gmb_task_t* go = gmb_task_create(vm->scheduler, g_onkey[i].target_addr);
            if (go == NULL)
                return;
            go->vm_ctx = vm;
            go->globals_nums = task->globals_nums;
            go->globals_refs = task->globals_refs;
            go->globals_types = task->globals_types;
            go->globals_basic = task->globals_basic;
            go->globals_count = task->globals_count;
            go->cancellation_requested = task->cancellation_requested;
            /* Nessun wg/args: l'handler è una SUB senza parametri (canone ON KEY GOSUB). */
            gmb_sched_enqueue(vm->scheduler, go);
            return;
        }
    }
}


/* ---- Eventi controllo (parità PCode EventDispatcher.RegisterControlEvent + FireWindowEvent) ---- */
#define GMB_CTRL_CAP 128
typedef struct { int handle; int ev; int target_addr; int used; void* inst; int has_inst; char method[64]; } gmb_ctrl_entry_t;
static gmb_ctrl_entry_t g_ctrl[GMB_CTRL_CAP];
static int g_ctrl_count = 0;

void gmb_vm_control_register(int handle, int ev, int action, int target_addr)
{
    if (ev <= 0) return;
    for (int i = 0; i < g_ctrl_count; i++)
        if (g_ctrl[i].used && g_ctrl[i].handle == handle && g_ctrl[i].ev == ev)
        {
            if (action == 0) { g_ctrl[i].used = 0; return; }
            g_ctrl[i].target_addr = target_addr; g_ctrl[i].has_inst = 0; return;
        }
    if (action == 0 || target_addr < 0) return;
    if (g_ctrl_count >= GMB_CTRL_CAP) return;
    g_ctrl[g_ctrl_count++] = (gmb_ctrl_entry_t){ handle, ev, target_addr, 1, NULL, 0, {0} };
}

/* §11.7-3: registra un metodo-evento di una classe UserControl (metodo per NOME, istanza = SELF).
 * L'evento esegue il metodo-istanza nel task GPM corrente (run-to-completion), mai inline su thread
 * esterno .NET. */
void gmb_vm_control_register_inst(int handle, int ev, const char* method, void* inst)
{
    if (ev <= 0 || !method || !*method) return;
    for (int i = 0; i < g_ctrl_count; i++)
        if (g_ctrl[i].used && g_ctrl[i].handle == handle && g_ctrl[i].ev == ev)
        {
            g_ctrl[i].inst = inst; g_ctrl[i].has_inst = 1;
            snprintf(g_ctrl[i].method, sizeof(g_ctrl[i].method), "%s", method);
            return;
        }
    if (g_ctrl_count >= GMB_CTRL_CAP) return;
    g_ctrl[g_ctrl_count] = (gmb_ctrl_entry_t){ handle, ev, -1, 1, inst, 1, {0} };
    snprintf(g_ctrl[g_ctrl_count].method, sizeof(g_ctrl[g_ctrl_count].method), "%s", method);
    g_ctrl_count++;
}

/* Ritorna 1 e compila inst/method se l'entry per (handle,ev) è un metodo-istanza UserControl. */
int gmb_vm_control_inst_of(int handle, int ev, void** out_inst, char* out_method, int cap)
{
    for (int i = 0; i < g_ctrl_count; i++)
        if (g_ctrl[i].used && g_ctrl[i].handle == handle && g_ctrl[i].ev == ev && g_ctrl[i].has_inst)
        {
            if (out_inst) *out_inst = g_ctrl[i].inst;
            if (out_method && cap > 0) snprintf(out_method, (size_t)cap, "%s", g_ctrl[i].method);
            return 1;
        }
    return 0;
}

int gmb_vm_control_target(int handle, int ev)
{
    for (int i = 0; i < g_ctrl_count; i++)
        if (g_ctrl[i].used && g_ctrl[i].handle == handle && g_ctrl[i].ev == ev)
            return g_ctrl[i].target_addr;
    return -1;
}
void gmb_vm_control_fire(gmb_vm_t* vm, gmb_task_t* task, int handle, int ev)
{
    if (vm == NULL || task == NULL || vm->scheduler == NULL || ev <= 0) return;
    for (int i = 0; i < g_ctrl_count; i++)
        if (g_ctrl[i].used && g_ctrl[i].handle == handle && g_ctrl[i].ev == ev)
        {
            gmb_task_t* go = gmb_task_create(vm->scheduler, g_ctrl[i].target_addr);
            if (go == NULL) return;
            go->vm_ctx = vm;
            go->globals_nums = task->globals_nums;
            go->globals_refs = task->globals_refs;
            go->globals_types = task->globals_types;
            go->globals_basic = task->globals_basic;
            go->globals_count = task->globals_count;
            go->cancellation_requested = task->cancellation_requested;
            gmb_sched_enqueue(vm->scheduler, go);
            return;
        }
}