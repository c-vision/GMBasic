// FILE: Basic.AI.OpenAiServer (VERSIONE C# core del server OpenAI-compatible, MULTI-SESSIONE).
//
// Due versioni indipendenti (regola §0bis: C e C# mai bridge in-process):
//   - C (nativa): native/gmb_llm_server (parla con l'engine C direttamente).
//   - C# (questa): sopra il provider gestito (MlxLlmCompleter) — sessioni persistenti,
//     streaming token, tokenizer nativo, chat-template.
//
// Continuous batching (Fase 3b): il server ha una CODA di richieste e un SINGOLO thread che fa
// da scheduler (il thread che ha caricato il modello: lo stream MLX è thread-affine). Ogni round
// preleva fino a GMB_MAX_ACTIVE richieste "semplici" e, se >=2, le genera in UN forward batched
// per step (ISessionBatchCompleter.GenerateManyStream -> llm_ai_mlx_session_generate_batch,
// parità bit col decode sequenziale). Le richieste con tools/json-schema o i provider senza
// supporto batch restano sul percorso per-sessione. Gate GMB_SERVER_BATCH (default ON, =0 off).
//
// v1: GET /v1/models, /v1/status, POST /v1/chat/completions e /v1/completions (stream/non).
// Sampling: per-request temperature/top_p/top_k/seed quando presenti nel body (surface
// ISessionSamplingCompleter sul provider mlx); se assenti vale la config del provider.
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Basic.AI.Rag;
using Basic.Mcp;
using Basic.Providers;

namespace Basic.AI;

/// <summary>Host C# del server OpenAI-compatible sopra un completer mlx locale. Avviato
/// da OpenAIServer/ (host tracked) o, in futuro, dai comandi BASIC "AI SERVER ...".</summary>
public static class OpenAiServer
{
    sealed class Model
    {
        public required ILlmCompleter Completer;
        public string Id = "model";
        public int MaxTokens = 256;
        public ITokenizingCompleter? Tok => Completer as ITokenizingCompleter;
    }

    /// <summary>Una richiesta in coda. Il parsing (nessuna chiamata all'engine) avviene sul thread
    /// HTTP; la generazione e la scrittura della risposta avvengono sullo scheduler (thread del
    /// motore). Il thread HTTP attende <see cref="Done"/>.</summary>
    sealed class Job
    {
        public required HttpListenerContext Ctx;
        public bool Chat;
        public bool Anthropic;                             /* /v1/messages: protocollo Anthropic */
        public bool Stream;
        public int MaxTokens;
        public float Temperature, TopP;
        public int TopK;
        public long Seed;
        public bool SamplingSet;
        public float RepetitionPenalty = 1f, FrequencyPenalty, PresencePenalty;  /* §I */
        public bool PenaltiesSet;
        public string Prompt = "";
        public List<McpToolDefinition>? Tools;
        public string? JsonSchema;
        public bool Think;                                 /* reasoning_effort / thinking abilitato */
        public int Effort = 50;                            /* 1..100: prefisso "Reasoning Effort: N" */
        public List<(byte[] Bytes, string Fmt)>? Images;    /* input multimodale (immagini) */
        public int Sid = -1;
        public readonly StringBuilder Text = new();
        public readonly ManualResetEventSlim Done = new(false);
    }

    static Model? _m;
    static IVisionCompleter? _vision;   /* VLM per l'input immagini (opzionale, vedi Start) */
    static ILatentCompleter? _latent;   /* embeddings locali (EmbedText del provider mlx) */
    static HttpListener? _ln;
    static long _requests, _completions;
    static readonly ConcurrentQueue<Job> _queue = new();
    static volatile bool _running;
    static int _maxActive = 2;
    static bool _batchOn = true;

    public static void Start(string modelDir, int port = 8020, string host = "127.0.0.1",
        float temperature = float.NaN, float topP = 0f, int topK = 0, long seed = 0, int maxTokens = 256,
        string? visionModelPath = null, string? visionMmprojPath = null)
    {
        if (_m != null) throw new InvalidOperationException("server già avviato");
        var envActive = Basic.Providers.GmbEnv.Get("GMB_MAX_ACTIVE");
        if (envActive != null && int.TryParse(envActive, out var ma) && ma > 0 && ma <= 64) _maxActive = ma;
        if (Basic.Providers.GmbEnv.Get("GMB_SERVER_BATCH") == "0") _batchOn = false;

        var cfg = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["modelDir"] = modelDir,
            ["maxTokens"] = maxTokens.ToString(),
            ["prefillChunk"] = "4096",
        };
        if (!float.IsNaN(temperature)) cfg["temperature"] = temperature.ToString(System.Globalization.CultureInfo.InvariantCulture);
        if (topP > 0) cfg["topP"] = topP.ToString(System.Globalization.CultureInfo.InvariantCulture);
        if (topK > 0) cfg["topK"] = topK.ToString();
        if (seed != 0) cfg["seed"] = seed.ToString();

        var desc = new ProviderDescriptor(ProviderRole.Completion, "openaiserver", "mlx", cfg);
        var comp = ProviderFactory.CreateCompletion(desc);
        if (comp is not ISessionAwareLlmCompleter)
            throw new InvalidOperationException("il provider mlx non espone le sessioni (richiesto dal server)");

