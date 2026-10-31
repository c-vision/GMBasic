// Browser/AuthProfileStore.cs — G6: profili di autenticazione nominati e riusabili.
//
// Salva/ripristina lo stato di autenticazione del browser (cookies) sotto
// work/browser-auth/<nome>/. Ogni profilo e' una cartella con cookies.json.
// LOAD ripristina i cookies nel browser corrente via Network.setCookie.
//
// Storage = categoria output (mai fixture): work/ locale accanto al generatore.
// Riusa il client CDP per leggere/scrivere i cookies.
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Basic.AI.Browser;

public static class AuthProfileStore
{
    /// <summary>Directory base dei profili (work/browser-auth/ relativa alla CWD).</summary>
    public static string BaseDir => System.IO.Path.Combine(System.IO.Path.GetTempPath(), "gmb_browser_auth");

    public static string ProfileDir(string name)
        => System.IO.Path.Combine(BaseDir, Sanitize(name));

    /// <summary>Comando AUTH unificato: "save"|"load"|"list".
    /// Dispatcher C# che usa il client CDP (BrowserRuntime).</summary>
    public static string Dispatch(string op, string name)
    {
        try
        {
            switch (op.ToLowerInvariant())
            {
                case "save": return Save(name).GetAwaiter().GetResult();
                case "load": return Load(name).GetAwaiter().GetResult();
                case "list": return List();
                case "fork": return Fork(name).GetAwaiter().GetResult();
                default: return "error: opzione AUTH sconosciuta: " + op + " (save|load|list|fork)";
            }
        }
        catch (System.Exception ex) { return "error: " + ex.Message; }
    }

    private static string Sanitize(string name)
    {
        var chars = name.ToCharArray();
        for (int i = 0; i < chars.Length; i++)
            if (!char.IsLetterOrDigit(chars[i]) && chars[i] is not '_' and not '-')
                chars[i] = '_';
        var s = new string(chars);
        return string.IsNullOrWhiteSpace(s) ? "default" : s;
    }

    /// <summary>Salva i cookies correnti del browser nel profilo. Ritorna messaggio.</summary>
    public static async Task<string> Save(string name, CancellationToken ct = default)
    {
        var cookies = await BrowserRuntime.GetCookiesAsync(ct);
        if (!cookies.Success) return "error: impossibile leggere cookies: " + cookies.Error;
        if (cookies.Data is not { } json || json == "null") return "error: nessun cookie da salvare";

        var dir = ProfileDir(name);
        System.IO.Directory.CreateDirectory(dir);
        var path = System.IO.Path.Combine(dir, "cookies.json");
        await System.IO.File.WriteAllTextAsync(path, json, ct);
        return "ok: auth profile salvato: " + name + " (" + path + ")";
    }

    /// <summary>Ripristina i cookies dal profilo nel browser corrente.</summary>
    public static async Task<string> Load(string name, CancellationToken ct = default)
    {
        var path = System.IO.Path.Combine(ProfileDir(name), "cookies.json");
        if (!System.IO.File.Exists(path)) return "error: auth profile non trovato: " + name;
        string json;
        try { json = await System.IO.File.ReadAllTextAsync(path, ct); }
        catch (System.Exception ex) { return "error: lettura profilo: " + ex.Message; }

        // per ogni cookie, importalo nel browser
        using var doc = JsonDocument.Parse(json);
        if (!doc.RootElement.TryGetProperty("cookies", out var arr) &&
            !doc.RootElement.TryGetProperty("result", out arr))
            return "error: formato cookies inatteso";
        // CDP Network.setCookie per ciascun cookie
        if (arr.ValueKind == JsonValueKind.Array)
        {
            foreach (var c in arr.EnumerateArray())
            {
                string n = Str(c, "name"), v = Str(c, "value");
                string url = "http://localhost/";
                string? domain = c.TryGetProperty("domain", out var d) ? d.GetString() : null;
                if (domain is { Length: > 0 }) url = (domain.StartsWith('.') ? "http" : "http") + "://" + domain.TrimStart('.') + "/";
                await BrowserRuntime.SetCookieAsync(n, v, url, ct);
            }
            return "ok: auth profile ripristinato: " + name + " (" + Count(arr) + " cookies)";
        }
        return "error: profilo vuoto o formato non array";
    }

    /// <summary>Elenca i profili esistenti (nome per riga).</summary>
    public static string List()
    {
        if (!System.IO.Directory.Exists(BaseDir)) return "(nessun auth profile)";
        var sb = new System.Text.StringBuilder();
        foreach (var d in System.IO.Directory.GetDirectories(BaseDir))
        {
            if (System.IO.File.Exists(System.IO.Path.Combine(d, "cookies.json")))
            {
                if (sb.Length > 0) sb.Append('\n');
                sb.Append(System.IO.Path.GetFileName(d));
            }
        }
        return sb.Length == 0 ? "(nessun auth profile)" : sb.ToString();
    }

    /// <summary>G7: fork sessione CDP — clona stato auth in una nuova sessione.
    /// name formato "dst,src" (src opzionale, default "default" = sessione primaria).</summary>
    public static async Task<string> Fork(string spec, CancellationToken ct = default)
    {
        var parts = (spec ?? "").Split(',', 2, System.StringSplitOptions.TrimEntries);
        var dst = parts.Length > 0 ? parts[0] : "";
        var src = parts.Length > 1 ? parts[1] : null;
        if (string.IsNullOrWhiteSpace(dst)) return "error: specificare 'dst' o 'dst,src'";
        return await BrowserRuntime.ForkSessionAsync(dst, src, ct);
    }

    private static string Str(JsonElement e, string k, string d = "")
        => e.TryGetProperty(k, out var p) && p.ValueKind == JsonValueKind.String ? p.GetString() ?? d : d;
    private static int Count(JsonElement arr)
    {
        int n = 0; if (arr.ValueKind == JsonValueKind.Array) n = arr.GetArrayLength(); return n;
    }
}