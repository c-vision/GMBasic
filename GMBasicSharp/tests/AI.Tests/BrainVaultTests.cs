// FILE: AI.Tests.BrainVaultTests
// §2 Second Brain — BrainVault: genera pagine di documentazione dalla StdLib (una .md per
// namespace), le indicizza in RAG (FixtureEmbeddingProvider), e risponde a query semantiche.
// Isolato: salva/ripristina lo stato globale (resolver/SqlDataSession/AiConfig/BrainRegistry).
using System.Collections.Immutable;
using Basic.AI;
using Basic.AI.Knowledge;
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class BrainVaultTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _outDir = Path.Combine(Path.GetTempPath(), "gmbasic-bv-" + Guid.NewGuid().ToString("N"));
    private readonly string _ragDir = Path.Combine(Path.GetTempPath(), "gmbasic-bv-rag-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-bv-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string? _prevRagDir;
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;

    public BrainVaultTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        _prevRagDir = AiConfig.RagDefaultDir;
        AiConfig.RagDefaultDir = _ragDir;
        BrainRegistry.OverridePathForTests(_registryPath);
        _prevEmb = Basic.SQLite.Vector.EmbeddingProviderResolver.Default;
        _prevDims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
    }

    public void Dispose()
    {
        // Ripristina il singleton al valore PRECEDENTE (chi lo aveva prima, lo ritrova). Non
        // chiamare CloseAll sul nostro DataSession (chiuderebbe aree condivise con altri test).
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = _prevEmb;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiConfig.RagDefaultDir = _prevRagDir ?? AiConfig.RagDefaultDir;
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        try { Directory.Delete(_outDir, recursive: true); } catch { }
        try { Directory.Delete(_ragDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    /// <summary>Una NativeProcedureDefinition sintetica per testare la generazione di pagine
    /// (senza dipendere da StdLib, non referenziato da AI.Tests).</summary>
    private static Basic.NativeProcedureDefinition Proc(string ns, string name, string[] args, string ret)
    {
        var ps = args.Select((a, i) => new Basic.Parameter(i, new Basic.Token(Basic.TokenKind.Identifier, a, null, Basic.SourcePosition.Unknown),
            new Basic.StringType(), Basic.SymbolFlags.None, null, false)).ToImmutableArray();
        return new Basic.NativeProcedureDefinition(0, Basic.SymbolFlags.IsPublic | Basic.SymbolFlags.IsFunction,
            new Basic.Token(Basic.TokenKind.Identifier, name, null, Basic.SourcePosition.Unknown), ps, new Basic.StringType(),
            Basic.TokenKind.Identifier);
    }

    [Fact]
    public void BuildNamespacePage_HasFrontmatterAndSignatureTable()
    {
        var procs = new List<Basic.NativeProcedureDefinition>
        {
            Proc("AI", "ENRICH", ["text", "mode"], "string"),
            Proc("AI", "QAANSWER", ["question", "brain"], "string"),
        };
        var md = BrainVault.BuildNamespacePage("AI", procs);
        md.Should().Contain("---");
        md.Should().Contain("namespace: AI");
        md.Should().Contain("| Funzione | Firma | Ritorno |");
        md.Should().Contain("AI.ENRICH");
        md.Should().Contain("AI.QAANSWER");
    }

    [Fact]
    public void WritePages_CreatesMarkdownPerNamespace()
    {
        var pages = new List<(string, string)>
        {
            ("AI", "# AI\n\nfirme"),
            ("GRAPH", "# GRAPH\n\nfirme"),
        };
        var written = BrainVault.WritePages(_outDir, pages);
        written.Should().Be(2);
        File.Exists(Path.Combine(_outDir, "brain", "AI.md")).Should().BeTrue();
        File.Exists(Path.Combine(_outDir, "brain", "GRAPH.md")).Should().BeTrue();
    }

    [Fact]
    public void Index_Then_Query_ReturnsHits()
    {
        var pages = new List<(string, string)>
        {
            ("AI", BuildMinimalPage("AI", "ricerca vettoriale semantica su collezioni sqlite")),
            ("GRAPH", BuildMinimalPage("GRAPH", "knowledge graph con wikilink e pagerank")),
        };
        BrainVault.WritePages(_outDir, pages);
        var dbPath = Path.Combine(AiConfig.RagDefaultDir, "brainvault.db");
        var pagesDir = Path.Combine(_outDir, "brain");
        var indexed = BrainVault.IndexTo(dbPath, pagesDir, "brainvault");
        indexed.Should().Be(2);

        BrainRegistry.Register("brainvault", mode: "rag", source: dbPath);
        var res = BrainVault.Query("ricerca vettoriale", 3);
        res.Should().Contain("\"Success\"");
        res.Should().Contain("hits");
    }

    private static string BuildMinimalPage(string ns, string text) =>
        $"---\ntitle: {ns}\nnamespace: {ns}\ntype: stdlib\nfunctions: 2\n---\n\n# Namespace {ns}\n\n{text}\n"
        + "\n| Funzione | Firma | Ritorno |\n|---|---|---|\n"
        + $"| `{ns}.FUNC1()` | `a:string` | `string` |\n"
        + $"| `{ns}.FUNC2()` | `b:int` | `double` |\n";

    [Fact]
    public void Query_BeforeBuild_ReturnsError()
    {
        // assicura che non ci sia un brain brainvault dal test precedente (config condivisa)
        BrainRegistry.Unregister("brainvault");
        var db = Path.Combine(AiConfig.RagDefaultDir, "brainvault.db");
        if (File.Exists(db)) File.Delete(db);
        var res = BrainVault.Query("qualsiasi", 3);
        res.Should().Contain("non costruito");
    }

    [Fact]
    public void Write_WithoutCompleter_ReturnsExplicitError()
    {
        LlmCompleterResolver.Default = null;
        var r = BrainVault.Write("genera una funzione che somma due numeri");
        r.Should().Contain("nessun provider LLM");
    }
}
