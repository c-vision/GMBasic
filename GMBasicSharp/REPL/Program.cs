using System.Diagnostics;

namespace Basic.Executor;

public static class Program
{
    public static void Main(string[] args)
    {
        Environment.SetEnvironmentVariable("COREHOST_TRACE", "0");
        Environment.SetEnvironmentVariable("DOTNET_AssemblyLoadDebug", "0");
        Environment.SetEnvironmentVariable("DOTNET_Debug", "0");

        if (Debugger.IsAttached)
        {
            var defaultListener = Trace.Listeners.OfType<DefaultTraceListener>().FirstOrDefault();
            if (defaultListener != null)
                Trace.Listeners.Remove(defaultListener);
        }

        // SVILUPPO: nel checkout le librerie appena compilate (`GMBasic/build/libs/<rid>/`) vincono
        // sulla cache — nessun download, solo copia. Va fatto PRIMA di ogni dispatch: i comandi CLI
        // usano il P/Invoke (fixtures, brain, pkg) e senza allineamento caricherebbero la cache.
        try { Basic.AI.ModelDownload.CoreLibsBootstrap.SyncLocalBuildToCache(); } catch { }

        // CLI pacchetti (gmb init|pack|pkg ...|publish) — indipendente dal REPL.
        if (CliPackages.TryDispatch(args)) return;

        AppDomain.CurrentDomain.ProcessExit += (_, _) => Basic.Providers.ProviderServer.Shutdown();
        // CLI SETUP unificato (gmb setup TORCH|BROWSER|OCR|ALL|status|rm) — sostituisce
        // ML e BROWSER: download on-demand dei componenti runtime con manifest in Resources/.
        if (CliSetup.TryDispatch(args)) return;
        // CLI LIBS (gmb libs update|check|publish) — bundle delle librerie native di supporto
        // (gruppo B) per GMBasicModules: rigenera .zip + Resources/libs-manifest.json.
        if (CliLibs.TryDispatch(args)) return;
        // CLI DAEMON (gmb daemon ...) — gestione daemon come servizi del SO (launchd).
        if (CliDaemon.TryDispatch(args)) return;
        // CLI RAG/GRAPH (gmb rag build/update/status/query, gmb graph build/update/status)
        // — costruzione RAG wiki + grafi Obsidian (servizi runtime riusabili condivisi col REPL).
        if (CliRagGraph.TryDispatch(args)) return;
        // CLI BRAIN (gmb brain use/detach/list/info/install/export/sign) — gestione cervelli
        // (Second Brain): import zip, export zip, firma con probe minimi, collegamento multi.
        if (CliBrain.TryDispatch(args)) return;

        // CLI grep/rg (gmb grep ...) — ricerca file headless (motore C condiviso, P/Invoke).
        if (CliGrep.TryDispatch(args)) return;
        var opts = ParseArgs(args);
        var cliArgs = ArgsParser.Parse(args);

        // V1.1 — gmb -target wasm (POC): hint, poi esegue comunque il file come IL (per verifica)
        if (cliArgs.Target != null && cliArgs.Target.Equals("wasm", StringComparison.OrdinalIgnoreCase))
        {
            Console.WriteLine("WASM target (V1.1, POC): gmb prog.bas -target wasm -o prog.wasm");
            Console.WriteLine("  Vedi include/WasmBridge.inc + webapp/static/wasm.js + Guide/utente/WebDashboard-Guide.md §5.4");
            if (cliArgs.FileName != null) Console.WriteLine($"  File: {cliArgs.FileName} → {cliArgs.Target} (eseguo comunque come IL per verifica)");
        }

        // Bootstrap (Resources-libs-Classificazione-Confine): al primo avvio garantisce le
        // librerie core native (download da GMBasicModules; percorso alternativo se assente/
        // stale; conferma se incomplete). Best-effort: non blocca l'avvio.
        try
        {
            var interactive = !Console.IsInputRedirected && !Console.IsOutputRedirected;
            Basic.AI.ModelDownload.CoreLibsBootstrap.EnsureAsync(interactive).GetAwaiter().GetResult();
        }
        catch { /* best-effort: mai bloccare l'avvio per il bootstrap */ }

        var engine = new ReplEngine();
        engine.BreakRequested += () => { Console.WriteLine("\nBREAK."); };

        engine.Run(cliArgs);
    }

    public record ExecutorOptions(
        string? FileName = null,
        bool Managed = false,
        bool Native = false,
        bool StayInRepl = false,
        bool PCodeOnly = false,
        bool Full = false,
        string? CompileBspDir = null,
        bool NoOptimize = false,
        bool DumpIl = false,
        C99GpmMode GpmMode = C99GpmMode.Always
    );

    private static ExecutorOptions ParseArgs(string[] args)
    {
        string? fileName = null;
        var managed = false;
        var native = false;
        var stayInRepl = false;
        var pcodeOnly = false;
        var full = false;
        string? compileBspDir = null;
        var noOptimize = false;
        var dumpIl = false;
        var gpmMode = C99GpmMode.Always;

        var positional = new List<string>();

        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            switch (arg)
            {
                // -m / -managed: DLL managed + apphost stub (richiede .NET installato).
                case "-m" or "--managed": managed = true; break;
                // -n / -native: singolo binario nativo Mach-O/ELF (NativeAOT, nessun runtime).
                case "-n" or "--native": native = true; break;
                case "-repl": stayInRepl = true; break;
                case "-pcode": pcodeOnly = true; break;
                case "-full": full = true; break;
                case "-bsp":
                    if (i + 1 < args.Length && !args[i + 1].StartsWith('-'))
                        compileBspDir = args[++i];
                    break;
                case "--no-optimize": noOptimize = true; break;
                case "--dump-il": dumpIl = true; break;
                case "--target" or "-target":
                    if (i + 1 < args.Length && !args[i + 1].StartsWith('-'))
                        i++;
                    break;
                case "-o":
                    if (i + 1 < args.Length && !args[i + 1].StartsWith('-'))
                        i++;
                    break;
                default:
                    if (arg.StartsWith("-gpm:", StringComparison.OrdinalIgnoreCase))
                    {
                        if (Enum.TryParse<C99GpmMode>(arg["-gpm:".Length..], true, out var m))
                            gpmMode = m;
                    }
                    else if (!arg.StartsWith('-')) positional.Add(arg);
                    break;
            }
        }

        if (positional.Count > 0) fileName = positional[0];

        return new ExecutorOptions(fileName, managed, native, stayInRepl, pcodeOnly, full, compileBspDir, noOptimize, dumpIl, gpmMode);
    }
}
