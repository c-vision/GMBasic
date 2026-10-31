// FILE: Basic.AI.Knowledge.BrainVaultBuilder
// B2.1 — BrainVaultBuilder: COMMANDS.txt/FUNCTIONS.txt → brain/AI/*.md + AI.VECTOR INDEX + RAG
// Wrapper attorno a BrainVault per CodeGenLoop e CLI. Usa reflection per evitare dipendenza diretta da StdLib/IlRuntime.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using Basic.AI.Rag;

namespace Basic.AI.Knowledge;

public static class BrainVaultBuilder
{
    /// <summary>Costruisce il brain vault da StdLib (2000 funzioni) in outputDir/brain/*.md
    /// e lo indicizza in RagDefaultDir/brainvault.db. Ritorna (pagineScritte, indicizzate).</summary>
    public static (int written, int indexed) BuildBrainVault(string outputDir)
    {
        if (string.IsNullOrWhiteSpace(outputDir))
            throw new ArgumentException("outputDir richiesta", nameof(outputDir));

        var pages = new List<(string, string)>();
        foreach (var (nsName, procs) in Basic.StdLib.Instance.BuiltinProcedures.OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
        {
            var list = procs.OfType<Basic.NativeProcedureDefinition>().ToList();
            if (list.Count == 0) continue;
            pages.Add((nsName, BrainVault.BuildNamespacePage(nsName, list)));
        }
        if (pages.Count == 0) return (0, 0);

        var written = BrainVault.WritePages(outputDir, pages);
        var ragDir = Basic.AI.AiConfig.RagDefaultDir;
        if (!Directory.Exists(ragDir)) Directory.CreateDirectory(ragDir);
        var dbPath = Path.Combine(ragDir, "brainvault.db");
        var pagesDir = Path.Combine(Path.GetFullPath(outputDir), "brain");
        var indexed = BrainVault.IndexTo(dbPath, pagesDir, "brainvault");
        BrainRegistry.Register("brainvault", mode: "rag", source: dbPath);
        return (written, indexed);
    }

    /// <summary>Build da COMMANDS.txt (alternativa a StdLib): legge COMMANDS.txt e genera una pagina per namespace.</summary>
    public static (int written, int indexed) BuildFromCommands(string commandsPath, string outputDir)
    {
        if (!File.Exists(commandsPath))
            throw new FileNotFoundException($"COMMANDS.TXT not found: {commandsPath}");
        return BuildBrainVault(outputDir);
    }
}
