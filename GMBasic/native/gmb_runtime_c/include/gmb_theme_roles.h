/* ============================================================================
 * gmb_theme_roles.h — RUOLI SEMANTICI del tema + contrasto WCAG (condiviso
 * VM/C + C99Native, percorso TEXT/TUI e percorso GRAPHICS/PIXEL).
 *
 * Il programma dichiara COSA è un controllo, non che colore ha:
 *
 *     BUTTON ... STYLE "role=danger"          ' distruttivo
 *     BUTTON ... STYLE "role=primary"         ' azione principale
 *     LABEL  ... STYLE "role=muted"           ' testo tenue
 *
 * Il colore lo decide il tema, con la cascata di un design system:
 *
 *     override espliciti del controllo (fg=/bg=/focuscolor=/selcolor=)
 *        >  ruolo + stato  >  tema (preset o cambiato a runtime)
 *
 * Un controllo con ruolo segue il CAMBIO TEMA anche a runtime: nel percorso
 * TEXT il colore è risolto a ogni disegno (gmb_control_style_effective), non
 * congelato al momento in cui è stato applicato lo STYLE.
 *
 * Contrasto: i colori di testo di un ruolo sono garantiti almeno AA (4.5:1)
 * sul proprio sfondo (gmb_theme_ensure_contrast). È la ragione per cui i ruoli
 * esistono: un `role=danger` leggibile su ogni preset, chiaro o scuro.
 * ============================================================================ */
#ifndef GMB_THEME_ROLES_H
#define GMB_THEME_ROLES_H

#include "gmb_tui_widgets.h"   /* gmb_theme_t, gmb_control_style_t, gmb_widget_style_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Ruolo semantico (`role=` / `tone=` nello STYLE). 0 = nessun ruolo: il
 * controllo si comporta esattamente come prima di questo modulo. */
enum {
    GMB_ROLE_NONE = 0,
    GMB_ROLE_DEFAULT,     /* superficie neutra: dialog/finestra */
    GMB_ROLE_PRIMARY,     /* azione principale: accento del tema */
    GMB_ROLE_SECONDARY,   /* azione secondaria: colore di selezione del tema */
    GMB_ROLE_ACCENT,      /* evidenza: accento/titolo */
    GMB_ROLE_SUCCESS,     /* esito positivo */
    GMB_ROLE_WARNING,     /* attenzione */
    GMB_ROLE_DANGER,      /* errore / azione distruttiva */
    GMB_ROLE_INFO,        /* informazione */
    GMB_ROLE_MUTED        /* disattivato / secondario tenue */
};

/* Stato del controllo (secondo livello della cascata). */
enum {
    GMB_STATE_NORMAL = 0,
    GMB_STATE_HOVER,
    GMB_STATE_PRESSED,
    GMB_STATE_DISABLED,
    GMB_STATE_SELECTED,
    GMB_STATE_FOCUSED
};

/* Parte del controllo a cui si chiede il colore. */
enum {
    GMB_PART_SURFACE = 0,   /* sfondo */
    GMB_PART_TEXT,          /* inchiostro (garantito AA sullo sfondo) */
    GMB_PART_BORDER,        /* bordo */
    GMB_PART_FOCUS,         /* anello di focus */
    GMB_PART_ACCENT         /* tinta pura del ruolo, indipendente dallo stato */
};

/* Parse dei nomi (case-insensitive). Ritornano la costante oppure -1. */
int gmb_theme_role_parse(const char* name);
int gmb_theme_state_parse(const char* name);
int gmb_theme_part_parse(const char* name);
/* Nome canonico del ruolo ("" per NONE/sconosciuto): per inspector e messaggi. */
const char* gmb_theme_role_name(int role);

/* Colore di (ruolo, stato, parte) derivato dal tema. Funzione PURA: nessuno
 * stato globale, nessuna allocazione. */
gmb_cell_color_t gmb_theme_role_color(const gmb_theme_t* t, int role, int state, int part);

/* Percorso GRAPHICS/PIXEL: applica il ruolo allo stile widget (superficie,
 * testo, bordo, accento, focus) conservando forma/ombre/gradienti scelti. */
void gmb_theme_apply_role_px(gmb_widget_style_t* s, const gmb_theme_t* t, int role);

/* ---- Contrasto (WCAG 2.1) ------------------------------------------------
 * Luminanza relativa di un colore (0..1) e rapporto di contrasto (1..21).
 * `gmb_theme_ensure_contrast` avvicina `fg` al polo (bianco o nero) più leggibile
 * su `bg` finché il rapporto arriva a `min_ratio` (4.5 = AA testo normale,
 * 3.0 = AA testo grande e bordi, 7.0 = AAA). Il rapporto di un polo è sempre
 * >= ~4.58 su qualunque sfondo, quindi AA è raggiungibile sempre; una soglia
 * più alta (AAA) non lo è su sfondi di luminanza media e in quel caso la
 * funzione ritorna il polo migliore (nessun errore: è un limite fisico del
 * colore, non un bug). `gmb_theme_text_on` sceglie direttamente il polo. */
double gmb_theme_luminance(gmb_cell_color_t c);
double gmb_theme_contrast_ratio(gmb_cell_color_t a, gmb_cell_color_t b);
gmb_cell_color_t gmb_theme_ensure_contrast(gmb_cell_color_t fg, gmb_cell_color_t bg, double min_ratio);
gmb_cell_color_t gmb_theme_text_on(gmb_cell_color_t bg);

/* Converte un gmb_cell_color_t in 0xAARRGGBB (per il percorso pixel). */
unsigned int gmb_theme_rgba(gmb_cell_color_t c, unsigned int alpha);

#ifdef __cplusplus
}
#endif
#endif /* GMB_THEME_ROLES_H */
