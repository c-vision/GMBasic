/* gmb_builtin_tray.c — handler VMC del namespace TRAY (icona barra SO).
 * §0quater: file di dominio. §0bis: chiama la libreria del runtime condiviso
 * (native/gmb_runtime_c/src/gmb_tray*.c) — stessa usata da C99Native.
 * Codici: TRAYCREATE=3720 … TRAYAVAILABLE=3728 (BuiltInFunction).
 * Predicati (SELFTEST/AVAILABLE): convenzione Bit -1/0 (allineata a IL/C99). */
#include "gmb_vm_core.h"
#include "gmb_tray.h"

void gmb_bf_tray(gmb_vm_t* vm, gmb_stack_entry_t* args, int argc, int func_code,
                 gmb_builtin_out_t* bout, gmb_task_t* task)
{
    (void)vm; (void)task;
    char s0[1024], s1[1024];
    s0[0] = 0; s1[0] = 0;
    if (argc >= 1) vm_entry_str(args[0], s0, (int)sizeof(s0));
    if (argc >= 2) vm_entry_str(args[1], s1, (int)sizeof(s1));
    bout->is_num = 1; bout->result = 0; bout->result_ref = NULL; bout->result_ref_bt = 0;
    switch (func_code) {
        case 3720: bout->result = (double)gmb_tray_create(s0, s1); break;
        case 3721: bout->result = (double)gmb_tray_item((int)args[0].num, s1); break;
        case 3722: bout->result = (double)gmb_tray_sep(); break;
        case 3723: bout->result = (double)gmb_tray_tooltip(s0); break;
        case 3724: bout->result = (double)gmb_tray_poll(); break;
        case 3725: gmb_tray_remove(); bout->result = 0.0; break;
        case 3726: bout->is_num = 0; bout->result_ref = (gmb_obj_t*)gmb_str_cstr(gmb_tray_backend());
                   bout->result_ref_bt = GMB_BT_STRING; break;
        case 3727: bout->result = gmb_tray_selftest() ? -1.0 : 0.0; break;
        case 3728: bout->result = gmb_tray_available() ? -1.0 : 0.0; break;
        case 3729: bout->result = (double)gmb_tray_last(); break;
        default: break;
    }
}
