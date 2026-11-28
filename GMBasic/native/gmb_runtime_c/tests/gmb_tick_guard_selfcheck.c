/* gmb_tick_guard_selfcheck.c — §0ter: la guardia delle attese BLOCCANTI è viva.
 *
 * Perché esiste. La guardia non è un'asserzione di comodo: è il contratto "un worker GPM non
 * si blocca mai", e senza una prova dura la prima modifica che rimette un Thread.Sleep /
 * gmb_sleep_ms su un task tornerebbe a passare in silenzio (caso reale: la VM/C dormiva il
 * worker dentro la cadenza TICK, e nessuna gamba di parità lo ha mai visto). Le prove
 * stavano in una sonda /tmp, che sparisce: qui restano nel repo e girano con `ctest`.
 *
 * COME SI PROVA UNA GUARDIA CHE UCCIDE. La guardia chiama `_Exit(2)`: non è osservabile dal
 * processo che la esegue. Quindi i casi "deve fermare" girano in un PROCESSO FIGLIO (fork):
 * il figlio esce 2, il padre controlla il codice di uscita e il messaggio (stderr del figlio
 * dirottato su un tubo). I casi "deve permettere" girano in-process.
 *
 * Le due query sono quelle vere del runtime (inversione di controllo): la guardia interroga
 * `gmb_gpm_set_on_worker_query` ("sono su un worker GPM?") e `gmb_gpm_set_park_hook` ("sai
 * parcheggiare?"). Qui si registrano funzioni finte, senza scheduler: è il contratto, non
 * un mock del comportamento.
 *
 * Eseguito da: ctest -R gmb_tick_guard_selfcheck (build di gmb_runtime_c)
 *              oppure: cmake --build <build> --target gmb_tick_guard_selfcheck && ./gmb_tick_guard_selfcheck
 */
#include "gmb_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

static int g_fails = 0;
static int g_park_calls = 0;
static int g_on_worker = 0;

static void check(int cond, const char* what, int line)
{
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}

#define CHECK(c, w) check((c) != 0, (w), __LINE__)

/* Le due query finte: il "backend" di questa prova. */
static int fake_on_worker(void) { return g_on_worker; }
static void fake_park(int ms) { (void)ms; g_park_calls++; }

/* ── processi figli: i casi in cui la guardia DEVE fermare il processo ── */
/* CH_SYSCALL_BOUNDARY = strategia "confine del syscall" (VM/C): park NON disponibile come hook,
 * ma dichiarato a livello di syscall, quindi un'attesa reale è comunque una regressione. */
enum { CH_SLEEP_IN_TASK = 1, CH_PARK = 2, CH_NO_PARK = 3, CH_SYSCALL_BOUNDARY = 4 };

#if !defined(_WIN32)
typedef struct { int code; char err[1024]; } child_t;

/* Nel figlio: registra le query finte come farebbe un backend, poi esegue il caso.
 * Ritorna SOLO se la guardia non scatta (0 = caso permesso, 1 = prova fallita). */
static int child_body(int mode)
{
    gmb_gpm_set_on_worker_query(fake_on_worker);
    g_on_worker = 1;
    /* Il figlio eredita lo stato del padre (fork): lo stato delle query si dichiara qui,
     * per caso, non si eredita — altrimenti il caso "senza park" vedrebbe l'hook del padre. */
    g_park_calls = 0;
    gmb_gpm_set_park_hook((mode == CH_PARK || mode == CH_SLEEP_IN_TASK) ? fake_park : NULL);
    /* Strategia "confine del syscall" (VM/C): park non registrato come hook, ma dichiarato.
     * Negli altri casi resta spenta. */
    gmb_gpm_set_park_at_syscall(mode == CH_SYSCALL_BOUNDARY ? 1 : 0);

    switch (mode)
    {
    case CH_PARK:            /* l'attesa cooperativa NON deve fermare il processo */
        gmb_gpm_sleep(3);
        gmb_gpm_sleep(3);
        gmb_gpm_sleep(3);
        return g_park_calls == 3 ? 0 : 1;
    case CH_NO_PARK:         /* nessuna strategia dichiarata (runtime senza scheduler) */
        gmb_gpm_sleep(3);
        gmb_sleep_ms(3);     /* su un worker, ma senza park dichiarato: attesa reale ammessa */
        return 0;
    case CH_SLEEP_IN_TASK:   /* REGRESSIONE: gmb_sleep_ms dentro un task GPM (park a hook) */
        gmb_sleep_ms(10);
        return 1;
    case CH_SYSCALL_BOUNDARY: /* REGRESSIONE: stessa attesa, park solo a CONFINE DEL SYSCALL
                               * (VM/C): deve fermare il processo lo stesso. */
        gmb_sleep_ms(5);
        return 1;
    }
    return 1;
}

/* Fork: il figlio esegue `child_body`, il padre ne raccoglie exit code e stderr. */
static int spawn_child(int mode, child_t* out)
{
    int p[2];
    pid_t pid;
    ssize_t n;
    int st = 0;

    out->code = -1;
    out->err[0] = 0;
    if (pipe(p) != 0) return -1;
    /* Il buffer di stdout NON deve essere duplicato nel figlio (lo riflushherebbe a metà
     * prova, stampando due volte le righe già emesse dal padre). */
    fflush(stdout);
    pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); return -1; }
    if (pid == 0)
    {
        close(p[0]);
        dup2(p[1], 2);      /* il messaggio FATAL del figlio arriva al padre */
        close(p[1]);
        {
            int rc = child_body(mode);
            fflush(stderr);
            _Exit(rc);
        }
    }
    close(p[1]);
    n = read(p[0], out->err, sizeof(out->err) - 1);
    if (n > 0) out->err[n] = 0;
    close(p[0]);
    if (waitpid(pid, &st, 0) < 0) return -1;
    out->code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return 0;
}

