// FILE: SemanticGraph/Lsp/LspServer.cs — §4.10 LSP basato sul SemanticGraph.
// Migrato da REPL/Notebook/LspServer.cs + cablato al grafo: ora serve completion,
// definition, references, hover, documentSymbol, workspace/symbol.
// Consumatore primario: LSP del Notebook. Framing JSON-RPC 2.0 su stdio (Content-Length).

using System.Text;
using System.Text.Json;
using Basic.Pipeline;

namespace GMBasic.SemanticGraph.Lsp;

/// <summary>Stato per-documento: testo + grafo semantico derivato (rebuild su didChange).</summary>
public sealed class LspSession
{
    private readonly Dictionary<string, string> _docs = new(StringComparer.Ordinal);
    private readonly Dictionary<string, SemanticGraph> _graphs = new(StringComparer.Ordinal);

    public IReadOnlyDictionary<string, string> Documents => _docs;

    public void SetDocument(string uri, string text)
    {
        _docs[uri] = text;
        try
        {
            var (_, program) = SourcePipeline.CompileFromStringWithRegistry(
                text, FileNameOf(uri), Basic.StdLib.Instance, SourcePipeline.DefaultNamespacesStandard);
            _graphs[uri] = SemanticGraphExtractor.Extract(program, uri);
        }
        catch { /* documento non compilabile: mantiene il vecchio grafo (best-effort LSP) */ }
    }

    public string? Text(string uri) => _docs.TryGetValue(uri, out var t) ? t : null;
    public SemanticGraph? Graph(string uri) => _graphs.TryGetValue(uri, out var g) ? g : null;

    /// <summary>Simbolo la cui DEFINIZIONE cade sulla riga LSP (0-based) data.</summary>
    public SymbolNode? SymbolAt(string uri, int lspLine)
    {
        var g = Graph(uri); if (g is null) return null;
        // il nome del simbolo è sulla riga (Line 1-based) → confronta con lspLine+1
        return g.Symbols.Where(s => s.Line == lspLine + 1)
                        .OrderBy(s => s.Column).FirstOrDefault();
    }

    /// <summary>Target di un riferimento (call/var) che cade sulla riga data (per goto-definition).</summary>
    public SymbolNode? ReferenceTargetAt(string uri, int lspLine)
    {
        var g = Graph(uri); if (g is null) return null;
        var reference = g.References.Where(r => r.Line == lspLine + 1).OrderBy(r => r.Column).FirstOrDefault();
        if (reference is null) return null;
        var edge = g.OutEdges(reference.Id).FirstOrDefault();
        return edge is null ? null : g.GetSymbol(edge.ToId);
    }

    private static string FileNameOf(string uri)
    {
        var idx = uri.LastIndexOf('/'); var name = idx >= 0 ? uri[(idx + 1)..] : uri;
        return name.Length == 0 ? "doc.bas" : name;
    }
}

public sealed class LspServer
{
    private static readonly string[] BasicKeywords =
    [
        "PRINT", "LET", "DIM", "REDIM", "IF", "THEN", "ELSE", "ELSEIF", "END", "FOR", "TO",
        "NEXT", "WHILE", "WEND", "DO", "LOOP", "UNTIL", "SELECT", "CASE", "GOTO", "GOSUB",
        "RETURN", "SUB", "FUNCTION", "END SUB", "END FUNCTION", "REM", "IMPORT", "NAMESPACE",
        "TYPE", "END TYPE", "CLASS", "END CLASS", "CONST", "EXIT", "CONTINUE", "ON", "ERROR",
        "RESUME", "CALL", "DEF", "DATA", "READ", "RESTORE", "INPUT", "OPEN", "CLOSE",
    ];

    private readonly LspSession _session = new();

