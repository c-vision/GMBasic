/* gmb_keyboard_extra.c — parità common/CommonLib/Keyboard (KeyCombination, KeyboardTypes,
 * ConsoleKeyboardService, HeadlessKeyboardService).cs — flusso autonomo C (no bridge)
 * Buildato su gmb_console_sendkey/readkey (typeahead) + registry hook ON KEY.
 */
#include "gmb_keyboard_extra.h"
#include "gmb_runtime.h"
#include "gmb_tui_app.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GMB_KEY_HOOKS 32

typedef struct gmb_key_hook {
    gmb_key_combination_t combo;
    gmb_key_hook_fn fn;
    void* userdata;
    int in_use;
} gmb_key_hook_t;

static gmb_key_hook_t g_hooks[GMB_KEY_HOOKS];
static int g_paused = 0;
static int g_cancel = 0;

gmb_key_combination_t gmb_key_combination_new(int key, int modifiers)
{
    gmb_key_combination_t c;
    c.key = key;
    c.modifiers = modifiers;
    return c;
}
int gmb_key_combination_equals(gmb_key_combination_t a, gmb_key_combination_t b)
{ return a.key == b.key && a.modifiers == b.modifiers; }

/* Dispatch hook: se una combo è registrata, la invoca e ritorna 1 (tasto consumato). */
static int key_dispatch_hook(gmb_key_combination_t combo)
{
    for (int i = 0; i < GMB_KEY_HOOKS; i++)
        if (g_hooks[i].in_use && gmb_key_combination_equals(g_hooks[i].combo, combo)) {
            if (g_hooks[i].fn) g_hooks[i].fn(g_hooks[i].userdata);
            return 1;
        }
    return 0;
}

void gmb_key_register_hook(gmb_key_combination_t combo, gmb_key_hook_fn fn, void* userdata)
{
    for (int i = 0; i < GMB_KEY_HOOKS; i++)
        if (!g_hooks[i].in_use) {
            g_hooks[i].combo = combo;
            g_hooks[i].fn = fn;
            g_hooks[i].userdata = userdata;
            g_hooks[i].in_use = 1;
            return;
        }
}
void gmb_key_unregister_hook(gmb_key_combination_t combo)
{
    for (int i = 0; i < GMB_KEY_HOOKS; i++)
        if (g_hooks[i].in_use && gmb_key_combination_equals(g_hooks[i].combo, combo))
            g_hooks[i].in_use = 0;
}
void gmb_key_unregister_all_hooks(void)
{
    memset(g_hooks, 0, sizeof(g_hooks));
}
int gmb_key_is_hook_registered(gmb_key_combination_t combo)
{
    for (int i = 0; i < GMB_KEY_HOOKS; i++)
        if (g_hooks[i].in_use && gmb_key_combination_equals(g_hooks[i].combo, combo)) return 1;
    return 0;
}

static int key_to_upper(int k)
{
    if (k >= 'a' && k <= 'z') return k - 32;
    return k;
}

int gmb_key_try_read(int convert_uppercase)
{
    if (g_cancel) { g_cancel = 0; return 0; }
    if (g_paused) return 0;
    gmb_string_t s = gmb_console_readkey(0);
    if (!s.data || !*s.data) { if (s.data) gmb_string_free(&s); return 0; }
    int key = (unsigned char)s.data[0];
    gmb_string_free(&s);
    /* dispatch hook sulla combinazione base (senza modificatori in headless) */
    if (key_dispatch_hook(gmb_key_combination_new(key, 0))) return 0;
    if (convert_uppercase) key = key_to_upper(key);
    return key;
}

int gmb_key_read_key(int timeout_ms, int convert_uppercase)
{
    if (g_cancel) { g_cancel = 0; return 0; }
    if (g_paused) return 0;
    gmb_string_t s = gmb_console_readkey(timeout_ms);
    if (!s.data || !*s.data) { if (s.data) gmb_string_free(&s); return 0; }
    int key = (unsigned char)s.data[0];
    gmb_string_free(&s);
    if (key_dispatch_hook(gmb_key_combination_new(key, 0))) return 0;
    if (convert_uppercase) key = key_to_upper(key);
    return key;
}

