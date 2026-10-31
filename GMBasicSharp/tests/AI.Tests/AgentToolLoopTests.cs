// FILE: Mcp.Tests.AgentToolLoopTests
using System.Diagnostics;
using Basic.Agents;
using Basic.AI.Rag;
using Basic.Mcp;
using Basic.Providers;
using FluentAssertions;
using Xunit;

namespace AI.Tests;

[Trait("Category", "Fast")]
public sealed class AgentToolLoopTests : IDisposable
{
    public AgentToolLoopTests()
    {
        AgentServer.ResetForTests();
        McpToolRegistry.ResetForTests();
        McpConfig.ResetForTests();
        McpMarketplace.ResetForTests();
        ProviderServer.ResetForTests();
        McpRuntimeBridge.ResetForTests();
        McpRuntimeBridge.Dispatcher = new StubToolDispatcher("4");
    }

    public void Dispose()
    {
        AgentServer.ResetForTests();
        McpToolRegistry.ResetForTests();
        McpConfig.ResetForTests();
        McpMarketplace.ResetForTests();
        ProviderServer.ResetForTests();
        McpRuntimeBridge.ResetForTests();
    }

    [Fact]
    public void Ask_WithRegisteredTool_RunsToolLoop()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("Calc");
        AgentServer.Instance.RegisterTool("Calc", "add_numbers");

