/* ============================================================================
 * gmb_vm_events.h — Registry eventi runtime (ON KEY) e dispatch alla lettura tasti.
 * File: native/gmb_vm_c/gmb_vm_events.h
 *
 * Contratto (parità PCode/IL — FireKeyEvent / DispatchKeyEvent):
 *  - gmb_vm_onkey_register(keycode, action, target_addr):
 *      action != 0 -> registra (o sostituisce) l'handler per `keycode`;
 *      action == 0 (ON KEY STOP) -> rimuove gli handler per `keycode`.
 *  - gmb_vm_onkey_fire(vm, task, keycode):
 *      chiamato DOPO la lettura di un tasto dal runtime di input (INKEY per primo);
 *      se esiste un handler per `keycode` spawna un task indipendente all'indirizzo
 *      handler (senza wg/args, con i globali di `task`), come OP_GO senza wg.
 *
 * Limitazione dichiarata: registry per-processo (una VM per host) — specchio del
 * singleton EventDispatcher del backend C#.
 * ============================================================================ */

#ifndef GMB_VM_EVENTS_H
#define GMB_VM_EVENTS_H

#include "gmb_vm_core.h"
#include "gmb_gpm.h"

void gmb_vm_onkey_register(int keycode, int action, int target_addr);
void gmb_vm_onkey_fire(gmb_vm_t* vm, gmb_task_t* task, int keycode);

/* Eventi controllo (ON <EV> CONTROL <h>): registry (handle, ev) -> target_addr.
 * action==0 = STOP (rimuove). gmb_vm_control_fire spawna un task all'indirizzo handler. */
void gmb_vm_control_register(int handle, int ev, int action, int target_addr);
void gmb_vm_control_register_inst(int handle, int ev, const char* method, void* inst);
int  gmb_vm_control_inst_of(int handle, int ev, void** out_inst, char* out_method, int cap);
void gmb_vm_control_fire(gmb_vm_t* vm, gmb_task_t* task, int handle, int ev);
/* Indirizzo handler registrato per (handle, ev), o -1. */
int  gmb_vm_control_target(int handle, int ev);

#endif /* GMB_VM_EVENTS_H */
