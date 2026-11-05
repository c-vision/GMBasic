/* poll.h (Windows) — `poll()` su WSAPoll. Su Winsock i descrittori sono SOCKET (non int),
 * quindi `struct pollfd` è già dichiarato da <winsock2.h> (fd/events/revents) e qui si
 * aggiunge solo la funzione: NON si rimappa `pollfd` con un #define (renderebbe invalida
 * la dichiarazione, e `poll` risulterebbe non dichiarata).
 */
#ifndef GMB_WIN32_POLL_H
#define GMB_WIN32_POLL_H
#if defined(_WIN32) || defined(_WIN64)
#include "gmb_win32.h"
#include <winsock2.h>

#ifndef POLLIN
#define POLLIN  0x0001
#endif
#ifndef POLLOUT
#define POLLOUT 0x0004
#endif
#ifndef POLLERR
#define POLLERR 0x0008
#endif
#ifndef POLLHUP
#define POLLHUP 0x0010
#endif

typedef unsigned long nfds_t;

static inline int poll(struct pollfd* fds, nfds_t n, int timeout)
{
    return WSAPoll((WSAPOLLFD*)fds, (ULONG)n, timeout);
}
#endif /* _WIN32 */
#endif /* GMB_WIN32_POLL_H */
