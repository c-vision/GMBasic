/* gmb_tui_datetimeinput.h — DateTimeInput.cs (specchio ClipperLib/Inputs). */
#ifndef GMB_TUI_DATETIMEINPUT_H
#define GMB_TUI_DATETIMEINPUT_H
#include "gmb_tui_datatypes.h"
#ifdef __cplusplus
extern "C" {
#endif
int gmb_datetime_parse_long_date(const char* date, char* out, int cap);
int gmb_datetime_parse_short_date(const char* date, char* out, int cap);
int gmb_datetime_parse_date_and_max_len(const char* date, bool long_date, char* out, int cap, int* max_len);
int gmb_datetime_now_string(const char* format, char* out, int cap);
#ifdef __cplusplus
}
#endif
#endif
