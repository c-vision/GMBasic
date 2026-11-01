/* native/repl/progress.c — speculare a ConsoleProgressBar.cs: renderer di progresso su
 * una riga console (■ parte fatta, ░ resto), riusabile da CLI e REPL. Senza TTY si
 * disattiva (solo l'esito finale).
 */
#include "progress.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static bool g_progress_tty = false;
static char g_progress_last[256] = "";
static int g_progress_width = 40;

void gmb_progress_init(int width)
{
    g_progress_tty = isatty(1) && isatty(2);
    g_progress_width = width < 10 ? 10 : (width > 60 ? 60 : width);
    g_progress_last[0] = 0;
}

bool gmb_progress_enabled(void) { return g_progress_tty; }

static void fmt_bytes(long b, char* out, int cap)
{
    if (b >= (1L << 20)) snprintf(out, cap, "%.0fMB", (double)b / (1L << 20));
    else snprintf(out, cap, "%ldB", b);
}

void gmb_progress_report(int percent, long bytes_done, long bytes_total, const char* file)
{
    if (!g_progress_tty) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int filled = (int)((double)g_progress_width * percent / 100.0);
    char bar[128];
    int i, bi = 0;
    /* █ = E2 96 88, ░ = E2 96 91 (UTF-8 a 3 byte) */
    for (i = 0; i < filled && i < g_progress_width; i++) { bar[bi++] = 0xe2; bar[bi++] = 0x96; bar[bi++] = 0x88; }
    for (; i < g_progress_width; i++) { bar[bi++] = 0xe2; bar[bi++] = 0x96; bar[bi++] = 0x91; }
    bar[bi] = 0;
    char bd[32], bt[32];
    fmt_bytes(bytes_done, bd, sizeof(bd));
    fmt_bytes(bytes_total, bt, sizeof(bt));
    char line[256];
    snprintf(line, sizeof(line), "[%s] %3d%%  %9s/%9s  %s", bar, percent, bd, bt, file ? file : "");
    /* pad per non lasciare residui della riga precedente */
    int l = (int)strlen(line);
    if (l < (int)strlen(g_progress_last)) { for (int p = l; p < (int)strlen(g_progress_last); p++) line[p] = ' '; line[strlen(g_progress_last)] = 0; }
    printf("\r%s", line);
    fflush(stdout);
    snprintf(g_progress_last, sizeof(g_progress_last), "%s", line);
}

void gmb_progress_finish(void)
{
    if (g_progress_tty && g_progress_last[0]) printf("\n");
    g_progress_last[0] = 0;
}
