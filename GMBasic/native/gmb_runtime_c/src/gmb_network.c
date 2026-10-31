/* ============================================================================
 * gmb_network.c — Implementazione rete TCP/UDP (socket POSIX).
 * Registry di handle long → socket, come PCodeVM. Fase R1.
 * ============================================================================ */
#include "gmb_network.h"
#include "gmb_bsp.h"
#include "gmb_commonlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <pthread.h>   /* winpthreads: fornita dal toolchain (thread del server HTTP) */
#include <poll.h>      /* shim shared/include/win32/poll.h → WSAPoll */
typedef SOCKET gmb_net_sock_t;
#define GMB_NET_SOCK_OK(s) ((s) != INVALID_SOCKET)
#define GMB_NET_SOCK_INVALID ((int64_t)-1)
#define GMB_NET_ERR() (WSAGetLastError())
typedef struct { volatile LONG locked; } gmb_net_lock_t;
static void gmb_net_lock(gmb_net_lock_t* m) { while (_InterlockedExchange(&m->locked, 1)) { ; } }
static void gmb_net_unlock(gmb_net_lock_t* m) { _InterlockedExchange(&m->locked, 0); }
typedef HANDLE gmb_net_thread_t;
static void gmb_net_thread_create(gmb_net_thread_t* t, void* (*fn)(void*), void* arg)
{ *t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, NULL); }
static int g_wsa = 0;
static void wsa_init(void) { if (!g_wsa) { WSADATA d; if (WSAStartup(MAKEWORD(2,2), &d) == 0) g_wsa = 1; } }
#define close(s) closesocket(s)
static int set_nonblocking(gmb_net_sock_t sock)
{
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
}
#else
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <time.h>
typedef int gmb_net_sock_t;
#define GMB_NET_SOCK_OK(s) ((s) >= 0)
#define GMB_NET_SOCK_INVALID ((int64_t)-1)
#define GMB_NET_ERR() (errno)
typedef struct { volatile int locked; } gmb_net_lock_t;
static void gmb_net_lock(gmb_net_lock_t* m) { while (__sync_lock_test_and_set(&m->locked, 1)) { ; } }
static void gmb_net_unlock(gmb_net_lock_t* m) { __sync_lock_release(&m->locked); }
typedef pthread_t gmb_net_thread_t;
static void gmb_net_thread_create(gmb_net_thread_t* t, void* (*fn)(void*), void* arg)
{ pthread_create(t, NULL, fn, arg); }
static int set_nonblocking(gmb_net_sock_t sock)
{
    int fl = fcntl(sock, F_GETFL, 0);
    return fcntl(sock, F_SETFL, fl | O_NONBLOCK);
}
#endif

static int gmb_net_setsockopt(gmb_net_sock_t sock, int level, int optname, const void* optval, socklen_t optlen)
{
#if defined(_WIN32) || defined(_WIN64)
    return setsockopt(sock, level, optname, (const char*)optval, (int)optlen);
#else
    return setsockopt(sock, level, optname, optval, optlen);
#endif
}

#define GMB_NET_MAX_HANDLES 256

typedef struct {
    gmb_net_sock_t sock;    /* file descriptor */
    int is_udp;
    int is_listener;     /* TCP listener */
    int in_use;
    /* peer remoto (per REMOTEIP/REMOTEPORT) — valorizzato in connect/accept */
    struct sockaddr_storage peer;
    int peer_valid;
    /* porta locale (per LOCALPORT) — valorizzata in listen/open/connect */
    int local_port;
    /* SSL/TLS: SSL* per client accettati/connessi, SSL_CTX* per listener SSL */
    void* ssl;        /* SSL* (non-NULL = canale cifrato) */
    void* ssl_ctx;    /* SSL_CTX* (non-NULL = listener TLS) */
    short ssl_want;   /* V7b: ultimo SSL_WANT (0=nessuno,1=read,2=write) per il park */
    /* V7c (ABA): generation token monotono — incrementato a ogni net_alloc. Un task
     * parcheggiato memorizza la gen dell'handle e la riconfronta al retry: se lo slot
     * è stato chiuso e riusato, la gen differisce e la syscall risolve come "chiuso"
     * invece di operare sul socket nuovo. */
    uint32_t gen;
} gmb_net_handle_t;

static gmb_net_handle_t g_handles[GMB_NET_MAX_HANDLES];
static uint32_t g_handle_gen = 0;   /* V7c: contatore generation token (monotono) */
static gmb_net_lock_t g_net_lock;

static int64_t gmb_now_ms_net(void); /* forward: definita più sotto (timebase watcher) */

/* ============================================================================
 * Session store nativo (HTTP.SESSIONCREATE + GMBSESSION), 2026-09-02.
 * In-memory, sliding 30 min, thread-safe (spinlock). Layout minimale: id, user, role.
 * ============================================================================ */
#define GMB_SESS_MAX 512
#define GMB_SESS_TTL_MS (30ll * 60ll * 1000ll)

typedef struct {
    char id[40];
    char user[96];
    char role[64];
    int64_t last_ms;
} gmb_sess_t;

static gmb_sess_t g_sessions[GMB_SESS_MAX];
static gmb_net_lock_t g_sessions_lock;
static uint64_t g_sess_seq = 0;

static int64_t gmb_c99_now_ms(void) { return (int64_t)(gmb_now_ms_net()); }

static void gmb_sess_gen_id(char out[40])
{
    uint64_t seq = ++g_sess_seq;
    uint8_t seed[24];
    int64_t now = gmb_c99_now_ms();
    memcpy(seed, &now, 8);
    memcpy(seed + 8, &seq, 8);
    memcpy(seed + 16, &seq, 8);
    uint8_t h[32];
    gmb_sha256(seed, sizeof(seed), h);
    char* hex = gmb_hex_encode_string(h, 32);
    if (hex) { snprintf(out, 40, "%s", hex); free(hex); } else { snprintf(out, 40, "sess%llu", (unsigned long long)seq); }
}

/* Crea una sessione per (user, role): ritorna il cookie GMBSESSION allocato. */
char* gmb_session_create(const char* user, const char* role)
{
    if (!user) user = "";
    if (!role) role = "";
    gmb_net_lock(&g_sessions_lock);
    int64_t now = gmb_c99_now_ms();
    /* slot libero o, in overflow, quello meno recente */
    int slot = -1;
    int64_t oldest = now;
    for (int i = 0; i < GMB_SESS_MAX; i++)
    {
        if (g_sessions[i].id[0] == '\0') { slot = i; break; }
        if (g_sessions[i].last_ms < oldest) { oldest = g_sessions[i].last_ms; slot = i; }
    }
    if (slot < 0) slot = 0;
    memset(&g_sessions[slot], 0, sizeof(g_sessions[0]));
    gmb_sess_gen_id(g_sessions[slot].id);
    snprintf(g_sessions[slot].user, sizeof(g_sessions[slot].user), "%s", user);
    snprintf(g_sessions[slot].role, sizeof(g_sessions[slot].role), "%s", role);
    g_sessions[slot].last_ms = now;
    char cookie[256];
    snprintf(cookie, sizeof(cookie), "GMBSESSION=%s; Path=/; HttpOnly; SameSite=Lax", g_sessions[slot].id);
    char* out = strdup(cookie);
    gmb_net_unlock(&g_sessions_lock);
    return out;
}

/* True se la sessione esiste ed è valida (sliding: aggiorna last_ms). */
int gmb_session_verify(const char* id)
{
    if (!id || !*id) return 0;
    gmb_net_lock(&g_sessions_lock);
    int64_t now = gmb_c99_now_ms();
    int ok = 0;
    for (int i = 0; i < GMB_SESS_MAX && !ok; i++)
        if (g_sessions[i].id[0] && strcmp(g_sessions[i].id, id) == 0)
        {
            if (now - g_sessions[i].last_ms <= GMB_SESS_TTL_MS) { g_sessions[i].last_ms = now; ok = 1; }
            else memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
        }
    gmb_net_unlock(&g_sessions_lock);
    return ok;
}

/* Copia il ruolo (e l'utente) della sessione, se valida. Ritorna 1 su successo. */
int gmb_session_get(const char* id, char* user_out, size_t user_cap, char* role_out, size_t role_cap)
{
    if (!id || !*id) return 0;
    gmb_net_lock(&g_sessions_lock);
    int64_t now = gmb_c99_now_ms();
    int ok = 0;
    for (int i = 0; i < GMB_SESS_MAX && !ok; i++)
        if (g_sessions[i].id[0] && strcmp(g_sessions[i].id, id) == 0)
        {
            if (now - g_sessions[i].last_ms <= GMB_SESS_TTL_MS)
            {
                g_sessions[i].last_ms = now;
                if (user_out && user_cap) snprintf(user_out, user_cap, "%s", g_sessions[i].user);
                if (role_out && role_cap) snprintf(role_out, role_cap, "%s", g_sessions[i].role);
                ok = 1;
            }
            else memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
        }
    gmb_net_unlock(&g_sessions_lock);
    return ok;
}

/* ============================================================================
 * Hook di blocco astratti + network-watcher (runtime neutro rispetto alla GPM).
 * Il backend GPM (VM/C: nessun hook; C99: coroutine green-thread) registra
 * block/wake. Il watcher fa poll() sui fd con waiter in attesa e, a fd pronto o
 * timeout, chiama wake_fn(waiter) → il backend riaccoda il task parcheggiato.
 * ============================================================================ */
#define GMB_NET_MAX_WAITERS 512

typedef struct {
    int fd;
    void* waiter;             /* opacoptrin del task parcheggiato (mai interpretato qui) */
    int64_t timeout_at_ms;    /* 0 = nessun timeout (attesa indefinita) */
    short events;             /* POLLIN/POLLOUT (V7b: WRITE readiness); 0 = POLLIN */
    uint32_t gen;             /* V7c: generation token dell'handle al park (0 = nessuno) */
    int in_use;
} gmb_net_waiter_t;

static gmb_net_block_fn g_block_hook = NULL;
static gmb_net_wake_fn g_wake_hook = NULL;
static void* g_block_ctx = NULL;
static gmb_net_waiter_t g_waiters[GMB_NET_MAX_WAITERS];
static gmb_net_lock_t g_waiters_lock;
static gmb_net_thread_t g_watcher_thread;
static volatile int g_watcher_running = 0;

static int64_t gmb_now_ms_net(void);
static gmb_net_handle_t* net_get(int64_t handle);

/* confronto stringhe case-insensitive portatile (strcasecmp/_stricmp) */
static int gmb_net_ieq(const char* a, const char* b)
{
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

void gmb_net_set_block_hooks(gmb_net_block_fn block, gmb_net_wake_fn wake, void* ctx)
{
    gmb_net_lock(&g_net_lock);
    g_block_hook = block;
    g_wake_hook = wake;
    g_block_ctx = ctx;
    gmb_net_unlock(&g_net_lock);
}

/* API esposte al backend: registra il task corrente (waiter opaco) in attesa su fd.
   Il network-watcher lo sveglia a fd pronto o timeout via wake_fn. */
int gmb_net_waiter_register_gen(int fd, void* waiter, int64_t timeout_ms, short events, uint32_t gen)
{
    gmb_net_lock(&g_waiters_lock);
    for (int i = 0; i < GMB_NET_MAX_WAITERS; i++)
    {
        if (!g_waiters[i].in_use)
        {
            g_waiters[i].fd = fd;
            g_waiters[i].waiter = waiter;
            g_waiters[i].timeout_at_ms = timeout_ms > 0 ? gmb_now_ms_net() + timeout_ms : 0;
            g_waiters[i].events = events;
            g_waiters[i].gen = gen;
            g_waiters[i].in_use = 1;
            gmb_net_unlock(&g_waiters_lock);
            return 0;
        }
    }
    gmb_net_unlock(&g_waiters_lock);
    return -1;
}

int gmb_net_waiter_register_ev(int fd, void* waiter, int64_t timeout_ms, short events)
{
    return gmb_net_waiter_register_gen(fd, waiter, timeout_ms, events, 0);
}

int gmb_net_waiter_register(int fd, void* waiter, int64_t timeout_ms)
{
    return gmb_net_waiter_register_ev(fd, waiter, timeout_ms, 0);
}

void gmb_net_waiter_unregister(int fd, void* waiter)
{
    gmb_net_lock(&g_waiters_lock);
    for (int i = 0; i < GMB_NET_MAX_WAITERS; i++)
    {
        if (g_waiters[i].in_use && g_waiters[i].fd == fd
            && (waiter == NULL || g_waiters[i].waiter == waiter))
        {
            g_waiters[i].in_use = 0;
            if (waiter != NULL) break;  /* una sola voce per (fd,waiter) */
        }
    }
    gmb_net_unlock(&g_waiters_lock);
}

/* V7c (wake-on-close): rimuove TUTTI i waiter su fd e li raccoglie in out (max cap)
   perché il chiamante li svegli dopo il close. Ritorna il numero raccolto. Un handle
   chiuso mentre un task è parcheggiato non deve lasciarlo attendere fino al timeout:
   il wake lo riattiva subito e il generation token al retry risolve come "chiuso". */
int gmb_net_waiter_take_fd(int fd, void** out, int cap)
{
    gmb_net_lock(&g_waiters_lock);
    int n = 0;
    for (int i = 0; i < GMB_NET_MAX_WAITERS; i++)
    {
        if (g_waiters[i].in_use && g_waiters[i].fd == fd)
        {
            if (out && n < cap) out[n++] = g_waiters[i].waiter;
            g_waiters[i].in_use = 0;
        }
    }
    gmb_net_unlock(&g_waiters_lock);
    return n;
}

static int64_t gmb_now_ms_net(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* network-watcher: poll su tutti i fd in attesa; a pronto/timeout sveglia il backend */
static void* gmb_net_watcher(void* arg)
{
    (void)arg;
    struct pollfd pfds[GMB_NET_MAX_WAITERS];
    int idx_map[GMB_NET_MAX_WAITERS];
    while (g_watcher_running)
    {
        int n = 0;
        int64_t now = gmb_now_ms_net();
        int64_t earliest = 0;
        gmb_net_lock(&g_waiters_lock);
        for (int i = 0; i < GMB_NET_MAX_WAITERS; i++)
        {
            if (!g_waiters[i].in_use) continue;
            pfds[n].fd = g_waiters[i].fd;
            pfds[n].events = g_waiters[i].events ? g_waiters[i].events : POLLIN;
            pfds[n].revents = 0;
            idx_map[n] = i;
            n++;
            if (g_waiters[i].timeout_at_ms > 0
                && (earliest == 0 || g_waiters[i].timeout_at_ms < earliest))
                earliest = g_waiters[i].timeout_at_ms;
        }
        gmb_net_unlock(&g_waiters_lock);

        int timeout_ms = 250;
        if (earliest > 0)
        {
            int64_t d = earliest - now;
            timeout_ms = (int)(d < 1 ? 1 : (d > 250 ? 250 : d));
        }
        int rc = poll(pfds, (nfds_t)n, timeout_ms);
        if (rc > 0)
        {
            gmb_net_lock(&g_waiters_lock);
            for (int i = 0; i < n; i++)
            {
                if (pfds[i].revents && g_waiters[idx_map[i]].in_use)
                {
                    void* waiter = g_waiters[idx_map[i]].waiter;
                    g_waiters[idx_map[i]].in_use = 0;
                    if (waiter && g_wake_hook)
                        g_wake_hook(g_block_ctx, waiter);
                }
            }
            gmb_net_unlock(&g_waiters_lock);
        }
        else if (rc == 0)
        {
            /* timeout: sveglia chi è scaduto */
            int64_t now2 = gmb_now_ms_net();
            gmb_net_lock(&g_waiters_lock);
            for (int i = 0; i < GMB_NET_MAX_WAITERS; i++)
            {
                if (g_waiters[i].in_use && g_waiters[i].timeout_at_ms > 0
                    && g_waiters[i].timeout_at_ms <= now2)
                {
                    void* waiter = g_waiters[i].waiter;
                    g_waiters[i].in_use = 0;
                    if (waiter && g_wake_hook)
                        g_wake_hook(g_block_ctx, waiter);
                }
            }
            gmb_net_unlock(&g_waiters_lock);
        }
    }
    return NULL;
}

int gmb_net_wait_fd(int fd, int64_t timeout_ms)
{
    gmb_net_lock(&g_net_lock);
    int has_hooks = (g_block_hook != NULL);
    gmb_net_block_fn block = g_block_hook;
    void* ctx = g_block_ctx;
    gmb_net_unlock(&g_net_lock);

    if (!has_hooks)
    {
        /* VM/C (o runtime senza GPM C99): poll non-bloccante con timeout.
           Il chiamante gestisce il "non pronto" (semantica attuale). */
        struct pollfd p;
        p.fd = fd;
        p.events = POLLIN;
        p.revents = 0;
        int t = timeout_ms > 0 ? (int)timeout_ms : -1;
        int rc = poll(&p, 1, t);
        if (rc < 0) return -1;
        if (rc == 0) return 0;
        return 1;
    }

    /* backend con hook: il block_fn C99 registra il task corrente nel registry
       (gmb_net_waiter_register) e poi parcheggia la coroutine (gmb_block).
       Al resume, il fd è stato rimosso dal watcher: torna pronto. */
    if (block) block(ctx, fd, timeout_ms);
    gmb_net_waiter_unregister(fd, NULL);
    return 1;
}

int gmb_net_tcp_recv_block(int64_t handle, uint8_t* buf, size_t len, int64_t timeout_ms)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || h->is_udp) return -1;
    /* timeout assoluto (0 = attesa indefinita) */
    int64_t deadline = timeout_ms > 0 ? gmb_now_ms_net() + timeout_ms : 0;
    for (;;)
    {
        int n = (int)recv(h->sock, buf, len, 0);
        if (n >= 0) return n;               /* dati (anche 0 = chiuso) */
#if defined(_WIN32) || defined(_WIN64)
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK && e != WSAEINTR) return -1;
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return -1;
#endif
        /* fd non pronto: attende (cooperativo via hook, o poll) */
        int64_t remain = deadline > 0 ? deadline - gmb_now_ms_net() : 0;
        if (deadline > 0 && remain <= 0) return 0;   /* timeout scaduto */
        int w = gmb_net_wait_fd((int)h->sock, deadline > 0 ? remain : 0);
        if (w == 0) return 0;               /* timeout */
        if (w < 0) return -1;
        /* fd pronto: riprova la recv */
    }
}

void gmb_net_init(void)
{
#if defined(_WIN32) || defined(_WIN64)
    wsa_init();
#endif
#if GMB_HAS_SSL
    gmb_ssl_init();
#endif
    gmb_net_lock(&g_net_lock);
    memset(g_handles, 0, sizeof(g_handles));
    gmb_net_unlock(&g_net_lock);
    gmb_net_lock(&g_waiters_lock);
    memset(g_waiters, 0, sizeof(g_waiters));
    gmb_net_unlock(&g_waiters_lock);
    if (!g_watcher_running)
    {
        g_watcher_running = 1;
        gmb_net_thread_create(&g_watcher_thread, gmb_net_watcher, NULL);
    }
}

void gmb_net_shutdown(void)
{
    if (g_watcher_running)
    {
        g_watcher_running = 0;
        /* il watcher si sveglia entro ~250ms dal poll e termina */
    }
    gmb_net_lock(&g_net_lock);
    for (int i = 0; i < GMB_NET_MAX_HANDLES; i++)
    {
        if (g_handles[i].in_use && GMB_NET_SOCK_OK(g_handles[i].sock))
        {
            close(g_handles[i].sock);
            g_handles[i].in_use = 0;
        }
    }
    gmb_net_unlock(&g_net_lock);
}

static int64_t net_alloc(gmb_net_sock_t sock, int is_udp, int is_listener)
{
    gmb_net_lock(&g_net_lock);
    for (int i = 1; i < GMB_NET_MAX_HANDLES; i++)
    {
        if (!g_handles[i].in_use)
        {
            g_handles[i].sock = sock;
            g_handles[i].is_udp = is_udp;
            g_handles[i].is_listener = is_listener;
            g_handles[i].in_use = 1;
            g_handles[i].peer_valid = 0;
            g_handles[i].local_port = 0;
            g_handles[i].ssl = NULL;
            g_handles[i].ssl_ctx = NULL;
            g_handles[i].ssl_want = 0;
            g_handles[i].gen = ++g_handle_gen;   /* V7c: nuova generazione a ogni riuso */
            if (g_handle_gen == 0) g_handle_gen = 1;  /* wrap: 0 = "nessun token" */
            gmb_net_unlock(&g_net_lock);
            return (int64_t)i;
        }
    }
    gmb_net_unlock(&g_net_lock);
    close(sock);
    return -1;
}

/* valorizza la porta locale dell'handle (getsockname) */
static void net_update_local_port(gmb_net_handle_t* h)
{
#if defined(_WIN32) || defined(_WIN64)
    struct sockaddr_in sa;
    int slen = sizeof(sa);
    if (getsockname(h->sock, (struct sockaddr*)&sa, &slen) == 0)
        h->local_port = ntohs(sa.sin_port);
#else
    struct sockaddr_in sa;
    socklen_t slen = sizeof(sa);
    if (getsockname(h->sock, (struct sockaddr*)&sa, &slen) == 0)
        h->local_port = ntohs(sa.sin_port);
#endif
}

static gmb_net_handle_t* net_get(int64_t handle)
{
    if (handle <= 0 || handle >= GMB_NET_MAX_HANDLES) return NULL;
    gmb_net_handle_t* h = &g_handles[handle];
    return h->in_use ? h : NULL;
}

static void net_release(int64_t handle)
{
    void* wake_list[GMB_NET_MAX_WAITERS];
    int nw = 0;
    gmb_net_wake_fn wake = NULL;
    void* wctx = NULL;
    gmb_net_lock(&g_net_lock);
    gmb_net_handle_t* h = &g_handles[handle];
    if (h->in_use)
    {
        /* V7c wake-on-close: raccoglie i waiter sul fd PRIMA del close (V7b: nessun
         * waiter deve restare su un fd chiuso/riciclato — ABA) e li sveglia dopo il
         * rilascio dei lock. Al retry il generation token risolve come "chiuso". */
        wake = g_wake_hook;
        wctx = g_block_ctx;
        if (GMB_NET_SOCK_OK(h->sock))
            nw = gmb_net_waiter_take_fd((int)h->sock, wake_list, GMB_NET_MAX_WAITERS);
#if GMB_HAS_SSL
        if (h->ssl) { SSL_shutdown((SSL*)h->ssl); SSL_free((SSL*)h->ssl); h->ssl = NULL; }
        if (h->ssl_ctx) { SSL_CTX_free((SSL_CTX*)h->ssl_ctx); h->ssl_ctx = NULL; }
#endif
        if (GMB_NET_SOCK_OK(h->sock)) close(h->sock);
        h->in_use = 0;
    }
    gmb_net_unlock(&g_net_lock);
    for (int i = 0; i < nw; i++)
        if (wake && wake_list[i]) wake(wctx, wake_list[i]);
}

/* V7c: generation token corrente dell'handle (0 = handle non valido/chiuso). Il
 * backend parcheggiato la confronta al retry per rilevare il riuso dello slot. */
uint32_t gmb_net_handle_gen(int64_t handle)
{
    if (handle <= 0 || handle >= GMB_NET_MAX_HANDLES) return 0;
    gmb_net_lock(&g_net_lock);
    gmb_net_handle_t* h = &g_handles[handle];
    uint32_t g = h->in_use ? h->gen : 0;
    gmb_net_unlock(&g_net_lock);
    return g;
}

int gmb_net_handle_fd(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return -1;
    return (int)h->sock;
}

