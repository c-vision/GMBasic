/* gmb_fonts.h — namespace FONTS: rasterizzazione e metriche di font TrueType/OpenType.
 *
 * Una sola implementazione (gmb_fonts.c) sopra lo stb_truetype gia' vendorizzato
 * (include/stb_truetype.h, implementazione in src/stb_truetype_impl.c). I backend
 * (VM/C, C99Native, IL/PCodeVM via dylib) delegano: nessuna copia dell'algoritmo.
 *
 * Convenzioni di ritorno (uguali per tutte le funzioni):
 *   0 o valore >= 0   successo
 *   -1                file illeggibile o assente
 *   -2                font non valido (intestazione/offset non riconosciuti)
 *   -3                argomento non valido (dimensione <= 0, puntatori NULL)
 *   -4                buffer di uscita troppo piccolo
 */
#ifndef GMB_FONTS_H
#define GMB_FONTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Metriche del glifo `codepoint` a `pixel_height` pixel di altezza (em).
 * out8 riceve, nell'ordine:
 *   [0] larghezza bitmap (px)        [1] altezza bitmap (px)
 *   [2] xoff (px, dal pen)           [3] yoff (px, dal baseline verso l'alto)
 *   [4] advance (px, frazionario)    [5] ascent (px)
 *   [6] descent (px, negativo)       [7] line gap (px)
 * Un codepoint non coperto dal font ritorna 0 con [0] e [1] a 0 (nessun glifo). */
int gmb_fonts_tt_metrics(const char* path, double pixel_height, int codepoint, double* out8);

/* Bitmap di copertura (alpha 0..255) del glifo `codepoint`, riga per riga dall'alto,
 * `out_w * out_h` byte, stride = out_w. `out` deve avere capacita' >= out_w*out_h.
 * out_w/out_h/xoff/yoff possono essere NULL se non servono. */
int gmb_fonts_tt_render(const char* path, double pixel_height, int codepoint,
                        unsigned char* out, int cap,
                        int* out_w, int* out_h, int* out_xoff, int* out_yoff);

/* Numero di codepoint mappati dal cmap (0..0xFFFF, surrogati esclusi). -1 su errore. */
int gmb_fonts_tt_glyph_count(const char* path);

/* Codepoint mappati (ordine crescente) in `out` (capacita' `cap`).
 * Ritorna il numero scritto (<= cap); se il buffer e' piccolo ritorna la capacita'
 * necessaria (maggiore di cap) senza scrivere oltre. -1 su errore. */
int gmb_fonts_tt_glyph_codes(const char* path, int* out, int cap);

/* Le varianti a lista (_metrics_list/_render_list/_glyphs_list) sono dichiarate in
 * gmb_runtime.h: il C generato da C99Native include quel header, non questo. */

#ifdef __cplusplus
}
#endif

#endif /* GMB_FONTS_H */
