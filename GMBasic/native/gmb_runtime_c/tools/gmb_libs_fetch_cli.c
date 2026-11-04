/* gmb_libs_fetch_cli.c — CLI del downloader on-demand, usata dal runner C99 PRIMA del link
 * («il compilatore C deve scaricare»). Non sta nella libreria: `main` colliderebbe.
 *
 *   gmblibs <libs.txt> <file> [<file>…]
 *     scarica nella cache (~/.gmbasic/libs/<rid>/) e stampa il path di ogni file procurato.
 */
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include "gmb_runtime.h"

/* `--exec <exe>`: procura TUTTE le librerie elencate in `<exe>.libs.txt` (accanto all'exe) e poi
 * esegue l'eseguibile. È il "launcher" per una macchina senza compilatore e senza cache: le
 * dipendenze a LOAD-TIME non possono essere scaricate dal programma stesso (dyld fallisce prima
 * di main), quindi serve un passo prima dell'exec. Le lib finiscono ACCANTO all'exe, dove
 * l'rpath `@loader_path` le trova. */
static int run_exec(const char* exe)
{
    char idx[2600];
    snprintf(idx, sizeof idx, "%s.libs.txt", exe);
    FILE* f = fopen(idx, "rb");
    if (f) {
        char line[4096];
        while (fgets(line, sizeof line, f)) {
            char* t = strchr(line, '\t');          /* nome<TAB>sha<TAB>size<TAB>url */
            if (!t) continue;
            *t = '\0';
            char name[512];
            snprintf(name, sizeof name, "%s", line);
            char out[2600];
            if (!gmb_libs_ensure_next_to(exe, name, out, sizeof out))
                fprintf(stderr, "[libs] ATTENZIONE: '%s' non disponibile\n", name);
        }
        fclose(f);
    }
#if !defined(_WIN32)
    execl(exe, exe, (char*)NULL);
    perror("[libs] exec fallito");
    return 127;
#else
    return 0;
#endif
}

static void usage(const char* a0) {
    fprintf(stderr,
        "uso: %s <libs.txt> <file> [<file>...]\n"
        "     %s --exec <eseguibile>     # procura le sue lib (accanto all'exe) e lo esegue\n"
        "  formato libs.txt: nome<TAB>sha256<TAB>size<TAB>url\n", a0, a0);
}

int main(int argc, char** argv)
{
    if (argc >= 2 && (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        usage(argv[0]);
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "--exec") == 0) return run_exec(argv[2]);
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    int rc = 0;
    for (int i = 2; i < argc; i++) {
        char out[2600];
        if (gmb_libs_ensure_file_from(argv[1], argv[i], out, sizeof out)) printf("%s\n", out);
        else { fprintf(stderr, "[libs] impossibile procurarsi '%s'\n", argv[i]); rc = 1; }
    }
    return rc;
}
