/* gmb_vm_console.c — console, istruzioni WINDOW e READ della VM/C.
 * Estratto da gmb_vm_tui.c (§0quater): contiene `gmb_vm_exec_console_syscall`.
 */
#include "gmb_vm_tui.h"
#include "gmb_syscall_codes.h"   /* GMB_SC_<NOME> = valore dell'enum SysCall (generato) */
#include "gmb_form.h"
#include "gmb_vm_events.h"
#include "gmb_gpm.h"
#include "gmb_vm_value.h"
#include "gmb_tui_widgets.h"
#include "gmb_tui_windowstmt.h"
#include "gmb_tui_app.h"
#include "gmb_read.h"
#include "gmb_dscope.h"
#include "gmb_adapter.h"
#include "gmb_tui_io.h"
#include "gmb_tui_tbrowse.h"
#include "gmb_graphics.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "gmb_vm_tui_internal.h"

/* console/Clipper syscall per-code -> tabella {code, handler} (refactor §18.5). */
typedef struct { int code; void (*fn)(gmb_vm_t*, gmb_task_t*, int, int); } cons_sys_row;

static void vm_console_4(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code;
const char* prompt = argc > 0 && task->sp > 0 ? tui_pop_str(task) : "";
int nVars = argc; /* argc = numero variabili */
if (prompt && *prompt) { fputs(prompt, stdout); fflush(stdout); }
gmb_string_t line = gmb_console_readline();
if (nVars <= 1)
{
    tui_push_str(task, line.data ? line.data : "");
}
else
{
    /* splitta per virgola; campi mancanti → "" */
    const char* p = line.data ? line.data : "";
    for (int i = 0; i < nVars; i++)
    {
        char field[512];
        const char* c = strchr(p, ',');
        if (c) { size_t len = (size_t)(c - p); if (len >= sizeof(field)) len = sizeof(field) - 1; memcpy(field, p, len); field[len] = '\0'; p = c + 1; }
        else { snprintf(field, sizeof(field), "%s", p); p = p + strlen(p); }
        tui_push_str(task, field);
    }
}

}
static void vm_console_5(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code;
const char* prompt = argc > 0 && task->sp > 0 ? tui_pop_str(task) : "";
if (prompt && *prompt) { fputs(prompt, stdout); fflush(stdout); }
gmb_string_t line = gmb_console_readline();
tui_push_str(task, line.data ? line.data : "");

}
static void vm_console_6(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code;
int timeout_ms = 0;
if (argc > 0 && task->sp > 0) timeout_ms = (int)tui_pop_num(task);
if (timeout_ms <= 0) {
    /* INKEY senza timeout: poll NON bloccante (semantica INKEY$="" se nessun tasto). */
    gmb_string_t s = gmb_console_inkey();
    /* Dispatch eventi ON KEY alla lettura (parita' IL/PCode DispatchKeyEvent). */
    if (vm && task && s.data && s.data[0])
        gmb_vm_onkey_fire(vm, task, (unsigned char)s.data[0]);
    tui_push_str(task, s.data ? s.data : "");
    gmb_string_free(&s);
    return;
}
/* INKEY con timeout: attesa LIMITATA — prova non bloccante; se non c'è tasto ancora
 * parcheggia il task (timer di retry) invece di bloccare il worker GPM, e al resume
 * ri-esegue il syscall (il timeout residuo vive in task->sleep_until_ms). */
gmb_string_t s = {0};
int r = gmb_console_readkey_nb(&s);
if (r > 0 && s.data) {
    /* Dispatch eventi ON KEY alla lettura (parita' IL/PCode DispatchKeyEvent). */
    if (vm && task && s.data[0])
        gmb_vm_onkey_fire(vm, task, (unsigned char)s.data[0]);
    tui_push_str(task, s.data); gmb_string_free(&s); return;
}
if (r < 0) { tui_push_str(task, ""); gmb_string_free(&s); return; }
gmb_string_free(&s);
/* nessun tasto pronto su terminale reale → park con ri-esecuzione del syscall al wake */
gmb_task_push_num(task, (double)timeout_ms, GMB_BT_NUMBER); /* respingi il timeout */
task->ip -= 4; /* ri-esegue OP_SYSCALL (1 opcode + 2 code + 1 argc) al resume */
pthread_mutex_lock(&task->lock);
task->sleep_until_ms = gmb_now_ms() + 20;
task->state = GMB_TS_BLOCKED;
pthread_mutex_unlock(&task->lock);
gmb_sched_arm_timer(vm->scheduler, task);

}
static void vm_console_7(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* WAITKEY / GET_KEY (8/7): prova non bloccante; se il tasto non è pronto su terminale
 * reale parcheggia il task (timer retry 20ms) e ri-esegue il syscall al wake — il worker
 * NON si blocca (GPM-safe, parità IL v2 WaitKeyStep). Se non c'è terminale (CI/redirect)
 * ritorna "" finale (mai hang). */
gmb_string_t s = {0};
int r = gmb_console_readkey_nb(&s);
if (r > 0 && s.data) {
    /* Dispatch eventi ON KEY alla lettura (parita' IL/PCode DispatchKeyEvent). */
    if (vm && task && s.data[0])
        gmb_vm_onkey_fire(vm, task, (unsigned char)s.data[0]);
    tui_push_str(task, s.data); gmb_string_free(&s); return;
}
if (r < 0) { tui_push_str(task, ""); gmb_string_free(&s); return; }
gmb_string_free(&s);
task->ip -= 4; /* ri-esegue OP_SYSCALL al resume */
pthread_mutex_lock(&task->lock);
task->sleep_until_ms = gmb_now_ms() + 20;
task->state = GMB_TS_BLOCKED;
pthread_mutex_unlock(&task->lock);
gmb_sched_arm_timer(vm->scheduler, task);

}
static void vm_console_11(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}
gmb_cls();

}
/* CLOCATE row, col [, fg [, bg]] (SysCall 10, parity PCode): posiziona il cursore
 * (1-based) e imposta i colori se >= 0. Layout stack: row, col, fg, bg (argc 4).
 * Prima: code assente → consume_all silenzioso. */
