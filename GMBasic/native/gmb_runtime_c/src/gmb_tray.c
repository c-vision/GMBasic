/* gmb_tray.c — dispatch OS + CODA main-thread per la libreria tray (vedi include/gmb_tray.h).
 *
 * Stesso pattern SDL di GMBasic (CommonGraphics/Commands.cs + PCodeVmRunner):
 * le chiamate UI arrivano dai WORKER (BASIC/GPM) e vengono ACCODATE; il MAIN THREAD
 * le esegue chiamando gmb_tray_poll() nel suo loop (come PollEvents/Present).
 * Così AppKit (NSStatusItem) e Win32 (Shell_NotifyIcon) restano sul main thread.
 *
 * - create/item/sep/tooltip/remove: accodano e ritornano subito (fire-and-forget).
 * - poll: da chiamare sul MAIN THREAD; drena la coda ed esegue sull'OS layer.
 * - selftest: stato aggiornato dal poll (non tocca l'OS).
 */
#include "gmb_tray.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define TRAY_LOCK_INIT()  InitializeCriticalSection(&g_qm)
#define TRAY_LOCK()       EnterCriticalSection(&g_qm)
#define TRAY_UNLOCK()     LeaveCriticalSection(&g_qm)
static CRITICAL_SECTION g_qm;
#else
#include <pthread.h>
#define TRAY_LOCK_INIT()  pthread_mutex_init(&g_qm, NULL)
#define TRAY_LOCK()       pthread_mutex_lock(&g_qm)
#define TRAY_UNLOCK()     pthread_mutex_unlock(&g_qm)
static pthread_mutex_t g_qm = PTHREAD_MUTEX_INITIALIZER;
#endif

#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
int  gmb_tray_macos_available(void);
int  gmb_tray_macos_is_main(void);
int  gmb_tray_macos_create(const char *, const char *);
int  gmb_tray_macos_item(int, const char *);
int  gmb_tray_macos_sep(void);
int  gmb_tray_macos_tooltip(const char *);
void gmb_tray_macos_callback(gmb_tray_cb, void *);
int  gmb_tray_macos_poll(void);
void gmb_tray_macos_remove(void);
const char *gmb_tray_macos_backend(void);
int  gmb_tray_macos_selftest(void);
#elif defined(_WIN32)
int  gmb_tray_win_available(void);
int gmb_tray_win_is_main(void);
int  gmb_tray_win_create(const char *, const char *);
int  gmb_tray_win_item(int, const char *);
int  gmb_tray_win_sep(void);
int  gmb_tray_win_tooltip(const char *);
void gmb_tray_win_callback(gmb_tray_cb, void *);
int  gmb_tray_win_poll(void);
void gmb_tray_win_remove(void);
const char *gmb_tray_win_backend(void);
int  gmb_tray_win_selftest(void);
#endif

/* --- coda comandi (thread-safe) ------------------------------------------- */
enum { OP_CREATE = 1, OP_ITEM, OP_SEP, OP_TOOLTIP, OP_REMOVE };
typedef struct { int op; int id; char *s0; char *s1; } tray_cmd_t;
#define TRAY_QCAP 256
static tray_cmd_t g_q[TRAY_QCAP];
static int g_qh = 0, g_qt = 0, g_qcount = 0;
static int g_initialized = 0;
static int g_os_created = 0;   /* aggiornato dal poll sul main thread */
static gmb_tray_cb g_cb = NULL;
static void *g_user = NULL;
static int g_last_id = -1;

static void tray_init_once(void)
{
    if (!g_initialized) { TRAY_LOCK_INIT(); g_initialized = 1; }
}

static char *tray_strdup(const char *s) { return s ? strdup(s) : NULL; }

static void tray_enqueue(int op, int id, const char *s0, const char *s1)
{
    tray_init_once();
    TRAY_LOCK();
    if (g_qcount < TRAY_QCAP) {
        int i = g_qt;
        g_q[i].op = op; g_q[i].id = id;
        g_q[i].s0 = tray_strdup(s0); g_q[i].s1 = tray_strdup(s1);
        g_qt = (g_qt + 1) % TRAY_QCAP; g_qcount++;
    }
    TRAY_UNLOCK();
}

static void tray_free_cmd(tray_cmd_t *c) { free(c->s0); free(c->s1); c->s0 = c->s1 = NULL; }

/* --- API pubblica ---------------------------------------------------------- */
#if (defined(__APPLE__) && defined(GMB_TRAY_COCOA)) || defined(_WIN32)
static void tray_internal_cb(int item_id, void *user);   /* registrata all'OS: aggiorna g_last_id */
#endif
int gmb_tray_available(void)
{
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
    return gmb_tray_macos_available();
#elif defined(_WIN32)
    return gmb_tray_win_available();
#else
    return 0; /* Linux SNI: da implementare */
#endif
}

const char *gmb_tray_backend(void)
{
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
    return gmb_tray_macos_backend();
#elif defined(_WIN32)
    return gmb_tray_win_backend();
#else
    return "none";
#endif
}

