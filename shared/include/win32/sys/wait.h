/* sys/wait.h (Windows) — <sys/wait.h> non esiste su Win32.
 *
 * Nessun `wait()` POSIX: i processi figli si attendono con WaitForSingleObject e il codice
 * d'uscita si legge con GetExitCodeProcess (vedi gmb_ipc_client.c, ramo _WIN32). Qui si
 * forniscono le MACRO di decodifica dello status, così il codice che le usa compila e
 * interpreta un exit code già "grezzo" nel modo consueto.
 */
#pragma once
#include "gmb_win32.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

#include <sys/types.h>   /* pid_t lo fornisce mingw */

/* waitpid: su Windows il "pid" è il process id. Si apre il processo, si aspetta e si legge
 * l'exit code. `status` riceve un valore già decodificabile con WEXITSTATUS. */
static inline pid_t waitpid(pid_t pid, int* status, int options)
{
    HANDLE h;
    DWORD code = 0;
    (void)options;
    h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) { if (status) *status = -1; return -1; }
    WaitForSingleObject(h, INFINITE);
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    if (status) *status = (int)code;
    return pid;
}

#ifndef WIFEXITED
#define WIFEXITED(status)   (1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) ((status) & 0xFF)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(status)    (0)
#endif
#ifndef WIFSTOPPED
#define WIFSTOPPED(status)  (0)
#endif
#endif /* _WIN32 */
