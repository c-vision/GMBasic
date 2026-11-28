/* gmb_3d_stats.c — statistiche della scena (P5, `SCENE.STATS(name$)`).
 *
 * Perché esiste: la nota di thread/present (R4/R6) chiede di DIMOSTRARE due
 * invarianti, non di dichiararli — «RENDER non alloca per frame» e «RENDER una
 * volta per frame». I contatori del pass (tris_drawn/culled/pixels) vivono in
 * gmb_3d_cmd.c, le capacità in gmb_3d_scene_buf.c: qui si espongono con UN nome
 * e si contano le chiamate a RENDER per frame.
 *
 * Contratto (specifica `M-STATS`):
 *  - `renders`      = RENDER chiamati nel frame aperto; PRESENT chiude il frame
 *                     (lo azzera, vedi gmb_scene_present_done: un punto solo,
 *                     anche per le gambe che copiano il frame da sole) e RESET
 *                     azzera tutto. Chi non presenta mai legge il totale
 *                     dall'ultimo RESET.
 *  - `renders_max`  = il massimo raggiunto da `renders` dopo RESET: è il numero
 *                     da guardare («> 1 = RENDER chiamato più volte in un giro»).
 *  - `allocs`       = allocazioni del modulo 3D dall'avvio del processo
 *                     (gmb_3d_alloc_count): NON si azzera con RESET, è la prova
 *                     di R4 — un frame che alloca lo fa crescere.
 *  - `bytes`        = memoria RISERVATA dalla geometria (capacità, non uso):
 *                     il target e il buffer di pick non sono scena dichiarata e
 *                     non si contano.
 *  - `tris`/`draw`/`culled`/`pixels`/`objs`: gli stessi numeri di
 *                     SCENE.TRIS/DRAWN/CULLED/PIXELS, più gli oggetti.
 *  - chiave sconosciuta = −1 (C-SENTINEL). È una LETTURA: niente errore di
 *                     superficie, nessun valore inventato.
 *
 * Nessuna allocazione, nessun I/O, nessun orologio: si leggono contatori già
 * presenti (il tempo di un frame si misura fuori, piano §0septies).
 */
#include "gmb_3d_internal.h"

#include <ctype.h>

/* Frame aperto: RENDER dall'ultimo PRESENT/RESET, e il suo massimo. */
static unsigned long g_renders_frame;
static unsigned long g_renders_max;

void gmb_3d_stats_reset(void)
{
    g_renders_frame = 0;
    g_renders_max = 0;
}

void gmb_3d_stats_note_render(void)
{
    g_renders_frame++;
    if (g_renders_frame > g_renders_max) g_renders_max = g_renders_frame;
}

void gmb_3d_stats_note_present(void)
{
    g_renders_frame = 0;
}

/* Confronto ASCII case-insensitive: BASIC non distingue maiuscole, quindi
 * `STATS("TRIS")` deve valere `"tris"` (come per ogni nome del linguaggio). */
static int stats_key_is(const char* name, const char* key)
{
    for (; *name && *key; name++, key++)
        if (tolower((unsigned char)*name) != tolower((unsigned char)*key)) return 0;
    return *name == *key;
}

double gmb_scene_stats(const char* name)
{
    if (name == NULL) return -1.0;

    /* Ogni chiave è la lettura che già esiste (M-STATS): nessun secondo calcolo
     * da tenere allineato a quello che SCENE.TRIS/DRAWN/CULLED/PIXELS mostrano. */
    if (stats_key_is(name, "tris"))        return (double)gmb_scene_tris();
    if (stats_key_is(name, "draw"))        return (double)gmb_scene_drawn();
    if (stats_key_is(name, "culled"))      return (double)gmb_scene_culled();
    if (stats_key_is(name, "pixels"))      return (double)gmb_scene_pixels();
    if (stats_key_is(name, "objs"))        return (double)gmb_3d_scene_geom()->nobj;
    if (stats_key_is(name, "bytes"))       return gmb_3d_scene_bytes();
    if (stats_key_is(name, "allocs"))      return (double)gmb_3d_alloc_count();
    if (stats_key_is(name, "renders"))     return (double)g_renders_frame;
    if (stats_key_is(name, "renders_max")) return (double)g_renders_max;
    return -1.0;
}