int64_t gmb_net_tcp_listen(const char* host, int port)
{
    gmb_net_sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!GMB_NET_SOCK_OK(sock)) return -1;
    int yes = 1;
    gmb_net_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = host && *host ? inet_addr(host) : INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(sock); return -1; }
    if (listen(sock, 32) < 0) { close(sock); return -1; }
    set_nonblocking(sock);
    int64_t h = net_alloc(sock, 0, 1);
    if (h > 0)
    {
        gmb_net_handle_t* hh = net_get(h);
        if (hh) net_update_local_port(hh);
    }
    return h;
}

int64_t gmb_net_tcp_connect(const char* host, int port)
{
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return -1;
    gmb_net_sock_t sock = socket(res->ai_family, res->ai_socktype, 0);
    if (!GMB_NET_SOCK_OK(sock)) { freeaddrinfo(res); return -1; }
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        close(sock); freeaddrinfo(res); return -1;
    }
    /* salva il peer remoto (per REMOTEIP/REMOTEPORT) */
    struct sockaddr_storage peer_sa;
    socklen_t peer_len = sizeof(peer_sa);
    getpeername(sock, (struct sockaddr*)&peer_sa, &peer_len);
    freeaddrinfo(res);
    /* nonblocking dopo il connect (parità con listener/accept): i wrapper bloccanti
     * (recv_block/send_block) gestiscono EAGAIN via gmb_net_wait_fd, e la VM/C può
     * parcheggiare i task sul fd invece di bloccare il worker in recv/send. */
    set_nonblocking(sock);
    int yes = 1;
    gmb_net_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    int64_t h = net_alloc(sock, 0, 0);
    if (h > 0)
    {
        gmb_net_handle_t* hh = net_get(h);
        if (hh)
        {
            memcpy(&hh->peer, &peer_sa, sizeof(peer_sa));
            hh->peer_valid = 1;
            net_update_local_port(hh);
        }
    }
    return h;
}

/* §16: connect non-bloccante da un indirizzo GIÀ risolto (corpo condiviso con
 * connect_start; il VM/C lo usa dopo la risoluzione DNS asincrona nel pool).
 * Ritorna handle>0 e imposta *pending: 0 = connesso subito, 1 = in corso
 * (EINPROGRESS): attendere POLLOUT e poi chiamare gmb_net_connect_finish. */
int64_t gmb_net_tcp_connect_addr(const uint8_t* sa, int sa_len, int* pending)
{
    *pending = 0;
    if (!sa || sa_len <= 0) return -1;
    gmb_net_sock_t sock = socket(((const struct sockaddr*)sa)->sa_family, SOCK_STREAM, 0);
    if (!GMB_NET_SOCK_OK(sock)) return -1;
    set_nonblocking(sock);
    struct sockaddr_storage peer_sa;
    memset(&peer_sa, 0, sizeof(peer_sa));
    memcpy(&peer_sa, sa, (size_t)sa_len <= sizeof(peer_sa) ? (size_t)sa_len : sizeof(peer_sa));
    int rc = connect(sock, (const struct sockaddr*)sa, (socklen_t)sa_len);
    if (rc < 0)
    {
        int e = GMB_NET_ERR();
#if defined(_WIN32) || defined(_WIN64)
        if (e != WSAEINPROGRESS && e != WSAEWOULDBLOCK) { close(sock); return -1; }
#else
        if (e != EINPROGRESS && e != EWOULDBLOCK) { close(sock); return -1; }
#endif
        *pending = 1;
    }
    int yes = 1;
    gmb_net_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    int64_t h = net_alloc(sock, 0, 0);
    if (h <= 0) { close(sock); return -1; }
    gmb_net_handle_t* hh = net_get(h);
    if (hh)
    {
        memcpy(&hh->peer, &peer_sa, sizeof(peer_sa));
        hh->peer_valid = 1;
        if (!*pending) net_update_local_port(hh);
    }
    return h;
}

/* V7b: connect non-bloccante con risoluzione DNS SINCRONA (usato dal backend C99
 * coi green-thread e come fallback del VM/C a pool DNS pieno). Il VM/C preferisce
 * il path §16 (gmb_net_dns_resolve_start + gmb_net_tcp_connect_addr).
 * Ritorna handle>0 e imposta *pending come gmb_net_tcp_connect_addr. */
int64_t gmb_net_tcp_connect_start(const char* host, int port, int* pending)
{
    *pending = 0;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) return -1;
    int64_t h = gmb_net_tcp_connect_addr((const uint8_t*)res->ai_addr, (int)res->ai_addrlen, pending);
    freeaddrinfo(res);
    return h;
}

/* V7b: completa il connect non-bloccante (dopo il park su POLLOUT).
 * Ritorna 0 = connesso, -1 = fallito (SO_ERROR). */
int gmb_net_connect_finish(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return -1;
    int soerr = 0;
    socklen_t sl = sizeof(soerr);
    if (getsockopt(h->sock, SOL_SOCKET, SO_ERROR, (void*)&soerr, &sl) < 0) return -1;
    if (soerr != 0) return -1;
    net_update_local_port(h);
    return 0;
}

/* ============================================================================
 * §16: pool DNS asincrono. getaddrinfo è bloccante e congelerebbe i worker
 * cooperativi della VM/C: ogni job gira su un thread dedicato (detached) e
 * segnala il completamento scrivendo 1 byte in una pipe — l'estremo di lettura
 * è parcheggiabile dal backend via gmb_net_waiter_register_* (POLLIN), con lo
 * stesso watcher dei socket. L'id del job è monotono e mai riusato (anti-ABA:
 * il park non può risvegliarsi su un job diverso). Pool pieno o piattaforma
 * senza pipe (Windows) → start ritorna -1 e il chiamante degrada al path
 * sincrono (gmb_net_tcp_connect_start).
 * ============================================================================ */
#define GMB_DNS_MAX_JOBS 16

typedef struct {
    int64_t id;                 /* monotono, mai riusato */
    char host[256];
    int port;
    int in_use;
    int abandoned;              /* consumatore arreso (timeout/task morto): il
                                 * worker libera lo slot al completamento */
    int done;
    int ok;
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int rfd, wfd;               /* pipe di completamento (rfd parcheggiabile) */
} gmb_dns_job_t;

static gmb_dns_job_t g_dns_jobs[GMB_DNS_MAX_JOBS];
static gmb_net_lock_t g_dns_lock;
static int64_t g_dns_next_id = 1;
static int g_dns_active = 0;

#if !defined(_WIN32) && !defined(_WIN64)

/* Chiamato SEMPRE sotto g_dns_lock. */
static void dns_release_locked(gmb_dns_job_t* j)
{
    close(j->rfd);
    close(j->wfd);
    j->in_use = 0;
    g_dns_active--;
}

static void* gmb_dns_worker(void* arg)
{
    pthread_detach(pthread_self());
    gmb_dns_job_t* j = (gmb_dns_job_t*)arg;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", j->port);
    /* l'unica parte bloccante del path di connessione, ora fuori dai worker */
    int rc = getaddrinfo(j->host, portstr, &hints, &res);
    gmb_net_lock(&g_dns_lock);
    if (!j->in_use) { gmb_net_unlock(&g_dns_lock); if (res) freeaddrinfo(res); return NULL; }
    if (rc == 0 && res)
    {
        memcpy(&j->addr, res->ai_addr,
               res->ai_addrlen <= sizeof(j->addr) ? res->ai_addrlen : sizeof(j->addr));
        j->addr_len = res->ai_addrlen;
        j->ok = 1;
        freeaddrinfo(res);
    }
    j->done = 1;
    int abandoned = j->abandoned;
    int wfd = j->wfd;
    if (abandoned) dns_release_locked(j);   /* nessuno consumerà: libera subito */
    gmb_net_unlock(&g_dns_lock);
    if (!abandoned) { char b = 1; ssize_t wr = write(wfd, &b, 1); (void)wr; }
    return NULL;
}

#endif /* !_WIN32 */

/* Avvia la risoluzione asincrona di host:port. Ritorna l'id del job (>0) o -1
 * (pool pieno/host nullo/piattaforma senza pipe → il chiamante usa il path
 * sincrono). */
int64_t gmb_net_dns_resolve_start(const char* host, int port)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)host; (void)port;
    return -1;
#else
    if (!host || !*host) return -1;
    gmb_net_lock(&g_dns_lock);
    gmb_dns_job_t* j = NULL;
    for (int i = 0; i < GMB_DNS_MAX_JOBS; i++)
        if (!g_dns_jobs[i].in_use) { j = &g_dns_jobs[i]; break; }
    if (!j) { gmb_net_unlock(&g_dns_lock); return -1; }
    int fds[2];
    if (pipe(fds) != 0) { gmb_net_unlock(&g_dns_lock); return -1; }
    memset(j, 0, sizeof(*j));
    j->id = g_dns_next_id++;
    snprintf(j->host, sizeof(j->host), "%s", host);
    j->port = port;
    j->rfd = fds[0];
    j->wfd = fds[1];
    set_nonblocking(j->rfd);
    j->in_use = 1;
    g_dns_active++;
    int64_t id = j->id;
    gmb_net_unlock(&g_dns_lock);
    pthread_t th;
    if (pthread_create(&th, NULL, gmb_dns_worker, j) != 0)
    {
        gmb_net_lock(&g_dns_lock);
        if (j->in_use && j->id == id) dns_release_locked(j);
        gmb_net_unlock(&g_dns_lock);
        return -1;
    }
    return id;
#endif
}

/* Ritorna l'fd parcheggiabile (POLLIN) del job, o -1. */
int gmb_net_dns_job_fd(int64_t id)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)id;
    return -1;
#else
    gmb_net_lock(&g_dns_lock);
    int fd = -1;
    for (int i = 0; i < GMB_DNS_MAX_JOBS; i++)
        if (g_dns_jobs[i].in_use && g_dns_jobs[i].id == id && !g_dns_jobs[i].abandoned)
        { fd = g_dns_jobs[i].rfd; break; }
    gmb_net_unlock(&g_dns_lock);
    return fd;
#endif
}

/* Consuma un job COMPLETATO (il byte nella pipe ha già svegliato il chiamante).
 * out_sa/out_len: sockaddr risolto (buffer del chiamante). Ritorna 0 = risolto,
 * -1 = fallito/job sconosciuto/non ancora done. Libera lo slot in ogni caso. */
int gmb_net_dns_resolve_finish(int64_t id, uint8_t* out_sa, int sa_cap, int* out_len)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)id; (void)out_sa; (void)sa_cap; (void)out_len;
    return -1;
#else
    if (id <= 0) return -1;
    gmb_net_lock(&g_dns_lock);
    for (int i = 0; i < GMB_DNS_MAX_JOBS; i++)
    {
        gmb_dns_job_t* j = &g_dns_jobs[i];
        if (j->in_use && j->id == id && !j->abandoned)
        {
            if (!j->done) { gmb_net_unlock(&g_dns_lock); return -1; }
            int ok = j->ok;
            if (ok && out_sa && out_len)
            {
                int n = (int)j->addr_len;
                if (n > sa_cap) n = sa_cap;
                memcpy(out_sa, &j->addr, (size_t)n);
                *out_len = (int)j->addr_len;
            }
            dns_release_locked(j);
            gmb_net_unlock(&g_dns_lock);
            return ok ? 0 : -1;
        }
    }
    gmb_net_unlock(&g_dns_lock);
    return -1;
#endif
}

/* Abbandona un job (timeout del connect / task liberato col job in volo). Se il
 * worker è ancora in volo lo slot resta in_use e viene liberato dal worker al
 * completamento (nessun write sulla pipe). Sicuro su id mai riusati. */
void gmb_net_dns_cancel(int64_t id)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)id;
#else
    if (id <= 0) return;
    gmb_net_lock(&g_dns_lock);
    for (int i = 0; i < GMB_DNS_MAX_JOBS; i++)
    {
        gmb_dns_job_t* j = &g_dns_jobs[i];
        if (j->in_use && j->id == id && !j->abandoned)
        {
            if (j->done) dns_release_locked(j);
            else j->abandoned = 1;
            break;
        }
    }
    gmb_net_unlock(&g_dns_lock);
#endif
}

int64_t gmb_net_tcp_accept(int64_t listener)
{
    gmb_net_handle_t* h = net_get(listener);
    if (!h || !h->is_listener) return -1;
    struct sockaddr_storage cli;
    socklen_t clen = sizeof(cli);
    gmb_net_sock_t client = accept(h->sock, (struct sockaddr*)&cli, &clen);
    if (client < 0) return -1;
    set_nonblocking(client);
    int yes = 1;
    gmb_net_setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    int64_t ch = net_alloc(client, 0, 0);
    if (ch > 0)
    {
        gmb_net_handle_t* chh = net_get(ch);
        if (chh)
        {
            memcpy(&chh->peer, &cli, sizeof(cli));
            chh->peer_valid = 1;
            net_update_local_port(chh);
        }
    }
    return ch;
}

int gmb_net_tcp_send(int64_t handle, const uint8_t* data, size_t len)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || h->is_udp) return -2;
    int sent = (int)send(h->sock, data, len, 0);
    if (sent < 0)
    {
        int e = GMB_NET_ERR();
#if defined(_WIN32) || defined(_WIN64)
        if (e == WSAEWOULDBLOCK || e == WSAEINTR) return -1; /* riprovabile */
#else
        if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) return -1; /* riprovabile */
#endif
        return -2; /* errore hard (ECONNRESET ecc.): il park non deve riprovare */
    }
    return sent;
}

int gmb_net_tcp_recv(int64_t handle, uint8_t* buf, size_t len)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || h->is_udp) return -1;
    int n = (int)recv(h->sock, buf, len, 0);
    return n; /* 0 = chiuso, -1 = EAGAIN (non disponibile) */
}

/* NET.PEEK(handle, n): sbircia fino a n byte SENZA rimuoverli dal buffer (MSG_PEEK). */
int gmb_net_tcp_peek(int64_t handle, uint8_t* buf, size_t len)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || h->is_udp) return -1;
    int n = (int)recv(h->sock, buf, len, MSG_PEEK);
    return n;
}

/* gmb_net_tcp_peek_str: NET.PEEK(handle, numBytes) → stringa (parità IL IlRuntime.TcpPeek).
 * Handle invalido → stringa vuota. */
gmb_string_t gmb_net_tcp_peek_str(int64_t handle, int len)
{
    if (len < 0) len = 0;
    if (len > 65536) len = 65536;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return gmb_string_alloc("");
    int n = gmb_net_tcp_peek(handle, (uint8_t*)buf, (size_t)len);
    if (n < 0) n = 0;
    buf[n] = 0;
    gmb_string_t s = gmb_string_alloc(buf);
    free(buf);
    return s;
}

void gmb_net_tcp_close(int64_t handle)
{
    net_release(handle);
}

int64_t gmb_net_udp_open(int port)
{
    gmb_net_sock_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (!GMB_NET_SOCK_OK(sock)) return -1;
    /* Bind SEMPRE, anche con port 0 (che chiede una porta effimera all'OS): senza bind
       getsockname ritorna 0 e LOCALPORT(handle) resta 0 fino al primo sendto (cfr.
       UdpHandle.cs:204-207 — il percorso C# di IL/PCode binda incondizionato; §0bis
       impone parità col runtime C condiviso). */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(sock); return -1; }
    set_nonblocking(sock);
    int64_t h = net_alloc(sock, 1, 0);
    if (h > 0)
    {
        gmb_net_handle_t* hh = net_get(h);
        if (hh) net_update_local_port(hh);
    }
    return h;
}

int gmb_net_udp_send(int64_t handle, const char* host, int port, const uint8_t* data, size_t len)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->is_udp) return -1;
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons((uint16_t)port);
    to.sin_addr.s_addr = inet_addr(host);
    int r = (int)sendto(h->sock, data, len, 0, (struct sockaddr*)&to, sizeof(to));
    /* il primo send lega la porta locale (se era 0/auto) */
    if (h->local_port == 0) net_update_local_port(h);
    return r;
}

int gmb_net_udp_recv(int64_t handle, uint8_t* buf, size_t len, char* from_host, int from_host_cap, int* from_port)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->is_udp) return -1;
    struct sockaddr_in from;
    socklen_t flen = sizeof(from);
    int n = (int)recvfrom(h->sock, buf, len, 0, (struct sockaddr*)&from, &flen);
    if (n >= 0 && from_host && from_host_cap > 0)
    {
        inet_ntop(AF_INET, &from.sin_addr, from_host, (socklen_t)from_host_cap);
        if (from_port) *from_port = ntohs(from.sin_port);
        /* salva il peer per REMOTEIP/REMOTEPORT dopo una UDP RECV */
        struct sockaddr_storage ss;
        memset(&ss, 0, sizeof(ss));
        struct sockaddr_in* si = (struct sockaddr_in*)&ss;
        si->sin_family = AF_INET;
        si->sin_port = from.sin_port;
        si->sin_addr = from.sin_addr;
        memcpy(&h->peer, &ss, sizeof(ss));
        h->peer_valid = 1;
    }
    return n;
}

void gmb_net_udp_close(int64_t handle)
{
    net_release(handle);
}

/* ============================================================================
 * Introspezione (funzioni NET: CONN/REMOTEIP/REMOTEPORT/LOCALPORT/LOCALHOST/GETHOSTIP)
 * ============================================================================ */
int gmb_net_conn(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return 0;
    /* il listener "esiste" (CONN su listener → 1? PCodeVM: handle attivo) */
    return 1;
}

/* RECV(handle): byte disponibili da leggere (ioctl FIONREAD) — parità C# TcpRecv avail. */
int gmb_net_tcp_available(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || h->is_udp) return 0;
#if defined(_WIN32) || defined(_WIN64)
    unsigned long n = 0;
    if (ioctlsocket(h->sock, FIONREAD, &n) != 0) return 0;
    return (int)n;
#else
    int n = 0;
    if (ioctl(h->sock, FIONREAD, &n) != 0) return 0;
    return n > 0 ? n : 0;
#endif
}

int gmb_net_remote_ip(int64_t handle, char* out, int cap)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->peer_valid || cap <= 0) return -1;
#if defined(_WIN32) || defined(_WIN64)
    if (h->peer.ss_family == AF_INET)
    {
        struct sockaddr_in* si = (struct sockaddr_in*)&h->peer;
        inet_ntop(AF_INET, &si->sin_addr, out, (socklen_t)cap);
        return 0;
    }
    else if (h->peer.ss_family == AF_INET6)
    {
        struct sockaddr_in6* si6 = (struct sockaddr_in6*)&h->peer;
        inet_ntop(AF_INET6, &si6->sin6_addr, out, (socklen_t)cap);
        return 0;
    }
#else
    if (h->peer.ss_family == AF_INET)
    {
        struct sockaddr_in* si = (struct sockaddr_in*)&h->peer;
        inet_ntop(AF_INET, &si->sin_addr, out, (socklen_t)cap);
        return 0;
    }
    else if (h->peer.ss_family == AF_INET6)
    {
        struct sockaddr_in6* si6 = (struct sockaddr_in6*)&h->peer;
        inet_ntop(AF_INET6, &si6->sin6_addr, out, (socklen_t)cap);
        return 0;
    }
#endif
    return -1;
}

int gmb_net_remote_port(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->peer_valid) return 0;
#if defined(_WIN32) || defined(_WIN64)
    if (h->peer.ss_family == AF_INET)
        return ntohs(((struct sockaddr_in*)&h->peer)->sin_port);
    else if (h->peer.ss_family == AF_INET6)
        return ntohs(((struct sockaddr_in6*)&h->peer)->sin6_port);
#else
    if (h->peer.ss_family == AF_INET)
        return ntohs(((struct sockaddr_in*)&h->peer)->sin_port);
    else if (h->peer.ss_family == AF_INET6)
        return ntohs(((struct sockaddr_in6*)&h->peer)->sin6_port);
#endif
    return 0;
}

int gmb_net_local_port(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return 0;
    if (h->local_port == 0) net_update_local_port(h);
    return h->local_port;
}

int gmb_net_local_host(char* out, int cap)
{
    if (!out || cap <= 0) return -1;
    out[0] = '\0';
    /* Parità IL (IlRuntime.Host → Dns.GetHostName): LOCALHOST() è il NOME host,
       non il suo IP (il resolve-a-IP precedente divergeva da IL/PCode/C99). */
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) return -1;
    hostname[sizeof(hostname) - 1] = '\0';
    size_t n = strlen(hostname);
    if ((int)n >= cap) n = (size_t)(cap - 1);
    memcpy(out, hostname, n);
    out[n] = '\0';
    return 0;
}

/* GETHOSTIP(host): risolve l'host in IP stringa (parità syscall VM/C 561). */
int gmb_net_resolve_host(const char* host, char* out, int cap)
{
    if (!out || cap <= 0) return -1;
    out[0] = '\0';
    if (!host || !*host) return -1;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if (getaddrinfo(host, NULL, &hints, &res) == 0 && res) {
        struct sockaddr_in* sa = (struct sockaddr_in*)res->ai_addr;
        inet_ntop(AF_INET, &sa->sin_addr, out, (socklen_t)cap);
        freeaddrinfo(res);
        return 0;
    }
    return -1;
}

int gmb_net_get_host_ip(const char* hostname, char* out, int cap)
{
    if (!hostname || !out || cap <= 0) return -1;
    out[0] = '\0';
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0 || !res) return -1;
    struct sockaddr_in* sa = (struct sockaddr_in*)res->ai_addr;
    inet_ntop(AF_INET, &sa->sin_addr, out, (socklen_t)cap);
    freeaddrinfo(res);
    return 0;
}

int gmb_net_tcp_shutdown(int64_t handle, const char* mode)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || h->is_udp) return -1;
    int how = 2; /* SHUT_RDWR (both) */
    if (mode)
    {
        if (strcmp(mode, "send") == 0 || strcmp(mode, "1") == 0) how = 1;
        else if (strcmp(mode, "receive") == 0 || strcmp(mode, "2") == 0) how = 0;
    }
    return shutdown(h->sock, how);
}

