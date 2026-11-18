/* gmb_first_use_spawn.c — PRIMO USO dei due compilatori (`gmb`, `gmbnative`) e della REPL.
 *
 * È un file di dominio del runtime ma NON entra in `gmb_runtime`/`gmb_boot`: viene compilato
 * dentro ciascun eseguibile che ne ha bisogno (gmb, gmbnative, gmbrepl), così nessuno di loro
 * deve linkare la libreria di boot o libcurl (nessuna dipendenza nuova).
 *
 * PERCHÉ DELEGA. La logica di "procura quello che manca" (librerie per RID + toolchain zig)
 * vive UNA sola volta, nel runtime C (`gmb_first_use.c`), ed è raggiungibile con
 * `gmbadmin first-use`. Qui non si duplica niente: si cerca l'eseguibile `gmbadmin` (accanto
 * al binario, come già fa il forwarding dei comandi admin) e lo si esegue. Così `gmb` e
 * `gmbnative` e `gmbrepl` NON devono linkare il runtime di boot né libcurl: nessun cambio di
 * dipendenze.
 *
 * COSTO A CACHE COMPLETA: una `stat` su `~/.gmbasic/.first-use-<rid>` e nient'altro — nessun
 * processo, nessuna rete. Con il marker presente il compilatore parte come sempre.
 *
 * QUESTA FUNZIONE NON BLOCCA MAI: qualunque problema (admin assente, rete giù, estrazione
 * fallita) produce un avviso su stderr e il compilatore prosegue con ciò che ha.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

/* 1 se esiste almeno un marker di primo uso (per QUALUNQUE RID): a cache completa il
 * compilatore non deve nemmeno cercare/avviare gmbadmin. */
static int fu_marker_present(void)
{
    const char* home = getenv("HOME");
    if (!home || !*home) return 0;
    char dir[2048];
    snprintf(dir, sizeof dir, "%s/.gmbasic", home);
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    int found = 0;
    while ((e = readdir(d)) != NULL)
    {
        if (strncmp(e->d_name, ".first-use-", 11) == 0) { found = 1; break; }
    }
    closedir(d);
    return found;
}

/* Percorso di `gmbadmin`: accanto all'eseguibile (layout di build e di install), poi nel PATH. */
static int fu_find_admin(char* out, size_t cap)
{
    char exe[4096];
    ssize_t n = -1;
#if defined(__APPLE__)
    extern int _NSGetExecutablePath(char*, unsigned int*);
    unsigned int sz = (unsigned int)sizeof exe;
    if (_NSGetExecutablePath(exe, &sz) == 0) n = (ssize_t)strlen(exe);
#elif defined(__linux__)
    n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) exe[n] = '\0';
#endif
    if (n > 0)
    {
        char* sl = strrchr(exe, '/');
        if (sl)
        {
            *sl = '\0';
            static const char* rel[] = {
                "gmbadmin", "../gmbadmin/gmbadmin", "../gmbadmin/build/gmbadmin",
                "../../gmbadmin/build/gmbadmin", "../../../gmbadmin/build/gmbadmin", NULL
            };
            for (int i = 0; rel[i]; i++)
            {
                char cand[4300];
                snprintf(cand, sizeof cand, "%s/%s", exe, rel[i]);
                if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
            }
        }
    }
    const char* path = getenv("PATH");
    if (path)
    {
        char tmp[4096];
        snprintf(tmp, sizeof tmp, "%s", path);
        for (char* d = strtok(tmp, ":"); d; d = strtok(NULL, ":"))
        {
            char cand[4300];
            snprintf(cand, sizeof cand, "%s/gmbadmin", d);
            if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
        }
    }
    return 0;
}

int gmb_first_use_spawn_if_needed(int argc, char** argv)
{
    if (fu_marker_present()) return 0;                 /* cache già completa: nessun costo */
    for (int i = 1; i < argc; i++)
        if (argv[i] && !strcmp(argv[i], "--no-bootstrap")) return 0;   /* opt-out esplicito */
    char admin[4300];
    if (!fu_find_admin(admin, sizeof admin))
    {
        /* Non è un errore: può essere un'installazione che porta solo il compilatore.
         * Il messaggio dice COME completare, senza bloccare. */
        fprintf(stderr, "[first-use] gmbadmin non trovato: le librerie/toolchain non sono state verificate.\n"
                        "            rimedio: `gmb libs ensure` (oppure installare gmbadmin accanto a gmb).\n");
        return 0;
    }
    char cmd[4600];
    snprintf(cmd, sizeof cmd, "\"%s\" first-use", admin);
    (void)system(cmd);
    return 0;
}
