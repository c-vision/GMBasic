/* gmb_ui_gfx.c — tabella dell'interfaccia grafica (core). Vuota finche' il modulo UI
 * non la installa; in headless la pagina grafica non esiste (stato reale, non stub). */
#include <string.h>
#include "gmb_ui_gfx.h"

gmb_ui_gfx_iface_t gmb_ui_gfx;

void gmb_ui_gfx_install(const gmb_ui_gfx_iface_t* iface) {
    if (iface) gmb_ui_gfx = *iface;
    else memset(&gmb_ui_gfx, 0, sizeof gmb_ui_gfx);
}
