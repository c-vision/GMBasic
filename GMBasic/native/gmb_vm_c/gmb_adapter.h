/* ============================================================================
 * gmb_adapter.h — Adapter di valore tra VM/C (gmb_stack_entry_t) e il runtime
 * condiviso (gmb_val_t di gmb_runtime_c). Fase B dell'unificazione runtime.
 *
 * La VM mantiene il suo stack operandi (gmb_stack_entry_t, usato dal GPM), ma
 * al confine con le librerie condivise converte a gmb_val_t (modello transpiler).
 * ============================================================================ */
#ifndef GMB_ADAPTER_H
#define GMB_ADAPTER_H

#include "gmb_vm_value.h"
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Converte uno stack_entry VM in gmb_val_t (modello transpiler).
 * NOTA: gmb_val_t è a valore; le stringhe vengono copiate (gmb_make_val_string). */
gmb_val_t gmb_adapter_stack_to_val(gmb_stack_entry_t s);
/* Come stack_to_val ma SENZA il riferimento in piu' sugli oggetti contati: vista in prestito dell'entry
 * (valida finche' l'entry vive). Da usare quando il valore va subito in un contenitore che trattiene da
 * se' (gmb_list_add/set/insert, gmb_dict_set) e l'entry viene poi rilasciata dal chiamante. */
gmb_val_t gmb_adapter_stack_to_val_borrowed(gmb_stack_entry_t s);

/* Converte gmb_val_t in uno stack_entry VM (refcount).
 * Le stringhe diventano gmb_obj_string_t con retain. */
gmb_stack_entry_t gmb_adapter_val_to_stack(gmb_val_t v);

/* Variante "by-reference" di gmb_adapter_val_to_stack: se il valore è già un oggetto VM
 * (gmb_obj_t) di kind GMB_OBJ_DICT/GMB_OBJ_LIST, restituisce il RIFERIMENTO diretto (solo
 * retain, nessuna copia) invece della copia superficiale che la normale conversione applica
 * per valore. Usata da OP_LOAD_ELEMENT_REF (Compiler.UserTypes.cs::AssignField,
 * `Arr(i).Campo = valore`): senza questo, l'elemento letto per l'assegnazione era sempre una
 * copia scartata subito dopo — la scrittura non persisteva mai nell'array (bug scoperto
 * 2026-09-02, vedi Classi-Distruttore-Ereditarieta-CrossFile-Bugs). Per qualunque altro caso
 * (numeri, stringhe, valori runtime nativi senza già un gmb_obj_t wrapper) si comporta
 * identicamente a gmb_adapter_val_to_stack, che non ha nulla da preservare per riferimento. */
gmb_stack_entry_t gmb_adapter_val_to_stack_ref(gmb_val_t v);

/* Numeri / bool / nil helper */
gmb_val_t gmb_adapter_num(double n);
double gmb_adapter_to_double(gmb_val_t v);

/* Discriminazione UNICA dei contenitori (lista/dict) fra le due rappresentazioni che
 * coesistono per un oggetto boxato:
 *   1) header VM:  kind GMB_OBJ_LIST / GMB_OBJ_DICT   (gmb_shared_list_new/dict_new)
 *   2) box runtime: kind 0 + type_name "gmb_list"/"gmb_dict" (gmb_make_val_list/dict)
 * Il kind NON basta da solo: GMB_OBJ_STRING vale 0 (gmb_runtime.h), lo STESSO valore che
 * gmb_obj_new_typed lascia di default, quindi un box runtime-wrapped sembrava una stringa e
 * gli si leggeva ->data come char* (in un dict finiva una stringa vuota invece dell'array:
 * il sintomo "backends": "" del registry VM/C).(type_name NULL resta il discriminante
 * sicuro: le vere stringhe non lo impostano mai). Unica implementazione usata da
 * val_to_stack, stack_to_val e dallo store di OP_STORE_ELEMENT. */
int gmb_adapter_obj_is_container(const gmb_obj_t* o);

#ifdef __cplusplus
}
#endif

#endif /* GMB_ADAPTER_H */