int gmb_net_tcp_setopt(int64_t handle, const char* name, const char* value)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return -1;
    int on = 0;
    if (value)
    {
        if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0
            || strcmp(value, "TRUE") == 0 || strcmp(value, "on") == 0) on = 1;
    }
    if (name && strcmp(name, "TCP_NODELAY") == 0)
        return gmb_net_setsockopt(h->sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (name && strcmp(name, "SO_REUSEADDR") == 0)
        return gmb_net_setsockopt(h->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (name && strcmp(name, "SO_KEEPALIVE") == 0)
        return gmb_net_setsockopt(h->sock, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    return -1;
}

/* ============================================================================
 * SSL/TLS (OpenSSL libssl). Client: skip validazione certificato (coerente con
 * SslConnect C#). Listener: carica cert PFX/P12 o PEM.
 * ============================================================================ */
#ifdef __has_include
#if __has_include(<openssl/ssl.h>) && __has_include(<openssl/err.h>)
#define GMB_HAS_SSL 1
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#endif
#endif

#ifndef GMB_HAS_SSL
#define GMB_HAS_SSL 0
#endif

#if GMB_HAS_SSL
static int g_ssl_inited = 0;
static gmb_net_lock_t g_ssl_init_lock;

void gmb_ssl_init(void)
{
    if (g_ssl_inited) return;
    gmb_net_lock(&g_ssl_init_lock);
    if (!g_ssl_inited)
    {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        g_ssl_inited = 1;
    }
    gmb_net_unlock(&g_ssl_init_lock);
}

/* crea un SSL_CTX client (verifica disabilitata — dev/self-signed) */
static SSL_CTX* ssl_client_ctx(void)
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return NULL;
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    return ctx;
}

/* carica un certificato (PEM o PFX/P12) in un SSL_CTX server */
static int ssl_load_cert(SSL_CTX* ctx, const char* path, const char* password)
{
    if (!path) return -1;
    /* prova PEM prima */
    if (SSL_CTX_use_certificate_chain_file(ctx, path) == 1 &&
        SSL_CTX_use_PrivateKey_file(ctx, path, SSL_FILETYPE_PEM) == 1)
        return 0;
    /* fallback: PKCS#12 (PFX) — i sample usano server.pfx */
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 16*1024*1024) { fclose(f); return -1; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);

    const unsigned char* p12buf = buf;
    PKCS12* p12 = d2i_PKCS12(NULL, &p12buf, (int)sz);
    if (!p12) { free(buf); return -1; }
    EVP_PKEY* pkey = NULL;
    X509* cert = NULL;
    STACK_OF(X509)* ca = NULL;
    if (PKCS12_parse(p12, password ? password : "", &pkey, &cert, &ca) != 1)
    {
        PKCS12_free(p12);
        free(buf);
        return -1;
    }
    PKCS12_free(p12);
    free(buf);
    int ok = 1;
    if (SSL_CTX_use_certificate(ctx, cert) != 1) ok = 0;
    if (SSL_CTX_use_PrivateKey(ctx, pkey) != 1) ok = 0;
    X509_free(cert);
    EVP_PKEY_free(pkey);
    if (ca) sk_X509_pop_free(ca, X509_free);
    return ok ? 0 : -1;
}

int64_t gmb_ssl_connect(const char* host, int port, int64_t timeout_ms)
{
    gmb_ssl_init();
    /* riusa la connect TCP (bloccante, con timeout via poll prima) */
    int64_t tcp_h = gmb_net_tcp_connect(host, (int)port);
    if (tcp_h < 0) return -1;
    gmb_net_handle_t* h = net_get(tcp_h);
    if (!h) { gmb_net_tcp_close(tcp_h); return -1; }
    SSL_CTX* ctx = ssl_client_ctx();
    if (!ctx) { gmb_net_tcp_close(tcp_h); return -1; }
    SSL* ssl = SSL_new(ctx);
    if (!ssl) { SSL_CTX_free(ctx); gmb_net_tcp_close(tcp_h); return -1; }
    SSL_set_fd(ssl, (int)h->sock);
    SSL_set_tlsext_host_name(ssl, host);
    SSL_set_connect_state(ssl);
    /* handshake su socket NON-bloccante (tcp_connect imposta O_NONBLOCK dopo il connect):
     * SSL_connect singolo ritorna -1 con WANT_READ → il client falliva sempre (esposto da
     * NetSslConnect/NetSslConcurrency su c99n/c99). Stesso loop del server handshake sotto. */
    int done = 0;
    for (int i = 0; i < 50 && !done; i++)
    {
        int r = SSL_connect(ssl);
        if (r == 1) { done = 1; break; }
        int err = SSL_get_error(ssl, r);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        {
            gmb_net_wait_fd((int)h->sock, 1000);
            continue;
        }
        break;
    }
    if (!done)
    {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        gmb_net_tcp_close(tcp_h);
        return -1;
    }
    /* marca l'handle come SSL client: rilascia il ctx dopo l'handshake */
    h->ssl = ssl;
    SSL_CTX_free(ctx); /* il SSL mantiene i riferimenti interni necessari */
    return tcp_h;
}

int64_t gmb_ssl_listen(int port, const char* cert_path, const char* cert_password)
{
    gmb_ssl_init();
    int64_t tcp_h = gmb_net_tcp_listen("", port);
    if (tcp_h < 0) return -1;
    gmb_net_handle_t* h = net_get(tcp_h);
    if (!h) { gmb_net_tcp_close(tcp_h); return -1; }
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) { gmb_net_tcp_close(tcp_h); return -1; }
    if (ssl_load_cert(ctx, cert_path, cert_password) != 0)
    {
        SSL_CTX_free(ctx);
        gmb_net_tcp_close(tcp_h);
        return -1;
    }
    h->ssl_ctx = ctx;
    return tcp_h;
}

int64_t gmb_ssl_accept(int64_t listener)
{
    gmb_net_handle_t* l = net_get(listener);
    if (!l || !l->is_listener || !l->ssl_ctx) return -1;
    /* accept TCP non-blocking: se nessun client, -1 */
    int64_t client = gmb_net_tcp_accept(listener);
    if (client < 0) return -1;
    return gmb_ssl_server_handshake(listener, client);
}

int64_t gmb_ssl_server_handshake(int64_t listener, int64_t client_socket)
{
    gmb_net_handle_t* l = net_get(listener);
    gmb_net_handle_t* c = net_get(client_socket);
    if (!l || !l->ssl_ctx || !c) return -1;
    SSL* ssl = SSL_new((SSL_CTX*)l->ssl_ctx);
    if (!ssl) { gmb_net_tcp_close(client_socket); return -1; }
    SSL_set_fd(ssl, (int)c->sock);
    SSL_set_accept_state(ssl);
    /* handshake non-blocking con retry su EAGAIN (usa gmb_net_wait_fd) */
    int done = 0;
    for (int i = 0; i < 50 && !done; i++)
    {
        int r = SSL_accept(ssl);
        if (r == 1) { done = 1; break; }
        int err = SSL_get_error(ssl, r);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        {
            gmb_net_wait_fd((int)c->sock, 1000);
            continue;
        }
        break;
    }
    if (!done)
    {
        SSL_free(ssl);
        gmb_net_tcp_close(client_socket);
        return -1;
    }
    c->ssl = ssl;
    return client_socket;
}

int gmb_ssl_send(int64_t handle, const uint8_t* data, size_t len)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->ssl) return -2;
    int n = SSL_write((SSL*)h->ssl, data, (int)len);
    if (n > 0) { h->ssl_want = 0; return n; }
    int err = SSL_get_error((SSL*)h->ssl, n);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    {
        h->ssl_want = (short)(err == SSL_ERROR_WANT_READ ? 1 : 2);
        return -1; /* riprovabile */
    }
    h->ssl_want = 0;
    return -2; /* errore hard */
}

int gmb_ssl_recv(int64_t handle, uint8_t* buf, size_t len)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->ssl) return -1;
    int n = SSL_read((SSL*)h->ssl, buf, (int)len);
    if (n > 0) { h->ssl_want = 0; return n; }
    int err = SSL_get_error((SSL*)h->ssl, n);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    {
        h->ssl_want = (short)(err == SSL_ERROR_WANT_READ ? 1 : 2);
        return -1;
    }
    h->ssl_want = 0;
    return n; /* 0 = chiuso pulito, <0 = errore */
}

int gmb_ssl_recv_block(int64_t handle, uint8_t* buf, size_t len, int64_t timeout_ms)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h || !h->ssl) return -1;
    int64_t deadline = timeout_ms > 0 ? gmb_now_ms_net() + timeout_ms : 0;
    for (;;)
    {
        int n = SSL_read((SSL*)h->ssl, buf, (int)len);
        if (n > 0) return n;
        int err = SSL_get_error((SSL*)h->ssl, n);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) return n;
        int64_t remain = deadline > 0 ? deadline - gmb_now_ms_net() : 0;
        if (deadline > 0 && remain <= 0) return 0;
        int w = gmb_net_wait_fd((int)h->sock, deadline > 0 ? remain : 0);
        if (w == 0) return 0;
        if (w < 0) return -1;
    }
}

/* V7b: ultimo SSL_WANT (0=nessuno, 1=read, 2=write) per scegliere gli eventi del park */
int gmb_ssl_last_want(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    return h ? (int)h->ssl_want : 0;
}

void gmb_ssl_close(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (h)
    {
        if (h->ssl) { SSL_shutdown((SSL*)h->ssl); SSL_free((SSL*)h->ssl); h->ssl = NULL; }
        if (h->ssl_ctx) { SSL_CTX_free((SSL_CTX*)h->ssl_ctx); h->ssl_ctx = NULL; }
    }
    net_release(handle);
}

int gmb_ssl_is_active(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    /* true se è un canale cifrato (ssl) O un listener TLS (ssl_ctx) — il TCP ACCEPT su un
       listener SSL deve fare l'handshake server, non un plaintext accept */
    return (h && (h->ssl || h->ssl_ctx)) ? 1 : 0;
}
#endif /* GMB_HAS_SSL */

/* ============================================================================
 * WebSocket (RFC 6455): handshake + frame codec. Client via socket TCP raw (ws://);
 * il server handshake è usato dall'HTTP ROUTE upgrade (R5).
 * ============================================================================ */
#define GMB_WS_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* invia un frame WebSocket completo; client=1 → frame mascherato */
static int ws_send_frame(int64_t handle, int opcode, const uint8_t* data, size_t len, int client_mask)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return -1;
    uint8_t hdr[14];
    size_t hlen = 2;
    hdr[0] = (uint8_t)(0x80 | (opcode & 0x0F)); /* FIN + opcode */
    uint8_t maskbit = client_mask ? 0x80 : 0x00;
    if (len < 126)
    {
        hdr[1] = (uint8_t)(maskbit | (uint8_t)len);
    }
    else if (len <= 0xFFFF)
    {
        hdr[1] = (uint8_t)(maskbit | 126);
        hdr[2] = (uint8_t)((len >> 8) & 0xFF);
        hdr[3] = (uint8_t)(len & 0xFF);
        hlen = 4;
    }
    else
    {
        hdr[1] = (uint8_t)(maskbit | 127);
        uint64_t l64 = (uint64_t)len;
        for (int i = 0; i < 8; i++) hdr[2 + i] = (uint8_t)((l64 >> (56 - 8 * i)) & 0xFF);
        hlen = 10;
    }
    uint8_t mask[4] = { 0x11, 0x22, 0x33, 0x44 };
    if (client_mask)
    {
        for (int i = 0; i < 4; i++) hdr[hlen + i] = mask[i];
        hlen += 4;
    }
    if (send(h->sock, hdr, hlen, 0) != (int)hlen) return -1;
    if (len > 0)
    {
        uint8_t* framed = NULL;
        if (client_mask)
        {
            framed = (uint8_t*)malloc(len);
            if (!framed) return -1;
            for (size_t i = 0; i < len; i++) framed[i] = data[i] ^ mask[i % 4];
            data = framed;
        }
        int sent = (int)send(h->sock, data, len, 0);
        if (framed) free(framed);
        return sent;
    }
    return 0;
}

/* legge esattamente n byte (loop su recv con gmb_net_wait_fd) */
static int ws_recv_exact(int64_t handle, uint8_t* buf, size_t n, int64_t timeout_ms)
{
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return -1;
    size_t got = 0;
    int64_t deadline = timeout_ms > 0 ? gmb_now_ms_net() + timeout_ms : 0;
    while (got < n)
    {
        int r = (int)recv(h->sock, buf + got, n - got, 0);
        if (r > 0) { got += (size_t)r; continue; }
        if (r == 0) return -1; /* chiuso */
#if defined(_WIN32) || defined(_WIN64)
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK && e != WSAEINTR) return -1;
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return -1;
#endif
        int64_t remain = deadline > 0 ? deadline - gmb_now_ms_net() : 0;
        if (deadline > 0 && remain <= 0) return -1;
        int w = gmb_net_wait_fd((int)h->sock, deadline > 0 ? remain : 0);
        if (w == 0) return -1;
        if (w < 0) return -1;
    }
    return 0;
}

int64_t gmb_ws_connect(const char* host, int port, const char* path, int64_t timeout_ms)
{
    int64_t tcp_h = gmb_net_tcp_connect(host, (int)port);
    if (tcp_h < 0) return -1;
    gmb_net_handle_t* h = net_get(tcp_h);
    if (!h) { gmb_net_tcp_close(tcp_h); return -1; }

    /* handshake HTTP/1.1 upgrade */
    char key[17];
    snprintf(key, sizeof(key), "%016llx", (unsigned long long)(gmb_now_ms_net() * 2654435761u ^ 0x9E3779B9));
    char req[1024];
    const char* p = (path && *path) ? path : "/";
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
        p, host, port, key);
    if (send(h->sock, req, strlen(req), 0) != (int)strlen(req)) { gmb_net_tcp_close(tcp_h); return -1; }

    /* legge la risposta fino a \r\n\r\n */
    char resp[2048];
    size_t total = 0;
    int64_t deadline = timeout_ms > 0 ? gmb_now_ms_net() + timeout_ms : 0;
    while (total < sizeof(resp) - 1)
    {
        int r = (int)recv(h->sock, resp + total, sizeof(resp) - 1 - total, 0);
        if (r > 0)
        {
            total += (size_t)r;
            resp[total] = 0;
            if (strstr(resp, "\r\n\r\n")) break;
            continue;
        }
        if (r == 0) { gmb_net_tcp_close(tcp_h); return -1; }
#if defined(_WIN32) || defined(_WIN64)
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK && e != WSAEINTR) { gmb_net_tcp_close(tcp_h); return -1; }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { gmb_net_tcp_close(tcp_h); return -1; }
#endif
        int64_t remain = deadline > 0 ? deadline - gmb_now_ms_net() : 0;
        if (deadline > 0 && remain <= 0) { gmb_net_tcp_close(tcp_h); return -1; }
        int w = gmb_net_wait_fd((int)h->sock, deadline > 0 ? remain : 0);
        if (w <= 0) { gmb_net_tcp_close(tcp_h); return -1; }
    }
    if (total == 0 || !strstr(resp, " 101 ")) { gmb_net_tcp_close(tcp_h); return -1; }
    return tcp_h;
}

int gmb_ws_send_text(int64_t handle, const char* text)
{
    return ws_send_frame(handle, GMB_WS_TEXT, (const uint8_t*)(text ? text : ""), text ? strlen(text) : 0, 1);
}

int gmb_ws_send_binary(int64_t handle, const uint8_t* data, size_t len)
{
    return ws_send_frame(handle, GMB_WS_BINARY, data, len, 1);
}

int gmb_ws_send_close(int64_t handle, const char* reason)
{
    size_t rl = reason ? strlen(reason) : 0;
    uint8_t payload[2 + 125];
    payload[0] = 0x03; /* close code 1000 = normal closure (big-endian) */
    payload[1] = 0xE8;
    if (rl > 125) rl = 125;
    if (rl > 0) memcpy(payload + 2, reason, rl);
    return ws_send_frame(handle, GMB_WS_CLOSE, payload, 2 + rl, 1);
}

int gmb_ws_recv_frame(int64_t handle, int* out_opcode, char** out_payload, size_t* out_payload_len, int64_t timeout_ms)
{
    *out_opcode = 0;
    *out_payload = NULL;
    *out_payload_len = 0;
    gmb_net_handle_t* h = net_get(handle);
    if (!h) return -1;

    uint8_t hdr[2];
    if (ws_recv_exact(handle, hdr, 2, timeout_ms) != 0) return -1;
    int fin = (hdr[0] & 0x80) != 0;
    int opcode = hdr[0] & 0x0F;
    int masked = (hdr[1] & 0x80) != 0;
    uint64_t len = hdr[1] & 0x7F;
    if (len == 126)
    {
        uint8_t ext[2];
        if (ws_recv_exact(handle, ext, 2, timeout_ms) != 0) return -1;
        len = ((uint64_t)ext[0] << 8) | ext[1];
    }
    else if (len == 127)
    {
        uint8_t ext[8];
        if (ws_recv_exact(handle, ext, 8, timeout_ms) != 0) return -1;
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
    }
    if (len > (64u * 1024 * 1024)) return -1; /* guardia 64MB */

    uint8_t mask[4] = { 0, 0, 0, 0 };
    if (masked && ws_recv_exact(handle, mask, 4, timeout_ms) != 0) return -1;

    char* payload = (char*)malloc(len ? (size_t)len + 1 : 1);
    if (!payload) return -1;
    if (len > 0)
    {
        if (ws_recv_exact(handle, (uint8_t*)payload, (size_t)len, timeout_ms) != 0)
        {
            free(payload);
            return -1;
        }
        if (masked)
            for (uint64_t i = 0; i < len; i++) payload[i] = (char)(((uint8_t)payload[i]) ^ mask[i % 4]);
    }
    payload[len] = 0;
    *out_opcode = opcode;
    *out_payload = payload;
    *out_payload_len = (size_t)len;
    (void)fin;
    return 0;
}

void gmb_ws_close(int64_t handle)
{
    gmb_net_handle_t* h = net_get(handle);
    if (h)
    {
        /* invia un close frame best-effort prima di chiudere */
        uint8_t payload[2] = { 0x03, 0xE8 };
        ws_send_frame(handle, GMB_WS_CLOSE, payload, 2, 1);
    }
    net_release(handle);
}

/* Handshake server WebSocket: calcola l'accept key dal Sec-WebSocket-Key e risponde 101.
   Usato dall'HTTP ROUTE upgrade. SHA1 + base64. */

int gmb_ws_server_handshake(int64_t client_socket, const char* request_headers)
{
    /* estrae Sec-WebSocket-Key (case-insensitive) */
    const char* p = request_headers;
    char key[128] = "";
    const char* found = NULL;
    while (p && *p)
    {
        const char* nl = strstr(p, "\r\n");
        size_t llen = nl ? (size_t)(nl - p) : strlen(p);
        if (llen > 2)
        {
            char line[256];
            size_t cl = llen < sizeof(line) - 1 ? llen : sizeof(line) - 1;
            memcpy(line, p, cl);
            line[cl] = 0;
            if (strncasecmp(line, "sec-websocket-key:", 18) == 0)
            {
                const char* v = line + 18;
                while (*v == ' ' || *v == '\t') v++;
                snprintf(key, sizeof(key), "%s", v);
                found = key;
                break;
            }
        }
        if (!nl) break;
        p = nl + 2;
    }
    if (!found || !*key) return -1;

    /* accept = base64(SHA1(key + GUID)) */
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, GMB_WS_MAGIC_GUID);
    uint8_t digest[20];
    gmb_sha1((const uint8_t*)combined, strlen(combined), digest);
    char* accept_b64 = gmb_base64_encode(digest, 20);
    if (!accept_b64) return -1;

    char resp[256];
    snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept_b64);
    free(accept_b64);
    /* client_socket è un fd TCP grezzo (da accept dell'HTTP server) — invia direttamente */
    int sent = (int)send(client_socket, resp, strlen(resp), 0);
    return sent < 0 ? -1 : 0; /* 0 = handshake ok (il chiamante usa != 0 come fallimento) */
}

/* ---- HTTP client via libcurl ---- */
#ifdef __has_include
#if __has_include(<curl/curl.h>)
#define GMB_HAS_CURL 1
#include <curl/curl.h>
#endif
#endif

#ifndef GMB_HAS_CURL
#define GMB_HAS_CURL 0
#endif

#if GMB_HAS_CURL
/* Persistenza cookie GLOBALE tra richieste. Il cookie engine di libcurl
 * (CURLOPT_COOKIEFILE "") è PER easy handle: ogni FETCH ne crea uno nuovo, quindi senza uno
 * share i cookie ricevuti non tornano alla richiesta successiva. Il canone HttpOptions è
 * "CookieJar null -> jar globale condiviso" (parità IL/PCode, che usano un client
 * persistente condiviso). CURL_LOCK_DATA_COOKIE condivide lo store; i lock callback lo
 * rendono thread-safe (più FETCH possono girare in parallelo sui loro pthread). */
static CURLSH* g_curl_cookie_share = NULL;
static pthread_mutex_t g_curl_cookie_share_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_curl_cookie_lock[CURL_LOCK_DATA_LAST];
static int g_curl_cookie_lock_init = 0;

static void gmb_curl_cookie_lock_cb(CURL* h, curl_lock_data data, curl_lock_access acc, void* u)
{
    (void)h; (void)acc; (void)u;
    int i = (int)data;
    if (i >= 0 && i < CURL_LOCK_DATA_LAST) pthread_mutex_lock(&g_curl_cookie_lock[i]);
}
static void gmb_curl_cookie_unlock_cb(CURL* h, curl_lock_data data, void* u)
{
    (void)h; (void)u;
    int i = (int)data;
    if (i >= 0 && i < CURL_LOCK_DATA_LAST) pthread_mutex_unlock(&g_curl_cookie_lock[i]);
}
static CURLSH* gmb_curl_get_cookie_share(void)
{
    pthread_mutex_lock(&g_curl_cookie_share_mtx);
    if (!g_curl_cookie_share)
    {
        if (!g_curl_cookie_lock_init)
        {
            for (int i = 0; i < CURL_LOCK_DATA_LAST; i++) pthread_mutex_init(&g_curl_cookie_lock[i], NULL);
            g_curl_cookie_lock_init = 1;
        }
        CURLSH* sh = curl_share_init();
        if (sh)
        {
            curl_share_setopt(sh, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
            curl_share_setopt(sh, CURLSHOPT_LOCKFUNC, gmb_curl_cookie_lock_cb);
            curl_share_setopt(sh, CURLSHOPT_UNLOCKFUNC, gmb_curl_cookie_unlock_cb);
        }
        g_curl_cookie_share = sh;
    }
    pthread_mutex_unlock(&g_curl_cookie_share_mtx);
    return g_curl_cookie_share;
}

static size_t http_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    char** out = (char**)userdata;
    size_t old = *out ? strlen(*out) : 0;
    char* nb = (char*)realloc(*out, old + total + 1);
    if (!nb) return 0;
    memcpy(nb + old, ptr, total);
    nb[old + total] = '\0';
    *out = nb;
    return total;
}

/* write callback binaria (BodyBytes) — accumula byte senza terminatore */
static size_t http_write_bytes_cb(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    struct { char* buf; size_t len; size_t cap; }* out = (void*)userdata;
    if (out->len + total + 1 > out->cap)
    {
        size_t ncap = out->cap ? out->cap * 2 : 8192;
        while (ncap < out->len + total + 1) ncap *= 2;
        char* nb = (char*)realloc(out->buf, ncap);
        if (!nb) return 0;
        out->buf = nb;
        out->cap = ncap;
    }
    memcpy(out->buf + out->len, ptr, total);
    out->len += total;
    out->buf[out->len] = '\0';
    return total;
}

/* header write callback (per out_headers "Nome: Valore\n") */
static size_t http_header_cb(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    char** out = (char**)userdata;
    /* salta la request-line "HTTP/1.1 200 OK" (niente ':') e i header già-copiati */
    const char* p = (const char*)ptr;
    size_t n = total;
    if (n >= 2 && p[n-1] == '\n') n--;
    if (n >= 1 && p[n-1] == '\r') n--;
    const char* colon = memchr(p, ':', n);
    if (!colon || n == 0) return total; /* request line o riga vuota */
    size_t old = *out ? strlen(*out) : 0;
    char* nb = (char*)realloc(*out, old + n + 2);
    if (!nb) return 0;
    memcpy(nb + old, p, n);
    nb[old + n] = '\n';
    nb[old + n + 1] = '\0';
    *out = nb;
    return total;
}

/* normalizza un'impronta hex (rimuove ':'/spazi/trattini, uppercase) — come
   HttpClientHelper.NormalizeHexFingerprint */
static void normalize_hex(const char* in, char* out, size_t out_cap)
{
    size_t j = 0;
    for (const char* p = in; *p && j < out_cap - 1; p++)
    {
        char c = *p;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            out[j++] = (c >= 'a' && c <= 'f') ? (char)(c - 'a' + 'A') : c;
    }
    out[j] = '\0';
}

