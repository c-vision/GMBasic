/* ============================================================================
 * gmb_tui_repl.c — gmb_tui_repl. Porting del runtime C (specchio ClipperLib).
 * File per-modulo: piccolo, verificabile e committabile singolarmente.
 * ============================================================================ */
#include "gmb_tui_widgets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ===================== Repl ===================== */

void gmb_repl_init(gmb_repl_t* r, const char* prompt)
{
    memset(r, 0, sizeof(*r));
    snprintf(r->prompt, sizeof(r->prompt), "%s", prompt ? prompt : "> ");
    r->history_count = 0;
}

void gmb_repl_register(gmb_repl_t* r, const char* name, const char* description, gmb_repl_handler_t handler)
{
    if (r->commands_count >= GMB_REPL_MAX_CMDS) return;
    gmb_repl_cmd_t* c = &r->commands[r->commands_count++];
    snprintf(c->name, sizeof(c->name), "%s", name);
    snprintf(c->description, sizeof(c->description), "%s", description ? description : "");
    c->handler = handler;
}

void gmb_repl_register_completer(gmb_repl_t* r, gmb_repl_completer_t c, void* userdata)
{
    r->completer = c;
    r->completer_userdata = userdata;
}

static bool gmb_repl_has_word_sep(char c) { return c == ' ' || c == '\t'; }

static int gmb_repl_commands_starting_with(const gmb_repl_t* r, const char* prefix, char out[][256], int max)
{
    int n = 0;
    int plen = (int)strlen(prefix);
    for (int i = 0; i < r->commands_count && n < max; i++)
    {
        if (strncmp(r->commands[i].name, prefix, (size_t)plen) == 0)
            snprintf(out[n++], 256, "%s", r->commands[i].name);
    }
    return n;
}

int gmb_repl_complete(gmb_repl_t* r, const char* line, int pos, char out[][256], int max)
{
    int start = -1;
    for (int i = 0; i < pos && i < (int)strlen(line); i++)
        if (gmb_repl_has_word_sep(line[i])) start = i;
    int word_start = start < 0 ? 0 : start + 1;
    if (pos - word_start <= 0) return 0;

    int word_index = 0;
    for (int i = 0; i < pos && i < (int)strlen(line); i++)
        if (gmb_repl_has_word_sep(line[i])) word_index++;
    if (word_index == 0)
    {
        char prefix[256];
        int plen = pos - word_start;
        if (plen >= 256) plen = 255;
        memcpy(prefix, line + word_start, (size_t)plen);
        prefix[plen] = 0;
        return gmb_repl_commands_starting_with(r, prefix, out, max);
    }
    if (r->completer)
    {
        int count = 0;
        r->completer(line, pos, out, &count, max, r->completer_userdata);
        return count;
    }
    return 0;
}

int gmb_repl_history_add(gmb_repl_t* r, const char* line)
{
    if (r->history_count >= GMB_REPL_MAX_HISTORY) return -1;
    snprintf(r->history[r->history_count], 256, "%s", line);
    return r->history_count++;
}

int gmb_repl_history_prev(gmb_repl_t* r, const char* current, char* out, int cap)
{
    if (r->history_count == 0) return -1;
    int idx = -1;
    for (int i = r->history_count - 1; i >= 0; i--)
    {
        if (strcmp(r->history[i], current) == 0) { idx = i; break; }
    }
    idx = (idx < 0) ? r->history_count - 1 : idx - 1;
    if (idx < 0) return -1;
    snprintf(out, (size_t)cap, "%s", r->history[idx]);
    return idx;
}

int gmb_repl_history_next(gmb_repl_t* r, char* out, int cap)
{
    if (r->history_count == 0) return -1;
    snprintf(out, (size_t)cap, "");
    return r->history_count;
}

bool gmb_repl_save_history(const gmb_repl_t* r, const char* path)
{
    FILE* f = fopen(path, "w");
    if (!f) return false;
    for (int i = 0; i < r->history_count; i++)
        fprintf(f, "%s\n", r->history[i]);
    fclose(f);
    return true;
}

int gmb_repl_load_history(gmb_repl_t* r, const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int n = 0;
    while (fgets(line, sizeof(line), f) && n < GMB_REPL_MAX_HISTORY)
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        snprintf(r->history[n++], 256, "%s", line);
    }
    r->history_count = n;
    fclose(f);
    return n;
}

bool gmb_repl_dispatch(gmb_repl_t* r, const char* line)
{
    char cmd[64];
    const char* sp = strchr(line, ' ');
    if (!sp) sp = strchr(line, '\t');
    size_t n = sp ? (size_t)(sp - line) : strlen(line);
    if (n >= sizeof(cmd)) n = sizeof(cmd) - 1;
    memcpy(cmd, line, n);
    cmd[n] = 0;
    for (int i = 0; i < r->commands_count; i++)
    {
        if (strcmp(r->commands[i].name, cmd) == 0)
        {
            if (r->commands[i].handler)
                r->commands[i].handler(line, r->userdata);
            return true;
        }
    }
    return false;
}
