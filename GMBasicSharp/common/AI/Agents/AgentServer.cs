// FILE: Basic.Agents.AgentServer
using Basic.Mcp;
using Basic.Providers;
using Basic.SQLite.Vector;
using Basic.AI.Rag;

namespace Basic.Agents;

/// <summary>In-process agent registry with chat history and provider-backed ASK.</summary>
/// <remarks>
/// ponytail: singleton + JSON SAVE/LOAD; upgrade path: ISAM transactional store.
///
/// Locking discipline (added when it became clear GMBASIC's PCodeVm runs real GOMAXPROCS
/// worker threads -- see PCodeVm.cs -- so two goroutines calling AIAGENT.ASK on two different
/// agents is a real, not hypothetical, scenario): a single class-wide lock would serialize
/// every ASK call process-wide regardless of which agent/model, defeating the whole point of
/// GOMAXPROCS. Instead:
///   - <see cref="_dirLock"/> protects ONLY the <c>_agents</c> dictionary's membership and the
///     <c>_hotSwappableSessions</c> LRU list -- always held briefly, never across a native call.
///   - Each <see cref="AgentRecord.StateLock"/> protects that ONE agent's mutable fields (incl.
///     its native session) and is held for the duration of that agent's ASK -- so two different
///     agents' sessions can genuinely decode in parallel on separate cores.
///   - Invariant that makes this deadlock-free without a lock-ordering scheme: no thread ever
///     holds more than one AgentRecord.StateLock at the same time. FASE 2b eviction needs to
///     touch a DIFFERENT agent's (the victim's) state -- that always happens as its own
///     fully self-contained acquire/mutate/release (<see cref="SwapOutAgent"/>), completed and
///     released BEFORE the calling agent's own StateLock is ever acquired in <see cref="Ask"/>.
///   - <see cref="_dirLock"/> is always the innermost lock relative to StateLock (acquired only
///     while holding at most the CURRENT thread's own agent's StateLock, never anyone else's,
///     and released before any other AgentRecord lock is taken) -- so it never participates in
///     a cycle either.
/// </remarks>
public sealed class AgentServer
{
    public static AgentServer Instance { get; } = new();

    private readonly Dictionary<string, AgentRecord> _agents = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _dirLock = new();

    /// <summary>FASE 2b: agents with a currently-live (non-swapped) session on a completer that
    /// opted into swap eviction (<c>kvSwapEnabled=true</c>), oldest-used first. Everything else
    /// -- swap-disabled completers, or completers that don't implement
    /// <see cref="IDiskSwappableSessionCompleter"/> at all -- never appears here and behaves
    /// exactly like pre-FASE-2b: unlimited hot sessions. Protected by <see cref="_dirLock"/>.</summary>
    private readonly LinkedList<AgentRecord> _hotSwappableSessions = new();
    private AgentMemoryStore? _memoryStore;
    private int _memoryInitLock;

    public AgentMemoryStore GetMemoryStore()
    {
        if (_memoryStore is null && Interlocked.CompareExchange(ref _memoryInitLock, 1, 0) == 0)
        {
            try
            {
                var conn = new Microsoft.Data.Sqlite.SqliteConnection("Data Source=:memory:");
                conn.Open();
                using var cmd = conn.CreateCommand();
                cmd.CommandText = "PRAGMA journal_mode=WAL";
                cmd.ExecuteNonQuery();
                _memoryStore = new AgentMemoryStore(conn);
            }
            finally
            {
                _memoryInitLock = 0;
            }
        }
        return _memoryStore!;
    }

    private AgentServer()
    {
    }

    /// <summary>Livello di permesso DEFAULT applicato ai nuovi agenti (impostato con
    /// `AI PERMISSION DEFAULT ...`). Default v1 = YOLO (per non rompere i sample esistenti).</summary>
    public AgentPermissionLevel PermissionDefault { get; set; } = AgentPermissionLevel.YOLO;

    public bool Create(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
        {
            if (_agents.ContainsKey(name))
                return false;

            _agents[name] = new AgentRecord { Name = name, PermissionLevel = PermissionDefault };
        }

        AgentEventLog.Instance.Record(AgentEventKind.Created, name, string.Empty);
        return true;
    }

    public bool Delete(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);

        AgentRecord? agent;
        lock (_dirLock)
        {
            if (!_agents.Remove(name, out agent))
                return false;
        }

        lock (agent!.StateLock)
        {
            if (agent.SessionOwner is not null && agent.SessionId is not null)
                agent.SessionOwner.DestroySession(agent.SessionId.Value);
            if (agent.SwappedPathPrefix is { } path)
                DeleteSwapFiles(path); // FASE 2b: a session swapped to disk isn't reachable via SessionOwner/SessionId -- clean it up explicitly or it leaks on disk forever
        }

        lock (_dirLock)
            _hotSwappableSessions.Remove(agent);

