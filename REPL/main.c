/* native/repl/main.c — speculare a Program.cs: entry point + USAGE.
 * ReplNative (C): REPL del Native Executor (vmc|c99), mai IL.
 */
#include "repl.h"
#include "gmb_iconsole.h"
#include <unistd.h>

static const char* USAGE =
    "gm (ReplNative, C) — Native Executor vmc|c99, mai IL\n"
    "USO: gm <file.bas> [--backend <vmc|c99>] [opzioni]\n"
    "Backend: vmc (chunk->gmb_vm_host) | c99 (C99->cc)\n"
    "Opzioni (da REPL/Arguments.cs):\n"
    "  -noconsole  -c/-console (no SDL)  -s (system console)  -c64  --pcode(vmc)\n"
    "  --c99       --ast  -n/-m (vietati: IL)  -e/-executor (verbose)\n"
    "  --no-optimize  --dump-il  -bsp <dir>  -gpm[:mode]  -w <w> -h <h>\n"
    "  --debug-tcp <port>  --run <cmd>  --il-module <dll> (vietato: IL)  -D<name>[=val]\n"
    "  --engine <c|csharp>  --csharp-cli <path>  (default: c; csharp = gmbcs, opzionale)\n"
    "  env: GMBASIC_DEFINES ; GMBASIC_GPM\n"
    "Comandi REPL: QUIT EXIT CLS PROMPT HELP LIST EDIT NEW AUTO SAVE LOAD DELETE DEL RENUM\n"
    "  SAMPLE COMPILE BSP RUN EXEC LOADPRG PRGINFO AST DASM IL PCODE GPM LS CD WHERE WH\n"
    "  RN RENAME RM DL DELF CP COPY DP DUPLICATE BL BACKUP DEPLOY CONVERT BATCH CONVFMT\n"
    "  WEB2MD PROVIDERS DEP PACKAGE HF MLX ML AIPATH\n";

int main(int argc, char** argv)
{
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) { printf("%s", USAGE); return 0; }
    if (argc >= 2 && strcmp(argv[1], "--lsp") == 0) {
        fprintf(stderr, "?-lsp: il language server e' il tool separato 'gmb_lsp' (native/lsp).\n"
                        "      Avvialo come processo dal tuo editor (IDE GMBasic, VS Code).\n");
        return 2;
    }
    rpl_options_t o;
    if (rpl_parse_args(argc, argv, &o) != 0) { printf("%s", USAGE); return 2; }
    /* IL-only flag vietati */
    if (rpl_il_forbidden(argc, argv)) {
        fprintf(stderr, "?-n/-m/--il-module (IL) appartengono a REPL (C#). ReplNative esegue solo vmc|c99.\n");
        return 2;
    }
    /* -size/-font sovrascrivono i settings: passati alla console via env. */
    if (o.grid_width > 0 && o.grid_height > 0) { char _b[64]; snprintf(_b, sizeof(_b), "%d,%d", o.grid_width, o.grid_height); setenv("GMB_CONSOLE_SIZE", _b, 1); }
    if (o.font_name || o.font_size > 0) { char _b[192]; snprintf(_b, sizeof(_b), "%s,%d", o.font_name ? o.font_name : "", o.font_size); setenv("GMB_CONSOLE_FONT", _b, 1); }
    gmb_iconsole_boot();   /* startup console: replica Executor.Startup.cs (GMB_CONSOLE/GMBASIC_HEADLESS) */
    if (o.file_name) {
        if (rpl_engine_cs(&o)) return rpl_run_cs(o.file_name, &o);   /* engine C#: gmbcs run/debug */
        if (o.backend == RPL_VMC) return rpl_run_vmc(o.file_name, &o);
        return rpl_run_c99(o.file_name, &o);
    }
    /* REPL interattivo */
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    return rpl_loop(&o, cwd);
}
