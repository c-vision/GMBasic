/* gmb_net_http_selfcheck.c — verifica gmb_net_http_get contro un server socket locale.
 *
 * PORTABILE (2026-10-29): il server un-request girava in un processo FIGLIO creato con
 * `fork()`, che non esiste su Windows. Ora gira in un THREAD (pthread), che è supportato
 * ovunque (winpthreads sul toolchain Windows) e non cambia la sostanza del test: bind +
 * accept + risposta HTTP, mentre il "main" fa la GET.
 */
#define _DEFAULT_SOURCE
#include "gmb_network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(_WIN32) || defined(_WIN64)
#  define GMB_TEST_CLOSE(s) closesocket(s)
#  define GMB_TEST_SETOPT(fd, lvl, opt, val) \
       setsockopt((fd), (lvl), (opt), (const char*)(val), sizeof(*(val)))
#else
#  define GMB_TEST_CLOSE(s) close(s)
#  define GMB_TEST_SETOPT(fd, lvl, opt, val) \
       setsockopt((fd), (lvl), (opt), (val), sizeof(*(val)))
#endif

static int g_srv = -1;

static void* server_once(void* arg)
{
    (void)arg;
    int c = accept(g_srv, NULL, NULL);
    if (c < 0) return NULL;
    char buf[2048];
    (void)recv(c, buf, sizeof(buf), 0);
    const char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello";
    (void)send(c, resp, (int)strlen(resp), 0);
    GMB_TEST_CLOSE(c);
    return NULL;
}

int main(void)
{
    int port = 47899;
    g_srv = socket(AF_INET, SOCK_STREAM, 0);
    if (g_srv < 0) { printf("gmb_net_http_selfcheck: socket FAIL\n"); return 1; }
    int opt = 1;
    GMB_TEST_SETOPT(g_srv, SOL_SOCKET, SO_REUSEADDR, &opt);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    if (bind(g_srv, (struct sockaddr*)&a, sizeof(a)) != 0 || listen(g_srv, 1) != 0) {
        printf("gmb_net_http_selfcheck: bind/listen FAIL\n");
        return 1;
    }

    pthread_t th;
    if (pthread_create(&th, NULL, server_once, NULL) != 0) {
        printf("gmb_net_http_selfcheck: thread FAIL\n");
        GMB_TEST_CLOSE(g_srv);
        return 1;
    }
    usleep(300000);

    char url[64];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/", port);
    char* body = gmb_net_http_get(url);
    int ok = body && strcmp(body, "hello") == 0;
    if (body) free(body);
    pthread_join(th, NULL);
    GMB_TEST_CLOSE(g_srv);
    printf("gmb_net_http_selfcheck: %s (body=%s)\n", ok ? "ok" : "FAIL", body ? body : "(null)");
    return ok ? 0 : 1;
}
