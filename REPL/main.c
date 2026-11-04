/* native/repl/main.c — speculare a Program.cs: entry point + USAGE.
 * ReplNative (C): REPL del Native Executor (vmc|c99), mai IL.
 */
#include "repl.h"
#include "gmb_iconsole.h"
#include <unistd.h>

static const char* USAGE =
    "gmbrepl (REPL unica, C) — esegue vmc|c99; engine C# opzionale per IL/P-Code\n"
    "USO: gmbrepl [<file.bas>] [opzioni]\n"
    "Backend C: vmc (chunk -> gmb_vm_host) | c99 (C99 -> cc)\n"
    "Opzioni:\n"
    "  -noconsole  -c/-console (no SDL)  -s (system console)  -c64  -w <w> -h <h>\n"
    "  -size W,H  -font nome,size  -e/-executor (verbose)\n"
    "  --ast (stampa l'AST del file)  -repl (dopo il file resta nella REPL)\n"
    "  --pcode  --no-optimize  --dump-il (dump IL con --engine=csharp)\n"
    "  -bsp <dir> (precompila pagine BSP)  -gpm <auto|always|linear>\n"
    "  --debug-tcp <port> (avviso: non implementato nel mondo C)\n"
    "  --run <cmd>  -D<name>[=val]\n"
    "  -n/--native, -m/--managed → engine C# (richiede --engine=csharp)\n"
    "  -full  → distribuzione: engine C# = runtime .NET, engine C = --libs (lib accanto all'exe, sottocartelle)\n"
    "  --il-module <dll>                  → solo con --engine=csharp\n"
    "  --engine <c|csharp>  --csharp-cli <path>  (default: c; csharp = gmbcs)\n"
    "Comandi REPL: QUIT EXIT CLS PROMPT HELP LIST EDIT NEW AUTO SAVE LOAD DELETE DEL RENUM\n"
    "  SAMPLE COMPILE BSP RUN EXEC LOADPRG PRGINFO AST DASM IL PCODE GPM LS CD WHERE WH\n"
    "  GREP (RG) RN RENAME RM DL DELF CP COPY DP DUPLICATE BL BACKUP DEPLOY CONVERT BATCH CONVFMT\n"
    "  WEB2MD PROVIDERS DEP PACKAGE HF MLX ML AIPATH SET STATUS DEBUG BREAK CLEAR STEP\n"
    "  CONT TRACE WATCH UNWATCH PALETTE BRAIN RAG GRAPH IDE LIBS EVAL DIR\n"
    "  FIXTURES SETUP LLM ADD REMOVE BUMP RESTORE UPDATE ROLLBACK TRUST CACHE AIBENCH\n";


int main(int argc, char** argv)
{
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-help") == 0)) { printf("%s", USAGE); return 0; }
    if (argc >= 2 && strcmp(argv[1], "--lsp") == 0) {
        fprintf(stderr, "?-lsp: il language server e' il tool separato 'gmblsp' (native/lsp).\n"
                        "      Avvialo come processo dal tuo editor (IDE GMBasic, VS Code).\n");
        return 2;
    }
    rpl_options_t o;
    if (rpl_parse_args(argc, argv, &o) != 0) { printf("%s", USAGE); return 2; }
    /* Flag del mondo C#: -n/-m richiedono il build dell'engine (gmbcs); con --engine=c
     * non hanno senso. --il-module è accettato SOLO con --engine=csharp (passa a gmbcs). */
    int has_il_module = 0;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--il-module")) has_il_module = 1;
    if (has_il_module && !rpl_engine_cs(&o)) {
        fprintf(stderr, "?--il-module richiede --engine=csharp (moduli IL esterni sono del mondo C#).\n");
        return 2;
    }
    /* -n/-m sono build dell'engine C#: con --engine=csharp vengono inoltrati a gmbcs
     * (run --engine native|il). Nel mondo C il nativo canonico è `gmb build`.
     * `-full` invece vale per ENTRAMBI: con C# è la distribuzione .NET, con l'engine C è
     * `--alllibs` (copia accanto all'eseguibile tutte le librerie usate). */
    if ((o.native_backend || o.managed_backend) && !rpl_engine_cs(&o)) {
        fprintf(stderr, "?-n/--native e -m/--managed sono dell'engine C#: aggiungi --engine=csharp\n"
                        "  (nel mondo C il nativo canonico è `gmb build`).\n");
        return 2;
    }
    /* -size/-font sovrascrivono i settings: passati alla console via env. */
    if (o.grid_width > 0 && o.grid_height > 0) { char _b[64]; snprintf(_b, sizeof(_b), "%d,%d", o.grid_width, o.grid_height); setenv("GMB_CONSOLE_SIZE", _b, 1); }
    if (o.font_name || o.font_size > 0) { char _b[192]; snprintf(_b, sizeof(_b), "%s,%d", o.font_name ? o.font_name : "", o.font_size); setenv("GMB_CONSOLE_FONT", _b, 1); }
    gmb_iconsole_boot();   /* startup console: replica Executor.Startup.cs (GMB_CONSOLE/GMBASIC_HEADLESS) */

    /* -bsp <dir>: precompila le pagine BSP all'avvio (come il vecchio runner -bsp). */
    if (o.compile_bsp_dir && *o.compile_bsp_dir) {
        int rc = rpl_bsp_compile_dir(o.compile_bsp_dir, &o);
        if (!o.file_name) return rc;
    }
    /* Flag che il mondo C non può onorare: messaggio ESPLICITO (mai silenzio). */
    if (o.debug_port > 0)
        fprintf(stderr, "?--debug-tcp: il debug TCP per IDE esterni non è implementato nel mondo C;\n"
                        "  usa l'IDE integrato (comando IDE) o `gmbcs debug <file>` (engine C#).\n");
    if (o.no_optimize && !rpl_engine_cs(&o))
        fprintf(stderr, "?--no-optimize: il compilatore C non ha un ottimizzatore (flag senza effetto);\n"
                        "  per l'engine C# l'ottimizzazione IL è controllata da `gmbcs`.\n");

    if (o.file_name) {
        int rc = 0;
        if (o.ast) {   /* --ast: stampa l'AST e (senza -repl) esce, come il vecchio runner */
            if (rpl_engine_cs(&o)) rc = rpl_cs_cmd("dump ast", o.file_name, NULL, &o);
            else rc = rpl_gmb_dump("ast", o.file_name, NULL);
            if (!o.stay_in_repl) return rc;
        } else {
            if (rpl_engine_cs(&o)) rc = rpl_run_cs(o.file_name, &o);   /* engine C#: gmbcs run/debug */
            else if (o.backend == RPL_VMC) rc = rpl_run_vmc(o.file_name, &o);
            else rc = rpl_run_c99(o.file_name, &o);
            if (!o.stay_in_repl) return rc;
        }
    }
    /* REPL interattivo */
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    return rpl_loop(&o, cwd);
}
