// FILE: Basic.SQLite.Vector.IReranker
namespace Basic.SQLite.Vector;

/// <summary>Cross-encoder-style reranker: scores a single (query, document) pair jointly —
/// higher score means more relevant. Unlike embedding similarity (two independent vectors
/// compared post-hoc), a cross-encoder sees both texts together, which is slower per-pair but
/// meaningfully more accurate for reordering an already-retrieved candidate set.</summary>
/// <remarks>ponytail: scoring only — sort/take-topK policy lives in <see cref="RerankerScoring"/>,
/// shared by every kind so it isn't duplicated per implementation.</remarks>
public interface IReranker
{
    string ModelName { get; }

    float Score(string query, string document);
}
