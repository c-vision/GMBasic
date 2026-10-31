// FILE: Basic.SQLite.Vector.OnnxCrossEncoderReranker
using Microsoft.ML.OnnxRuntime;
using Microsoft.ML.OnnxRuntime.Tensors;

namespace Basic.SQLite.Vector;

/// <summary>Real cross-encoder reranker via ONNX Runtime — encodes <c>(query, document)</c> as a
/// single BERT sentence-pair sequence and reads a single relevance logit from the model output.</summary>
/// <remarks>
/// ponytail: CPU-only inference (plain <see cref="InferenceSession"/> ctor, no GPU execution
/// provider configured — same as <see cref="OnnxEmbeddingProvider"/>). This is deliberate: it
/// means the model's memory footprint is system RAM, not VRAM, so it runs identically on a
/// machine with no dedicated GPU or as little as 2GB of VRAM. Default target model is
/// cross-encoder/ms-marco-MiniLM-L-6-v2 (~90MB fp32 ONNX export, 6-layer, well-established
/// accuracy/latency tradeoff for reranking small candidate sets) — any BERT-family cross-encoder
/// exported to ONNX with a matching vocab.txt works the same way (config-driven, not hardcoded).
/// </remarks>
public sealed class OnnxCrossEncoderReranker : IReranker, IDisposable
{
    private const int DefaultMaxSequenceLength = 256;

    private readonly string _modelPath;
    private readonly int _maxSequenceLength;
    private readonly BertWordPieceTokenizer _tokenizer;
    private InferenceSession? _session;
    private string[]? _inputNames;
    private string? _outputName;

    public OnnxCrossEncoderReranker(
        string modelPath,
        string modelVersion,
        string? vocabPath = null,
        int maxSequenceLength = DefaultMaxSequenceLength)
    {
        ArgumentException.ThrowIfNullOrEmpty(modelPath);
        ArgumentException.ThrowIfNullOrEmpty(modelVersion);
        if (maxSequenceLength <= 0)
            throw new ArgumentOutOfRangeException(nameof(maxSequenceLength));
        if (!File.Exists(modelPath))
            throw new FileNotFoundException("ONNX reranker model file not found.", modelPath);

        _modelPath = modelPath;
        ModelName = modelVersion;
        _maxSequenceLength = maxSequenceLength;
        var resolvedVocabPath = vocabPath ?? Path.Combine(Path.GetDirectoryName(modelPath)!, "vocab.txt");
        if (!File.Exists(resolvedVocabPath))
            throw new FileNotFoundException("BERT vocab.txt not found.", resolvedVocabPath);

        _tokenizer = BertWordPieceTokenizer.Load(resolvedVocabPath);
    }

    public string ModelName { get; }

    public float Score(string query, string document)
    {
        ArgumentException.ThrowIfNullOrEmpty(query);
        ArgumentNullException.ThrowIfNull(document);

        var session = GetSession();
        var encoded = _tokenizer.EncodePair(query, document, _maxSequenceLength);
        var inputs = BuildInputs(session, encoded);
        using var results = session.Run(inputs);

        var output = results.First(r => r.Name == _outputName).AsEnumerable<float>().ToArray();
        var logit = output.Length switch
        {
            0 => throw new InvalidOperationException("ONNX reranker returned an empty output."),
            1 => output[0],
            // Some cross-encoder exports emit a 2-class [not_relevant, relevant] softmax input
            // instead of a single logit — the second class is the relevance signal.
            2 => output[1],
            _ => output[0]
        };

        return Sigmoid(logit);
    }

    public void Dispose() => _session?.Dispose();

    private static float Sigmoid(float x) => 1f / (1f + MathF.Exp(-x));

    private InferenceSession GetSession()
    {
        if (_session != null)
            return _session;

        _session = new InferenceSession(_modelPath);
        _inputNames = _session.InputMetadata.Keys.ToArray();
        _outputName = _session.OutputMetadata.Keys.FirstOrDefault(k =>
                          k.Contains("logit", StringComparison.OrdinalIgnoreCase) ||
                          k.Contains("score", StringComparison.OrdinalIgnoreCase) ||
                          k.Equals("output", StringComparison.OrdinalIgnoreCase))
                      ?? _session.OutputMetadata.Keys.First();
        return _session;
    }

    private List<NamedOnnxValue> BuildInputs(InferenceSession session, EncodedSequence encoded)
    {
        var seqLen = encoded.Length;
        var inputs = new List<NamedOnnxValue>(_inputNames!.Length);

        foreach (var name in _inputNames!)
        {
            if (name.Contains("input_ids", StringComparison.OrdinalIgnoreCase))
            {
                var tensor = new DenseTensor<long>([1, seqLen]);
                for (var i = 0; i < seqLen; i++)
                    tensor[0, i] = encoded.InputIds[i];
                inputs.Add(NamedOnnxValue.CreateFromTensor(name, tensor));
            }
            else if (name.Contains("attention_mask", StringComparison.OrdinalIgnoreCase))
            {
                var tensor = new DenseTensor<long>([1, seqLen]);
                for (var i = 0; i < seqLen; i++)
                    tensor[0, i] = encoded.AttentionMask[i];
                inputs.Add(NamedOnnxValue.CreateFromTensor(name, tensor));
            }
            else if (name.Contains("token_type", StringComparison.OrdinalIgnoreCase))
            {
                var tensor = new DenseTensor<long>([1, seqLen]);
                for (var i = 0; i < seqLen; i++)
                    tensor[0, i] = encoded.TokenTypeIds[i];
                inputs.Add(NamedOnnxValue.CreateFromTensor(name, tensor));
            }
        }

        return inputs;
    }
}
