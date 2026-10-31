/* gmb_module.h — registry di moduli esterni (model compiler §4.3, lato runtime C condiviso). */
#ifndef GMB_MODULE_H
#define GMB_MODULE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registra una dylib su uno SLOT a priorità P (la più alta vince; a parità l'ultima). */
int gmb_module_register(const char* slot, const char* dylib, int priority);
/* Cambia la priorità della dylib registrata su uno slot (dylib vuota = la vincente). */
int gmb_module_priority(const char* slot, const char* dylib, int priority);
/* La dylib VINCENTE per lo slot (priorità più alta), NULL se nessuna. */
const char* gmb_module_resolve(const char* slot);
/* Simbolo dalla dylib vincente dello slot (dlopen/dlsym), NULL se non risolve. */
void* gmb_module_sym(const char* slot, const char* symbol);
/* Elenca i moduli: "slot\tdylib\tpriority\n" per riga. Ritorna il numero di moduli. */
int gmb_module_list(char* out, size_t cap);
/* Svuota il registry. */
void gmb_module_reset(void);
/* Dispatcher unico REGISTER|PRIORITY|LIST|SYM -> report in `out`. */
int gmb_module_cmd_to(const char* sub, const char* a, const char* b, char* out, size_t cap);
/* Chiama un simbolo: packed = "simbolo|sig|argsCsv" (sig: prima lettera = ritorno). */
int gmb_module_call_to(const char* slot, const char* packed, char* out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* GMB_MODULE_H */
