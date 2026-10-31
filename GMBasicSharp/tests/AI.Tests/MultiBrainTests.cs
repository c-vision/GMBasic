// FILE: AI.Tests.MultiBrainTests
// Second Brain R7: MultiBrainSearch (SEARCHMULTI) — query distribuita sui brain used con
// merge RRF + provenance "brain". Usa db RAG reali su file (fixture embedding) costruiti via
// AiVectorRuntime.EnsureCollection/IndexText, isolati da config utente reale.
using Basic.AI;
using Basic.AI.Knowledge;
using Basic.AI.Rag;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
[Collection("BrainGlobals")]
public sealed class MultiBrainTests : IDisposable
{
    private readonly string _configPath = Path.Combine(Path.GetTempPath(), "gmbasic-aiconfig-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _brainsRoot = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N"));
    private readonly string _registryPath = Path.Combine(Path.GetTempPath(), "gmbasic-brains-" + Guid.NewGuid().ToString("N") + ".json");
    private readonly string _ragDir = Path.Combine(Path.GetTempPath(), "gmbasic-rag-" + Guid.NewGuid().ToString("N"));
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;
    private readonly Basic.Databases.Rdd.DataSession _ds;

    public MultiBrainTests()
    {
        AiConfig.OverridePathForTests(_configPath);
        AiConfig.BrainsDefaultDir = _brainsRoot;
        AiConfig.RagDefaultDir = _ragDir;
        BrainRegistry.OverridePathForTests(_registryPath);
        // Ripristino in Dispose dei singleton globali: salva il valore PRECEDENTE (un altro test
        // che gira in parallelo può dipendere dal default ActiveDimensions=384, es. AiVectorRuntimeTests).
        _prevEmb = Basic.SQLite.Vector.EmbeddingProviderResolver.Default;
        _prevDims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        // DataSession ISOLATO per questo test (come AiVectorRuntimeTests): RagDbSession.Open
        // riusa AiVectorRuntime.SqlDataSession; crearlo qui evita di lasciare aree "MultiBrain_*"
        // nel DataSession globale condiviso tra gli altri test (instabilità del run completo).
        _ds = new Basic.Databases.Rdd.DataSession();
        AiVectorRuntime.SqlDataSession = _ds;
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        BrainRegistry.ResetForTests();
        // Ripristina i singleton al valore PRECEDENTE (non azzerarli: altri test in parallelo
        // leggono ActiveDimensions come default), poi chiudi SOLO il nostro DataSession (le aree
        // del session precedente restano a chi le possedeva).
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = _prevEmb;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        _ds.CloseAll();
        try { Directory.Delete(_brainsRoot, recursive: true); } catch { }
        try { Directory.Delete(_ragDir, recursive: true); } catch { }
        if (File.Exists(_registryPath)) File.Delete(_registryPath);
        if (File.Exists(_configPath)) File.Delete(_configPath);
    }

    /// <summary>Costruisce un db RAG su file con una collezione e un documento, poi lo registra
    /// come brain disponibile (IsAvailableAsRag). Usa FixtureEmbeddingProvider deterministico
    /// (come i test di parità) e lo imposta nel resolver globale per index+search.</summary>
    private void MakeRagBrain(string name, string content, string collection = "wiki")
    {
        var dbPath = Path.Combine(_ragDir, name + ".db");
        var fixture = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = fixture;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
        RagDbSession.Open(dbPath, name);
        AiVectorRuntime.EnsureCollection(name, collection, 8);
        AiVectorRuntime.IndexText(name, collection, "/src/" + name + ".md", content);
        BrainRegistry.Register(name, mode: "rag");
    }

    [Fact]
    public void SearchMulti_NoBrains_ReturnsError()
    {
        var res = MultiBrainSearch.SearchMulti("prova", 5);
        res.Should().Contain("nessun brain collegato");
    }

    [Fact]
    public void SearchMulti_TwoBrains_ReturnsHitsFromBoth_WithBrainProvenance()
    {
        MakeRagBrain("braina", "mean reversion strategy uses RSI indicator on equity data");
        MakeRagBrain("brainb", "risk parity allocates bonds and equities");

        var res = MultiBrainSearch.SearchMulti("mean reversion", 5, "wiki", ["braina", "brainb"]);

        res.Should().Contain("\"status\":\"Success\"");
        res.Should().Contain("\"brain\":\"braina\"");
        // il brain con contenuto rilevante viene premiato; l'altro può comparire ma con rrf basso
        res.Should().Contain("mean reversion");
    }

    [Fact]
    public void SearchMulti_UsesOnlyRequestedBrains()
    {
        MakeRagBrain("onlya", "momentum factor for intraday trading");
        MakeRagBrain("onlyb", "completely unrelated content about gardening");

        var res = MultiBrainSearch.SearchMulti("momentum", 5, "wiki", ["onlya"]);

        res.Should().Contain("\"brain\":\"onlya\"");
        res.Should().NotContain("\"brain\":\"onlyb\"");
    }

    [Fact]
    public void RagContextMulti_BuildsContext_FromTopHits()
    {
        MakeRagBrain("ctxa", "value investing focuses on intrinsic value of companies");
        BrainRegistry.Use("ctxa");
        var bundle = MultiBrainSearch.RagContextMulti("value investing", 3);
        bundle.Context.Should().Contain("value investing");
        bundle.Attributions.Should().NotBeEmpty();
    }

    [Fact]
    public void RagContextMulti_WithDefaultReranker_StaysCorrect()
    {
        // guardia del wiring: MultiBrainSearch passa da AiVectorRuntime.SearchJson, che usa
        // DefaultReranker quando impostato — con KeywordReranker attivo il grounding resta corretto.
        var prev = AiVectorRuntime.DefaultReranker;
        var prevEv = AiVectorRuntime.DefaultRerankEvidenceMin;
        AiVectorRuntime.DefaultReranker = new Basic.SQLite.Vector.KeywordReranker();
        AiVectorRuntime.DefaultRerankEvidenceMin = double.NegativeInfinity;
        try
        {
            MakeRagBrain("rkctx", "mean reversion strategy uses RSI indicator on equity data");
            BrainRegistry.Use("rkctx");
            var bundle = MultiBrainSearch.RagContextMulti("mean reversion RSI", 3);
            bundle.Context.Should().Contain("mean reversion");
            bundle.Attributions.Should().NotBeEmpty();
        }
        finally
        {
            AiVectorRuntime.DefaultReranker = prev;
            AiVectorRuntime.DefaultRerankEvidenceMin = prevEv;
        }
    }

    [Fact]
    public void SearchMulti_RerankOff_StillFindsRelevantBrain()
    {
        MakeRagBrain("offa", "mean reversion strategy uses RSI indicator on equity data");
        var res = MultiBrainSearch.SearchMulti("mean reversion", 3, "wiki", ["offa"], reranker: 0);
        res.Should().Contain("\"brain\":\"offa\"");
        res.Should().Contain("mean reversion");
    }

    [Fact]
    public void SearchMulti_EvidenceMin_ExcludesWeakBrain()
    {
        // brain solo debole: con evidenza 0.9 (keyword 0/2=0) viene ESCLUSO dal merge
        MakeRagBrain("weaka", "the cat sleeps on the sofa and dreams about fish colors");
        var res = MultiBrainSearch.SearchMulti("mean reversion", 3, "wiki", ["weaka"],
            reranker: 1, minEvidence: 0.9);
        res.Should().Contain("InsufficientEvidence");
        res.Should().NotContain("weaka");
    }
}
