/* ============================================================================
 * gmb_tui_datetimeinput.c — DateTimeInput.cs (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_datetimeinput.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int gmb_datetime_parse_long_date(const char* date, char* out, int cap)
{
    gmb_dt_t dt;
    if (!gmb_dates_parse_flexible(date, &dt))
    {
        if (out && cap > 0) out[0] = '\0';
        return 0;
    }
    return gmb_dates_format(&dt, "dd/mm/yyyy", out, cap);
}

int gmb_datetime_parse_short_date(const char* date, char* out, int cap)
{
    gmb_dt_t dt;
    if (!gmb_dates_parse_flexible(date, &dt))
    {
        if (out && cap > 0) out[0] = '\0';
        return 0;
    }
    return gmb_dates_format(&dt, "dd/mm/yy", out, cap);
}

int gmb_datetime_parse_date_and_max_len(const char* date, bool long_date, char* out, int cap, int* max_len)
{
    if (long_date)
    {
        if (max_len) *max_len = 10;
        return gmb_datetime_parse_long_date(date, out, cap);
    }
    if (max_len) *max_len = 8;
    return gmb_datetime_parse_short_date(date, out, cap);
}

int gmb_datetime_now_string(const char* format, char* out, int cap)
{
    gmb_dt_t now;
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    if (!tm) { if (out && cap > 0) out[0] = '\0'; return 0; }
    now.year = tm->tm_year + 1900;
    now.month = tm->tm_mon + 1;
    now.day = tm->tm_mday;
    now.hour = tm->tm_hour;
    now.minute = tm->tm_min;
    now.second = tm->tm_sec;
    return gmb_dates_format(&now, format, out, cap);
}
