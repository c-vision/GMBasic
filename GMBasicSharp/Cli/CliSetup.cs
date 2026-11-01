// REPL/CliSetup.cs — comando CLI unificato `gmb setup ...` per i componenti runtime
// scaricabili on-demand (sostituisce `gmb ml ...` e `gmb browser ...`).
//   gmb setup [TORCH|BROWSER|OBSCURA|OCR|ALL] [--force] [--quiet] [--yes]
//   gmb setup status                       stato cache di TUTTI i componenti
//   gmb setup rm <TORCH|BROWSER|OBSCURA|OCR|ALL> [--force]
//   gmb setup help                         elenco componenti e cosa scarica ognuno
// Ogni componente ha il proprio manifest in Resources/ (ml/chromium/obscura/ocr-manifest.json)
// e la propria cache nella radice utente ~/.gmbasic/libs/<comp>/<rid>/ (GmbiPaths; configurabile).
// I comandi ML e BROWSER sono stati rimossi: SETUP è l'unico punto di accesso.

using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Basic.AI.ModelDownload;
using Basic.ModelDownload;

namespace Basic.Executor;

public static class CliSetup
{
    public static bool TryDispatch(string[] args)
    {
        if (args.Length == 0 || !args[0].Equals("setup", StringComparison.OrdinalIgnoreCase)) return false;
        var rest = args.Skip(1).ToArray();
        var cmd = rest.Length > 0 ? rest[0].ToLowerInvariant() : "help";
        switch (cmd)
        {
            case "status": Run(() => Status(rest.Skip(1).ToArray())); break;
            case "rm": Run(() => Rm(rest.Skip(1).ToArray())); break;
            case "setup": Run(() => Setup(rest.Skip(1).ToArray())); break;
            default:
                if (FindComponentOrAll(rest) != null)
                    Run(() => Setup(rest));
                else
                    Help();
                break;
        }
        return true;
    }

    private static void Run(Action fn)
    {
        try { fn(); }
        catch (Exception ex) { Console.Error.WriteLine($"?{ex.Message}"); }
    }

    private static string[]? FindComponentOrAll(string[] args)
    {
        if (args.Length == 0) return null;
        var first = args[0].ToUpperInvariant();
        if (first == "ALL") return args;
        return SetupComponents.Find(first) != null ? args : null;
    }

    private static void Setup(string[] args)
    {
        var name = args.Length > 0 ? args[0].ToUpperInvariant() : "ALL";
        var flags = args.Skip(1).ToArray();
        var force = flags.Any(f => f.Equals("--force", StringComparison.OrdinalIgnoreCase));
        var quiet = flags.Any(f => f.Equals("--quiet", StringComparison.OrdinalIgnoreCase));
        var yes = flags.Any(f => f.Equals("--yes", StringComparison.OrdinalIgnoreCase));

        var targets = name == "ALL"
            ? SetupComponents.All
            : new[] { SetupComponents.Find(name) ?? throw new InvalidOperationException($"componente '{name}' sconosciuto (TORCH|BROWSER|OBSCURA|OCR|ALL)") };

        // ALL: mostra le dimensioni e chiede conferma (salvo --yes).
        if (name == "ALL" && !yes && !force)
        {
            Console.WriteLine("Componenti da scaricare in <cache>/<comp>/<rid>/ (~/.gmbasic/libs):");
            foreach (var c in targets)
                Console.WriteLine($"  {c.Name,-8} {c.Description}");
            Console.Write("Procedere? [y/N] ");
            var a = Console.ReadLine();
            if (a == null || !(a.Trim().Equals("y", StringComparison.OrdinalIgnoreCase)
                            || a.Trim().Equals("yes", StringComparison.OrdinalIgnoreCase)))
            { Console.WriteLine("annullato."); return; }
        }

        foreach (var c in targets)
        {
            var rid = MlCache.ResolveRid();
            if (!force && c.IsCached(rid))
            {
                Console.WriteLine($"[{c.Name.ToLowerInvariant()}] già in cache e valido: {c.CacheDir()}");
                continue;
            }
            if (force)
            {
                var dir = c.CacheDir();
                if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
            }

            var bar = new ConsoleProgressBar();
            Console.WriteLine($"[{c.Name.ToLowerInvariant()}] RID={rid} — scarico in {c.CacheDir()}...");
            var res = c.Setup(quiet ? null : Console.WriteLine, (done, total) => bar.Report(new ModelDownloader.DownloadProgress(done, total, total > 0 ? done * 100.0 / total : 0, null)), CancellationToken.None)
                .GetAwaiter().GetResult();
            bar.Finish();

            if (res.Success)
                Console.WriteLine($"[{c.Name.ToLowerInvariant()}] OK: {res.CacheDir} ({res.Files.Count} file). {c.ReadyNote}");
            else
                Console.Error.WriteLine($"?{c.Name} setup fallito: {res.Error}");
        }
    }

