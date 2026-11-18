/* gmb_cli_engine.c — selezione engine del CLI `gmb` (C) e inoltro a `gmbcs` (C#).
 *
 * Contratto UNICO: `gmb <comando> ... --engine c|csharp`. Con `csharp` lo STESSO argv
 * (tolti --engine/--csharp-cli) viene eseguito da `gmbcs`, che accetta gli stessi
 * parametri (--project/--entry/--out/...). Nessuna traduzione, nessuna logica C#-only.
 */
#include "gmb_cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

/* ── ENGINE: `gmb --engine c|csharp` mantiene UNA superficie CLI ──────────────────────────
 * Il front è questo CLI C. Con `--engine=csharp` lo STESSO argv (tolti --engine e
 * --csharp-cli) viene eseguito da `gmbcs`, che accetta lo stesso contratto
 * (--project/--entry/--out/...). Nessuna traduzione di parametri. */
int gmb_cli_find_gmbcs(const char* explicit_path, char* out, size_t cap)
{
    /* `out` è un OUT: a ogni tentativo fallito resta dentro l'ultimo path composto
     * (`<dir>/gmbcs`), e chi testa `out[0]` per sapere se gmbcs c'è lo scambia per trovato
     * (bug visto: `gmb doctor` riportava `[ok] /sbin/gmbcs`, che non esiste). Si azzera qui. */
    if (out && cap) out[0] = '\0';
    if (explicit_path && *explicit_path) {
        if (access(explicit_path, X_OK) == 0) { snprintf(out, cap, "%s", explicit_path); return 1; }
        return 0;
    }
    /* PATH (gmbcs installato) */
    const char* pe = getenv("PATH");
    if (pe) {
        char tmp[4096];
        snprintf(tmp, sizeof tmp, "%s", pe);
        for (char* d = strtok(tmp, ":"); d; d = strtok(NULL, ":")) {
            snprintf(out, cap, "%s/gmbcs", d);
            if (access(out, X_OK) == 0) return 1;
        }
    }
    /* checkout di sviluppo: <repo>/GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs */
    const char* rel[] = {
        "GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs",
        "../GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs",
        "../../GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs",
        "../../../GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs",
        "../../../../GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs",
        NULL
    };
    for (int i = 0; rel[i]; i++)
        if (access(rel[i], X_OK) == 0) { snprintf(out, cap, "%s", rel[i]); return 1; }
    char exe[4096], dir[4096];
    ssize_t n = -1;
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof exe;
        if (_NSGetExecutablePath(exe, &sz) == 0) n = (ssize_t)strlen(exe);
    }
#else
    n = readlink("/proc/self/exe", exe, sizeof exe - 1);
#endif
    if (n > 0) {
        exe[n] = 0;
        char* sl = strrchr(exe, '/');
        if (sl) { *sl = 0; snprintf(dir, sizeof dir, "%s", exe); }
        else dir[0] = 0;
    } else {
        snprintf(dir, sizeof dir, "%s", ".");
    }
    char cand[4096];
    snprintf(cand, sizeof cand, "%s/../../../../GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs", dir);
    if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
    snprintf(cand, sizeof cand, "%s/../../../GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs", dir);
    if (access(cand, X_OK) == 0) { snprintf(out, cap, "%s", cand); return 1; }
    return 0;
}


/* Ritorna -1 = proseguire con l'engine C; altrimenti il codice di uscita.
 * Con csharp esegue gmbcs (exec: non ritorna, salvo errore). */
int gmb_cli_engine_maybe(int argc, char** argv)
{
    /* --engine: selezione del compilatore con la STESSA superficie. */
    {
        const char* eng = NULL;
        const char* cli = NULL;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--engine") && i + 1 < argc) eng = argv[++i];
            else if (!strncmp(argv[i], "--engine=", 9)) eng = argv[i] + 9;
            else if (!strcmp(argv[i], "--csharp-cli") && i + 1 < argc) cli = argv[++i];
            else if (!strncmp(argv[i], "--csharp-cli=", 13)) cli = argv[i] + 13;
        }
        if (eng && (strcmp(eng, "csharp") == 0 || strcmp(eng, "cs") == 0)) {
            char path[4096];
            if (!gmb_cli_find_gmbcs(cli, path, sizeof path)) {
                fprintf(stderr, "?gmb: engine C# (gmbcs) non trovato. Compila GMBasicSharp/CompilerCli "
                                "oppure passa --csharp-cli <path>.\n");
                return 2;
            }
            /* argv senza --engine <v> e --csharp-cli <v> (stessi parametri, nessuna traduzione) */
            char** na = (char**)calloc((size_t)argc + 2, sizeof(char*));
            if (!na) return 2;
            int k = 0;
            na[k++] = path;
            for (int i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "--engine") && i + 1 < argc) { i++; continue; }
                if (!strncmp(argv[i], "--engine=", 9)) continue;
                if (!strcmp(argv[i], "--csharp-cli") && i + 1 < argc) { i++; continue; }
                if (!strncmp(argv[i], "--csharp-cli=", 13)) continue;
                na[k++] = argv[i];
            }
            na[k] = NULL;
            if (strchr(path, '/')) execv(path, na);
            else execvp(path, na);
            perror("?gmb: exec gmbcs");
            return 2;
        }
        if (eng && strcmp(eng, "c") != 0) {
            fprintf(stderr, "?gmb: --engine sconosciuto '%s' (c|csharp)\n", eng);
            return 2;
        }
    }
    return -1;
}
