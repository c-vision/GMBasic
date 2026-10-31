// FILE: tests/GMBASIC.SemanticGraph.Tests/LspTests.cs — §4.10: test dell'LSP sul grafo.
using System.Text.Json;
using GMBasic.SemanticGraph.Lsp;
using Xunit;

namespace GMBasic.SemanticGraph.Tests;

public class LspTests
{
    private const string Src = """
        SUB Saluta(nome AS STRING)
            PRINT "ciao " + nome
        END SUB

        FUNCTION Doppio(x AS INTEGER) AS INTEGER
            Saluta("mondo")
            RETURN x * 2
        END FUNCTION

        SUB Main()
            DIM v AS INTEGER
            v = Doppio(21)
        END SUB
        """;

    private static JsonElement Call(LspServer s, int id, string method, object? prms)
    {
        var req = JsonSerializer.Serialize(new { jsonrpc = "2.0", id, method, @params = prms });
        var resp = s.Handle(req) ?? "null";
        return JsonDocument.Parse(resp).RootElement.GetProperty("result");
    }

    private static LspServer Opened()
    {
        var s = new LspServer();
        s.Handle(JsonSerializer.Serialize(new { jsonrpc = "2.0", method = "textDocument/didOpen",
            @params = new { textDocument = new { uri = "file:///test.bas", text = Src } } }));
        return s;
    }

    [Fact]
    public void Initialize_advertises_semantic_capabilities()
    {
        var s = new LspServer();
        var caps = Call(s, 1, "initialize", new { }).GetProperty("capabilities");
        Assert.True(caps.GetProperty("definitionProvider").GetBoolean());
        Assert.True(caps.GetProperty("referencesProvider").GetBoolean());
        Assert.True(caps.GetProperty("hoverProvider").GetBoolean());
        Assert.True(caps.GetProperty("documentSymbolProvider").GetBoolean());
        Assert.True(caps.GetProperty("workspaceSymbolProvider").GetBoolean());
    }

    [Fact]
    public void DocumentSymbol_lists_declared_symbols()
    {
        var s = Opened();
        var syms = Call(s, 2, "textDocument/documentSymbol",
            new { textDocument = new { uri = "file:///test.bas" } });
        var names = syms.EnumerateArray().Select(x => x.GetProperty("name").GetString()).ToList();
        Assert.Contains("Saluta", names);
        Assert.Contains("Doppio", names);
        Assert.Contains("Main", names);
    }

    [Fact]
    public void Definition_jumps_from_call_to_declaration()
    {
        var s = Opened();
        // la chiamata Doppio(21) è in Main, riga 12 (0-based) nella Src
        var def = Call(s, 3, "textDocument/definition",
            new { textDocument = new { uri = "file:///test.bas" }, position = new { line = 11, character = 6 } });
        var arr = def.EnumerateArray().ToList();
        Assert.NotEmpty(arr);
        var line = arr[0].GetProperty("range").GetProperty("start").GetProperty("line").GetInt32();
        Assert.Equal(4, line);
    }

    [Fact]
    public void References_finds_call_sites()
    {
        var s = Opened();
        var refs = Call(s, 4, "textDocument/references",
            new { textDocument = new { uri = "file:///test.bas" }, position = new { line = 0, character = 5 } });
        Assert.NotEmpty(refs.EnumerateArray().ToList());
    }

    [Fact]
    public void Hover_returns_markdown_for_symbol()
    {
        var s = Opened();
        var hover = Call(s, 5, "textDocument/hover",
            new { textDocument = new { uri = "file:///test.bas" }, position = new { line = 0, character = 5 } });
        var md = hover.GetProperty("contents").GetProperty("value").GetString();
        Assert.Contains("Saluta", md);
    }
}