        _m = new Model { Completer = comp, Id = Path.GetFileName(modelDir.TrimEnd('/', '\\')), MaxTokens = maxTokens };
        _latent = comp as ILatentCompleter;   /* /v1/embeddings: hidden states reali del motore */
        /* VLM opzionale per l'input immagini: se configurato, le richieste con immagini vengono
         * descritte e iniettate nel prompt (parita' con fmlx_serve --vision-model). */
        if (!string.IsNullOrEmpty(visionModelPath))
        {
            // Consumo del descriptor .gmmod: se accanto al modello vision c'è un <stem>.gmmod,
            // è sufficiente quello (base+mmproj+modalità DICHIARATI dall'ABI), senza mmproj esplicito.
            var visionGmmod = File.Exists(Path.ChangeExtension(visionModelPath!, ".gmmod"))
                ? Path.ChangeExtension(visionModelPath!, ".gmmod") : null;
            if (!string.IsNullOrEmpty(visionMmprojPath) || visionGmmod is not null)
            {
                var vcfg = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
                {
                    ["modelPath"] = visionModelPath!,
                };
                if (!string.IsNullOrEmpty(visionMmprojPath)) vcfg["mmprojPath"] = visionMmprojPath!;
                if (visionGmmod is not null) vcfg["gmmodPath"] = visionGmmod;
                var vdesc = new ProviderDescriptor(ProviderRole.Completion, "openaiserver-vision",
                                                   "llamacpp-vision", vcfg);
                _vision = ProviderFactory.CreateCompletion(vdesc) as IVisionCompleter
                    ?? throw new InvalidOperationException("llamacpp-vision non espone IVisionCompleter");
            }
        }
        _ln = new HttpListener();
        _ln.Prefixes.Add($"http://{host}:{port}/");
        _ln.Start();
        Console.WriteLine($"[openai-server] {_m.Id} su http://{host}:{port}/ max_active={_maxActive} batch={_batchOn} (Ctrl-C per fermare)");
        _running = true;

        // Thread HTTP: accetta e accoda (nessuna chiamata all'engine).
        var lt = new Thread(ListenLoop) { IsBackground = true, Name = "openai-http" };
        lt.Start();
        // Scheduler sul thread corrente (T): è il thread che ha caricato il modello e possiede lo
        // stream MLX. Tutte le chiamate all'engine avvengono qui.
        SchedulerLoop();

