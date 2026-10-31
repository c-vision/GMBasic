// FILE: Basic.AI.Knowledge.BrainVault
// §2 Second Brain — Brain Vault: genera un brain di documentazione dalla StdLib GMBasic
// (pagine .md per namespace con firme), indicizza in RAG, e interroga per code generation.
// Riusabile da REPL, CLI e BASIC. La generazione delle pagine dalla StdLib vive in IlRuntime
// (che ha accesso al registry StdLib); qui la scrittura su disco + indicizzazione RAG.
// Mai stub: senza db → errore esplicito.
using System.Text;
using Basic.AI.Rag;

namespace Basic.AI.Knowledge;

/// <summary>Brain Vault: scrive le pagine di documentazione su disco e le indicizza come RAG
/// per retrieval (code generation). Le pagine sono già generate dal chiamante (IlRuntime).</summary>
public static class BrainVault
{
    private const string Collection = "wiki";

    /// <summary>Costruisce la pagina markdown di un namespace (frontmatter + tabella firme).</summary>
    public static string BuildNamespacePage(string nsName, IReadOnlyList<Basic.NativeProcedureDefinition> procs)
    {
        var sb = new StringBuilder();
        sb.AppendLine("---");
        sb.AppendLine($"title: {nsName}");
        sb.AppendLine($"namespace: {nsName}");
        sb.AppendLine($"type: stdlib");
        sb.AppendLine($"functions: {procs.Count}");
        sb.AppendLine("---");
        sb.AppendLine();
        sb.AppendLine($"# Namespace {nsName} — {procs.Count} funzioni GMBasic");
        sb.AppendLine();
        sb.AppendLine("| Funzione | Firma | Ritorno |");
        sb.AppendLine("|---|---|---|");
        foreach (var p in procs.OrderBy(p => p.OriginalName.Lexeme, StringComparer.OrdinalIgnoreCase))
        {
            var name = p.OriginalName.Lexeme;
            var args = string.Join(", ", p.Parameters.Select(FormatParam));
            var ret = p.IsFunction ? FormatType(p.ReturnType) : "—";
            var optional = p.Parameters.Count(a => a.Flags.HasFlag(Basic.SymbolFlags.IsOptional)) > 0 ? " *(opzionali: "
                + string.Join(", ", p.Parameters.Where(a => a.Flags.HasFlag(Basic.SymbolFlags.IsOptional)).Select(a => a.OriginalName.Lexeme)) + ")*" : "";
            sb.AppendLine($"| `{nsName}.{name}()` | `{args}` | `{ret}`{optional} |");
        }
        sb.AppendLine();
        sb.AppendLine("Usa questa pagina per generare codice GMBasic corretto: quando ti chiedono"
            + " di scrivere codice che usa queste funzioni, consulta la firma esatta e i tipi.");
        return sb.ToString();
    }

    /// <summary>Formatta un parametro (nome + tipo, opzionale/ByRef).</summary>
    private static string FormatParam(Basic.Parameter p)
    {
        var sb = new StringBuilder();
        if (p.Flags.HasFlag(Basic.SymbolFlags.IsOptional)) sb.Append("?");
        if (p.Flags.HasFlag(Basic.SymbolFlags.IsByRef)) sb.Append("@");
        sb.Append(p.OriginalName.Lexeme);
        if (p.Flags.HasFlag(Basic.SymbolFlags.IsArray)) sb.Append("[]");
        sb.Append(":").Append(FormatType(p.DataType));
        return sb.ToString();
    }

    /// <summary>Formatta un DataType in un nome breve leggibile.</summary>
    private static string FormatType(Basic.DataType t) => t switch
    {
        Basic.StringType => "string",
        Basic.DoubleType => "double",
        Basic.IntegerType => "int",
        Basic.LongType => "long",
        Basic.BitType => "bool",
        Basic.ByteType => "byte",
        Basic.ArrayType at => FormatType(at.ElementType) + "[]",
        Basic.DictType => "dict",
        _ => t.ToString(),
    };

    /// <summary>Scrive le pagine .md nell'outputDir/brain/, una per namespace.
    /// pages = lista di (nomeNamespace, contenutoMarkdown). Ritorna conteggio pagine.</summary>
    public static int WritePages(string outputDir, IEnumerable<(string Name, string Content)> pages)
    {
        if (string.IsNullOrWhiteSpace(outputDir))
            throw new ArgumentException("outputDir richiesta", nameof(outputDir));
        var brainDir = Path.Combine(outputDir, "brain");
        Directory.CreateDirectory(brainDir);
        var count = 0;
        foreach (var (ns, md) in pages)
        {
            File.WriteAllText(Path.Combine(brainDir, Sanitize(ns) + ".md"), md);
            count++;
        }
        return count;
    }