/* ============================================================================
 * HTTP client — percorso con CERTIFICATE PINNING SHA-256 via OpenSSL del runtime.
 *
 * Il pin è la decisione di fiducia (stessa semantica managed HttpClientHelper /
 * VM/C typeCode-22: SHA-256 del DER del certificato presentato, con/senza ':').
 * Il percorso curl NON può fare questo su macOS: il backend SecureTransport
 * confronta l'SPKI (digest DIVERSO dal DER) e mantiene comunque la verifica della
 * catena di sistema, che rifiuta il self-signed anche con pin corretto
 * (divergenza trovata su HttpFetchAdvanced: pin CORRETTO -> status 0 su tutte le
 * gambe C, -1/200 su IL/PCodeVM). Qui: handshake con SSL_VERIFY_NONE (ctx del
 * runtime), confronto manuale dell'impronta, e se (e solo se) combacia la richiesta
 * HTTP/1.1 prosegue sul canale TLS già cifrato.
 * ============================================================================ */

/* Header dei browser preset — UNICA tabella, usata sia dal percorso curl sia da
   quello pinned (ordine = ordine sul wire, documentato su HttpOptions.BrowserPreset). */
typedef struct { const char* name; const char* value; } gmb_net_hdr_pair;

static const gmb_net_hdr_pair GMB_NET_PRESET_CHROME[] = {
    { "sec-ch-ua", "\"Not_A Brand\";v=\"8\", \"Chromium\";v=\"120\", \"Google Chrome\";v=\"120\"" },
    { "sec-ch-ua-mobile", "?0" },
    { "sec-ch-ua-platform", "\"Windows\"" },
    { "Upgrade-Insecure-Requests", "1" },
    { "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36" },
    { "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7" },
    { "Sec-Fetch-Site", "none" },
    { "Sec-Fetch-Mode", "navigate" },
    { "Sec-Fetch-User", "?1" },
    { "Sec-Fetch-Dest", "document" },
    { "Accept-Encoding", "gzip, deflate, br" },
    { "Accept-Language", "en-US,en;q=0.9" },
};
static const gmb_net_hdr_pair GMB_NET_PRESET_FIREFOX[] = {
    { "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0" },
    { "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8" },
    { "Accept-Language", "en-US,en;q=0.5" },
    { "Accept-Encoding", "gzip, deflate, br" },
    { "Upgrade-Insecure-Requests", "1" },
    { "Sec-Fetch-Dest", "document" },
    { "Sec-Fetch-Mode", "navigate" },
    { "Sec-Fetch-Site", "none" },
    { "Sec-Fetch-User", "?1" },
};
static const gmb_net_hdr_pair GMB_NET_PRESET_SAFARI[] = {
    { "User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.1 Safari/605.1.15" },
    { "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8" },
    { "Accept-Language", "en-US,en;q=0.9" },
    { "Accept-Encoding", "gzip, deflate, br" },
};

static const gmb_net_hdr_pair* gmb_net_preset_table(const char* preset, int* count)
{
    *count = 0;
    if (!preset || !*preset) return NULL;
    if (gmb_net_ieq(preset, "chrome")) { *count = (int)(sizeof(GMB_NET_PRESET_CHROME)/sizeof(GMB_NET_PRESET_CHROME[0])); return GMB_NET_PRESET_CHROME; }
    if (gmb_net_ieq(preset, "firefox")) { *count = (int)(sizeof(GMB_NET_PRESET_FIREFOX)/sizeof(GMB_NET_PRESET_FIREFOX[0])); return GMB_NET_PRESET_FIREFOX; }
    if (gmb_net_ieq(preset, "safari")) { *count = (int)(sizeof(GMB_NET_PRESET_SAFARI)/sizeof(GMB_NET_PRESET_SAFARI[0])); return GMB_NET_PRESET_SAFARI; }
    return NULL;
}

/* base64 RFC 4648 (padding incluso) — unica sorgente per l'header Basic sia del
   percorso curl sia del pinned. Ritorna stringa malloc NUL-terminata. */
static char* gmb_net_b64_encode(const uint8_t* raw, size_t len)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01623456789+/";
    char* enc = (char*)malloc(4 * ((len + 2) / 3) + 1);
    if (!enc) return NULL;
    size_t o = 0;
    size_t i = 0;
    for (; i + 2 < len; i += 3)
    {
        uint32_t v = ((uint32_t)raw[i] << 16) | ((uint32_t)raw[i+1] << 8) | (uint32_t)raw[i+2];
        enc[o++] = b64[(v >> 18) & 63];
        enc[o++] = b64[(v >> 12) & 63];
        enc[o++] = b64[(v >> 6) & 63];
        enc[o++] = b64[v & 63];
    }
    if (i + 1 == len)
    {
        uint32_t v = (uint32_t)raw[i] << 16;
        enc[o++] = b64[(v >> 18) & 63];
        enc[o++] = b64[(v >> 12) & 63];
        enc[o++] = '=';
        enc[o++] = '=';
    }
    else if (i + 2 == len)
    {
        uint32_t v = ((uint32_t)raw[i] << 16) | ((uint32_t)raw[i+1] << 8);
        enc[o++] = b64[(v >> 18) & 63];
        enc[o++] = b64[(v >> 12) & 63];
        enc[o++] = b64[(v >> 6) & 63];
        enc[o++] = '=';
    }
    enc[o] = '\0';
    return enc;
}

/* ---- COOKIEJAR / sessione — UNICA sorgente C (AGENTS §0bis) per VM/C e
   C99Native. Il jar è il dict { nome-cookie → valore } dell'oggetto NEW COOKIEJAR
   (parità managed: CommonLib.Http.CookieJar ≈ CookieContainer per-istanza; qui basta il
   per-instance perché ogni jar è il dict del PROPRIO oggetto). Il wrapper FETCH invia i
   cookie con gmb_http_jar_cookie_header() e li memorizza dalla risposta con
   gmb_http_jar_capture(); il lato server legge la sessione con
   gmb_http_parse_session_id() (parità del helper managed ParseSessionIdCookie: nome
   case-insensitive, valore CRUDO senza trim — rotondità asimmetrica documentata lì). */

static int gmb_net_ieq_pfx(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return 1;
}

char* gmb_http_parse_session_id(const char* cookie_header)
{
    if (!cookie_header || !*cookie_header) return NULL;
    static const char want[] = "GM_BASIC_SESSION";
    const char* p = cookie_header;
    while (*p)
    {
        const char* semi = strchr(p, ';');
        size_t partlen = semi ? (size_t)(semi - p) : strlen(p);
        const char* eq = (const char*)memchr(p, '=', partlen);
        if (eq && eq > p)
        {
            const char* ns = p;
            while (ns < eq && (*ns == ' ' || *ns == '\t')) ns++;
            const char* ne = eq;
            while (ne > ns && (ne[-1] == ' ' || ne[-1] == '\t')) ne--;
            if ((size_t)(ne - ns) == sizeof(want) - 1 && gmb_net_ieq_pfx(ns, want, sizeof(want) - 1))
            {
                size_t vl = partlen - (size_t)(eq + 1 - p);
                char* v = (char*)malloc(vl + 1);
                if (v) { memcpy(v, eq + 1, vl); v[vl] = '\0'; }
                return v;
            }
        }
        if (!semi) break;
        p = semi + 1;
    }
    return NULL;
}

/* Accoda "k=v" al buffer di costruzione della header Cookie (usato sotto). */
static void gmb_net_jar_append(char** buf, size_t* len, size_t* cap, const char* s)
{
    size_t sl = strlen(s);
    if (*len + sl + 1 > *cap)
    {
        size_t nc = *cap ? *cap * 2 : 128;
        while (nc < *len + sl + 1) nc *= 2;
        char* np = (char*)realloc(*buf, nc);
        if (!np) return;
        *buf = np; *cap = nc;
    }
    memcpy(*buf + *len, s, sl);
    (*len) += sl;
    (*buf)[*len] = '\0';
}

char* gmb_http_jar_cookie_header(const gmb_dict_t* jar)
{
    if (!jar) return NULL;
    char* buf = NULL; size_t len = 0, cap = 0;
    int first = 1;
    for (size_t i = 0; i < jar->count; i++)
    {
        const char* k = jar->items[i].key;
        if (!k || !*k || k[0] == '_' && k[1] == '_') continue;  /* chiavi interne (__str, ...) */
        gmb_val_t v = jar->items[i].value;
        if (v.kind != GMB_TYPE_STRING || !v.v_string.data) continue;
        gmb_net_jar_append(&buf, &len, &cap, first ? "" : "; ");
        first = 0;
        gmb_net_jar_append(&buf, &len, &cap, k);
        gmb_net_jar_append(&buf, &len, &cap, "=");
        gmb_net_jar_append(&buf, &len, &cap, v.v_string.data);
    }
    if (first) { free(buf); return NULL; }  /* jar vuoto: nessun header Cookie */
    return buf;
}

void gmb_http_jar_capture(gmb_dict_t* jar, const char* headers_block)
{
    if (!jar || !headers_block) return;
    const char* line = headers_block;
    while (line && *line)
    {
        const char* eol = strchr(line, '\n');
        size_t llen = eol ? (size_t)(eol - line) : strlen(line);
        if (eol && llen && line[llen - 1] == '\r') llen--;
        if (llen > 12 && gmb_net_ieq_pfx(line, "set-cookie:", 11))
        {
            const char* v = line + 11;
            while (v < line + (llen > 11 ? llen : 11) && (*v == ' ' || *v == '\t')) v++;
            size_t vlen = (size_t)(line + llen - v);
            const char* semi = (const char*)memchr(v, ';', vlen);
            size_t pairlen = semi ? (size_t)(semi - v) : vlen;
            const char* eq = (const char*)memchr(v, '=', pairlen);
            if (eq && eq > v)
            {
                size_t nl = (size_t)(eq - v);
                char* name = (char*)malloc(nl + 1);
                char* val = (char*)malloc(pairlen - nl);
                if (name && val)
                {
                    memcpy(name, v, nl); name[nl] = '\0';
                    memcpy(val, eq + 1, pairlen - nl - 1); val[pairlen - nl - 1] = '\0';
                    gmb_dict_set(jar, name, gmb_make_val_string(val));
                }
                free(name); free(val);
            }
        }
        if (!eol) break;
        line = eol + 1;
    }
}

/* HTTPRESPONSE __headers: stringa "Nome: Valore\n..." che l'handler wrapper legge e
 * monta su *out_headers (il server runtime la propaga in risposta). Unica sorgente
 * condivisa (§0bis) per SetCookie/SetHeader/Header arbitrario — VM/C,
 * C99Native la chiamano, nessun emitter duplica la concat. */
void gmb_http_response_append_header(gmb_val_t res, const char* line)
{
    if (res.kind != GMB_TYPE_OBJECT) return;
    gmb_obj_t* o = (gmb_obj_t*)res.v_ptr;
    if (!o || !o->data) return;
    gmb_dict_t* d = (gmb_dict_t*)o->data;
    gmb_val_t hv = gmb_dict_get(*d, "__headers");
    const char* cur = (hv.kind == GMB_TYPE_STRING && hv.v_string.data) ? hv.v_string.data : "";
    if (!line) return;
    size_t need = strlen(cur) + strlen(line) + 2;
    char* nb = (char*)malloc(need);
    if (!nb) return;
    snprintf(nb, need, "%s%s\n", cur, line);
    gmb_dict_set(d, "__headers", gmb_make_val_string(nb));
    free(nb);
}

void gmb_http_response_set_cookie(gmb_val_t res, const char* name, const char* value)
{
    if (!name) return;
    size_t need = strlen(name) + (value ? strlen(value) : 0) + 16;
    char* hline = (char*)malloc(need);
    if (!hline) return;
    snprintf(hline, need, "Set-Cookie: %s=%s", name, value ? value : "");
    gmb_http_response_append_header(res, hline);
    free(hline);
}

/* Mappa estensione -> Content-Type. SORGENTE UNICA (parità bit-exact del reference C#
 * PCodeHttpResponse.GuessContentType): prima le gambe C mappavano solo .zip/.json e per il
 * resto lasciavano il default "text/plain" del server, divergendo da IL/PCode/VM-C su .png,
 * .txt, .html, ... La usano il helper condiviso e il VM/C inline (nessuna copia). */
const char* gmb_http_guess_content_type(const char* path)
{
    const char* ext = path ? strrchr(path, '.') : NULL;
    if (!ext) return "application/octet-stream";
    if (strcasecmp(ext, ".zip") == 0) return "application/zip";
    if (strcasecmp(ext, ".json") == 0) return "application/json";
    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcasecmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcasecmp(ext, ".js") == 0) return "application/javascript";
    if (strcasecmp(ext, ".png") == 0) return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, ".gif") == 0) return "image/gif";
    if (strcasecmp(ext, ".svg") == 0) return "image/svg+xml";
    return "application/octet-stream";
}

void gmb_http_response_send_file(gmb_val_t res, const char* path, const char* content_type)
{
    FILE* ff;
    if (!path || !*path) return;
    ff = fopen(path, "rb");
    if (!ff)
    {
        /* Status E StatusCode: il wrapper C99 legge "StatusCode" (parità con res.SetStatus),
         * il serializer VM/C legge entrambe le chiavi; settarle tutte e due mantiene la
         * parità di status 500 su tutte le gambe. */
        gmb_val_dict_set(res, "Status", gmb_make_val_double(500.0));
        gmb_val_dict_set(res, "StatusCode", gmb_make_val_double(500.0));
        gmb_val_dict_set(res, "Body", gmb_make_val_string("500 SendFile: file non leggibile"));
        gmb_val_dict_set(res, "__body_len", gmb_make_val_double(0.0));
        return;
    }
    fseek(ff, 0, SEEK_END);
    {
        long fsz = ftell(ff);
        fseek(ff, 0, SEEK_SET);
        if (fsz < 0) fsz = 0;
        {
            char* bb = (char*)malloc((size_t)fsz + 1);
            if (bb)
            {
                size_t got = fsz ? fread(bb, 1, (size_t)fsz, ff) : 0;
                bb[got] = 0;
                /* gmb_make_val_string_counted_n COPIA n byte (NUL interni preservati: uno ZIP
                 * ha 0x00 a offset 5) → il buffer temporaneo si libera. */
                gmb_val_dict_set(res, "Body", gmb_make_val_string_counted_n(bb, got));
                gmb_val_dict_set(res, "__body_len", gmb_make_val_double((double)got));
                free(bb);
            }
        }
    }
    fclose(ff);
    if (content_type && *content_type)
        gmb_val_dict_set(res, "ContentType", gmb_make_val_string(content_type));
    else
        gmb_val_dict_set(res, "ContentType", gmb_make_val_string(gmb_http_guess_content_type(path)));
}

#if GMB_HAS_SSL
#include <openssl/evp.h>

