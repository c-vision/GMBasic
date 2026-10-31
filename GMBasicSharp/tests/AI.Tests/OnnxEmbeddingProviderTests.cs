// FILE: Sqlite.Tests.Vector.OnnxEmbeddingProviderTests

using Basic.SQLite.Vector;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class OnnxEmbeddingProviderTests
{
    [Fact(DisplayName = "OnnxEmbed skipped when GMBASIC_ONNX_MODEL unset")]
    public void Embed_WhenModelEnvUnset_ReturnsWithoutRunning()
    {
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_ONNX_MODEL");
        if (!string.IsNullOrWhiteSpace(modelPath))
            return; // ponytail: other agents/CI with model set — run real path below

        var act = () => new OnnxEmbeddingProvider("missing.onnx", "test-v1", 384);
        act.Should().Throw<FileNotFoundException>();
    }

    [Fact(DisplayName = "OnnxEmbed real model when GMBASIC_ONNX_MODEL set")]
    public void Embed_WhenModelEnvSet_ProducesNormalizedVector()
    {
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_ONNX_MODEL");
        if (string.IsNullOrWhiteSpace(modelPath))
            return; // ponytail: graceful skip — no model bundled in repo

        var dimsText = Environment.GetEnvironmentVariable("GMBASIC_ONNX_DIMS");
        var dimensions = int.TryParse(dimsText, out var parsed) ? parsed : 384;
        var vocabPath = Environment.GetEnvironmentVariable("GMBASIC_ONNX_VOCAB");
        var version = Environment.GetEnvironmentVariable("GMBASIC_ONNX_VERSION") ?? "onnx-test-v1";

        using var provider = string.IsNullOrWhiteSpace(vocabPath)
            ? new OnnxEmbeddingProvider(modelPath, version, dimensions)
            : new OnnxEmbeddingProvider(modelPath, version, dimensions, vocabPath);

        var vector = provider.Embed("vector cosine similarity for semantic retrieval", dimensions);

        vector.Should().HaveCount(dimensions);
        vector.Should().AllSatisfy(v => float.IsFinite(v).Should().BeTrue());
        var norm = Math.Sqrt(vector.Sum(v => v * v));
        norm.Should().BeApproximately(1f, 0.01f);
    }
}
