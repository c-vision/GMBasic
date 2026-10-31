#ifndef VB_GMB_H
#define VB_GMB_H

#include "vb_model.h"

#include <stddef.h>

/* Generates a GMBasic `.ui.bas` module from a loaded VB6 form.
 *
 * The layout uses the pixel form API (SCREEN/WINDOW/BUTTON/TEXTBOX/...): coordinates
 * are converted from VB twips (15 per pixel), the `vb6` theme is selected, and an
 * event loop pumps the window. Event handlers are emitted as empty stubs; existing
 * VB6 code is NOT translated automatically. Caller frees the returned string. */
char *vb_gmb_form_export(const VbForm *form);

#endif
