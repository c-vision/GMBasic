// FILE: Basic.AI.AiServerManager — gestione del server LLM OpenAI-compatible come PROCESSO ESTERNO.
//
// Comando `AI SERVER ...` (Fase 1 IL) -> questo manager. SEMANTICA: il server C
// (`native/gmb_llm_server/gmb_llm_server`) gira come processo figlio; il mondo C# lo avvia,
// lo interroga via HTTP (readiness /v1/status) e ne gestisce la residenza modello
// (/v1/models/load|unload). Nessun bridge/P/Invoke in-process: comunicazione = processo + HTTP.
//
// Mai stub/fail-loud: ogni operazione fallita ritorna uno stato con Error esplicito.
using System.Diagnostics;
using System.Net.Http;
using System.Text;
using System.Text.Json;

namespace Basic.AI;

/// <summary>Stato osservabile del server LLM gestito dal manager.</summary>
public sealed record AiServerState(
    bool Running,
    int Pid,
    int Port,
    string? Model,
    bool Loaded,
    bool Busy,
    string? Error)
{
    public bool Ok => Error is null;
}

/// <summary>Gestione del ciclo di vita del server C OpenAI-compatible (processo esterno).</summary>
public static class AiServerManager
{
    static readonly object Gate = new();
    static Process? _proc;
    static int _port;
    static string? _modelDir;

