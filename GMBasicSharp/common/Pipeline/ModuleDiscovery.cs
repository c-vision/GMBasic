// FILE: Basic.Pipeline.ModuleDiscovery.cs
//
// Discovery dei moduli BASIC (`*.bas` / `*.inc`) e mappa namespace→file, condivisa da TUTTI i
// pipeline di compilazione:
//   - Orchestrator (Executor, programma completo, entry .bas)
//   - SourcePipeline (pipeline isolati: pagine BSP, EVAL)
//
// Prima questa logica esisteva SOLO come metodi privati di Orchestrator: i pipeline isolati
// non potevano risolvere gli `IMPORT` di una libreria BASIC e compilavano con l'insieme dei file
// ridotto alla sola entry. Conseguenza reale: una pagina `.bsp` con `IMPORT DASHBOARD` falliva
// con "Nessuna funzione pubblica ... nel namespace ..." (namespace noto, corpo mai caricato).
// Una sola implementazione, stesso risultato nei due percorsi.

namespace Basic.Pipeline;

/// <summary>Individuazione dei file sorgente BASIC e della mappa namespace → file.</summary>
public static class ModuleDiscovery
{
    private static readonly string[] SearchPatterns = ["*.bas", "*.inc"];

    /// <summary>
    /// Tutti i file `.bas`/`.inc` raggiungibili da <paramref name="projectRoot"/>: la cartella del
    /// progetto (ricorsiva), le <see cref="CommonLib.EffectiveSettings.ImportFolders"/>, i
    /// <see cref="CommonLib.EffectiveSettings.ImportFiles"/> e le cartelle di ricerca del
    /// compilatore (<c>compiler.searchFolders</c>). Lo shadowing è per NOME: un file del progetto
    /// (o degli import espliciti) vince su quello di una radice globale.
    /// </summary>
    public static HashSet<string> DiscoverSourceFiles(string projectRoot, CommonLib.EffectiveSettings settings)
    {
        var projectSourceFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        try
        {
            // 1. Cartella del progetto.
            if (Directory.Exists(projectRoot))
            {
                foreach (var pattern in SearchPatterns)
                    foreach (var file in Directory.GetFiles(projectRoot, pattern, SearchOption.AllDirectories))
                        projectSourceFiles.Add(Path.GetFullPath(file));
            }

            // 2. Cartelle di import: relative al progetto E alla repo-root (una libreria condivisa
            //    come `Controls/` deve essere importabile da un sample in qualunque sottocartella).
            foreach (var folder in settings.ImportFolders)
            {
                foreach (var baseDir in new[] { projectRoot, CommonLib.GmbiPaths.RepoRoot })
                {
                    var importPath = Path.GetFullPath(Path.Combine(baseDir, folder));
                    if (!Directory.Exists(importPath)) continue;
                    foreach (var pattern in SearchPatterns)
                        foreach (var file in Directory.GetFiles(importPath, pattern, SearchOption.AllDirectories))
                            projectSourceFiles.Add(Path.GetFullPath(file));
                }
            }

            // 3. File di import espliciti (progetto, poi repo-root).
            foreach (var file in settings.ImportFiles)
            {
                foreach (var baseDir in new[] { projectRoot, CommonLib.GmbiPaths.RepoRoot })
                {
                    var importPath = Path.GetFullPath(Path.Combine(baseDir, file));
                    if (File.Exists(importPath)) projectSourceFiles.Add(importPath);
                }
            }

            // 4. `compiler.searchFolders` (default in CompilerSettings.SearchFolders: include/,
            //    Controls/, <install>/../shared/include, ...). Shadowing per nome file.
            var localNames = new HashSet<string>(
                projectSourceFiles.Select(f => Path.GetFileName(f) ?? f), StringComparer.OrdinalIgnoreCase);
            foreach (var searchRoot in ResolveSearchFolders(settings.Compiler.SearchFolders, projectRoot))
            {
                foreach (var pattern in SearchPatterns)
                {
                    foreach (var file in Directory.GetFiles(searchRoot, pattern, SearchOption.AllDirectories))
                    {
                        if (localNames.Contains(Path.GetFileName(file))) continue;
                        projectSourceFiles.Add(Path.GetFullPath(file));
                    }
                }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            throw new global::Basic.BasicException(global::Basic.ErrorCode.FileNotFound, global::Basic.SourcePosition.Unknown,
                $"Errore durante la scansione del progetto: {ex.Message}");
        }

        return projectSourceFiles;
    }

    /// <summary>Mappa namespace → file che lo dichiarano (`NAMESPACE <nome>`).</summary>
    public static Dictionary<string, List<string>> BuildNamespaceMap(IEnumerable<string> files)
    {
        var map = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in files)
        {
            try
            {
                foreach (var line in File.ReadLines(file))
                {
                    var trimmedLine = line.Trim();
                    if (!trimmedLine.StartsWith("NAMESPACE", StringComparison.OrdinalIgnoreCase)) continue;

                    var parts = trimmedLine.Split([' ', '\t'], StringSplitOptions.RemoveEmptyEntries);
                    if (parts.Length <= 1) continue;

                    var nsName = parts[1];
                    if (!map.TryGetValue(nsName, out var fileList))
                    {
                        fileList = [];
                        map[nsName] = fileList;
                    }
                    if (!fileList.Contains(file)) fileList.Add(file);
                }
            }
            catch (IOException)
            {
                /* file illeggibile: ignorato come in Orchestrator */
            }
        }
        return map;
    }