static void vm_console_10(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code;
    double bg = 0, fg = 0, col = 0, row = 0;
    if (argc > 0 && task->sp > 0) bg = tui_pop_num(task);
    if (argc > 1 && task->sp > 0) fg = tui_pop_num(task);
    if (argc > 2 && task->sp > 0) col = tui_pop_num(task);
    if (argc > 3 && task->sp > 0) row = tui_pop_num(task);
    for (int i = 4; i < argc && task->sp > 0; i++)
    {
        gmb_stack_entry_t a = gmb_task_pop(task);
        gmb_obj_release(a.ref);
    }
    if (row >= 0 && col >= 0) gmb_locate((int)row, (int)col);
    if ((int)fg >= 0 || (int)bg >= 0) gmb_set_color((int)fg, (int)bg);
}
/* CPRINT [row, col,] text [, fg [, bg]] (SysCall 9, parity PCode): scrive il testo
 * (posizionando il cursore se row/col >= 0). Layout stack: row, col, text, fg, bg.
 * Prima: code assente → consume_all silenzioso. */
static void vm_console_9(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code;
    double bg = 0, fg = 0, col = 0, row = 0;
    char text[8192] = "";
    if (argc > 0 && task->sp > 0) bg = tui_pop_num(task);
    if (argc > 1 && task->sp > 0) fg = tui_pop_num(task);
    if (argc > 2 && task->sp > 0)
    {
        gmb_stack_entry_t te = gmb_task_pop(task);
        if (te.type == 2 && te.ref && te.ref->kind == GMB_OBJ_STRING)
        {
            gmb_obj_string_t* os = (gmb_obj_string_t*)te.ref;
            size_t n = os->length < sizeof(text) - 1 ? os->length : sizeof(text) - 1;
            memcpy(text, os->data, n);
            text[n] = '\0';
        }
        else if (te.type == 1)
        {
            snprintf(text, sizeof(text), "%g", te.num);
        }
        gmb_obj_release(te.ref);
    }
    if (argc > 3 && task->sp > 0) col = tui_pop_num(task);
    if (argc > 4 && task->sp > 0) row = tui_pop_num(task);
    for (int i = 5; i < argc && task->sp > 0; i++)
    {
        gmb_stack_entry_t a = gmb_task_pop(task);
        gmb_obj_release(a.ref);
    }
    if (row >= 0 && col >= 0) gmb_locate((int)row, (int)col);
    if ((int)fg >= 0 || (int)bg >= 0) gmb_set_color((int)fg, (int)bg);
    fputs(text, stdout);
    fflush(stdout);
}
static void vm_console_13(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
int on = 1;
if (argc > 0 && task->sp > 0) on = tui_pop_num(task) != 0;
gmb_console_cursor(on);

}
static void vm_console_14(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
const char* shape = argc > 0 && task->sp > 0 ? tui_pop_str(task) : "";
gmb_console_cursor_shape(shape);

}
static void vm_console_268(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
const char* pic = argc > 0 && task->sp > 0 ? tui_pop_str(task) : "";
const char* text = argc > 1 && task->sp > 0 ? tui_pop_str(task) : "";
int col = argc > 2 && task->sp > 0 ? (int)tui_pop_num(task) : 1;
int row = argc > 3 && task->sp > 0 ? (int)tui_pop_num(task) : 1;
(void)pic;
if (text && *text)
{
    printf("\033[%d;%dH%s", row, col, text);
    fflush(stdout);
}

}
static void vm_console_269(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* 16 argomenti, ultimo in cima allo stack = max. Consuma tutti. */
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}

}
static void vm_console_290(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
double sub = 0;
double vnum = 0;
const char* sval = "";
if (argc > 0 && task->sp > 0) sub = tui_pop_num(task);
/* I subCode booleani passano value come NUMERO (ON=1/OFF=0): 0-3, 6-7, 10, 12-13, 21.
 * I subCode a stringa (THEME=4, DATEFORMAT=5) passano value come stringa. */
if (argc > 1 && task->sp > 0)
{
    if (sub == 4 || sub == 5 || sub == 8 || sub == 9 || sub == 11 || sub == 14 || sub == 15 || sub == 16)
        sval = tui_pop_str(task);
    else
        vnum = tui_pop_num(task);
}
gmb_console_set_clipper((int)sub, vnum, sval);

}
static void vm_console_296(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}

}
/* SPRITEDEF: carattere → 0xRRGGBB (0 = trasparente), mappatura canonica PCodeVM. */
static int spritedef_packed(int ch) {
    if (ch == '.' || ch == ' ') return 0;
    if (ch >= '0' && ch <= '9') { int v = (ch - '0') * 16 + 15; return (v << 16) | (v << 8) | v; }
    { int idx = toupper(ch) - 'A' + 10; if (idx < 0) idx = 0;
      return ((idx * 8 + 127) << 16) | ((idx * 4 + 63) << 8) | (idx * 2 + 31); }
}
/* 293 = SPRITE_DEF: id in cima (push per ultimo), poi le righe in ordine di push. */
static void vm_console_293(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code;
    double id = 0;
    int rows = argc - 1, y, x;
    if (argc > 0 && task->sp > 0) id = tui_pop_num(task);
    if (rows <= 0) return;
    gmb_stack_entry_t* entries = (gmb_stack_entry_t*)calloc((size_t)rows, sizeof(gmb_stack_entry_t));
    if (!entries) return;
    for (int i = rows - 1; i >= 0; i--) entries[i] = gmb_task_pop(task);
    {
        const char* r0 = (entries[0].type == 2 && entries[0].ref && entries[0].ref->kind == GMB_OBJ_STRING)
            ? ((gmb_obj_string_t*)entries[0].ref)->data : "";
        int w = (int)strlen(r0);
        if (w > 0) {
            int* px = (int*)malloc((size_t)w * (size_t)rows * sizeof(int));
            if (px) {
                for (y = 0; y < rows; y++) {
                    const char* r = (entries[y].type == 2 && entries[y].ref && entries[y].ref->kind == GMB_OBJ_STRING)
                        ? ((gmb_obj_string_t*)entries[y].ref)->data : "";
                    int L = (int)strlen(r);
                    for (x = 0; x < w; x++) {
                        int ch = x < L ? (unsigned char)r[x] : '.';
                        px[(size_t)y * (size_t)w + (size_t)x] = spritedef_packed(ch);
                    }
                }
                gmb_gfx_sprite_def((int)id, w, rows, px);
                free(px);
            }
        }
    }
    for (int i = 0; i < rows; i++) gmb_obj_release(entries[i].ref);
    free(entries);
}
/* WINDOW_DEFINE (303): push title, top, left, bottom, right, pixel (C#
 * Compiler.Statements.VisitDefineWindowStatement) → pop inverso. Modello
 * gmb_winstmt_* (registry + stato + render), nome = titolo come il C# (FindByTitle). */
