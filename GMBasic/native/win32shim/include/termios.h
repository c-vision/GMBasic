/* termios.h (Windows) — superfici minime per compilare. La TUI su Windows usa le API
 * della console (SetConsoleMode), non il raw mode POSIX: qui si espongono tipi e
 * costanti, e `cfmakeraw`/`tcgetattr`/`tcsetattr` restano no-op espliciti.
 */
#ifndef GMB_WIN32_TERMIOS_H
#define GMB_WIN32_TERMIOS_H
#if defined(_WIN32) || defined(_WIN64)
#include "gmb_win32.h"

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 32
struct termios {
    tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed, c_ospeed;
};

/* flag POSIX usati nel codice per decidere il raw mode */
#define IGNBRK  0x0001
#define BRKINT  0x0002
#define PARMRK  0x0008
#define ISTRIP  0x0020
#define INLCR   0x0040
#define IGNCR   0x0080
#define ICRNL   0x0100
#define IXON    0x0400
#define OPOST   0x0001
#define ECHO    0x0008
#define ECHONL  0x0010
#define ICANON  0x0100
#define ISIG    0x0080
#define IEXTEN  0x0400
#define CSIZE   0x0030
#define PARENB  0x0100
#define CS8     0x0030
#define VTIME   5
#define VMIN    6
#define TCSANOW 0

static inline int tcgetattr(int fd, struct termios* t) { (void)fd; if (t) memset(t, 0, sizeof(*t)); return 0; }
static inline int tcsetattr(int fd, int act, const struct termios* t) { (void)fd; (void)act; (void)t; return 0; }
static inline void cfmakeraw(struct termios* t) { (void)t; }
static inline speed_t cfgetispeed(const struct termios* t) { (void)t; return 0; }
static inline speed_t cfgetospeed(const struct termios* t) { (void)t; return 0; }
#endif /* _WIN32 */
#endif /* GMB_WIN32_TERMIOS_H */
