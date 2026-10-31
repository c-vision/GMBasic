/* native/repl/menu.c — speculare a PaginatedMenu.cs: browser full-screen di righe
 * con titolo, scroll, selezione (Enter) o sola visualizzazione, ESC per uscire.
 * gmb_menu_show(items, count, title, selectable) → indice selezionato o -1.
 */
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>

/* raw mode + lettura tasti (stesso approccio dell'editor full-screen) */
static int g_ttyfd = -1;
static struct termios g_orig;

static int menu_tty_open(void)
{
    g_ttyfd = open("/dev/tty", O_RDWR);
    if (g_ttyfd < 0) return -1;
    struct termios raw;
    tcgetattr(g_ttyfd, &g_orig);
    raw = g_orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    tcsetattr(g_ttyfd, TCSANOW, &raw);
    return 0;
}

static void menu_tty_close(void)
{
    if (g_ttyfd >= 0) { tcsetattr(g_ttyfd, TCSANOW, &g_orig); close(g_ttyfd); g_ttyfd = -1; }
}

static int tty_read(int ms, unsigned char* out)
{
    fd_set fds; struct timeval tv;
    FD_ZERO(&fds); FD_SET(g_ttyfd, &fds);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    if (select(g_ttyfd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    return read(g_ttyfd, out, 1) == 1 ? 0 : -1;
}

static int menu_key(void)
{
    unsigned char c;
    if (read(g_ttyfd, &c, 1) != 1) return -1;
    if (c == 27) {
        unsigned char s[2];
        if (tty_read(100, &s[0]) != 0) return 27;    /* ESC nudo */
        if (tty_read(100, &s[1]) != 0) return 27;
        if (s[0] == '[') {
            switch (s[1]) {
                case 'A': return 1;   /* up */
                case 'B': return 2;   /* down */
                case 'H': return 5;   /* home */
                case 'F': return 6;   /* end */
                case '~':
                    if (tty_read(100, &c) != 0) return -1;
                    switch (c) { case '5': return 9; case '6': return 10; }
                    return -1;
            }
        }
        if (s[0] == 'O' && s[1] == 'P') return 12;   /* F1 */
        return -1;
    }
    if (c == 127 || c == 8) return 7;  /* backspace */
    return c;
}

int gmb_menu_show(const char** items, int count, const char* title, bool selectable)
{
    if (count <= 0) return -1;
    if (menu_tty_open() != 0) return -1;
    printf("\033[?25l");
    int rows = 25, cols = 80;
    int cur = 0, top = 0, result = -1;
    bool running = true;

    while (running) {
        struct winsize ws;
        if (ioctl(g_ttyfd, TIOCGWINSZ, &ws) == 0) { rows = ws.ws_row; cols = ws.ws_col; }
        if (rows < 5) rows = 25;
        int page = rows - 3;
        if (cur < top) top = cur;
        if (cur >= top + page) top = cur - page + 1;
        if (top < 0) top = 0;
        if (top > count - page) top = count > page ? count - page : 0;

        printf("\033[2J\033[H\033[0;44m  %s  |  %d items  |  ESC=Quit\033[0m\n", title ? title : "Select", count);
        for (int i = top; i < top + page && i < count; i++) {
            if (i == cur) printf("\033[46m\033[30m> %-*.*s\033[0m\033[K\n", cols - 4, cols - 4, items[i]);
            else printf("  %-*.*s\033[K\n", cols - 4, cols - 4, items[i]);
        }
        printf("\033[0;44m  %s\033[0m\033[K", selectable ? "\xE2\x86\x91/\xE2\x86\x93 Enter=Select ESC=Quit" : "\xE2\x86\x91/\xE2\x86\x93 ESC=Quit");

        int k = menu_key();
        switch (k) {
            case 1: if (cur > 0) cur--; break;                       /* up */
            case 2: if (cur < count - 1) cur++; break;               /* down */
            case 5: cur = 0; break;                                  /* home */
            case 6: cur = count - 1; break;                          /* end */
            case 9: cur -= page; if (cur < 0) cur = 0; break;        /* pgup */
            case 10: cur += page; if (cur > count - 1) cur = count - 1; break; /* pgdn */
            case 13:                                                   /* enter */
                if (selectable) { result = cur; running = false; }
                break;
            case 27: running = false; break;                         /* esc */
            default: break;
        }
    }
    printf("\033[2J\033[H\033[?25h\033[0m");
    menu_tty_close();
    return result;
}
