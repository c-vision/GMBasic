/* ============================================================================
 * gmb_form_read.h — Form Clipper pixel: SAY/GET (buffer campi).
 *
 * `@ r, c SAY ...` / `@ r, c GET var [clausole]` registrano SAY e campi.
 * Il loop modale `gmb_form_read` è stato RIMOSSO (2026-10-09): il path READ è migrato
 * al modello unificato CONTROLS+park (gmb_read_run / IlRuntime.ReadRun). Restano
 * gmb_form_clear/say/get (usati dall'emissione @SAY/@GET dei backend).
 * ========================================================================== */
#ifndef GMB_FORM_READ_H
#define GMB_FORM_READ_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

void gmb_form_clear(void);

/* @ r,c SAY <text> */
void gmb_form_say(int row, int col, const char* text);

/* @ r,c GET <var> — tipi: "text"/"checkbox"/"spinner"/"listbox"/"combobox".
 * ptr: double* (numerico/indice/bool) o gmb_string_t* (text), in base a is_string.
 * items: "a|b|c" (list/combobox). Ritorna 1 se registrato. */
void gmb_form_paint_says_px(void);
void gmb_form_cell_px(int* out_cw, int* out_ch);

int  gmb_form_get(int row, int col, void* ptr, int is_string, const char* type,
                  const char* picture, const char* items, int width, int height,
                   double vmin, double vmax, double step, const char* caption);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FORM_READ_H */