/* buffer dinamico per la risposta grezza */
typedef struct { char* p; size_t len, cap; } gmb_net_sbuf_t;
static void gmb_net_sbuf_add(gmb_net_sbuf_t* b, const char* s, size_t n)
{
    if (b->len + n + 1 > b->cap)
    {
        size_t nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->len + n + 1) nc *= 2;
        char* np = (char*)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

/* Impronta SHA-256 (DER, esadecimale maiuscolo) del certificato presentato dal peer. */
static int gmb_net_peer_cert_sha256(int64_t h, char* out, size_t out_cap)
{
    gmb_net_handle_t* hh = net_get(h);
    if (!hh || !hh->ssl) return -1;
    X509* cert = SSL_get1_peer_certificate((SSL*)hh->ssl);
    if (!cert) return -1;
    unsigned char* der = NULL;
    int derl = i2d_X509(cert, &der);
    X509_free(cert);
    if (derl <= 0 || !der) return -1;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdlen = 0;
    int ok = EVP_Digest(der, (size_t)derl, md, &mdlen, EVP_sha256(), NULL);
    OPENSSL_free(der);
    if (!ok || mdlen * 2 + 1 > out_cap) return -1;
    static const char hx[] = "0123456789ABCDEF";
    for (unsigned int i = 0; i < mdlen; i++)
    {
        out[i*2] = hx[md[i] >> 4];
        out[i*2+1] = hx[md[i] & 15];
    }
    out[mdlen*2] = '\0';
    return 0;
}

static int gmb_net_https_pinned(const char* url, const gmb_net_http_options_t* opts,
                                int* out_status, char** out_body, size_t* out_body_len,
                                char** out_error, char** out_headers)
{
    char cur[2048];
    snprintf(cur, sizeof(cur), "%s", url);
    long timeout = (opts && opts->timeout_ms > 0) ? opts->timeout_ms : 30000;
    int allowRedirects = !opts || opts->allow_redirects;
    int maxRedirects = (opts && opts->max_redirects > 0) ? opts->max_redirects : 10;
    const char* method = (opts && opts->method && *opts->method) ? opts->method : "GET";
    int64_t deadline = gmb_now_ms_net() + timeout;

    for (int hop = 0; ; hop++)
    {
        /* parse https://host[:port][/path] */
        if (strncmp(cur, "https://", 8) != 0)
        {
            if (out_error) *out_error = strdup("pinning requires https URL");
            return -1;
        }
        char host[512]; int port = 443; char path[1536] = "/";
        const char* hp = cur + 8;
        const char* slash = strchr(hp, '/');
        size_t hostlen = slash ? (size_t)(slash - hp) : strlen(hp);
        if (hostlen == 0 || hostlen >= sizeof(host)) { if (out_error) *out_error = strdup("bad url"); return -1; }
        memcpy(host, hp, hostlen); host[hostlen] = '\0';
        char* colon = strrchr(host, ':');
        if (colon && colon != host) { *colon = '\0'; port = atoi(colon + 1); if (port <= 0) port = 443; }
        if (slash)
        {
            const char* qs = strchr(slash, '?');
            snprintf(path, sizeof(path), "%s", slash);
            (void)qs;
        }

        int64_t h = gmb_ssl_connect(host, port, deadline - gmb_now_ms_net());
        if (h < 0)
        {
            if (out_error) *out_error = strdup("TLS connect failed");
            return -1;
        }

        /* Decisione di fiducia, DUE modalità (percorso OpenSSL unico, §0bis):
           - PIN presente: SHA-256 del DER == impronta configurata (unica fiducia,
             il pin SOSTITUISCE la catena — passa anche un self-signed).
           - AllowInsecureSSL (nessun pin): nessuna verifica — ssl_client_ctx è già
             SSL_VERIFY_NONE. Serve perché su macOS il curl di sistema usa
             SecureTransport, che IGNORA CURLOPT_SSL_VERIFYPEER=0 sui certificati
             self-signed (rc=60 / alert "unknown ca"): l'unica via che realizza la
             semantica managed di AllowInsecureSSL è questo client OpenSSL. */
        if (opts && opts->pinned_cert_sha256 && *opts->pinned_cert_sha256)
        {
            char got[129];
            if (gmb_net_peer_cert_sha256(h, got, sizeof(got)) != 0)
            {
                gmb_ssl_close(h);
                if (out_error) *out_error = strdup("no peer certificate");
                return -1;
            }
            char want[128];
            normalize_hex(opts->pinned_cert_sha256, want, sizeof(want));
            if (!gmb_net_ieq(got, want))
            {
                gmb_ssl_close(h);
                if (out_error) *out_error = strdup("SSL: certificate does not match pinned SHA-256");
                return -1;
            }
        }

        /* costruisce la richiesta HTTP/1.1 (Connection: close) */
        gmb_net_sbuf_t req = { 0 };
        char line[1024];
        snprintf(line, sizeof(line), "%s %s HTTP/1.1\r\n", method, path);
        gmb_net_sbuf_add(&req, line, strlen(line));
        if (port == 443) snprintf(line, sizeof(line), "Host: %s\r\n", host);
        else snprintf(line, sizeof(line), "Host: %s:%d\r\n", host, port);
        gmb_net_sbuf_add(&req, line, strlen(line));
        gmb_net_sbuf_add(&req, "Connection: close\r\n", 19);

        int presetCount = 0;
        const gmb_net_hdr_pair* preset = gmb_net_preset_table(opts ? opts->browser_preset : NULL, &presetCount);
        for (int i = 0; i < presetCount; i++)
        {
            snprintf(line, sizeof(line), "%s: %s\r\n", preset[i].name, preset[i].value);
            gmb_net_sbuf_add(&req, line, strlen(line));
        }
        /* auth (Basic > Bearer) se nessun Authorization esplicito */
        int has_auth = 0, has_ua = presetCount > 0, has_cookie = 0;
        for (int i = 0; opts && i < opts->header_count; i++)
        {
            if (opts->header_names[i] && gmb_net_ieq(opts->header_names[i], "Authorization") && opts->header_values[i] && *opts->header_values[i]) has_auth = 1;
            if (opts->header_names[i] && gmb_net_ieq(opts->header_names[i], "User-Agent") && opts->header_values[i] && *opts->header_values[i]) has_ua = 1;
            if (opts->header_names[i] && gmb_net_ieq(opts->header_names[i], "Cookie") && opts->header_values[i] && *opts->header_values[i]) has_cookie = 1;
        }
        if (!has_auth && opts && opts->username && *opts->username)
        {
            size_t ul = strlen(opts->username), pl = opts->password ? strlen(opts->password) : 0;
            char* raw = (char*)malloc(ul + pl + 2);
            if (raw)
            {
                memcpy(raw, opts->username, ul);
                raw[ul] = ':';
                if (pl) memcpy(raw + ul + 1, opts->password, pl);
                raw[ul + pl + 1] = '\0';
                char* enc = gmb_net_b64_encode((const uint8_t*)raw, ul + pl + 1);
                if (enc)
                {
                    snprintf(line, sizeof(line), "Authorization: Basic %s\r\n", enc);
                    gmb_net_sbuf_add(&req, line, strlen(line));
                    free(enc);
                }
                free(raw);
            }
        }
        else if (!has_auth && opts && opts->bearer_token && *opts->bearer_token)
        {
            snprintf(line, sizeof(line), "Authorization: Bearer %s\r\n", opts->bearer_token);
            gmb_net_sbuf_add(&req, line, strlen(line));
        }
        if (!has_ua) { gmb_net_sbuf_add(&req, "User-Agent: GM-BASIC\r\n", 22); }
        for (int i = 0; opts && i < opts->header_count; i++)
        {
            if (!opts->header_names[i] || !opts->header_values[i]) continue;
            snprintf(line, sizeof(line), "%s: %s\r\n", opts->header_names[i], opts->header_values[i]);
            gmb_net_sbuf_add(&req, line, strlen(line));
        }
        /* COOKIEJAR (come nel percorso curl): solo se nessun Cookie esplicito */
        if (opts && opts->cookie_header && *opts->cookie_header && !has_cookie)
        {
            snprintf(line, sizeof(line), "Cookie: %s\r\n", opts->cookie_header);
            gmb_net_sbuf_add(&req, line, strlen(line));
        }
        if (opts && opts->body)
        {
            snprintf(line, sizeof(line), "Content-Length: %zu\r\n\r\n", opts->body_len);
            gmb_net_sbuf_add(&req, line, strlen(line));
            gmb_net_sbuf_add(&req, opts->body, opts->body_len);
        }
        else
        {
            gmb_net_sbuf_add(&req, "\r\n", 2);
        }

        /* send (canale non-bloccante: retry su WANT) */
        size_t off = 0;
        int send_err = 0;
        gmb_net_handle_t* sh = net_get(h);
        while (off < req.len)
        {
            if (gmb_now_ms_net() > deadline) { send_err = 1; break; }
            int n = gmb_ssl_send(h, (const uint8_t*)req.p + off, req.len - off);
            if (n > 0) { off += (size_t)n; continue; }
            if (n == -1) { if (sh) gmb_net_wait_fd((int)sh->sock, 50); continue; }
            send_err = 1; break;
        }
        free(req.p);
        if (send_err)
        {
            gmb_ssl_close(h);
            if (out_error) *out_error = strdup("TLS send failed");
            return -1;
        }

        /* read until close (Connection: close onora la fine del corpo) */
        gmb_net_sbuf_t rsp = { 0 };
        char rbuf[8192];
        for (;;)
        {
            int64_t remain = deadline - gmb_now_ms_net();
            if (remain <= 0) break;
            int n = gmb_ssl_recv_block(h, (uint8_t*)rbuf, sizeof(rbuf), remain);
            if (n > 0) { gmb_net_sbuf_add(&rsp, rbuf, (size_t)n); continue; }
            break; /* 0 = chiuso, <0 = errore: si chiude comunque */
        }
        gmb_ssl_close(h);

        /* parse status + headers (accumulati come fa curl: ogni blocco) */
        char* sep = rsp.p ? strstr(rsp.p, "\r\n\r\n") : NULL;
        if (!sep)
        {
            free(rsp.p);
            if (out_error) *out_error = strdup("TLS read failed or malformed response");
            return -1;
        }
        int status = 0;
        sscanf(rsp.p, "HTTP/%*s %d", &status);
        /* Location per redirect */
        char location[2048] = "";
        {
            size_t hlen = (size_t)(sep - rsp.p);
            for (const char* q = rsp.p; (size_t)(q - rsp.p) < hlen; )
            {
                const char* eol = strstr(q, "\r\n");
                if (!eol || (size_t)(eol - rsp.p) > hlen) break;
                if (strncmp(q, "Location:", 9) == 0)
                {
                    const char* v = q + 9;
                    while (v < eol && (*v == ' ' || *v == '\t')) v++;
                    size_t vl = (size_t)(eol - v);
                    if (vl >= sizeof(location)) vl = sizeof(location) - 1;
                    memcpy(location, v, vl); location[vl] = '\0';
                }
                q = eol + 2;
            }
        }

        /* corpo: tutto ciò che segue l'header block (server chiude la connessione) */
        size_t blen = rsp.len - ((size_t)(sep - rsp.p) + 4);
        char* body = (char*)malloc(blen + 1);
        if (body) { memcpy(body, sep + 4, blen); body[blen] = '\0'; }
        char* hdrblock = NULL;
        {
            size_t hl = (size_t)(sep - rsp.p) + 2;
            hdrblock = (char*)malloc(hl + 1);
            if (hdrblock) { memcpy(hdrblock, rsp.p, hl); hdrblock[hl] = '\0'; }
        }
        free(rsp.p);

        if (allowRedirects && status >= 300 && status < 400 && location[0] && hop < maxRedirects)
        {
            if (location[0] == '/') snprintf(cur, sizeof(cur), "https://%s%s", host, location);
            else if (strncmp(location, "https://", 8) == 0) snprintf(cur, sizeof(cur), "%s", location);
            else snprintf(cur, sizeof(cur), "https://%s/%s", host, location);
            free(body); free(hdrblock);
            continue;
        }

        if (out_status) *out_status = status;
        if (out_body) *out_body = body ? body : strdup("");
        else free(body);
        if (out_body_len) *out_body_len = blen;
        if (out_headers) *out_headers = hdrblock;
        else free(hdrblock);
        return 0;
    }
}
#endif /* GMB_HAS_SSL (pinned path) */

int gmb_net_http_request(const char* url, const gmb_net_http_options_t* opts,
                         int* out_status, char** out_body, size_t* out_body_len,
                         char** out_error, char** out_headers)
{
    /* HTTPS con una DECISIONE DI FIDUCIA non-standard (pin SHA-256 oppure
       AllowInsecureSSL): usa il percorso OpenSSL del runtime, non curl. Motivo:
       su macOS il curl di sistema è SecureTransport e ignora CURLOPT_SSL_VERIFYPEER=0
       sui self-signed (rc=60), mentre ssl_client_ctx è SSL_VERIFY_NONE — unica via
       che realizzi la semantica managed. Il pin, se presente, è verificato lì.
       L'HTTPS ordinario (né pin né insecure) resta su curl, che valida la catena. */
    if (opts && strncmp(url, "https://", 8) == 0 &&
        ((opts->pinned_cert_sha256 && *opts->pinned_cert_sha256) || opts->allow_insecure_ssl))
    {
#if GMB_HAS_SSL
        if (out_status) *out_status = 0;
        if (out_body) *out_body = NULL;
        if (out_body_len) *out_body_len = 0;
        if (out_error) *out_error = NULL;
        if (out_headers) *out_headers = NULL;
        return gmb_net_https_pinned(url, opts, out_status, out_body, out_body_len, out_error, out_headers);
#else
        if (opts->pinned_cert_sha256 && *opts->pinned_cert_sha256)
        {
            if (out_error) *out_error = strdup("certificate pinning requires OpenSSL runtime");
            return -1;
        }
        /* insecure senza OpenSSL: si cade su curl (Linux: OpenSSL backend, rispetta
           VERIFYPEER=0); su altre piattaforme può fallire, errore da curl. */
#endif
    }

    if (out_status) *out_status = 0;
    if (out_body) *out_body = NULL;
    if (out_body_len) *out_body_len = 0;
    if (out_error) *out_error = NULL;
    if (out_headers) *out_headers = NULL;

    CURL* c = curl_easy_init();
    if (!c) { if (out_error) *out_error = strdup("curl init failed"); return -1; }

    const char* method = (opts && opts->method && *opts->method) ? opts->method : "GET";
    long timeout = (opts && opts->timeout_ms > 0) ? opts->timeout_ms : 30000;
    int allowRedirects = !opts || opts->allow_redirects;      /* default true */
    int maxRedirects = (opts && opts->max_redirects > 0) ? opts->max_redirects : 10;
    int insecure = (opts && opts->allow_insecure_ssl);

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, allowRedirects ? 1L : 0L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, (long)maxRedirects);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, timeout);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    if (insecure)
    {
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    /* metodo (default GET): per GET con body si usa CUSTOMREQUEST */
    if (strcmp(method, "GET") != 0)
        curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, method);

    /* cookie engine: use_cookies → store cookie GLOBALE e PERSISTENTE tra richieste.
       CURLOPT_SHARE (CURL_LOCK_DATA_COOKIE) fa sì che lo store del cookie engine sia condiviso
       tra gli easy handle (ogni FETCH ne crea uno nuovo) → i cookie ricevuti tornano alla
       richiesta successiva (canone: CookieJar null -> jar globale condiviso). */
    if (opts && opts->use_cookies)
    {
        CURLSH* cookie_share = gmb_curl_get_cookie_share();
        if (cookie_share) curl_easy_setopt(c, CURLOPT_SHARE, cookie_share);
        curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");   /* abilita il cookie engine */
        curl_easy_setopt(c, CURLOPT_COOKIEJAR, "");    /* in memoria (share) */
    }

    /* auth Basic/Bearer — solo se Authorization non esplicito (gestito negli header).
       L'header auth va NELLA lista headers condivisa (insieme a preset e custom). */
    struct curl_slist* headers = NULL;
    int has_auth_header = 0;
    if (opts)
        for (int i = 0; i < opts->header_count && !has_auth_header; i++)
            if (opts->header_names[i] && gmb_net_ieq(opts->header_names[i], "Authorization"))
                has_auth_header = 1;
    if (!has_auth_header)
    {
        if (opts && opts->username && *opts->username)
        {
            /* Basic base64(user:pass) — costruiamo l'header manualmente */
            size_t ul = strlen(opts->username), pl = opts->password ? strlen(opts->password) : 0;
            char* raw = (char*)malloc(ul + pl + 2);
            if (raw)
            {
                memcpy(raw, opts->username, ul);
                raw[ul] = ':';
                if (pl) memcpy(raw + ul + 1, opts->password, pl);
                raw[ul + pl + 1] = '\0';
                char* enc = gmb_net_b64_encode((const uint8_t*)raw, ul + pl + 1);
                if (enc)
                {
                    char* authhdr = (char*)malloc(21 + strlen(enc) + 1);
                    if (authhdr)
                    {
                        sprintf(authhdr, "Authorization: Basic %s", enc);
                        headers = curl_slist_append(headers, authhdr);
                        free(authhdr);
                    }
                    free(enc);
                }
                free(raw);
            }
        }
        else if (opts && opts->bearer_token && *opts->bearer_token)
        {
            char* authhdr = (char*)malloc(22 + strlen(opts->bearer_token) + 1);
            if (authhdr)
            {
                sprintf(authhdr, "Authorization: Bearer %s", opts->bearer_token);
                headers = curl_slist_append(headers, authhdr);
                free(authhdr);
            }
        }
    }

    /* proxy */
    if (opts && opts->proxy_url && *opts->proxy_url)
    {
        curl_easy_setopt(c, CURLOPT_PROXY, opts->proxy_url);
        if (opts->proxy_username && *opts->proxy_username)
        {
            char* p = (char*)malloc((opts->proxy_username ? strlen(opts->proxy_username) : 0)
                                    + (opts->proxy_password ? strlen(opts->proxy_password) : 0) + 2);
            if (p)
            {
                sprintf(p, "%s:%s", opts->proxy_username ? opts->proxy_username : "",
                        opts->proxy_password ? opts->proxy_password : "");
                curl_easy_setopt(c, CURLOPT_PROXYUSERPWD, p);
                free(p);
            }
        }
    }

    /* certificato client (PFX/P12) */
    if (opts && opts->client_cert_path && *opts->client_cert_path)
    {
        curl_easy_setopt(c, CURLOPT_SSLCERT, opts->client_cert_path);
        curl_easy_setopt(c, CURLOPT_SSLCERTTYPE, "P12");
        if (opts->client_cert_password)
            curl_easy_setopt(c, CURLOPT_KEYPASSWD, opts->client_cert_password);
    }

    /* pinning SHA-256: gestito INTERAMENTE dal percorso dedicato gmb_net_https_pinned
       (dispatch in cima a gmb_net_http_request). CURLOPT_PINNEDPUBLICKEY è rimosso:
       ha semantica SPKI + catena di sistema (SecureTransport lo rifiuta sul
       self-signed), divergente dal contratto managed (DER-SHA256 che SOSTITUISCE la
       fiducia). Su http:// il pin managed non scatta mai — qui come lì, nessun effetto. */

    /* browser preset: header realistici nell'ordine del preset (prima degli espliciti) */
    struct curl_slist* preset_headers = NULL;
    {
        int presetCount = 0;
        const gmb_net_hdr_pair* preset = gmb_net_preset_table(opts ? opts->browser_preset : NULL, &presetCount);
        for (int i = 0; i < presetCount; i++)
        {
            char hdr[1024];
            snprintf(hdr, sizeof(hdr), "%s: %s", preset[i].name, preset[i].value);
            preset_headers = curl_slist_append(preset_headers, hdr);
        }
        /* concatena l'header auth (già in `headers`) dopo i preset */
        if (preset_headers)
        {
            struct curl_slist* tail = preset_headers;
            while (tail->next) tail = tail->next;
            tail->next = headers;
            headers = preset_headers;
        }
    }

    /* header custom espliciti (precedenza: rimpiazza il preset per lo stesso nome) */
    if (opts)
    {
        for (int i = 0; i < opts->header_count; i++)
        {
            if (!opts->header_names[i] || !opts->header_values[i]) continue;
            const char* key = opts->header_names[i];
            size_t kl = strlen(key), vl = strlen(opts->header_values[i]);
            char* hdr = (char*)malloc(kl + vl + 3);
            if (hdr)
            {
                memcpy(hdr, key, kl);
                hdr[kl] = ':';
                hdr[kl + 1] = ' ';
                memcpy(hdr + kl + 2, opts->header_values[i], vl);
                hdr[kl + vl + 2] = '\0';
                headers = curl_slist_append(headers, hdr);
                free(hdr);
            }
        }
    }
    /* COOKIEJAR (opts.cookie_header, montato dal chiamante con
       gmb_http_jar_cookie_header): applicato SOLO se nessun header Cookie esplicito è
       già presente (parità managed: CookieContainer allegato all'handler; un Cookie
       scritto a mano vince). */
    if (opts && opts->cookie_header && *opts->cookie_header)
    {
        int has_cookie = 0;
        for (int i = 0; i < opts->header_count; i++)
            if (opts->header_names[i] && gmb_net_ieq(opts->header_names[i], "Cookie")) { has_cookie = 1; break; }
        if (!has_cookie)
        {
            char* hdr = (char*)malloc(strlen(opts->cookie_header) + 10);
            if (hdr) { sprintf(hdr, "Cookie: %s", opts->cookie_header); headers = curl_slist_append(headers, hdr); free(hdr); }
        }
    }
    if (headers) curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);

    /* corpo richiesta */
    if (opts && opts->body)
    {
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, opts->body);
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)opts->body_len);
        if (strcmp(method, "GET") != 0)
        {
            /* per i metodi con body: POST-like */
            if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0
                || strcmp(method, "PATCH") == 0 || strcmp(method, "DELETE") == 0)
            {
                /* CURLOPT_CUSTOMREQUEST già impostato; body va con POSTFIELDS */
            }
        }
    }

    /* risposta */
    struct { char* buf; size_t len; size_t cap; } body_out = { NULL, 0, 0 };
    char* headers_out = NULL;
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, http_write_bytes_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body_out);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, http_header_cb);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &headers_out);

    CURLcode rc = curl_easy_perform(c);

    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);

    if (headers) curl_slist_free_all(headers);
    /* headers e preset_headers sono la STESSA lista quando il preset è applicato
       (headers = preset_headers) — mai liberare entrambi (double free). */
    curl_easy_cleanup(c);

    if (rc != CURLE_OK)
    {
        if (out_error) *out_error = strdup(curl_easy_strerror(rc));
        free(body_out.buf);
        free(headers_out);
        return -1;
    }

    if (out_status) *out_status = (int)status;
    if (out_body) *out_body = body_out.buf ? body_out.buf : strdup("");
    if (out_body_len) *out_body_len = body_out.len;
    if (out_headers) *out_headers = headers_out;
    else free(headers_out);
    return 0;
}

/* --- download in streaming (libcurl) con barra di avanzamento disegnata dal C --- */
typedef struct { FILE* f; int show; int last_pct; } gmb_dl_state_t;

static size_t gmb_dl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    gmb_dl_state_t* s = (gmb_dl_state_t*)userdata;
    return fwrite(ptr, size, nmemb, s->f);
}

static int gmb_dl_xfer_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                          curl_off_t ultotal, curl_off_t ulnow)
{
    (void)ultotal; (void)ulnow;
    gmb_dl_state_t* s = (gmb_dl_state_t*)clientp;
    if (!s || !s->show) return 0;
    if (dltotal > 0)
    {
        int pct = (int)((dlnow * 100) / dltotal);
        if (pct != s->last_pct)
        {
            s->last_pct = pct;
            int bars = pct / 2;
            fprintf(stderr, "\r  [");
            for (int i = 0; i < 50; i++) fputc(i < bars ? '#' : '-', stderr);
            fprintf(stderr, "] %3d%%  %.1f/%.1f MB", pct,
                    (double)dlnow / 1048576.0, (double)dltotal / 1048576.0);
            fflush(stderr);
        }
    }
    else if (dlnow > 0)
    {
        fprintf(stderr, "\r  scaricati %.1f MB...", (double)dlnow / 1048576.0);
        fflush(stderr);
    }
    return 0;
}

int gmb_net_http_download(const char* url, const char* dest_path, int show_progress)
{
    CURL* c = curl_easy_init();
    if (!c) return -1;
    FILE* f = fopen(dest_path, "wb");
    if (!f) { curl_easy_cleanup(c); return -1; }
    gmb_dl_state_t st;
    st.f = f; st.show = show_progress; st.last_pct = -1;

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 600000L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, gmb_dl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &st);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, gmb_dl_xfer_cb);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &st);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);

    CURLcode rc = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    fclose(f);
    curl_easy_cleanup(c);
    if (show_progress) fprintf(stderr, "\n");
    if (rc != CURLE_OK || code >= 400) { remove(dest_path); return -1; }
    return 0;
}

char* gmb_net_http_get(const char* url)
{
    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.method = "GET";
    opts.timeout_ms = 15000;
    int status = 0;
    char* body = NULL;
    size_t blen = 0;
    char* err = NULL;
    if (gmb_net_http_request(url, &opts, &status, &body, &blen, &err, NULL) != 0)
    {
        if (body) free(body);
        free(err);
        return NULL;
    }
    return body;
}

/* Client HTTP semplificato (GET/POST/PUT/DELETE) → body (malloc) o NULL su errore.
   Fonte unica per C99/C99Native/VM-C (§0bis). */
char* gmb_net_http_body(const char* method, const char* url, const char* body)
{
    gmb_net_http_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.method = (method && *method) ? method : "GET";
    opts.timeout_ms = 15000;
    if (body && *body) { opts.body = body; opts.body_len = strlen(body); }
    int status = 0;
    char* rb = NULL;
    size_t rl = 0;
    char* re = NULL;
    char* rh = NULL;
    if (gmb_net_http_request(url, &opts, &status, &rb, &rl, &re, &rh) != 0)
    {
        if (rb) free(rb);
        free(re);
        free(rh);
        return NULL;
    }
    free(re);
    free(rh);
    return rb;
}
#else
char* gmb_net_http_get(const char* url)
{
    /* fallback: nessuna libcurl — HTTP GET minimale via socket (non implementato) */
    (void)url;
    return NULL;
}
char* gmb_net_http_body(const char* method, const char* url, const char* body)
{
    (void)method; (void)url; (void)body;
    return NULL;
}
#endif
/* ============================================================================
 * HTTP server (route verso la VM)
 * ============================================================================ */
#define GMB_HTTP_MAX_ROUTES 64
#define GMB_HTTP_MAX_HEADERS 128

typedef struct {
    char method[16];
    char path[256];
    /* segmenti del template: seg[0]="users", seg[1]="{id}", ... (0 se path="*") */
    char path_seg[16][64];
    int path_seg_count;
    gmb_http_handler_fn handler;
    /* Forma BINARIA della route (res.SendFile): quando handler_bin e' valorizzato il
     * server usa handler_bin e la lunghezza che dichiara, invece di strlen(out_body).
     * Coesiste con `handler` (usata dal C generato da gmb_c99native, firma storica). */
    gmb_http_handler_bin_fn handler_bin;
    void* ctx;
    int in_use;
} gmb_http_route_t;

typedef struct {
    gmb_net_sock_t sock;         /* listener */
    gmb_net_thread_t thread;
    int running;
    gmb_http_route_t routes[GMB_HTTP_MAX_ROUTES];
    /* HTTP USE (middleware): stesso struct delle route; method legacy "*", path = PREFISSO
     * (match StartsWith, non template), ordine di registrazione. Parità PCodeHttpRouter. */
    gmb_http_route_t middlewares[GMB_HTTP_MAX_ROUTES];
    int port;
    /* statici: prefix → dir fisica (max 16) */
    char static_prefix[16][64];
    char static_dir[16][512];
    int static_count;
    int request_timeout_ms;      /* default 30000 */
    SSL_CTX* ssl_ctx;            /* non-NULL = server HTTPS (TLS) */
    char bsp_dir[512];           /* radice pagine .bsp (HTTP BSP); vuoto = disattivo */
    /* Security (2026-09-02, parità HTTP CONFIG IL/PCode): header automatici + IP allow/block */
    int security_headers;        /* 1 = aggiungi HSTS/CSP/X-Frame-Options/... ad ogni risposta */
    char allowed_ip[512];        /* allow-list IP singoli/CIDR IPv4, virgola; vuoto = tutti */
    char blocked_ip[512];        /* block-list IP singoli/CIDR IPv4, virgola; vuoto = nessuno */
    long max_body;               /* MaxRequestBodySize; 0 = illimitato */
    int rate_per_min;            /* RateLimitRequestsPerMinute; 0 = disabilitato */
    int rate_burst;              /* RateLimitBurstSize (capacity token bucket) */
    double rate_tokens;          /* token bucket: token disponibili */
    int64_t rate_last_ms;        /* ultimo refill (ms) */
    gmb_net_lock_t rate_lock;    /* spinlock (connessioni concorrenti) */
} gmb_http_server_t;

static int gmb_http_is_dir(const char* p)
{
    struct stat st;
    if (!p || !*p) return 0;
    if (stat(p, &st) != 0) return 0;
#ifdef S_ISDIR
    return S_ISDIR(st.st_mode) ? 1 : 0;
#else
    return (st.st_mode & _S_IFDIR) != 0;
#endif
}

static void gmb_http_maybe_infer_bsp(gmb_http_server_t* s, const char* static_dir)
{
    char pages[512];
    if (!s || s->bsp_dir[0]) return;
    if (static_dir && *static_dir)
    {
        snprintf(pages, sizeof(pages), "%s/../pages", static_dir);
        if (gmb_http_is_dir(pages))
        {
            snprintf(s->bsp_dir, sizeof(s->bsp_dir), "%s", pages);
            return;
        }
    }
    if (gmb_http_is_dir("pages"))
        snprintf(s->bsp_dir, sizeof(s->bsp_dir), "%s", "pages");
}

static gmb_http_server_t g_http_servers[8];
static void* http_server_loop(void* arg);

int64_t gmb_http_server_start(int port)
{
    gmb_net_sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!GMB_NET_SOCK_OK(sock)) return -1;
    int yes = 1;
    gmb_net_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { fprintf(stderr, "[http_start] bind FAIL errno=%d\n", errno); close(sock); return -1; }
     if (listen(sock, 32) < 0) { fprintf(stderr, "[http_start] listen FAIL\n"); close(sock); return -1; }
    for (int i = 0; i < 8; i++)
    {
        if (g_http_servers[i].sock == 0)
        {
            /* NON fare memset: azzererebbe le route già registrate prima di HTTP SERVER
               (HTTP ROUTE ... HTTP SERVER — ordine canonico del sample). */
            g_http_servers[i].sock = sock;
            g_http_servers[i].port = port;
            g_http_servers[i].running = 1;
            if (g_http_servers[i].request_timeout_ms == 0)
                g_http_servers[i].request_timeout_ms = 30000;
            gmb_http_maybe_infer_bsp(&g_http_servers[i], NULL);
            gmb_net_thread_create(&g_http_servers[i].thread, http_server_loop, &g_http_servers[i]);
            return (int64_t)(i + 1);
        }
    }
    close(sock);
    return -1;
}

/* Server HTTP su TLS (HTTPS): come gmb_http_server_start ma il listener accetta con
   handshake TLS (certificato PEM/PFX). Il loop http_server_loop usa ssl_ctx per
   SSL_accept/SSL_read/SSL_write. Certificato vuoto → -1 (mai server plain sotto maschera). */
int64_t gmb_http_server_start_tls(int port, const char* cert_path, const char* cert_password)
{
    gmb_ssl_init();
    if (!cert_path || !*cert_path) return -1;
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return -1;
    if (ssl_load_cert(ctx, cert_path, cert_password) != 0)
    {
        SSL_CTX_free(ctx);
        return -1;
    }
    gmb_net_sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!GMB_NET_SOCK_OK(sock)) { SSL_CTX_free(ctx); return -1; }
    int yes = 1;
    gmb_net_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { fprintf(stderr, "[https_start] bind FAIL errno=%d\n", errno); close(sock); SSL_CTX_free(ctx); return -1; }
    if (listen(sock, 32) < 0) { fprintf(stderr, "[https_start] listen FAIL\n"); close(sock); SSL_CTX_free(ctx); return -1; }
    for (int i = 0; i < 8; i++)
    {
        if (g_http_servers[i].sock == 0)
        {
            g_http_servers[i].sock = sock;
            g_http_servers[i].port = port;
            g_http_servers[i].running = 1;
            g_http_servers[i].ssl_ctx = ctx;
            if (g_http_servers[i].request_timeout_ms == 0)
                g_http_servers[i].request_timeout_ms = 30000;
            gmb_http_maybe_infer_bsp(&g_http_servers[i], NULL);
            gmb_net_thread_create(&g_http_servers[i].thread, http_server_loop, &g_http_servers[i]);
            return (int64_t)(i + 1);
        }
    }
    close(sock);
    SSL_CTX_free(ctx);
    return -1;
}

