// ObservationSources.cs — A5 (asse A, fase 2): le FONTI OBSERVE mancanti (TEST/DATABASE/PROCESS/
// BROWSER) come ContextObject con TYPE esplicito. Specchio esatto del runtime C
// (native/gmb_runtime_c/src/gmb_observe_sources.c): STESSO artefatto JSON, stesso ordine di chiavi,
// stessi errori espliciti. Nel context entra solo il RIFERIMENTO (id/type/source/artifact/hash/size).
using System.IO;
using System.Text;

namespace Basic.AI.Agents;

public static class ObservationSources
{
    /// <summary>Artefatti delle fonti OBSERVE: path RELATIVI deterministici (identici nei due mondi,
    /// così `AI.OBSERVATIONFILE$` è lo stesso testo su tutte le gambe).</summary>
    public const string TestArtifact = "work/ai_observe_test.json";
    public const string DatabaseArtifact = "work/ai_observe_database.json";
    public const string ProcessArtifact = "work/ai_observe_process.json";
    public const string BrowserArtifact = "work/ai_observe_browser.png";

    /// <summary>Hook del database e del browser registrati dal backend (common/AI non conosce
    /// IlBackendRuntime: stesso pattern di AiWorkerRuntime.RequireEvaluator).
    /// `SqlOpenHandle` (path) → handle (0 = errore); `SqlQueryRows` (handle, sql) → righe;
    /// `SqlCloseHandle` (handle); `BrowserScreenshot` (url, outPath) → path o "error…".</summary>
    public static Func<string, long>? SqlOpenHandle { get; set; }
    public static Func<long, string, long>? SqlQueryRows { get; set; }
    public static Action<long>? SqlCloseHandle { get; set; }

    private static void Write(string path, string content)
    {
        var dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(path, content, new UTF8Encoding(false));
    }

    /// <summary>`AI OBSERVE TEST "<path.bas>"`: compila il file con lo STESSO strumento di
    /// `AI VERIFY … COMPILE` e registra l'esito come ContextObject di tipo "test".
    /// Ritorna l'id, oppure -1 se il file non è compilabile (errore ESPLICITO a monte).</summary>
    public static long ObserveTest(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return -1;
        bool ok;
        string diag;
        try
        {
            var (compiled, diagnostics) = Basic.Agents.CodeGenTools.ToolCompile(path);
            ok = compiled; diag = diagnostics ?? "";
        }
        catch (System.Exception ex) { ok = false; diag = ex.Message; }
        if (!ok) return -1;
        Write(TestArtifact, "{\"path\":\"" + JsonEscape(path) + "\",\"ok\":true,\"diagnostics\":\"\"}\n");
        return ObservationRegistry.RecordTyped("test", path, TestArtifact);
    }

    /// <summary>`AI OBSERVE DATABASE "<path>", {"query":"SELECT …"}`: apre il database, esegue la
    /// query e registra il RIFERIMENTO (alias/path + query + numero righe) come ContextObject di
    /// tipo "database". Ritorna l'id, -1 su errore (DB non apribile / query fallita).</summary>
    public static long ObserveDatabase(string path, string query)
    {
        if (string.IsNullOrWhiteSpace(path)) return -1;
        var sql = string.IsNullOrWhiteSpace(query) ? "SELECT name FROM sqlite_master" : query;
        if (SqlOpenHandle is null || SqlQueryRows is null)
            throw new System.InvalidOperationException(
                "AI OBSERVE DATABASE: nessun accesso SQL registrato dal backend (hook SqlOpenHandle/SqlQueryRows)");
        long rows;
        long handle = 0;
        try
        {
            handle = SqlOpenHandle(path);
            if (handle == 0) return -1;
            rows = SqlQueryRows(handle, sql);
        }
        catch { return -1; }
        finally { if (handle != 0) { try { SqlCloseHandle?.Invoke(handle); } catch { } } }
        Write(DatabaseArtifact,
            "{\"path\":\"" + JsonEscape(path) + "\",\"query\":\"" + JsonEscape(sql) + "\",\"rows\":" + rows + "}\n");
        return ObservationRegistry.RecordTyped("database", path, DatabaseArtifact);
    }

    /// <summary>`AI OBSERVE PROCESS "<nome>"`: verifica che il processo esista e registra il
    /// RIFERIMENTO (la query) come ContextObject di tipo "process". -1 se NON esiste (errore
    /// esplicito: un'osservazione su un processo inesistente non va inventata).</summary>
    public static long ObserveProcess(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return -1;
        bool found;
        try { found = System.Diagnostics.Process.GetProcessesByName(name).Length > 0; }
        catch { found = false; }
        if (!found) return -1;
        Write(ProcessArtifact, "{\"query\":\"" + JsonEscape(name) + "\"}\n");
        return ObservationRegistry.RecordTyped("process", name, ProcessArtifact);
    }

    /// <summary>`AI OBSERVE BROWSER "<url>"`: cattura lo screenshot della pagina via BROWSER runtime
    /// (CDP) e lo registra come ContextObject di tipo "browser". -1 se il browser/CDP non è
    /// disponibile o la cattura fallisce (errore esplicito a monte).</summary>
    public static long ObserveBrowser(string url)
    {
        if (string.IsNullOrWhiteSpace(url)) return -1;
        string shot;
        try
        {
            // Naviga la pagina e cattura lo screenshot (stesso runtime BROWSER delle altre gambe C#).
            var nav = Basic.AI.Browser.BrowserRuntime.NavigateAsync(url).GetAwaiter().GetResult();
            if (!nav.Success) return -1;
            shot = Basic.AI.Browser.BrowserRuntime.ScreenshotAsync(BrowserArtifact).GetAwaiter().GetResult() is { } sc && sc.Success
                ? sc.Data ?? "" : "";
        }
        catch { return -1; }
        if (string.IsNullOrEmpty(shot) || shot.StartsWith("error", System.StringComparison.Ordinal)) return -1;
        return ObservationRegistry.RecordTyped("browser", url, shot);
    }

    internal static string JsonEscape(string s)
    {
        var sb = new StringBuilder();
        foreach (var c in s)
        {
            if (c == '"') sb.Append("\\\"");
            else if (c == '\\') sb.Append("\\\\");
            else if (c == '\n') sb.Append("\\n");
            else if (c == '\r') sb.Append("\\r");
            else if (c == '\t') sb.Append("\\t");
            else sb.Append(c);
        }
        return sb.ToString();
    }
}
