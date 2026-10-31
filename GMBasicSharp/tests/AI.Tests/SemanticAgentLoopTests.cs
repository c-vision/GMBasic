// FILE: AI.Tests.SemanticAgentLoopTests
// §4.10 esposizione agenti: tool-loop `code.*` sul grafo semantico del codice (implementazione C#
// indipendente; il gemello C usa gmb_semgraph). Speculare a RagAgentLoopTests/GraphAgentLoopTests.
using System;
using System.IO;
using Basic.Agents;
using Xunit;

namespace AI.Tests;

public sealed class SemanticAgentLoopTests : IDisposable
{
    private readonly string _file;

    public SemanticAgentLoopTests()
    {
        _file = Path.Combine(Path.GetTempPath(), "semloop_" + Guid.NewGuid().ToString("N") + ".bas");
        // Alpha -> Beta -> Gamma (call chain) per symbol/callmap/impact.
        File.WriteAllText(_file,
            "SUB Alpha()\n" +
            "  Beta()\n" +
            "END SUB\n" +
            "SUB Beta()\n" +
            "  Gamma()\n" +
            "END SUB\n" +
            "FUNCTION Gamma() AS INTEGER\n" +
            "  Gamma = 1\n" +
            "END FUNCTION\n");
    }

    public void Dispose()
    {
        try { File.Delete(_file); } catch { /* best-effort */ }
    }

    private string Art(string name) => $"{{\"path\":\"{_file}\",\"name\":\"{name}\"}}";

    [Fact(DisplayName = "code.Register: espone i tool code.* nel registry")]
    public void Register_Exposes_Tools()
    {
        SemanticAgentLoop.Register();
        foreach (var (n, _) in SemanticAgentLoop.Tools)
            Assert.NotNull(Basic.Mcp.McpToolRegistry.Instance.Get(n));
    }

    [Fact(DisplayName = "code.symbol: definizione di Beta")]
    public void Symbol_Definition()
    {
        var r = SemanticAgentLoop.Execute("code.symbol", Art("Beta"));
        Assert.Contains("Beta", r);
        Assert.StartsWith("procedure", r);
    }

    [Fact(DisplayName = "code.callmap: callers/callees di Beta")]
    public void Callmap()
    {
        var r = SemanticAgentLoop.Execute("code.callmap", Art("Beta"));
        Assert.Contains("callers=[Alpha]", r);
        Assert.Contains("callees=[Gamma]", r);
    }

    [Fact(DisplayName = "code.impact: impatto transitivo di Gamma include Alpha e Beta")]
    public void Impact()
    {
        var r = SemanticAgentLoop.Execute("code.impact", Art("Gamma"));
        Assert.Contains("Beta", r);
        Assert.Contains("Alpha", r);
    }

    [Fact(DisplayName = "code.fileoutline: i simboli del file")]
    public void FileOutline()
    {
        var r = SemanticAgentLoop.Execute("code.fileoutline", $"{{\"path\":\"{_file}\"}}");
        Assert.Contains("Alpha", r);
        Assert.Contains("Beta", r);
        Assert.Contains("Gamma", r);
    }

    [Fact(DisplayName = "code.findsymbol: trova i simboli per sottostringa")]
    public void FindSymbol()
    {
        var r = SemanticAgentLoop.Execute("code.findsymbol", $"{{\"path\":\"{_file}\",\"query\":\"amm\"}}");
        Assert.Contains("Gamma", r);
    }

    [Fact(DisplayName = "code.*: path mancante -> errore esplicito")]
    public void Missing_Path_Error()
    {
        var r = SemanticAgentLoop.Execute("code.symbol", "{\"name\":\"Beta\"}");
        Assert.StartsWith("error:", r);
    }

    [Fact(DisplayName = "code.*: file inesistente -> errore esplicito")]
    public void Missing_File_Error()
    {
        var r = SemanticAgentLoop.Execute("code.symbol", "{\"path\":\"/no/such/file.bas\",\"name\":\"Beta\"}");
        Assert.StartsWith("error:", r);
    }
}
