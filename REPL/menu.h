/* native/repl/menu.h — speculare a PaginatedMenu.cs. */
#ifndef GMB_MENU_H
#define GMB_MENU_H

#include <stdbool.h>

/* Mostra un menu full-screen. Se selectable, Enter ritorna l'indice selezionato;
 * altrimenti mostra solo. ESC (o nessuna selezione) ritorna -1. */
int gmb_menu_show(const char** items, int count, const char* title, bool selectable);

#endif