        _running = false;
        (comp as IDisposable)?.Dispose();
        _m = null;
    }

    public static void Stop() { _running = false; try { _ln?.Stop(); } catch { } _ln = null; }

    static void ListenLoop()
    {
        while (_running)
        {
            HttpListenerContext ctx;
            try { ctx = _ln!.GetContext(); }
            catch { break; }
            _ = Task.Run(() => HandleRequest(ctx));
        }
    }

    static void HandleRequest(HttpListenerContext ctx)
    {
        try
        {
            var path = ctx.Request.Url!.AbsolutePath.TrimEnd('/');
            Interlocked.Increment(ref _requests);
            if (ctx.Request.HttpMethod == "GET" && path == "/v1/models") { TryJson(ctx, 200, Models()); return; }
            if (ctx.Request.HttpMethod == "GET" && path == "/v1/status") { TryJson(ctx, 200, Status()); return; }
            if (ctx.Request.HttpMethod == "GET" && path == "/metrics") { Metrics(ctx); return; }
            if (ctx.Request.HttpMethod == "POST" && (path == "/v1/chat/completions" || path == "/v1/completions"))
            { EnqueueCompletions(ctx, chat: path == "/v1/chat/completions"); return; }
            if (ctx.Request.HttpMethod == "POST" && path == "/v1/messages")
            { EnqueueMessages(ctx); return; }
            if (ctx.Request.HttpMethod == "POST" && path == "/v1/embeddings")
            { HandleEmbeddings(ctx); return; }
            TryJson(ctx, 404, Err($"endpoint sconosciuto {path}"));
        }
        catch (Exception e) { TryJson(ctx, 500, Err(e.Message)); }
    }

    /* /v1/embeddings: hidden states reali del motore (ILatentCompleter.EmbedText), nessun modello
     * extra. `dimensions` opzionale: tronca/zero-pad come LlmLatentEmbeddingProvider. */
    static void HandleEmbeddings(HttpListenerContext ctx)
    {
        if (_latent == null) { TryJson(ctx, 501, Err("embeddings non supportate da questo provider")); return; }
        string body = new StreamReader(ctx.Request.InputStream, Encoding.UTF8).ReadToEnd();
        try
        {
            using var doc = JsonDocument.Parse(body);
            var root = doc.RootElement;
            int dim = -1;
            if (root.TryGetProperty("dimensions", out var dv) && dv.ValueKind == JsonValueKind.Number) dim = dv.GetInt32();
            var inputs = new List<string>();
            if (root.TryGetProperty("input", out var iv))
            {
                if (iv.ValueKind == JsonValueKind.String) inputs.Add(iv.GetString() ?? "");
                else if (iv.ValueKind == JsonValueKind.Array)
                    foreach (var e in iv.EnumerateArray())
                        if (e.ValueKind == JsonValueKind.String) inputs.Add(e.GetString() ?? "");
            }
            if (inputs.Count == 0) { TryJson(ctx, 400, Err("input mancante")); return; }
            var data = new List<object>();
            for (int i = 0; i < inputs.Count; i++)
            {
                var v = _latent.EmbedText(inputs[i]) ?? Array.Empty<float>();
                if (dim > 0 && v.Length != dim) { var o = new float[dim]; Array.Copy(v, o, Math.Min(v.Length, dim)); v = o; }
                data.Add(new { @object = "embedding", index = i, embedding = v });
            }
            TryJson(ctx, 200, new { @object = "list", data, model = _m!.Id,
                usage = new { prompt_tokens = inputs.Count, total_tokens = inputs.Count } });
        }
        catch (JsonException) { TryJson(ctx, 400, Err("JSON non valido")); }
    }

    static void Metrics(HttpListenerContext ctx)
    {
        string body = $"gmb_requests_total {_requests}\ngmb_completions_total {_completions}\n";
        try
        {
            byte[] b = Encoding.UTF8.GetBytes(body);
            ctx.Response.StatusCode = 200;
            ctx.Response.ContentType = "text/plain; version=0.0.4";
            ctx.Response.ContentLength64 = b.Length;
            ctx.Response.OutputStream.Write(b);
            ctx.Response.Close();
        }
        catch { }
    }

    static object Models() => new
    {
        @object = "list",
        data = new[] { new { id = _m!.Id, @object = "model", owned_by = "gmbasic" } }
    };

    static object Status() => new { status = "ok", model = _m!.Id, loaded = _m != null, busy = _queue.Count >= _maxActive, version = "gmbasic-mlx-server-csharp-v1" };
    static object Err(string m) => new { error = new { message = m } };

    static void EnqueueCompletions(HttpListenerContext ctx, bool chat)
    {
        if (_queue.Count >= _maxActive) { TryJson(ctx, 503, Err("coda piena (admission)")); return; }
        string body = new StreamReader(ctx.Request.InputStream, Encoding.UTF8).ReadToEnd();
        var job = new Job { Ctx = ctx, Chat = chat, MaxTokens = _m!.MaxTokens };
        try
        {
            using var doc = JsonDocument.Parse(body);
            var root = doc.RootElement;
            if (root.TryGetProperty("suffix", out _) || root.TryGetProperty("logprobs", out _) || root.TryGetProperty("echo", out _))
            { TryJson(ctx, 400, Err("parametro non supportato (fail-loud)")); return; }
            double dn = 1, db = 1;
            if (root.TryGetProperty("n", out var nv) && nv.ValueKind == JsonValueKind.Number) dn = nv.GetDouble();
            if (root.TryGetProperty("best_of", out var bv) && bv.ValueKind == JsonValueKind.Number) db = bv.GetDouble();
            if (dn > 1 || db > 1) { TryJson(ctx, 400, Err("n>1/best_of>1 non supportato")); return; }

            if (root.TryGetProperty("max_tokens", out var mt) && mt.ValueKind == JsonValueKind.Number)
                job.MaxTokens = Math.Clamp(mt.GetInt32(), 1, 4096);
            job.Stream = root.TryGetProperty("stream", out var sv) && sv.ValueKind == JsonValueKind.True;

            if (root.TryGetProperty("temperature", out var tv) && tv.ValueKind == JsonValueKind.Number)
            { job.Temperature = (float)tv.GetDouble(); job.SamplingSet = true; }
            if (root.TryGetProperty("top_p", out var tpv) && tpv.ValueKind == JsonValueKind.Number)
            { job.TopP = (float)tpv.GetDouble(); job.SamplingSet = true; }
            if (root.TryGetProperty("top_k", out var tkv) && tkv.ValueKind == JsonValueKind.Number)
            { job.TopK = tkv.GetInt32(); job.SamplingSet = true; }
            if (root.TryGetProperty("seed", out var sdv) && sdv.ValueKind == JsonValueKind.Number)
            { job.Seed = (long)sdv.GetDouble(); job.SamplingSet = true; }
            /* §I penalty OpenAI-style separate (repetition/frequency/presence). */
            if (root.TryGetProperty("repetition_penalty", out var rpv) && rpv.ValueKind == JsonValueKind.Number)
            { job.RepetitionPenalty = (float)rpv.GetDouble(); job.PenaltiesSet = true; }
            if (root.TryGetProperty("frequency_penalty", out var fpv) && fpv.ValueKind == JsonValueKind.Number)
            { job.FrequencyPenalty = (float)fpv.GetDouble(); job.PenaltiesSet = true; }
            if (root.TryGetProperty("presence_penalty", out var ppv) && ppv.ValueKind == JsonValueKind.Number)
            { job.PresencePenalty = (float)ppv.GetDouble(); job.PenaltiesSet = true; }

            if (root.TryGetProperty("tools", out var toolsEl) && toolsEl.ValueKind == JsonValueKind.Array && toolsEl.GetArrayLength() > 0)
                job.Tools = ParseTools(toolsEl);

            if (root.TryGetProperty("response_format", out var rf) && rf.ValueKind == JsonValueKind.Object)
            {
                if (rf.TryGetProperty("json_schema", out var js) && js.ValueKind == JsonValueKind.Object
                    && js.TryGetProperty("schema", out var sch))
                    job.JsonSchema = sch.GetRawText();
                else if (rf.TryGetProperty("type", out var t) && t.GetString() == "json_object")
                    job.JsonSchema = "{\"type\":\"object\"}";
            }

            /* REASONING: reasoning_effort (OpenAI, "none"=off) o thinking (Anthropic, type=enabled). */
            if (root.TryGetProperty("reasoning_effort", out var rev) && rev.ValueKind == JsonValueKind.String)
            {
                var re = rev.GetString() ?? "";
                job.Think = !re.Equals("none", StringComparison.OrdinalIgnoreCase);
                job.Effort = re.Equals("high", StringComparison.OrdinalIgnoreCase) ? 80
                           : re.Equals("low", StringComparison.OrdinalIgnoreCase) ? 20 : 50;
            }
            else if (root.TryGetProperty("thinking", out var thv) && thv.ValueKind == JsonValueKind.Object
                     && thv.TryGetProperty("type", out var tht) && tht.GetString() == "enabled")
                job.Think = true;

            if (chat)
            {
                var imgs = new List<(byte[] Bytes, string Fmt)>();
                job.Prompt = ExtractChat(root, imgs);
                if (imgs.Count > 0)
                {
                    if (_vision == null)
                    { TryJson(ctx, 400, Err("input immagini non supportato: server avviato senza VLM (visionModelPath/visionMmprojPath)")); return; }
                    job.Images = imgs;
                }
            }
            else if (root.TryGetProperty("prompt", out var p) && p.ValueKind == JsonValueKind.String) job.Prompt = p.GetString() ?? "";
            else { TryJson(ctx, 400, Err("campo 'prompt' o 'messages' mancante")); return; }
            if (string.IsNullOrEmpty(job.Prompt)) { TryJson(ctx, 400, Err("prompt vuoto")); return; }
        }
        catch (JsonException) { TryJson(ctx, 400, Err("JSON non valido")); return; }

        _queue.Enqueue(job);
        job.Done.Wait();
    }

    // ── Anthropic /v1/messages ──────────────────────────────────────────────────────

    static void EnqueueMessages(HttpListenerContext ctx)
    {
        if (_queue.Count >= _maxActive) { TryJson(ctx, 503, Err("coda piena (admission)")); return; }
        string body = new StreamReader(ctx.Request.InputStream, Encoding.UTF8).ReadToEnd();
        var job = new Job { Ctx = ctx, Chat = true, Anthropic = true, MaxTokens = _m!.MaxTokens };
        try
        {
            using var doc = JsonDocument.Parse(body);
            var root = doc.RootElement;
            if (root.TryGetProperty("max_tokens", out var mt) && mt.ValueKind == JsonValueKind.Number)
                job.MaxTokens = Math.Clamp(mt.GetInt32(), 1, 4096);
            else { TryJson(ctx, 400, Err("max_tokens obbligatorio (Anthropic)")); return; }
            job.Stream = root.TryGetProperty("stream", out var sv) && sv.ValueKind == JsonValueKind.True;
            if (root.TryGetProperty("temperature", out var tv) && tv.ValueKind == JsonValueKind.Number) { job.Temperature = (float)tv.GetDouble(); job.SamplingSet = true; }
            if (root.TryGetProperty("top_p", out var tpv) && tpv.ValueKind == JsonValueKind.Number) { job.TopP = (float)tpv.GetDouble(); job.SamplingSet = true; }
            if (root.TryGetProperty("top_k", out var tkv) && tkv.ValueKind == JsonValueKind.Number) { job.TopK = tkv.GetInt32(); job.SamplingSet = true; }
            if (root.TryGetProperty("thinking", out var thv) && thv.ValueKind == JsonValueKind.Object
                && thv.TryGetProperty("type", out var tht) && tht.GetString() == "enabled") job.Think = true;
            if (root.TryGetProperty("tools", out var te) && te.ValueKind == JsonValueKind.Array && te.GetArrayLength() > 0)
                job.Tools = ParseAnthropicTools(te);

            var imgs = new List<(byte[] Bytes, string Fmt)>();
            var sb = new StringBuilder();
            if (root.TryGetProperty("system", out var sys)) AppendAnthropicText(sys, sb, "System: ", imgs);
            if (root.TryGetProperty("messages", out var msg) && msg.ValueKind == JsonValueKind.Array)
                foreach (var m in msg.EnumerateArray())
                    if (m.TryGetProperty("content", out var c)) AppendAnthropicText(c, sb, null, imgs);
            job.Prompt = sb.ToString().TrimEnd();
            if (imgs.Count > 0)
            {
                if (_vision == null) { TryJson(ctx, 400, Err("input immagini non supportato: server avviato senza VLM")); return; }
                job.Images = imgs;
            }
            if (string.IsNullOrEmpty(job.Prompt)) { TryJson(ctx, 400, Err("prompt vuoto")); return; }
        }
        catch (JsonException) { TryJson(ctx, 400, Err("JSON non valido")); return; }

        _queue.Enqueue(job);
        job.Done.Wait();
    }

    /* Testo (e immagini) di un `content`/`system` Anthropic: stringa, o array di blocchi
     * {type:text,text} / {type:image,source}. */
    static void AppendAnthropicText(JsonElement c, StringBuilder sb, string? prefix,
                                    List<(byte[] Bytes, string Fmt)>? images)
    {
        if (c.ValueKind == JsonValueKind.String) { sb.Append(prefix).Append(c.GetString()).Append('\n'); return; }
        if (c.ValueKind != JsonValueKind.Array) return;
        if (prefix != null) sb.Append(prefix);
        foreach (var part in c.EnumerateArray())
        {
            if (part.ValueKind != JsonValueKind.Object) continue;
            var ty = part.TryGetProperty("type", out var t) ? t.GetString() : null;
            if (ty == "text" && part.TryGetProperty("text", out var tx) && tx.ValueKind == JsonValueKind.String)
                sb.Append(tx.GetString()).Append('\n');
            else if ((ty == "image" || ty == "document") && part.TryGetProperty("source", out var src))
                DecodeAnthropicImage(src, images);
        }
    }

    static void WriteAnthropicJson(Job j)
    {
        var (reasoning, content) = SplitThink(j.Text.ToString());
        var blocks = new List<object>();
        if (!string.IsNullOrEmpty(reasoning)) blocks.Add(new { type = "thinking", thinking = reasoning, signature = "" });
        blocks.Add(new { type = "text", text = content });
        var resp = new Dictionary<string, object?>
        {
            ["id"] = "msg-gmb-cs-" + Guid.NewGuid().ToString("N")[..8],
            ["type"] = "message",
            ["role"] = "assistant",
            ["model"] = _m!.Id,
            ["content"] = blocks,
            ["stop_reason"] = "end_turn",
            ["stop_sequence"] = null,
            ["usage"] = new { input_tokens = _m.Tok?.CountTokens(j.Prompt) ?? 0, output_tokens = 0 },
        };
        TryJson(j.Ctx, 200, resp);
    }

    /* SSE Anthropic: il testo e' gia' interamente generato (j.Text), quindi i frame sono emessi
     * in blocco ma nell'ordine/dagli indici stabili del protocollo Anthropic. */
    static void AnthropicSse(Job j)
    {
        j.Ctx.Response.StatusCode = 200;
        j.Ctx.Response.ContentType = "text/event-stream";
        j.Ctx.Response.Headers["Cache-Control"] = "no-cache";
        var sw = j.Ctx.Response.OutputStream;
        void Ev(string ev, object data)
        {
            sw.Write(Encoding.UTF8.GetBytes($"event: {ev}\ndata: {JsonSerializer.Serialize(data)}\n\n"));
            sw.Flush();
        }
        string id = "msg-gmb-cs-" + Guid.NewGuid().ToString("N")[..8];
        int inTok = _m!.Tok?.CountTokens(j.Prompt) ?? 0;
        Ev("message_start", new { type = "message_start", message = new { id, type = "message", role = "assistant", model = _m.Id, content = Array.Empty<object>(), stop_reason = (string?)null, stop_sequence = (string?)null, usage = new { input_tokens = inTok, output_tokens = 0 } } });
        var (reasoning, content) = SplitThink(j.Text.ToString());
        int idx = 0;
        if (!string.IsNullOrEmpty(reasoning))
        {
            Ev("content_block_start", new { type = "content_block_start", index = idx, content_block = new { type = "thinking", thinking = "" } });
            Ev("content_block_delta", new { type = "content_block_delta", index = idx, delta = new { type = "thinking_delta", thinking = reasoning } });
            Ev("content_block_stop", new { type = "content_block_stop", index = idx });
            idx++;
        }
        Ev("content_block_start", new { type = "content_block_start", index = idx, content_block = new { type = "text", text = "" } });
        Ev("content_block_delta", new { type = "content_block_delta", index = idx, delta = new { type = "text_delta", text = content } });
        Ev("content_block_stop", new { type = "content_block_stop", index = idx });
        Ev("message_delta", new { type = "message_delta", delta = new { stop_reason = "end_turn", stop_sequence = (string?)null }, usage = new { output_tokens = 0 } });
        Ev("message_stop", new { type = "message_stop" });
        try { j.Ctx.Response.Close(); } catch { }
    }

    // ── Scheduler (thread T del motore) ─────────────────────────────────────────────
    static void SchedulerLoop()
    {
        while (_running)
        {
            if (!_queue.TryDequeue(out var first)) { Thread.Sleep(1); continue; }
            var group = new List<Job> { first };
            while (group.Count < _maxActive && _queue.TryDequeue(out var j)) group.Add(j);
            bool batchable = _batchOn && group.Count >= 2
                && _m!.Completer is ISessionBatchCompleter
                && group.TrueForAll(x => x.Tools == null && x.JsonSchema == null);
            try
            {
                if (batchable) RunBatch(group);
                else foreach (var j in group) RunSingle(j);
            }
            catch (Exception e)
            {
                foreach (var j in group)
                {
                    try { TryJson(j.Ctx, 500, Err(e.Message)); } catch { }
                    j.Done.Set();
                }
            }
        }
    }

    /// <summary>Genera un gruppo di richieste "semplici" in UN forward batched per step. Le
    /// sessioni sono create/appese/distrutte qui (thread T); i token sono streammati al volo.</summary>
    static void RunBatch(List<Job> jobs)
    {
        var comp = (ISessionAwareLlmCompleter)_m!.Completer;
        var batch = (ISessionBatchCompleter)comp;
        int n = jobs.Count;
        var sids = new int[n];
        var maxes = new int[n];
        int created = 0;
        try
        {
            for (int i = 0; i < n; i++)
            {
                var j = jobs[i];
                sids[i] = comp.CreateSession();
                created++;
                if (j.SamplingSet && comp is ISessionSamplingCompleter ssc)
                    ssc.SetSessionSampling(sids[i], j.Temperature, j.TopP, j.TopK, j.Seed);
                if (j.PenaltiesSet && comp is ISessionPenaltyCompleter spc)
                    spc.SetSessionPenalties(sids[i], j.RepetitionPenalty, j.FrequencyPenalty, j.PresencePenalty);
                comp.AppendMessage(sids[i], "system", j.Chat ? "" : "Complete the text as-is.");
                comp.AppendMessage(sids[i], "user", j.Prompt);
                maxes[i] = j.MaxTokens;
                if (j.Stream)
                {
                    j.Ctx.Response.StatusCode = 200;
                    j.Ctx.Response.ContentType = "text/event-stream";
                    j.Ctx.Response.Headers["Cache-Control"] = "no-cache";
                }
            }
            batch.GenerateManyStream(sids, maxes, (i, piece) =>
            {
                var j = jobs[i];
                j.Text.Append(piece);
                if (j.Stream) SseDelta(j.Ctx.Response.OutputStream, piece);
            });
            for (int i = 0; i < n; i++) FinishJob(jobs[i]);
            Interlocked.Add(ref _completions, n);
        }
        finally
        {
            for (int i = 0; i < created; i++) comp.DestroySession(sids[i]);
            for (int i = 0; i < n; i++) jobs[i].Done.Set();
        }
    }

    static void RunSingle(Job j)
    {
        try
        {
            ApplyReasoningAndImages(j);
            if (j.Tools != null && j.Tools.Count > 0) { RunWithTools(j); return; }
            if (j.JsonSchema != null) { RunWithJsonSchema(j); return; }
            RunPlain(j);
        }
        finally { j.Done.Set(); }
    }

    /* REASONING (prefisso effort stile DS4) + IMMAGINI (descrizione via VLM) iniettati nel prompt.
     * Le immagini sono descritte QUI, sul thread del motore (il VLM e' una lane llama.cpp/mtmd;
     * il suo handle e' usato in modo seriale dallo scheduler). */
    static void ApplyReasoningAndImages(Job j)
    {
        if (j.Think)
            j.Prompt = $"Reasoning Effort: {j.Effort} (range 1-100, the higher the value, the more thorough the reasoning)\n\n" + j.Prompt;
        if (j.Images != null && j.Images.Count > 0 && _vision != null)
        {
            var sb = new StringBuilder();
            foreach (var im in j.Images)
            {
                var d = _vision.DescribeImage(im.Bytes, im.Fmt, "Descrivi questa immagine e trascrivi ogni testo visibile.");
                if (!string.IsNullOrEmpty(d)) sb.Append("[immagine] ").Append(d).Append('\n');
            }
            if (sb.Length > 0) j.Prompt = sb.ToString() + j.Prompt;
        }
    }

    static void RunPlain(Job j)
    {
        var comp = (ISessionAwareLlmCompleter)_m!.Completer;
        j.Sid = comp.CreateSession();
        try
        {
            if (j.SamplingSet && comp is ISessionSamplingCompleter ssc)
                ssc.SetSessionSampling(j.Sid, j.Temperature, j.TopP, j.TopK, j.Seed);
            if (j.PenaltiesSet && comp is ISessionPenaltyCompleter spc2)
                spc2.SetSessionPenalties(j.Sid, j.RepetitionPenalty, j.FrequencyPenalty, j.PresencePenalty);
            comp.AppendMessage(j.Sid, "system", j.Chat ? "" : "Complete the text as-is.");
            comp.AppendMessage(j.Sid, "user", j.Prompt);
            Interlocked.Increment(ref _completions);
            if (j.Stream)
            {
                if (!j.Anthropic)
                {
                    j.Ctx.Response.StatusCode = 200;
                    j.Ctx.Response.ContentType = "text/event-stream";
                    j.Ctx.Response.Headers["Cache-Control"] = "no-cache";
                }
                var sw = j.Ctx.Response.OutputStream;
                string full = comp is ISessionStreamingCompleter sc
                    ? sc.GenerateFromSessionStream(j.Sid, j.MaxTokens, piece => { j.Text.Append(piece); if (!j.Anthropic) SseDelta(sw, piece); })
                    : comp.GenerateFromSession(j.Sid, j.MaxTokens);
                if (comp is not ISessionStreamingCompleter) j.Text.Append(full);
                if (j.Anthropic) AnthropicSse(j); else SseFinal(j);
            }
            else
            {
                string text = comp is ISessionStreamingCompleter sc
                    ? sc.GenerateFromSessionStream(j.Sid, j.MaxTokens, piece => j.Text.Append(piece))
                    : comp.GenerateFromSession(j.Sid, j.MaxTokens);
                if (comp is not ISessionStreamingCompleter) j.Text.Append(text);
                if (j.Anthropic) WriteAnthropicJson(j); else WriteCompletionJson(j);
            }
        }
        finally { comp.DestroySession(j.Sid); }
    }

    /// <summary>Scrive la risposta finale di un job non-stream (JSON) o la coda SSE se stream.
    /// Usata dal percorso batched (nel percorso singolo la scrittura è inline).</summary>
    static void FinishJob(Job j)
    {
        if (j.Stream) SseFinal(j);
        else WriteCompletionJson(j);
    }

    /* Separa il ragionamento (`...`) dalla risposta. Se non c'e' un blocco chiuso,
     * tutto e' risposta. Usato per emettere `reasoning_content` (estensione OpenAI). */
    static (string Reasoning, string Content) SplitThink(string s)
    {
        int close = s.IndexOf("</think>", StringComparison.OrdinalIgnoreCase);
        if (close < 0) return ("", s);
        int open = s.IndexOf("<think>", StringComparison.OrdinalIgnoreCase);
        int start = open >= 0 && open < close ? open + 7 : 0;
        return (s.Substring(start, close - start).Trim('\n'), s.Substring(close + 8).TrimStart('\n'));
    }

    static void WriteCompletionJson(Job j)
    {
        var promptTokens = _m.Tok?.CountTokens(j.Prompt) ?? 0;
        var (reasoning, content) = SplitThink(j.Text.ToString());
        var resp = new
        {
            id = "cmpl-gmb-cs-" + Guid.NewGuid().ToString("N")[..8],
            @object = "text_completion",
            created = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
            model = _m.Id,
            choices = new[] { new { text = content, reasoning_content = string.IsNullOrEmpty(reasoning) ? null : reasoning, index = 0, finish_reason = "stop", logprobs = (object?)null } },
            usage = new { prompt_tokens = promptTokens, completion_tokens = 0, total_tokens = promptTokens }
        };
        TryJson(j.Ctx, 200, resp);
    }

    static void SseDelta(Stream sw, string piece)
    {
        byte[] b = Encoding.UTF8.GetBytes($"data: {Delta(piece, null)}\n\n");
        sw.Write(b); sw.Flush();
    }

    static void SseFinal(Job j)
    {
        var sw = j.Ctx.Response.OutputStream;
        byte[] f1 = Encoding.UTF8.GetBytes($"data: {Delta("", "stop")}\n\n");
        sw.Write(f1); sw.Flush();
        byte[] f2 = Encoding.UTF8.GetBytes("data: [DONE]\n\n");
        sw.Write(f2); sw.Flush();
        try { j.Ctx.Response.Close(); } catch { }
    }

    static List<McpToolDefinition> ParseTools(JsonElement toolsEl)
    {
        var list = new List<McpToolDefinition>();
        foreach (var t in toolsEl.EnumerateArray())
        {
            if (!t.TryGetProperty("function", out var fn)) continue;
            var name = fn.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "";
            if (string.IsNullOrWhiteSpace(name)) continue;
            var desc = fn.TryGetProperty("description", out var d) ? d.GetString() ?? "" : "";
            JsonObject schema;
            if (fn.TryGetProperty("parameters", out var p) && p.ValueKind == JsonValueKind.Object)
                schema = JsonNode.Parse(p.GetRawText()) as JsonObject ?? new JsonObject();
            else
                schema = new JsonObject { ["type"] = "object" };
            if (!schema.ContainsKey("type")) schema["type"] = "object";
            list.Add(new McpToolDefinition { Name = name, Description = desc, QualifiedProcedureName = name, InputSchema = schema });
        }
        return list;
    }

    /* Anthropic tools: {name, description, input_schema}. */
    static List<McpToolDefinition> ParseAnthropicTools(JsonElement toolsEl)
    {
        var list = new List<McpToolDefinition>();
        foreach (var t in toolsEl.EnumerateArray())
        {
            if (t.ValueKind != JsonValueKind.Object) continue;
            var name = t.TryGetProperty("name", out var n) ? n.GetString() ?? "" : "";
            if (string.IsNullOrWhiteSpace(name)) continue;
            var desc = t.TryGetProperty("description", out var d) ? d.GetString() ?? "" : "";
            JsonObject schema;
            if (t.TryGetProperty("input_schema", out var p) && p.ValueKind == JsonValueKind.Object)
                schema = JsonNode.Parse(p.GetRawText()) as JsonObject ?? new JsonObject();
            else
                schema = new JsonObject { ["type"] = "object" };
            if (!schema.ContainsKey("type")) schema["type"] = "object";
            list.Add(new McpToolDefinition { Name = name, Description = desc, QualifiedProcedureName = name, InputSchema = schema });
        }
        return list;
    }

    static void WriteAnthropicMessage(Job j, List<object> blocks, string stopReason)
    {
        var resp = new Dictionary<string, object?>
        {
            ["id"] = "msg-gmb-cs-" + Guid.NewGuid().ToString("N")[..8],
            ["type"] = "message",
            ["role"] = "assistant",
            ["model"] = _m!.Id,
            ["content"] = blocks,
            ["stop_reason"] = stopReason,
            ["stop_sequence"] = null,
            ["usage"] = new { input_tokens = _m.Tok?.CountTokens(j.Prompt) ?? 0, output_tokens = 0 },
        };
        TryJson(j.Ctx, 200, resp);
    }

    static void RunWithTools(Job j)
    {
        var comp = _m!.Completer;
        if (comp is INativeToolDispatchCompleter native)
        {
            int sid = ((ISessionAwareLlmCompleter)comp).CreateSession();
            try
            {
                if (j.SamplingSet && comp is ISessionSamplingCompleter ssc)
                    ssc.SetSessionSampling(sid, j.Temperature, j.TopP, j.TopK, j.Seed);
                ((ISessionAwareLlmCompleter)comp).AppendMessage(sid, "system", j.Chat ? "" : "Complete the text as-is.");
                ((ISessionAwareLlmCompleter)comp).AppendMessage(sid, "user", j.Prompt);
                var result = native.DispatchTool(sid, j.Tools!);
                if (result.ToolCall is { } call)
                {
                    if (j.Anthropic)
                    {
                        object input = new JsonObject();
                        try { input = JsonNode.Parse(string.IsNullOrWhiteSpace(call.ArgumentsJson) ? "{}" : call.ArgumentsJson) ?? new JsonObject(); } catch { }
                        WriteAnthropicMessage(j, new List<object>
                        {
                            new { type = "tool_use", id = "toolu_" + Guid.NewGuid().ToString("N")[..8], name = call.Name, input }
                        }, "tool_use");
                        return;
                    }
                    var resp = new
                    {
                        id = "chatcmpl-gmb-" + Guid.NewGuid().ToString("N")[..8],
                        @object = "chat.completion",
                        created = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                        model = _m!.Id,
                        choices = new[]
                        {
                            new
                            {
                                index = 0,
                                message = new
                                {
                                    role = "assistant",
                                    content = (string?)null,
                                    tool_calls = new[]
                                    {
                                        new
                                        {
                                            id = "call_" + Guid.NewGuid().ToString("N")[..8],
                                            type = "function",
                                            function = new { name = call.Name, arguments = call.ArgumentsJson }
                                        }
                                    }
                                },
                                finish_reason = "tool_calls",
                                logprobs = (object?)null
                            }
                        },
                        usage = new { prompt_tokens = _m.Tok?.CountTokens(j.Prompt) ?? 0, completion_tokens = 0, total_tokens = _m.Tok?.CountTokens(j.Prompt) ?? 0 }
                    };
                    TryJson(j.Ctx, 200, resp);
                    return;
                }
                if (result.FinalAnswer is { } ans)
                {
                    if (j.Anthropic)
                    {
                        WriteAnthropicMessage(j, new List<object> { new { type = "text", text = ans } }, "end_turn");
                        return;
                    }
                    var resp2 = new
                    {
                        id = "chatcmpl-gmb-" + Guid.NewGuid().ToString("N")[..8],
                        @object = "chat.completion",
                        created = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                        model = _m!.Id,
                        choices = new[] { new { index = 0, message = new { role = "assistant", content = ans, tool_calls = (object?)null }, finish_reason = "stop", logprobs = (object?)null } },
                        usage = new { prompt_tokens = _m.Tok?.CountTokens(j.Prompt) ?? 0, completion_tokens = 0, total_tokens = _m.Tok?.CountTokens(j.Prompt) ?? 0 }
                    };
                    TryJson(j.Ctx, 200, resp2);
                    return;
                }
                TryJson(j.Ctx, 500, Err("dispatch nativo senza ToolCall né FinalAnswer"));
            }
            finally { ((ISessionAwareLlmCompleter)comp).DestroySession(sid); }
            return;
        }
        TryJson(j.Ctx, 501, Err("tools non supportato da questo provider (serve INativeToolDispatchCompleter)"));
    }

    static void RunWithJsonSchema(Job j)
    {
        var comp = (ISessionAwareLlmCompleter)_m!.Completer;
        int sid = comp.CreateSession();
        try
        {
            if (j.SamplingSet && comp is ISessionSamplingCompleter ssc)
                ssc.SetSessionSampling(sid, j.Temperature, j.TopP, j.TopK, j.Seed);
            comp.AppendMessage(sid, "system", j.Chat ? "Rispondi SOLO con JSON valido conforme allo schema richiesto. Non aggiungere testo fuori dal JSON." : "Complete the text as JSON.");
            comp.AppendMessage(sid, "user", j.Prompt + "\n\nSchema JSON richiesto: " + j.JsonSchema);
            string raw = comp is ISessionStreamingCompleter sc
                ? sc.GenerateFromSessionStream(sid, j.MaxTokens, _ => { })
                : comp.GenerateFromSession(sid, j.MaxTokens);
            string outJson = ExtractJson(raw) ?? raw;
            try { JsonDocument.Parse(outJson); }
            catch { outJson = "{}"; }
        var promptTokens = _m!.Tok?.CountTokens(j.Prompt) ?? 0;
            var resp = new
            {
                id = "chatcmpl-gmb-" + Guid.NewGuid().ToString("N")[..8],
                @object = "chat.completion",
                created = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                model = _m!.Id,
                choices = new[] { new { index = 0, message = new { role = "assistant", content = outJson, tool_calls = (object?)null }, finish_reason = "stop", logprobs = (object?)null } },
                usage = new { prompt_tokens = promptTokens, completion_tokens = 0, total_tokens = promptTokens }
            };
            TryJson(j.Ctx, 200, resp);
        }
        finally { comp.DestroySession(sid); }
    }

    static string? ExtractJson(string s)
    {
        int a = s.IndexOf('{'), b = s.LastIndexOf('}');
        if (a >= 0 && b > a) return s.Substring(a, b - a + 1);
        a = s.IndexOf('['); b = s.LastIndexOf(']');
        if (a >= 0 && b > a) return s.Substring(a, b - a + 1);
        return null;
    }

    static string ExtractChat(JsonElement root, List<(byte[] Bytes, string Fmt)>? images)
    {
        if (!root.TryGetProperty("messages", out var msg) || msg.ValueKind != JsonValueKind.Array) return "";
        var sb = new StringBuilder();
        foreach (var m in msg.EnumerateArray())
        {
            if (!m.TryGetProperty("role", out var r) || !m.TryGetProperty("content", out var c)) continue;
            string? role = r.GetString();
            if (c.ValueKind == JsonValueKind.String)
            {
                if (role == "system") { sb.Append("System: ").Append(c.GetString()).Append('\n'); continue; }
                sb.Append(c.GetString()).Append('\n');
            }
            else if (c.ValueKind == JsonValueKind.Array)   /* content multimodale: testo + immagini */
            {
                foreach (var part in c.EnumerateArray())
                {
                    if (part.ValueKind != JsonValueKind.Object) continue;
                    var ty = part.TryGetProperty("type", out var t) ? t.GetString() : null;
                    if (ty == "text" && part.TryGetProperty("text", out var tx) && tx.ValueKind == JsonValueKind.String)
                        sb.Append(tx.GetString()).Append('\n');
                    else if (ty == "image_url" && part.TryGetProperty("image_url", out var iu)
                             && iu.TryGetProperty("url", out var u) && u.ValueKind == JsonValueKind.String)
                        DecodeImageUrl(u.GetString() ?? "", images);
                    else if ((ty == "image" || ty == "document") && part.TryGetProperty("source", out var src))
                        DecodeAnthropicImage(src, images);
                }
            }
        }
        return sb.ToString().TrimEnd();
    }

    /* OpenAI image_url: `data:<mime>;base64,<...>` -> byte immagine. Solo data URL. */
    static void DecodeImageUrl(string url, List<(byte[] Bytes, string Fmt)>? images)
    {
        if (images == null) return;
        int comma = url.IndexOf(',');
        if (comma < 0 || !url.StartsWith("data:", StringComparison.OrdinalIgnoreCase)) return;
        string head = url.Substring(5, comma - 5);           /* <mime>;base64 */
        byte[] bytes;
        try { bytes = Convert.FromBase64String(url.Substring(comma + 1)); } catch { return; }
        if (head.Contains("pdf"))
        { foreach (var pg in RasterizePdf(bytes)) images.Add((pg, ".png")); return; }   /* PDF inline */
        images.Add((bytes, FmtOf(head)));
    }
    /* Anthropic image: {source:{type:base64,media_type,data}}. */
    static void DecodeAnthropicImage(JsonElement src, List<(byte[] Bytes, string Fmt)>? images)
    {
        if (images == null) return;
        if (!src.TryGetProperty("data", out var d) || d.ValueKind != JsonValueKind.String) return;
        string media = src.TryGetProperty("media_type", out var mm) ? (mm.GetString() ?? "image/png") : "image/png";
        byte[] bytes;
        try { bytes = Convert.FromBase64String(d.GetString() ?? ""); } catch { return; }
        if (media.Contains("pdf"))
        { foreach (var pg in RasterizePdf(bytes)) images.Add((pg, ".png")); return; }       /* PDF inline */
        images.Add((bytes, FmtOf(media)));
    }
    /* PDF inline MULTI-PAGINA: rasterizza tutte le pagine con `pdftoppm` (poppler), fallback
     * `sips` (prima pagina). Ritorna i PNG (byte) delle pagine, in ordine. */
    static List<byte[]> RasterizePdf(byte[] pdf)
    {
        var pages = new List<byte[]>();
        var p = Path.Combine(Path.GetTempPath(), "gmb-pdf-" + Guid.NewGuid().ToString("N") + ".pdf");
        var prefix = Path.Combine(Path.GetTempPath(), "gmb-pdfp-" + Guid.NewGuid().ToString("N"));
        try
        {
            File.WriteAllBytes(p, pdf);
            try
            {
                var psi = new ProcessStartInfo("pdftoppm") { RedirectStandardOutput = true, RedirectStandardError = true };
                psi.ArgumentList.Add("-png"); psi.ArgumentList.Add("-r"); psi.ArgumentList.Add("150");
                psi.ArgumentList.Add(p); psi.ArgumentList.Add(prefix);
                using var pr = Process.Start(psi); pr?.WaitForExit(30000);
            }
            catch { }
            foreach (var f in Directory.GetFiles(Path.GetTempPath(), Path.GetFileName(prefix) + "-*.png"))
            {
                try { pages.Add(File.ReadAllBytes(f)); } catch { }
                try { File.Delete(f); } catch { }
            }
            if (pages.Count == 0)   /* fallback: prima pagina via sips */
            {
                var o = prefix + "-f.png";
                try
                {
                    var psi2 = new ProcessStartInfo("sips") { RedirectStandardOutput = true, RedirectStandardError = true };
                    psi2.ArgumentList.Add("-s"); psi2.ArgumentList.Add("format"); psi2.ArgumentList.Add("png");
                    psi2.ArgumentList.Add(p); psi2.ArgumentList.Add("--out"); psi2.ArgumentList.Add(o);
                    using var pr2 = Process.Start(psi2); pr2?.WaitForExit(20000);
                }
                catch { }
                if (File.Exists(o)) { try { pages.Add(File.ReadAllBytes(o)); } catch { } try { File.Delete(o); } catch { } }
            }
        }
        catch { }
        finally { try { File.Delete(p); } catch { } }
        return pages;
    }
    static string FmtOf(string mime)
    {
        if (mime.Contains("jpeg") || mime.Contains("jpg")) return ".jpg";
        if (mime.Contains("webp")) return ".webp";
        return ".png";
    }

    static string Delta(string text, string? finish) =>
        JsonSerializer.Serialize(new
        {
            id = "cmpl-gmb-cs",
            @object = "text_completion",
            model = _m!.Id,
            choices = new[] { new { text, index = 0, finish_reason = finish, logprobs = (object?)null } }
        });

    static void TryJson(HttpListenerContext ctx, int status, object payload)
    {
        try
        {
            byte[] b = JsonSerializer.SerializeToUtf8Bytes(payload);
            ctx.Response.StatusCode = status;
            ctx.Response.ContentType = "application/json";
            ctx.Response.ContentLength64 = b.Length;
            ctx.Response.OutputStream.Write(b);
            ctx.Response.Close();
        }
        catch { }
    }
}
