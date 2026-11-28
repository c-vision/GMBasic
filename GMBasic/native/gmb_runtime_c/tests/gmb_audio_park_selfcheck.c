/* gmb_audio_park_selfcheck.c — §0ter: SOUND/MML asincroni + guardia "parcheggia al syscall".
 *
 * Perché esiste. La VM/C non ha una coroutine per il frame C di un syscall: un'attesa reale
 * dentro una syscall dormirebbe il worker GPM e nessuna gamba di test lo vedrebbe. La
 * soluzione è doppia e qui si prova tutta:
 *   1. l'audio NON attende: gmb_audio_sound_async / gmb_audio_mml_async avviano la
 *      riproduzione sul thread audio (che non esegue BASIC) e ritornano la DURATA da
 *      attendere, così il chiamante parcheggia il task con un timer;
 *   2. la guardia vale anche qui: dichiarato `gmb_gpm_set_park_at_syscall(1)`, ogni
 *      gmb_sleep_ms su un worker è fatale (exit 2) — il park non si aggira.
 *
 * La query "sono su un worker?" è THREAD-AWARE (risponde 1 solo sul thread di prova): il
 * sequencer MML è un thread audio e lì le attese reali sono legittime — se la guardia lo
 * colpisse, questo stesso test morirebbe nella parte 3.
 *
 * Eseguito da: ctest -R gmb_audio_park_selfcheck (build di gmb_runtime_c)
 */
#include "gmb_runtime.h"
#include "gmb_audio.h"

#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int g_fails = 0;

static void check(int cond, const char* what, int line)
{
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}

#define CHECK(c, w) check((c) != 0, (w), __LINE__)

#if !defined(_WIN32)
static pthread_t g_test_thread;
static volatile int g_on_worker = 0;

/* Il "backend" di questa prova: sono su un worker solo se lo dico E sono il thread di prova.
 * Il sequencer MML e ogni altro thread restano fuori (là l'attesa reale è legittima). */
static int fake_on_worker(void)
{
    if (!g_on_worker) return 0;
    return pthread_equal(pthread_self(), g_test_thread) ? 1 : 0;
}

/* ── processi figli: i casi in cui la guardia DEVE fermare il processo ── */
enum { CH_SLEEP_MS = 1, CH_GPM_SLEEP = 2 };

static int child_body(int mode)
{
    gmb_gpm_set_on_worker_query(fake_on_worker);
    gmb_gpm_set_park_at_syscall(1);
    g_on_worker = 1;
    switch (mode)
    {
    case CH_SLEEP_MS:   /* REGRESSIONE: attesa reale su un worker che parcheggia al syscall */
        gmb_sleep_ms(10);
        return 1;
    case CH_GPM_SLEEP:  /* gmb_gpm_sleep senza park (VM/C) ⇒ attesa reale ⇒ errore */
        gmb_gpm_sleep(10);
        return 1;
    }
    return 1;
}

typedef struct { int code; char err[1024]; } child_t;

/* Il padre DEVE leggere lo stderr del figlio: chiudendo la pipe senza leggere, il figlio
 * muore di SIGPIPE al primo fprintf del messaggio FATAL (prova falsa, esito a caso). */
static int spawn_child(int mode, child_t* out)
{
    int p[2];
    pid_t pid;
    ssize_t n;
    int st = 0;
    out->code = -1;
    out->err[0] = 0;
    if (pipe(p) != 0) return -1;
    fflush(stdout);
    pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); return -1; }
    if (pid == 0)
    {
        close(p[0]);
        dup2(p[1], 2);
        close(p[1]);
        fflush(stderr);
        _Exit(child_body(mode));
    }
    close(p[1]);
    n = read(p[0], out->err, sizeof(out->err) - 1);
    if (n > 0) out->err[n] = 0;
    close(p[0]);
    if (waitpid(pid, &st, 0) < 0) return -1;
    if (WIFSIGNALED(st)) printf("  (figlio ucciso dal segnale %d)\n", WTERMSIG(st));
    out->code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return 0;
}

static void expect_guard_stops(int mode, const char* what)
{
    child_t c;
    if (spawn_child(mode, &c) != 0) { printf("FAIL: fork/pipe falliti su '%s'\n", what); g_fails++; return; }
    CHECK(c.code == 2, what);
    CHECK(strstr(c.err, "FATAL") != NULL && strstr(c.err, "(§0ter)") != NULL,
          "il messaggio dichiara FATAL (§0ter)");
    CHECK(strstr(c.err, "gmb_sleep_ms dentro un task") != NULL, "il messaggio nomina gmb_sleep_ms");
    if (c.code != 2) printf("  (exit osservato: %d) stderr: %s\n", c.code, c.err);
}
#endif

