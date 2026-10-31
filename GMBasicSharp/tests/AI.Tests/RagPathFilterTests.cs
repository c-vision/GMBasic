// FILE: AI.Tests.RagPathFilterTests
// L4: PATH_FILTER/TAG_FILTER/DATE_RANGE via filter JSON di AI.SEARCH.
// Usa un db RAG reale su file (fixture embedding deterministico) isolato.
using Basic.AI;
using Basic.AI.Rag;
using Basic.SQLite.Vector;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RagPathFilterTests : IDisposable
{
    private readonly string _tmp;
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSqlSession;
    private readonly Basic.Databases.Rdd.DataSession _ds;
    private const string Alias = "ragfilter-test";
    private const string Coll = "docs";

    public RagPathFilterTests()
    {
        _tmp = Path.Combine(Path.GetTempPath(), "gmb-ragfilter-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_tmp);
        AiConfig.OverridePathForTests(Path.Combine(_tmp, "ai.json"));
        // Salva i singleton globali per ripristinarli in Dispose: altri test in parallelo
        // (es. AiVectorRuntimeTests) leggono ActiveDimensions col default 384.
        _prevEmb = EmbeddingProviderResolver.Default;
        _prevDims = EmbeddingProviderResolver.ActiveDimensions;
        _prevSqlSession = AiVectorRuntime.SqlDataSession;
        EmbeddingProviderResolver.Default = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.ActiveDimensions = 8;
        _ds = new Basic.Databases.Rdd.DataSession();
        AiVectorRuntime.SqlDataSession = _ds;
        var dbPath = Path.Combine(_tmp, "test.db");
        RagDbSession.Open(dbPath, Alias);
        AiVectorRuntime.EnsureCollection(Alias, Coll, 8);
        AiVectorRuntime.IndexTextWithMetadata(Alias, Coll, "/wiki/quant/strat.md",
            "mean reversion with RSI on equities", "{\"tags\":[\"strategy\",\"quant\"],\"date\":\"2024-01-15\"}");
        AiVectorRuntime.IndexTextWithMetadata(Alias, Coll, "/wiki/risk/parity.md",
            "risk parity allocates bonds and equities", "{\"tags\":[\"portfolio\"],\"date\":\"2023-06-01\"}");
    }

    public void Dispose()
    {
        AiConfig.ResetForTests();
        // Ripristina i singleton al valore PRECEDENTE (non azzerarli: AiVectorRuntimeTests
        // dipende dal default ActiveDimensions=384 in esecuzione parallela), poi chiudi SOLO il
        // nostro DataSession (le aree del session precedente restano a chi le possedeva).
        EmbeddingProviderResolver.Default = _prevEmb;
        EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        AiVectorRuntime.SqlDataSession = _prevSqlSession;
        _ds.CloseAll();
        try { Directory.Delete(_tmp, recursive: true); } catch { }
    }

    [Fact]
    public void PathFilter_KeepsOnlyMatchingFolder()
    {
        var res = AiVectorRuntime.SearchJson(Alias, Coll, "mean reversion", 5, hybrid: true,
            "{\"path\":\"/wiki/quant/\"}");
        Assert.Contains("/wiki/quant/strat.md", res);
        Assert.DoesNotContain("/wiki/risk/parity.md", res);
    }

    [Fact]
    public void TagFilter_KeepsOnlyMatchingTag()
    {
        var res = AiVectorRuntime.SearchJson(Alias, Coll, "bonds", 5, hybrid: true,
            "{\"tag\":\"portfolio\"}");
        Assert.Contains("/wiki/risk/parity.md", res);
        Assert.DoesNotContain("/wiki/quant/strat.md", res);
    }

    [Fact]
    public void DateRange_Filters()
    {
        var res = AiVectorRuntime.SearchJson(Alias, Coll, "equities", 5, hybrid: true,
            "{\"minDate\":\"2024-01-01\"}");
        Assert.Contains("/wiki/quant/strat.md", res);
        Assert.DoesNotContain("/wiki/risk/parity.md", res);
    }
}