    /// <summary>Radici di ricerca dei moduli BASIC da `compiler.searchFolders`.
    /// Placeholder: `{project}` = cartella della entry, `{install}` = root di GMBasic.
    /// Una voce relativa SENZA placeholder è cercata nella cartella del progetto, poi risalendo le
    /// cartelle, poi nella root dell'install. `*` finale = ogni sottocartella (una di livello).</summary>
    public static List<string> ResolveSearchFolders(IReadOnlyList<string> entries, string projectRoot)
    {
        var installRoot = Path.GetFullPath(CommonLib.GmbiPaths.RepoRoot);
        var roots = new List<string>();

        void Add(string? dir)
        {
            if (string.IsNullOrWhiteSpace(dir)) return;
            var full = Path.GetFullPath(dir);
            if (!Directory.Exists(full)) return;
            if (!roots.Contains(full, StringComparer.OrdinalIgnoreCase)) roots.Add(full);
        }

        foreach (var raw in entries)
        {
            if (string.IsNullOrWhiteSpace(raw)) continue;
            var entry = raw.Trim();
            var hinted = entry.Contains("{project}", StringComparison.OrdinalIgnoreCase)
                      || entry.Contains("{install}", StringComparison.OrdinalIgnoreCase);
            var expanded = entry
                .Replace("{project}", projectRoot, StringComparison.OrdinalIgnoreCase)
                .Replace("{install}", installRoot, StringComparison.OrdinalIgnoreCase);

            if (expanded.EndsWith("/*", StringComparison.Ordinal)
                || expanded.EndsWith("\\*", StringComparison.Ordinal))
            {
                var parent = expanded[..^2];
                if (!Directory.Exists(parent)) continue;
                foreach (var sub in Directory.EnumerateDirectories(parent)) Add(sub);
                continue;
            }

            if (hinted) { Add(expanded); continue; }

            var local = Path.Combine(projectRoot, expanded);
            if (Directory.Exists(local)) { Add(local); continue; }
            var up = FindSubdirUp(projectRoot, expanded);
            if (up != null) { Add(up); continue; }
            Add(Path.Combine(installRoot, expanded));
        }

        return roots;
    }

    /// <summary>Risale da <paramref name="fromDir"/> cercando `<livello>/<relative>` (cartella).</summary>
    public static string? FindSubdirUp(string fromDir, string relative)
    {
        var dir = new DirectoryInfo(Path.GetFullPath(fromDir));
        for (var depth = 0; dir != null && depth < 16; depth++, dir = dir.Parent)
        {
            var cand = Path.Combine(dir.FullName, relative);
            if (Directory.Exists(cand)) return cand;
        }
        return null;
    }
}
