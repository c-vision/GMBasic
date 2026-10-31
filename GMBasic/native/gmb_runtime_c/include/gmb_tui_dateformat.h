/* gmb_tui_dateformat.h — Globals.cs DateFormat (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_DATEFORMAT_H
#define GMB_TUI_DATEFORMAT_H
#include "gmb_tui.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
    GMB_DF_ANSI = 0, GMB_DF_BRITISH, GMB_DF_FRENCH, GMB_DF_GERMAN,
    GMB_DF_ITALIAN, GMB_DF_JAPAN, GMB_DF_USA, GMB_DF_AMERICAN
};
int  gmb_input_dateformat_template(int f, bool century, char* out, int cap);
int  gmb_input_dateformat_chrono(int f, bool century, char* out, int cap);
char gmb_input_dateformat_separator(int f);
#ifdef __cplusplus
}
#endif
#endif