    /// <summary>Elabora un messaggio JSON-RPC e ritorna il testo della risposta (null per le
    /// notifiche). I metodi semantici usano il <see cref="LspSession"/> (grafo).</summary>
    public string? Handle(string rawRequest)
    {
        using var doc = JsonDocument.Parse(rawRequest);
        var root = doc.RootElement;
        var method = root.GetProperty("method").GetString();
        object? id = null;
        if (root.TryGetProperty("id", out var idEl) && idEl.ValueKind != JsonValueKind.Null)
            id = idEl.ValueKind == JsonValueKind.Number ? idEl.GetInt32() : idEl.GetString();
        var hasParams = root.TryGetProperty("params", out var prms) && prms.ValueKind != JsonValueKind.Undefined;

        switch (method)
        {
            case "initialize":
                return Response(id, Result: new
                {
                    capabilities = new
                    {
                        textDocumentSync = 1,
                        completionProvider = new { triggerCharacters = new[] { "." } },
                        definitionProvider = true,
                        referencesProvider = true,
                        hoverProvider = true,
                        documentSymbolProvider = true,
                        workspaceSymbolProvider = true,
                    },
                    serverInfo = new { name = "gmbasic-lsp", version = "0.2" },
                });
            case "initialized": return null;
            case "textDocument/didOpen":
            case "textDocument/didChange":
                if (hasParams) TrackDocument(prms);
                return null;
            case "textDocument/completion": return hasParams ? Response(id, CompletionResult(prms)) : null;
            case "textDocument/definition": return hasParams ? Response(id, DefinitionResult(prms)) : null;
            case "textDocument/references": return hasParams ? Response(id, ReferencesResult(prms)) : null;
            case "textDocument/hover": return hasParams ? Response(id, HoverResult(prms)) : null;
            case "textDocument/documentSymbol": return hasParams ? Response(id, DocumentSymbolResult(prms)) : null;
            case "workspace/symbol": return hasParams ? Response(id, WorkspaceSymbolResult(prms)) : null;
            case "shutdown": return Response(id, Result: null);
            case "exit": return null;
            default: return Response(id, Error: new { code = -32601, message = "method not found: " + method });
        }
    }

    private void TrackDocument(JsonElement prms)
    {
        string uri = "", text = "";
        if (prms.TryGetProperty("textDocument", out var td) && td.TryGetProperty("uri", out var u))
            uri = u.GetString() ?? "";
        if (td.TryGetProperty("text", out var tOpen) && tOpen.ValueKind == JsonValueKind.String)
            text = tOpen.GetString() ?? "";
        if (prms.TryGetProperty("contentChanges", out var changes))
            foreach (var c in changes.EnumerateArray())
                if (c.TryGetProperty("text", out var ct)) text = ct.GetString() ?? "";
        if (uri.Length > 0) _session.SetDocument(uri, text);
    }

    private static (string uri, int line, int col) Pos(JsonElement prms)
    {
        string uri = ""; int line = 0, col = 0;
        if (prms.TryGetProperty("textDocument", out var td) && td.TryGetProperty("uri", out var u)) uri = u.GetString() ?? "";
        if (prms.TryGetProperty("position", out var pos))
        {
            if (pos.TryGetProperty("line", out var l)) line = l.GetInt32();
            if (pos.TryGetProperty("character", out var c)) col = c.GetInt32();
        }
        return (uri, line, col);
    }

    private JsonElement CompletionResult(JsonElement prms)
    {
        var (uri, line, _) = Pos(prms);
        var text = _session.Text(uri) ?? "";
        var lines = text.Split('\n');
        var currentLine = line >= 0 && line < lines.Length ? lines[line] : "";
        return JsonArr(BuildCompletions(uri, currentLine));
    }

    private List<string> BuildCompletions(string uri, string line)
    {
        var items = new List<string>();
        var trimmed = line.TrimStart();
        var dot = trimmed.LastIndexOf('.');
        if (dot > 0)
        {
            var ns = trimmed[..dot].Trim();
            if (Basic.StdLib.Instance.BuiltinProcedures.TryGetValue(ns, out var procs))
                foreach (var p in procs.OrderBy(p => p.Name.Lexeme, StringComparer.OrdinalIgnoreCase))
                    items.Add(Item(ns + "." + p.Name.Lexeme, "Function", p.Name.Lexeme + "(" + Sig(p) + ")", p.Name.Lexeme));
            return items;
        }

        // Simboli del documento (dal grafo) — prima dei builtin: sono quelli che l'utente definisce.
        var g = _session.Graph(uri);
        if (g is not null)
        {
            var seenDoc = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var s in g.Symbols.Where(s => s.Namespace is null).OrderBy(s => s.Name))
                if (seenDoc.Add(s.Name)) items.Add(Item(s.Name, KindOf(s.Kind), s.Label, s.Name));
        }

