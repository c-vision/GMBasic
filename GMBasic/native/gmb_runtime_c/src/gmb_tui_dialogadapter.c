/* ============================================================================
 * gmb_tui_dialogadapter.c — DialogAdapter.cs (specchio ClipperLib/Controls).
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gmb_dialog_build_input(const gmb_question_t* q, char* out, int cap)
{
    snprintf(out, (size_t)cap, "%s", q->default_str);
}
bool gmb_dialog_build_confirm(const gmb_question_t* q)
{
    return q->has_default_bool && q->default_bool;
}
int gmb_dialog_build_select(const gmb_question_t* q)
{
    if (q->choices_count == 0) return 0;
    int i = q->has_default_index ? q->default_index : 0;
    if (i < 0) i = 0;
    if (i >= q->choices_count) i = q->choices_count - 1;
    return i;
}
int gmb_dialog_build_multiselect(const gmb_question_t* q)
{
    return q->has_default_index ? q->default_index : 0;
}
