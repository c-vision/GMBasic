/* ============================================================================
 * gmb_tui_dateformat.c — Globals.cs DateFormat (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_dateformat.h"
#include <stdio.h>
#include <string.h>

int gmb_input_dateformat_template(int f, bool century, char* out, int cap)
{
    const char* t;
    switch (f)
    {
        case GMB_DF_ITALIAN: t = century ? "dd-mm-yyyy" : "dd-mm-yy"; break;
        case GMB_DF_ANSI: t = century ? "yyyy.mm.dd" : "yy.mm.dd"; break;
        case GMB_DF_AMERICAN: t = century ? "mm/dd/yyyy" : "mm/dd/yy"; break;
        case GMB_DF_BRITISH: case GMB_DF_FRENCH: t = century ? "dd/mm/yyyy" : "dd/mm/yy"; break;
        case GMB_DF_USA: t = century ? "dd-mm-yyyy" : "dd-mm-yy"; break;
        case GMB_DF_GERMAN: t = century ? "mm-dd-yyyy" : "mm-dd-yy"; break;
        case GMB_DF_JAPAN: t = century ? "yyyy.mm.dd" : "yy.mm.dd"; break;
        default: t = "dd-mm-yyyy"; break;
    }
    snprintf(out, cap, "%s", t);
    return (int)strlen(out);
}

int gmb_input_dateformat_chrono(int f, bool century, char* out, int cap)
{
    const char* t;
    switch (f)
    {
        case GMB_DF_ITALIAN: t = century ? "%d-%m-%Y" : "%d-%m-%y"; break;
        case GMB_DF_ANSI: t = century ? "%Y.%m.%d" : "%y.%m.%d"; break;
        case GMB_DF_AMERICAN: t = century ? "%m/%d/%Y" : "%m/%d/%y"; break;
        case GMB_DF_BRITISH: case GMB_DF_FRENCH: t = century ? "%d/%m/%Y" : "%d/%m/%y"; break;
        case GMB_DF_USA: t = century ? "%d-%m-%Y" : "%d-%m-%y"; break;
        case GMB_DF_GERMAN: t = century ? "%m-%d-%Y" : "%m-%d-%y"; break;
        case GMB_DF_JAPAN: t = century ? "%Y.%m.%d" : "%y.%m.%d"; break;
        default: t = "%d-%m-%Y"; break;
    }
    snprintf(out, cap, "%s", t);
    return (int)strlen(out);
}

char gmb_input_dateformat_separator(int f)
{
    switch (f)
    {
        case GMB_DF_ITALIAN: case GMB_DF_USA: case GMB_DF_GERMAN: return '-';
        case GMB_DF_ANSI: case GMB_DF_JAPAN: return '.';
        default: return '/';
    }
}