        var answer = AgentServer.Instance.Ask("Calc", "tool: add_numbers please");
        answer.Should().Contain("4");
    }

    [Fact]
    public void Ask_NativeToolCompleter_UsesNativeDispatchPath()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("Native");
        AgentServer.Instance.RegisterTool("Native", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "Native");
        var stub = new StubNativeCompleter();

        var answer = AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool");

        answer.Should().Contain("4");
        stub.DispatchCalls.Should().Be(2); // 1 tool call + 1 final answer
        agent.History.Should().Contain(m => m.Role == "tool" && m.Content.Contains("[add_numbers]"));
    }

    [Fact]
    public void Ask_WithPluginRegisteredTool_RunsToolLoop()
    {
        // Fase 3 piano AI-Plugins: un plugin operativo registra i suoi tool in McpToolRegistry
        // (via PluginRegistry.Enable) e AgentToolLoop li invoca nel round-trip.
        Basic.AI.PluginRegistry.Enable("web-fetch");
        var tool = McpToolRegistry.Instance.Get("web_fetch");
        tool.Should().NotBeNull();
        tool!.QualifiedProcedureName.Should().Be("WebFetch");

        AgentServer.Instance.Create("WebAgent");
        AgentServer.Instance.RegisterTool("WebAgent", "web_fetch");
        var agent = AgentServer.Instance.List().First(a => a.Name == "WebAgent");
        var stub = new StubPluginToolCompleter();
        var dispatcher = McpRuntimeBridge.Dispatcher!;

        var answer = AgentToolLoop.AskWithTools(agent, stub, dispatcher, "usa il tool web_fetch");

        answer.Should().Contain("42");
        agent.History.Should().Contain(m => m.Role == "tool" && m.Content.Contains("[web_fetch]"));
        var stubDispatcher = dispatcher as StubToolDispatcher;
        stubDispatcher.Should().NotBeNull();
        stubDispatcher!.LastProcedure.Should().Be("WebFetch");
    }

    [Fact]
    public void Marketplace_ClonesPluginIntoDestination()
    {
        // Fase 5 piano AI-Plugins: AI PLUGINS MARKETPLACE clona un indice di plugin
        // (repo git) nella destinazione. Verificato con un repo git locale file://.
        var baseDir = Path.Combine(Path.GetTempPath(), "gmb-mkt-" + Guid.NewGuid().ToString("N"));
        var repo = Path.Combine(baseDir, "repo");
        var pluginDir = Path.Combine(repo, "demo-web");
        Directory.CreateDirectory(pluginDir);
        File.WriteAllText(Path.Combine(pluginDir, "PLUGIN.md"),
            "---\nname: demo-web\ndescription: demo\ntrigger: demo\ntools: []\n---\n# demo\n");
        Git(repo, "init");
        Git(repo, "add", "-A");
        Git(repo, "-c", "user.email=t@t", "-c", "user.name=t", "commit", "-qm", "init");

        var dest = Path.Combine(baseDir, "dest");
        var result = Basic.AI.PluginRegistry.CloneFromIndex("file://" + repo, dest);

        result.Should().Be(1.0);
        Directory.Exists(Path.Combine(dest, "demo-web")).Should().BeTrue();
        File.Exists(Path.Combine(dest, "demo-web", "PLUGIN.md")).Should().BeTrue();
    }

    [Fact]
    public void PluginCall_InvokesRegisteredProcedure()
    {
        // Fase 6: PluginRegistry.Call trova la procedura del plugin (procedures del
        // manifest) e la invoca via CallHandler (registrato dal backend IL).
        Basic.AI.PluginRegistry.Enable("web-fetch");
        string? calledProc = null;
        object?[]? calledArgs = null;
        Basic.AI.PluginRegistry.CallHandler = (proc, args) =>
        {
            calledProc = proc;
            calledArgs = args;
            return "42";
        };

        var result = Basic.AI.PluginRegistry.Call("web-fetch", new object?[] { "x" });

        result.Should().Be("42");
        calledProc.Should().Be("WebFetch");
        calledArgs.Should().Contain("x");
    }

    [Fact]
    public void PluginIndex_ScansOperationalPlugins()
    {
        // Archivio plugins/: tutti i plugin operativi rilevati dall'indice.
        var names = Basic.AI.PluginIndex.Scan().Select(p => p.Name).ToArray();
        names.Should().Contain(new[] { "web-fetch", "pdf", "markdown", "report", "fs-scoped", "mcp-tool", "browser" });
    }

    [Fact]
    public void PluginGet_FallsBackToIndex()
    {
        // AI.PLUGINSINFO su un plugin non ancora Enable-ato: Get ripiega sull'indice.
        var record = Basic.AI.PluginRegistry.Get("web-fetch");
        record.Should().NotBeNull();
        record!.Description.Should().Contain("web");
        record.Procedures.Should().Contain("WebFetch");
    }

    [Fact]
    public void PluginLifecycle_CreateScanRemove()
    {
        var name = "lifecycle-" + Guid.NewGuid().ToString("N")[..6];
        Basic.AI.PluginRegistry.Create(name, "desc").Should().BeTrue();
        Basic.AI.PluginIndex.Refresh();
        Basic.AI.PluginIndex.Scan().Should().Contain(p => p.Name == name);
        Basic.AI.PluginRegistry.Unregister(name);
        Directory.Delete(Path.Combine(Basic.AI.AiConfig.PluginsDefaultPath, name), true);
        Basic.AI.PluginIndex.Refresh();
        Basic.AI.PluginIndex.Scan().Should().NotContain(p => p.Name == name);
    }

    [Fact]
    public void PluginEnable_BrowserRegistersTools()
    {
        // Criterio 10.6.1 (sezione 10 piano): ENABLE del plugin browser registra i 5 tool.
        Basic.AI.PluginRegistry.Enable("browser");
        McpToolRegistry.Instance.Get("browser_navigate").Should().NotBeNull();
        McpToolRegistry.Instance.Get("browser_text").Should().NotBeNull();
        McpToolRegistry.Instance.Get("browser_title").Should().NotBeNull();
        McpToolRegistry.Instance.Get("browser_evaluate").Should().NotBeNull();
        McpToolRegistry.Instance.Get("browser_screenshot").Should().NotBeNull();
    }

    private static void Git(string workdir, params string[] args)
    {
        var psi = new ProcessStartInfo("git", string.Join(" ", args))
        {
            WorkingDirectory = workdir,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        using var p = Process.Start(psi)!;
        p.WaitForExit();
    }

    [Fact]
    public void SystemPrompt_WithPluginsEnabled_IncludesPluginsSection()
    {
        // Fase 4 piano AI-Plugins: con PluginsEnabled il prompt dell'agente include la
        // sezione "Plugin disponibili" con i plugin attivi (descrizione + trigger).
        Basic.AI.PluginRegistry.Enable("web-fetch");
        AgentServer.Instance.Create("PluginPromptAgent");
        var agent = AgentServer.Instance.List().First(a => a.Name == "PluginPromptAgent");
        agent.PluginsEnabled = true;

        var prompt = agent.ResolveSystemPrompt();

        prompt.Should().Contain("Plugin disponibili");
        prompt.Should().Contain("web-fetch");
        prompt.Should().Contain("Scarica una pagina web");
    }

    private sealed class StubPluginToolCompleter : IToolCallingCompleter
    {
        public string ModelName => "stub-plugin";
        public int CompleteWithToolsCalls { get; private set; }
        public string Complete(string systemPrompt, string userPrompt) => "";
        public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools)
        {
            CompleteWithToolsCalls++;
            return CompleteWithToolsCalls == 1
                ? new ToolCallOutcome(null, [new ToolCallRequest("1", "web_fetch", "{\"url\":\"https://example.com\"}")])
                : new ToolCallOutcome("42", null);
        }
    }

    [Fact]
    public void AskWithToolsStream_StreamsOnlyFinalAnswer()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("StreamTool");
        AgentServer.Instance.RegisterTool("StreamTool", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "StreamTool");
        var stub = new StubStreamingToolCompleter();
        var chunks = new List<string>();

        var answer = AgentToolLoop.AskWithToolsStream(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool", chunks.Add);

        answer.Should().Contain("4");
        // La risposta finale e' streammata chunk per chunk; i chunk della tool call scartati.
        chunks.Should().Contain("Il risultato");
        chunks.Should().Contain(" e' 4.");
        chunks.Should().NotContain(c => c.Contains("tool_calls"));
        agent.History.Should().Contain(m => m.Role == "tool" && m.Content.Contains("[add_numbers]"));
    }

    private sealed class StubStreamingToolCompleter : IStreamingLlmCompleter
    {
        public string ModelName => "stub-stream";
        private int _calls;

        public string Complete(string systemPrompt, string userPrompt) => "";

        public IEnumerable<string> CompleteStream(string systemPrompt, string userPrompt)
        {
            _calls++;
            if (_calls == 1)
                yield return "{\"tool_calls\":[{\"name\":\"add_numbers\",\"arguments\":{\"a\":2,\"b\":2}}]}";
            else
            {
                yield return "Il risultato";
                yield return " e' 4.";
            }
        }
    }

    [Fact]
    public void AskWithTools_AutoDiscoversClientTools()
    {
        McpConfig.AddServer(new McpServerEntry("fake", "http", Url: "http://127.0.0.1:1/mcp"));
        McpConfig.SetEnabled("fake", true);
        McpMarketplace.RememberTools("fake", [("read_file", "legge un file")]);

        AgentServer.Instance.Create("Auto");
        AgentServer.Instance.Set("Auto", "clienttools", "ON");
        var agent = AgentServer.Instance.List().First(a => a.Name == "Auto");
        var stub = new StubJsonToolCompleter();

        var answer = AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool");

        answer.Should().NotBeEmpty();
        // Il tool client e' stato riconosciuto e invocato (ERROR perche' il server non
        // risponde) — NON 'unknown tool'.
        agent.History.Should().Contain(m => m.Role == "tool" && m.Content.Contains("[fake.read_file]"));
    }

    [Fact]
    public void AskWithTools_ClientToolsDisabledByDefault_NotInvoked()
    {
        McpConfig.AddServer(new McpServerEntry("fake", "http", Url: "http://127.0.0.1:1/mcp"));
        McpConfig.SetEnabled("fake", true);
        McpMarketplace.RememberTools("fake", [("read_file", "legge un file")]);

        AgentServer.Instance.Create("AutoOff");
        var agent = AgentServer.Instance.List().First(a => a.Name == "AutoOff");
        var stub = new StubJsonToolCompleter();

        AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool");

        // Default OFF: i client tools NON sono invocati (il call produce un errore, non un
        //'esecuzione del server).
        var toolMsg = agent.History.FirstOrDefault(m => m.Role == "tool" && m.Content.Contains("[fake.read_file]"));
        toolMsg.Should().NotBeNull();
        toolMsg!.Content.Should().Contain("disabilitati");
    }

    [Fact]
    public void AskWithTools_WithSession_ReusesSessionAndKeepsItAlive()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("SessTool");
        AgentServer.Instance.RegisterTool("SessTool", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "SessTool");
        var stub = new StubSessionToolCompleter();

        var answer = AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool", sessionId: 7);

        answer.Should().Contain("4");
        stub.GenerateCalls.Should().Be(2); // 1 tool call + 1 final, dalla KV
        stub.AppendMessages.Should().Contain("tool"); // risultato del tool rientra nella sessione
        stub.DestroyCalls.Should().Be(0); // sessione persistente NON distrutta
    }

    private sealed class StubSessionToolCompleter : ISessionAwareLlmCompleter
    {
        public string ModelName => "stub-session-tool";
        public int GenerateCalls { get; private set; }
        public int DestroyCalls { get; private set; }
        public List<string> AppendMessages { get; } = [];

        public string Complete(string systemPrompt, string userPrompt) => "";
        public int CreateSession() => 0;
        public void AppendMessage(int sessionId, string role, string content) => AppendMessages.Add(role);
        public string GenerateFromSession(int sessionId, int maxTokens)
        {
            GenerateCalls++;
            return GenerateCalls == 1
                ? "{\"tool_calls\":[{\"name\":\"add_numbers\",\"arguments\":{\"a\":2,\"b\":2}}]}"
                : "Il risultato e' 4.";
        }
        public void DestroySession(int sessionId) => DestroyCalls++;
    }

    private sealed class StubJsonToolCompleter : ILlmCompleter
    {
        public string ModelName => "stub-json";
        private int _calls;

        public string Complete(string systemPrompt, string userPrompt)
        {
            _calls++;
            return _calls == 1
                ? "{\"tool_calls\":[{\"name\":\"fake.read_file\",\"arguments\":{\"path\":\"/tmp/a.txt\"}}]}"
                : "Risposta finale.";
        }
    }

    [Fact]
    public void AskWithTools_NativeHttpToolCalling_DispatchesStructuredCalls()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("NativeHttp");
        AgentServer.Instance.RegisterTool("NativeHttp", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "NativeHttp");
        var stub = new StubToolCallingCompleter();

        var answer = AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool");

        answer.Should().Contain("4");
        agent.History.Should().Contain(m => m.Role == "tool" && m.Content.Contains("[add_numbers]"));
        stub.CompleteWithToolsCalls.Should().Be(2);
    }

    private sealed class StubToolCallingCompleter : IToolCallingCompleter
    {
        public string ModelName => "stub-native-http";
        public int CompleteWithToolsCalls { get; private set; }

        public string Complete(string systemPrompt, string userPrompt) => "";

        public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools)
        {
            CompleteWithToolsCalls++;
            return CompleteWithToolsCalls == 1
                ? new ToolCallOutcome(null, [new ToolCallRequest("1", "add_numbers", "{\"a\":2,\"b\":2}")])
                : new ToolCallOutcome("Il risultato e' 4.", null);
        }
    }

    [Fact]
    public void AskWithTools_ToolResultAware_PassesStructuredResults()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("Tra");
        AgentServer.Instance.RegisterTool("Tra", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "Tra");
        var stub = new StubToolResultCompleter();

        var answer = AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "usa il tool");

        answer.Should().Contain("4");
        stub.ToolResultsCalls.Should().Be(1);
        stub.LastExecuted.Should().Contain(e => e.Name == "add_numbers" && e.ToolUseId == "tu_1");
    }

    [Fact]
    public void LoopDetection_RepeatedAction_StopsAfterThreshold()
    {
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("Loop");
        AgentServer.Instance.RegisterTool("Loop", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "Loop");
        var stub = new StubLoopCompleter();
        var dispatcher = new StubToolDispatcher("5");

        var answer = AgentToolLoop.AskWithTools(agent, stub, dispatcher, "fai sempre add_numbers");

        answer.Should().Contain("Loop rilevato");
        // State probe: risposta identica al round 2 -> stop (1 sola invocazione del tool).
        dispatcher.Calls.Should().Be(1);
    }

    private sealed class StubLoopCompleter : ILlmCompleter
    {
        public string ModelName => "stub-loop";
        public string Complete(string systemPrompt, string userPrompt) =>
            "{\"tool_calls\":[{\"name\":\"add_numbers\",\"arguments\":{\"a\":2,\"b\":3}}]}";
    }

    private sealed class StubToolResultCompleter : IToolResultAwareCompleter
    {
        public string ModelName => "stub-tra";
        public int ToolResultsCalls { get; private set; }
        public List<ToolExecution> LastExecuted { get; } = [];

        public string Complete(string systemPrompt, string userPrompt) => "";

        public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools) =>
            new(null, [new ToolCallRequest("tu_1", "add_numbers", "{\"a\":2,\"b\":2}")]);

        public ToolCallOutcome CompleteWithToolResults(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools, IReadOnlyList<ToolExecution> executed)
        {
            ToolResultsCalls++;
            LastExecuted.AddRange(executed);
            return new ToolCallOutcome("Il risultato e' 4.", null);
        }
    }

    [Fact]
    public void AskWithTools_ChunkJson_MultipleCallsExecutedInOneRound()
    {
        // T2.1 Action Chunking (SPACE, arXiv:2609.02042): una risposta del modello con PIU'
        // tool_calls in un solo blocco viene eseguita in UN solo round; i risultati combinati
        // (log per-tool nella history) tornano al modello nel round successivo.
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("ChunkJson");
        AgentServer.Instance.RegisterTool("ChunkJson", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "ChunkJson");
        var stub = new StubChunkJsonCompleter();
        var dispatcher = new StubToolDispatcher("4");

        var answer = AgentToolLoop.AskWithTools(agent, stub, dispatcher, "esegui entrambe le somme");

        // Un solo round di tool (chunk) + un round finale -> 2 chiamate al modello.
        stub.Calls.Should().Be(2);
        // Entrambe le azioni del chunk sono state eseguite e il log combinato è tornato in history.
        agent.History.Count(m => m.Role == "tool" && m.Content.Contains("[add_numbers]")).Should().Be(2);
        answer.Should().Contain("Fatto entrambe");
    }

    [Fact]
    public void AskWithTools_ChunkToolResultAware_PassesCombinedExecutedList()
    {
        // T2.1: il percorso ToolResult-aware riceve nel round successivo la LISTA COMBINATA
        // di tutte le azioni del chunk (fan-out di un blocco, fan-in al modello).
        McpToolRegistry.Instance.Register(new McpToolDefinition
        {
            Name = "add_numbers",
            Description = "add_numbers",
            QualifiedProcedureName = "Add",
            InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
        });

        AgentServer.Instance.Create("ChunkTra");
        AgentServer.Instance.RegisterTool("ChunkTra", "add_numbers");
        var agent = AgentServer.Instance.List().First(a => a.Name == "ChunkTra");
        var stub = new StubChunkToolResultCompleter();

        var answer = AgentToolLoop.AskWithTools(agent, stub, McpRuntimeBridge.Dispatcher!, "esegui entrambe le somme");

        stub.LastExecuted.Count.Should().Be(2);   // fan-in: entrambi i risultati del chunk
        stub.LastExecuted.Select(e => e.Name).Distinct().Should().ContainSingle(x => x == "add_numbers");
        answer.Should().Contain("Sommati");
    }

    private sealed class StubChunkJsonCompleter : ILlmCompleter
    {
        public string ModelName => "stub-chunk-json";
        public int Calls { get; private set; }

        public string Complete(string systemPrompt, string userPrompt)
        {
            Calls++;
            return Calls == 1
                ? "{\"tool_calls\":[{\"name\":\"add_numbers\",\"arguments\":{\"a\":2,\"b\":2}},{\"name\":\"add_numbers\",\"arguments\":{\"a\":7,\"b\":1}}]}"
                : "Fatto entrambe.";
        }
    }

    private sealed class StubChunkToolResultCompleter : IToolResultAwareCompleter
    {
        public string ModelName => "stub-chunk-tra";
        public List<ToolExecution> LastExecuted { get; } = [];

        public string Complete(string systemPrompt, string userPrompt) => "";

        public ToolCallOutcome CompleteWithTools(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools) =>
            new(null, [
                new ToolCallRequest("tu_a", "add_numbers", "{\"a\":2,\"b\":2}"),
                new ToolCallRequest("tu_b", "add_numbers", "{\"a\":7,\"b\":1}")
            ]);

        public ToolCallOutcome CompleteWithToolResults(string systemPrompt, string userPrompt, IReadOnlyList<McpToolDefinition> tools, IReadOnlyList<ToolExecution> executed)
        {
            LastExecuted.AddRange(executed);
            return new ToolCallOutcome("Sommati entrambi.", null);
        }
    }

    private sealed class StubNativeCompleter : INativeToolDispatchCompleter
    {
        public string ModelName => "stub-native";
        public int DispatchCalls { get; private set; }

        public string Complete(string systemPrompt, string userPrompt) => "";
        public int CreateSession() => 1;
        public void AppendMessage(int sessionId, string role, string content) { }
        public string GenerateFromSession(int sessionId, int maxTokens) => "";
        public void DestroySession(int sessionId) { }

        public NativeDispatchResult DispatchTool(int sessionId, IReadOnlyList<McpToolDefinition> tools)
        {
            DispatchCalls++;
            return DispatchCalls == 1
                ? new NativeDispatchResult(new ToolCallRequest("1", "add_numbers", "{\"a\":2,\"b\":2}"), null)
                : new NativeDispatchResult(null, "Il risultato e' 4.");
        }
    }
}
