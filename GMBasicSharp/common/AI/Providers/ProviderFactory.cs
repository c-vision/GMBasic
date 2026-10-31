// FILE: Basic.Providers.ProviderFactory
using Basic.SQLite.Vector;
using Basic.AI.Rag;

namespace Basic.Providers;

internal static class ProviderFactory
{
    public static IEmbeddingProvider CreateEmbedding(ProviderDescriptor descriptor) =>
        descriptor.Kind switch
        {
            "fixture" => new FixtureEmbeddingProvider(),
            "onnx" => CreateOnnxEmbedding(descriptor),
            "ollama" => new OllamaEmbeddingProvider(descriptor),
            "openai" => new OpenAiEmbeddingProvider(descriptor),
            // Completer locali con latent: l'embedding viene dal motore del completer stesso
            // (gmb_ai_* embed) — zero modello aggiuntivo.
            "llamacpp" or "mlx" => CreateLatentEmbedding(descriptor),
            _ => throw new InvalidOperationException(
                $"Embedding provider kind '{descriptor.Kind}' is not supported.")
        };

    private static IEmbeddingProvider CreateLatentEmbedding(ProviderDescriptor descriptor)
    {
        var completer = CreateCompletion(descriptor);
        if (completer is ILatentCompleter latent)
            return new LlmLatentEmbeddingProvider(latent, descriptor.Name);
        throw new InvalidOperationException(
            $"Embedding provider kind '{descriptor.Kind}' does not expose latent embeddings.");
    }

    public static ILlmCompleter CreateCompletion(ProviderDescriptor descriptor) =>
        descriptor.Kind switch
        {
            "fixture" => new FixtureLlmCompleter(),
            "ollama" => new OllamaLlmCompleter(descriptor),
            "openai" => new OpenAiLlmCompleter(descriptor),
            "anthropic" => new AnthropicLlmCompleter(descriptor),
            "llamacpp" => new LlamaCppLlmCompleter(descriptor),
            "llamacpp-vision" => new LlamaCppVisionCompleter(descriptor),
            "llamacpp-audio" => new LlamaCppAudioCompleter(descriptor),
            "apple-vision" => new AppleVisionCompleter(descriptor),
            "mlx" => new MlxLlmCompleter(descriptor),
            _ => throw new InvalidOperationException(
                $"Completion provider kind '{descriptor.Kind}' is not supported.")
        };

    private static IEmbeddingProvider CreateOnnxEmbedding(ProviderDescriptor descriptor)
    {
        var model = descriptor.GetConfig("model")
                    ?? throw new InvalidOperationException(
                        $"ONNX provider '{descriptor.Name}' requires config key 'model'.");
        var version = descriptor.GetConfig("modelVersion") ?? "onnx-v1";
        var dims = int.TryParse(descriptor.GetConfig("dimensions"), out var d) ? d : 384;
        var vocab = descriptor.GetConfig("vocab");
        // maxSequenceLength: 128 = DefaultMaxSequenceLength di OnnxEmbeddingProvider. Va passato
        // ESPLICITO perché Activator.CreateInstance(Type, object?[]) NON applica i default dei
        // parametri del ctor (con 4 args contro un ctor a 5 parametri opzionali: MissingMethodException
        // — regressione introdotta dal passaggio a reflection del decoupling NativeAOT §9.7, era
        // `new OnnxEmbeddingProvider(...)` che applicava i default). Config opzionale "maxSequenceLength".
        var maxSeq = int.TryParse(descriptor.GetConfig("maxSequenceLength"), out var ms) && ms > 0 ? ms : 128;
        return (IEmbeddingProvider)CreateOnnx("Basic.SQLite.Vector.OnnxEmbeddingProvider",
            [model, version, dims, vocab, maxSeq]);
    }

    public static IReranker CreateReranker(ProviderDescriptor descriptor) =>
        descriptor.Kind switch
        {
            "keyword" => new KeywordReranker(),
            "onnx" => CreateOnnxReranker(descriptor),
            _ => throw new InvalidOperationException(
                $"Reranker kind '{descriptor.Kind}' is not supported.")
        };

    private static IReranker CreateOnnxReranker(ProviderDescriptor descriptor)
    {
        var model = descriptor.GetConfig("model")
                    ?? throw new InvalidOperationException(
                        $"ONNX reranker '{descriptor.Name}' requires config key 'model'.");
        var version = descriptor.GetConfig("modelVersion") ?? "onnx-reranker-v1";
        var vocab = descriptor.GetConfig("vocab");
        // maxSequenceLength esplicito (256 = DefaultMaxSequenceLength del reranker): vedi
        // CreateOnnxEmbedding — Activator non applica i default, con 3 args era MissingMethodException.
        var maxSeq = int.TryParse(descriptor.GetConfig("maxSequenceLength"), out var ms) && ms > 0 ? ms : 256;
        return (IReranker)CreateOnnx("Basic.SQLite.Vector.OnnxCrossEncoderReranker",
            [model, version, vocab, maxSeq]);
    }

    /// <summary>Istanzia LAZY per nome i provider ONNX (NativeAOT §9.7 passo 2): AI NON referenzia
    /// Databases.Onnx (OnnxRuntime fuori dal grafo di chi non usa embedding/rerank ONNX). Il tipo è
    /// in "Databases.Onnx" distribuito accanto al binario SOLO da chi lo serve; assente → errore
    /// esplicito, mai crash silenzioso.</summary>
    private static object CreateOnnx(string typeFullName, object?[] ctorArgs)
    {
        var t = Type.GetType($"{typeFullName}, Databases.Onnx", throwOnError: false);
        if (t is null)
        {
            try
            {
                var asm = System.Reflection.Assembly.Load(new System.Reflection.AssemblyName("Databases.Onnx"));
                t = asm.GetType(typeFullName, throwOnError: false);
            }
            catch { t = null; }
        }
        if (t is null)
            throw new InvalidOperationException(
                $"Provider ONNX '{typeFullName}' non disponibile: l'assembly 'Databases.Onnx' non è accanto al " +
                $"binario (NativeAOT lazy — distribuisci Databases.Onnx.dll + Microsoft.ML.OnnxRuntime per usare embedding/rerank ONNX).");
        // Activator incapsula le eccezioni del ctor in TargetInvocationException: si scarta per
        // propagare l'eccezione REALE (FileNotFoundException del modello, ArgumentNull, ...) come
        // faceva il vecchio `new` diretto — i caller (e i test) matchano sul tipo concreto.
        try
        {
            return System.Activator.CreateInstance(t, ctorArgs)
                   ?? throw new InvalidOperationException($"Provider ONNX '{typeFullName}' non istanziabile.");
        }
        catch (System.Reflection.TargetInvocationException tie) when (tie.InnerException is not null)
        {
            System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(tie.InnerException).Throw();
            throw;   // unreachable
        }
    }
}
