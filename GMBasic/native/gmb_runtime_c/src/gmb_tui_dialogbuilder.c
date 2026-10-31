/* ============================================================================
 * gmb_tui_dialogbuilder.c — gmb_tui_dialogbuilder. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void gmb_question_init(gmb_question_t* q, const char* name, int kind)
{
    memset(q, 0, sizeof(*q));
    if (name) snprintf(q->name, sizeof(q->name), "%s", name);
    q->kind = kind;
    snprintf(q->message, sizeof(q->message), "%s: ", name ? name : "");
}
void gmb_question_add_choice(gmb_question_t* q, const char* text)
{
    if (q->choices_count >= 16) return;
    snprintf(q->choices[q->choices_count], 256, "%s", text ? text : "");
    q->choices_sep[q->choices_count] = false;
    q->choices_count++;
}
void gmb_question_add_choice_checked(gmb_question_t* q, const char* text, bool checked_)
{
    if (q->multi_count >= 16) return;
    snprintf(q->multi_texts[q->multi_count], 256, "%s", text ? text : "");
    q->multi_checked[q->multi_count] = checked_;
    q->multi_count++;
}
void gmb_question_add_expand_choice(gmb_question_t* q, char key, const char* text)
{
    if (q->expand_count >= 8) return;
    q->expand_keys[q->expand_count] = key;
    snprintf(q->expand_texts[q->expand_count], 256, "%s", text ? text : "");
    q->expand_count++;
}
int gmb_question_resolve(gmb_question_t* q, const char* raw, gmb_answer_t* out)
{
    memset(out, 0, sizeof(*out));
    if (!raw) return 0;
    if (q->kind == GMB_QK_INPUT || q->kind == GMB_QK_PASSWORD)
    {
        if (!raw[0]) { snprintf(out->str, sizeof(out->str), "%s", q->default_str); }
        else snprintf(out->str, sizeof(out->str), "%s", raw);
        out->has_string = true;
        return 1;
    }
    if (q->kind == GMB_QK_CONFIRM)
    {
        if (strcmp(raw, "y") == 0 || strcmp(raw, "yes") == 0) { out->has_bool = true; out->bool_ = true; return 1; }
        if (strcmp(raw, "n") == 0 || strcmp(raw, "no") == 0) { out->has_bool = true; out->bool_ = false; return 1; }
        if (!raw[0] && q->has_default_bool) { out->has_bool = true; out->bool_ = q->default_bool; return 1; }
        return 0;
    }
    if (q->kind == GMB_QK_INT)
    {
        if (!raw[0] && q->has_default_int) { out->has_int = true; out->int_ = q->default_int; return 1; }
        char* end = NULL;
        long v = strtol(raw, &end, 10);
        if (end && end != raw) { out->has_int = true; out->int_ = v; return 1; }
        return 0;
    }
    if (q->kind == GMB_QK_FLOAT)
    {
        if (!raw[0] && q->has_default_float) { out->has_float = true; out->float_ = q->default_float; return 1; }
        char* end = NULL;
        double v = strtod(raw, &end);
        if (end && end != raw) { out->has_float = true; out->float_ = v; return 1; }
        return 0;
    }
    if (q->kind == GMB_QK_SELECT)
    {
        for (int i = 0; i < q->choices_count; i++)
            if (strcmp(q->choices[i], raw) == 0) { out->has_int = true; out->int_ = i; out->has_string = true; snprintf(out->str, sizeof(out->str), "%s", raw); return 1; }
        return 0;
    }
    if (q->kind == GMB_QK_MULTISELECT)
    {
        char copy[512];
        snprintf(copy, sizeof(copy), "%s", raw);
        char* save = NULL;
        for (char* tok = strtok_r(copy, ",", &save); tok; tok = strtok_r(NULL, ",", &save))
        {
            while (*tok == ' ') tok++;
            long idx = atol(tok);
            if (idx >= 0 && idx < q->multi_count) { out->has_int = true; out->int_ = idx; }
        }
        return out->has_int ? 1 : 0;
    }
    if (q->kind == GMB_QK_EXPAND)
    {
        char key = raw[0];
        for (int i = 0; i < q->expand_count; i++)
            if (q->expand_keys[i] == key) { out->has_bool = true; out->bool_ = true; out->has_string = true; snprintf(out->str, sizeof(out->str), "%s", q->expand_texts[i]); return 1; }
        return 0;
    }
    return 0;
}

/* ===================== Feeder ===================== */
