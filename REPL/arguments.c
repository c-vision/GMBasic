/* native/repl/arguments.c — speculare a Arguments.cs (REPL): parsing CLI completo.
 * Tutti i flag di Arguments.cs; i flag IL (-n/-m, --il-module) sono VIETATI qui
 * (ReplNative esegue solo vmc|c99).
 */
#include "repl.h"
#include <ctype.h>

static int flag_present(const char** args, int n, const char* flag, const char* alias)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(args[i], flag) == 0) return 1;
        if (alias && strcmp(args[i], alias) == 0) return 1;
    }
    return 0;
}

static const char* flag_value(const char** args, int n, const char* flag)
{
    for (int i = 0; i + 1 < n; i++)
        if (strcmp(args[i], flag) == 0) return args[i + 1];
    return NULL;
}

int rpl_parse_args(int argc, char** argv, rpl_options_t* o)
{
    memset(o, 0, sizeof(*o));
    o->backend = RPL_VMC;          /* default: vmc (native) */
    o->use_sdl = true;
    o->gpm_mode = "Auto";   /* default EFFETTIVO: rilevamento (Always solo su richiesta) */
    o->grid_width = o->grid_height = 0;
    o->debug_port = -1;

    const char** a = (const char**)(argv + 1);
    int n = argc - 1;
    const char* backend_flag = flag_value(a, n, "--backend");
    if (!backend_flag) {
        /* --backend=<v> */
        for (int i = 0; i < n; i++)
            if (strncmp(a[i], "--backend=", 10) == 0) backend_flag = a[i] + 10;
    }
    if (backend_flag) {
        if (strncmp(backend_flag, "vm", 2) == 0) o->backend = RPL_VMC;
        else if (strcmp(backend_flag, "c99") == 0) o->backend = RPL_C99;
        else { fprintf(stderr, "?--backend sconosciuto: %s (vmc|c99)\n", backend_flag); return 1; }
    }

    /* --engine c|csharp (anche --engine=…): sceglie il compilatore pilotato. */
    const char* eng = flag_value(a, n, "--engine");
    if (!eng)
        for (int i = 0; i < n; i++)
            if (strncmp(a[i], "--engine=", 9) == 0) eng = a[i] + 9;
    if (eng) {
        if (strcmp(eng, "c") == 0) o->engine = "c";
        else if (strcmp(eng, "csharp") == 0 || strcmp(eng, "cs") == 0) o->engine = "csharp";
        else { fprintf(stderr, "?--engine sconosciuto: %s (c|csharp)\n", eng); return 1; }
    } else o->engine = "c";
    o->csharp_cli = flag_value(a, n, "--csharp-cli");

    o->no_console = flag_present(a, n, "-noconsole", NULL);
    if (flag_present(a, n, "-c", "-console")) o->use_sdl = false;
    o->use_system_console = flag_present(a, n, "-s", NULL);
    o->use_c64_mode = flag_present(a, n, "-c64", NULL);
    o->verbose = flag_present(a, n, "-e", "-executor");
    o->no_optimize = flag_present(a, n, "--no-optimize", NULL);
    o->dump_il = flag_present(a, n, "--dump-il", NULL);
    o->ast = flag_present(a, n, "--ast", NULL);
    o->stay_in_repl = flag_present(a, n, "-repl", NULL);
    o->pcode = flag_present(a, n, "--pcode", "-pcode");
    /* --il-module <dll> (ripetibile): moduli IL esterni, solo con engine C#. */
    o->il_modules_count = 0;
    for (int i = 0; i < n && o->il_modules_count < 8; i++)
        if (!strcmp(a[i], "--il-module") && i + 1 < n) o->il_modules[o->il_modules_count++] = a[i + 1];
    o->compile_bsp_dir = flag_value(a, n, "-bsp");
    const char* gpm = flag_value(a, n, "-gpm");
    /* -gpm <mode> (o -gpm:<mode>): niente variabili d'ambiente per il comportamento. */
    if (gpm) o->gpm_mode = gpm;
    else {
        for (int i = 0; i < n; i++)
            if (strncmp(a[i], "-gpm:", 5) == 0) { o->gpm_mode = a[i] + 5; break; }
    }
    const char* w = flag_value(a, n, "-w"); if (w) o->grid_width = atoi(w);
    const char* h = flag_value(a, n, "-h"); if (h) o->grid_height = atoi(h);
    /* -size W,H (grid celle) e -font name,size: sovrascrivono i settings (parità C#). */
    const char* sz = flag_value(a, n, "-size");
    if (sz) { int sw = 0, sh = 0; if (sscanf(sz, "%d,%d", &sw, &sh) == 2) { o->grid_width = sw; o->grid_height = sh; } }
    const char* fo = flag_value(a, n, "-font");
    if (fo) { char nm[128] = {0}; int fs = 0;
        if (sscanf(fo, "%127[^,],%d", nm, &fs) >= 1) { if (nm[0]) o->font_name = strdup(nm); if (fs > 0) o->font_size = fs; } }
    const char* dp = flag_value(a, n, "--debug-tcp"); if (dp) o->debug_port = atoi(dp);
    o->run_command = flag_value(a, n, "--run");

    /* -D / env GMBASIC_DEFINES */
    const char* envd = getenv("GMBASIC_DEFINES");
    char envbuf[2048];
    if (envd) {
        snprintf(envbuf, sizeof(envbuf), "%s", envd);
        char* tok = strtok(envbuf, ";");
        while (tok && o->defines_count < 64) { o->defines[o->defines_count++] = strdup(tok); tok = strtok(NULL, ";"); }
    }
    for (int i = 0; i < n; i++)
        if (strncmp(a[i], "-D", 2) == 0 && strlen(a[i]) > 2 && o->defines_count < 64)
            o->defines[o->defines_count++] = a[i] + 2;

    /* Selezione console d'avvio (parità Arguments.cs::Parse): i flag legacy -c/-s/-c64
     * mappano sul runtime C via GMB_CONSOLE (0=%SDL default, 1=%C64, 2=%ANSI, 3=%SYSTEM).
     * Settato solo se un flag è presente, così non sovrascrive un GMB_CONSOLE impostato a mano. */
    /* Precedenza come Arguments.cs/InitConsole: -s (System) vince su -c (Standard);
     * -c64 seleziona C64; senza flag resta il default SDL (GMB_CONSOLE non toccato). */
    if (o->use_c64_mode) setenv("GMB_CONSOLE", "1", 1);
    else if (o->use_system_console) setenv("GMB_CONSOLE", "3", 1);
    else if (!o->use_sdl) setenv("GMB_CONSOLE", "2", 1);
    /* -noconsole: esecuzione SENZA console grafica (headless): GMBASIC_HEADLESS=1 salta
     * SDL/video e usa la HeadlessConsole (stdout lineare). */
    if (o->no_console) setenv("GMBASIC_HEADLESS", "1", 1);

    /* file = primo arg non-flag, saltando flag che consumano un valore */
    static const char* value_flags[] = {"--backend", "-bsp", "-gpm", "-w", "-h", "-size", "-font", "--debug-tcp", "--run", "--il-module", "--engine", "--csharp-cli", NULL};
    for (int i = 0; i < n; i++) {
        int consumed = 0;
        if (a[i][0] == '-') {
            for (int f = 0; value_flags[f]; f++)
                if (strcmp(a[i], value_flags[f]) == 0) { i++; consumed = 1; break; }
            if (consumed) continue;
            continue; /* altro flag: salta */
        }
        o->file_name = a[i];
        break;
    }
    return 0;
}

/* flag IL vietati: -n/-m/--il-module appartengono a REPL (C#). */
int rpl_il_forbidden(int argc, char** argv)
{
    const char** a = (const char**)(argv + 1);
    int n = argc - 1;
    return flag_present(a, n, "-n", "--native") ||
           flag_present(a, n, "-m", "--managed") ||
           flag_value(a, n, "--il-module") != NULL;
}
