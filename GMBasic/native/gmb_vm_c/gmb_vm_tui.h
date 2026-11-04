/* ============================================================================
 * gmb_vm_tui.h — Controlli Clipper TUI nella VM/C (syscall 273–289).
 * Semantica: PCodeVM deprecato di gmo (PCodeProcessor.Tui.cs). Input REALE
 * (no headless): default = raw tty (/dev/tty) con fallback a typeahead iniettata
 * (gmb_clipper_input_keyboard). Render = gmb_render_screen → stdout.
 * ============================================================================ */
#ifndef GMB_VM_TUI_H
#define GMB_VM_TUI_H

#include "gmb_vm_core.h"
#include "gmb_tui_widgets.h"

typedef struct gmb_screen_buffer gmb_screen_buffer_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Codici syscall TUI — VALORI REALI di Compiler/SysCalls.cs (C#). Erano 273-289/290
 * (specchio obsoleto): l'enum C# è cresciuto di 5 membri → CHECKBOX=278 etc. Con i numeri
 * vecchi ogni widget eseguiva l'handler sbagliato (es. CHECKBOX→FILEDIALOG: hang). */
enum {
    GMB_TUI_CHECKBOX    = 278,
    GMB_TUI_LISTBOX     = 279,
    GMB_TUI_COMBOBOX    = 280,
    GMB_TUI_MENU        = 281,
    GMB_TUI_STATUSBAR   = 282,
    GMB_TUI_FILEDIALOG  = 283,
    GMB_TUI_PRINTER     = 284,
    GMB_TUI_TOGGLE      = 285,
    GMB_TUI_RADIOGROUP  = 286,
    GMB_TUI_SLIDER      = 287,
    GMB_TUI_TAB         = 288,
    GMB_TUI_GRID        = 289,
    GMB_TUI_DATEPICKER  = 290,
    GMB_TUI_COLORPICKER = 291,
    GMB_TUI_TREEVIEW    = 292,
    GMB_TUI_SPINNER     = 293,
    GMB_TUI_TBROWSE     = 294,
    GMB_TUI_LAST_HANDLE = 295,
    GMB_CONTROL_APPLY_PROPS = 296,
    GMB_TUI_SET_CLIPPER = 297,
    GMB_TUI_NUMERICUPDOWN = 424,
    GMB_TUI_STARRATING = 425,
    GMB_TUI_SCROLLBAR = 426,
    GMB_TUI_SCROLLVIEW = 427,
    GMB_TUI_LABEL = 428,
    GMB_TUI_DATALIST = 429,
    GMB_TUI_PROPERTYGRID = 430,
    GMB_TUI_LOGVIEW = 431,
    GMB_TUI_HEXVIEW = 432,
    GMB_TUI_BIGTEXT = 433,
    GMB_TUI_CODEAREA = 434,
    GMB_TUI_REORDERLISTBOX = 435,
    GMB_TUI_ALERTBOX = 436,
    GMB_TUI_CONFIRMATIONBOX = 437,
    GMB_TUI_FILEEXPLORER = 438,
    GMB_TUI_DASHBOARD = 439,
    GMB_TUI_SPLITTER = 451,
    GMB_TUI_CONTEXTMENU = 452,
};

/* Contesto TUI per-VM (creato lazy al primo syscall). */
typedef struct gmb_vm_tui gmb_vm_tui_t;

/* Esegue un syscall TUI (273–290): costruisce il controllo runtime, loop modale
 * con input reale, spinge il risultato come PCodeVM. */
void gmb_vm_exec_tui_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void gmb_vm_exec_console_syscall(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);

/* Window manager statico condiviso (WINDOW_DEFINE/WINDOW_ACTION). */
gmb_window_manager_t* gmb_vm_tui_static_wm(void);

/* Inietta una stringa di tasti nella typeahead (test/host). */
void gmb_vm_tui_inject_keys(gmb_vm_t* vm, const char* s);

/* Contesto (vm, task) per i thunk degli eventi finestra: va impostato attorno a ogni punto
 * che può innalzare un evento (WINDOW … ACTION, frame/pump). NULL = nessun handler eseguito. */
void gmb_vm_tui_event_ctx(gmb_vm_t* vm, gmb_task_t* task);

/* Imposta un hook di render custom (default: gmb_render_screen → stdout). */
void gmb_vm_tui_set_render(gmb_vm_t* vm, void (*render)(gmb_screen_buffer_t*, void*), void* ctx);

/* Libera il contesto TUI (chiamato da gmb_vm_free). */
void gmb_vm_tui_free(gmb_vm_t* vm);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_TUI_H */
