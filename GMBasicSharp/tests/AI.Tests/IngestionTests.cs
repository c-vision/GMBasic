// FILE: AI.Tests.IngestionTests
using Basic.AI.Rag;
using Basic.AI.Skills;
using Basic.AI.Guardrails;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class IngestionTests : IDisposable
{
    private readonly string _tmpPdf = Path.Combine(Path.GetTempPath(), "gmb_ingest_" + Guid.NewGuid().ToString("N") + ".pdf");
    private readonly string _tmpTxt = Path.Combine(Path.GetTempPath(), "gmb_ingest_" + Guid.NewGuid().ToString("N") + ".txt");
    private readonly string? _prevRagDir;
    private readonly string _ragDir;          // dir TEMPORANEA di questo test (l'unica cancellabile)
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;

    public IngestionTests()
    {
        _prevRagDir = Basic.AI.AiConfig.RagDefaultDir;
        _ragDir = Path.Combine(Path.GetTempPath(), "gmb_ingest_rag_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_ragDir);
        Basic.AI.AiConfig.RagDefaultDir = _ragDir;
        _prevEmb = Basic.SQLite.Vector.EmbeddingProviderResolver.Default;
        _prevDims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
    }

    public void Dispose()
    {
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        Basic.AI.AiConfig.RagDefaultDir = _prevRagDir ?? Basic.AI.AiConfig.RagDefaultDir;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = _prevEmb;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        try { if (File.Exists(_tmpPdf)) File.Delete(_tmpPdf); } catch {}
        try { if (File.Exists(_tmpTxt)) File.Delete(_tmpTxt); } catch {}
        // ⚠ INCIDENTE 2026-09-16: qui c'era `Directory.Delete(AiConfig.RagDefaultDir, true)` DOPO aver
        // ripristinato `RagDefaultDir` al valore precedente (= <repo>/rag): ogni run della suite cancellava
        // la cartella rag reale (brainvault.db perso). Si cancella SOLO la dir temporanea di QUESTO test,
        // e solo se è davvero sotto la temp (guardia sistemica TestPaths).
        TestPaths.SafeDeleteTree(_ragDir);
        SkillRegistry.Clear();
    }

    [Fact]
    public void Import_Txt_As_Note_Then_Search()
    {
        File.WriteAllText(_tmpTxt, "Questo è un documento di test per la ricerca semantica. Contiene la parola chiave unicorn.");
        var (chunks, indexed, sourceId) = IngestionBridge.Import(_tmpTxt, "note", "kb", "docs");
        Assert.True(chunks > 0);
        Assert.True(indexed > 0);
        var hits = AiVectorRuntime.SearchJson("kb", "docs", "unicorn", 3, false, null);
        Assert.Contains("unicorn", hits, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void RequireSkill_And_Search()
    {
        Assert.True(SkillRegistry.RequireSkill("github:user/repo"));
        Assert.True(SkillRegistry.Exists("user/repo"));
        var res = SkillRegistry.Search("user", 5);
        Assert.NotEmpty(res);
        var json = SkillRegistry.SearchJson("user", 5);
        Assert.Contains("user/repo", json);
    }

    [Fact]
    public void RunStructured_InvalidJson_ReturnsError()
    {
        // Use a completer that returns non-JSON text → validation must fail with explicit error
        var badCompleter = new TextCompleter("[non-json response]");
        var schema = "{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"string\"}},\"required\":[\"field\"]}";
        var (ok, json, err) = StructuredRunner.RunStructured("prompt", schema, badCompleter);
        Assert.False(ok);
        Assert.NotNull(err);
    }

    private sealed class TextCompleter : Basic.AI.Rag.ILlmCompleter
    {
        private readonly string _text;
        public TextCompleter(string text) => _text = text;
        public string ModelName => "text";
        public string Complete(string systemPrompt, string userPrompt) => _text;
    }

    [Fact]
    public void RunStructured_WithValidJsonCompleter_Passes()
    {
        var validCompleter = new ValidJsonCompleter("{\"field\":\"hello\"}");
        var schema = "{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"string\"}},\"required\":[\"field\"]}";
        var (ok, json, err) = StructuredRunner.RunStructured("prompt", schema, validCompleter);
        Assert.True(ok);
        Assert.Contains("hello", json);
    }

    private sealed class ValidJsonCompleter : Basic.AI.Rag.ILlmCompleter
    {
        private readonly string _json;
        public ValidJsonCompleter(string json) => _json = json;
        public string ModelName => "valid-json";
        public string Complete(string systemPrompt, string userPrompt) => _json;
    }
}
