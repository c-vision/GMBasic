/* ============================================================================
 * gmb_tui_pathcompleter.c — LineEditor.cs PathCompleter (specchio ClipperLib/Inputs).
 * ============================================================================ */
#include "gmb_tui_pathcompleter.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>

static void pc_current_token(const char* line, int pos, char* out, int cap)
{
    int end = (int)strlen(line);
    if (pos < end) end = pos;
    int start = end;
    while (start > 0 && !isspace((unsigned char)line[start - 1])) start--;
    int len = end - start;
    if (len >= cap) len = cap - 1;
    memcpy(out, line + start, (size_t)len);
    out[len] = '\0';
}

void gmb_path_completer_complete(const char* text, int pos, gmb_path_completer_result_t* out)
{
    memset(out, 0, sizeof(*out));
    if (!text) return;
    char token[512];
    pc_current_token(text, pos, token, (int)sizeof(token));
    out->replacement_start = pos - (int)strlen(token);
    out->replacement_len = (int)strlen(token);
    if (token[0] == 0) return;

    char dir[512];
    char prefix[256];
    int tlen = (int)strlen(token);
    if (token[tlen - 1] == '/' || token[tlen - 1] == '\\')
    {
        snprintf(dir, sizeof(dir), "%s", token);
        prefix[0] = '\0';
    }
    else
    {
        const char* last_sep = NULL;
        for (const char* p = token; *p; p++)
            if (*p == '/' || *p == '\\') last_sep = p;
        if (last_sep)
        {
            int dlen = (int)(last_sep - token) + 1;
            snprintf(dir, sizeof(dir), "%.*s", dlen, token);
            snprintf(prefix, sizeof(prefix), "%s", last_sep + 1);
        }
        else
        {
            snprintf(dir, sizeof(dir), "");
            snprintf(prefix, sizeof(prefix), "%s", token);
        }
    }

    const char* base = dir[0] ? dir : ".";
    DIR* d = opendir(base);
    if (!d) return;
    struct dirent* de;
    int plen = (int)strlen(prefix);
    while ((de = readdir(d)) != NULL && out->count < GMB_PC_MAX_COMPLETIONS)
    {
        if (strncasecmp(de->d_name, prefix, (size_t)plen) != 0) continue;
        char cand[256];
        snprintf(cand, sizeof(cand), "%s%s", dir, de->d_name);
        bool is_dir = de->d_type == DT_DIR;
        if (is_dir) strncat(cand, "/", sizeof(cand) - strlen(cand) - 1);
        snprintf(out->completions[out->count++], 256, "%s", cand);
    }
    closedir(d);
    for (int i = 1; i < out->count; i++)
    {
        for (int j = i; j > 0 && strcasecmp(out->completions[j - 1], out->completions[j]) > 0; j--)
        {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s", out->completions[j - 1]);
            snprintf(out->completions[j - 1], 256, "%s", out->completions[j]);
            snprintf(out->completions[j], 256, "%s", tmp);
        }
    }
}
