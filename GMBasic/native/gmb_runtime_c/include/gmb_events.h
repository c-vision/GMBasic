/* ============================================================================
 * gmb_events.h — Registry eventi runtime condiviso (ON KEY) + dispatch alla lettura.
 * File: native/gmb_runtime_c/include/gmb_events.h
 *
 * Contratto (parità canone IL/PCode — EventDispatcher.RegisterKey + DispatchKey):
 *  - gmb_events_onkey_register(keycode, fn): registra (o sostituisce) il handler C
 *      per `keycode`; fn==NULL = ON KEY STOP (deregistra).
 *  - gmb_events_onkey_dispatch(keycode): chiamare SUBITO DOPO la lettura di un tasto
 *      (INKEY/GET) — fire INLINE del handler (deterministico, ma silenzioso-sbagliato
 *      vietato: registri pieni segnalati nelle note del file).
 *
 * Specchio C# singleton ClipperLib.App.EventDispatcher; della VM/C usa gmb_vm_events
 * (spawn task all'indirizzo — stesso canone PCode FireKeyEvent).
 * ============================================================================ */

#ifndef GMB_EVENTS_H
#define GMB_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Il handler è una SUB BASIC emessa come `gmb_double_t f(void)` (convenzione
 * gmb_user_<Nome> del C99Native): il valore di ritorno è ignorato al fire. */
#include "gmb_runtime.h"

typedef gmb_double_t (*gmb_onkey_fn_t)(void);

void gmb_events_onkey_register(int keycode, gmb_onkey_fn_t fn);
void gmb_events_onkey_dispatch(int keycode);

#ifdef __cplusplus
}
#endif

#endif /* GMB_EVENTS_H */
