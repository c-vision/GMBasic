/* gmb_builtin_registry.h — registro delle tabelle builtin per dominio.
 *
 * Il core della VM registra la propria tabella; ogni modulo di dominio (gmb_builtin_ext.c)
 * registra la sua al load via constructor. Il dispatcher cerca prima nel core, poi nelle
 * tabelle registrate. Cosi' un host che non linka un modulo non ne tira gli handler
 * (il build completo li include tutti: compilatore completo, nessuna feature rimossa).
 */
#ifndef GMB_BUILTIN_REGISTRY_H
#define GMB_BUILTIN_REGISTRY_H

#include <stddef.h>
#include "gmb_vm_core.h"

typedef struct {
    int code;
    void (*handler)(gmb_vm_t*, gmb_stack_entry_t*, int, int, gmb_builtin_out_t*, gmb_task_t*);
} gmb_bf_impl_t;

void gmb_builtin_register(const gmb_bf_impl_t* table, size_t count);

#endif
