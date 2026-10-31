// FILE: AI.Tests.LatentCompleterTests
// R5 — latent nativo (ILatentCompleter) su llama.cpp via llama_get_embeddings.
// Env-gated come LlamaCppProviderTests: serve GMBASIC_LLAMACPP_MODEL (GGUF) per il path
// reale; senza, i test saltano (ponytail, stessa convenzione ONNX).
using Basic.AI.Rag;
using Basic.Agents.Recursive;
using Basic.Providers;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class LatentCompleterTests
{
    private const string ModelEnv = "GMBASIC_LLAMACPP_MODEL";

    private static ILatentCompleter? LoadCompleter()
    {
        var modelPath = Environment.GetEnvironmentVariable(ModelEnv);
        if (string.IsNullOrWhiteSpace(modelPath) || !File.Exists(modelPath)) return null;
        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion, "llamacpp-latent", "llamacpp",
            new Dictionary<string, string>
            {
                ["modelPath"] = modelPath,
                ["ctxSize"] = Environment.GetEnvironmentVariable("GMBASIC_LLAMACPP_CTX") ?? "2048",
                ["maxTokens"] = "32"
            });
        var completer = ProviderFactory.CreateCompletion(descriptor);
        return completer as ILatentCompleter;
    }

    [Fact(DisplayName = "R5: EmbDim > 0 e i hidden nativi hanno la dimensione dichiarata quando GMBASIC_LLAMACPP_MODEL set")]
    public void EmbDim_And_LastHidden_HaveSameNonZeroDim_WhenModelEnvSet()
    {
        var latent = LoadCompleter();
        if (latent is null) return; // graceful skip
        using var disposable = latent as IDisposable;

        latent.EmbDim.Should().BeGreaterThan(0);
        var session = latent.CreateSession();
        try
        {
            latent.AppendMessage(session, "user", "Once upon a time");
            var reply = latent.GenerateFromSession(session, 16);
            reply.Should().NotBeNullOrWhiteSpace();

            var hidden = latent.LastHidden(session);
            hidden.Should().NotBeNull();
            hidden.Length.Should().Be(latent.EmbDim, "LastHidden must return exactly d_h floats");

            // l'embedding non deve essere tutto zero
            hidden.Any(x => x != 0).Should().BeTrue("the real hidden state must carry signal, not be all-zero");
        }
        finally
        {
            latent.DestroySession(session);
        }
    }

    [Fact(DisplayName = "R5: EmbedText restituisce un vettore reale di lunghezza EmbDim e gli embed di testi diversi differiscono")]
    public void EmbedText_ReturnsRealVector_DistinctForDifferentTexts_WhenModelEnvSet()
    {
        var latent = LoadCompleter();
        if (latent is null) return; // graceful skip
        using var disposable = latent as IDisposable;

        var e1 = latent.EmbedText("the quick brown fox");
        var e2 = latent.EmbedText("a very different sentence about aviation");
        e1.Should().NotBeNull();
        e1.Length.Should().Be(latent.EmbDim);
        e2.Should().NotBeNull();
        e2.Length.Should().Be(latent.EmbDim);

        double Dot(float[] a, float[] b) { double s = 0; for (var i = 0; i < a.Length; i++) s += a[i] * b[i]; return s; }
        var cos = Dot(e1, e2) / (Math.Sqrt(Dot(e1, e1)) * Math.Sqrt(Dot(e2, e2)) + 1e-9);
        cos.Should().BeLessThan(0.999, "two unrelated texts must not map to the exact same embedding");
    }

    [Fact(DisplayName = "R5: RecursiveTrainer con embedding nativi — la loss coseno scende con il training")]
    public void RecursiveTrainer_NativeEmbedding_LossDecreases_WhenModelEnvSet()
    {
        var latent = LoadCompleter();
        if (latent is null) return; // graceful skip
        using var disposable = latent as IDisposable;

        var dim = latent.EmbDim;
        var handle = RecursiveLink.CreateInner(dim, 32);

        // coppie con overlap (hanno coseno>0 iniziale) per dare una direzione al gradiente
        var pairs = new[] { ("alpha beta gamma", "beta gamma"), ("delta epsilon zeta", "epsilon zeta") };
        var lr = 0.05;
        double first = double.NaN, last = double.NaN;
        for (var step = 0; step < 30; step++)
        {
            var (x, y) = pairs[step % pairs.Length];
            var h = latent.EmbedText(x);
            var t = latent.EmbedText(y);
            if (h.Length != dim || t.Length != dim) return; // modello incompatibile → skip
            var loss = RecursiveLink.TrainStep(handle, Array.ConvertAll(h, x => (double)x), Array.ConvertAll(t, x => (double)x), lr);
            if (step == 0) first = loss;
            last = loss;
        }
        RecursiveLink.ResetForTests();
        last.Should().BeLessThan(first, "training on real hidden states must reduce the cosine loss (R5 gate)");
    }

    [Fact(DisplayName = "R5 (MLX): EmbDim/LastHidden/EmbedText reali via pending_h quando GMBASIC_MLX_MODEL_DIR set")]
    public void Mlx_Latent_EmbDimLastHiddenEmbedText_WhenModelEnvSet()
    {
        var modelDir = Environment.GetEnvironmentVariable("GMBASIC_MLX_MODEL_DIR");
        if (string.IsNullOrWhiteSpace(modelDir) || !Directory.Exists(modelDir)) return; // graceful skip

        var descriptor = new ProviderDescriptor(
            ProviderRole.Completion, "mlx-latent", "mlx",
            new Dictionary<string, string>
            {
                ["modelDir"] = modelDir,
                ["maxTokens"] = "16"
            });
        var completer = ProviderFactory.CreateCompletion(descriptor);
        if (completer is not ILatentCompleter latent) { (completer as IDisposable)?.Dispose(); return; }
        using var disposable = completer as IDisposable;

        latent.EmbDim.Should().BeGreaterThan(0);

        var session = latent.CreateSession();
        try
        {
            latent.AppendMessage(session, "user", "Once upon a time");
            var reply = latent.GenerateFromSession(session, 8);
            reply.Should().NotBeNullOrWhiteSpace();

            var hidden = latent.LastHidden(session);
            hidden.Length.Should().Be(latent.EmbDim);
            hidden.Any(x => x != 0).Should().BeTrue("the real MLX hidden must carry signal, not be all-zero");
        }
        finally
        {
            latent.DestroySession(session);
        }

        var e = latent.EmbedText("the quick brown fox");
        e.Length.Should().Be(latent.EmbDim);
        e.Any(x => x != 0).Should().BeTrue();
    }
}
