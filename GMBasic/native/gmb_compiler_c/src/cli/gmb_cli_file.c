/* gmb_cli_file.c — `gmb <file.bas>`: esegue un singolo programma con il compilatore C.
 *
 * Contratto storico del CLI (`gmb primo.bas`): prima eseguiva con l'engine C#, ora con il
 * compilatore C (equivalente a `gmb run --project <cartella> --entry <file>`). Serve anche
 * a chi non ha la REPL: un singolo `.bas` resta eseguibile dal CLI.
 *
 * Se è presente `--engine=csharp`, l'engine C# è già stato scelto da `gmb_cli_engine_maybe`
 * (che gira PRIMA di questo): qui non si interferisce.
 */
#include "gmb_cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <libgen.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static int is_bas_file(const char* p)
{
    if (!p || !*p || p[0] == '-') return 0;
    size_t l = strlen(p);
    if (l < 5 || strcasecmp(p + l - 4, ".bas") != 0) return 0;
    return access(p, R_OK) == 0 ? 1 : 0;
}

/* -1 = non è un file .bas (prosegui); altrimenti exit code. */
int gmb_cli_file_maybe(int argc, char** argv)
{
    if (argc < 2 || !is_bas_file(argv[1])) return -1;

    char path[4096];
    snprintf(path, sizeof path, "%s", argv[1]);
    char* dir = dirname(path);          /* modifica `path` */
    char entry[4096];
    snprintf(entry, sizeof entry, "%s", argv[1]);
    char* base = basename(entry);

    /* auto-riesecuzione: gmb run --project <dir> --entry <file> [altri flag] */
    char** nv = (char**)calloc((size_t)argc + 6, sizeof(char*));
    if (!nv) { fprintf(stderr, "?gmb: memoria esaurita\n"); return 2; }
    int n = 0;
    nv[n++] = argv[0];
    nv[n++] = (char*)"run";
    nv[n++] = (char*)"--project";
    nv[n++] = dir;
    nv[n++] = (char*)"--entry";
    nv[n++] = base;
    for (int i = 2; i < argc; i++) nv[n++] = argv[i];
    nv[n] = NULL;
    /* stesso eseguibile: /proc/self/exe su Linux, _NSGetExecutablePath su macOS */
    char self[4096] = "";
#if defined(__APPLE__)
    {
        uint32_t sz = (uint32_t)sizeof self;
        if (_NSGetExecutablePath(self, &sz) != 0) self[0] = 0;
    }
#else
    {
        ssize_t n2 = readlink("/proc/self/exe", self, sizeof self - 1);
        if (n2 > 0) self[n2] = 0;
    }
#endif
    if (self[0]) execv(self, nv);
    execvp(argv[0], nv);
    perror("?gmb: riesecuzione con `run` fallita");
    return 2;
}
