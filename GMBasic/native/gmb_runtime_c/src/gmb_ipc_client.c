/* gmb_ipc_client.c — IPC v1 client: esegue processo nativo MLX con protocollo JSON.
 *
 * R2: invece di spawnare il processo con argv posizionali e leggere il report
 * da file, inviamo una richiesta JSON su stdin e leggiamo la risposta JSON da
 * stdout. Il processo (gmb_mlx_ipc_host o gmb_mlx_finetune in modalita' ipc)
 * risponde con messaggi delimitati da newline.
 *
 * Per compatibilità: se il processo non supporta il protocollo IPC (fallback),
 * torniamo al comportamento vecchio (argv posizionali + report file).
 * TODO: rimuovere fallback quando l'harness sara' migrato.
 */
#include "gmb_ipc_protocol.h"
#include "gmb_runtime.h"     /* gmb_rid() / gmb_paths_libs_cache_dir(): cache dei componenti */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <io.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#endif

/* --------------------------------- JSON builders --------------------------- */

static int jsncat(char* buf, size_t cap, size_t* off, const char* s)
{
    size_t sl = strlen(s);
    if (*off + sl >= cap) return -1;
    memcpy(buf + *off, s, sl + 1);
    *off += sl;
    return 0;
}
static int jsncatf(char* buf, size_t cap, size_t* off, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - *off) return -1;
    *off += n;
    return 0;
}

#define JSNCAT(s) do { if (jsncat(buf, cap, &of, s) != 0) return -1; } while(0)
#define JSNCATF(...) do { if (jsncatf(buf, cap, &of, __VA_ARGS__) != 0) return -1; } while(0)

int gmb_ipc_build_finetune(const char* model_dir, const char* dataset,
                           const char* holdout, const char* ckpt,
                           int rank, int layer, int steps, int batch,
                           double lr, int seed, const char* frozen,
                           char* buf, size_t cap)
{
    size_t of = 0;
    JSNCAT("{\"id\":1,\"method\":\"finetune\",\"params\":{");
    JSNCATF("\"model_dir\":\"%s\"", model_dir ? model_dir : "");
    JSNCATF(",\"dataset\":\"%s\"", dataset ? dataset : "");
    JSNCATF(",\"holdout\":\"%s\"", holdout ? holdout : "");
    JSNCATF(",\"ckpt\":\"%s\"", ckpt ? ckpt : "");
    JSNCATF(",\"rank\":%d,\"layer\":%d,\"steps\":%d,\"batch\":%d", rank, layer, steps, batch);
    JSNCATF(",\"lr\":%.6f,\"seed\":%d", lr, seed);
    if (frozen && frozen[0]) JSNCATF(",\"frozen\":\"%s\"", frozen);   /* §4: frozen=all-but-head */
    JSNCAT("}}\n");
    return 0;
}

int gmb_ipc_build_apply(const char* ckpt_prefix, const char* model_dir,
                        const char* dataset, char* buf, size_t cap)
{
    size_t of = 0;
    JSNCAT("{\"id\":1,\"method\":\"apply\",\"params\":{");
    JSNCATF("\"ckpt_prefix\":\"%s\"", ckpt_prefix ? ckpt_prefix : "");
    JSNCATF(",\"model\":\"%s\"", model_dir ? model_dir : "");
    JSNCATF(",\"dataset\":\"%s\"", dataset ? dataset : "");
    JSNCAT("}}\n");
    return 0;
}

int gmb_ipc_build_status(char* buf, size_t cap)
{
    size_t of = 0;
    JSNCAT("{\"id\":1,\"method\":\"status\"}\n");
    return 0;
}

int gmb_ipc_build_cancel(char* buf, size_t cap)
{
    size_t of = 0;
    JSNCAT("{\"id\":1,\"method\":\"cancel\"}\n");
    return 0;
}

/* --------------------------- risposta JSON parser ------------------------- */

/* Trova "key": in body e posiziona il cursore dopo i due punti e spazi.
 * Usato dai parser sottostanti. Ritorna puntatore o NULL. */
static const char* js_find(const char* body, const char* key)
{
    char pat[128]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(body, pat);
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Estrae un valore stringa JSON (tra virgolette) o number/bool.
 * body: deve iniziare DOPO i due punti di "key". */
static const char* js_val_end(const char* p)
{
    if (*p == '"') {
        p++;
        while (*p && *p != '"') { if (*p == '\\') p++; p++; }
        return *p == '"' ? p + 1 : p;
    }
    /* number o boolean: fino a virgola, spazio, } o newline */
    while (*p && *p != ',' && *p != ' ' && *p != '\t' && *p != '}' && *p != '\n') p++;
    return p;
}

int gmb_ipc_extract(const char* body, const char* key, char* out, size_t cap)
{
    if (!out || cap == 0) return -1;
    out[0] = 0;
    const char* p = js_find(body, key);
    if (!p) return -1;
    if (*p == '"') {
        /* stringa */
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < cap) {
            if (*p == '\\') { p++; if (*p && i + 1 < cap) out[i++] = *p; }
            else out[i++] = *p;
            p++;
        }
        out[i] = 0;
    } else {
        /* number o bool: copia fino a delimitatore */
        const char* e = js_val_end(p);
        size_t n = (size_t)(e - p);
        if (n >= cap) n = cap - 1;
        memcpy(out, p, n);
        out[n] = 0;
    }
    return 0;
}

double gmb_ipc_extract_dbl(const char* body, const char* key, double dflt)
{
    char tmp[64];
    if (gmb_ipc_extract(body, key, tmp, sizeof tmp) != 0) return dflt;
    return strtod(tmp, NULL);
}

