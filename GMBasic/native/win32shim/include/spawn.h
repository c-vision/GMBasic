/* spawn.h (Windows) — `posix_spawn*` su CreateProcess.
 *
 * Windows non ha `fork`, quindi nemmeno `posix_spawn` POSIX. Ma la forma che il nostro codice
 * usa (file_actions con adddup2/addclose + argv) si mappa bene su CreateProcess con
 * STARTUPINFO dei tre handle standard. Implementazione REALE, header-only:
 *   - ogni `adddup2(fd, 0|1|2)` duplica l'handle del fd come INERITABILE e lo assegna;
 *   - `addclose` chiude l'handle nel figlio (non lo si eredita);
 *   - `posix_spawn`/`posix_spawnp` lanciano il processo e ritornano il PID Windows.
 * `waitpid` (vedi sys/wait.h) aspetta quel PID e ne legge l'exit code.
 */
#pragma once
#include "gmb_win32.h"

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>

#include <sys/types.h>   /* pid_t lo fornisce mingw */

#define POSIX_SPAWN_FILE_ACTIONS_MAX 16

typedef struct {
    int kind;      /* 0 = dup2, 1 = close */
    int fd;
    int target;    /* 0/1/2 per dup2 */
} gmb_spawn_action_t;

typedef struct {
    gmb_spawn_action_t acts[POSIX_SPAWN_FILE_ACTIONS_MAX];
    int n;
} posix_spawn_file_actions_t;

static inline int posix_spawn_file_actions_init(posix_spawn_file_actions_t* fa)
{
    if (!fa) return -1;
    fa->n = 0;
    return 0;
}
static inline int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* fa)
{
    if (fa) fa->n = 0;
    return 0;
}
static inline int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* fa, int fd, int target)
{
    if (!fa || fa->n >= POSIX_SPAWN_FILE_ACTIONS_MAX) return -1;
    fa->acts[fa->n].kind = 0; fa->acts[fa->n].fd = fd; fa->acts[fa->n].target = target; fa->n++;
    return 0;
}
static inline int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* fa, int fd)
{
    if (!fa || fa->n >= POSIX_SPAWN_FILE_ACTIONS_MAX) return -1;
    fa->acts[fa->n].kind = 1; fa->acts[fa->n].fd = fd; fa->acts[fa->n].target = -1; fa->n++;
    return 0;
}

static inline HANDLE gmb_spawn_dup_inherit(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd), dup = NULL;
    if (h == INVALID_HANDLE_VALUE || h == NULL) return NULL;
    if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup,
                         0, TRUE, DUPLICATE_SAME_ACCESS))
        return NULL;
    return dup;
}

static inline int gmb_spawn_common(const char* path, posix_spawn_file_actions_t* fa,
                                   char* const argv[], pid_t* pid_out)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdline[8192] = {0};
    size_t off = 0;
    HANDLE st[3] = { NULL, NULL, NULL };

    if (!path || !pid_out) return -1;
    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);

    if (fa) {
        for (int i = 0; i < fa->n; i++) {
            if (fa->acts[i].kind == 0 && fa->acts[i].target >= 0 && fa->acts[i].target <= 2) {
                HANDLE d = gmb_spawn_dup_inherit(fa->acts[i].fd);
                if (d) st[fa->acts[i].target] = d;
            }
        }
    }
    if (!st[0]) st[0] = GetStdHandle(STD_INPUT_HANDLE);
    if (!st[1]) st[1] = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!st[2]) st[2] = GetStdHandle(STD_ERROR_HANDLE);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = st[0]; si.hStdOutput = st[1]; si.hStdError = st[2];

    /* command line: argv uniti (CreateProcess vuole una stringa) */
    for (int i = 0; argv && argv[i]; i++) {
        size_t l = strlen(argv[i]);
        if (off + l + 4 >= sizeof cmdline) break;
        if (off) cmdline[off++] = ' ';
        cmdline[off++] = '"';
        memcpy(cmdline + off, argv[i], l);
        off += l;
        cmdline[off++] = '"';
        cmdline[off] = '\0';
    }

    int ok = CreateProcessA(path, cmdline[0] ? cmdline : NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    for (int i = 0; i < 3; i++) {
        if (st[i] && st[i] != GetStdHandle(STD_INPUT_HANDLE) &&
            st[i] != GetStdHandle(STD_OUTPUT_HANDLE) && st[i] != GetStdHandle(STD_ERROR_HANDLE))
            CloseHandle(st[i]);
    }
    if (!ok) return -1;
    *pid_out = (pid_t)pi.dwProcessId;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

static inline int posix_spawn(pid_t* pid, const char* path, posix_spawn_file_actions_t* fa,
                              const void* attr, char* const argv[], char* const envp[])
{
    (void)attr; (void)envp;
    return gmb_spawn_common(path, fa, argv, pid);
}
static inline int posix_spawnp(pid_t* pid, const char* file, posix_spawn_file_actions_t* fa,
                               const void* attr, char* const argv[], char* const envp[])
{
    (void)attr; (void)envp;
    return gmb_spawn_common(file, fa, argv, pid);
}
#endif /* _WIN32 */
