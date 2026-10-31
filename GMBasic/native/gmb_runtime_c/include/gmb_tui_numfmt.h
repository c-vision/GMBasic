/* ============================================================================
 * gmb_tui_numfmt.h — NumberFormat (porting di ClipperLib/Datatypes NumbersEx.cs
 * classe NumberFormat / numfmt). Format-spec stile Python:
 * [[fill]align][sign][symbol][0][width][,][.precision][type].
 * ============================================================================ */
#ifndef GMB_TUI_NUMFMT_H
#define GMB_TUI_NUMFMT_H

#include "gmb_tui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gmb_number_format {
    char decimal_sep;      /* default '.' */
    char group_delimiter;  /* default ',' */
} gmb_number_format_t;

void gmb_numfmt_init(gmb_number_format_t* nf, char decimal_sep, char group_delimiter);

/* Format(value, pattern): ritorna la lunghezza scritta in out. */
int gmb_numfmt_format(const gmb_number_format_t* nf, const char* pattern, double value,
                      char* out, int cap);

#ifdef __cplusplus
}
#endif

#endif /* GMB_TUI_NUMFMT_H */
