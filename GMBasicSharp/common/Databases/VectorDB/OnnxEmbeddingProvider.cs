// FILE: Basic.SQLite.Vector.OnnxEmbeddingProvider
using Microsoft.ML.OnnxRuntime;
using Microsoft.ML.OnnxRuntime.Tensors;

namespace Basic.SQLite.Vector;

/// <summary>ONNX Runtime embedding provider for sentence-transformer style models.</summary>
/// <remarks>
/// ponytail: assumes BERT WordPiece vocab (vocab.txt beside model or explicit path),
/// uncased tokenization, mean pooling over last_hidden_state (not CLS). Upgrade path: model-specific tokenizer hook.
/// </remarks>
public sealed class OnnxEmbeddingProvider : IEmbeddingProvider, IDisposable
{
    private const int DefaultMaxSequenceLength = 128;

    private readonly string _modelPath;
    private readonly string _vocabPath;
    private readonly int _dimensions;
    private readonly int _maxSequenceLength;
    private readonly BertWordPieceTokenizer _tokenizer;
    private InferenceSession? _session;
    private string[]? _inputNames;
    private string? _outputName;

    public OnnxEmbeddingProvider(
        string modelPath,
        string modelVersion,
        int dimensions,
        string? vocabPath = null,
        int maxSequenceLength = DefaultMaxSequenceLength)
    {
        ArgumentException.ThrowIfNullOrEmpty(modelPath);
        ArgumentException.ThrowIfNullOrEmpty(modelVersion);
        if (dimensions <= 0)
            throw new ArgumentOutOfRangeException(nameof(dimensions));
        if (maxSequenceLength <= 0)
            throw new ArgumentOutOfRangeException(nameof(maxSequenceLength));
        if (!File.Exists(modelPath))
            throw new FileNotFoundException("ONNX model file not found.", modelPath);

        _modelPath = modelPath;
        ModelVersion = modelVersion;
        _dimensions = dimensions;
        _maxSequenceLength = maxSequenceLength;
        _vocabPath = vocabPath ?? Path.Combine(Path.GetDirectoryName(modelPath)!, "vocab.txt");
        if (!File.Exists(_vocabPath))
            throw new FileNotFoundException("BERT vocab.txt not found.", _vocabPath);

        _tokenizer = BertWordPieceTokenizer.Load(_vocabPath);
    }

    public string ModelVersion { get; }

    public float[] Embed(string text, int dimensions)
    {
        ArgumentException.ThrowIfNullOrEmpty(text);
        if (dimensions != _dimensions)
            throw new ArgumentException($"Expected { _dimensions} dimensions, got {dimensions}.");

        var session = GetSession();
        var encoded = _tokenizer.Encode(text, _maxSequenceLength);
        var inputs = BuildInputs(session, encoded);
        using var results = session.Run(inputs);

        var output = results.First(r => r.Name == _outputName).AsEnumerable<float>().ToArray();
        var vector = output.Length == _dimensions
            ? output
            : output.Length == encoded.Length * _dimensions
                ? MeanPool(output, encoded.AttentionMask, encoded.Length, _dimensions)
                : throw new InvalidOperationException(
                    $"Unexpected ONNX output length {output.Length}; expected {_dimensions} or {encoded.Length * _dimensions}.");

        VectorMath.Normalize(vector);
        return vector;
    }

    public void Dispose() => _session?.Dispose();

    private InferenceSession GetSession()
    {
        if (_session != null)
            return _session;

        _session = new InferenceSession(_modelPath);
        _inputNames = _session.InputMetadata.Keys.ToArray();
        _outputName = _session.OutputMetadata.Keys.FirstOrDefault(k =>
                          k.Contains("sentence", StringComparison.OrdinalIgnoreCase) ||
                          k.Contains("embedding", StringComparison.OrdinalIgnoreCase) ||
                          k.Equals("output", StringComparison.OrdinalIgnoreCase))
                      ?? _session.OutputMetadata.Keys.First();
        return _session;
    }

    private List<NamedOnnxValue> BuildInputs(InferenceSession session, EncodedSequence encoded)
    {
        var batch = 1;
        var seqLen = encoded.Length;
        var inputs = new List<NamedOnnxValue>(_inputNames!.Length);

        foreach (var name in _inputNames!)
        {
            if (name.Contains("input_ids", StringComparison.OrdinalIgnoreCase))
            {
                var tensor = new DenseTensor<long>([batch, seqLen]);
                for (var i = 0; i < seqLen; i++)
                    tensor[0, i] = encoded.InputIds[i];
                inputs.Add(NamedOnnxValue.CreateFromTensor(name, tensor));
            }
            else if (name.Contains("attention_mask", StringComparison.OrdinalIgnoreCase))
            {
                var tensor = new DenseTensor<long>([batch, seqLen]);
                for (var i = 0; i < seqLen; i++)
                    tensor[0, i] = encoded.AttentionMask[i];
                inputs.Add(NamedOnnxValue.CreateFromTensor(name, tensor));
            }
            else if (name.Contains("token_type", StringComparison.OrdinalIgnoreCase))
            {
                var tensor = new DenseTensor<long>([batch, seqLen]);
                inputs.Add(NamedOnnxValue.CreateFromTensor(name, tensor));
            }
        }

        return inputs;
    }

    private static float[] MeanPool(ReadOnlySpan<float> hidden, ReadOnlySpan<long> mask, int seqLen, int hiddenDim)
    {
        var pooled = new float[hiddenDim];
        var count = 0f;
        for (var t = 0; t < seqLen; t++)
        {
            if (mask[t] == 0)
                continue;
            count++;
            var offset = t * hiddenDim;
            for (var h = 0; h < hiddenDim; h++)
                pooled[h] += hidden[offset + h];
        }

        if (count > 0)
        {
            for (var h = 0; h < hiddenDim; h++)
                pooled[h] /= count;
        }

        return pooled;
    }
}
