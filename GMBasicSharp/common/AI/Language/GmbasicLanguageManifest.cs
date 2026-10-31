// FILE: Basic.AI.Language.GmbasicLanguageManifest
// B5.5 — GMBASIC_LANGUAGE.json (da StdLib) + STRICT mode (limita i namespace dichiarati).

using System.Text.Json;
using System.Text.Json.Serialization;

namespace Basic.AI.Language;

public static class LanguageManifest
{
    private static bool _strict;
    public static bool StrictMode { get => _strict; set => _strict = value; }

    /// <summary>Genera GMBASIC_LANGUAGE.json con tutte le funzioni StdLib da StdLib.Instance.
    /// Formato: { generatedAt, strict, namespaces: { NS: [ {name, kind, returnType} ] } }.</summary>
    public static string Build()
    {
        var namespaces = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var (nsName, procs) in Basic.StdLib.Instance.BuiltinProcedures
                     .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase))
        {
            var funcs = new List<object>();
            foreach (var p in procs.OfType<Basic.NativeProcedureDefinition>())
            {
                var name = p.OriginalName.Lexeme;
                funcs.Add(new
                {
                    name,
                    kind = p.IsFunction ? "function" : "command",
                    ns = nsName,
                    paramCount = p.Parameters.Length,
                    ret = p.IsFunction ? p.ReturnType?.ToString() : null
                });
            }
            if (funcs.Count > 0) namespaces[nsName] = funcs;
        }
        return JsonSerializer.Serialize(new
        {
            generatedAt = DateTime.UtcNow,
            version = 1,
            strict = _strict,
            namespaces
        }, new JsonSerializerOptions { WriteIndented = true });
    }

    /// <summary>Scrive il manifest su file e ritorna il path (o errore esplicito).</summary>
    public static string WriteTo(string outputPath)
    {
        var json = Build();
        if (string.IsNullOrWhiteSpace(outputPath)) return "Error: outputPath vuota";
        try
        {
            var dir = Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            File.WriteAllText(outputPath, json);
            return outputPath;
        }
        catch (Exception ex) { return $"Error: {ex.Message}"; }
    }

    /// <summary>C4 — Consumer per IDE/HELP: cerca nel manifest (file o JSON) le funzioni il cui
    /// nome/ns matcha. Ritorna [{ns,name,kind,paramCount}] JSON.</summary>
    public static string Search(string query, string? manifestPathOrJson = null)
    {
        if (string.IsNullOrWhiteSpace(query)) return "[]";
        string json;
        if (string.IsNullOrWhiteSpace(manifestPathOrJson))
        {
            json = Build();
        }
        else if (File.Exists(manifestPathOrJson))
        {
            json = File.ReadAllText(manifestPathOrJson);
        }
        else
        {
            json = manifestPathOrJson; // già JSON
        }
        var q = query.ToLowerInvariant();
        var hits = new List<object>();
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.TryGetProperty("namespaces", out var nss) && nss.ValueKind == JsonValueKind.Object)
            {
                foreach (var ns in nss.EnumerateObject())
                {
                    if (ns.Value.ValueKind == JsonValueKind.Array)
                    {
                        foreach (var f in ns.Value.EnumerateArray())
                        {
                            var name = f.TryGetProperty("name", out var nm) ? nm.GetString() ?? "" : "";
                            var full = (ns.Name + "." + name).ToLowerInvariant();
                            if (full.Contains(q) || name.ToLowerInvariant().Contains(q))
                            {
                                hits.Add(new
                                {
                                    ns = ns.Name,
                                    name,
                                    kind = f.TryGetProperty("kind", out var k) ? k.GetString() : null,
                                    paramCount = f.TryGetProperty("paramCount", out var pc) ? pc.GetInt32() : 0
                                });
                            }
                        }
                    }
                }
            }
        }
        catch { }
        return JsonSerializer.Serialize(new { query, count = hits.Count, hits });
    }
}