/* HTTP CONFIG cfg → stato del certificato TLS consumato dal prossimo HTTP SERVER.
 * Sorgente UNICA condivisa (§0bis): i backend C (C99Native; emitter C# ha una copia
 * inline storica) delegano qui il dispatch plain/tls, cosi' la semantica HTTP CONFIG
 * + HTTP SERVER non e' reinventata negli emitter. Parita' managed: un cert impostato
 * rende il server HTTPS, altrimenti HTTP. */
static char g_http_cert_path[1024] = "";
static char g_http_cert_pass[256] = "";

void gmb_http_server_set_cert(const char* path, const char* pass)
{
    if (path) snprintf(g_http_cert_path, sizeof(g_http_cert_path), "%s", path);
    else g_http_cert_path[0] = '\0';
    if (pass) snprintf(g_http_cert_pass, sizeof(g_http_cert_pass), "%s", pass);
    else g_http_cert_pass[0] = '\0';
}

int64_t gmb_http_server_start_tls_or_plain(int port)
{
    if (g_http_cert_path[0]) return gmb_http_server_start_tls(port, g_http_cert_path, g_http_cert_pass);
    return gmb_http_server_start(port);
}

static gmb_http_server_t* http_get(int64_t handle)
{
    if (handle <= 0 || handle > 8) return NULL;
    gmb_http_server_t* s = &g_http_servers[handle - 1];
    return GMB_NET_SOCK_OK(s->sock) ? s : NULL;
}

/* gmb_http_query_dict — query string → val dict (parità IL/PCode/VM: req.Query("k")
 * è un lookup nel DICT dei parametri, non sulla stringa grezza). Prima i wrapper C
 * mettevano "Query" come stringa → req.Query("k") dava nil (<nil>). */
gmb_val_t gmb_http_query_dict(const char* qs)
{
    gmb_val_t out = gmb_make_val_dict(gmb_dict_new());
    if (!qs || !*qs) return out;
    char* buf = strdup(qs);
    if (!buf) return out;
    char* p = buf;
    while (p && *p)
    {
        char* amp = strchr(p, '&');
        if (amp) *amp = '\0';
        char* eq = strchr(p, '=');
        char* val = (char*)"";
        if (eq) { *eq = '\0'; val = eq + 1; }
        if (*p) gmb_val_dict_set(out, p, gmb_make_val_string(val));
        p = amp ? amp + 1 : NULL;
    }
    free(buf);
    return out;
}

/* gmb_http_body_json — semantica canonica della chiave "BodyJson" nei dict req dei wrapper
 * handler (parità PCode BuildRequestDict): il body è parsato come JSON SOLO se Content-Type
 * contiene "application/json" (case-insensitive) e il body non è vuoto/bianchi; altrimenti nil.
 * Unica sorgente (AGENTS §0bis) per i tre wrapper C (VM/C, C99Native). */
gmb_val_t gmb_http_body_json(const char* content_type, const char* body)
{
    if (!content_type || !body) return gmb_val_nil();
    static const char needle[] = "application/json";
    int found = 0;
    for (const char* p = content_type; *p && !found; p++)
    {
        size_t i = 0;
        while (i < sizeof(needle) - 1)
        {
            char a = p[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (a != needle[i]) break;
            i++;
        }
        if (i == sizeof(needle) - 1) found = 1;
    }
    if (!found) return gmb_val_nil();
    const char* s = body;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) return gmb_val_nil();
    /* Guardìa "strict come JsonDocument.Parse": un body che NON inizia per JSON valido
     * ("ciao") per il parser CADREBBE sul ramo numerico e darebbe double 0 (PCode dà null). */
    char c0 = *s;
    if (!(c0 == '{' || c0 == '[' || c0 == '"' || c0 == '-' || c0 == '+' || c0 == 't'
          || c0 == 'f' || c0 == 'n' || (c0 >= '0' && c0 <= '9')))
        return gmb_val_nil();
    return gmb_json_parse(body);
}

/* gmb_httpconfig_seed/new — default di NEW HTTPCONFIG: UNICA sorgente C (AGENTS §0bis) per
 * VM/C (OP_NEW typeCode 23) ed emettitori C99Native. Chiavi/valori rispecchiano
 * CommonLib.Http.HttpConfig e il helper managed PCodeProcessor.NewHttpConfigDefaults (stessa
 * semina anche di "DefaultRequestTimeout", alias solo dell'oggetto C#: nel modello duck-typed
 * è una chiave propria con lo stesso default, senza sincronizzazione). */
void gmb_httpconfig_seed(gmb_dict_t* d)
{
    if (!d) return;
    gmb_dict_set(d, "Port",                     gmb_make_val_double(8080));
    gmb_dict_set(d, "CertificatePath",           gmb_val_nil());
    gmb_dict_set(d, "CertificatePassword",       gmb_val_nil());
    gmb_dict_set(d, "RequestTimeoutMs",          gmb_make_val_double(30000));
    gmb_dict_set(d, "DefaultRequestTimeout",     gmb_make_val_double(30000));
    gmb_dict_set(d, "MaxRequestBodySize",        gmb_make_val_double(10485760));
    gmb_dict_set(d, "RateLimitRequestsPerMinute", gmb_make_val_double(60));
    gmb_dict_set(d, "RateLimitBurstSize",        gmb_make_val_double(120));
    gmb_dict_set(d, "EnableKeepAlive",           gmb_make_val_double(-1)); /* TRUE canon: -1 */
}

gmb_val_t gmb_httpconfig_new(void)
{
    gmb_dict_t d = gmb_dict_new();
    gmb_httpconfig_seed(&d);
    return gmb_make_val_dict_take(&d);
}

/* gmb_http_fetchresult_seed — chiavi scalari canoniche del dict FETCHRESULT: UNICA
   sorgente C (AGENTS §0bis) per VM/C, C99Native. IsSuccess numerico
   -1/0 (canon PRINT true = -1, parità managed; via gmb_val_to_double resta -1);
   ContentLength = byte del body (parità FetchResult.BodyBytes.Length del PCode). */
void gmb_http_fetchresult_seed(gmb_dict_t* d, int status, const char* body, size_t body_len,
                               const char* url, const char* err)
{
    if (!d) return;
    gmb_dict_set(d, "StatusCode", gmb_make_val_double((double)status));
    gmb_dict_set(d, "IsSuccess", gmb_make_val_double(status >= 200 && status < 300 ? -1 : 0));
    /* Body binario-safe (parità managed Body/BodyBytes che mantengono i NUL): dal buffer
       curl con la sua length, non da strlen (PNG/zip altrimenti troncatissero al primo 0). */
    {
        gmb_val_t b; b.kind = GMB_TYPE_STRING;
        b.v_string = gmb_string_from_bytes(body, (gmb_int_t)body_len);
        gmb_dict_set(d, "Body", b);
    }
    gmb_dict_set(d, "Url", gmb_make_val_string(url ? url : ""));
    gmb_dict_set(d, "ErrorMessage", gmb_make_val_string(err ? err : ""));
    gmb_dict_set(d, "ContentLength", gmb_make_val_double((double)body_len));
}

/* gmb_http_headers_fill — riempie d con gli header di RISPOSTA (righe "Nome: Valore\n",
   formato di http_header_cb). UNICA implementazione del parsing (§0bis): la usano la
   costruzione SHARED di VM/C (gmb_obj_shared_val_t, richiesta dal member-dispatch .Get del
   VM) e quella REGOLARE degli emitter C99Native (gmb_make_val_dict_take). Prima
   il parsing viveva inline solo in VM/C e C99Native non lo facevano affatto →
   `fr.Headers.Get("k")` dava nil ("<nil>") su C99 / era assente (bug
   Headers-Get-Divergenza-Backend). Chiavi verbatim (case-preserving) come il managed. */
void gmb_http_headers_fill(gmb_dict_t* d, const char* headers_raw)
{
    if (!d || !headers_raw) return;
    char* line = (char*)headers_raw;
    while (line && *line)
    {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char* colon = strchr(line, ':');
        if (colon)
        {
            *colon = '\0';
            char* val = colon + 1;
            while (*val == ' ') val++;
            gmb_dict_set(d, line, gmb_make_val_string(val));
        }
        if (!nl) break;
        line = nl + 1;
    }
}

/* FETCHRESULT.SaveToFile(path): scrive i byte di Body (length-aware, quindi binario-safe:
   parità con CommonLib FetchResult.SaveToFile che scrive BodyBytes). 0 ok, -1 errore.
   Sorgente unica C per VM/C, C99Native (AGENTS §0bis). */
int gmb_http_fetchresult_save(gmb_val_t res, const char* path)
{
    if (res.kind != GMB_TYPE_OBJECT || !path || !*path) return -1;
    gmb_dict_t d = gmb_val_as_dict(res);
    gmb_val_t b = gmb_dict_get(d, "Body");
    if (b.kind != GMB_TYPE_STRING) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    if (b.v_string.data && b.v_string.length)
        fwrite(b.v_string.data, 1, b.v_string.length, f);
    fclose(f);
    return 0;
}

/* Alloca il primo slot libero di un array di route/middleware e ci scrive
 * method/path/template segmentati. Unica implementazione del matching (usata
 * sia dalla route testuale sia da quella binaria, sia dai middleware): evita
 * due copie che divergono. */
static gmb_http_route_t* http_route_alloc(gmb_http_route_t* arr, const char* method, const char* path)
{
    for (int i = 0; i < GMB_HTTP_MAX_ROUTES; i++)
    {
        if (arr[i].in_use) continue;
        gmb_http_route_t* r = &arr[i];
        r->in_use = 1;
        snprintf(r->method, sizeof(r->method), "%s", method);
        snprintf(r->path, sizeof(r->path), "%s", path);
        r->handler = NULL;
        r->handler_bin = NULL;
        r->path_seg_count = 0;
        /* split path in segmenti (es. "/users/{id}" → ["users","{id}"]) */
        if (strcmp(path, "*") != 0 && *path == '/')
        {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s", path + 1);
            char* save = NULL;
            char* tok = strtok_r(tmp, "/", &save);
            while (tok && r->path_seg_count < 16)
            {
                snprintf(r->path_seg[r->path_seg_count], 64, "%s", tok);
                r->path_seg_count++;
                tok = strtok_r(NULL, "/", &save);
            }
        }
        return r;
    }
    return NULL;
}

int gmb_http_server_route_bin(int64_t server, const char* method, const char* path,
                              gmb_http_handler_bin_fn handler, void* ctx)
{
    gmb_http_server_t* s = http_get(server);
    if (!s || !handler) return -1;
    gmb_http_route_t* r = http_route_alloc(s->routes, method, path);
    if (!r) return -1;
    r->handler_bin = handler;
    r->ctx = ctx;
    return 0;
}

int gmb_http_server_route(int64_t server, const char* method, const char* path,
                          gmb_http_handler_fn handler, void* ctx)
{
    gmb_http_server_t* s = http_get(server);
    if (!s || !handler) return -1;
    gmb_http_route_t* r = http_route_alloc(s->routes, method, path);
    if (!r) return -1;
    r->handler = handler;
    r->ctx = ctx;
    return 0;
}

/* HTTP USE — registra un middleware (catena prima della route). path_prefix e' un PREFISSO
 * (StartsWith, non template) e method e' "*" nella sintassi legacy — semantica identica a
 * PCodeHttpMiddleware/ExecuteHttpUse gestito (path nil → "/", metodo "*", per registrazione).
 * Può girare prima di HTTP SERVER (http_get accetta lo slot non ancora avviato, come route). */
int gmb_http_server_use_bin(int64_t server, const char* method, const char* path_prefix,
                            gmb_http_handler_bin_fn handler, void* ctx)
{
    gmb_http_server_t* s = http_get(server);
    if (!s || !handler) return -1;
    gmb_http_route_t* r = http_route_alloc(s->middlewares, method, path_prefix);
    if (!r) return -1;
    r->handler_bin = handler;
    r->ctx = ctx;
    return 0;
}

int gmb_http_server_use(int64_t server, const char* method, const char* path_prefix,
                        gmb_http_handler_fn handler, void* ctx)
{
    gmb_http_server_t* s = http_get(server);
    if (!s || !handler) return -1;
    gmb_http_route_t* r = http_route_alloc(s->middlewares, method, path_prefix);
    if (!r) return -1;
    r->handler = handler;
    r->ctx = ctx;
    return 0;
}

/* gmb_http_headers_from_dict — UNICA sorgente (AGENTS §0bis) per la serializzazione
 * client-side degli header HTTPOPTIONS nel FETCH. Il dict `hdrs` è il contenuto di
 * opts.Headers: le chiavi property canon (ContentType/UserAgent) diventano i nomi wire
 * (Content-Type/User-Agent), le altre restano verbatim (es. Headers.Add("X-Auth-Token",…)),
 * i valori NON stringa/vuoti si scartano, dedup case-insensitive sul nome wire. Prima
 * VM/C e il C emesso (C99Native) leggevano SOLO 3 chiavi note → un header
 * arbitrario non partiva e il server non lo riceveva (HttpRoutes: req.Headers.Get("X-…")
 * nil → 401). Ritorna il numero di header scritti (≤ max). */
int gmb_http_headers_from_dict(gmb_dict_t* hdrs, const char** names, const char** vals, int max)
{
    if (!hdrs || !names || !vals || max <= 0) return 0;
    int n = 0;
    for (size_t i = 0; i < hdrs->count && n < max; i++)
    {
        const char* raw = hdrs->items[i].key;
        if (!raw) continue;
        const char* wire = gmb_net_ieq(raw, "ContentType") ? "Content-Type"
                       : gmb_net_ieq(raw, "UserAgent") ? "User-Agent" : raw;
        gmb_val_t v = gmb_dict_get(*hdrs, raw);
        if (v.kind != GMB_TYPE_STRING || !v.v_string.data || !*v.v_string.data) continue;
        int dup = 0;
        for (int p = 0; p < n; p++) if (gmb_net_ieq(names[p], wire)) { dup = 1; break; }
        if (dup) continue;
        names[n] = wire;
        vals[n] = v.v_string.data;   /* vive nel dict finché il fetch consuma l'array */
        n++;
    }
    return n;
}

/* ============================================================================
 * Catena middleware — registry degli step di avanzamento (HTTP USE NEXT)
 * ============================================================================
 * Il dispatch (http_handle_conn) esegue la catena SULLA CONNESSIONE: ogni passo
 * intermedio ha un id di step; "HTTP USE NEXT" (esplicito collo handle, implicito
 * con lo step corrente) marca l'advance dello step e il loop della connessione,
 * dopo aver atteso il task del passo, prosegue col passo successivo. Il pattern
 * asincrono (GO SUB + handle catturato, HttpRoutes Esempio 4) funziona perche'
 * l'id dello step è stabile e indipendente dal task: un task GPM in background
 * può chiamare gmb_http_use_next(h) dopo la fine del middleware.
 * Parità del modello managed (PCodeHttpNextHandle + response.Sent), dove l'handle
 * è l'oggetto con la coda dei passi rimanenti. */
#define GMB_HTTP_MAX_CHAIN_STEPS 256
typedef struct { volatile int in_use; volatile int advanced; } gmb_http_chain_step_t;
static gmb_http_chain_step_t g_http_chain_steps[GMB_HTTP_MAX_CHAIN_STEPS];
static gmb_net_lock_t g_http_chain_lock;
static int g_http_chain_rr = 0;

int64_t gmb_http_chain_step_alloc(void)
{
    gmb_net_lock(&g_http_chain_lock);
    int64_t id = 0;
    for (int i = 0; i < GMB_HTTP_MAX_CHAIN_STEPS && !id; i++)
    {
        int idx = (g_http_chain_rr + i) % GMB_HTTP_MAX_CHAIN_STEPS;
        if (!g_http_chain_steps[idx].in_use)
        {
            g_http_chain_steps[idx].in_use = 1;
            g_http_chain_steps[idx].advanced = 0;
            g_http_chain_rr = (idx + 1) % GMB_HTTP_MAX_CHAIN_STEPS;
            id = idx + 1; /* sempre >= 1: 0 = "nessuno step" */
        }
    }
    gmb_net_unlock(&g_http_chain_lock);
    return id;
}

int gmb_http_chain_step_advance(int64_t id)
{
    if (id < 1 || id > GMB_HTTP_MAX_CHAIN_STEPS) return -1;
    gmb_net_lock(&g_http_chain_lock);
    g_http_chain_steps[id - 1].advanced = 1;
    gmb_net_unlock(&g_http_chain_lock);
    return 0;
}

int gmb_http_chain_step_is_advanced(int64_t id)
{
    if (id < 1 || id > GMB_HTTP_MAX_CHAIN_STEPS) return 0;
    gmb_net_lock(&g_http_chain_lock);
    int v = g_http_chain_steps[id - 1].advanced;
    gmb_net_unlock(&g_http_chain_lock);
    return v;
}

void gmb_http_chain_step_free(int64_t id)
{
    if (id < 1 || id > GMB_HTTP_MAX_CHAIN_STEPS) return;
    gmb_net_lock(&g_http_chain_lock);
    g_http_chain_steps[id - 1].in_use = 0;
    g_http_chain_steps[id - 1].advanced = 0;
    gmb_net_unlock(&g_http_chain_lock);
}

/* Lo step della richiesta CORRENTE su questo thread — per gli handler inline dei
 * backend C generati (HTTP USE NEXT senza handle). Il dispatch lo imposta prima di
 * invocare ogni passo e lo azzera dopo. */
static _Thread_local int64_t g_http_current_step = 0;
int64_t gmb_http_current_step(void) { return g_http_current_step; }
/* Setter per i backend C99 (handler su worker GPM): il TLS del thread del server non e'
 * visibile al worker, quindi il wrapper dell'handler lo imposta all'inizio del task. */
void gmb_http_set_current_step(int64_t step_id) { g_http_current_step = step_id; }

/* HTTP USE NEXT next — avanzamento della catena. step_id = handle esplicito della
 * forma `HTTP USE NEXT h`, oppure 0 = "lo step del chiamante" (forma implicita, dal
 * thread-local nei backend inline; la VM/C passa task->http_chain_step dall'outside). */
int gmb_http_use_next(int64_t step_id)
{
    int64_t id = step_id > 0 ? step_id : g_http_current_step;
    if (id < 1) return -1; /* fuori catena (handler finale/ultimo passo): no-op, come managed */
    return gmb_http_chain_step_advance(id);
}


/* Statico: registra una mappatura prefix→dir fisica. */
int gmb_http_server_static(int64_t server, const char* prefix, const char* dir)
{
    gmb_http_server_t* s = http_get(server);
    if (!s || s->static_count >= 16) return -1;
    snprintf(s->static_prefix[s->static_count], sizeof(s->static_prefix[0]), "%s", prefix);
    /* Normalizza la dir (path assoluto, `..` risolto) come fa Path.GetFullPath in C#:
       senza, un HTTP STATIC con dir relativa che contiene `..` (es. "../Web/RegistryBsp")
       faceva scattare la guardia anti-path-traversal sul path FISICO non risolto (il guard
       cerca ".." in fpath) e ogni richiesta *.bsp/statica finiva in 403. La normalizzazione
       alla registrazione mantiene il guard su `..` correttamente: ora blocca SOLO `..`
       presenti nell'URL del client, non quelli legittimi della dir configurata. */
    {
        gmb_string_t norm = gmb_fs_normalize_path(gmb_string_alloc((char*)dir));
        snprintf(s->static_dir[s->static_count], sizeof(s->static_dir[0]), "%s",
                 norm.data ? norm.data : (dir ? dir : ""));
        if (norm.data) free(norm.data);
    }
    s->static_count++;
    gmb_http_maybe_infer_bsp(s, s->static_dir[s->static_count - 1]);
    return 0;
}

int gmb_http_server_bsp(int64_t server, const char* dir)
{
    gmb_http_server_t* s;
    if (server <= 0 || server > 8 || !dir) return -1;
    /* Anche prima di HTTP SERVER: slot BSS, sock può essere ancora 0. */
    s = &g_http_servers[server - 1];
    snprintf(s->bsp_dir, sizeof(s->bsp_dir), "%s", dir);
    return 0;
}

/* CONFIG: porta/cert (già al start), timeout richiesta. */
int gmb_http_server_set_timeout(int64_t server, int timeout_ms)
{
    gmb_http_server_t* s = http_get(server);
    if (!s) return -1;
    s->request_timeout_ms = timeout_ms > 0 ? timeout_ms : 30000;
    return 0;
}

/* CONFIG security (2026-09-02): SecurityHeaders + AllowedIPs/BlockedIPs (CIDR IPv4/IP singoli,
   virgola). Parità con CommonLib.Http.HttpSecurity usata da IL/PCode. */
int gmb_http_server_set_security(int64_t server, int security_headers, const char* allowed, const char* blocked)
{
    gmb_http_server_t* s = http_get(server);
    if (!s) return -1;
    s->security_headers = security_headers ? 1 : 0;
    s->allowed_ip[0] = 0;
    s->blocked_ip[0] = 0;
    if (allowed && *allowed) snprintf(s->allowed_ip, sizeof(s->allowed_ip), "%s", allowed);
    if (blocked && *blocked) snprintf(s->blocked_ip, sizeof(s->blocked_ip), "%s", blocked);
    return 0;
}

/* CONFIG MaxRequestBodySize (0 = illimitato) → 413 se Content-Length supera la soglia. */
int gmb_http_server_set_max_body(int64_t server, long bytes)
{
    gmb_http_server_t* s = http_get(server);
    if (!s) return -1;
    s->max_body = bytes > 0 ? bytes : 0;
    return 0;
}

/* CONFIG RateLimitRequestsPerMinute/RateLimitBurstSize — token bucket condiviso per server
   (connessioni concorrenti; spinlock). per_min <= 0 = disabilitato. */
int gmb_http_server_set_rate_limit(int64_t server, int per_min, int burst)
{
    gmb_http_server_t* s = http_get(server);
    if (!s) return -1;
    gmb_net_lock(&s->rate_lock);
    s->rate_per_min = per_min > 0 ? per_min : 0;
    s->rate_burst = burst > 0 ? burst : 0;
    s->rate_tokens = (double)(s->rate_burst > 0 ? s->rate_burst : (s->rate_per_min > 0 ? s->rate_per_min : 1));
    s->rate_last_ms = 0;
    gmb_net_unlock(&s->rate_lock);
    return 0;
}

/* True se la richiesta ha un token del bucket. Refill continuo: per_min/60 token al secondo,
   capacity = burst (o per_min se burst assente). */