int gmb_tray_create(const char *icon_path, const char *tooltip)
{
    /* Registra la callback interna con l'OS: senza, i click del menu non arrivano
     * mai a g_last_id (bug: "Esci" non funzionava). La callback utente resta opzionale. */
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
    gmb_tray_macos_callback(tray_internal_cb, NULL);
#elif defined(_WIN32)
    gmb_tray_win_callback(tray_internal_cb, NULL);
#endif
    tray_enqueue(OP_CREATE, 0, icon_path, tooltip);
    return 0;
}

int gmb_tray_item(int id, const char *label)
{
    tray_enqueue(OP_ITEM, id, label, NULL);
    return 0;
}

int gmb_tray_sep(void)
{
    tray_enqueue(OP_SEP, 0, NULL, NULL);
    return 0;
}

int gmb_tray_tooltip(const char *text)
{
    tray_enqueue(OP_TOOLTIP, 0, text, NULL);
    return 0;
}

/* Callback interna registrata all'OS: registra l'id (read-and-clear) e inoltra. */
#if (defined(__APPLE__) && defined(GMB_TRAY_COCOA)) || defined(_WIN32)
static void tray_internal_cb(int item_id, void *user)
{
    g_last_id = item_id;
    if (g_cb) g_cb(item_id, g_user);
    (void)user;
}
#endif

int gmb_tray_last(void)
{
    int v = g_last_id;
    g_last_id = -1;
    return v;
}

void gmb_tray_set_callback(gmb_tray_cb cb, void *user)
{
    g_cb = cb; g_user = user;
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
    gmb_tray_macos_callback(tray_internal_cb, user);
#elif defined(_WIN32)
    gmb_tray_win_callback(tray_internal_cb, user);
#endif
}

/* Drena la coda eseguendo i comandi sull'OS layer: chiamare SOLO sul MAIN THREAD
 * (come GraphicsManager.Present()/PollEvents nel loop dell'host). */
static int tray_os_is_main(void)
{
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
    return gmb_tray_macos_is_main();
#elif defined(_WIN32)
    return gmb_tray_win_is_main();
#else
    return 1;
#endif
}

int gmb_tray_poll(void)
{
    int n = 0;
    tray_init_once();
    /* CONTRATTO (2026-11-13): la MACCHINA A STATI si drena su QUALUNQUE thread; gli EFFETTI
     * sull'OS (AppKit/Win32) solo sul MAIN thread. Prima si usciva subito fuori dal main thread:
     * negli host senza main loop (harness xUnit, processi headless) la coda non veniva MAI
     * drenata e `TRAYSELFTEST()` restava 0 mentre su C99/VM-C (main thread) era -1 → il
     * MiniTest `tray/tray_parity` non era confrontabile fra le gambe (divergenza di HOST).
     * L'invariante "AppKit solo sul main thread" resta: fuori dal main non si chiama l'OS layer. */
    int on_main = tray_os_is_main();
    for (;;) {
        tray_cmd_t c;
        TRAY_LOCK();
        if (g_qcount == 0) { TRAY_UNLOCK(); break; }
        c = g_q[g_qh];
        g_qh = (g_qh + 1) % TRAY_QCAP; g_qcount--;
        TRAY_UNLOCK();

        switch (c.op) {
        case OP_CREATE:
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
            if (on_main) gmb_tray_macos_create(c.s0, c.s1);
#elif defined(_WIN32)
            if (on_main) gmb_tray_win_create(c.s0, c.s1);
#endif
            g_os_created = 1;
            break;
        case OP_ITEM:
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
            if (on_main) gmb_tray_macos_item(c.id, c.s0);
#elif defined(_WIN32)
            if (on_main) gmb_tray_win_item(c.id, c.s0);
#endif
            break;
        case OP_SEP:
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
            if (on_main) gmb_tray_macos_sep();
#elif defined(_WIN32)
            if (on_main) gmb_tray_win_sep();
#endif
            break;
        case OP_TOOLTIP:
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
            if (on_main) gmb_tray_macos_tooltip(c.s0);
#elif defined(_WIN32)
            if (on_main) gmb_tray_win_tooltip(c.s0);
#endif
            break;
        case OP_REMOVE:
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
            if (on_main) gmb_tray_macos_remove();
#elif defined(_WIN32)
            if (on_main) gmb_tray_win_remove();
#endif
            g_os_created = 0;
            break;
        default: break;
        }
        tray_free_cmd(&c);
        n++;
    }
    /* Pompa anche gli eventi OS pendenti (menu/click): solo sul main thread. */
    if (on_main) {
#if defined(__APPLE__) && defined(GMB_TRAY_COCOA)
        n += gmb_tray_macos_poll();
#elif defined(_WIN32)
        n += gmb_tray_win_poll();
#endif
    }
    return n;
}

void gmb_tray_remove(void)
{
    tray_enqueue(OP_REMOVE, 0, NULL, NULL);
}

int gmb_tray_selftest(void)
{
    return g_os_created;
}