static void expect_guard_stops(int mode, const char* what, const char* needle)
{
    child_t c;
    if (spawn_child(mode, &c) != 0) { printf("FAIL: fork/pipe falliti su '%s'\n", what); g_fails++; return; }
    CHECK(c.code == 2, what);
    CHECK(strstr(c.err, "FATAL") != NULL, "il messaggio dichiara FATAL");
    CHECK(strstr(c.err, "(§0ter)") != NULL, "il messaggio cita §0ter");
    CHECK(strstr(c.err, needle) != NULL, "il messaggio nomina il percorso");
    if (c.code != 2) printf("  (exit osservato: %d) stderr: %s\n", c.code, c.err);
}
#endif

int main(void)
{
    /* ── 1. Nessuna query registrata: la guardia è un no-op ─────────────────────────────
     * Oracoli, selfcheck, runner senza scheduler non devono accorgersene. */
    printf("guardia §0ter: senza query è un no-op... ");
    gmb_gpm_set_on_worker_query(NULL);
    gmb_gpm_set_park_hook(NULL);
    CHECK(gmb_gpm_on_worker() == 0, "senza query: non sono su un worker");
    CHECK(gmb_gpm_park_available() == 0, "senza hook: nessun park");
    gmb_sleep_ms(5);            /* attese reali ammesse */
    gmb_gpm_sleep(5);
    printf("%s\n", g_fails ? "KO" : "OK");

    /* ── 2. Park registrato, ma fuori da un worker: gmb_gpm_sleep delega all'hook ────── */
    printf("guardia §0ter: il park registrato è usato... ");
    {
        int before = g_fails;
        g_park_calls = 0;
        gmb_gpm_set_on_worker_query(fake_on_worker);   /* risponde 0: main/lineare */
        gmb_gpm_set_park_hook(fake_park);
        CHECK(gmb_gpm_park_available() == 1, "park disponibile");
        gmb_gpm_sleep(4);
        gmb_gpm_sleep(0);                              /* ms<=0: no-op, nessun park */
        CHECK(g_park_calls == 1, "gmb_gpm_sleep delega all'hook (una volta)");
        gmb_sleep_ms(4);                               /* fuori da un task: permesso */
        gmb_gpm_set_park_hook(NULL);                   /* i figli dichiarano il proprio stato */
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

#if !defined(_WIN32)
    /* ── 3. Worker + park: gmb_sleep_ms dentro un task è una REGRESSIONE ──────────────── */
    printf("guardia §0ter: gmb_sleep_ms in un task ferma il processo... ");
    {
        int before = g_fails;
        expect_guard_stops(CH_SLEEP_IN_TASK, "gmb_sleep_ms su un worker: exit 2",
                           "gmb_sleep_ms dentro un task");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 4. Stessa attesa con la strategia "confine del syscall" (VM/C): la guardia vede anche
     * il park dichiarato a livello di syscall, non solo l'hook di park. La guardia non è più un
     * simbolo del runtime (vive dentro gmb_sleep_ms): quello che si prova è il SUO effetto. */
    printf("guardia §0ter: confine del syscall (VM/C) ferma il processo... ");
    {
        int before = g_fails;
        expect_guard_stops(CH_SYSCALL_BOUNDARY, "gmb_sleep_ms con park a confine di syscall: exit 2",
                           "gmb_sleep_ms dentro un task");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 5. Worker + park: l'attesa cooperativa NON deve fermare nulla (falso positivo) ─ */
    printf("guardia §0ter: gmb_gpm_sleep su un worker parcheggia e prosegue... ");
    {
        int before = g_fails;
        child_t c;
        if (spawn_child(CH_PARK, &c) != 0) { printf("FAIL: fork/pipe falliti\n"); g_fails++; }
        else
        {
            CHECK(c.code == 0, "worker con park: exit 0");
            if (c.code != 0) printf("  (exit %d) stderr: %s\n", c.code, c.err);
        }
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 6. Worker senza strategia dichiarata: attesa reale permessa, nessun falso positivo ─ */
    printf("guardia §0ter: backend senza park (VM/C) resta permesso... ");
    {
        int before = g_fails;
        child_t c;
        if (spawn_child(CH_NO_PARK, &c) != 0) { printf("FAIL: fork/pipe falliti\n"); g_fails++; }
        else
        {
            CHECK(c.code == 0, "worker senza park: exit 0");
            if (c.code != 0) printf("  (exit %d) stderr: %s\n", c.code, c.err);
        }
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }
#else
    printf("guardia §0ter: casi di processo (exit 2) SKIP: nessun fork su Windows\n");
#endif

    /* Igiene: le query finte non devono restare attaccate al runtime. */
    gmb_gpm_set_on_worker_query(NULL);
    gmb_gpm_set_park_hook(NULL);

    if (g_fails) { printf("FALLITI: %d\n", g_fails); return 1; }
    printf("TUTTO OK\n");
    return 0;
}
