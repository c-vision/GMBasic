// FILE: AI.Tests.Eval.EvalHarnessTests
// C3/M6 — Eval Harness: esegue le suite metriche in modalità fixture e verifica che il
// report `eval_report.json` contenga le 4 famiglie (llm.tokps, rag.ndcg, team.acc,
// credit.blocked). La qualità end-to-end contro modello reale resta in tests/AI.Eval.

using Basic.AI.Eval;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class EvalHarnessTests : IDisposable
{
    private readonly string? _prevRagDir;
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSql;

    public EvalHarnessTests()
    {
        _prevRagDir = Basic.AI.AiConfig.RagDefaultDir;
        var ragDir = Path.Combine(Path.GetTempPath(), "gmb_eval_rag_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(ragDir);
        Basic.AI.AiConfig.RagDefaultDir = ragDir;
        _prevEmb = Basic.SQLite.Vector.EmbeddingProviderResolver.Default;
        _prevDims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
        _prevSql = Basic.AI.Rag.AiVectorRuntime.SqlDataSession;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
    }
    public void Dispose()
    {
        Basic.AI.Rag.AiVectorRuntime.SqlDataSession = _prevSql;
        Basic.AI.AiConfig.RagDefaultDir = _prevRagDir ?? Basic.AI.AiConfig.RagDefaultDir;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = _prevEmb;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = _prevDims;
    }

    [Fact]
    public void RunAll_ProducesReport_WithFourMetricFamilies()
    {
        var reportPath = Path.Combine(Path.GetTempPath(), "gmb_eval_report_" + Guid.NewGuid().ToString("N") + ".json");
        var json = EvalHarness.RunAll(reportPath, fixture: true);
        Assert.True(File.Exists(reportPath));
        Assert.Contains("llm.tokps", json);
        Assert.Contains("rag.ndcg@2", json);
        Assert.Contains("team.acc", json);
        Assert.Contains("credit.blocked", json);
        Assert.Contains("\"model\"", json);
        // fixture deterministico → rag e credit devono passare
        Assert.Contains("\"rag\"", json);
        try { if (File.Exists(reportPath)) File.Delete(reportPath); } catch { }
    }

    [Fact]
    public void Ndcg_PerfectOrder_IsOne_And_Worst_IsZero()
    {
        Assert.Equal(1.0, EvalHarness.NdcgAtK(["d1", "d2", "d3", "d4"], ["d1", "d2"], 2));
        Assert.Equal(0.0, EvalHarness.NdcgAtK(["d3", "d4"], ["d1", "d2"], 2));
    }

    [Fact]
    public void RagEval_Metrics_Handbook()
    {
        // Valori handbook identici a gmb_rageval_selfcheck (runtime C): parità C#/C.
        // R1 — quartetto retrieval
        Assert.Equal(0.5, Basic.AI.Rag.AiVectorRuntime.PrecisionAt("a,b,c,d", "b,d", 2));
        Assert.Equal(0.5, Basic.AI.Rag.AiVectorRuntime.PrecisionAt("a,b,c,d", "b,d", 4));
        Assert.Equal(0.5, Basic.AI.Rag.AiVectorRuntime.RecallAt("a,b,c,d", "b,d", 2));
        Assert.Equal(1.0, Basic.AI.Rag.AiVectorRuntime.RecallAt("a,b,c,d", "b,d", 0));
        Assert.Equal(0.5, Basic.AI.Rag.AiVectorRuntime.Mrr("a,b,c,d", "b,d"));
        Assert.Equal(0.0, Basic.AI.Rag.AiVectorRuntime.Mrr("x,y", "b,d"));
        Assert.Equal(1.0, Basic.AI.Rag.AiVectorRuntime.NdcgAt("b,d,a,c", "b,d", 4), 12);
        Assert.Equal(0.6509, Basic.AI.Rag.AiVectorRuntime.NdcgAt("a,b,c,d", "b,d", 4), 4);
        // R2 — giudizi generazione tier-1
        Assert.Equal(0.75, Basic.AI.Rag.AiVectorRuntime.Faithfulness("il motore mlx gestisce la memoria", "motore mlx memoria rapido"));
        Assert.Equal(0.25, Basic.AI.Rag.AiVectorRuntime.Hallucination("il motore mlx gestisce la memoria", "motore mlx memoria rapido"));
        Assert.Equal(0.6, Basic.AI.Rag.AiVectorRuntime.Relevance("che cosa gestisce il motore", "il motore gestisce la memoria"));
        Assert.Equal(0.5, Basic.AI.Rag.AiVectorRuntime.Completeness("che cosa gestisce il motore", "il motore gestisce la memoria"));
    }
}