static void vm_winstmt_303(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
double pixel = task->sp > 0 ? tui_pop_num(task) : 0;
double right = task->sp > 0 ? tui_pop_num(task) : 0;
double bottom = task->sp > 0 ? tui_pop_num(task) : 0;
double left = task->sp > 0 ? tui_pop_num(task) : 0;
double top = task->sp > 0 ? tui_pop_num(task) : 0;
char title[128] = "";
if (task->sp > 0) snprintf(title, sizeof(title), "%s", tui_pop_str(task));
/* Contesto per i thunk di ON WINDOW: DEFINE innalza OPEN → il thunk deve poter spawnare
 * il task dell'handler (altrimenti l'evento OPEN si perde sulla VM/C). */
gmb_vm_tui_event_ctx(vm, task);
gmb_winstmt_define(title, title, (int)top, (int)left, (int)bottom, (int)right,
                   pixel != 0 ? GMB_WINSTMT_PIXEL : 0);
gmb_vm_tui_event_ctx(NULL, NULL);

}
static void vm_console_210(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
double mode = 0, active = 0, visible = 0;
if (argc > 0 && task->sp > 0) visible = tui_pop_num(task);
if (argc > 1 && task->sp > 0) active = tui_pop_num(task);
if (argc > 2 && task->sp > 0) mode = tui_pop_num(task);
/* Nessun video hardware: il runtime C emette ANSI. gmb_gfx_screen è
 * già un no-op sicuro quando il backend grafico non è attivo. */
gmb_gfx_screen((int)mode, 0, 0, 0);
(void)active;

}
static void vm_console_219(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}

}
static void vm_console_226(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}
gmb_console_save_screen();

}
static void vm_console_227(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}
gmb_console_restore_screen();

}
/* WINDOW_ACTION (302): push action(num), name → pop name, action.
 * WindowAction { Activate, Deactivate, Release, Close, Minimize, Restore }. */