        var space = trimmed.LastIndexOfAny([' ', '(', ';', ',']);
        var prefix = (space < 0 ? trimmed : trimmed[(space + 1)..]).Trim();
        var pfx = prefix.ToUpperInvariant();
        foreach (var kw in BasicKeywords)
            if (pfx.Length == 0 || kw.StartsWith(pfx, StringComparison.OrdinalIgnoreCase))
                items.Add(Item(kw, "Keyword", kw));

        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var count = 0;
        foreach (var (_, procs) in Basic.StdLib.Instance.BuiltinProcedures)
        {
            foreach (var p in procs)
            {
                var name = p.Name.Lexeme;
                if (pfx.Length > 0 && !name.StartsWith(pfx, StringComparison.OrdinalIgnoreCase)) continue;
                if (!seen.Add(name)) continue;
                items.Add(Item(name, "Function", name + "(" + Sig(p) + ")"));
                if (++count >= 60) break;
            }
            if (count >= 60) break;
        }
        return items;
    }

    private JsonElement DefinitionResult(JsonElement prms)
    {
        var (uri, line, _) = Pos(prms);
        // 1) se la posizione è su un RIFERIMENTO (call/var), vai alla definizione del target
        var target = _session.ReferenceTargetAt(uri, line) ?? _session.SymbolAt(uri, line);
        if (target is null) return JsonArr(new List<string>());
        return JsonArr([Location(uri, target.Line - 1, target.Column - 1, target.Name.Length)]);
    }

    private JsonElement ReferencesResult(JsonElement prms)
    {
        var (uri, line, _) = Pos(prms);
        var g = _session.Graph(uri);
        var sym = _session.ReferenceTargetAt(uri, line) ?? _session.SymbolAt(uri, line);
        if (g is null || sym is null) return JsonArr(new List<string>());
        var locs = new List<string>();
        foreach (var e in g.InEdges(sym.Id))
        {
            if (g.GetReference(e.FromId) is { } r) locs.Add(Location(uri, r.Line - 1, r.Column - 1, r.Text.Length));
        }
        return JsonArr(locs);
    }

    private JsonElement HoverResult(JsonElement prms)
    {
        var (uri, line, _) = Pos(prms);
        var sym = _session.ReferenceTargetAt(uri, line) ?? _session.SymbolAt(uri, line);
        if (sym is null) return JsonArr(new List<string>());
        var md = $"**{sym.Label}**  \n`{sym.Kind}`" + (sym.TypeName is null ? "" : $"  \nTipo: `{sym.TypeName}`")
               + $"  \nDefinizione: {sym.File}:{sym.Line}";
        return JsonRaw(JsonSerializer.Serialize(new { contents = new { kind = "markdown", value = md } }));
    }

    private JsonElement DocumentSymbolResult(JsonElement prms)
    {
        var (uri, _, _) = Pos(prms);
        var g = _session.Graph(uri);
        if (g is null) return JsonArr(new List<string>());
        var items = g.Symbols.Where(s => s.Namespace is null)
            .OrderBy(s => s.Line)
            .Select(s => JsonSerializer.Serialize(new
            {
                name = s.Name,
                kind = SymbolKindLsp(s.Kind),
                range = new { start = new { line = s.Line - 1, character = s.Column - 1 },
                              end = new { line = s.Line - 1, character = s.Column - 1 + s.Name.Length } },
                selectionRange = new { start = new { line = s.Line - 1, character = s.Column - 1 },
                                       end = new { line = s.Line - 1, character = s.Column - 1 + s.Name.Length } },
            }))
            .ToStringList();
        return JsonArr(items);
    }

    private JsonElement WorkspaceSymbolResult(JsonElement prms)
    {
        string query = "";
        if (prms.TryGetProperty("query", out var q)) query = q.GetString() ?? "";
        var items = new List<string>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var (uri, g) in _session.Documents.Keys.Select(u => (u, _session.Graph(u))))
        {
            if (g is null) continue;
            foreach (var s in g.Symbols.Where(s => s.Namespace is null))
            {
                if (query.Length > 0 && !s.Name.Contains(query, StringComparison.OrdinalIgnoreCase)) continue;
                if (!seen.Add(uri + "\u0001" + s.Id)) continue;
                items.Add(JsonSerializer.Serialize(new
                {
                    name = s.Name,
                    kind = SymbolKindLsp(s.Kind),
                    location = new { uri, range = new { start = new { line = s.Line - 1, character = s.Column - 1 },
                                                          end = new { line = s.Line - 1, character = s.Column - 1 + s.Name.Length } } },
                }));
            }
        }
        return JsonArr(items);
    }

    private static string Location(string uri, int line0, int col0, int len)
        => JsonSerializer.Serialize(new { uri, range = new { start = new { line = line0, character = col0 },
                                                              end = new { line = line0, character = col0 + len } } });

    private static string KindOf(SemanticSymbolKind k) => k switch
    {
        SemanticSymbolKind.Procedure or SemanticSymbolKind.Function or SemanticSymbolKind.Method => "Function",
        SemanticSymbolKind.Class or SemanticSymbolKind.Type or SemanticSymbolKind.Enum => "Class",
        SemanticSymbolKind.Constant => "Constant",
        SemanticSymbolKind.Parameter => "Variable",
        _ => "Variable",
    };

    private static int SymbolKindLsp(SemanticSymbolKind k) => k switch
    {
        SemanticSymbolKind.Function or SemanticSymbolKind.Procedure or SemanticSymbolKind.Method => 12,
        SemanticSymbolKind.Class => 5,
        SemanticSymbolKind.Interface => 11,
        SemanticSymbolKind.Enum => 10,
        SemanticSymbolKind.Type => 5,
        SemanticSymbolKind.Constant => 14,
        SemanticSymbolKind.Field or SemanticSymbolKind.Property => 7,
        SemanticSymbolKind.Namespace => 3,
        SemanticSymbolKind.Module => 2,
        _ => 13,
    };

    private static string Sig(Basic.ProcedureDefinition p) => string.Join(", ", p.Parameters.Select(x => x.Name.Lexeme));
    private static string Item(string label, string kind, string detail, string? insertText = null)
        => JsonSerializer.Serialize(new { label, kind, detail, insertText });

    private static JsonElement JsonArr(IEnumerable<string> jsons) => JsonRaw("[" + string.Join(",", jsons) + "]");

    private static JsonElement JsonRaw(string json)
    {
        using var d = JsonDocument.Parse(json);
        return d.RootElement.Clone();
    }

    private static string Response(object? id, object? Result = null, object? Error = null)
    {
        using var ms = new MemoryStream();
        using (var w = new Utf8JsonWriter(ms))
        {
            w.WriteStartObject();
            w.WriteString("jsonrpc", "2.0");
            if (id is int i) w.WriteNumber("id", i);
            else if (id is string s) w.WriteString("id", s);
            else w.WriteNull("id");
            if (Error != null) { w.WritePropertyName("error"); JsonSerializer.Serialize(w, Error); }
            else
            {
                w.WritePropertyName("result");
                if (Result is null) w.WriteNullValue(); else JsonSerializer.Serialize(w, Result);
            }
            w.WriteEndObject();
        }
        return Encoding.UTF8.GetString(ms.ToArray());
    }

    // framing stdio (Content-Length: N\r\n\r\n<body>)
    public static string ReadMessage(Stream input)
    {
        var headers = new StringBuilder();
        int b;
        while (true) { b = input.ReadByte(); if (b < 0) return ""; headers.Append((char)b); if (headers.Length >= 4 && headers.ToString().EndsWith("\r\n\r\n")) break; }
        var hs = headers.ToString();
        var lenLine = hs.Split("\r\n").FirstOrDefault(x => x.StartsWith("Content-Length:", StringComparison.OrdinalIgnoreCase));
        if (lenLine == null) return "";
        var len = int.Parse(lenLine.Split(':')[1].Trim());
        var buf = new byte[len]; var read = 0;
        while (read < len) read += input.Read(buf, read, len - read);
        return Encoding.UTF8.GetString(buf, 0, len);
    }

    public static void WriteMessage(Stream output, string json)
    {
        var bytes = Encoding.UTF8.GetBytes(json);
        var header = Encoding.ASCII.GetBytes($"Content-Length: {bytes.Length}\r\n\r\n");
        output.Write(header, 0, header.Length); output.Write(bytes, 0, bytes.Length); output.Flush();
    }
}

internal static class SemanticGraphListExt
{
    public static List<string> ToStringList(this IEnumerable<string> src) => src.ToList();
}
