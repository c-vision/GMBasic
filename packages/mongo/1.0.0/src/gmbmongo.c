/* gmbmongo.c — shim FFI minimale del pacchetto `mongo` (namespace MONGO).
 *
 * Semantica ONESTA, senza fingere un client MongoDB completo:
 *  - AVAILABLE() = 1  → la libreria (questo shim) è presente e caricabile;
 *  - CONNECT(url)     → tenta una connessione TCP REALE a host:porta dell'URL
 *                       (timeout breve); 0 se non raggiungibile, altrimenti un handle;
 *  - PING(h)          → 0: l'handshake MongoDB (OP_MSG/hello) NON è implementato in questo
 *                       shim, quindi un socket aperto non è un server verificato. Meglio un
 *                       fallimento esplicito che un falso successo;
 *  - CLOSE(h)         → chiude il socket.
 * Con un server assente (porta chiusa) il programma stampa "nomongo", che è l'oracolo del
 * MiniTest require_mongo.
 */
#include "gmbmongo.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define GMB_INVALID_SOCK INVALID_SOCKET
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
typedef int sock_t;
#define GMB_INVALID_SOCK (-1)
#endif

int gmbmongo_available(void)
{
    return 1;   /* lo shim è caricato: la lib "c'è" */
}

/* "mongodb://host:porta[/...]" oppure "host:porta" → host/porta. 1 = ok. */
static int parse_host_port(const char* url, char* host, size_t hcap, int* port)
{
    if (!url || !*url) return 0;
    const char* p = url;
    const char* scheme = "mongodb://";
    if (strncmp(p, scheme, strlen(scheme)) == 0) p += strlen(scheme);
    const char* at = strrchr(p, '@');
    if (at) p = at + 1;
    const char* slash = strchr(p, '/');
    size_t hostlen = slash ? (size_t)(slash - p) : strlen(p);
    const char* colon = NULL;
    for (size_t i = 0; i < hostlen; i++) if (p[i] == ':') colon = p + i;
    size_t n = colon ? (size_t)(colon - p) : hostlen;
    if (n == 0 || n >= hcap) return 0;
    memcpy(host, p, n); host[n] = 0;
    *port = 27017;
    if (colon) {
        int v = atoi(colon + 1);
        if (v > 0 && v < 65536) *port = v;
    }
    return 1;
}

int gmbmongo_connect(const char* url)
{
    char host[256];
    int port = 27017;
    if (!parse_host_port(url, host, sizeof(host), &port)) return 0;

#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa; if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
#endif

    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return 0;

    int fd = -1;
    for (struct addrinfo* ai = res; ai && fd < 0; ai = ai->ai_next) {
        sock_t s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == GMB_INVALID_SOCK) continue;
        /* non-blocking + select con timeout: niente attese lunghe in CI */
#if defined(_WIN32) || defined(_WIN64)
        u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
#else
        int fl = fcntl(s, F_GETFL, 0); if (fl >= 0) fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
        int rc = connect(s, ai->ai_addr, (int)ai->ai_addrlen);
        int ok = (rc == 0);
        if (!ok) {
            fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf);
            struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 500000;   /* 1.5s */
            if (select((int)(s + 1), NULL, &wf, NULL, &tv) > 0) {
                int soerr = 0; socklen_t len = sizeof(soerr);
                if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &len) == 0 && soerr == 0) ok = 1;
            }
        }
        if (ok) fd = (int)s;
        else {
#if defined(_WIN32) || defined(_WIN64)
            closesocket(s);
#else
            close(s);
#endif
        }
    }
    freeaddrinfo(res);
    return fd > 0 ? fd : 0;
}

int gmbmongo_ping(int h)
{
    /* Nessun handshake MongoDB (OP_MSG "hello") implementato in questo shim: un socket
     * aperto NON è un server verificato. 0 = fallimento esplicito (mai falso successo). */
    (void)h;
    return 0;
}

int gmbmongo_close(int h)
{
    if (h > 0) {
#if defined(_WIN32) || defined(_WIN64)
        closesocket((sock_t)h);
        WSACleanup();
#else
        close(h);
#endif
    }
    return 0;
}
