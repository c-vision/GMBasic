/* ============================================================================
 * gmb_events.c — Registry eventi runtime condiviso (ON KEY) + dispatch alla lettura.
 * File: native/gmb_runtime_c/src/gmb_events.c
 *
 * Contratto (parità canone IL/PCode — EventDispatcher.RegisterKey + DispatchKey):
 * le gambe che NON hanno uno scheduler di task (C99Native registrato a
 * funzione) usano questo registry a FUNZIONE e chiamano gmb_events_onkey_dispatch
 * subito dopo la lettura di un tasto (INKEY/GET — hook a livello emitter). Il fire
 * è INLINE (deterministico). Per la VM/C (task all'indirizzo) si usa gmb_vm_events
 * (spawn task indipendente = parità FireKeyEvent) — stessa semantica, modello
 * di dispatch diverso: registry a indirizzo vs a funzione.
 *
 * Limitazione dichiarata: registry statico per-processo (una VM per host) — specchio
 * del singleton ClipperLib.App.EventDispatcher del backend C#.
 * ============================================================================ */

#include "gmb_events.h"

#include <stddef.h>

#define GMB_EVK_CAP 64

typedef struct {
    int keycode;
    gmb_onkey_fn_t fn;
    int used;
} gmb_evk_entry_t;

static gmb_evk_entry_t g_evk[GMB_EVK_CAP];
static int g_evk_count = 0;

void gmb_events_onkey_register(int keycode, gmb_onkey_fn_t fn)
{
    if (keycode <= 0)
        return;
    for (int i = 0; i < g_evk_count; i++)
    {
        if (g_evk[i].used && g_evk[i].keycode == keycode)
        {
            /* re-registrazione: sostituisce; con fn==NULL (STOP) rimuove */
            if (fn == NULL)
            {
                g_evk[i] = (gmb_evk_entry_t){0};
                return;
            }
            g_evk[i].fn = fn;
            return;
        }
    }
    if (fn == NULL)
        return; /* STOP su tasto non registrato: no-op */
    if (g_evk_count >= GMB_EVK_CAP)
        return; /* registry pieno: registro silenziosamente fallito dichiarato qui */
    g_evk[g_evk_count++] = (gmb_evk_entry_t){ keycode, fn, 1 };
}

/* Chiamata alla LETTURA di un tasto (inviare il byte/ConsoleKey letto). Fire INLINE
 * del handler registrato (nessun task: deterministico, mai silenzioso). */
void gmb_events_onkey_dispatch(int keycode)
{
    if (keycode <= 0)
        return;
    for (int i = 0; i < g_evk_count; i++)
    {
        if (g_evk[i].used && g_evk[i].keycode == keycode && g_evk[i].fn)
        {
            g_evk[i].fn();
            return;
        }
    }
}
