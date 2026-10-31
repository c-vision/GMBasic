// FILE: AI.Tests.RagEmbeddingProviderSelectionTests
// Fix 2026-09-07: EnsureEmbeddingFromProviders deve RISPETTARE la scelta esplicita dell'utente
// (activeIndex / AI SET ACTIVE) e NON sovrascriverla col primo non-fixture dell'order.
// Prima: il SetActive forzato ignorava GetActiveName → un utente su mlx-emb vedeva il build
// usare il primo non-fixture dell'order (es. bge).
using Basic.AI.Rag;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class RagEmbeddingProviderSelectionTests : IDisposable
{
    public RagEmbeddingProviderSelectionTests() => ProviderServer.ResetForTests();

    public void Dispose() => ProviderServer.ResetForTests();

    [Fact(DisplayName = "SelectEmbeddingActive rispetta l'attivo scelto dall'utente (non lo sovrascrive)")]
    public void SelectEmbedding_Respects_UserActive()
    {
        var server = ProviderServer.Instance;
        // due embedding 'emb-a' e 'emb-b' (kind fixture ma nomi che NON iniziano con 'fixture' →
        // considerati 'reali' ai fini della selezione).
        server.Register(ProviderRole.Embedding, "emb-a", "fixture");
        server.Register(ProviderRole.Embedding, "emb-b", "fixture");
        server.SetActive(ProviderRole.Embedding, "emb-b");   // l'utente sceglie emb-b

        RagDbSession.SelectEmbeddingActive(server);

        server.GetActiveName(ProviderRole.Embedding).Should().Be("emb-b",
            "la scelta esplicita dell'utente non va sovrascritta dal primo non-fixture dell'order");
    }

    [Fact(DisplayName = "SelectEmbeddingActive con attivo fixture → fallisce al primo non-fixture")]
    public void SelectEmbedding_WhenActiveIsFixture_FallsBackToReal()
    {
        var server = ProviderServer.Instance;
        server.Register(ProviderRole.Embedding, "fixture-embedding", "fixture");   // default
        server.Register(ProviderRole.Embedding, "emb-a", "fixture");
        server.SetActive(ProviderRole.Embedding, "fixture-embedding");   // attivo = fixture

        RagDbSession.SelectEmbeddingActive(server);

        server.GetActiveName(ProviderRole.Embedding).Should().NotBe("fixture-embedding",
            "il fixture non è adatto al RAG → si deve attivare un non-fixture");
    }
}