static void vm_winstmt_302(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
int action = task->sp > 0 ? (int)tui_pop_num(task) : 0;
/* WINDOW SHOW(6)/HIDE(7): form globale (nome vuoto) = visibilita' dello screen. */
if (action == 6 || action == 7) { gmb_winstmt_show(action == 6 ? 1 : 0); return; }
static const char* acts[] = { "Activate", "Deactivate", "Release", "Close", "Minimize", "Restore" };
/* Contesto per i thunk di ON WINDOW: questa azione può innalzare FOCUS/CLOSE/MINIMIZE/… */
gmb_vm_tui_event_ctx(vm, task);
gmb_winstmt_action(name, (action >= 0 && action < 6) ? acts[action] : "Activate");
gmb_vm_tui_event_ctx(NULL, NULL);

}
/* WINDOW_APPEND (304): push text, name → pop name, text. */
static void vm_winstmt_304(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
char text[512] = "";
if (task->sp > 0) snprintf(text, sizeof(text), "%s", tui_pop_str(task));
gmb_winstmt_append(name, text);

}
/* WINDOW_SET_CONTENT (305): push lines(list), name → pop name, lista. */
static void vm_winstmt_305(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
static char lines[64][256];
int n = tui_pop_str_list(task, lines, 64);
gmb_winstmt_clear(name);
for (int i = 0; i < n; i++) gmb_winstmt_append(name, lines[i]);

}
/* WINDOW_CLEAR (306): push name → pop name. */
static void vm_winstmt_306(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
gmb_winstmt_clear(name);

}
/* WINDOW_SCROLL (307): push delta, name → pop name, delta. */
static void vm_winstmt_307(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
int delta = task->sp > 0 ? (int)tui_pop_num(task) : 0;
gmb_winstmt_scroll(name, delta);

}
/* WINDOW_EDIT (308): push name, enable → pop enable, name (allineato al case PCode). */
static void vm_winstmt_308(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
int on = task->sp > 0 ? (int)tui_pop_num(task) : 0;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
gmb_winstmt_edit(name, on);

}
/* WINDOW_SELECT (309): push target → pop target (stringa = nome finestra). */
static void vm_winstmt_309(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
if (task->sp > 0)
{
    gmb_stack_entry_t e = gmb_task_pop(task);
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING)
        gmb_winstmt_select(((gmb_obj_string_t*)e.ref)->data);
    if (e.ref) gmb_obj_release(e.ref);
}

}
/* WINDOW_THEME (310): push themeName → pop. Forma STRINGA = tema per nome
 * (gmb_winstmt_theme_name, stessa palette di C99Native/IL); forma NUMERICA = legado
 * gmb_winstmt_theme(n). Prima la stringa veniva castata a numero (0) e il nome del tema
 * andava perso: su VM/C WINDOW THEME "Nord" non applicava nulla. */
