// FILE: Basic.SQLite.Vector.IEmbeddingProvider
namespace Basic.SQLite.Vector;

/// <summary>Produces embedding vectors for text (fixture, ONNX, or remote provider).</summary>
public interface IEmbeddingProvider
{
    string ModelVersion { get; }

    float[] Embed(string text, int dimensions);

    /// <summary>Fingerprint dell'identità del MODELLO (pesi/vocab/config), se il provider sa
    /// calcolarla. VUOTA = l'indexer deriva il digest da ModelVersion + dimensioni. È la base
    /// della guardia d'integrità per-collection (fase 3, todo Tokenizer-WordPiece): un vettore
    /// scritto con un modello non può finire in una collection dichiarata con un altro.</summary>
    string ModelDigest => "";
}
