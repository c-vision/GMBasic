// FILE: GMBasicSharp/tests/GMBASIC.IlBackend.Tests/BuildFreshness.cs
// REGOLA ANTI-STALE del gate di parità (2026-11-01).
//
// PERCHÉ. Il gate xUnit (`MiniTests_IL_PCode_Parity`) esegue i MiniTest sulle gambe
// IL/PCode/VM-C usando le DLL del compilatore CARICATE dal processo di test, cioè le copie
// dentro `bin/Debug/net10.0` del progetto di test. Se dopo una modifica al frontend o a un
// backend il progetto di test NON viene ricostruito — o viene ricostruito senza ricostruire
// le dipendenze — il gate testa codice VECCHIO e può dare un falso verde (o un falso rosso).
// Caso reale (2026-11-01): il canone `AND`/`OR` era stato corretto in `Compiler`/IL ma il
// gate continuava a riportare i valori vecchi, perché la DLL del test project era stale;
// serviva un `dotnet build` esplicito del test project per accorgersene.
//
// COSA FA. All'avvio del modulo di test (ModuleInitializer: si applica a QUALUNQUE run, anche
// filtrato con `--filter`) confronta il timestamp degli assembly tracciati con il sorgente
// `.cs`/`.csproj` più recente del progetto corrispondente. Se una DLL è più vecchia del suo
// sorgente, l'intera suite fallisce con l'istruzione esatta da eseguire. Non esiste un
// interruttore per disattivarla (nessuna variabile d'ambiente, AGENTS §0): una DLL stale nel
// gate di parità non è mai una situazione accettabile.

using System.Runtime.CompilerServices;

namespace Basic.IlBackend.Tests;

internal static class BuildFreshness
{
    /// <summary>Assembly del compilatore → cartella sorgente (relativa a `GMBasicSharp/`).
    /// Sono le dipendenze da cui dipende la correttezza del gate di parità: se una cambia e
    /// non viene ricostruita, i MiniTest misurano il comportamento vecchio.</summary>
    private static readonly (string Assembly, string SourceDir)[] Tracked =
    [
        ("Ast", "common/Ast"),
        ("Lexer", "common/Lexer"),
        ("Parser", "common/Parser"),
        ("Compilation", "common/Compilation"),
        ("Linker", "common/Linker"),
        ("Compiler", "common/Compiler"),
        ("CommonLib", "common/CommonLib"),
        ("GMBasic.StdLib", "common/StdLib"),
        ("GMBasic.IlBackend", "common/IlBackendCodegen"),
        ("Pipeline", "common/Pipeline"),
        ("BRT", "common/BRT"),
    ];

    /// <summary>Tolleranza fra mtime del sorgente e mtime della DLL: il filesystem può
    /// arrotondare e l'ordine di scrittura dentro la stessa build non è garantito. Con 2 s
    /// non si producono falsi positivi su build reali, mentre una modifica vera (secondi o
    /// minuti dopo la build) resta sempre rilevata.</summary>
    private static readonly TimeSpan Tolerance = TimeSpan.FromSeconds(2);

    [ModuleInitializer]
    internal static void Init() => AssertFresh();

    /// <summary>Fallisce se un assembly tracciato è più vecchio del suo sorgente.</summary>
    public static void AssertFresh()
    {
        var repoRoot = FindRepoRoot();
        if (repoRoot is null) return; // fuori dal checkout: nessun confronto possibile

        var problems = new List<string>();
        foreach (var (assemblyName, relDir) in Tracked)
        {
            var asm = AppDomain.CurrentDomain.GetAssemblies()
                .FirstOrDefault(a => string.Equals(a.GetName().Name, assemblyName, StringComparison.OrdinalIgnoreCase));
            if (asm is null || string.IsNullOrEmpty(asm.Location) || !File.Exists(asm.Location)) continue;

            var srcDir = Path.Combine(repoRoot, "GMBasicSharp",
                relDir.Replace('/', Path.DirectorySeparatorChar));
            if (!Directory.Exists(srcDir)) continue;

            var newest = NewestSource(srcDir);
            if (newest is not { } src) continue;

            var dllTime = File.GetLastWriteTimeUtc(asm.Location);
            if (src.Time > dllTime + Tolerance)
                problems.Add($"{assemblyName}: '{Path.GetFileName(asm.Location)}' è del {dllTime:yyyy-MM-dd HH:mm:ss}Z " +
                             $"ma il sorgente '{Path.GetRelativePath(repoRoot, src.File)}' è del {src.Time:yyyy-MM-dd HH:mm:ss}Z");
        }

        if (problems.Count == 0) return;

        throw new InvalidOperationException(
            "DLL STALE nel test runner: il gate di parità testa codice VECCHIO." + Environment.NewLine +
            "  Ricostruisci il progetto di test (e le sue dipendenze) e rilancia:" + Environment.NewLine +
            "    dotnet build GMBasicSharp/tests/GMBASIC.IlBackend.Tests" + Environment.NewLine +
            "  Dettaglio:" + Environment.NewLine +
            "    " + string.Join(Environment.NewLine + "    ", problems));
    }

    private static (string File, DateTime Time)? NewestSource(string dir)
    {
        (string File, DateTime Time)? best = null;
        foreach (var f in Directory.EnumerateFiles(dir, "*.cs", SearchOption.AllDirectories))
        {
            var norm = f.Replace('\\', '/');
            if (norm.Contains("/obj/") || norm.Contains("/bin/")) continue;
            var t = File.GetLastWriteTimeUtc(f);
            if (best is null || t > best.Value.Time) best = (f, t);
        }
        foreach (var f in Directory.EnumerateFiles(dir, "*.csproj", SearchOption.TopDirectoryOnly))
        {
            var t = File.GetLastWriteTimeUtc(f);
            if (best is null || t > best.Value.Time) best = (f, t);
        }
        return best;
    }

    private static string? FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, "GMBasicSharp", "common", "Parser")))
                return dir.FullName;
            dir = dir.Parent;
        }
        return null;
    }
}