int main(void)
{
#if !defined(_WIN32)
    g_test_thread = pthread_self();   /* la query risponde 1 solo su questo thread */
#endif

    /* ── 1. Nessuna capacità dichiarata: la guardia è un no-op ───────────────────────────
     * Oracoli e runner senza scheduler non devono accorgersene. */
    printf("audio §0ter: senza query la guardia è un no-op... ");
    gmb_gpm_set_on_worker_query(NULL);
    gmb_gpm_set_park_at_syscall(0);
    CHECK(gmb_gpm_park_at_syscall() == 0, "park_at_syscall spento");
    gmb_sleep_ms(3);
    printf("%s\n", g_fails ? "KO" : "OK");

    /* I casi di processo vengono PRIMA di ogni audio: fork() in un processo con il thread
     * audio di SDL e il sequencer già vivi non è affidabile (il figlio può morire su un lock
     * ereditato), e la prova diventerebbe falsa invece che rossa per il motivo giusto. */
#if !defined(_WIN32)
    {
        /* ── 2. Worker che parcheggia al syscall: gmb_sleep_ms è una REGRESSIONE ──────── */
        printf("audio §0ter: gmb_sleep_ms in un task VM/C ferma il processo... ");
        int before = g_fails;
        expect_guard_stops(CH_SLEEP_MS, "gmb_sleep_ms con park_at_syscall: exit 2");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    {
        /* ── 3. gmb_gpm_sleep senza park: sulla VM/C non è utilizzabile (nessun hook) ──── */
        printf("audio §0ter: gmb_gpm_sleep su un task VM/C ferma il processo... ");
        int before = g_fails;
        expect_guard_stops(CH_GPM_SLEEP, "gmb_gpm_sleep senza park: exit 2");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }
#else
    printf("audio §0ter: casi di processo (exit 2) SKIP: nessun fork su Windows\n");
#endif

    {
        /* ── 4. SOUND asincrono: NON attende sul worker, ritorna la durata ──────────────── */
        printf("audio §0ter: SOUND asincrono non attende sul worker... ");
        int before = g_fails;
#if !defined(_WIN32)
        gmb_gpm_set_on_worker_query(fake_on_worker);
        gmb_gpm_set_park_at_syscall(1);
        g_on_worker = 1;
#endif
        /* Scalda il mixer (dlopen + Mix_OpenAudio costano ~200 ms la prima volta): quella è
         * inizializzazione, non un'attesa del worker, e falserebbe la misura. */
        gmb_audio_sound_async(440, 0.01, 10);
        int64_t t0 = gmb_ticks_ms();
        int ms = gmb_audio_sound_async(440, 0.40, 50);
        int64_t elapsed = gmb_ticks_ms() - t0;
        CHECK(ms == 400, "SOUND asincrono ritorna la durata in ms (400)");
        CHECK(elapsed < 250, "SOUND asincrono ritorna subito: non attende i 400 ms del tono");
        if (elapsed >= 250) printf("  (atteso <250 ms, misurato %lld ms) ", (long long)elapsed);
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    {
        /* ── 5. MML asincrono: ritorna subito la durata TOTALE; il sequencer (thread audio)
         *       suona con attese reali e NON fa scattare la guardia ─────────────────────── */
        printf("audio §0ter: PLAY asincrono ritorna la durata senza attese... ");
        int before = g_fails;
        /* T240 L4: 240/(240×4) = 0,25 s per nota × 4 note = 1000 ms (il parser clampa il
         * tempo a 32..255: T400 diventerebbe 255, non 400). */
        int64_t t0 = gmb_ticks_ms();
        int ms = gmb_audio_mml_async("T240 L4 C D E F");
        int64_t elapsed = gmb_ticks_ms() - t0;
        CHECK(ms >= 970 && ms <= 1030, "PLAY asincrono ritorna la durata totale (~1000 ms)");
        if (ms < 970 || ms > 1030) printf("  (durata misurata: %d ms) ", ms);
        CHECK(elapsed < 250, "PLAY asincrono ritorna subito (la sequenza suona sul thread audio)");
        if (elapsed >= 250) printf("  (atteso <250 ms, misurato %lld ms) ", (long long)elapsed);
        CHECK(gmb_audio_mml_async("") == 0, "PLAY asincrono di una stringa vuota: 0");
        CHECK(gmb_audio_mml_async(NULL) == 0, "PLAY asincrono di NULL: 0");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* Igiene: nessuna capacità finta deve restare attaccata al runtime. */
    gmb_gpm_set_on_worker_query(NULL);
    gmb_gpm_set_park_at_syscall(0);

    if (g_fails) { printf("FALLITI: %d\n", g_fails); return 1; }
    printf("TUTTO OK\n");
    return 0;
}