    static string? ResolveBinary()
    {
        var env = Environment.GetEnvironmentVariable("GMBASIC_LLM_SERVER");
        if (!string.IsNullOrWhiteSpace(env) && File.Exists(env)) return env;
        // cerca la build canonica relativa alla root del repo (risalendo da CWD/assembly)
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 8 && dir != null; i++, dir = dir.Parent)
        {
            var cand = Path.Combine(dir.FullName, "native", "gmb_llm_server", "build", "gmb_llm_server");
            if (File.Exists(cand)) return cand;
            var candSrc = Path.Combine(dir.FullName, "native", "gmb_llm_server", "gmb_llm_server");
            if (File.Exists(candSrc)) return candSrc;
        }
        return null;
    }

    /// <summary>Avvia il server C come processo figlio (binary da GMBASIC_LLM_SERVER o build
    /// canonica) e attende readiness su /v1/status (timeout ~60s).</summary>
    public static AiServerState Start(string modelDir, int port = 8020, int maxActive = 0)
    {
        lock (Gate)
        {
            if (_proc is { HasExited: false })
                return Err($"server gia' avviato (pid {_proc.Id} su porta {_port})");
            var bin = ResolveBinary();
            if (bin is null) return Err("binario server non trovato (GMBASIC_LLM_SERVER o native/gmb_llm_server/build)");
            var dir = Path.GetFullPath(modelDir);
            if (!Directory.Exists(dir)) return Err($"model dir inesistente: {dir}");
            try
            {
                var psi = new ProcessStartInfo(bin, $"\"{dir}\" {port}")
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                };
                if (maxActive > 0) psi.Environment["GMB_MAX_ACTIVE"] = maxActive.ToString();
                var p = Process.Start(psi);
                if (p is null) return Err("Process.Start ha ritornato null");
                _proc = p; _port = port; _modelDir = dir;
            }
            catch (Exception ex)
            {
                _proc = null;
                return Err($"avvio fallito: {ex.Message}");
            }
            var deadline = DateTime.UtcNow.AddSeconds(60);
            while (DateTime.UtcNow < deadline)
            {
                if (_proc.HasExited)
                    return Err($"server uscito durante l'avvio (code {_proc.ExitCode})");
                var s = ProbeStatus();
                if (s is { Ok: true, Loaded: true }) return s;
                Thread.Sleep(250);
            }
            return Err("timeout readiness (server non pronto in 60s)");
        }
    }

    /// <summary>Ferma il processo (kill) se attivo.</summary>
    public static AiServerState Stop()
    {
        lock (Gate)
        {
            if (_proc is null || _proc.HasExited) { _proc = null; return new(false, 0, _port, _modelDir, false, false, null); }
            try { _proc.Kill(entireProcessTree: true); }
            catch (Exception ex) { return Err($"kill fallito: {ex.Message}"); }
            try { _proc.WaitForExit(3000); } catch { }
            _proc = null;
            return new(false, 0, _port, _modelDir, false, false, null);
        }
    }

    static AiServerState Err(string msg) => new(false, _proc?.Id ?? 0, _port, _modelDir, false, false, msg);

    static HttpClient Http()
    {
        var h = new HttpClient { Timeout = TimeSpan.FromSeconds(10) };
        h.DefaultRequestHeaders.ConnectionClose = false;
        return h;
    }

    /// <summary>Stato live: interroga GET /v1/status del processo gestito.</summary>
    public static AiServerState Status()
    {
        lock (Gate)
        {
            if (_proc is null)
                return Err("nessun server avviato");
            if (_proc.HasExited)
                return Err("processo uscito");
            return ProbeStatus() ?? Err("GET /v1/status non riuscito");
        }
    }

    static AiServerState? ProbeStatus()
    {
        if (_proc is null || _proc.HasExited) return null;
        try
        {
            using var h = Http();
            using var resp = h.GetAsync($"http://127.0.0.1:{_port}/v1/status").GetAwaiter().GetResult();
            if (!resp.IsSuccessStatusCode) return null;
            using var doc = JsonDocument.Parse(resp.Content.ReadAsStringAsync().GetAwaiter().GetResult());
            var r = doc.RootElement;
            bool loaded = r.TryGetProperty("loaded", out var l) && l.ValueKind == JsonValueKind.True;
            bool busy = r.TryGetProperty("busy", out var b) && b.ValueKind == JsonValueKind.True;
            string? model = r.TryGetProperty("model", out var m) ? m.GetString() : null;
            return new AiServerState(true, _proc!.Id, _port, model, loaded, busy, null);
        }
        catch { return null; }
    }

    static AiServerState Post(string endpoint, string jsonBody, bool expectLoaded)
    {
        var st = Status();
        if (!st.Ok || !st.Running) return Err("server non avviato");
        try
        {
            using var h = Http();
            using var content = new StringContent(jsonBody, Encoding.UTF8, "application/json");
            using var resp = h.PostAsync($"http://127.0.0.1:{st.Port}{endpoint}", content).GetAwaiter().GetResult();
            var body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            using var doc = JsonDocument.Parse(body);
            var r = doc.RootElement;
            string? msg = r.TryGetProperty("error", out var e) && e.TryGetProperty("message", out var mm) ? mm.GetString() : null;
            bool loaded = r.TryGetProperty("loaded", out var l) && l.ValueKind == JsonValueKind.True;
            if (!resp.IsSuccessStatusCode || msg is not null)
                return new(true, st.Pid, st.Port, st.Model, st.Loaded, st.Busy, msg ?? $"HTTP {(int)resp.StatusCode}");
            if (expectLoaded && !loaded)
                return new(true, st.Pid, st.Port, st.Model, false, st.Busy, "server non riporta loaded");
            return Status() ?? Err("stato non leggibile dopo operazione");
        }
        catch (Exception ex)
        {
            return new(true, st.Pid, st.Port, st.Model, st.Loaded, st.Busy, ex.Message);
        }
    }

    /// <summary>Residenza: carica e tiene residente il modello dir (POST /v1/models/load).</summary>
    public static AiServerState LoadModel(string dir)
    {
        lock (Gate)
        {
            var full = Path.GetFullPath(dir);
            if (!Directory.Exists(full)) return Err($"model dir inesistente: {full}");
            return Post("/v1/models/load", $"{{\"model_dir\":\"{JsonEsc(full)}\"}}", expectLoaded: true);
        }
    }

    /// <summary>Residenza: libera il modello residente (POST /v1/models/unload).</summary>
    public static AiServerState UnloadModel()
    {
        lock (Gate) { return Post("/v1/models/unload", "{}", expectLoaded: false); }
    }

    static string JsonEsc(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n");
}