static void vm_winstmt_310(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
if (task->sp > 0)
{
    gmb_stack_entry_t e = gmb_task_pop(task);
    if (e.type == 2 && e.ref && e.ref->kind == GMB_OBJ_STRING) {
        gmb_winstmt_theme_name(((gmb_obj_string_t*)e.ref)->data);
    } else {
        int theme = (e.type != 2 && e.ref == NULL) ? (int)e.num : 0;
        gmb_winstmt_theme(theme);
    }
    if (e.ref) gmb_obj_release(e.ref);
}

}
/* WINDOW_DESKTOP (311): push desktopNum → pop. */
static void vm_winstmt_311(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
int desktop = task->sp > 0 ? (int)tui_pop_num(task) : 0;
gmb_winstmt_desktop(desktop);

}
/* WINDOW_MOVE_DESKTOP (312): push name, desktopNum → pop desktop, name. */
static void vm_winstmt_312(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
int desktop = task->sp > 0 ? (int)tui_pop_num(task) : 0;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
gmb_winstmt_move_desktop(name, desktop);

}
/* WINDOW_FRAME (313): push name(||""), style → pop style, name ("" = globale). */
static void vm_winstmt_313(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
int style = task->sp > 0 ? (int)tui_pop_num(task) : 0;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
if (name[0]) gmb_winstmt_frame(name, style); else gmb_winstmt_frame_all(style);

}
/* WINDOW_SHADOW (314): push name(||""), on → pop on, name ("" = globale). */
static void vm_winstmt_314(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
int on = task->sp > 0 ? (int)tui_pop_num(task) : 0;
char name[128] = "";
if (task->sp > 0) snprintf(name, sizeof(name), "%s", tui_pop_str(task));
if (name[0]) gmb_winstmt_shadow(name, on); else gmb_winstmt_shadow_all(on);

}
static void vm_console_230(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* SAVE SCREEN [nome] (C# SysCall=230): polimorfico (parity SaveScreenBuffer):
 * buffer/TUI → snapshot; schermo grafico → PNG; console → snapshot.
 * ref trattenuto durante l'uso (stessa lezione di 321/322). */
gmb_stack_entry_t _a230 = {0};
if (argc > 0 && task->sp > 0) _a230 = gmb_task_pop(task);
const char* name = (_a230.type == 2 && _a230.ref && _a230.ref->kind == GMB_OBJ_STRING)
    ? ((gmb_obj_string_t*)_a230.ref)->data : "";
gmb_console_savescreen_dispatch(name);
if (_a230.ref) gmb_obj_release(_a230.ref);

}
static void vm_console_231(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* RESTORE SCREEN [nome] (C# SysCall=231): polimorfico (parity RestoreScreenBuffer). */
gmb_stack_entry_t _a231 = {0};
if (argc > 0 && task->sp > 0) _a231 = gmb_task_pop(task);
const char* name = (_a231.type == 2 && _a231.ref && _a231.ref->kind == GMB_OBJ_STRING)
    ? ((gmb_obj_string_t*)_a231.ref)->data : "";
gmb_console_restorescreen_dispatch(name);
if (_a231.ref) gmb_obj_release(_a231.ref);

}
static void vm_console_321(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* CONSOLE_CAPTION (C# SysCall=321; ex-305 pre-shift enum): titolo finestra/headless.
 * ref TRATTENUTO durante l'uso (mai tui_pop_str qui: rilascia prima dell'uso e le
 * stringhe calcolate — es. "a"+"b" — diventano dangling; vedi 322). */
gmb_stack_entry_t _a321 = {0};
if (argc > 0 && task->sp > 0) _a321 = gmb_task_pop(task);
const char* title = (_a321.type == 2 && _a321.ref && _a321.ref->kind == GMB_OBJ_STRING)
    ? ((gmb_obj_string_t*)_a321.ref)->data : "";
gmb_console_set_title(title);
if (_a321.ref) gmb_obj_release(_a321.ref);

}
static void vm_console_322(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* CONSOLE_SENDKEY (C# SysCall=322; ex-306 pre-shift): inietta nella typeahead (parity
 * chunk builtin 3614 e C99 st_sendkey → gmb_console_sendkey). DOPPIA iniezione:
 * globale (letta dalle read scalari INKEY/GET/WAITKEY via readkey_nb) E vm->tui
 * (letta dai modali TUI_SYS via tui_run_modal: hanno stato clipper proprio, la globale
 * non la vedono — senza questa i modali restano appesi nonostante SENDKEY). */
const char* keys;
{
    /* ref TRATTENUTO durante l'uso (vedi 321): tui_pop_str rilascia prima dell'uso e
     * le stringhe calcolate (es. CHR(13)+CHR(13)) arrivano dangling → typeahead garbage. */
    gmb_stack_entry_t _a322 = {0};
    if (argc > 0 && task->sp > 0) _a322 = gmb_task_pop(task);
    keys = (_a322.type == 2 && _a322.ref && _a322.ref->kind == GMB_OBJ_STRING)
        ? ((gmb_obj_string_t*)_a322.ref)->data : "";
    gmb_console_sendkey(keys);
    if (keys && *keys) { tui_ensure(vm); gmb_vm_tui_inject_keys(vm, keys); }
    if (_a322.ref) gmb_obj_release(_a322.ref);
}

}
static void vm_console_323(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
/* SCROLLMODE [mode] (C# SysCall=323): regione scroll (0,25) se mode!=0 altrimenti (0,0);
 * parity PCode PopInt. Prima: corpo palette stale (mai raggiungibile: main table a headless). */
double m = 0;
if (argc > 0 && task->sp > 0) {
    gmb_stack_entry_t a = gmb_task_pop(task);
    if (a.type == 1) m = a.num;
    else if (a.type == 2 && a.ref && a.ref->kind == GMB_OBJ_STRING && ((gmb_obj_string_t*)a.ref)->data)
        m = atof(((gmb_obj_string_t*)a.ref)->data);
    if (a.ref) gmb_obj_release(a.ref);
}
gmb_console_set_scroll_region(0, m != 0.0 ? 25 : 0);

}
/* @GET (274): crea il campo = text-input CONTROLS nel contesto READ unificato.
 * Stack (16): row,col,var,picture,valid,rangeLo,rangeHi,color,type,items,w,h,caption,step,min,max. */
typedef struct { gmb_vm_t* vm; gmb_task_t* task; } vm_read_pred_ctx;
static vm_read_pred_ctx g_vm_read_pred;

static void vm_read_apply(void* user, gmb_read_field_t* f, const char* text)
{
    (void)user;
    if (!f || !f->apply_target) return;
    gmb_obj_t* target = (gmb_obj_t*)f->apply_target;
    if (target->kind == GMB_OBJ_BYREF) {
        /* Target NUMERICO (DIM q AS DOUBLE + @GET): il testo va convertito in numero,
         * altrimenti la cella double contiene una stringa e PRINT mostra "5" senza
         * formattazione (divergenza IL/C99: " 5 "). */
        gmb_stack_entry_t cur = gmb_byref_load((gmb_obj_byref_t*)target);
        if (cur.type == 1) {
            double dv = text ? atof(text) : 0.0;
            gmb_byref_store((gmb_obj_byref_t*)target, gmb_adapter_val_to_stack(gmb_make_val_double(dv)));
        } else {
            gmb_byref_store((gmb_obj_byref_t*)target, gmb_adapter_val_to_stack(gmb_make_val_string(text ? text : "")));
        }
    }
    else if (target->kind == GMB_OBJ_STRING) {
        /* Target DYNAMIC: conserva il TIPO corrente (numerico vs stringa) — un DYNAMIC
         * AS DOUBLE riempito da @GET deve restare numerico, altrimenti PRINT lo tratta
         * come stringa ("5" invece di " 5 "). */
        const char* dn = ((gmb_obj_string_t*)target)->data;
        /* LO STORAGE DELLE DYNAMIC È IL DYN SCOPE per-frame (OP_DYN_*), non gmb_dscope:
         * scrivere in dscope lasciava la variabile al valore iniziale (bug @GET su VM/C).
         * g_vm_read_pred.task è il task del programma; i suoi frame non sono TLS. */
        {
            gmb_task_t* t = g_vm_read_pred.task;
            if (t && dn && dn[0]) {
                int kind = gmb_vm_dyn_kind(t, dn);
                if (kind >= 0) {
                    gmb_stack_entry_t nv = (kind == 1)
                        ? gmb_adapter_val_to_stack(gmb_make_val_double(text ? atof(text) : 0.0))
                        : gmb_adapter_val_to_stack(gmb_make_val_string(text ? text : ""));
                    gmb_vm_dyn_store_name(t, dn, nv);
                    if (nv.type == 2 && nv.ref) gmb_obj_release(nv.ref);
                    return;
                }
            }
        }
        gmb_val_t cur = gmb_dscope_get_val(dn);
        if (cur.kind == GMB_TYPE_DOUBLE || cur.kind == GMB_TYPE_INT) {
            gmb_val_t v = gmb_make_val_double(text ? atof(text) : 0.0);
            gmb_dscope_set(dn, v);
        } else {
            gmb_val_t v = gmb_make_val_string(text ? text : "");
            gmb_dscope_set(dn, v);
            gmb_string_free(&v.v_string);
        }
    }
}
static void vm_read_apply_text(void* user, const char* text)
{ vm_read_apply(NULL, (gmb_read_field_t*)user, text); }

/* Runner predicato (WHEN/VALID): esegue il bytecode dall'indirizzo addr fino a OP_END_VALID
 * (mirror di PCodeProcessor.EvaluateValidPredicate), restituendo lo stack-top come bool.
 * Re-entry del dispatch via gmb_vm_tick (1 opcode per volta). Non-blocking. */
#define GMB_OP_END_VALID 128
static int vm_run_predicate(gmb_vm_t* vm, gmb_task_t* task, int addr)
{
    if (addr <= 0 || !vm || !vm->chunk) return 1;
    int saved_ip = task->ip, saved_sp = task->sp, saved_state = task->state;
    task->ip = addr;
    int guard = 0, completed = 0;
    while (guard++ < 4096)
    {
        if (task->ip < 0 || task->ip >= (int)vm->chunk->code_len) break;
        if (vm->chunk->code[task->ip] == GMB_OP_END_VALID) { completed = 1; break; }
        int tick = gmb_vm_tick(vm, task);
        /* gmb_vm_tick esegue un QUANTUM, non un solo opcode: END_VALID è già
         * stato consumato quando torna (state=FINISHED). */
        if (task->state == GMB_TS_FINISHED && task->current_ip >= 0 &&
            task->current_ip < (int)vm->chunk->code_len &&
            vm->chunk->code[task->current_ip] == GMB_OP_END_VALID) { completed = 1; break; }
        if (tick < 0 || task->state != saved_state) break;
    }
    int result = 0;
    if (completed && task->sp > saved_sp)
    {
        gmb_stack_entry_t e = gmb_task_pop(task);
        result = (e.type == 1) ? (e.num != 0.0) : 0;
        gmb_obj_release(e.ref);
    }
    gmb_task_truncate(task, saved_sp);
    task->ip = saved_ip;
    if (completed) task->state = saved_state;
    return result;
}
static int vm_read_pred(void* user, int addr)
{
    vm_read_pred_ctx* p = (vm_read_pred_ctx*)user;
    return (p && p->vm && p->task) ? vm_run_predicate(p->vm, p->task, addr) : 1;
}
static void vm_console_atget(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code;
    double when = (argc >= 17) ? tui_pop_num(task) : 0;
    double maxV = tui_pop_num(task), minV = tui_pop_num(task), stepV = tui_pop_num(task);
    (void)maxV; (void)minV;
    const char* caption = tui_pop_str(task); (void)caption;
    double ctrlH = tui_pop_num(task), ctrlW = tui_pop_num(task); (void)ctrlH;
    const char* items = tui_pop_str(task);
    const char* type = tui_pop_str(task);
    const char* color = tui_pop_str(task); (void)color;
    double rangeHi = tui_pop_num(task), rangeLo = tui_pop_num(task); (void)rangeHi; (void)rangeLo;
    double valid = tui_pop_num(task);
    const char* pic = tui_pop_str(task); (void)pic;
    gmb_stack_entry_t varE = gmb_task_pop(task);   /* var ref (byref) o SLOT per i locali */
    if (varE.type == 1)
    {
        /* Il compilatore C passa lo SLOT dei locali come numero (vedi cc ATFORM GET):
         * qui si materializza la cella write-through, che copre anche le STRINGHE. */
        gmb_stack_entry_t cellE = gmb_task_capture_local_cell(task, (int)varE.num);
        varE = cellE;
    }
    double col = tui_pop_num(task), row = tui_pop_num(task);
    int width = (ctrlW > 0) ? (int)ctrlW : 10;
    const char* initial = "";
    char initial_num[64]; initial_num[0] = 0;
    gmb_string_t initial_dyn = {0};
    if (varE.ref && varE.ref->kind == GMB_OBJ_BYREF) {
        gmb_stack_entry_t value = gmb_byref_load((gmb_obj_byref_t*)varE.ref);
        if (value.type == 2 && value.ref && value.ref->kind == GMB_OBJ_STRING)
            initial = ((gmb_obj_string_t*)value.ref)->data;
        else if (value.type == 1) {
            /* NUMERICO: il campo parte dal VALORE CORRENTE della variabile (come C99/C99B, che
             * passano già il valore): con "" un Enter su un campo non digitato azzerava la
             * variabile (ToDouble("") = 0). Stesso formato di gmb_double_to_string (%.15g). */
            snprintf(initial_num, sizeof initial_num, "%.15g", value.num);
            initial = initial_num;
        }
    } else if (varE.ref && varE.ref->kind == GMB_OBJ_STRING) {
        /* DYNAMIC: il valore CORRENTE (prefill del campo) vive nel dyn scope del task, non
         * nella gmb_dscope: leggerlo dalla dscope dava prefill vuoto (divergenza IL: "ab" + "7"
         * → "ab7", VM/C → "7"). Stessa fonte del write-back in vm_read_apply. */
        const char* nmI = ((gmb_obj_string_t*)varE.ref)->data;
        gmb_stack_entry_t cur;
        if (nmI && gmb_vm_dyn_get(task, nmI, &cur)) {
            if (cur.type == 1) {
                snprintf(initial_num, sizeof initial_num, "%.15g", cur.num);
                initial = initial_num;
            } else if (cur.type == 2 && cur.ref && cur.ref->kind == GMB_OBJ_STRING) {
                initial = ((gmb_obj_string_t*)cur.ref)->data;
            }
        } else {
            initial_dyn = gmb_dscope_get_string(nmI);
            initial = initial_dyn.data ? initial_dyn.data : "";
        }
    }
    gmb_read_ctx_t* ctx = gmb_read_active();
    ctx->apply = vm_read_apply;
    ctx->pred = vm_read_pred;
    g_vm_read_pred.vm = vm; g_vm_read_pred.task = task;
    ctx->pred_user = &g_vm_read_pred;
    /* Campi TIPIZZATI come C99Native: stessa factory del runtime condiviso. */
    extern int gmb_read_add_field_typed(int,int,int,const char*,const char*,const char*,const char*,double,double,double);
    double vmin = (rangeHi > rangeLo) ? rangeLo : minV;
    double vmax = (rangeHi > rangeLo) ? rangeHi : maxV;
    double vstep = stepV;
    int fidx = gmb_read_add_field_typed((int)row, (int)col, width, initial, type ? type : "",
                                        pic ? pic : "", items ? items : "", vmin, vmax, vstep);
    gmb_string_free(&initial_dyn);
    gmb_read_field_t* f = (fidx >= 0) ? &ctx->fields[fidx] : NULL;
    if (f && valid > 0) f->valid_addr = (int)valid;
    if (f && when > 0) f->when_addr = (int)when;
    if (f && varE.ref) {
        f->apply_target = varE.ref;  /* adotta il riferimento estratto dallo stack */
        f->apply_text = vm_read_apply_text;
        f->apply_text_user = f;
    } else gmb_obj_release(varE.ref);
}
/* READ (275): avvia il loop READ unificato (park-based). */
/* MESSAGE/ERROR: agiscono sull'ultimo campo @GET aggiunto (idx -1). */
static void vm_console_msg(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char t[256]; tui_pop_str_into(task, t, sizeof t);
    gmb_read_set_message(-1, t);
}
static void vm_console_err(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)code; (void)argc;
    char t[256]; tui_pop_str_into(task, t, sizeof t);
    gmb_read_set_error(-1, t);
}

static void vm_console_read(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    (void)vm; (void)task; (void)code; (void)argc;
    gmb_read_ctx_t* ctx = gmb_read_active();
    gmb_read_run(ctx);
    for (int i = 0; i < ctx->n_fields; i++) {
        gmb_obj_release((gmb_obj_t*)ctx->fields[i].apply_target);
        ctx->fields[i].apply_target = NULL;
    }
    gmb_read_reset(ctx);
}

static const cons_sys_row CONSOLE_SYS_TBL[] = {
    { 4, vm_console_4 },
    { 5, vm_console_5 },
    { 6, vm_console_6 },
    { 7, vm_console_7 },
    { 8, vm_console_7 },
    { 9, vm_console_9 },
    { 10, vm_console_10 },
    { 11, vm_console_11 },
    { 13, vm_console_13 },
    { 14, vm_console_14 },
    { 210, vm_console_210 },
    { 293, vm_console_293 },
    { 219, vm_console_219 },
    { 226, vm_console_226 },
    { 227, vm_console_227 },
    { 228, vm_console_226 },
    { 229, vm_console_227 },
    { 268, vm_console_268 },
    { 269, vm_console_269 },
    { GMB_SC_SET_CLIPPER, vm_console_290 },   /* 297: SET BELL/THEME/DATEFORMAT… (era il letterale 290,
                                                * ormai TUI_DATEPICKER: con il vecchio mapping il
                                                * comando SET non arrivava al runtime) */
    { GMB_SC_SPRITE_DEF, vm_console_296 },
    /* WINDOW 302-314 (codici reali SysCalls.cs; 297/298 sono SPRITE_CMD/TILEMAP_CMD
     * → grafica, instradati a headless_big in gmb_vm_core.c). 308 (WINDOW_EDIT) ora ha
     * l'argomento enable sullo stack (fix Compiler.Statements.VisitWindowEditStatement). */
    { GMB_SC_WINDOW_ACTION, vm_winstmt_302 },
    { GMB_SC_WINDOW_DEFINE, vm_winstmt_303 },
    { GMB_SC_WINDOW_APPEND, vm_winstmt_304 },
    { GMB_SC_WINDOW_SET_CONTENT, vm_winstmt_305 },
    { GMB_SC_WINDOW_CLEAR, vm_winstmt_306 },
    { GMB_SC_WINDOW_SCROLL, vm_winstmt_307 },
    { GMB_SC_WINDOW_EDIT, vm_winstmt_308 },
    { GMB_SC_WINDOW_SELECT, vm_winstmt_309 },
    { GMB_SC_WINDOW_THEME, vm_winstmt_310 },
    { GMB_SC_WINDOW_DESKTOP, vm_winstmt_311 },
    { GMB_SC_WINDOW_MOVE_DESKTOP, vm_winstmt_312 },
    { GMB_SC_WINDOW_FRAME, vm_winstmt_313 },
    { GMB_SC_WINDOW_SHADOW, vm_winstmt_314 },
    { GMB_SC_CONSOLE_CAPTION, vm_console_321 },
    { GMB_SC_CONSOLE_SENDKEY, vm_console_322 },
    { 230, vm_console_230 },
    { 231, vm_console_231 },
    { GMB_SC_CONSOLE_SCROLLMODE, vm_console_323 },
    { 274, vm_console_atget },
    { 481, vm_console_msg },
    { 482, vm_console_err },
    { 275, vm_console_read },
};

static void vm_console_consume_all(gmb_vm_t* vm, gmb_task_t* task, int code, int argc) {
    (void)vm; (void)code; (void)argc;
for (int i = 0; i < argc && task->sp > 0; i++)
{
    gmb_stack_entry_t a = gmb_task_pop(task);
    gmb_obj_release(a.ref);
}

}

void gmb_vm_exec_console_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc)
{
    /* SCOPE DINAMICO (DYNAMIC §2.5): stessi helper del runtime condiviso usati da C99Native. */
    if (code == 470) { extern void gmb_dscope_push(void); gmb_dscope_push(); return; }
    if (code == 471) { extern void gmb_dscope_pop(void); gmb_dscope_pop(); return; }
    if (code == 473) {   /* GET name -> push valore (prestito copiato) */
        char nm[128]; tui_pop_str_into(task, nm, sizeof nm);
        extern gmb_val_t gmb_dscope_get_val(const char*);
        gmb_val_t v = gmb_dscope_get_val(nm);
        if (v.kind == GMB_TYPE_DOUBLE || v.kind == GMB_TYPE_INT || v.kind == GMB_TYPE_BOOL) {
            /* Scalare: entry NUMERICA (non push_ref con ref NULL: PRINT la vedeva vuota). */
            double d = (v.kind == GMB_TYPE_INT) ? (double)v.v_int : (v.kind == GMB_TYPE_BOOL ? (v.v_bool ? -1.0 : 0.0) : v.v_double);
            gmb_stack_entry_t se; memset(&se, 0, sizeof se); se.type = 1; se.num = d; se.basic_type = GMB_BT_NUMBER;
            if (task->sp < GMB_STACK_MAX) task->stack[task->sp++] = se;
        } else {
            push_ref_entry(task, (gmb_obj_t*)gmb_adapter_val_to_stack(v).ref,
                           gmb_adapter_val_to_stack(v).basic_type);
        }
        return;
    }
    if (code == 474) {   /* SET name, value */
        gmb_stack_entry_t v = task->sp > 0 ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        char nm[128]; tui_pop_str_into(task, nm, sizeof nm);
        extern void gmb_dscope_set(const char*, gmb_val_t);
        extern gmb_val_t gmb_dscope_get_val(const char*);
        gmb_val_t cur = gmb_dscope_get_val(nm), nv = gmb_adapter_stack_to_val(v);
        if ((cur.kind == GMB_TYPE_DOUBLE || cur.kind == GMB_TYPE_INT) && nv.kind == GMB_TYPE_STRING)
            nv = gmb_make_val_double(nv.v_string.data ? atof(nv.v_string.data) : 0.0);
        gmb_dscope_set(nm, nv);
        gmb_obj_release(v.ref);
        return;
    }
    if (code == 475) {   /* SET con stack [value][name]: store_target ha già push-ato il valore */
        char nm[128]; tui_pop_str_into(task, nm, sizeof nm);
        gmb_stack_entry_t v = task->sp > 0 ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        extern void gmb_dscope_set(const char*, gmb_val_t);
        extern gmb_val_t gmb_dscope_get_val(const char*);
        gmb_val_t cur = gmb_dscope_get_val(nm), nv = gmb_adapter_stack_to_val(v);
        /* Preserva il TIPO del DYNAMIC: un READ su `DYNAMIC q AS DOUBLE` arriva come testo
         * ("5") e deve restare numerico, altrimenti PRINT lo formatta come stringa ("5"
         * invece di " 5 ", divergenza IL/C99/P-Code). */
        if ((cur.kind == GMB_TYPE_DOUBLE || cur.kind == GMB_TYPE_INT) && nv.kind == GMB_TYPE_STRING)
            nv = gmb_make_val_double(nv.v_string.data ? atof(nv.v_string.data) : 0.0);
        gmb_dscope_set(nm, nv);
        return;
    }
    if (code == 472) {   /* DECLARE name, value (argc=2) */
        gmb_stack_entry_t v = task->sp > 0 ? gmb_task_pop(task) : (gmb_stack_entry_t){0};
        char nm[128]; tui_pop_str_into(task, nm, sizeof nm);
        extern void gmb_dscope_declare(const char*, gmb_val_t);
        gmb_dscope_declare(nm, gmb_adapter_stack_to_val(v));
        gmb_obj_release(v.ref);
        return;
    }
    (void)vm;
    for (size_t i = 0; i < sizeof(CONSOLE_SYS_TBL) / sizeof(CONSOLE_SYS_TBL[0]); i++)
        if (CONSOLE_SYS_TBL[i].code == code) { CONSOLE_SYS_TBL[i].fn(vm, task, code, argc); return; }
    vm_console_consume_all(vm, task, code, argc);
}

