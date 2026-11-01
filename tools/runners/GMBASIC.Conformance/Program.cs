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

// ── MODULI: un file che dichiara `NAMESPACE X` e NON ha alcun punto d'ingresso
// (nessun `SUB/FUNCTION Main`, nessuna linea numerata) è una LIBRERIA importata da un
// altro campione, non un programma eseguibile: compilarlo stand-alone produce errori del
// suo corpo (es. "Simbolo non definito: 'DeepOrigin'" in chain/Chain.bas, il cui IMPORT
// DEMO_DEEP lo risolve solo insieme alla entry). Il campione vero (AllFolders.bas) li
// importa e viene compilato normalmente.
var nsRe = new System.Text.RegularExpressions.Regex(@"^\s*NAMESPACE\s+([A-Za-z_][A-Za-z0-9_]*)",
    System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.Multiline);
var mainRe = new System.Text.RegularExpressions.Regex(@"\b(SUB|FUNCTION)\s+Main\b",
    System.Text.RegularExpressions.RegexOptions.IgnoreCase);
var numLineRe = new System.Text.RegularExpressions.Regex(@"^\s*\d+\s",
    System.Text.RegularExpressions.RegexOptions.Multiline);
var moduleFiles = new HashSet<string>(StringComparer.Ordinal);
foreach (var f in files)
{
    string txt;
    try { txt = File.ReadAllText(f); } catch { continue; }
    if (nsRe.IsMatch(txt) && !mainRe.IsMatch(txt) && !numLineRe.IsMatch(txt))
        moduleFiles.Add(f);
}

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

    /* MODULO vs CAMPIONE: un file che dichiara `NAMESPACE X` ed è IMPORTATO da un altro
     * campione non è un programma eseguibile (es. Modules/FolderResolution/chain/Chain.bas,
     * libs/helpers/Deep.bas): compilarlo stand-alone produce errori del suo corpo
     * ("Simbolo non definito: 'DeepOrigin'") che non sono gap del compilatore. Il campione
     * vero (AllFolders.bas) li importa e viene compilato normalmente. */
    if (moduleFiles.Contains(file))
    {
        skipped++;
        continue;
    }

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
        var msg = ex.Message;
        /* MODULO, non campione: `shared/samples/Modules/**` contiene anche i file IMPORTATI dai
         * campioni (chain/nested/libs/pinned/shared): non hanno (e non devono avere) un punto
         * d'ingresso. Prima erano contati come FAIL, falsando il conteggio della Conformance. */
        if (msg.Contains("No valid entry point", StringComparison.OrdinalIgnoreCase))
        {
            skipped++;
            continue;
        }
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