        AgentEventLog.Instance.Record(AgentEventKind.Deleted, name, string.Empty);
        return true;
    }

    public bool Exists(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
            return _agents.ContainsKey(name);
    }

    public List<AgentRecord> List()
    {
        lock (_dirLock)
            return [.._agents.Values];
    }

    /// <summary>One-line operational summary — the "AGENT STATUS" primitive of the minimal
    /// Agent Operating Center: model/provider/history/tool-count, without needing a dashboard.</summary>
    public string GetStatusSummary(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var agent = GetAgentRef(name);
        lock (agent.StateLock)
        {
            return $"name={agent.Name} id={agent.Id} model={agent.Model} " +
                   $"provider={agent.ProviderName ?? "(active)"} browser={agent.BrowserProvider} temperature={agent.Temperature.ToString(System.Globalization.CultureInfo.InvariantCulture)} " +
                    $"maxTokens={agent.MaxTokens} historyCount={agent.History.Count} toolCount={agent.ToolNames.Count}";
        }
    }

    /// <summary>Ritorna il completer cache-manager e la sessionId della sessione KV dell'agente,
    /// se l'agente esiste e il suo completer implementa <see cref="IKvCacheManager"/>. Ritorna
    /// null altrimenti (agente inesistente, nessuna sessione, completer fixture).</summary>
    public (IKvCacheManager Manager, int SessionId)? GetAgentCacheManager(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        AgentRecord? agent;
        lock (_dirLock)
            _agents.TryGetValue(name, out agent);
        if (agent is null)
            return null;
        lock (agent.StateLock)
        {
            if (agent.SessionOwner is not IKvCacheManager mgr || agent.SessionId is not { } sid)
                return null;
            return (mgr, sid);
        }
    }

    /// <summary>Ritorna il completer session-aware e la sessionId della sessione dell'agente,
    /// se l'agente esiste e il suo completer è session-aware. Ritorna null altrimenti
    /// (agente inesistente, nessuna sessione, completer non-session-aware, fixture).</summary>
    public (ISessionAwareLlmCompleter Completer, int SessionId)? GetAgentCompleter(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        AgentRecord? agent;
        lock (_dirLock)
            _agents.TryGetValue(name, out agent);
        if (agent is null)
            return null;
        lock (agent.StateLock)
        {
            if (agent.SessionOwner is not { } owner || agent.SessionId is not { } sid)
                return null;
            return (owner, sid);
        }
    }

    /// <summary>Prefill della sessione KV dell'agente: append del testo come turno nel formato
    /// della chat (role: "user" default, "system" esplicito). Ritorna un JSON con esito o
    /// errore esplicito se l'agente non esiste, non ha una sessione attiva o il completer non
    /// è session-aware. Mai un risultato silenzioso.</summary>
    public string Prefill(string name, string role, string text)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(text);
        var pair = GetAgentCompleter(name);
        if (pair is null)
            return $"{{\"error\":\"'{name}' non risolve a un agente con sessione KV (agente non registrato o non avviato)\"}}";
        var (completer, sid) = pair.Value;
        try
        {
            var pre = GetAgentCacheStats(name);
            var sw = System.Diagnostics.Stopwatch.StartNew();
            completer.AppendMessage(sid, string.IsNullOrWhiteSpace(role) ? "user" : role, text);
            sw.Stop();
            var post = GetAgentCacheStats(name);
            _prefillStats[name] = new RunStats(DeltaTokens(pre, post), sw.ElapsedMilliseconds, pre, post);
        }
        catch (Exception ex)
        {
            return $"{{\"error\":\"prefill fallito: {ex.Message}\"}}";
        }
        var stats = GetAgentCacheStats(name);
        var effRole = string.IsNullOrWhiteSpace(role) ? "user" : role;
        return $"{{\"ok\":true,\"context\":\"{name}\",\"role\":\"{effRole}\",\"stats\":{(stats ?? "null")}}}";
    }

    /// <summary>Delta token tra due snapshot stats KV (post.NTokens - pre.NTokens). Se uno dei
    /// due snapshot è null (completer non cache-manager), ritorna la lunghezza di una stima
    /// minima (1) — il chiamante usa il valore solo come ordine di grandezza delle prestazioni.</summary>
    private static int DeltaTokens(string? pre, string? post)
    {
        if (pre is null || post is null) return 1;
        int? nTokensOf(string? j)
        {
            if (j is null) return null;
            try
            {
                using var d = System.Text.Json.JsonDocument.Parse(j);
                if (d.RootElement.TryGetProperty("nTokens", out var t) && t.ValueKind == System.Text.Json.JsonValueKind.Number)
                    return t.GetInt32();
            }
            catch { /* malformato */ }
            return null;
        }
        var a = nTokensOf(pre);
        var b = nTokensOf(post);
        if (a is null || b is null) return 1;
        var delta = b.Value - a.Value;
        return delta >= 0 ? delta : 1;
    }

    /// <summary>Decode (generazione) dalla sessione KV dell'agente, senza appendere un nuovo
    /// turno user: il modello continua dal pending. maxTokens &gt; 0: limite esplicito.
    /// Ritorna JSON con il testo o errore esplicito. Mai un risultato silenzioso.</summary>
    public string Generate(string name, int maxTokens)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var pair = GetAgentCompleter(name);
        if (pair is null)
            return $"{{\"error\":\"'{name}' non risolve a un agente con sessione KV (agente non registrato o non avviato)\"}}";
        var (completer, sid) = pair.Value;
        try
        {
            var pre = GetAgentCacheStats(name);
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var text = completer.GenerateFromSession(sid, maxTokens > 0 ? maxTokens : 0);
            sw.Stop();
            var post = GetAgentCacheStats(name);
            _decodeStats[name] = new RunStats(text.Length, sw.ElapsedMilliseconds, pre, post);
            return System.Text.Json.JsonSerializer.Serialize(new { ok = true, context = name, text });
        }
        catch (Exception ex)
        {
            return $"{{\"error\":\"decode fallito: {ex.Message}\"}}";
        }
    }

    /// <summary>Ultime prestazioni di prefill/decode per agente (AI.PREFILLSTATS/AI.DECODESTATS):
    /// token del delta KV, durata ms, stats prima/dopo. Misurate nel runtime — mai stub.</summary>
    public sealed record RunStats(int Tokens, long ElapsedMs, string? StatsBefore, string? StatsAfter);

    private readonly Dictionary<string, RunStats> _prefillStats = new();
    private readonly Dictionary<string, RunStats> _decodeStats = new();
    private readonly Dictionary<string, string> _benchResult = new();
    private readonly Dictionary<string, int> _inputTokens = new(); // E6: ultimo input_tokens/turn per agente
    // FreeToken §4.4: semantic anchor checkpoints per agente (kind, ctxChars, ms) ai confini
    // toolCall/toolResult/turn — per misurare il delta ri-prefillabile (risparmio potenziale).
    private readonly Dictionary<string, List<(string Kind, long Chars, long Ms)>> _anchors = new();

    /// <summary>Registra un anchor di contesto per l'agente (boundary del tool-loop).</summary>
    public void RecordAnchor(string name, string kind, long chars)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(kind)) return;
        lock (_dirLock)
        {
            if (!_anchors.TryGetValue(name, out var list)) { list = []; _anchors[name] = list; }
            list.Add((kind, chars, Environment.TickCount64));
            if (list.Count > 64) list.RemoveAt(0);
        }
    }

    /// <summary>AI.ANCHORSTATS(alias) — ultimi anchor + delta (risparmio potenziale FreeToken).
    /// JSON: {anchors:N, last, totalChars, deltaSinceLastToolResult, savedChars}.</summary>
    public string GetAnchorStats(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
        {
            if (!_anchors.TryGetValue(name, out var list) || list.Count == 0)
                return $"{{\"agent\":\"{name}\",\"error\":\"nessun anchor (tool-loop mai eseguito)\"}}";
            var last = list[^1];
            var lastToolResult = -1L;
            for (var i = list.Count - 1; i >= 0; i--)
                if (list[i].Kind == "toolResult") { lastToolResult = list[i].Chars; break; }
            var delta = lastToolResult >= 0 ? Math.Max(0, last.Chars - lastToolResult) : last.Chars;
            return $"{{\"agent\":\"{name}\",\"anchors\":{list.Count},\"lastKind\":\"{last.Kind}\",\"ctxChars\":{last.Chars}," +
                   $"\"deltaSinceToolResult\":{delta},\"pinnedEstimateChars\":{Math.Max(0, last.Chars - delta)},\"ms\":{last.Ms}}}";
        }
    }

    /// <summary>AI.PREFILLSTATS(alias) — ultima misura di prefill (token delta KV + durata).
    /// JSON con tok/s; errore esplicito se nessuna misura ancora. Mai stub.</summary>
    public string GetPrefillStats(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
        {
            if (!_prefillStats.TryGetValue(name, out var s))
                return $"{{\"error\":\"'{name}' non ha ancora misure di prefill\"}}";
            var tps = s.ElapsedMs > 0 ? s.Tokens * 1000.0 / s.ElapsedMs : 0.0;
            return $"{{\"context\":\"{name}\",\"tokens\":{s.Tokens},\"elapsedMs\":{s.ElapsedMs},\"tokPerSec\":{tps.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture)}}}";
        }
    }

    /// <summary>AI.DECODESTATS(alias) — ultima misura di decode (token delta KV + durata).
    /// JSON con tok/s; errore esplicito se nessuna misura ancora. Mai stub.</summary>
    public string GetDecodeStats(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
        {
            if (!_decodeStats.TryGetValue(name, out var s))
                return $"{{\"error\":\"'{name}' non ha ancora misure di decode\"}}";
            var tps = s.ElapsedMs > 0 ? s.Tokens * 1000.0 / s.ElapsedMs : 0.0;
            return $"{{\"context\":\"{name}\",\"tokens\":{s.Tokens},\"elapsedMs\":{s.ElapsedMs},\"tokPerSec\":{tps.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture)}}}";
        }
    }


    /// <summary>AI BENCHMARK: esegue N iterazioni di prefill/decode sulla sessione KV
    /// dell'agente e registra l'ultimo benchmark (lettura via AI.BENCHRESULT(alias)).
    /// mode: PREFILL | DECODE | ALL. Ritorna JSON con il risultato aggregato (avg/min/max
    /// tok/s) o errore esplicito. Mai uno stub: nessun risultato silenzioso.</summary>
    public string Benchmark(string name, string mode, int iterations, string text)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var m = mode.Trim().ToUpperInvariant();
        if (m is not ("PREFILL" or "DECODE" or "ALL"))
            return "{\"error\":\"modalità benchmark non valida: '" + mode + "' (attese PREFILL|DECODE|ALL)\"}";
        if (iterations < 1) iterations = 1;
        var pair = GetAgentCompleter(name);
        if (pair is null)
            return "{\"error\":\"'" + name + "' non risolve a un agente con sessione KV (agente non registrato o non avviato)\"}";
        var (completer, sid) = pair.Value;
        try
        {
            var blocks = new List<string>();
            if (m is "PREFILL" or "ALL")
                blocks.Add(RunBenchBlock("prefill", iterations, () =>
                {
                    var pre = GetAgentCacheStats(name);
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    completer.AppendMessage(sid, "user", string.IsNullOrWhiteSpace(text) ? "Benchmark prefill." : text);
                    sw.Stop();
                    var post = GetAgentCacheStats(name);
                    return (DeltaTokens(pre, post), sw.ElapsedMilliseconds);
                }));
            if (m is "DECODE" or "ALL")
                blocks.Add(RunBenchBlock("decode", iterations, () =>
                {
                    var pre = GetAgentCacheStats(name);
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    var outText = completer.GenerateFromSession(sid, 0);
                    sw.Stop();
                    var post = GetAgentCacheStats(name);
                    return (outText.Length, sw.ElapsedMilliseconds);
                }));
            var json = "{" + string.Join(",", blocks) + "}";
            lock (_dirLock)
                _benchResult[name] = json;
            return "{\"ok\":true,\"context\":\"" + name + "\",\"mode\":\"" + m + "\",\"iterations\":" + iterations + ",\"result\":" + json + "}";
        }
        catch (Exception ex)
        {
            return "{\"error\":\"benchmark fallito: " + ex.Message + "\"}";
        }
    }

    /// <summary>Esegue N iterazioni di un run di benchmark (prefill o decode) e formatta il
    /// blocco JSON aggregato {mode,iterations,tokens,elapsedMs,avg/min/maxTokPerSec}.</summary>
    private static string RunBenchBlock(string mode, int iterations, Func<(int Tokens, long Ms)> one)
    {
        var rates = new List<double>(); long tok = 0; long ms = 0;
        for (var i = 0; i < iterations; i++)
        {
            var (t, e) = one();
            tok += t; ms += e;
            if (e > 0) rates.Add(t * 1000.0 / e);
        }
        var avg = rates.Count > 0 ? rates.Average() : 0.0;
        var min = rates.Count > 0 ? rates.Min() : 0.0;
        var max = rates.Count > 0 ? rates.Max() : 0.0;
        var c = System.Globalization.CultureInfo.InvariantCulture;
        return "\"" + mode + "\":{\"mode\":\"" + mode + "\",\"iterations\":" + iterations + ",\"tokens\":" + tok + ",\"elapsedMs\":" + ms + "," +
               "\"avgTokPerSec\":" + avg.ToString("0.0", c) + ",\"minTokPerSec\":" + min.ToString("0.0", c) + ",\"maxTokPerSec\":" + max.ToString("0.0", c) + "}";
    }

    /// <summary>AI.BENCHRESULT(alias) — ultimo benchmark registrato per agente (JSON).
    /// Errore esplicito se nessun benchmark ancora. Mai uno stub.</summary>
    public string GetBenchResult(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
        {
            if (!_benchResult.TryGetValue(name, out var j))
                return "{\"error\":\"'" + name + "' non ha ancora un benchmark\"}";
            return j;
        }
    }

    // ── E6: monitoraggio input_tokens/turn (efficienza agenti) ─────────────────

    /// <summary>Conta i token del contesto di input dell'agente (system + history + prompt
    /// corrente) se il completer espone ITokenizingCompleter; altrimenti -1. Mai stub: -1
    /// esplicito se non conteggiabile.</summary>
    private void RecordInputTokens(AgentRecord agent, ILlmCompleter completer, string prompt)
    {
        if (completer is not ITokenizingCompleter tc)
        {
            lock (_dirLock) _inputTokens[agent.Name] = -1;
            return;
        }
        try
        {
            var ctx = string.Concat(agent.ResolveSystemPrompt(), "\n", BuildTranscript(agent.History), "\n", prompt);
            var n = tc.CountTokens(ctx);
            lock (_dirLock) _inputTokens[agent.Name] = n >= 0 ? n : -1;
        }
        catch
        {
            lock (_dirLock) _inputTokens[agent.Name] = -1;
        }
    }

    /// <summary>AI.INPUTTOKENS(alias) / lettura — ultimo input_tokens/turn dell'agente (conteggio
    /// token del contesto di input). -1 se non conteggiabile (completer non-tokenizing).</summary>
    public int GetInputTokens(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_dirLock)
            return _inputTokens.TryGetValue(name, out var n) ? n : -1;
    }

    /// <summary>AI CACHE CLEAR: svuota la cache KV della sessione dell'agente. Ritorna JSON
    /// con esito o errore esplicito se l'agente non esiste/non ha sessione/non supporta
    /// IKvCacheManager. Mai un risultato silenzioso.</summary>
    public string ClearCache(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var pair = GetAgentCacheManager(name);
        if (pair is null)
            return $"{{\"error\":\"'{name}' non risolve a un agente con sessione KV cache-manager (agente non registrato, non avviato o completer fixture)\"}}";
        var (mgr, sid) = pair.Value;
        try
        {
            mgr.ClearCache(sid);
            return $"{{\"ok\":true,\"context\":\"{name}\",\"cleared\":true}}";
        }
        catch (Exception ex)
        {
            return $"{{\"error\":\"cache clear fallito: {ex.Message}\"}}";
        }
    }

    /// <summary>AI CACHE TRIM: rimuove i primi <paramref name="dropTokens"/> token dalla cache
    /// KV della sessione dell'agente (eviction dei piu' vecchi). Ritorna JSON con esito o
    /// errore esplicito. Mai un risultato silenzioso.</summary>
    public string TrimCache(string name, int dropTokens)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var pair = GetAgentCacheManager(name);
        if (pair is null)
            return $"{{\"error\":\"'{name}' non risolve a un agente con sessione KV cache-manager (agente non registrato, non avviato o completer fixture)\"}}";
        var (mgr, sid) = pair.Value;
        try
        {
            mgr.TrimCache(sid, dropTokens);
            return $"{{\"ok\":true,\"context\":\"{name}\",\"dropped\":{dropTokens}}}";
        }
        catch (Exception ex)
        {
            return $"{{\"error\":\"cache trim fallito: {ex.Message}\"}}";
        }
    }

    /// <summary>KV-cache stats della sessione dell'agente (via <see cref="IKvCacheManager"/> del
    /// completer attivo), in JSON. Ritorna null se l'agente non esiste, non ha una sessione
    /// attiva, o il suo completer non implementa <see cref="IKvCacheManager"/> (es. fixture).
    /// Mai un risultato silenzioso: l'assenza di supporto è esplicita (null + motivo).</summary>
    public string? GetAgentCacheStats(string name)
    {
        var pair = GetAgentCacheManager(name);
        if (pair is null)
            return null;
        var (mgr, sid) = pair.Value;
        lock (_dirLock) { /* no-op: sid/mgr gia' letti sotto StateLock */ }
        var s = mgr.GetCacheStats(sid);
        return $"{{\"nCtx\":{s.NCtx},\"nTokens\":{s.NTokens},\"bytes\":{s.Bytes},\"posMin\":{s.PosMin},\"posMax\":{s.PosMax},\"usedPercent\":{s.UsedPercent.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture)}}}";
    }

    public bool Set(string name, string key, string value)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        var agent = GetAgentRef(name);
        value = value.Trim().Trim('"');
        lock (agent.StateLock)
        {
            switch (key.Trim().ToLowerInvariant())
            {
                case "model":
                    agent.Model = value;
                    return true;
                case "temperature":
                    agent.Temperature = double.TryParse(value, System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out var temp) ? temp : 0.7;
                    return true;
                case "maxtokens":
                    agent.MaxTokens = int.TryParse(value, System.Globalization.NumberStyles.Integer,
                        System.Globalization.CultureInfo.InvariantCulture, out var max) && max > 0 ? max : 2048;
                    return true;
                case "systemprompt":
                    agent.SystemPrompt = value;
                    return true;
                case "provider":
                    agent.ProviderName = string.IsNullOrWhiteSpace(value) ? null : value;
                    return true;
                case "skills":
                case "skillsenabled":
                    if (bool.TryParse(value, out var sk) || value is "ON" or "on" or "OFF" or "off" || string.IsNullOrEmpty(value))
                        agent.SkillsEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    else
                        agent.SkillsEnabled = true;
                    return true;
                case "sessionstreaming":
                case "sessionstreamingenabled":
                    agent.SessionStreamingEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                case "clienttools":
                case "clienttoolsenabled":
                    agent.ClientToolsEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                case "toolretrieval":
                case "toolretrievalenabled":
                    agent.ToolRetrievalEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                case "memory":
                case "memoryenabled":
                    agent.MemoryEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                case "plugins":
                case "pluginsenabled":
                    agent.PluginsEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                // Livello AGENT dello scope dei cervelli (F2): dichiarazione CSV, es. "finance AS fin, airline".
                // Valore vuoto = nessuna dichiarazione (INHERIT dal livello PROCESS).
                case "brains":
                case "brain":
                    agent.Brains = value;
                    return true;
                case "thinking":
                case "thinkingenabled":
                    agent.ThinkingEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                case "samples":
                case "reasoningsamples":
                    agent.ReasoningSamples = int.TryParse(value, System.Globalization.NumberStyles.Integer,
                        System.Globalization.CultureInfo.InvariantCulture, out var samples) ? samples : 1;
                    if (agent.ReasoningSamples < 1) agent.ReasoningSamples = 1;
                    return true;
                case "profile":
                    agent.GovernanceProfile = int.TryParse(value, System.Globalization.NumberStyles.Integer,
                        System.Globalization.CultureInfo.InvariantCulture, out var gp) ? (gp == 2 ? 2 : 1) : 1;
                    return true;
                case "router":
                case "routerenabled":
                    agent.RouterEnabled = value is "ON" or "on" or "TRUE" or "true" or "1" or "yes";
                    return true;
                case "smallprovider":
                    agent.SmallProvider = string.IsNullOrWhiteSpace(value) ? null : value.Trim();
                    return true;
                case "frontierprovider":
                    agent.FrontierProvider = string.IsNullOrWhiteSpace(value) ? null : value.Trim();
                    return true;
                case "category":
                    agent.Category = string.IsNullOrWhiteSpace(value) ? null : value.Trim();
                    return true;
                case "permission":
                    agent.PermissionLevel = value.Trim().ToLowerInvariant() switch
                    {
                        "sandbox" => AgentPermissionLevel.Sandbox,
                        "supervised" => AgentPermissionLevel.Supervised,
                        "yolo" => AgentPermissionLevel.YOLO,
                        _ => throw new ArgumentException($"Livello permesso sconosciuto '{value}'. Usa sandbox|supervised|yolo.")
                    };
                    return true;
                case "toolpolicy":
                    // value = "tool|policy" oppure "tool=policy" (policy: allow|deny|approve)
                {
                    var sep = value.Contains('=') ? '=' : '|';
                    var parts = value.Split(sep, 2);
                    if (parts.Length != 2 || string.IsNullOrWhiteSpace(parts[0]))
                        throw new ArgumentException("toolpolicy richiede 'tool|policy' (allow|deny|approve).");
                    var pol = parts[1].Trim().ToLowerInvariant();
                    if (pol is not ("allow" or "deny" or "approve"))
                        throw new ArgumentException($"Policy sconosciuta '{pol}'. Usa allow|deny|approve.");
                    agent.ToolPolicies[parts[0].Trim()] = pol;
                    return true;
                }
                case "thoughttokens":
                    agent.ThinkingTokens = int.TryParse(value, System.Globalization.NumberStyles.Integer,
                        System.Globalization.CultureInfo.InvariantCulture, out var tt) && tt > 0 ? tt : 8;
                    return true;
                case "reflection":
                    agent.ReflectionMode = value.Trim().ToLowerInvariant() is "intra" or "inter" or "both" ? value.Trim().ToLowerInvariant() : "off";
                    return true;
                case "critic":
                    agent.CriticPersona = string.IsNullOrWhiteSpace(value) ? null : value;
                    return true;
                case "browser":
                case "browserprovider":
                    {
                        // Valore dalla superficie BASIC: codice numerico %AUTO/%CHROMIUM/%OBSCURA (controllo di
                        // compilazione) — accetta anche i legacy "auto"/"chromium"/"obscura" dai
                        // servizi interni/CLI. Mai stringhe nel BASIC per identificare il motore.
                        var v = value;
                        if (double.TryParse(v, System.Globalization.NumberStyles.Float,
                                System.Globalization.CultureInfo.InvariantCulture, out var code))
                            v = Basic.AI.Browser.BrowserProviderRegistry.EngineName(code);
                        else
                            v = Basic.AI.Browser.BrowserProviderRegistry.Normalize(v);
                        if (v is not ("chromium" or "obscura" or "auto"))
                            throw new ArgumentException($"Browser provider sconosciuto '{value}'. Usa %AUTO|%CHROMIUM|%OBSCURA.");
                        agent.BrowserProvider = v;
                        return true;
                    }
                default:
                    throw new ArgumentException($"Unknown agent property '{key}'.");
            }
        }
    }

    public string Get(string name, string key)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        var agent = GetAgentRef(name);
        lock (agent.StateLock)
        {
            return key.Trim().ToLowerInvariant() switch
            {
                "id" => agent.Id,
                "name" => agent.Name,
                "model" => agent.Model,
                "temperature" => agent.Temperature.ToString(System.Globalization.CultureInfo.InvariantCulture),
                "maxtokens" => agent.MaxTokens.ToString(System.Globalization.CultureInfo.InvariantCulture),
                "systemprompt" => agent.SystemPrompt,
                "provider" => agent.ProviderName ?? string.Empty,
                "skills" or "skillsenabled" => agent.SkillsEnabled ? "ON" : "OFF",
                "sessionstreaming" or "sessionstreamingenabled" => agent.SessionStreamingEnabled ? "ON" : "OFF",
                "clienttools" or "clienttoolsenabled" => agent.ClientToolsEnabled ? "ON" : "OFF",
                "memory" or "memoryenabled" => agent.MemoryEnabled ? "ON" : "OFF",
                "brains" or "brain" => agent.Brains,
                "thinking" or "thinkingenabled" => agent.ThinkingEnabled ? "ON" : "OFF",
                "permission" => agent.PermissionLevel.ToString(),
                "toolpolicies" => string.Join(",", agent.ToolPolicies.Select(kv => kv.Key + "=" + kv.Value)),
                "thoughttokens" => agent.ThinkingTokens.ToString(System.Globalization.CultureInfo.InvariantCulture),
                "samples" or "reasoningsamples" => agent.ReasoningSamples.ToString(System.Globalization.CultureInfo.InvariantCulture),
                "router" or "routerenabled" => agent.RouterEnabled ? "ON" : "OFF",
                "smallprovider" => agent.SmallProvider ?? string.Empty,
                "frontierprovider" => agent.FrontierProvider ?? string.Empty,
                "category" => agent.Category ?? string.Empty,
                "reflection" => agent.ReflectionMode,
                "critic" => agent.CriticPersona ?? string.Empty,
                "browser" or "browserprovider" => agent.BrowserProvider,
                _ => throw new ArgumentException($"Unknown agent property '{key}'.")
            };
        }
    }

    public string Ask(string name, string prompt)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(prompt);

        var agent = GetAgentRef(name);
        // Resolved and (if needed) evicted-for BEFORE agent.StateLock is acquired -- see the
        // class doc comment: eviction may need a DIFFERENT agent's (the victim's) StateLock,
        // and this class's whole deadlock-freedom argument rests on never holding two
        // AgentRecord.StateLocks at once. Reads agent.ProviderName/SessionOwner/SessionId here
        // without the lock -- reference/value field reads are atomic in .NET (no torn values),
        // so this is a benign, self-correcting staleness window at worst (matches the same
        // tolerance already accepted for the LRU candidate scan), never a corruption risk.
        var completer = ResolveCompleter(agent);
        if (completer is ISessionAwareLlmCompleter sacForEviction)
            MaybeEvictBeforeSessionLocked(agent, sacForEviction);
        RecordInputTokens(agent, completer, prompt);

        lock (agent.StateLock)
        {
            // Il tool-loop parte SOLO se serve davvero: binding esplicito dei tool dell'agente
            // (ToolNames) OPPURE il prompt matcha il trigger di un plugin/skill NOTO
            // (acquisizione automatica per-ask — vedi PluginIndex.MatchByTrigger: il contesto
            // cresce solo quando il task lo richiede). I tool "di default del registry" MCP NON
            // bastano piu' da soli: con server MCP configurati (config/mcp.json) il termine
            // `McpToolRegistry.Instance.List().Count > 0` faceva scattare il loop per OGNI ask,
            // degradando le risposte semplici dei modelli piccoli.
            var hasDefaultOrExplicitTools = agent.ToolNames.Count > 0
                || Basic.AI.PluginIndex.MatchByTrigger(prompt).Any();
            if (hasDefaultOrExplicitTools)
            {
                var dispatcher = McpRuntimeBridge.Dispatcher
                               ?? throw new InvalidOperationException(
                                   "MCP tool dispatcher is not available. Register tools and ensure Executor is running.");

                var toolCompleter = completer;
                if (toolCompleter is not FixtureToolCallingCompleter && toolCompleter is FixtureLlmCompleter)
                    toolCompleter = new FixtureToolCallingCompleter();

                // Sessione persistente: il tool-loop riusa la KV dell'agente (niente
                // creazione/distruzione per turno) invece di ri-tokenizzare il transcript.
                int? sessionId = null;
                if (toolCompleter is ISessionAwareLlmCompleter sacForTools)
                {
                    EnsureSessionLocked(agent, sacForTools);
                    sessionId = agent.SessionId;
                }

                var answer = AgentToolLoop.AskWithTools(agent, toolCompleter, dispatcher, prompt, sessionId: sessionId,
                    boundary: (kind, chars) => RecordAnchor(agent.Name, kind, chars));
                if (string.IsNullOrEmpty(agent.Model))
                    agent.Model = toolCompleter.ModelName;
                AgentEventLog.Instance.Record(AgentEventKind.Asked, name, $"prompt={prompt} answer={answer}");
                AgentReflection.Reflect(agent, completer, $"Utente: {prompt}\n\nAgente: {answer}");
                return answer;
            }

            // Quiet-STaR (9.3): se ThinkingEnabled, il prompt è wrappato con i marcatori di
            // pensiero — l'agente ragiona prima di rispondere (8 token di thought per default).
            // Il prompt wrappato va NELLA history (è ciò che viene inviato), così il transcript
            // e la sessione lo vedono entrambi.
            var effectivePrompt = agent.ThinkingEnabled
                ? $"<|start_of_thought|>Pensa per {agent.ThinkingTokens} token di pensiero prima di rispondere.<|end_of_thought|>\n{prompt}"
                : prompt;
            agent.History.Add(new AgentChatMessage("user", effectivePrompt));

            // T3.1 Differential Router (R1): con router=ON il path no-tool sceglie il completer
            // tra smallprovider (Local) e frontierprovider (Frontier) tramite il TES.
            ILlmCompleter? routedCompleter = null;
            if (agent.RouterEnabled)
            {
                var routedProvider = PickRoutedProvider(agent, prompt);
                routedCompleter = ProviderServer.Instance.GetCompletionByName(routedProvider!)
                    ?? throw new InvalidOperationException(
                        $"router: completer '{routedProvider}' non disponibile (impostare smallprovider/frontierprovider validi).");
                var decision = Basic.AI.Providers.DifferentialRouter.Decide(prompt ?? "", 256);
                AgentEventLog.Instance.Record(AgentEventKind.Asked, name,
                    $"router tier={decision.Tier} tes={decision.Tes.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture)} provider={routedProvider}");
            }

            string plainAnswer;
            if (completer is ISessionAwareLlmCompleter sac)
            {
                EnsureSessionLocked(agent, sac);
                sac.AppendMessage(agent.SessionId!.Value, "user", effectivePrompt);
                plainAnswer = sac.GenerateFromSession(agent.SessionId.Value, agent.MaxTokens);
            }
            else
            {
                var userPrompt = BuildTranscript(agent.History);
                var systemPrompt = agent.ResolveSystemPrompt(prompt);
                var completion = routedCompleter ?? completer;
                // T1.1 Best-of-N sampling+voting: con thinking ON e samples>1 si campiona N volte
                // la stessa completion e vince la maggioranza (robustezza su modelli stocastici).
                plainAnswer = agent.ThinkingEnabled && agent.ReasoningSamples > 1
                    ? Basic.Agents.Reasoning.BestOfN(() => completion.Complete(systemPrompt, userPrompt) ?? "", agent.ReasoningSamples)
                    : completion.Complete(systemPrompt, userPrompt);
            }

            agent.History.Add(new AgentChatMessage("assistant", plainAnswer));
            if (string.IsNullOrEmpty(agent.Model))
                agent.Model = completer.ModelName;
            AgentEventLog.Instance.Record(AgentEventKind.Asked, name, $"prompt={prompt} answer={plainAnswer}");
            AgentReflection.Reflect(agent, completer, $"Utente: {prompt}\n\nAgente: {plainAnswer}");
            return plainAnswer;
        }
    }

    // ── T1.2 Chained Recursive Reasoning (Blackboard-as-Artifacts, arXiv:2608.05124) ──
    // Task lunghi senza esplosione del contesto: ogni step produce un ARTEFATTO scritto sul
    // Blackboard (prefisso `bbPrefix`); lo step successivo riparte da un prompt che RIASSUME
    // lo stato accumulato dalla Blackboard (il "reset del contesto" è implicito: il completer
    // senza sessione non accumula il transcript precedente, l'unico stato portato avanti è il
    // riassunto degli artefatti). Ritorna la risposta dell'ULTIMO step.
    /// <summary>Legge l'argomento `brain` dal JSON di una tool call (per l'enforcement del grant, F5).
    /// Ritorna null se assente/illeggibile (in quel caso si usa lo scope dell'agente/task).</summary>
    private static string? BrainArgOf(string argumentsJson)
    {
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(argumentsJson);
            return doc.RootElement.TryGetProperty("brain", out var b) && b.ValueKind == System.Text.Json.JsonValueKind.String
                ? b.GetString() : null;
        }
        catch { return null; }
    }

    public string AskChained(string name, IReadOnlyList<string> steps, string bbPrefix = "chain")
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        if (steps is null || steps.Count == 0)
            throw new ArgumentException("AskChained: serve almeno uno step.");
        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);
        return AskChainedCore(agent, completer, steps, bbPrefix);
    }

    public string AskChainedCore(AgentRecord agent, ILlmCompleter completer,
        IReadOnlyList<string> steps, string bbPrefix)
    {
        ArgumentNullException.ThrowIfNull(completer);
        if (steps is null || steps.Count == 0)
            throw new ArgumentException("AskChainedCore: serve almeno uno step.");
        var last = "";
        foreach (var step in steps)
            last = AskChainedStepCore(agent, completer, step, bbPrefix);
        return last;
    }

    /// <summary>Un singolo anello della catena: legge lo stato accumulato sul Blackboard
    /// (<c>prefix_state</c>), compone il prompt col riassunto, genera con UN solo step (niente
    /// transcript cumulato = reset di contesto implicito), appende l'artefatto e ritorna la
    /// risposta. Chiamabile ripetutamente: ogni chiamata è il passo successivo della catena.</summary>
    public string AskChainedStep(string name, string prompt, string bbPrefix)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);
        return AskChainedStepCore(agent, completer, prompt, bbPrefix);
    }

    public string AskChainedStepCore(AgentRecord agent, ILlmCompleter completer,
        string step, string bbPrefix)
    {
        ArgumentNullException.ThrowIfNull(completer);
        ArgumentException.ThrowIfNullOrWhiteSpace(step);
        var system = agent.ResolveSystemPrompt();
        var stateKey = bbPrefix + "_state";
        var prior = Basic.Agents.Blackboard.Read(stateKey);
        var prompt = string.IsNullOrEmpty(prior)
            ? step
            : step + "\n\n[stato precedente (blackboard)]\n" + prior;
        string answer;
        if (agent.ThinkingEnabled && agent.ReasoningSamples > 1)
            answer = Basic.Agents.Reasoning.BestOfN(
                () => completer.Complete(system, prompt) ?? "", agent.ReasoningSamples);
        else
            answer = completer.Complete(system, prompt) ?? "";
        var count = string.IsNullOrEmpty(prior)
            ? 1
            : prior.Split('\n').Count(l => l.Length > 0) + 1;
        Basic.Agents.Blackboard.Write(bbPrefix + "_s" + count, answer);
        Basic.Agents.Blackboard.Write(stateKey,
            string.IsNullOrEmpty(prior) ? answer : prior + "\n" + answer);
        return answer;
    }

    /// <summary>Loop Brain→Azione NATIVO (CUA) indipendente da MCP: il completer dell'agente
    /// (es. modello locale MLX) decide le azioni browser.*/tool.search; le osservazioni reali
    /// rientrano nel transcript fino alla risposta finale. ui.*/computer.* → errore esplicito
    /// (richiedono AGENT.RUN su VM/C).</summary>
    /// <summary>Loop Brain→Azione NATIVO (CUA) indipendente da MCP/native-dispatch: il completer
    /// dell'agente (es. modello locale MLX, sessione KV persistente) genera testo; se contiene
    /// tool_calls JSON vengono eseguiti (browser.* reali via CDP, tool.search) e il risultato
    /// rientra nella sessione; si ripete fino alla risposta finale. Il path native
    /// (INativeToolDispatchCompleter) NON è usato: i suoi argomenti sono limitati a enum/boolean/
    /// numeri, incompatibili con le nostre azioni (url/expression liberi).</summary>
    public string AskCua(string name, string prompt, int rounds = 5)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(prompt);
        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);
        if (completer is ISessionAwareLlmCompleter sacForEviction)
            MaybeEvictBeforeSessionLocked(agent, sacForEviction);
        RecordInputTokens(agent, completer, prompt);

        lock (agent.StateLock)
        {
            var sac = completer as ISessionAwareLlmCompleter;
            int? sid = null;
            if (sac is not null)
            {
                EnsureSessionLocked(agent, sac);
                sid = agent.SessionId;
            }

            var codeMode = Basic.Providers.GmbEnv.Get("GMB_AGENT_MODE")?.Trim()
                .Equals("CODE", StringComparison.OrdinalIgnoreCase) == true;
            var userMsg = codeMode
                ? prompt + "\n\nRegola: rispondi SOLO con un programma GMBASIC completo (SUB Main ... END SUB). "
                  + "Nessun tool, nessun JSON, nessun testo extra."
                : prompt + "\n\n" + CuaAgentLoop.ToolInstructions();
            agent.History.Add(new AgentChatMessage("user", userMsg));
            if (sac is not null)
                sac.AppendMessage(sid!.Value, "user", userMsg);

            var systemTools = "## Available tools\n" + CuaAgentLoop.ToolInstructions();
            var attempted = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var lastAnswer = "";

            for (var round = 0; round < rounds; round++)
            {
                string answer = sac is not null
                    ? sac.GenerateFromSession(sid!.Value, agent.MaxTokens)
                    : completer.Complete(agent.ResolveSystemPrompt(prompt) + "\n" + systemTools, BuildTranscriptFor(agent));

                if (answer.Length > 0 && answer.Equals(lastAnswer, StringComparison.Ordinal))
                {
                    var msg = "Loop rilevato: risposta identica senza progresso. Interrompo.";
                    agent.History.Add(new AgentChatMessage("assistant", msg));
                    return msg;
                }
                lastAnswer = answer;

                // MODALITA' CODE (mondo C#): il brain ha risposto con un programma BASIC.
                // Lo esegue il runner registrato dall'host (Executor/REPL) — niente C.
                if (Basic.Providers.GmbEnv.Get("GMB_AGENT_MODE")?.Trim()
                        .Equals("CODE", StringComparison.OrdinalIgnoreCase) == true)
                {
                    if (Basic.Agents.CodeRunner.Instance is not { } runner)
                        return "error: CODE mode richiede un runner BASIC registrato (CodeRunner.Instance, host Executor/REPL).";
                    var cleaned = StripThinking(answer);
                    var fb = cleaned.IndexOf("```");
                    if (fb >= 0)
                    {
                        var rest = cleaned[(fb + 3)..];
                        var nl = rest.IndexOf('\n');
                        if (nl >= 0) rest = rest[(nl + 1)..];
                        var fe = rest.IndexOf("```");
                        cleaned = fe >= 0 ? rest[..fe] : rest;
                    }
                    return runner.RunBasic(cleaned);
                }

                var actionable = StripThinking(answer);
                var calls = ToolCallParser.TryParse(actionable) ?? [];
                if (calls.Count == 0)
                {
                    agent.History.Add(new AgentChatMessage("assistant", answer));
                    return answer;
                }

                agent.History.Add(new AgentChatMessage("assistant", answer));
                foreach (var call in calls)
                {
                    var key = call.Name + "|" + call.ArgumentsJson;
                    attempted[key] = attempted.TryGetValue(key, out var c) ? c + 1 : 1;
                    if (attempted[key] >= 3)
                    {
                        var msg = $"Loop rilevato: l'azione '{call.Name}' tentata 3 volte. Interrompo.";
                        agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {msg}"));
                        return msg;
                    }
                    var resultText = CuaAgentLoop.Execute(call.Name, call.ArgumentsJson);
                    agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                    AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, name,
                        $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
                    sac?.AppendMessage(sid!.Value, "tool", $"[{call.Name}] {resultText}");
                }
            }

            var exceeded = $"Tool-calling loop exceeded {rounds} rounds.";
            agent.History.Add(new AgentChatMessage("assistant", exceeded));
            return exceeded;
        }
    }

    /// <summary>AskGraph: l'agente esplora il knowledge graph del Second Brain con i tool
    /// graph.* (query/neighbors/path/visite/hub). Speculare ad AskCua ma sul grafo: il modello
    /// decide quali nodi/vicini/path interrogare per rispondere, il dispatch è C# nativo verso
    /// GraphMultiQuery (nessun codice BASIC, sola lettura del grafo).</summary>
    public string AskGraph(string name, string prompt, int rounds = 5, string? brainScope = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(prompt);
        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);
        if (completer is ISessionAwareLlmCompleter sacForEviction)
            MaybeEvictBeforeSessionLocked(agent, sacForEviction);
        RecordInputTokens(agent, completer, prompt);

        GraphAgentLoop.Register();   // assicura i tool graph.* nel registry

        lock (agent.StateLock)
        {
            var sac = completer as ISessionAwareLlmCompleter;
            int? sid = null;
            if (sac is not null)
            {
                EnsureSessionLocked(agent, sac);
                sid = agent.SessionId;
            }

            var userMsg = prompt + "\n\n" + GraphAgentLoop.ToolInstructions();
            agent.History.Add(new AgentChatMessage("user", userMsg));
            if (sac is not null)
                sac.AppendMessage(sid!.Value, "user", userMsg);

            var systemTools = "## Available tools\n" + GraphAgentLoop.ToolInstructions();
            var attempted = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var lastAnswer = "";

            for (var round = 0; round < rounds; round++)
            {
                string answer = sac is not null
                    ? sac.GenerateFromSession(sid!.Value, agent.MaxTokens)
                    : completer.Complete(agent.ResolveSystemPrompt(prompt) + "\n" + systemTools, BuildTranscriptFor(agent));

                if (answer.Length > 0 && answer.Equals(lastAnswer, StringComparison.Ordinal))
                {
                    var msg = "Loop rilevato: risposta identica senza progresso. Interrompo.";
                    agent.History.Add(new AgentChatMessage("assistant", msg));
                    return msg;
                }
                lastAnswer = answer;

                var actionable = StripThinking(answer);
                var calls = ToolCallParser.TryParse(actionable) ?? [];
                if (calls.Count == 0)
                {
                    agent.History.Add(new AgentChatMessage("assistant", answer));
                    return answer;
                }

                agent.History.Add(new AgentChatMessage("assistant", answer));
                foreach (var call in calls)
                {
                    var key = call.Name + "|" + call.ArgumentsJson;
                    attempted[key] = attempted.TryGetValue(key, out var c) ? c + 1 : 1;
                    if (attempted[key] >= 3)
                    {
                        var msg = $"Loop rilevato: l'azione '{call.Name}' tentata 3 volte. Interrompo.";
                        agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {msg}"));
                        return msg;
                    }
                    // F2: livello AGENT dello scope cervelli (null = nessuna dichiarazione → default del tool).
                    // F5: enforcement del grant — un `brain` fuori dallo scope dell'agente è un errore esplicito.
                    var resultText = Agents.AgentBrains.TryResolveForTool(agent, BrainArgOf(call.ArgumentsJson), brainScope, out var graphBrains, out var graphErr)
                        ? GraphAgentLoop.Execute(call.Name, call.ArgumentsJson, graphBrains)
                        : "error: " + graphErr;
                    agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                    AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, name,
                        $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
                    sac?.AppendMessage(sid!.Value, "tool", $"[{call.Name}] {resultText}");
                }
            }

            var exceeded = $"Tool-calling loop exceeded {rounds} rounds.";
            agent.History.Add(new AgentChatMessage("assistant", exceeded));
            return exceeded;
        }
    }

    /// <summary>AskCode: l'agente interroga il grafo semantico del CODICE con i tool code.*
    /// (symbol/callmap/impact/context/findsymbol/fileoutline/repomap). Speculare ad AskGraph ma sul
    /// codice: il dispatch è C# nativo verso SemanticAgentLoop (grafo C#), nessun codice BASIC.
    /// Il gemello C (VM/C, C99Native) usa gmb_agent_run_code + gmb_semquery — nessun P/Invoke.</summary>
    public string AskCode(string name, string prompt, int rounds = 5)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(prompt);
        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);
        if (completer is ISessionAwareLlmCompleter sacForEviction)
            MaybeEvictBeforeSessionLocked(agent, sacForEviction);
        RecordInputTokens(agent, completer, prompt);

        SemanticAgentLoop.Register();   // assicura i tool code.* nel registry

        lock (agent.StateLock)
        {
            var sac = completer as ISessionAwareLlmCompleter;
            int? sid = null;
            if (sac is not null) { EnsureSessionLocked(agent, sac); sid = agent.SessionId; }

            var userMsg = prompt + "\n\n" + SemanticAgentLoop.ToolInstructions();
            agent.History.Add(new AgentChatMessage("user", userMsg));
            if (sac is not null) sac.AppendMessage(sid!.Value, "user", userMsg);

            var systemTools = "## Available tools\n" + SemanticAgentLoop.ToolInstructions();
            var attempted = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var lastAnswer = "";

            for (var round = 0; round < rounds; round++)
            {
                string answer = sac is not null
                    ? sac.GenerateFromSession(sid!.Value, agent.MaxTokens)
                    : completer.Complete(agent.ResolveSystemPrompt(prompt) + "\n" + systemTools, BuildTranscriptFor(agent));

                if (answer.Length > 0 && answer.Equals(lastAnswer, StringComparison.Ordinal))
                {
                    var msg = "Loop rilevato: risposta identica senza progresso. Interrompo.";
                    agent.History.Add(new AgentChatMessage("assistant", msg));
                    return msg;
                }
                lastAnswer = answer;

                var actionable = StripThinking(answer);
                var calls = ToolCallParser.TryParse(actionable) ?? [];
                if (calls.Count == 0)
                {
                    agent.History.Add(new AgentChatMessage("assistant", answer));
                    return answer;
                }

                agent.History.Add(new AgentChatMessage("assistant", answer));
                foreach (var call in calls)
                {
                    var key = call.Name + "|" + call.ArgumentsJson;
                    attempted[key] = attempted.TryGetValue(key, out var c) ? c + 1 : 1;
                    if (attempted[key] >= 3)
                    {
                        var msg = $"Loop rilevato: l'azione '{call.Name}' tentata 3 volte. Interrompo.";
                        agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {msg}"));
                        return msg;
                    }
                    var resultText = SemanticAgentLoop.Execute(call.Name, call.ArgumentsJson);
                    agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                    AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, name,
                        $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
                    sac?.AppendMessage(sid!.Value, "tool", $"[{call.Name}] {resultText}");
                }
            }

            var exceeded = $"Tool-calling loop exceeded {rounds} rounds.";
            agent.History.Add(new AgentChatMessage("assistant", exceeded));
            return exceeded;
        }
    }

    /// <summary>AskRag: l'agente interroga i brain RAG del Second Brain con i tool rag.*
    /// (search/context/ask) per rispondere a una domanda grounded sul contenuto dei vault.
    /// Speculare ad AskGraph ma sul retrieval vettoriale; dispatch C# nativo verso
    /// MultiBrainSearch (sola lettura, nessun codice BASIC nel GPM).</summary>
    public string AskRag(string name, string prompt, int rounds = 5, string? brainScope = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(prompt);
        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);
        if (completer is ISessionAwareLlmCompleter sacForEviction)
            MaybeEvictBeforeSessionLocked(agent, sacForEviction);
        RecordInputTokens(agent, completer, prompt);

        RagAgentLoop.Register();   // assicura i tool rag.* nel registry

        lock (agent.StateLock)
        {
            var sac = completer as ISessionAwareLlmCompleter;
            int? sid = null;
            if (sac is not null)
            {
                EnsureSessionLocked(agent, sac);
                sid = agent.SessionId;
            }

            var userMsg = prompt + "\n\n" + RagAgentLoop.ToolInstructions();
            agent.History.Add(new AgentChatMessage("user", userMsg));
            if (sac is not null)
                sac.AppendMessage(sid!.Value, "user", userMsg);

            var systemTools = "## Available tools\n" + RagAgentLoop.ToolInstructions();
            var attempted = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var lastAnswer = "";

            for (var round = 0; round < rounds; round++)
            {
                string answer = sac is not null
                    ? sac.GenerateFromSession(sid!.Value, agent.MaxTokens)
                    : completer.Complete(agent.ResolveSystemPrompt(prompt) + "\n" + systemTools, BuildTranscriptFor(agent));

                if (answer.Length > 0 && answer.Equals(lastAnswer, StringComparison.Ordinal))
                {
                    var msg = "Loop rilevato: risposta identica senza progresso. Interrompo.";
                    agent.History.Add(new AgentChatMessage("assistant", msg));
                    return msg;
                }
                lastAnswer = answer;

                var actionable = StripThinking(answer);
                var calls = ToolCallParser.TryParse(actionable) ?? [];
                if (calls.Count == 0)
                {
                    agent.History.Add(new AgentChatMessage("assistant", answer));
                    return answer;
                }

                agent.History.Add(new AgentChatMessage("assistant", answer));
                foreach (var call in calls)
                {
                    var key = call.Name + "|" + call.ArgumentsJson;
                    attempted[key] = attempted.TryGetValue(key, out var c) ? c + 1 : 1;
                    if (attempted[key] >= 3)
                    {
                        var msg = $"Loop rilevato: l'azione '{call.Name}' tentata 3 volte. Interrompo.";
                        agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {msg}"));
                        return msg;
                    }
                    // F2: livello AGENT dello scope cervelli (null = nessuna dichiarazione → default del tool).
                    // F5: enforcement del grant — un `brain` fuori dallo scope dell'agente è un errore esplicito.
                    var resultText = Agents.AgentBrains.TryResolveForTool(agent, BrainArgOf(call.ArgumentsJson), brainScope, out var ragBrains, out var ragErr)
                        ? RagAgentLoop.Execute(call.Name, call.ArgumentsJson, ragBrains)
                        : "error: " + ragErr;
                    agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                    AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, name,
                        $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
                    sac?.AppendMessage(sid!.Value, "tool", $"[{call.Name}] {resultText}");
                }
            }

            var exceeded = $"Tool-calling loop exceeded {rounds} rounds.";
            agent.History.Add(new AgentChatMessage("assistant", exceeded));
            return exceeded;
        }
    }

    private static string StripThinking(string answer)
    {
        if (string.IsNullOrEmpty(answer)) return answer;
        int s = answer.IndexOf("<think>", StringComparison.OrdinalIgnoreCase);
        if (s < 0) return answer;
        int e = answer.IndexOf("</think>", s + 7, StringComparison.OrdinalIgnoreCase);
        if (e < 0) return answer;
        var tail = answer[(e + 8)..].TrimStart();
        return tail.Length > 0 ? tail : answer;
    }

    private static string BuildTranscriptFor(AgentRecord agent)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var m in agent.History)
            sb.AppendLine($"{m.Role}: {m.Content}");
        return sb.ToString().TrimEnd();
    }

    /// <summary>Ask con STREAMING: invoca <paramref name="onToken"/> per ogni delta di contenuto
    /// man mano che arriva (token per token con un completer <see cref="IStreamingLlmCompleter"/>).
    /// Se l'agente ha tool, sessione persistente o un completer non-streaming, viene consegnata
    /// la risposta intera in un'unica chiamata (fallback). La concatenazione dei delta è la
    /// risposta completa, registrata in history come per <see cref="Ask"/>.</summary>
    public void AskStream(string name, string prompt, Action<string> onToken)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(prompt);

        var agent = GetAgentRef(name);
        var completer = ResolveCompleter(agent);

        if (completer is IStreamingLlmCompleter sc
            && agent.ToolNames.Count == 0
            && completer is not ISessionAwareLlmCompleter)
        {
            lock (agent.StateLock)
            {
                agent.History.Add(new AgentChatMessage("user", prompt));
                var sb = new System.Text.StringBuilder();
                var userPrompt = BuildTranscript(agent.History);
                foreach (var chunk in sc.CompleteStream(agent.ResolveSystemPrompt(prompt), userPrompt))
                {
                    if (string.IsNullOrEmpty(chunk)) continue;
                    sb.Append(chunk);
                    onToken?.Invoke(chunk);
                }

                var full = sb.ToString();
                agent.History.Add(new AgentChatMessage("assistant", full));
                if (string.IsNullOrEmpty(agent.Model))
                    agent.Model = sc.ModelName;
                AgentEventLog.Instance.Record(AgentEventKind.Asked, name, $"prompt={prompt} answer={full}");
            }
            return;
        }

        // Agente con tool: tool-loop con streaming della risposta finale (i round dei tool
        // call sono interni, solo la finale arriva chunk per chunk). Il percorso NATIVO
        // (INativeToolDispatchCompleter) e le sessioni restano bloccanti -> chunk unico.
        if (agent.ToolNames.Count > 0 && completer is not ISessionAwareLlmCompleter)
        {
            lock (agent.StateLock)
            {
                var dispatcher = McpRuntimeBridge.Dispatcher
                               ?? throw new InvalidOperationException(
                                   "MCP tool dispatcher is not available. Register tools and ensure Executor is running.");
                var full = AgentToolLoop.AskWithToolsStream(agent, completer, dispatcher, prompt, onToken);
                if (string.IsNullOrEmpty(agent.Model))
                    agent.Model = completer.ModelName;
                AgentEventLog.Instance.Record(AgentEventKind.Asked, name, $"prompt={prompt} answer={full}");
            }
            return;
        }

        // Sessione persistente con streaming per-token (ISessionStreamingCompleter, abilitato
        // per agente via AI AGENT SET nome, sessionstreaming, ON — default OFF per l'overhead).
        if (completer is ISessionStreamingCompleter ssc
            && agent.SessionStreamingEnabled
            && agent.ToolNames.Count == 0)
        {
            lock (agent.StateLock)
            {
                agent.History.Add(new AgentChatMessage("user", prompt));
                EnsureSessionLocked(agent, ssc);
                ssc.AppendMessage(agent.SessionId!.Value, "user", prompt);
                var sb = new System.Text.StringBuilder();
                var full = ssc.GenerateFromSessionStream(agent.SessionId.Value, agent.MaxTokens, chunk =>
                {
                    if (string.IsNullOrEmpty(chunk)) return;
                    sb.Append(chunk);
                    onToken?.Invoke(chunk);
                });
                agent.History.Add(new AgentChatMessage("assistant", full));
                if (string.IsNullOrEmpty(agent.Model))
                    agent.Model = completer.ModelName;
                AgentEventLog.Instance.Record(AgentEventKind.Asked, name, $"prompt={prompt} answer={full}");
            }
            return;
        }

        // Fallback: sessione persistente o completer non-streaming -> risposta intera.
        onToken?.Invoke(Ask(name, prompt));
    }

    /// <summary>Bind an MCP tool name to an agent for ASK tool-calling.</summary>
    public bool RegisterTool(string agentName, string toolName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agentName);
        ArgumentException.ThrowIfNullOrWhiteSpace(toolName);
        var agent = GetAgentRef(agentName);
        lock (agent.StateLock)
        {
            if (McpToolRegistry.Instance.Get(toolName) is null)
                throw new KeyNotFoundException($"MCP tool '{toolName}' is not registered.");

            return agent.ToolNames.Add(toolName);
        }
    }

    /// <summary>Remove a tool binding from an agent.</summary>
    public bool UnregisterTool(string agentName, string toolName)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(agentName);
        ArgumentException.ThrowIfNullOrWhiteSpace(toolName);
        var agent = GetAgentRef(agentName);
        lock (agent.StateLock)
            return agent.ToolNames.Remove(toolName);
    }

    public void Save(string name, string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var agent = GetAgentRef(name);
        lock (agent.StateLock)
            AgentPersistence.SaveToFile(path, AgentPersistence.Capture(agent));
    }

    public void Load(string name, string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var snapshot = AgentPersistence.LoadFromFile(path);
        var agent = AgentPersistence.Restore(snapshot with { Name = name });
        lock (_dirLock)
            _agents[name] = agent;
    }

    /// <summary>Persists agent state to the transactional ISAM store (<c>agents.isdb</c>).
    /// In Commercial edition the store is `agents.isdb` (JSON dictionary, transactional via file lock);
    /// in Community it remains a placeholder per D11, but the file-based fallback is kept for verification.</summary>
    public void SaveToIsam(string name, string? storePath = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var agent = GetAgentRef(name);
        var path = string.IsNullOrWhiteSpace(storePath) ? "agents.isdb" : storePath!;
        var snapshot = AgentPersistence.Capture(agent);
        // Store as dictionary name -> snapshot (transactional via lock + atomic write)
        lock (_dirLock)
        {
            Dictionary<string, AgentPersistence.Snapshot> store;
            if (File.Exists(path))
            {
                try
                {
                    var json = File.ReadAllText(path);
                    store = System.Text.Json.JsonSerializer.Deserialize<Dictionary<string, AgentPersistence.Snapshot>>(json)
                            ?? new Dictionary<string, AgentPersistence.Snapshot>(StringComparer.OrdinalIgnoreCase);
                }
                catch { store = new Dictionary<string, AgentPersistence.Snapshot>(StringComparer.OrdinalIgnoreCase); }
            }
            else
            {
                store = new Dictionary<string, AgentPersistence.Snapshot>(StringComparer.OrdinalIgnoreCase);
            }
            store[name] = snapshot;
            var tmp = path + ".tmp";
            File.WriteAllText(tmp, System.Text.Json.JsonSerializer.Serialize(store, new System.Text.Json.JsonSerializerOptions{ WriteIndented = true }));
            File.Move(tmp, path, overwrite: true);
        }
    }

    /// <summary>Loads agent state from the ISAM store (<c>agents.isdb</c>).</summary>
    public void LoadFromIsam(string name, string? storePath = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var path = string.IsNullOrWhiteSpace(storePath) ? "agents.isdb" : storePath!;
        Dictionary<string, AgentPersistence.Snapshot> store;
        lock (_dirLock)
        {
            if (!File.Exists(path))
                throw new FileNotFoundException($"ISAM store not found: '{path}'.", path);
            var json = File.ReadAllText(path);
            store = System.Text.Json.JsonSerializer.Deserialize<Dictionary<string, AgentPersistence.Snapshot>>(json)
                    ?? throw new InvalidDataException($"ISAM store '{path}' is empty or invalid.");
        }
        if (!store.TryGetValue(name, out var snapshot))
            throw new KeyNotFoundException($"Agent '{name}' not found in ISAM store '{path}'.");
        var agent = AgentPersistence.Restore(snapshot with { Name = name });
        lock (_dirLock)
            _agents[name] = agent;
    }

    internal static void ResetForTests()
    {
        AgentEventLog.ResetForTests();
        lock (Instance._dirLock)
        {
            Instance._agents.Clear();
            Instance._hotSwappableSessions.Clear();
            Instance._anchors.Clear();
        }
    }

    /// <summary>Registra/aggiorna un record direttamente (bundle .bagent, restore non-da-file).
    /// Non registra eventi; chiude una sessione precedente con lo stesso nome se esiste.</summary>
    internal void Upsert(AgentRecord agent)
    {
        ArgumentNullException.ThrowIfNull(agent);
        lock (_dirLock)
        {
            if (_agents.TryGetValue(agent.Name, out var old))
            {
                lock (old.StateLock)
                {
                    if (old.SessionOwner is not null && old.SessionId is not null)
                        try { old.SessionOwner.DestroySession(old.SessionId.Value); } catch { }
                }
            }
            _agents[agent.Name] = agent;
        }
    }

    private AgentRecord GetAgentRef(string name)
    {
        lock (_dirLock)
            return _agents.TryGetValue(name, out var agent)
                ? agent
                : throw new KeyNotFoundException($"Agent '{name}' is not registered.");
    }

    /// <summary>FASE 2b, called BEFORE agent.StateLock is acquired (see class doc comment for
    /// why): if it looks like <paramref name="agent"/> is about to need a brand-new session and
    /// its completer opted into swap eviction, evicts the completer's own least-recently-used
    /// hot session (on a DIFFERENT agent) first, so <see cref="EnsureSessionLocked"/> doesn't
    /// have to. Best-effort on a pre-lock read of <paramref name="agent"/>'s fields -- if it
    /// raced and turns out to have been unnecessary or insufficient, EnsureSessionLocked simply
    /// proceeds anyway (fail open, same philosophy as a failed SwapOutSession call: never block
    /// or throw for a soft capacity heuristic).</summary>
    private void MaybeEvictBeforeSessionLocked(AgentRecord agent, ISessionAwareLlmCompleter sac)
    {
        var willNeedNewSession = agent.SwappedPathPrefix is null && (agent.SessionOwner != sac || agent.SessionId is null);
        if (!willNeedNewSession) return;

        if (sac is not IDiskSwappableSessionCompleter swappable) return;
        var policy = swappable.GetSwapPolicy();
        if (!policy.Enabled) return;

        // D1 (2026-09-10): due cap indipendenti, trip la prima che scatta. Il conteggio è
        // quello storico FASE 2b; il watermark RAM somma i byte di stato KV delle sessioni
        // calde di QUESTO completer (gmb_ai_session_cache_stats → llama_state_get_size, l'ingombro
        // reale) e va misurato FUORI da _dirLock (la GetCacheStats nativa prende lock propri:
        // mai annidare). Fail-open su ogni misura fallita (sessione morta/racing): quella
        // sessione contribute-zero, l'evict resta un'euristica soft.
        AgentRecord? victim;
        var hotForThisCompleter = new List<AgentRecord>();
        lock (_dirLock)
            foreach (var a in _hotSwappableSessions)
                if (ReferenceEquals(a.SessionOwner, swappable))
                    hotForThisCompleter.Add(a);

        if (policy.MaxKvBytes > 0 && swappable is IKvCacheManager kv)
        {
            var totalBytes = 0L;
            foreach (var a in hotForThisCompleter)
                if (a.SessionId is { } sid)
                {
                    try { totalBytes += kv.GetCacheStats(sid).Bytes; }
                    catch { /* raced/destroyed: fail-open, contributes 0 */ }
                }

            while (totalBytes > policy.MaxKvBytes && hotForThisCompleter.Count > 0)
            {
                var candidate = hotForThisCompleter[0];
                hotForThisCompleter.RemoveAt(0);
                if (ReferenceEquals(candidate, agent)) continue; // never evict the session we're about to use

                // Pre-lock best-effort read of the bytes this candidate would free (same
                // pre-lock-read philosophy as the method doc comment); SwapOutAgent itself
                // re-checks under candidate.StateLock that nothing raced.
                long freed = 0;
                if (candidate.SessionId is { } candidateSid)
                {
                    try { freed = kv.GetCacheStats(candidateSid).Bytes; } catch { freed = 0; }
                }

                SwapOutAgent(candidate, swappable, policy);

                if (candidate.SwappedPathPrefix is not null) // SwapOutAgent succeeded
                    totalBytes -= freed;
                if (totalBytes <= policy.MaxKvBytes) break;
            }
            return;
        }

        lock (_dirLock)
        {
            var current = _hotSwappableSessions.Where(a => ReferenceEquals(a.SessionOwner, swappable)).ToList();
            victim = current.Count >= policy.MaxHotSessions ? current[0] : null;
        }
        if (victim is null || ReferenceEquals(victim, agent)) return;

        SwapOutAgent(victim, swappable, policy);
    }

    /// <summary>Fully self-contained: acquires ONLY victim.StateLock (never anyone else's, never
    /// nested with a caller-held AgentRecord lock), does the swap-out, releases it, then
    /// separately touches _dirLock. See the class doc comment for why this shape is what makes
    /// the whole locking scheme deadlock-free.</summary>
    private void SwapOutAgent(AgentRecord victim, IDiskSwappableSessionCompleter swappable, SwapPolicy policy)
    {
        lock (victim.StateLock)
        {
            if (victim.SessionOwner != swappable || victim.SessionId is null)
                return; // raced since the candidate scan (already evicted/rebound by someone else) -- nothing to do

            var pathPrefix = MakeSwapPathPrefix(policy, victim);

            // Multi-process safety: two GMBASIC processes sharing this kvSwapPath must never
            // both write ".kv.tmp"/".meta.tmp" for the same pathPrefix at once (see
            // SwapPathLock's doc comment). Fail open on timeout -- same philosophy as a failed
            // SwapOutSession call: leave the victim live in RAM rather than block/lose it.
            using var fileLock = SwapPathLock.TryAcquire(pathPrefix, TimeSpan.FromSeconds(5));
            if (fileLock is null) return;

            if (!swappable.SwapOutSession(victim.SessionId.Value, pathPrefix))
                return; // fail open -- leave the victim's session live in RAM rather than lose it silently

            swappable.DestroySession(victim.SessionId.Value);
            victim.SwappedPathPrefix = pathPrefix;
            victim.SessionId = null;
            victim.SessionOwner = null;
        }

        lock (_dirLock)
            _hotSwappableSessions.Remove(victim);
    }

    /// <summary>FASE 2: rebinds/creates the agent's persistent native session if needed (first
    /// ASK, or the active provider changed since the last one). FASE 2b: if this agent's session
    /// was previously swapped to disk, restores it first. Called only from inside
    /// <c>lock (agent.StateLock)</c> in <see cref="Ask"/> -- capacity eviction (a DIFFERENT
    /// agent's lock) already happened before that lock was taken, in
    /// <see cref="MaybeEvictBeforeSessionLocked"/>, so this method only ever touches
    /// <paramref name="agent"/> itself plus the shared _dirLock-protected LRU list.</summary>
    private void EnsureSessionLocked(AgentRecord agent, ISessionAwareLlmCompleter sac)
    {
        var swappable = sac as IDiskSwappableSessionCompleter;
        var policy = swappable?.GetSwapPolicy() ?? default;

        if (agent.SwappedPathPrefix is { } swappedPath)
        {
            // Multi-process safety, same reasoning as SwapOutAgent -- but unlike swap-out,
            // swap-in has nothing to fall back to (the session's state exists ONLY on disk at
            // this point), so a lock timeout is a hard error, not fail-open.
            using var fileLock = SwapPathLock.TryAcquire(swappedPath, TimeSpan.FromSeconds(5))
                ?? throw new InvalidOperationException($"Agent '{agent.Name}': timed out waiting for the swap lock on '{swappedPath}' (another process holding it?).");

            var restored = swappable!.SwapInSession(swappedPath);
            if (restored < 0)
                throw new InvalidOperationException($"Agent '{agent.Name}': failed to restore swapped-out session from '{swappedPath}'.");

            DeleteSwapFiles(swappedPath); // now live in RAM again -- the on-disk copy is stale as of the next AppendMessage
            agent.SessionId = restored;
            agent.SessionOwner = sac;
            agent.SwappedPathPrefix = null;
            if (policy.Enabled) TouchHotSessionLocked(agent);
            return;
        }

        if (agent.SessionOwner == sac && agent.SessionId is not null)
        {
            if (policy.Enabled) TouchHotSessionLocked(agent); // still-live session, reused -- refresh LRU position
            return;
        }

        if (agent.SessionOwner is not null && agent.SessionId is not null)
        {
            agent.SessionOwner.DestroySession(agent.SessionId.Value);
            lock (_dirLock)
                _hotSwappableSessions.Remove(agent);
        }

        // Capacity eviction (if any) already attempted in MaybeEvictBeforeSessionLocked before
        // agent.StateLock was acquired. If that raced and capacity is still momentarily over
        // the configured cap, we still create the session here -- fail open, never block/throw
        // for a soft heuristic; the next ASK on any agent sharing this completer will catch up.
        agent.SessionId = sac.CreateSession();
        agent.SessionOwner = sac;
        if (!string.IsNullOrEmpty(agent.SystemPrompt))
            sac.AppendMessage(agent.SessionId.Value, "system", agent.ResolveSystemPrompt());

        if (policy.Enabled) TouchHotSessionLocked(agent);
    }

    private void TouchHotSessionLocked(AgentRecord agent)
    {
        lock (_dirLock)
        {
            _hotSwappableSessions.Remove(agent); // no-op if not present; linear scan is fine at human-agent-count scale
            _hotSwappableSessions.AddLast(agent);
        }
    }

    private static string MakeSwapPathPrefix(SwapPolicy policy, AgentRecord agent)
    {
        Directory.CreateDirectory(policy.SwapDir);
        return Path.Combine(policy.SwapDir, agent.Id); // Id, not Name -- stable even if an agent is deleted and a new one later reuses the same name
    }

    private static void DeleteSwapFiles(string pathPrefix)
    {
        try { File.Delete(pathPrefix + ".kv"); } catch { /* best effort */ }
        try { File.Delete(pathPrefix + ".meta"); } catch { /* best effort */ }
        // MLX swap files (FASE 2b MLX): .mlxkv (safetensors KV), .mlxmeta (binary meta),
        // .mlxchat (JSON chat bookkeeping). Deleting non-existent files for a llama.cpp-only
        // agent is an innocent no-op; same for the llama.cpp files when the agent was MLX.
        try { File.Delete(pathPrefix + ".mlxkv"); } catch { /* best effort */ }
        try { File.Delete(pathPrefix + ".mlxmeta"); } catch { /* best effort */ }
        try { File.Delete(pathPrefix + ".mlxchat"); } catch { /* best effort */ }
        // Deliberately NOT deleted from inside a `using (SwapPathLock...)` scope (the swap-in
        // call site) -- deleting a file you still hold FileShare.None open on yourself can
        // throw on Windows. Left to age out here instead, where nothing holds it: harmless,
        // tiny (0-byte) litter until this agent is eventually deleted or swapped again.
        try { File.Delete(pathPrefix + ".lock"); } catch { /* best effort */ }
    }

    /// <summary>Resolves the completer an agent should use. Deliberately does NOT go through
    /// SetActive(role,name)+GetActiveCompletion() as two separate calls when the agent has an
    /// explicit ProviderName pinned -- under real GOMAXPROCS parallelism, another agent's
    /// concurrent SetActive could interleave between those two calls and hand this agent the
    /// WRONG completer. GetCompletionByName resolves the named provider directly and atomically
    /// instead. GetActiveCompletion() (the shared "whatever's globally active" pointer) is only
    /// used when the agent has no pinned provider -- reading the shared pointer IS the intended
    /// semantics there.</summary>
    private static ILlmCompleter ResolveCompleter(AgentRecord agent)
    {
        var providerName = agent.ProviderName;
        return string.IsNullOrWhiteSpace(providerName)
            ? ProviderServer.Instance.GetActiveCompletion()
            : ProviderServer.Instance.GetCompletionByName(providerName);
    }

    /// <summary>T3.1 (R1): seleziona il provider per un prompt quando il router è ON — puro e
    /// testabile. OFF ⇒ null. Config mancante (smallprovider/frontierprovider) ⇒ errore ESPLICITO.</summary>
    public static string? PickRoutedProvider(AgentRecord agent, string prompt)
    {
        if (agent is null || !agent.RouterEnabled) return null;
        // B5: routing per CATEGORIA (A1) — se l'agente ha una categoria e il CategoryRegistry
        // ha un provider configurato (auto ON), quello vince sul Differential Router.
        if (!string.IsNullOrWhiteSpace(agent.Category))
        {
            var byCategory = Basic.AI.Providers.CategoryRegistry.Pick(agent.Category);
            if (!string.IsNullOrWhiteSpace(byCategory)) return byCategory;
        }
        if (string.IsNullOrWhiteSpace(agent.SmallProvider) || string.IsNullOrWhiteSpace(agent.FrontierProvider))
            throw new ArgumentException("router ON: impostare smallprovider e frontierprovider (AI AGENT SET).");
        var decision = Basic.AI.Providers.DifferentialRouter.Decide(prompt ?? "", 256);
        return decision.Tier == Basic.AI.Providers.RouterTier.Local
            ? agent.SmallProvider
            : agent.FrontierProvider;
    }

    private static string BuildTranscript(IReadOnlyList<AgentChatMessage> history)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var message in history)
            sb.AppendLine($"{message.Role}: {message.Content}");
        return sb.ToString().TrimEnd();
    }

}
