// FILE: AI.Tests.RagBuildTests
// Test del build RAG: validazione provider embedding, diff/resume, rifiuto fixture.
// Il build REALE (con embedding vero) non è testato qui — richiede un provider locale
// (llamacpp/mlx) e sta nei test env-gated. Qui si verifica la struttura: BuildState,
// frontmatter→metadata, validazione provider, idempotenza del diff.
using Basic.AI.Graph;
using Basic.AI.Rag;
using Basic.Providers;
using Basic.SQLite.Vector;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class RagBuildTests : IDisposable
{
    private readonly string _tmp;
    public RagBuildTests()
    {
        _tmp = Path.Combine(Path.GetTempPath(), "gmb-ragbuild-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(_tmp);
    }
    public void Dispose() { try { Directory.Delete(_tmp, recursive: true); } catch { } }

    private string Write(string rel, string content)
    {
        var p = Path.Combine(_tmp, rel.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(p)!);
        File.WriteAllText(p, content);
        return p;
    }

    [Fact(DisplayName = "RagIndexService.EnumerateFiles: solo estensioni supportate, esclude immagini")]
    public void RagIndexService_Enumerates_Supported_Extensions()
    {
        Write("a.md", "# A");
        Write("b.txt", "b");
        Write("img.jpg", "binary");
        Write("c.pdf", "pdf");
        var files = RagIndexService.EnumerateFiles(_tmp);
        files.Should().Contain("a.md").And.Contain("b.txt").And.Contain("c.pdf");
        files.Should().NotContain(f => f.EndsWith(".jpg"));
    }

    [Fact(DisplayName = "ValidateEmbeddingProvider: rifiuta il fixture con errore esplicito")]
    public void ValidateEmbeddingProvider_Rejects_Fixture()
    {
        var prevEmb = EmbeddingProviderResolver.Default;
        var prevDims = EmbeddingProviderResolver.ActiveDimensions;
        var fixture = new FixtureEmbeddingProvider();
        EmbeddingProviderResolver.Default = fixture;
        EmbeddingProviderResolver.ActiveDimensions = 8;
        try
        {
            var act = () => RagIndexService.ValidateEmbeddingProvider();
            act.Should().Throw<InvalidOperationException>().WithMessage("*FIXTURE*");
        }
        finally
        {
            // Ripristina i precedenti (non azzerarli: test in parallelo leggono il default globale).
            EmbeddingProviderResolver.Default = prevEmb;
            EmbeddingProviderResolver.ActiveDimensions = prevDims;
        }
    }

    [Fact(DisplayName = "Frontmatter → metadata JSON: category/tags/source interrogabili")]
    public void Frontmatter_ToMetadata_Produces_Filterable_Json()
    {
        var fm = FrontmatterParser.Parse("---\ntitle: X\ncategory: Portfolio\ntags: [risk, parity]\n---\nbody");
        var meta = FrontmatterParser.ToMetadataJson(fm);
        meta.Should().Contain("Portfolio").And.Contain("risk").And.Contain("parity");
    }

    [Fact(DisplayName = "BuildState: hash invariato → skip (idempotenza diff)")]
    public void BuildState_Hash_Unchanged_Skips()
    {
        var p = Write("doc.md", "# Documento\ncontenuto");
        var state = BuildState.Load(Path.Combine(_tmp, "r.state.json"));
        var hash = BuildState.HashFile(p);
        state.MarkProcessed("doc.md", hash);

        var wiki = new Dictionary<string, string> { ["doc.md"] = BuildState.HashFile(p) };
        var diff = state.Diff(wiki);
        diff.Unchanged.Should().Contain("doc.md");
        diff.New.Should().BeEmpty();
        diff.Changed.Should().BeEmpty();
    }

    [Fact(DisplayName = "BuildState: hash diverso → CHANGED; file nuovo → NEW")]
    public void BuildState_Hash_Changed_And_New()
    {
        var p = Write("doc.md", "v1");
        var state = BuildState.Load(Path.Combine(_tmp, "r.state.json"));
        state.MarkProcessed("doc.md", BuildState.HashFile(p));

        Write("doc.md", "v2 con contenuto diverso"); // cambia hash
        Write("nuovo.md", "nuovo");
        var wiki = new Dictionary<string, string>
        {
            ["doc.md"] = BuildState.HashFile(p),
            ["nuovo.md"] = BuildState.HashFile(Path.Combine(_tmp, "nuovo.md")),
        };
        var diff = state.Diff(wiki);
        diff.Changed.Should().Contain("doc.md");
        diff.New.Should().Contain("nuovo.md");
        diff.Unchanged.Should().BeEmpty();
    }

    [Fact(DisplayName = "ResolveSecrets: env var mancante non fa fallire il Load dell'intero registry")]
    public void ResolveSecrets_MissingEnv_DoesNotThrow()
    {
        // Registry con un provider il cui apiKey referenzia un env var inesistente + un provider
        // normale. Il Load non deve fallire: il primo resta col placeholder, il secondo si carica.
        Environment.SetEnvironmentVariable("GMBASIC_TEST_EMB_FAKE", null);
        var snapshot = new ProviderRegistryPersistence.Snapshot(
            Version: 1,
            Providers: new Dictionary<string, ProviderRegistryPersistence.ProviderEntry>
            {
                ["remote-emb"] = new ProviderRegistryPersistence.ProviderEntry(
                    "embedding", "openai",
                    new Dictionary<string, string> { ["apiKey"] = "$GMBASIC_TEST_EMB_FAKE", ["model"] = "x" }),
                ["mlx-emb"] = new ProviderRegistryPersistence.ProviderEntry(
                    "embedding", "mlx",
                    new Dictionary<string, string> { ["modelDir"] = "/tmp/inexistente", ["dimensions"] = "384" }),
            },
            RoleOrder: new Dictionary<string, List<string>> { ["embedding"] = ["remote-emb", "mlx-emb"] },
            ActiveIndex: new Dictionary<string, int> { ["embedding"] = 0 });

        var path = Path.Combine(_tmp, "providers.json");
        ProviderRegistryPersistence.SaveToFile(path, snapshot);
        // Il Load non deve throware per l'env var mancante (fix 2026-08-31).
        var loaded = ProviderRegistryPersistence.LoadFromFile(path);
        loaded.Providers.Should().ContainKey("mlx-emb");
        loaded.Providers.Should().ContainKey("remote-emb");
    }
}