int gmb_key_read_key_info(int timeout_ms, int convert_uppercase, gmb_terminal_key_info_t* out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (g_cancel) { g_cancel = 0; return 0; }
    if (g_paused) return 0;
    gmb_string_t s = gmb_console_readkey(timeout_ms);
    if (!s.data || !*s.data) { if (s.data) gmb_string_free(&s); return 0; }
    int key = (unsigned char)s.data[0];
    gmb_string_free(&s);
    if (convert_uppercase) key = key_to_upper(key);
    out->key_char = (char)key;
    out->key = key;
    out->shift = (key >= 'A' && key <= 'Z') ? 1 : 0;
    out->alt = 0;
    out->control = 0;
    return 1;
}

/* Line editor (KeyboardServiceConsole.cs:194-322/342-459): prompt, cursore,
 * backspace/delete/left/right/home/end + stampabili. Legge da typeahead/readkey. */
char* gmb_key_read_line(const char* prompt)
{
    if (prompt) gmb_console_out_write(prompt);
    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    int pos = 0;
    for (;;) {
        int key = gmb_key_read_key(0, 0);
        if (key == 0) break; /* timeout/nessun input → interrompi */
        if (key == GMB_KEY_ENTER || key == '\n' || key == '\r') {
            gmb_console_out_write("\n");
            break;
        }
        if (key == GMB_KEY_BACKSPACE || key == 8) {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - (size_t)pos);
                pos--; len--;
                buf[len] = 0;
                gmb_console_out_write("\b \b");
            }
            continue;
        }
        if (key == GMB_KEY_DELETE) {
            if (pos < (int)len) {
                memmove(buf + pos, buf + pos + 1, len - (size_t)pos - 1);
                len--;
                buf[len] = 0;
                gmb_console_out_write("\b \b");
            }
            continue;
        }
        if (key == GMB_KEY_LEFT) { if (pos > 0) { pos--; gmb_console_out_write("\b"); } continue; }
        if (key == GMB_KEY_RIGHT) { if (pos < (int)len) { pos++; gmb_console_out_write("\033[C"); } continue; }
        if (key == GMB_KEY_HOME) { pos = 0; gmb_console_out_write("\033[0G"); continue; }
        if (key == GMB_KEY_END) { pos = (int)len; continue; }
        if (key == 22) {   /* Ctrl+V: incolla il clipboard (parita KeyboardServiceConsole) */
            char paste[1024];
            int pn = gmb_clipboard_get_buf(paste, (int)sizeof(paste));
            for (int pi = 0; pi < pn; pi++) {
                char pc = paste[pi];
                if (pc == 13 || pc == 10) continue;
                if (len + 1 >= cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
                memmove(buf + pos + 1, buf + pos, len - (size_t)pos);
                buf[pos] = pc; pos++; len++; buf[len] = 0;
            }
            if (pn > 0) gmb_console_out_write(paste);
            continue;
        }
        if (key == GMB_KEY_ESC) { free(buf); return NULL; }
        if (key >= 32 && key < 127) {
            if (len + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); if (!buf) return NULL; }
            memmove(buf + pos + 1, buf + pos, len - (size_t)pos);
            buf[pos] = (char)key;
            pos++; len++;
            buf[len] = 0;
            char c = (char)key;
            gmb_console_out_write(&c);
        }
    }
    buf[len] = 0;
    return buf;
}

void gmb_key_send_keys(const char* keys)
{
    if (!keys) return;
    gmb_console_sendkey(keys);
}
void gmb_key_send_key(char key)
{
    char s[2] = { key, 0 };
    gmb_console_sendkey(s);
}

void gmb_key_clear_buffer(void) { gmb_tui_global_typeahead_clear(); }
void gmb_key_reset(void) { gmb_tui_global_typeahead_clear(); g_paused = 0; g_cancel = 0; }
void gmb_key_cancel_pending_reads(void) { g_cancel = 1; }
void gmb_key_pause(void) { g_paused = 1; }
void gmb_key_resume(void) { g_paused = 0; }