    /// <summary>Indicizza le pagine in pagesDir in un db RAG al path esplicito dbPath
    /// (collezione wiki). Usa il provider embedding attivo.</summary>
    public static int IndexTo(string dbPath, string pagesDir, string name)
    {
        if (!Directory.Exists(pagesDir)) return 0;
        var files = Directory.GetFiles(pagesDir, "*.md", SearchOption.TopDirectoryOnly)
            .OrderBy(f => f).ToList();
        if (files.Count == 0) return 0;

        var dbAlias = "BvDb";
        RagDbSession.Open(dbPath, dbAlias);
        AiVectorRuntime.EnsureCollection(dbAlias, Collection, Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions);
        var count = 0;
        foreach (var f in files)
        {
            var sourceId = "/" + Path.GetFileName(f);
            var outcome = AiVectorRuntime.IndexText(dbAlias, Collection, sourceId, File.ReadAllText(f));
            if (outcome is Basic.SQLite.Vector.IndexTextOutcome.Indexed or Basic.SQLite.Vector.IndexTextOutcome.Updated) count++;
        }
        return count;
    }

    /// <summary>
    /// Nome effettivo del brain-vault. F4 — `brainvault` è un **alias riservato**: lo scope può
    /// rimapparlo su un brain diverso (`BRAIN USE altro AS brainvault`); senza mappatura resta il
    /// brain registrato `brainvault` (comportamento storico).
    /// </summary>
    public static string ResolveBrainVaultName(BrainScope? scope = null)
        => (scope ?? BrainScope.FromRegistry()).Resolve("brainvault")?.Ref.Name ?? "brainvault";

    /// <summary>Query semantica sul brain vault. Ritorna JSON {status, query, hits:[...]}.</summary>
    public static string Query(string query, int topK = 3, BrainScope? scope = null)
    {
        if (string.IsNullOrWhiteSpace(query)) return "{\"status\":\"Error\",\"error\":\"query vuota\"}";
        var brainVaultName = ResolveBrainVaultName(scope);
        var dbPath = BrainRegistry.RagDbPath(brainVaultName);
        if (dbPath is null || !File.Exists(dbPath))
            return $"{{\"status\":\"Error\",\"error\":\"{(brainVaultName == "brainvault" ? "brainvault non costruito (BuildBrainVault)" : $"brain '{brainVaultName}' non costruito")}\",\"hits\":[]}}";

        var dbAlias = "BvQ";
        RagDbSession.Open(dbPath, dbAlias);
        return AiVectorRuntime.SearchJson(dbAlias, Collection, query, topK, hybrid: true, filterJson: null);
    }

    /// <summary>WRITE: genera codice BASIC da una richiesta, usando il brain vault come contesto.
    /// Richiede un LLM attivo (LlmCompleterResolver.Default).</summary>
    public static string Write(string request, int topK = 3)
    {
        if (string.IsNullOrWhiteSpace(request)) return "Error: richiesta vuota";
        // ORDINE DEI CONTROLLI (parità IL/PCode/VM-C, stesso bug di AI.ANALYZEIMAGE): il
        // completer va verificato PRIMA di interrogare il brain vault. Query() apre il DB RAG
        // e può LANCIARE se il db esiste ma non è valido (es. creato da un altro test nella
        // stessa sessione) → la gamba PCode lanciava eccezione mentre l'oracolo deterministico
        // è l'errore esplicito "nessun provider LLM configurato (AI SET ACTIVE)".
        var completer = LlmCompleterResolver.Default;
        if (completer is null) return "Error: nessun provider LLM configurato (AI SET ACTIVE)";
        var context = Query(request, topK);
        var system = "Sei un generatore di codice GMBasic. Produci SOLO codice BASIC valido, "
            + "usando le firme esatte dal contesto. Niente spiegazioni fuori dal codice.";
        return completer.Complete(system, $"Contesto GMBasic (brain vault):\n{context}\n\nRichiesta: {request}");
    }

    private static string Sanitize(string s)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var sb = new System.Text.StringBuilder(s.Length);
        foreach (var c in s)
            sb.Append(invalid.Contains(c) ? '_' : c);
        return sb.ToString();
    }
}
