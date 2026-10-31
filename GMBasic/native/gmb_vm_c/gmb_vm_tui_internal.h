/* gmb_vm_tui_internal.h — helper CONDIVISI fra i file della VM/C per TUI e console.
 *
 * Nati dallo split di `gmb_vm_tui.c` (2205 righe → 3 file, §0quater). La parte "controlli" e
 * la parte "console" usano gli stessi primitivi di stack/input e lo stesso contesto modale,
 * che prima erano `static` in un unico file. Le definizioni restano UNA SOLA volta in
 * `gmb_vm_tui.c`; qui ci sono solo i prototipi (e il typedef che serve a entrambe).
 *
 * NB: NON si includono qui gli header di dominio (gmb_tui_app.h ecc.): ognuno di quei file ha
 * la propria lista, e includerli due volte in ordini diversi confonde i typedef. Chi include
 * questo header deve includere anche la propria lista, come faceva `gmb_vm_tui.c`.
 */
#ifndef GMB_VM_TUI_INTERNAL_H
#define GMB_VM_TUI_INTERNAL_H

#include "gmb_vm_tui.h"

/* Stato TUI per-VM (definito in gmb_vm_tui.c): i controlli ne leggono i campi. */
struct gmb_vm_tui {
    gmb_input_source_t input;
    gmb_clipper_state_t* st;   /* typeahead per l'input (gmb_clipper_input_keyboard) */
    gmb_screen_buffer_t* buf;  /* buffer 80x25 per il render */
    gmb_modal_host_t host;
    bool ready;
    void (*render)(gmb_screen_buffer_t* buf, void* ctx);
    void* render_ctx;
};

/* Contesto del loop modale (widget TUI): usato sia dal core sia dai controlli. */
typedef struct tui_modal_ctx {
    gmb_vm_tui_t* tui;
    gmb_control_t* ctrl;
    int exit_key;   /* GMB_K_ENTER o GMB_K_ESC */
    int result;
    bool done;
} tui_modal_ctx_t;

double      tui_pop_num(gmb_task_t* task);
const char* tui_pop_str(gmb_task_t* task);
int         tui_pop_str_list(gmb_task_t* task, char out[][256], int cap);
void        tui_push_num(gmb_task_t* task, double v, uint8_t bt);
void        tui_push_bool(gmb_task_t* task, bool b);
void        tui_push_str(gmb_task_t* task, const char* s);
void        tui_join_csv(const char items[][256], int n, char* out, int cap);
void        tui_ensure(gmb_vm_t* vm);
void        tui_pop_str_into(gmb_task_t* task, char* dst, size_t cap);

/* Gestori spostati nei file per dominio, chiamati dal dispatch TUI. */


/* Gestori spostati nei file per dominio, usati dal dispatch (tabella) del core. */
void tui_checkbox(gmb_vm_t* vm, gmb_task_t* task);
void tui_listbox(gmb_vm_t* vm, gmb_task_t* task);
void tui_combobox(gmb_vm_t* vm, gmb_task_t* task);
void tui_toggle(gmb_vm_t* vm, gmb_task_t* task);
void tui_radiogroup(gmb_vm_t* vm, gmb_task_t* task);
void tui_slider(gmb_vm_t* vm, gmb_task_t* task);
void tui_numericupdown(gmb_vm_t* vm, gmb_task_t* task);
void tui_starrating(gmb_vm_t* vm, gmb_task_t* task);
void tui_scrollbar(gmb_vm_t* vm, gmb_task_t* task);
void tui_scrollview(gmb_vm_t* vm, gmb_task_t* task);
void tui_label(gmb_vm_t* vm, gmb_task_t* task);
void tui_datalist(gmb_vm_t* vm, gmb_task_t* task);
void tui_propertygrid(gmb_vm_t* vm, gmb_task_t* task);
void tui_logview(gmb_vm_t* vm, gmb_task_t* task);
void tui_hexview(gmb_vm_t* vm, gmb_task_t* task);
void tui_bigtext(gmb_vm_t* vm, gmb_task_t* task);
void tui_codearea(gmb_vm_t* vm, gmb_task_t* task);
void tui_reorderlistbox(gmb_vm_t* vm, gmb_task_t* task);
void tui_alertbox(gmb_vm_t* vm, gmb_task_t* task);
void tui_confirmationbox(gmb_vm_t* vm, gmb_task_t* task);
void tui_fileexplorer(gmb_vm_t* vm, gmb_task_t* task);
void tui_dashboard(gmb_vm_t* vm, gmb_task_t* task);
void tui_splitter(gmb_vm_t* vm, gmb_task_t* task);
void tui_contextmenu(gmb_vm_t* vm, gmb_task_t* task);
void tui_tab(gmb_vm_t* vm, gmb_task_t* task);
void tui_grid(gmb_vm_t* vm, gmb_task_t* task);
void tui_datepicker(gmb_vm_t* vm, gmb_task_t* task);
void tui_colorpicker(gmb_vm_t* vm, gmb_task_t* task);
void tui_treeview(gmb_vm_t* vm, gmb_task_t* task);
void tui_menu(gmb_vm_t* vm, gmb_task_t* task);
void tui_filedialog(gmb_vm_t* vm, gmb_task_t* task);
void tui_printer(gmb_vm_t* vm, gmb_task_t* task);

#endif /* GMB_VM_TUI_INTERNAL_H */
