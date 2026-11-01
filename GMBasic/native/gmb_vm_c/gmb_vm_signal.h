/* gmb_vm_signal.h — handler dei syscall SIGNAL sulla VM/C (implementazione in
 * gmb_vm_signal.c). Il filtro e la consegna vivono nel runtime condiviso (gmb_signal.c). */
#ifndef GMB_VM_SIGNAL_H
#define GMB_VM_SIGNAL_H

#include "gmb_vm_core.h"

void vm_signal_subscribe(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void vm_signal_on(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void vm_signal_emit(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void vm_signal_unsubscribe(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);
void vm_signal_ping(gmb_vm_t* vm, gmb_task_t* task, int code, int argc);

#endif