    private static void Status(string[] args)
    {
        var rid = MlCache.ResolveRid();
        Console.WriteLine($"RID: {rid}");
        foreach (var c in SetupComponents.All)
        {
            var dir = c.CacheDir();
            var cached = c.IsCached(rid);
            var manifest = c.ManifestPath();
            Console.WriteLine($"  {c.Name,-8} cache={dir}");
            Console.WriteLine($"           manifest={(File.Exists(manifest) ? "ok" : "MANCANTE")}  stato={(cached ? "COMPLETA" : "assente → gmb setup " + c.Name)}");
        }
    }

    private static void Rm(string[] args)
    {
        var name = args.Length > 0 ? args[0].ToUpperInvariant() : "ALL";
        var force = args.Any(f => f.Equals("--force", StringComparison.OrdinalIgnoreCase));
        var targets = name == "ALL" ? SetupComponents.All : new[] { SetupComponents.Find(name) ?? throw new InvalidOperationException($"componente '{name}' sconosciuto (TORCH|BROWSER|OBSCURA|OCR|ALL)") };

        foreach (var c in targets)
        {
            var dir = c.CacheDir();
            if (!Directory.Exists(dir)) { Console.WriteLine($"[{c.Name.ToLowerInvariant()}] nessuna cache da eliminare."); continue; }
            var size = Directory.GetFiles(dir, "*", SearchOption.AllDirectories).Sum(f => new FileInfo(f).Length);
            var ok = force;
            if (!ok)
            {
                Console.Write($"[{c.Name.ToLowerInvariant()}] eliminare {dir} ({size / (double)(1 << 20):F0} MB)? [y/N] ");
                var a = Console.ReadLine();
                ok = a != null && (a.Trim().Equals("y", StringComparison.OrdinalIgnoreCase) || a.Trim().Equals("yes", StringComparison.OrdinalIgnoreCase));
            }
            if (ok) { Directory.Delete(dir, recursive: true); Console.WriteLine($"[{c.Name.ToLowerInvariant()}] cache eliminata."); }
            else Console.WriteLine($"[{c.Name.ToLowerInvariant()}] annullato.");
        }
    }

    private static void Help()
    {
        Console.WriteLine("SETUP — gestione componenti runtime scaricabili on-demand (manifest in Resources/)");
        Console.WriteLine("  gmb setup [TORCH|BROWSER|OBSCURA|OCR|ALL] [--force] [--quiet] [--yes]");
        Console.WriteLine("  gmb setup status                  stato cache di tutti i componenti");
        Console.WriteLine("  gmb setup rm <TORCH|BROWSER|OBSCURA|OCR|ALL> [--force]");
        Console.WriteLine("  gmb setup help");
        Console.WriteLine();
        Console.WriteLine("Componenti (in <cache>/<comp>/<rid>/ = ~/.gmbasic/libs):");
        foreach (var c in SetupComponents.All)
            Console.WriteLine($"  {c.Name,-8} {c.Description}  → {c.CacheDir()}");
        Console.WriteLine("  ALL        tutti i componenti sopra");
        Console.WriteLine();
        Console.WriteLine("Nessun modello/libreria è dato per scontato sull'host: si scarica solo");
        Console.WriteLine("quando serve, con SHA256 verificato. ML e BROWSER sono stati sostituiti da SETUP.");
    }
}