long gmb_ipc_extract_long(const char* body, const char* key, long dflt)
{
    char tmp[64];
    if (gmb_ipc_extract(body, key, tmp, sizeof tmp) != 0) return dflt;
    return strtol(tmp, NULL, 10);
}

int gmb_ipc_result_ok(const char* body)
{
    const char* p = js_find(body, "ok");
    if (!p) return 0;
    return (strncmp(p, "true", 4) == 0);
}

int gmb_ipc_response_type(const char* body)
{
    const char* p = js_find(body, "type");
    if (!p) return -2;
    if (strncmp(p, "\"result\"", 7) == 0) return 0;
    if (strncmp(p, "\"error\"", 6) == 0) return -1;
    if (strncmp(p, "\"progress\"", 9) == 0) return 1;
    return -2;
}

/* -------------------------- client esecuzione IPC ------------------------- */

/* Esegue un comando IPC: trova bin, spawna con pipe per stdin/stdout,
 * scrive request su stdin, legge response da stdout, ritorna exit code.
 *
 * A differenza di ft_runv (argv posizionale), questo manda una richiesta
 * JSON e riceve una risposta JSON strutturata — niente file report.
 *
 * Se il processo non supporta IPC (fallback), torna -2: il chiamante
 * puo' ripiegare su ft_runv. */
int gmb_ipc_spawn(const char* bin_name, const char* envvar,
                  const char* request_json,
                  char* response, size_t rcap,
                  int timeout_ms)
{
    char bin[1024]; bin[0] = 0;
    /* localizza binario (stessa logica di ft_find_bin) */
    const char* e = getenv(envvar);
    if (e && e[0]) { snprintf(bin, sizeof bin, "%s", e); }
    else {
        /* I binari ML stanno nella CACHE COMPONENTE: ~/.gmbasic/libs/ml/<rid>/.
         * Prima si cercava `libs-runtime/ml/<rid>/` relativo alla CWD: funzionava solo
         * lanciando dalla radice del checkout, e la cartella ora non esiste più. */
        char cache[1024], p[1400];
        gmb_paths_libs_cache_dir(cache, sizeof cache);
        if (cache[0]) {
            snprintf(p, sizeof p, "%s/ml/%s/%s", cache, gmb_libs_rid(), bin_name);
            if (access(p, X_OK) == 0) snprintf(bin, sizeof bin, "%s", p);
        }
    }
    if (!bin[0]) {
        snprintf(response, rcap, "error: runtime nativo %s non trovato", bin_name);
        return -1;
    }

#if defined(_WIN32) || defined(_WIN64)
    /* ── Windows: CreateProcess con pipe anonime (niente posix_spawn su Win32) ── */
    HANDLE r_in = NULL, w_in = NULL, r_out = NULL, w_out = NULL;
    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&r_in, &w_in, &sa, 0) || !CreatePipe(&r_out, &w_out, &sa, 0)) {
        snprintf(response, rcap, "error: pipe fallita");
        return -1;
    }
    /* gli estremi usati dal padre NON si ereditano */
    SetHandleInformation(w_in, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(r_out, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = r_in;
    si.hStdOutput = w_out;
    si.hStdError = w_out;

    BOOL created = CreateProcessA(bin, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(r_in);
    CloseHandle(w_out);
    if (!created) {
        CloseHandle(w_in); CloseHandle(r_out);
        snprintf(response, rcap, "error: CreateProcess fallito (%lu)", (unsigned long)GetLastError());
        return -1;
    }

    DWORD written = 0;
    WriteFile(w_in, request_json, (DWORD)strlen(request_json), &written, NULL);
    CloseHandle(w_in);

    response[0] = 0;
    size_t tot = 0;
    while (tot + 1 < rcap && tot < GMB_IPC_MAX_LINE) {
        DWORD got = 0;
        if (!ReadFile(r_out, response + tot, (DWORD)(rcap - tot - 1), &got, NULL) || got == 0) break;
        tot += (size_t)got;
    }
    response[tot] = 0;
    CloseHandle(r_out);

    if (timeout_ms > 0) WaitForSingleObject(pi.hProcess, (DWORD)timeout_ms);
    else WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec_dw = 0;
    GetExitCodeProcess(pi.hProcess, &ec_dw);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    int ec = (int)ec_dw;
#else
    /* pipe per stdin/stdout */
    int stdin_fds[2], stdout_fds[2];
    if (pipe(stdin_fds) != 0 || pipe(stdout_fds) != 0) {
        snprintf(response, rcap, "error: pipe fallita");
        return -1;
    }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, stdin_fds[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&fa, stdout_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, stdout_fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&fa, stdin_fds[1]);
    posix_spawn_file_actions_addclose(&fa, stdout_fds[0]);

    pid_t pid;
    char* argv[2]; argv[0] = bin; argv[1] = NULL;
    extern char** environ;
    int r = posix_spawnp(&pid, bin, &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(stdin_fds[0]);
    close(stdout_fds[1]);

    if (r != 0) {
        close(stdin_fds[1]); close(stdout_fds[0]);
        snprintf(response, rcap, "error: posix_spawnp fallito (%d)", r);
        return -1;
    }

    /* invia richiesta */
    write(stdin_fds[1], request_json, strlen(request_json));
    close(stdin_fds[1]);

    /* legge risposta (fino a timeout) */
    response[0] = 0;
    size_t tot = 0;
    /* leggiamo fino a GMB_IPC_MAX_LINE bytes o chiusura pipe */
    while (tot + 1 < rcap && tot < GMB_IPC_MAX_LINE) {
        ssize_t n = read(stdout_fds[0], response + tot, rcap - tot - 1);
        if (n <= 0) break;
        tot += (size_t)n;
    }
    response[tot] = 0;
    close(stdout_fds[0]);

    int wstatus;
    waitpid(pid, &wstatus, 0);
    int ec = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
#endif
    return ec;
}