static int http_rate_allow(gmb_http_server_t* s)
{
    if (s->rate_per_min <= 0) return 1;
    gmb_net_lock(&s->rate_lock);
    int64_t now = gmb_now_ms_net();
    if (s->rate_last_ms == 0)
    {
        s->rate_last_ms = now;
        if (s->rate_tokens <= 0) s->rate_tokens = 1.0;
    }
    else
    {
        double cap = (double)(s->rate_burst > 0 ? s->rate_burst : (s->rate_per_min > 0 ? s->rate_per_min : 1));
        double refill = (double)s->rate_per_min / 60.0;
        double elapsed = (double)(now - s->rate_last_ms) / 1000.0;
        s->rate_last_ms = now;
        s->rate_tokens += elapsed * refill;
        if (s->rate_tokens > cap) s->rate_tokens = cap;
    }
    int ok = s->rate_tokens >= 1.0;
    if (ok) s->rate_tokens -= 1.0;
    gmb_net_unlock(&s->rate_lock);
    return ok;
}

/* ---- IP allow/block (matcher CIDR IPv4, replica di CommonLib.Http.HttpSecurity) ---- */
static int http_ip4_parse(const char* text, uint32_t* out)
{
    struct in_addr a;
    if (!text || !*text) return 0;
    if (inet_pton(AF_INET, text, &a) != 1) return 0;
    *out = ntohl(a.s_addr);
    return 1;
}

static int http_ip_entry_match(const char* entry, uint32_t ip)
{
    const char* slash = strchr(entry, '/');
    if (slash)
    {
        char addr[64];
        size_t al = (size_t)(slash - entry);
        if (al >= sizeof(addr)) al = sizeof(addr) - 1;
        memcpy(addr, entry, al);
        addr[al] = 0;
        long bits = strtol(slash + 1, NULL, 10);
        if (bits < 0 || bits > 32) return 0;
        uint32_t a = 0;
        if (!http_ip4_parse(addr, &a)) return 0;
        uint32_t mask = bits == 0 ? 0 : (bits == 32 ? 0xFFFFFFFFu : (uint32_t)((0xFFFFFFFFu << (32 - bits)) & 0xFFFFFFFFu));
        return (ip & mask) == (a & mask);
    }
    uint32_t a = 0;
    return http_ip4_parse(entry, &a) && a == ip;
}

static int http_ip_in_list(const char* csv, uint32_t ip)
{
    if (!csv || !*csv) return 0;
    char copy[512];
    snprintf(copy, sizeof(copy), "%s", csv);
    char* save = NULL;
    char* tok = strtok_r(copy, ",", &save);
    while (tok)
    {
        while (*tok == ' ') tok++;
        if (http_ip_entry_match(tok, ip)) return 1;
        tok = strtok_r(NULL, ",", &save);
    }
    return 0;
}

static int http_ip_allowed(const char* ip, const char* allowed_csv, const char* blocked_csv)
{
    uint32_t a = 0;
    if (!http_ip4_parse(ip, &a)) return 1; /* IP non risolvibile → fail-open sul parse */
    if (allowed_csv && *allowed_csv && !http_ip_in_list(allowed_csv, a)) return 0;
    if (http_ip_in_list(blocked_csv, a)) return 0;
    return 1;
}

static int http_client_ip(gmb_net_sock_t fd, char* out, size_t cap)
{
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    if (getpeername(fd, (struct sockaddr*)&peer, &plen) == 0)
        return inet_ntop(AF_INET, &peer.sin_addr, out, (socklen_t)cap) ? 1 : 0;
    return 0;
}

/* Header di sicurezza automatici (parità CommonLib.Http.HttpSecurity.DefaultSecurityHeaders). */
static const char* const http_sec_headers[][2] = {
    { "Strict-Transport-Security", "max-age=31536000; includeSubDomains" },
    { "Content-Security-Policy", "default-src 'self'" },
    { "X-Frame-Options", "SAMEORIGIN" },
    { "X-Content-Type-Options", "nosniff" },
    { "Referrer-Policy", "strict-origin-when-cross-origin" },
    { "X-Permitted-Cross-Domain-Policies", "none" },
};
#define GMB_HTTP_SEC_HEADERS_COUNT (sizeof(http_sec_headers) / sizeof(http_sec_headers[0]))

/* True se `extra` (righe "Nome: Valore\r\n") contiene gia' l'header `name` (case-insensitive). */
static int http_extra_has(const char* extra, const char* name)
{
    size_t nl = strlen(name);
    const char* p = extra;
    while (p && *p)
    {
        const char* end = strstr(p, "\r\n");
        size_t ll = end ? (size_t)(end - p) : strlen(p);
        if (ll > nl + 2 && strncmp(p, name, nl) == 0 && p[nl] == ':') return 1;
        if (!end) break;
        p = end + 2;
    }
    return 0;
}

/* match un path contro una route template; estrae i parametri {x} in params (chiave→valore)
   e i nomi in param_names (allineati). Ritorna 1 se match, 0 altrimenti. */
static int route_match_path(const gmb_http_route_t* r, const char* path,
                            char (*params)[64], int params_cap, int* params_count,
                            char (*param_names)[64], int* param_names_count)
{
    if (r->path_seg_count == 0 && strcmp(r->path, "*") == 0) return 1; /* wildcard */
    if (*path != '/') return 0;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path + 1);
    char* segs[32];
    int n = 0;
    char* save = NULL;
    char* tok = strtok_r(tmp, "/", &save);
    while (tok && n < 32) { segs[n++] = tok; tok = strtok_r(NULL, "/", &save); }
    if (n != r->path_seg_count) return 0;
    int pc = 0;
    for (int i = 0; i < n; i++)
    {
        const char* pat = r->path_seg[i];
        if (pat[0] == '{' && pat[strlen(pat) - 1] == '}')
        {
            /* parametro: salva valore + nome */
            char key[64];
            size_t pl = strlen(pat) - 2;
            if (pl > 63) pl = 63;
            memcpy(key, pat + 1, pl);
            key[pl] = 0;
            if (params && pc < params_cap)
            {
                snprintf(params[pc], 64, "%s", segs[i]);
                if (params_count) (*params_count)++;
            }
            if (param_names && pc < 16)
            {
                snprintf(param_names[pc], 64, "%s", key);
                if (param_names_count) (*param_names_count)++;
            }
            pc++;
        }
        else if (strcmp(pat, segs[i]) != 0) return 0;
    }
    return 1;
}

/* Semplice parsing HTTP/1.1: "METHOD PATH HTTP/1.1" + headers + body */
/* I/O HTTP: usa SSL se il server è HTTPS, altrimenti socket plain. */
static ssize_t http_io_recv(gmb_net_sock_t fd, SSL* ssl, void* buf, size_t len)
{
    return ssl ? SSL_read(ssl, buf, (int)len) : recv(fd, buf, len, 0);
}
static int http_io_send(gmb_net_sock_t fd, SSL* ssl, const void* buf, size_t len)
{
    return ssl ? SSL_write(ssl, buf, (int)len) : (int)send(fd, buf, len, 0);
}

