/* gmb_cli_args.c — opzioni "extra" della CLI C: bundle (--libs/--plugins/--skills/
 * --brains), define del preprocessore (-D/--define) e --target.
 *
 * PERCHÉ UN FILE: sono opzioni che devono avere la STESSA superficie di `gmbcs` (contratto
 * unico C/C#); tenerle qui permette di confrontarle in un punto solo e mantiene `gmb_cli.c`
 * sotto la soglia di 1200 righe (AGENTS §0quater).
 */
#include "gmb_cli_internal.h"

#include <stdio.h>
#include <string.h>

/* `--nome` (selettore NULL) o `--nome=valore`: 1 se `a` è questa opzione. */
static int opt_sel(const char* a, const char* name, const char** sel)
{
    size_t n = strlen(name);
    if (strcmp(a, name) == 0) { *sel = NULL; return 1; }
    if (!strncmp(a, name, n) && a[n] == '=') { *sel = a + n + 1; return 1; }
    return 0;
}

/* Riconosce UNA opzione extra. 1 = consumata (o errore registrato), 0 = non è mia.
 * `*i` avanza quando il valore è l'argomento successivo. */
int gmb_parse_extra_arg(Options* o, const char* a, int argc, char** argv, int* i)
{
    const char* sel = NULL;

    /* ── bundle ── */
    if (opt_sel(a, "--libs", &sel) || opt_sel(a, "-libs", &sel)) { o->libs = 1; o->libs_sel = sel; return 1; }
    if (!strcmp(a, "--alllibs") || !strcmp(a, "-alllib") || !strcmp(a, "--all-libs")
        || !strcmp(a, "--full") || !strcmp(a, "-full")) { o->libs = 1; o->libs_sel = NULL; return 1; }
    if (opt_sel(a, "--plugins", &sel) || opt_sel(a, "-plugins", &sel)) { o->plugins = 1; o->plugins_sel = sel; return 1; }
    if (opt_sel(a, "--skills", &sel) || opt_sel(a, "-skills", &sel)) { o->skills = 1; o->skills_sel = sel; return 1; }
    if (opt_sel(a, "--brains", &sel) || opt_sel(a, "-brains", &sel)) { o->brains = 1; o->brains_sel = sel; return 1; }

    /* ── define del preprocessore BASIC (4 forme, come gmbcs) ── */
    {
        const char* spec = NULL;
        if (!strcmp(a, "-D") || !strcmp(a, "--define")) { if (*i + 1 < argc) spec = argv[++(*i)]; }
        else if (!strncmp(a, "-D", 2) && a[2]) spec = a + 2;
        else if (!strncmp(a, "--define=", 9)) spec = a + 9;
        if (spec) {
            if (o->ndefines < (int)(sizeof o->defines / sizeof o->defines[0]))
                snprintf(o->defines[o->ndefines++], sizeof o->defines[0], "%s", spec);
            return 1;
        }
    }

    /* ── target: il C produce solo nativo/C99; il resto è dell'engine C# ── */
    {
        const char* tv = NULL;
        if (!strcmp(a, "--target") && *i + 1 < argc) tv = argv[++(*i)];
        else if (!strncmp(a, "--target=", 9)) tv = a + 9;
        if (tv) { o->target = tv; return 1; }
    }
    return 0;
}
