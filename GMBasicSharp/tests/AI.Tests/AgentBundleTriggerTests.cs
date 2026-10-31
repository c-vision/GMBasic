// FILE: AI.Tests.AgentBundleTriggerTests
using Basic.Agents;
using Basic.AI.Knowledge;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentBundleTriggerTests : IDisposable
{
    private readonly string _tmp = Path.Combine(Path.GetTempPath(), "gmb_bt_" + Guid.NewGuid().ToString("N"));
    private readonly string? _prevRagDir;
    private readonly Basic.SQLite.Vector.IEmbeddingProvider? _prevEmb;
    private readonly int _prevDims;
    private readonly Basic.Databases.Rdd.DataSession? _prevSql;
    private readonly string _config = Path.Combine(Path.GetTempPath(), "gmb_bt_" + Guid.NewGuid().ToString("N") + ".json");

    public AgentBundleTriggerTests()
    {
        Basic.AI.AiConfig.OverridePathForTests(_config);
        Directory.CreateDirectory(_tmp);
        _prevRagDir = Basic.AI.AiConfig.RagDefaultDir;
        Basic.AI.AiConfig.RagDefaultDir = _tmp;
        _prevEmb = Basic.SQLite.Vector.EmbeddingProviderResolver.Default;
        _prevDims = Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions;
        _prevSql = Basic.AI.Rag.AiVectorRuntime.SqlDataSession;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = new Basic.SQLite.Vector.FixtureEmbeddingProvider();
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = 8;
        AgentServer.ResetForTests();
        AgentInsertTrigger.Clear();
    }
    public void Dispose()
    {
        AgentServer.ResetForTests();
        AgentInsertTrigger.Clear();
        Basic.AI.Rag.AiVectorRuntime.SqlDataSession = _prevSql;
        Basic.AI.AiConfig.RagDefaultDir = _prevRagDir ?? Basic.AI.AiConfig.RagDefaultDir;
        Basic.SQLite.Vector.EmbeddingProviderResolver.Default = _prevEmb;
        Basic.SQLite.Vector.EmbeddingProviderResolver.ActiveDimensions = _prevDims;
        Basic.AI.AiConfig.ResetForTests();
        try { Directory.Delete(_tmp, true); } catch { }
        if (File.Exists(_config)) File.Delete(_config);
    }

    [Fact]
    public void Bundle_ExportImport_RoundTrip()
    {
        AgentServer.Instance.Create("bundle-agent");
        AgentServer.Instance.Set("bundle-agent", "systemPrompt", "hi bundle");
        AgentServer.Instance.List().First(a => a.Name == "bundle-agent").History.Add(new AgentChatMessage("user", "saved"));
        var path = Path.Combine(_tmp, "agent.bagent");
        AgentBundle.Export("bundle-agent", path);
        Assert.True(File.Exists(path));
        var raw = File.ReadAllText(path);
        Assert.Contains("\"gmb-bagent\"", raw);

        AgentServer.Instance.Delete("bundle-agent");
        var name = AgentBundle.Import(path);
        Assert.Equal("bundle-agent", name);
        var restored = AgentServer.Instance.List().First(a => a.Name == "bundle-agent");
        Assert.Equal("hi bundle", restored.SystemPrompt);
        Assert.Contains(restored.History, m => m.Content == "saved");
    }

    [Fact]
    public void InsertTrigger_FiresAgent_OnIndex()
    {
        var alias = "kb" + System.Guid.NewGuid().ToString("N").Substring(0,6);
        AgentServer.Instance.Create("indexer-agent");
        AgentInsertTrigger.Register(alias, "docs", "indexer-agent", "Elabora {sourceId}: {text}");
        Assert.NotNull(AgentInsertTrigger.Get(alias, "docs"));

        // index che apre la sessione sotto alias kb
        var ragDir = Basic.AI.AiConfig.RagDefaultDir;
        Basic.AI.Rag.RagDbSession.Open(Path.Combine(ragDir, "rag.db"), alias);
        Basic.AI.Rag.AiVectorRuntime.EnsureCollection(alias, "docs", 8);
        var outcome = Basic.AI.Rag.AiVectorRuntime.IndexTextWithMetadata(alias, "docs", "doc1", "contenuto del trigger", "{}");
        Assert.True(outcome is Basic.SQLite.Vector.IndexTextOutcome.Indexed or Basic.SQLite.Vector.IndexTextOutcome.Updated);
        Assert.True(AgentInsertTrigger.Fired(alias, "docs") >= 1);
        // attesa breve: il fire è su un task in background che chiama AgentServer.Ask (fixture)
        System.Threading.Thread.Sleep(200);
        var agent = AgentServer.Instance.List().First(a => a.Name == "indexer-agent");
        Assert.True(agent.History.Count >= 2, "l'agente deve aver ricevuto il prompt del trigger (user+assistant)");
    }
}
