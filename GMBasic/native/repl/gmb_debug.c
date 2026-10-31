/* native/repl/gmb_debug.c — implementazione sessione di debug della ReplNative (C). */
#include "gmb_debug.h"
#include <string.h>
#include <stdio.h>

void gmb_debug_init(gmb_debug_session_t* s)
{
    memset(s, 0, sizeof(*s));
    s->mode = GMB_DBG_OFF;
    s->trace = false;
}

int gmb_debug_add_break(gmb_debug_session_t* s, int line)
{
    if (line <= 0) return -1;
    for (int i = 0; i < s->break_count; i++)
        if (s->breaks[i].line == line) { s->breaks[i].enabled = true; return i; }
    if (s->break_count >= GMB_DBG_MAX_BREAKS) return -1;
    s->breaks[s->break_count].line = line;
    s->breaks[s->break_count].enabled = true;
    s->breaks[s->break_count].hits = 0;
    return s->break_count++;
}

int gmb_debug_clear_break(gmb_debug_session_t* s, int line)
{
    for (int i = 0; i < s->break_count; i++)
        if (s->breaks[i].line == line)
        {
            for (int k = i; k < s->break_count - 1; k++) s->breaks[k] = s->breaks[k + 1];
            s->break_count--;
            return 0;
        }
    return -1;
}

int gmb_debug_clear_all_breaks(gmb_debug_session_t* s)
{
    s->break_count = 0;
    return 0;
}

int gmb_debug_add_watch(gmb_debug_session_t* s, const char* var)
{
    if (!var || !*var) return -1;
    for (int i = 0; i < s->watch_count; i++)
        if (strcmp(s->watches[i].name, var) == 0) return i;
    if (s->watch_count >= GMB_DBG_MAX_WATCH) return -1;
    snprintf(s->watches[s->watch_count].name, sizeof(s->watches[s->watch_count].name), "%s", var);
    return s->watch_count++;
}

int gmb_debug_clear_watch(gmb_debug_session_t* s, const char* var)
{
    for (int i = 0; i < s->watch_count; i++)
        if (strcmp(s->watches[i].name, var) == 0)
        {
            for (int k = i; k < s->watch_count - 1; k++) s->watches[k] = s->watches[k + 1];
            s->watch_count--;
            return 0;
        }
    return -1;
}

bool gmb_debug_has_break(const gmb_debug_session_t* s, int line)
{
    for (int i = 0; i < s->break_count; i++)
        if (s->breaks[i].line == line && s->breaks[i].enabled) return true;
    return false;
}

void gmb_debug_set_line(gmb_debug_session_t* s, int line)
{
    s->current_line = line;
    s->at_breakpoint = gmb_debug_has_break(s, line);
    if (s->at_breakpoint)
        for (int i = 0; i < s->break_count; i++)
            if (s->breaks[i].line == line) s->breaks[i].hits++;
}

bool gmb_debug_step_requested(const gmb_debug_session_t* s)
{
    return s->mode == GMB_DBG_STEPPING;
}
