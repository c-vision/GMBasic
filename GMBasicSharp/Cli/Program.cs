// gmb — CLI del mondo C# (ex `GMBasicSharp/REPL`, che conteneva anche la REPL interattiva).
//
// La REPL interattiva è il binario C `gmb_repl` (cartella `REPL/`). Questo CLI conserva i
// comandi amministrativi/di servizio che vivono nel mondo C# e l'esecuzione dei programmi
// con l'engine C# (delegata a `gmbcs`):
//
//   gmb init|pack|keygen|bump|add|remove|login|register|logout|cache|admin|pkg|publish
//       restore|update|trust|rollback                    (CliPackages)
//   gmb setup …                                          (componenti runtime on-demand)
//   gmb libs update|check|publish                         (bundle librerie native)
//   gmb daemon …                                          (servizi del SO)
//   gmb rag|graph …                                       (indice RAG / grafi)
//   gmb brain …                                           (Second Brain)
//   gmb grep|rg …                                         (ricerca)
//   gmb <file.bas> [-pcode]                               (esegue con l'engine C#, via gmbcs)
//
// Senza argomenti NON apre una REPL: indica `gmb_repl` (C).
using System.Diagnostics;

namespace Basic.Executor;

public static class Program
{
    public static int Main(string[] args)
    {
        Environment.SetEnvironmentVariable("COREHOST_TRACE", "0");
        Environment.SetEnvironmentVariable("DOTNET_AssemblyLoadDebug", "0");
        Environment.SetEnvironmentVariable("DOTNET_Debug", "0");

        if (Debugger.IsAttached)
        {
            var defaultListener = Trace.Listeners.OfType<DefaultTraceListener>().FirstOrDefault();
            if (defaultListener != null) Trace.Listeners.Remove(defaultListener);
        }

        if (args.Length == 1 && args[0] is "-v" or "--version")
        {
            Console.WriteLine("gmb 1.0.0 (CLI C#; REPL interattiva: gmb_repl)");
            return 0;
        }

        // Nel checkout le librerie appena compilate vincono sulla cache (come prima del split).
        try { Basic.AI.ModelDownload.CoreLibsBootstrap.SyncLocalBuildToCache(); } catch { }

        // Comandi CLI (indipendenti dalla REPL).
        if (CliPackages.TryDispatch(args)) return 0;
        AppDomain.CurrentDomain.ProcessExit += (_, _) => Basic.Providers.ProviderServer.Shutdown();
        if (CliSetup.TryDispatch(args)) return 0;
        if (CliLibs.TryDispatch(args)) return 0;
        if (CliDaemon.TryDispatch(args)) return 0;
        if (CliRagGraph.TryDispatch(args)) return 0;
        if (CliBrain.TryDispatch(args)) return 0;
        if (CliGrep.TryDispatch(args)) return 0;
        if (CliTools.TryDispatch(args)) return 0;

        if (args.Length == 1 && args[0] is "-h" or "--help")
        {
            Usage();
            return 0;
        }

        if (args.Length == 0 || args[0] is "repl" or "--repl")
        {
            Console.Error.WriteLine("La REPL interattiva e' il binario C `gmb_repl` (cartella REPL/):");
            Console.Error.WriteLine("  cmake -S REPL -B REPL/build && cmake --build REPL/build --target gmb_repl");
            Console.Error.WriteLine("  REPL/build/gmb_repl");
            Console.Error.WriteLine("Per eseguire un programma: `gmbcs run <file>` (engine C#) o");
            Console.Error.WriteLine("`gmb run --project <dir> --entry <file>` (compilatore C).");
            Console.Error.WriteLine("Questo CLI copre: pacchetti, brain, libs, setup, daemon, rag/graph, grep.");
            return 2;
        }

        // Un file BASIC: esecuzione con l'engine C# (gmbcs), con gli stessi flag storici.
        if (!args[0].StartsWith('-') || File.Exists(args[0]))
            return RunFile(args);

        Usage();
        return 2;
    }

    private static int RunFile(string[] args)
    {
        var list = new List<string>(args);
        var pcode = list.Remove("-pcode") | list.Remove("--pcode");
        var managed = list.Remove("-m") | list.Remove("--managed");
        var native = list.Remove("-n") | list.Remove("--native");
        var ast = list.Remove("--ast");
        var full = list.Remove("-full") | list.Remove("--full");

        if (native)
        {
            Console.Error.WriteLine("?-n/--native (AOT): non e' nel CLI C. Usa `gmbcs build --target il` (IL gestito)");
            Console.Error.WriteLine("  oppure il compilatore C: `gmb build --project <dir>`.");
            return 2;
        }
        if (full)
            return Basic.CliEngine.Program.Main(["build", "--target", "il", "--full", .. list]);

        if (ast)
            return Basic.CliEngine.Program.Main(["dump", "ast", .. list]);

        if (managed)
            return Basic.CliEngine.Program.Main(["build", "--target", "il", .. list]);

        // Flag della REPL (console/debug/BSP/wasm) non appartengono al runner headless: errore
        // esplicito, MAI ignorati in silenzio (migrazione F6b: nessuna funzione sparisce muta).
        for (var i = 0; i < list.Count; i++)
        {
            var a = list[i];
            if (a is "-size" or "-font" or "-w" or "-h" or "--debug-tcp" or "--run" or "-bsp"
                || a == "-gpm" || a.StartsWith("-gpm:")
                || a is "-c" or "-console" or "-s" or "-c64" or "-noconsole" or "--sdl")
            {
                Console.Error.WriteLine($"?{a}: opzione della REPL/console, non del runner headless.");
                Console.Error.WriteLine("  REPL interattiva e console: `gmb_repl` (REPL/build/gmb_repl).");
                Console.Error.WriteLine("  Debug dell'engine C#: `gmbcs debug <file> --break N`.");
                return 2;
            }
            if (a is "--target" or "-target" or "-o")
            {
                Console.Error.WriteLine($"?{a}: build wasm/POC rimossa con la REPL C#.");
                Console.Error.WriteLine("  Emetti C con il compilatore C: `gmb dump c <file.bas> --out <f.c>`.");
                return 2;
            }
            if (a == "--il-module") { i++; continue; }   // inoltrato a gmbcs
            if (a.StartsWith("-D")) continue;            // inoltrato a gmbcs
        }

        var runArgs = new List<string> { "run" };
        runArgs.AddRange(list);
        if (pcode) { runArgs.Add("--engine"); runArgs.Add("pcode"); }
        return Basic.CliEngine.Program.Main([.. runArgs]);
    }

    private static void Usage()
    {
        Console.WriteLine("""
            gmb — CLI del mondo C# (pacchetti, servizi, esecuzione con l'engine C#)

            uso:
              gmb init|pack|keygen|bump|add|remove|login|register|logout|cache|admin|pkg|publish
                  restore|update|trust|rollback
              gmb setup … | gmb libs update|check|publish | gmb daemon …
              gmb rag … | gmb graph … | gmb brain … | gmb grep|rg …
              gmb convert <file> [-o out] | gmb convfmt | gmb hf <repo> | gmb mlx <repo>
              gmb <file.bas> [-pcode]        esegue con l'engine C# (gmbcs)
              gmb -v | --version | -h | --help

            REPL interattiva: `gmb_repl` (C, cartella REPL/). Non esiste una REPL C#.
            """);
    }
}