static void http_msleep(int ms)
{
#if defined(_WIN32) || defined(_WIN64)
    Sleep(ms > 0 ? (DWORD)ms : 1);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* Invia la risposta HTTP: righe header "Nome: Valore\n" (mutabile, liberato dal chiamante),
 * Content-Type esplicito sostituisce il default, le altre righe (Location, Set-Cookie, ...)
 * passano in risposta; header di sicurezza automatici (HTTP CONFIG SecurityHeaders) non
 * sovrascrivono header già presenti. Estratto dal loop di dispatch per la catena middleware
 * (ogni passo può accumulare header nello stesso buffer combinato). */
static void http_send_response(gmb_http_server_t* s, gmb_net_sock_t client, SSL* ssl,
                               const char* body, size_t body_len, int status, char* headers)
{
    const char* ctype = "text/plain";
    size_t extra_cap = 128, extra_len = 0;
    char* extra = (char*)malloc(extra_cap);
    if (extra) extra[0] = '\0';
    if (headers && *headers && extra)
    {
        char* line = headers;
        while (line && *line)
        {
            char* nl = strchr(line, '\n');
            if (nl) *nl = 0;
            if (!*line) { if (!nl) break; line = nl + 1; continue; } /* righe vuote da concatenazione */
            if (strncmp(line, "Content-Type:", 13) == 0) { ctype = line + 13; while (*ctype==' ') ctype++; }
            else
            {
                size_t ll = strlen(line);
                while (extra_len + ll + 3 > extra_cap)
                {
                    extra_cap *= 2;
                    char* ne = (char*)realloc(extra, extra_cap);
                    if (!ne) break;
                    extra = ne;
                }
                memcpy(extra + extra_len, line, ll);
                extra_len += ll;
                extra[extra_len++] = '\r'; extra[extra_len++] = '\n';
                extra[extra_len] = '\0';
            }
            if (!nl) break;
            line = nl + 1;
        }
    }
    if (s->security_headers && extra)
    {
        for (size_t k = 0; k < GMB_HTTP_SEC_HEADERS_COUNT; k++)
        {
            if (http_extra_has(extra, http_sec_headers[k][0])) continue;
            size_t need = strlen(http_sec_headers[k][0]) + 2 + strlen(http_sec_headers[k][1]) + 4;
            if (extra_len + need + 1 > extra_cap)
            {
                while (extra_len + need + 1 > extra_cap) extra_cap *= 2;
                char* ne = (char*)realloc(extra, extra_cap);
                if (!ne) break;
                extra = ne;
            }
            int wrote = snprintf(extra + extra_len, extra_cap - extra_len, "%s: %s\r\n",
                                 http_sec_headers[k][0], http_sec_headers[k][1]);
            if (wrote > 0) extra_len += (size_t)wrote;
        }
    }
    char head[1024];
    snprintf(head, sizeof(head),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\n%sContent-Length: %zu\r\nConnection: close\r\n\r\n",
        status <= 0 ? 200 : status, ctype, extra ? extra : "", body_len);
    http_io_send(client, ssl, head, strlen(head));
    if (body && body_len) http_io_send(client, ssl, body, body_len);
    free(extra);
}


static void http_handle_conn(gmb_http_server_t* s, gmb_net_sock_t client, SSL* ssl)
{
    char buf[65536];
    size_t total = 0;
    int64_t deadline = gmb_now_ms_net() + (s->request_timeout_ms > 0 ? s->request_timeout_ms : 30000);
    while (total < sizeof(buf) - 1)
    {
        ssize_t r = http_io_recv(client, ssl, buf + total, sizeof(buf) - 1 - total);
        if (r > 0)
        {
            total += (size_t)r;
            buf[total] = '\0';
            if (strstr(buf, "\r\n\r\n") || strstr(buf, "\n\n")) break;
            continue;
        }
        if (r == 0) { close(client); return; }
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { close(client); return; }
        int64_t remain = deadline - gmb_now_ms_net();
        if (remain <= 0) { close(client); return; }
        int w = gmb_net_wait_fd((int)client, remain);
        if (w <= 0) { close(client); return; }
    }
    buf[total] = '\0';
    if (total == 0) { close(client); return; }

    /* request line: METHOD PATH VERSION */
    char method[16] = "", path[512] = "", ver[16] = "";
    sscanf(buf, "%15s %511s %15s", method, path, ver);
    if (!*method || !*path) { close(client); return; }

    /* IP remoto (per HTTP CONFIG AllowedIPs/BlockedIPs e REQ.RemoteIp) */
    char client_ip[64] = "";
    if (!http_client_ip(client, client_ip, sizeof(client_ip))) client_ip[0] = 0;

    /* IP allow/block (HTTP CONFIG SecurityHeaders/AllowedIPs/BlockedIPs — parità IL/PCode) */
    if (!http_ip_allowed(client_ip, s->allowed_ip, s->blocked_ip))
    {
        const char* forbidden = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        http_io_send(client, ssl, forbidden, strlen(forbidden));
        close(client);
        return;
    }

    /* path senza query + query string */
    char path_noq[512];
    snprintf(path_noq, sizeof(path_noq), "%s", path);
    char* q = strchr(path_noq, '?');
    char query[512] = "";
    if (q)
    {
        *q = '\0';
        snprintf(query, sizeof(query), "%s", q + 1);
    }

    /* headers + body */
    const char* hdr_end = strstr(buf, "\r\n\r\n");
    int sep = 4;
    if (!hdr_end) { hdr_end = strstr(buf, "\n\n"); sep = 2; }
    if (!hdr_end) { close(client); return; }
    const char* body_start = hdr_end + sep;
    size_t body_len = total - (size_t)(body_start - buf);
    char body[65536];
    size_t bl = body_len < sizeof(body) - 1 ? body_len : sizeof(body) - 1;
    memcpy(body, body_start, bl);
    body[bl] = 0;

    /* header names/values */
    const char* hn[GMB_HTTP_MAX_HEADERS];
    const char* hv[GMB_HTTP_MAX_HEADERS];
    int hc = 0;
    {
        char hdr_copy[8192];
        size_t hdr_len = (size_t)(hdr_end - buf);
        if (hdr_len > sizeof(hdr_copy) - 1) hdr_len = sizeof(hdr_copy) - 1;
        memcpy(hdr_copy, buf, hdr_len);
        hdr_copy[hdr_len] = 0;
        char* line = hdr_copy;
        while (line && *line && hc < GMB_HTTP_MAX_HEADERS)
        {
            char* nl = strchr(line, '\n');
            if (nl)
            {
                if (nl > line && nl[-1] == '\r') nl[-1] = 0;
                *nl = 0;
            }
            char* colon = strchr(line, ':');
            if (colon && hc < GMB_HTTP_MAX_HEADERS)
            {
                *colon = 0;
                const char* val = colon + 1;
                while (*val == ' ') val++;
                hn[hc] = line;
                hv[hc] = val;
                hc++;
            }
            if (!nl) break;
            line = nl + 1;
        }
    }

    /* Body size cap (HTTP CONFIG MaxRequestBodySize) — 413 se Content-Length supera la soglia */
    if (s->max_body > 0)
    {
        for (int i = 0; i < hc; i++)
        {
            if (gmb_net_ieq(hn[i], "Content-Length"))
            {
                long cl = atol(hv[i]);
                if (cl > s->max_body)
                {
                    const char* too = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    http_io_send(client, ssl, too, strlen(too));
                    close(client);
                    return;
                }
                break;
            }
        }
    }

    /* Rate limit (HTTP CONFIG RateLimitRequestsPerMinute/RateLimitBurstSize) — 429 */
    if (!http_rate_allow(s))
    {
        const char* limit = "HTTP/1.1 429 Too Many Requests\r\nContent-Type: text/plain\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        http_io_send(client, ssl, limit, strlen(limit));
        close(client);
        return;
    }

    /* WebSocket upgrade? */
    int is_ws_upgrade = 0;
    for (int i = 0; i < hc; i++)
        if (gmb_net_ieq(hn[i], "Upgrade") && strstr(hv[i], "websocket")) is_ws_upgrade = 1;

    /* header Host (per BSP: REQ.Headers["Host"] → le pagine si auto-localizzano
     * il registry). Default "localhost:<port>" se assente. */
    const char* host = NULL;
    for (int i = 0; i < hc; i++)
        if (gmb_net_ieq(hn[i], "Host")) { host = hv[i]; break; }
    char host_buf[256];
    if (!host || !*host)
    {
        snprintf(host_buf, sizeof(host_buf), "localhost:%d", s->port);
        host = host_buf;
    }

    /* match route (template + method) */
    gmb_http_request_t req;
    memset(&req, 0, sizeof(req));
    req.method = method;
    req.path = path_noq;
    req.query = query;
    req.body = body;
    req.header_names = hn;
    req.header_values = hv;
    req.header_count = hc;
    req.remote_ip = client_ip[0] ? client_ip : "";
    req.is_websocket_upgrade = is_ws_upgrade;

    int handled = 0;
    /* Catena middleware (HTTP USE) + route (HTTP ROUTE): il dispatch C rispecchia
     * PCodeHttpRouter.Match + ExecuteHttpUseNext del modello managed:
     *   catena = middleware il cui PREFISSO matcha il path (metodo "*" legacy) in ordine
     *            di registrazione, seguiti dalla route che matcha il template;
     *   ogni passo intermedio ha uno "step id" dal registry g_http_chain_steps:
     *   HTTP USE NEXT (implicito, o esplicito collo handle passato come 3° argomento)
     *   marca l'advance e il loop prosegue col passo successivo;
     *   un passo che NON avanza e HA scritto (body non vuoto o status != 200 — proxy di
     *   response.Sent managed) corta-circuita con la propria risposta (AuthMiddleware 401);
     *   un passo che non avanza e non ha scritto attende l'advance fino al timeout
     *   (pattern asincrono GO SUB + handle, HttpRoutes Esempio 4) e su scadenza risponde
     *   504 (parità CompleteAsTimeout managed).
     * rc != 0 alla route finale = "non gestita": si prova la successiva route matchata
     * (semantica storica del server C, usata dal C generato che ritorna -1). */
    int route_scan_from = 0;
    while (!handled)
    {
        const gmb_http_route_t* matched = NULL;
        for (int i = route_scan_from; i < GMB_HTTP_MAX_ROUTES; i++)
        {
            const gmb_http_route_t* r = &s->routes[i];
            if (!r->in_use) continue;
            if (!(strcmp(r->method, "*") == 0 || strcmp(r->method, method) == 0)) continue;
            int pc = 0;
            req.param_names_count = 0;
            if (!route_match_path(r, path_noq, req.params, 16, &pc, req.param_names, &req.param_names_count)) continue;
            req.params_count = pc;
            matched = r;
            route_scan_from = i + 1;
            break;
        }
        if (!matched) break;

        if (is_ws_upgrade)
        {
            /* WebSocket: handshake 101 PRIMA (il client aspetta la risposta). Poi alloca
               l'handle WS e passalo all'handler come req.ws_handle: il VM/C crea il dict
               WEBSOCKET e lo passa come 3° argomento della SUB di route (parità managed
               WsHandler(req,res,ws)), cosi' ws.OnMessage="..." registra la callback e avvia
               il receive-loop che dispatcha i messaggi a BASIC. La catena middleware NON si
               applica agli upgrade WS (parità managed: HandleWebSocketUpgradeAsync invoca
               direttamente l'handler della route). */
            if (gmb_ws_server_handshake(client, buf) != 0)
            {
                close(client);
                return;
            }
            int64_t ws_h = net_alloc(client, 0, 0);
            if (ws_h <= 0)
            {
                close(client);
                return;
            }
            req.ws_handle = ws_h;
            char* out = NULL;
            int st = 200;
            char* oh = NULL;
            /* La VM/C registra le route WS via route_bin (handler NULL, handler_bin
               valorizzato): chiamare matched->handler da sola era un NULL-deref → crash su
               WebSocketEcho. Una route in_use ha SEMPRE esattamente uno dei due (route/
               route_bin rifiutano handler NULL), quindi il ramo è esaustivo. */
            int wrc;
            size_t ws_out_len = 0;
            if (matched->handler)
                wrc = matched->handler(matched->ctx, &req, &out, &st, &oh);
            else
                wrc = matched->handler_bin(matched->ctx, &req, &out, &ws_out_len, &st, &oh);
            if (out) free(out);
            if (oh) free(oh);
            if (wrc == 1)
            {
                /* La VM ha preso in carico la sessione: il receive-loop (thread detached in
                   gmb_vm_core.c) possiede socket+handle e li rilascia alla chiusura. NON
                   chiudere qui (doppio close/use-after-free). */
                return;
            }
            /* Fallback: nessun OnMessage registrato → echo di base (comportamento storico). */
            while (1)
            {
                int op; char* pay = NULL; size_t pl = 0;
                if (gmb_ws_recv_frame(ws_h, &op, &pay, &pl, 60000) != 0) break;
                if (op == GMB_WS_TEXT) gmb_ws_send_text(ws_h, pay);
                if (op == GMB_WS_CLOSE) break;
                free(pay);
            }
            net_release(ws_h);
            close(client);
            return;
        }

        const gmb_http_route_t* chain[GMB_HTTP_MAX_ROUTES + 1];
        int chain_n = 0;
        for (int i = 0; i < GMB_HTTP_MAX_ROUTES && chain_n < GMB_HTTP_MAX_ROUTES; i++)
        {
            const gmb_http_route_t* m = &s->middlewares[i];
            if (!m->in_use) continue;
            if (!(strcmp(m->method, "*") == 0 || strcmp(m->method, method) == 0)) continue;
            size_t pl = strlen(m->path);
            if (!pl || strncmp(path_noq, m->path, pl) != 0) continue;
            chain[chain_n++] = m;
        }
        chain[chain_n++] = matched;

        char* acc = NULL; size_t acc_len = 0; /* header accumulati dai passi avanzati */
        for (int k = 0; k < chain_n; k++)
        {
            const gmb_http_route_t* r = chain[k];
            int is_last = (k == chain_n - 1);
            if (!r->handler && !r->handler_bin)
            {
                /* handler C di ALCUN tipo (testuale o binario) — es. transpiler C99 con
                   handler BASIC non collegato: rispondi 501 (mai crash). ATTENZIONE: la
                   VM/C registra OGNI handler via route_bin/use_bin (handler_bin valorizzato,
                   handler resta NULL): una guardia `!r->handler` da sola mandava ogni route
                   VM/C in 501 → parità rotta (roundtrip/sendjson). */
                const char* notimpl = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                http_io_send(client, ssl, notimpl, strlen(notimpl));
                free(acc);
                close(client);
                return;
            }
            int64_t step_id = 0;
            if (!is_last)
            {
                step_id = gmb_http_chain_step_alloc();
                if (!step_id)
                {
                    /* registry step pieno: errore esplicito, mai un fallthrough silenzioso */
                    const char* msg = "500 Internal Server Error: catena HTTP chain-step esausta";
                    http_send_response(s, client, ssl, msg, strlen(msg), 500, NULL);
                    free(acc);
                    close(client);
                    return;
                }
            }
            req.chain_step_id = step_id;
            g_http_current_step = step_id;

            char* out_body = NULL;
            char* out_headers = NULL;
            int out_status = 200;
            size_t out_body_len = 0;
            int rc;
            /* Route BINARIA (res.SendFile): il handler dichiara la lunghezza del corpo, che
             * puo' contenere NUL (uno ZIP ne ha: l'header locale ne contiene a offset 5). Con
             * la firma storica strlen(out_body) troncava il file al primo NUL. */
            if (r->handler_bin)
                rc = r->handler_bin(r->ctx, &req, &out_body, &out_body_len, &out_status, &out_headers);
            else
            {
                rc = r->handler(r->ctx, &req, &out_body, &out_status, &out_headers);
                out_body_len = out_body ? strlen(out_body) : 0;
            }

            int advanced = is_last ? 0 : gmb_http_chain_step_is_advanced(step_id);
            int wrote = (out_body_len > 0) || (out_status != 200);

            if (!is_last && !wrote && !advanced)
            {
                /* Il passo non ha né risposto né avanzato subito: attende l'advance da un
                   thread/task diverso (GO SUB + HTTP USE NEXT collo handle — HttpRoutes
                   Esempio 4, OperazioneLunga). Scadenza = request timeout → 504. */
                int64_t until = gmb_now_ms_net() + (s->request_timeout_ms > 0 ? s->request_timeout_ms : 30000);
                while (!advanced && gmb_now_ms_net() < until)
                {
                    http_msleep(1);
                    advanced = gmb_http_chain_step_is_advanced(step_id);
                }
                if (!advanced)
                {
                    const char* msg = "504 Gateway Timeout";
                    char hdr[] = "Content-Type: text/plain; charset=utf-8\n";
                    http_send_response(s, client, ssl, msg, strlen(msg), 504, hdr);
                    gmb_http_chain_step_free(step_id);
                    g_http_current_step = 0;
                    free(acc);
                    close(client);
                    return;
                }
            }

            if (!is_last && advanced && !wrote)
            {
                /* Prosegui la catena: gli header scritti dal passo si accumulano (parità
                   dell'oggetto res condiviso del modello managed). */
                if (out_headers && *out_headers)
                {
                    size_t hl = strlen(out_headers);
                    char* na = (char*)realloc(acc, acc_len + hl + 1);
                    if (na) { memcpy(na + acc_len, out_headers, hl); na[acc_len + hl] = 0; acc = na; acc_len += hl; }
                }
                gmb_http_chain_step_free(step_id);
                g_http_current_step = 0;
                free(out_body);
                free(out_headers);
                continue;
            }

            if (is_last && rc != 0)
            {
                /* Route finale "non gestita" (rc = -1, forma storica del C generato):
                   prova la successiva route matchata. */
                free(out_body);
                free(out_headers);
                g_http_current_step = 0;
                break;
            }

            /* Rispondi: corto-circuito del middleware (ha scritto: es. 401) oppure la
             * route finale. out_headers: righe "Nome: Valore\n" — Content-Type sostituisce
             * il default, TUTTE le altre righe (Location, Set-Cookie, ...) passano in
             * risposta; si concatenano gli header accumulati dai middleware avanzati. */
            char* combined = NULL;
            {
                size_t hlen = out_headers ? strlen(out_headers) : 0;
                if (acc_len || hlen)
                {
                    combined = (char*)malloc(acc_len + hlen + 1);
                    if (combined)
                    {
                        if (acc_len) memcpy(combined, acc, acc_len);
                        if (hlen) memcpy(combined + acc_len, out_headers, hlen);
                        combined[acc_len + hlen] = 0;
                    }
                }
            }
            http_send_response(s, client, ssl, out_body, out_body_len, out_status, combined);
            free(out_body);
            free(out_headers);
            free(combined);
            if (step_id) gmb_http_chain_step_free(step_id);
            g_http_current_step = 0;
            free(acc);
            close(client);
            return;
        }
        free(acc);
    }

    /* .bsp dopo le route esplicite, prima dello statico (HTTP ROUTE vince). */
    if (!handled && s->bsp_dir[0])
    {
        char* bsp_html = NULL;
        int bsp_st = 0;
        if (gmb_bsp_try_serve(s->bsp_dir, path_noq, query, host, &bsp_html, &bsp_st))
        {
            size_t olen = bsp_html ? strlen(bsp_html) : 0;
            const char* reason = bsp_st == 403 ? "Forbidden"
                : bsp_st == 500 ? "Internal Server Error" : "OK";
            char head[256];
            snprintf(head, sizeof(head),
                "HTTP/1.1 %d %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                bsp_st > 0 ? bsp_st : 200, reason, olen);
            http_io_send(client, ssl, head, strlen(head));
            if (bsp_html && olen) http_io_send(client, ssl, bsp_html, olen);
            free(bsp_html);
            close(client);
            return;
        }
    }

    /* fallback statico: prefix → file fisico */
    if (!handled && s->static_count > 0)
    {
        for (int i = 0; i < s->static_count; i++)
        {
            size_t plen = strlen(s->static_prefix[i]);
            if (strncmp(path_noq, s->static_prefix[i], plen) == 0)
            {
            char fpath[1024];
            snprintf(fpath, sizeof(fpath), "%s%s", s->static_dir[i], path_noq + plen);
            /* guardia path-traversal: rifiuta ".." */
            if (strstr(fpath, ".."))
            {
                const char* r404 = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                http_io_send(client, ssl, r404, strlen(r404));
                close(client);
                return;
            }
            /* .bsp nella static dir: renderizza via gmb_bsp_try_serve (HTTP STATIC serve
               anche le pagine Basic Server Pages, non il file grezzo). */
            {
                size_t pl = strlen(fpath);
                if (pl >= 4 && strcmp(fpath + pl - 4, ".bsp") == 0)
                {
                    char* bsp_html = NULL;
                    int bsp_st = 0;
                    if (gmb_bsp_try_serve(s->static_dir[i], path_noq, query, host, &bsp_html, &bsp_st))
                    {
                        size_t olen = bsp_html ? strlen(bsp_html) : 0;
                        const char* reason = bsp_st == 403 ? "Forbidden"
                            : bsp_st == 500 ? "Internal Server Error" : "OK";
                        char bsp_head[256];
                        snprintf(bsp_head, sizeof(bsp_head),
                            "HTTP/1.1 %d %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                            bsp_st > 0 ? bsp_st : 200, reason, olen);
                        http_io_send(client, ssl, bsp_head, strlen(bsp_head));
                        if (bsp_html && olen) http_io_send(client, ssl, bsp_html, olen);
                        free(bsp_html);
                        close(client);
                        return;
                    }
                }
            }
            FILE* f = fopen(fpath, "rb");
                if (f)
                {
                    char fbuf[65536];
                    size_t fr = fread(fbuf, 1, sizeof(fbuf), f);
                    fclose(f);
                    const char* ext = strrchr(fpath, '.');
                    const char* ct = "application/octet-stream";
                    if (ext)
                    {
                        if (strcmp(ext, ".html")==0 || strcmp(ext, ".htm")==0) ct = "text/html";
                        else if (strcmp(ext, ".css")==0) ct = "text/css";
                        else if (strcmp(ext, ".js")==0) ct = "application/javascript";
                        else if (strcmp(ext, ".json")==0) ct = "application/json";
                        else if (strcmp(ext, ".png")==0) ct = "image/png";
                        else if (strcmp(ext, ".jpg")==0 || strcmp(ext, ".jpeg")==0) ct = "image/jpeg";
                        else if (strcmp(ext, ".gif")==0) ct = "image/gif";
                        else if (strcmp(ext, ".svg")==0) ct = "image/svg+xml";
                        else if (strcmp(ext, ".txt")==0) ct = "text/plain";
                        else if (strcmp(ext, ".ico")==0) ct = "image/x-icon";
                    }
                    char head[512];
                    snprintf(head, sizeof(head),
                        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                        ct, fr);
                    http_io_send(client, ssl, head, strlen(head));
                    if (fr) http_io_send(client, ssl, fbuf, fr);
                    close(client);
                    return;
                }
            }
        }
    }

    const char* response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    http_io_send(client, ssl, response, strlen(response));
    close(client);
}

/* Una connessione per thread (detached): l'accept loop deve restare libero
 * anche mentre un handler — o una pagina BSP — FETCHa lo stesso server
 * (self-fetch altrimenti in deadlock; parità col server HTTP managed, async). */
typedef struct {
    gmb_http_server_t* s;
    gmb_net_sock_t client;
} http_conn_job_t;

static void* http_conn_thread(void* arg)
{
    http_conn_job_t* j = (http_conn_job_t*)arg;
    gmb_http_server_t* s = j->s;
    gmb_net_sock_t client = j->client;
    free(j);
    if (s->ssl_ctx)
    {
        /* HTTPS: handshake TLS server sul client appena accettato. */
        SSL* ssl = SSL_new(s->ssl_ctx);
        if (!ssl) { close(client); return NULL; }
        SSL_set_fd(ssl, (int)client);
        SSL_set_accept_state(ssl);
        int ok = 0;
        for (int i = 0; i < 50 && !ok; i++)
        {
            int r = SSL_accept(ssl);
            if (r == 1) { ok = 1; break; }
            int e = SSL_get_error(ssl, r);
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
            {
                gmb_net_wait_fd((int)client, 1000);
                continue;
            }
            break;
        }
        if (!ok) { SSL_free(ssl); close(client); return NULL; }
        http_handle_conn(s, client, ssl);
        SSL_free(ssl);
    }
    else
    {
        http_handle_conn(s, client, NULL);
    }
    return NULL;
}

static void* http_server_loop(void* arg)
{
    gmb_http_server_t* s = (gmb_http_server_t*)arg;
    while (s->running)
    {
        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        gmb_net_sock_t client = accept(s->sock, (struct sockaddr*)&cli, &clen);
        if (!GMB_NET_SOCK_OK(client)) continue;
        http_conn_job_t* j = (http_conn_job_t*)malloc(sizeof(http_conn_job_t));
        if (!j) { close(client); continue; }
        j->s = s;
        j->client = client;
        pthread_t th;
        if (pthread_create(&th, NULL, http_conn_thread, j) != 0)
        {
            free(j);
            close(client);
            continue;
        }
        pthread_detach(th);
    }
    return NULL;
}

void gmb_http_server_stop(int64_t server)
{
    gmb_http_server_t* s = http_get(server);
    if (!s) return;
    s->running = 0;
    if (GMB_NET_SOCK_OK(s->sock)) close(s->sock);
    s->sock = 0;
    /* non joiniamo il thread (il BASIC può riavviare); il thread muore al prossimo accept fallito */
}

/* ============================================================================
 * Multipart/form-data parser (§ HttpMultiPart)
 * Parsa il body multipart e riempie array di stringhe C plain (il chiamante
 * costruisce gli oggetti VM). Ritorna 0 se parsato, -1 se Content-Type non è
 * multipart/form-data o il boundary manca. Liberare con gmb_net_multipart_free.
 * ============================================================================ */

/* ============================================================================
 * Multipart/form-data parser (§ HttpMultiPart)
 * Parsa il body multipart e riempie array di stringhe C plain (il chiamante
 * costruisce gli oggetti VM). Ritorna 0 se parsato, -1 se Content-Type non è
 * multipart/form-data o il boundary manca. Liberare con gmb_net_multipart_free.
 * ============================================================================ */
void gmb_net_multipart_free(char** names, char** vals, char** files, int n)
{
    for (int i = 0; i < n; i++) { free(names[i]); free(vals[i]); if (files) free(files[i]); }
    free(names); free(vals); if (files) free(files);
}

int gmb_net_http_parse_multipart(const char* body, const char* content_type,
                                 char*** out_fn, char*** out_fv, int* out_nf,
                                 char*** out_fln, char*** out_ffn, char*** out_ft, char*** out_fd, int* out_nfl)
{
    if (!body || !content_type || strncasecmp(content_type, "multipart/form-data", 19) != 0) return -1;
    const char* bi = strcasestr(content_type, "boundary=");
    if (!bi) return -1;
    bi += 9;
    while (*bi == ' ') bi++;
    char boundary[256];
    size_t bl = 0;
    if (*bi == '"') { bi++; while (*bi && *bi != '"' && bl < sizeof(boundary)-1) boundary[bl++] = *bi++; }
    else while (*bi && *bi != ';' && *bi != '\r' && *bi != '\n' && *bi != ' ' && bl < sizeof(boundary)-1) boundary[bl++] = *bi++;
    boundary[bl] = '\0';
    if (bl == 0) return -1;

    char** fn = NULL; char** fv = NULL; int nf = 0;
    char** fln = NULL; char** ffn = NULL; char** ft = NULL; char** fd = NULL; int nfl = 0;

    const char* p = body;
    while (p && *p)
    {
        const char* d = strstr(p, "--");
        if (!d) break;
        if (strncmp(d + 2, boundary, bl) != 0) break;
        const char* ps = d + 2 + bl;
        if (strncmp(ps, "--", 2) == 0) break;
        if (*ps == '\r') ps++;
        if (*ps == '\n') ps++;
        if (!*ps) break;

        const char* he = strstr(ps, "\r\n\r\n");
        const char* bs = NULL;
        if (he) bs = he + 4;
        else { const char* alt = strstr(ps, "\n\n"); if (!alt) break; bs = alt + 2; }

        size_t hlen = (size_t)(bs - ps);
        char* hbuf = (char*)malloc(hlen + 1);
        if (!hbuf) break;
        memcpy(hbuf, ps, hlen); hbuf[hlen] = '\0';

        char dispName[512] = {0}, fileName[1024] = {0}, fileType[128] = {0};
        int isFile = 0;
        const char* cd = strcasestr(hbuf, "Content-Disposition:");
        if (cd)
        {
            const char* nm = strcasestr(cd, "name=");
            if (nm) { nm += 5; while (*nm==' ') nm++; size_t di=0; if (*nm=='"'){nm++;while(*nm&&*nm!='"'&&di<sizeof(dispName)-1)dispName[di++]=*nm++;} else while(*nm&&*nm!=';'&&*nm!='\r'&&di<sizeof(dispName)-1)dispName[di++]=*nm++; }
            const char* fm = strcasestr(cd, "filename=");
            if (fm) { isFile=1; fm+=9; while(*fm==' ')fm++; size_t fi=0; if(*fm=='"'){fm++;while(*fm&&*fm!='"'&&fi<sizeof(fileName)-1)fileName[fi++]=*fm++;} else while(*fm&&*fm!=';'&&*fm!='\r'&&fi<sizeof(fileName)-1)fileName[fi++]=*fm++; }
        }
        const char* ct = strcasestr(hbuf, "Content-Type:");
        if (ct) { ct+=13; while(*ct==' ')ct++; size_t ci=0; while(*ct&&*ct!='\r'&&*ct!='\n'&&ci<sizeof(fileType)-1)fileType[ci++]=*ct++; }
        free(hbuf);

        const char* endP = strstr(bs, "--");
        if (!endP) break;
        if (strncmp(endP + 2, boundary, bl) != 0) break;
        size_t bLen = (size_t)(endP - bs);
        while (bLen > 0 && (bs[bLen-1]=='\n' || bs[bLen-1]=='\r')) bLen--;
        char* partBody = (char*)malloc(bLen + 1);
        if (!partBody) break;
        memcpy(partBody, bs, bLen); partBody[bLen] = '\0';

        if (isFile)
        {
            int i = nfl;
            fln = (char**)realloc(fln, (nfl + 1) * sizeof(char*));
            ffn = (char**)realloc(ffn, (nfl + 1) * sizeof(char*));
            ft  = (char**)realloc(ft,  (nfl + 1) * sizeof(char*));
            fd  = (char**)realloc(fd,  (nfl + 1) * sizeof(char*));
            if (!fln || !ffn || !ft || !fd) { free(partBody); break; }
            fln[i] = strdup(dispName); ffn[i] = strdup(fileName);
            ft[i] = strdup(fileType);  fd[i] = partBody;
            nfl++;
        }
        else
        {
            int i = nf;
            fn = (char**)realloc(fn, (nf + 1) * sizeof(char*));
            fv = (char**)realloc(fv, (nf + 1) * sizeof(char*));
            if (!fn || !fv) { free(partBody); break; }
            fn[i] = strdup(dispName); fv[i] = partBody;
            nf++;
        }
        p = endP;
    }

    *out_fn = fn; *out_fv = fv; *out_nf = nf;
    *out_fln = fln; *out_ffn = ffn; *out_ft = ft; *out_fd = fd; *out_nfl = nfl;
    return 0;
}

/* Costruisce il dict HTTPPOSTEDDATA (gmb_val_t) da campi/file multipart parsati. */
static gmb_val_t gmb_multipart_build_val(char** fn, char** fv, int nf,
                                         char** fln, char** ffn, char** ft, char** fd, int nfl)
{
    gmb_val_t result = gmb_make_val_dict(gmb_dict_new());
    gmb_list_t fields = gmb_list_new();
    for (int i = 0; i < nf; i++)
    {
        gmb_val_t f = gmb_make_val_dict(gmb_dict_new());
        gmb_val_dict_set(f, "Name", gmb_make_val_string(fn[i] ? fn[i] : ""));
        gmb_val_dict_set(f, "Value", gmb_make_val_string(fv[i] ? fv[i] : ""));
        gmb_list_add(&fields, f);
    }
    gmb_list_t files = gmb_list_new();
    for (int i = 0; i < nfl; i++)
    {
        gmb_val_t f = gmb_make_val_dict(gmb_dict_new());
        gmb_val_dict_set(f, "Name", gmb_make_val_string(fln[i] ? fln[i] : ""));
        gmb_val_dict_set(f, "FileName", gmb_make_val_string(ffn[i] ? ffn[i] : ""));
        gmb_val_dict_set(f, "ContentType", gmb_make_val_string(ft[i] ? ft[i] : ""));
        gmb_val_dict_set(f, "Data", gmb_make_val_string(fd[i] ? fd[i] : ""));
        gmb_list_add(&files, f);
    }
    gmb_val_dict_set(result, "FormFields", gmb_make_val_list_take(&fields));
    gmb_val_dict_set(result, "FileFields", gmb_make_val_list_take(&files));
    gmb_list_free(&fields);
    gmb_list_free(&files);
    return result;
}

/* req.ParseMultipartForm() per il backend C99: legge Body + Content-Type dal dict req,
 * parsa multipart/form-data e ritorna il dict HTTPPOSTEDDATA (o NIL se non multipart). */
gmb_val_t gmb_http_req_parse_multipart(gmb_val_t req)
{
    gmb_dict_t rd = gmb_val_as_dict(req);
    gmb_val_t vb = gmb_dict_get(rd, "Body");
    gmb_val_t vc = gmb_dict_get(rd, "__content_type");
    const char* body = (vb.kind == GMB_TYPE_STRING && vb.v_string.data) ? vb.v_string.data : "";
    const char* ct = (vc.kind == GMB_TYPE_STRING && vc.v_string.data) ? vc.v_string.data : "";
    char** fn = NULL; char** fv = NULL; int nf = 0;
    char** fln = NULL; char** ffn = NULL; char** ft = NULL; char** fd = NULL; int nfl = 0;
    if (gmb_net_http_parse_multipart(body, ct, &fn, &fv, &nf, &fln, &ffn, &ft, &fd, &nfl) != 0)
        return gmb_val_nil();
    gmb_val_t result = gmb_multipart_build_val(fn, fv, nf, fln, ffn, ft, fd, nfl);
    gmb_net_multipart_free(fn, fv, NULL, nf);
    for (int i = 0; i < nfl; i++) { free(fln[i]); free(ffn[i]); free(ft[i]); free(fd[i]); }
    free(fln); free(ffn); free(ft); free(fd);
    return result;
}

/* --- URL-decode form (application/x-www-form-urlencoded) --------------------------------
 * Self-contained (bsp_url_decode è static in gmb_bsp.c): '+' → spazio, %XX → byte. */
static void gmb_form_url_decode(char* s)
{
    if (!s) return;
    char* w = s;
    for (char* r = s; *r; r++) {
        if (*r == '+') { *w++ = ' '; }
        else if (*r == '%' && r[1] && r[2]) {
            int hi = r[1], lo = r[2];
            int hv = (hi >= '0' && hi <= '9') ? hi - '0' : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
            int lv = (lo >= '0' && lo <= '9') ? lo - '0' : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
            if (hv >= 0 && lv >= 0) { *w++ = (char)((hv << 4) | lv); r += 2; }
            else { *w++ = *r; }
        }
        else { *w++ = *r; }
    }
    *w = 0;
}

/* Parser form-urlencoded MODEL-AGNOSTICO (§0bis): dal body + Content-Type produce gli array
 * di chiavi/valori (stringhe URL-decodificate, malloc'd) che ogni VM converte nel PROPRIO
 * modello oggetti (C99: gmb_val_t boxato qui sotto; VM/C: gmb_obj_shared_val_t). Ritorna 0
 * se Content-Type è form-urlencoded (anche body vuoto → n=0), -1 altrimenti. */
int gmb_http_form_parse(const char* body, const char* ct, char*** keys_out, char*** vals_out, int* n_out)
{
    if (keys_out) *keys_out = NULL;
    if (vals_out) *vals_out = NULL;
    if (n_out) *n_out = 0;
    if (!ct || !strstr(ct, "application/x-www-form-urlencoded"))
        return -1;
    if (!body || !*body)
        return 0;
    char** keys = NULL; char** vals = NULL; int n = 0, cap = 0;
    const char* p = body;
    while (*p)
    {
        const char* amp = strchr(p, '&');
        size_t seglen = amp ? (size_t)(amp - p) : strlen(p);
        if (seglen > 0)
        {
            char* seg = (char*)malloc(seglen + 1);
            if (seg)
            {
                memcpy(seg, p, seglen); seg[seglen] = 0;
                char* eq = strchr(seg, '=');
                if (eq) { *eq = 0; gmb_form_url_decode(seg); gmb_form_url_decode(eq + 1); }
                else gmb_form_url_decode(seg);
                if (n >= cap)
                {
                    int nc = cap ? cap * 2 : 8;
                    char** nk = (char**)realloc(keys, (size_t)nc * sizeof(char*));
                    char** nv = (char**)realloc(vals, (size_t)nc * sizeof(char*));
                    if (!nk || !nv) { free(seg); break; }
                    keys = nk; vals = nv; cap = nc;
                }
                keys[n] = strdup(seg);
                vals[n] = strdup(eq ? (eq + 1) : "");
                n++;
                free(seg);
            }
        }
        if (!amp) break;
        p = amp + 1;
    }
    if (keys_out) *keys_out = keys; else { for (int i=0;i<n;i++) free(keys[i]); free(keys); }
    if (vals_out) *vals_out = vals; else { for (int i=0;i<n;i++) free(vals[i]); free(vals); }
    if (n_out) *n_out = n;
    return 0;
}

/* Libera gli array di gmb_http_form_parse. */
void gmb_http_form_free(char** keys, char** vals, int n)
{
    for (int i = 0; i < n; i++) { free(keys ? keys[i] : NULL); free(vals ? vals[i] : NULL); }
    free(keys); free(vals);
}

/* HTTPREQUEST.ParseForm() (modello C99): body application/x-www-form-urlencoded → dict
 * { campo: valore } di stringhe URL-decodificate; NIL se il Content-Type non è
 * form-urlencoded (come l'IL, che ritorna true senza popolare ⇒ nil). L'oggetto ritornato
 * ha un proprio dict ⇒ va scritto nel suo stesso dict (mai una copia). */
gmb_val_t gmb_http_req_parse_form(gmb_val_t req)
{
    gmb_dict_t rd = gmb_val_as_dict(req);
    gmb_val_t vb = gmb_dict_get(rd, "Body");
    gmb_val_t vc = gmb_dict_get(rd, "__content_type");
    const char* body = (vb.kind == GMB_TYPE_STRING && vb.v_string.data) ? vb.v_string.data : "";
    const char* ct = (vc.kind == GMB_TYPE_STRING && vc.v_string.data) ? vc.v_string.data : "";
    char** keys = NULL; char** vals = NULL; int n = 0;
    if (gmb_http_form_parse(body, ct, &keys, &vals, &n) != 0)
        return gmb_val_nil();
    gmb_val_t out = gmb_make_val_dict(gmb_dict_new());
    /* Scrittura IN-PLACE sul dict boxato: `gmb_val_as_dict` restituisce una COPIA per
     * valore, quindi `gmb_dict_set(&copia, …)` perdeva OGNI campo e il dict tornava vuoto
     * (sintomo: `frm.nome` = null su C99Native in http/parseform.bas, mentre il parse
     * riusciva: n=3 con le chiavi giuste). Vedi
     * Bugs/(solved) - C99Native-ParseForm-Content-Type. */
    for (int i = 0; i < n; i++)
        gmb_val_dict_set(out, keys[i], gmb_make_val_string(vals[i]));
    gmb_http_form_free(keys, vals, n);
    return out;
}

/* Varianti gmb_string_t dei getter IP (parità C99: niente buffer inline nell'emitter). */
gmb_string_t gmb_net_remote_ip_str(int64_t handle) {
    char buf[128];
    gmb_net_remote_ip(handle, buf, sizeof(buf));
    return gmb_string_alloc(buf);
}
gmb_string_t gmb_net_local_host_str(void) {
    char buf[128];
    gmb_net_local_host(buf, sizeof(buf));
    return gmb_string_alloc(buf);
}
gmb_string_t gmb_net_resolve_host_str(const char* host) {
    char buf[128];
    gmb_net_resolve_host(host, buf, sizeof(buf));
    return gmb_string_alloc(buf);
}
