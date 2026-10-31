// GMBASIC Conformance runner.
// Compila i sample in shared/samples/ come metro di misura di cio' che e' implementato.
// Non esiste piu' alcuna edizione: ogni sample viene compilato ed eseguito con l'unico
// build, che contiene tutte le funzioni (database su file inclusi).
//
// Uso: dotnet run --project tests/GMBASIC.Conformance [<filtro>]

using System;
using System.IO;
using System.Linq;

var rootDir = FindRoot();
var samplesDir = Path.Combine(rootDir, "shared", "samples");
var sampleDirs = new List<string> { samplesDir };
Console.WriteLine("Conformance: un solo build, tutte le funzioni disponibili");

if (args.Length > 0 && args[0] == "vmtest")
{
    Environment.ExitCode = VmBridgeTest.Run(args);
    return;
}

var filter = args.Length > 0 ? args[0] : null;
// samples/database/ è scratch/lavoro git-ignorato (vedi AGENTS.md §9.3), non sample da testare.
var excludedDir = Path.Combine(samplesDir, "database") + Path.DirectorySeparatorChar;
var files = sampleDirs
    .Where(Directory.Exists)
    .SelectMany(d => Directory.GetFiles(d, "*.bas", SearchOption.AllDirectories))
    .Where(f => !f.StartsWith(excludedDir, StringComparison.Ordinal))
    .Where(f => filter is null || f.Contains(filter, StringComparison.OrdinalIgnoreCase))
    .OrderBy(f => f)
    .ToList();

Console.WriteLine($"Esempi trovati: {files.Count}");
Console.WriteLine();

int passed = 0, failed = 0, skipped = 0;
var errors = new List<(string File, string Error)>();

static string FindRoot()
{
    var dir = new DirectoryInfo(Directory.GetCurrentDirectory());
    while (dir != null)
    {
        if (File.Exists(Path.Combine(dir.FullName, "GMBasicSharp.slnx"))) return dir.Parent?.FullName ?? dir.FullName;
        if (File.Exists(Path.Combine(dir.FullName, "GMBASIC.sln"))) return dir.FullName;
        dir = dir.Parent;
    }
    return Directory.GetCurrentDirectory();
}

foreach (var file in files)
{
    var name = Path.GetRelativePath(samplesDir, file);
    try
    {
        var program = Basic.Orchestrator.Compile(file, null, false);
        if (program == null || program.MainModule == null)
        {
            Console.WriteLine($"  ERROR [{name}] compilazione fallita (null)");
            failed++;
            errors.Add((name, "Orchestrator.Compile returned null"));
            continue;
        }

        // Compila anche a P-Code (codegen)
        var chunk = new global::Compiler.Compiler().Compile(program);
        if (chunk == null || chunk.Code.Count == 0)
        {
            Console.WriteLine($"  ERROR [{name}] codegen P-Code vuoto");
            failed++;
            errors.Add((name, "P-Code chunk vuoto"));
            continue;
        }

        passed++;
    }
    catch (Exception ex)
    {
        // Errore chiaro per feature commerciali vs mancanza vera
        var msg = ex.Message;
        Console.WriteLine($"  ERROR [{name}] {msg.Split('\n')[0]}");
        failed++;
        errors.Add((name, msg));
    }
}

Console.WriteLine();
Console.WriteLine("================== CONFORMANCE REPORT ==================");
Console.WriteLine($"  PASS:   {passed}");
Console.WriteLine($"  FAIL:   {failed}");
Console.WriteLine($"  SKIP:   {skipped}");
Console.WriteLine($"  TOTALE: {files.Count}");
Console.WriteLine("=========================================================");

if (failed > 0)
{
    Console.WriteLine();
    Console.WriteLine("Esempi falliti (metrico di cosa manca):");
    foreach (var (f, e) in errors.Take(40))
        Console.WriteLine($"  - {f}: {e.Split('\n')[0]}");
    if (errors.Count > 40)
        Console.WriteLine($"  ... e altri {errors.Count - 40}");
    Environment.ExitCode = 1;
}
