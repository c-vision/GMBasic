// FILE: Basic.Providers.ProviderServer
using System.Text.Json;
using Basic.SQLite.Vector;
using Basic.AI.Rag;

namespace Basic.Providers;

/// <summary>Central registry for embedding and completion providers with rotation.</summary>
/// <remarks>
/// ponytail: in-process singleton; persistence via JSON SAVE/LOAD (<see cref="Save"/>, <see cref="Load"/>).
/// </remarks>
public sealed class ProviderServer
{
    public static ProviderServer Instance { get; } = new();

    private readonly Lock _lock = new();
    private readonly Dictionary<string, ProviderDescriptor> _providers = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, List<string>> _roleOrder = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, int> _activeIndex = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, object> _instances = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, int> _rotationCurrentWeight = new(StringComparer.OrdinalIgnoreCase);
    private bool _defaultsLoaded;

    private ProviderServer()
    {
    }

    /// <summary>
    /// Svuota TUTTO lo stato (provider, ordini, indici attivi, istanze, default).
    /// Usato dal test runner tra un sample e l'altro: i sample condividono il processo
    /// e il singleton; senza reset lo stato di un sample (es. un provider registrato)
    /// contamina il successivo (es. PROVIDER.LOAD che risolve un $env placeholder
    /// registrato da un sample precedente e throwa).
    /// </summary>
    public void ResetAll()
    {
        lock (_lock)
        {
            _providers.Clear();
            _roleOrder.Clear();
            _activeIndex.Clear();
            _instances.Clear();
            _rotationCurrentWeight.Clear();
            _defaultsLoaded = false;
        }
    }

    public void Register(string role, string name, string kind, IReadOnlyDictionary<string, string>? config = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        var normalizedRole = ProviderRole.Normalize(role);
        if (!ProviderRole.IsKnown(normalizedRole))
            throw new ArgumentException($"Unknown role '{role}'.", nameof(role));

        lock (_lock)
        {
            EnsureDefaultsLocked();
            var descriptor = new ProviderDescriptor(normalizedRole, name, kind, config);
            _providers[name] = descriptor;
            if (!_roleOrder.TryGetValue(normalizedRole, out var order))
            {
                order = [];
                _roleOrder[normalizedRole] = order;
            }

            if (!order.Contains(name, StringComparer.OrdinalIgnoreCase))
                order.Add(name);

            _instances.Remove(name);
        }
    }

    /// <summary>Parse di un oggetto JSON {chiave:valore} in Dictionary, SENZA reflection:
    /// JsonDocument/Utf8JsonReader sono AOT-safe. `JsonSerializer.Deserialize<T>` è disabilitato
    /// sotto NativeAOT (IL nativo) e faceva fallire AI REGISTER con un config JSON.</summary>
    internal static Dictionary<string, string> ParseConfigJson(string json)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        if (doc.RootElement.ValueKind == System.Text.Json.JsonValueKind.Object)
        {
            foreach (var prop in doc.RootElement.EnumerateObject())
            {
                result[prop.Name] = prop.Value.ValueKind == System.Text.Json.JsonValueKind.String
                    ? (prop.Value.GetString() ?? string.Empty)
                    : prop.Value.GetRawText();
            }
        }
        return result;
    }

    public void RegisterFromJson(string role, string name, string kind, string? configJson)
    {
        IReadOnlyDictionary<string, string>? config = null;
        if (!string.IsNullOrWhiteSpace(configJson))
        {
            var parsed = ParseConfigJson(configJson);
            if (parsed is not null)
                config = parsed;
        }

        Register(role, name, kind, config);
    }

    /// <summary>REGISTER unificato (AI-Provider-Register-Unified-Plan): una sola registrazione
    /// per tutti i modelli, con capability e source scheme-aware. source puo' essere:
    ///   - path locale (file .gguf / cartella MLX) -> auto-detect capability+kind
    ///   - "hf:<repo>" / "huggingface:<repo>" -> modello embedded da scaricare (scheme nel config)
    ///   - "http(s)://..." -> provider remoto via API (openai/anthropic/ollama), param = modello
    ///   - "<scheme>:<id>" (es. "ollama:gemma4:31b-mlx", "openai:gpt-4o") -> registry dal config
    /// capability: %CHAT/%OCR/%AUDIO; per i remoti è obbligatoria (un endpoint non si
    /// auto-ispeziona). Ritorna l'alias registrato. Non forza SetActive: l'attivazione è
    /// separata via ACTIVE, così chat/ocr/audio coesistono.</summary>
    public string RegisterSource(string alias, string? capability, string source, string? param = null)
    {
        // Role canonico UNICO da costante % (0..4) o nome di ruolo (completion/embedding/
        // reranker/ocr/audio). Embedding/reranker NON sono capability AI (chat/ocr/audio):
        // sono ruoli del registry vettoriale con factory/kind propri — qui source è il kind
        // e param il config JSON (forma legacy; vedi VectorProviderFailover/HealthCheck).
        var role = ProviderRole.FromCapabilityOrRole(capability);
        if (role is ProviderRole.Embedding or ProviderRole.Reranker)
        {
            IReadOnlyDictionary<string, string>? config = null;
            if (!string.IsNullOrWhiteSpace(param))
            {
                config = ParseConfigJson(param);
            }
            Register(role, alias, source, config);
            return alias;
        }

        var cap = role switch
        {
            ProviderRole.Ocr => Basic.AI.Rag.AiCapability.Ocr,
            ProviderRole.Audio => Basic.AI.Rag.AiCapability.Audio,
            _ => Basic.AI.Rag.AiCapability.Chat,
        };

        if (source.StartsWith("hf:", StringComparison.OrdinalIgnoreCase)
            || source.StartsWith("huggingface:", StringComparison.OrdinalIgnoreCase))
        {
            // Modello embedded da scaricare: hf:<repo> — il download è sincrono con progress.
            var repo = source[(source.IndexOf(':') + 1)..];
            return RegisterHf(alias, cap, repo, param);
        }

        var expanded = Basic.ModelDownload.PathExpansion.Expand(source);
        if (File.Exists(expanded) || Directory.Exists(expanded))
        {
            // Path locale: auto-detect capability/kind/config. Se param (JSON di config) e'
            // presente, le sue chiavi vengono MERGED sopra la config auto-detectata (modelDir
            // resta quello del path) -- permette da BASIC di abilitare opzioni provider reali
            // (es. DFlash2: {"speculation":"dflash","draftModelDir":"<draft>","temperature":"0"}).
            var detected = Basic.ModelDownload.ModelScanner.DetectConfig(expanded, cap == Basic.AI.Rag.AiCapability.Chat ? null : cap);
            var config = new Dictionary<string, string>(detected.Config);
            // "engine" nel param JSON = override ESPLICITO del motore (kind del provider): es.
            // {"engine":"mlx"} per caricare un .gguf supportato col motore MLX invece di llama.cpp,
            // o {"engine":"llamacpp"}. Non è una chiave di config del provider: non va in config.
            var kind = detected.Kind;
            if (!string.IsNullOrWhiteSpace(param))
            {
                var parsed = ParseConfigJson(param);
                if (parsed is not null)
                    foreach (var kv in parsed)
                    {
                        if (string.Equals(kv.Key, "engine", StringComparison.OrdinalIgnoreCase))
                        { if (!string.IsNullOrWhiteSpace(kv.Value)) kind = kv.Value; continue; }
                        if (!string.Equals(kv.Key, "modelDir", StringComparison.OrdinalIgnoreCase))
                            config[kv.Key] = kv.Value;
                    }
            }
            Register(cap.ToRole(), alias, kind, config);
            return alias;
        }

        if (source.StartsWith("http://", StringComparison.OrdinalIgnoreCase)
            || source.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            // Endpoint remoto: param (modello) obbligatorio.
            if (string.IsNullOrWhiteSpace(param))
                throw new InvalidOperationException("?parametro modello omesso per endpoint remoto (es. REGISTER(alias, %CHAT, url, \"gpt-4o\"))");
            var kind = ClassifyRemoteKind(source);
            var config = new Dictionary<string, string> { ["baseUrl"] = source, ["model"] = param };
            Register(cap.ToRole(), alias, kind, config);
            return alias;
        }

        // Scheme registry: <scheme>:<id> (es. ollama:gemma, openai:gpt-4o) — baseURL dal config.
        var idx = source.IndexOf(':');
        if (idx > 0)
        {
            var scheme = source[..idx];
            var id = source[(idx + 1)..];
            var baseUrl = Basic.Providers.ProviderConfig.LookupScheme(scheme);
            if (baseUrl is null)
                throw new InvalidOperationException($"?scheme sconosciuto '{scheme}' — aggiungilo al config (providers.json)");
            var kind = ClassifyRemoteKind(baseUrl);
            var config = new Dictionary<string, string> { ["baseUrl"] = baseUrl, ["model"] = id };
            Register(cap.ToRole(), alias, kind, config);
            return alias;
        }

        // Alias di modello locale: cerco il nome tra i modelli rilevati in AiConfig.ModelPaths
        // (es. AI REGISTER "ocr", %OCR, "PaddleOCR-VL-1.6" — senza path). L'utente non deve
        // ricordare i path: basta il nome del modello trovato da ModelScanner.ScanAll.
        var roots = Basic.AI.AiConfig.ModelPaths;
        if (roots.Count > 0)
        {
            foreach (var m in Basic.ModelDownload.ModelScanner.ScanAll(roots))
            {
                if (m.Name.Equals(source, StringComparison.OrdinalIgnoreCase))
                {
                    var detected = Basic.ModelDownload.ModelScanner.DetectConfig(m.Path, cap);
                    Register(cap.ToRole(), alias, detected.Kind, detected.Config);
                    return alias;
                }
            }
        }

        throw new InvalidOperationException(
            $"?sorgente senza scheme: usa hf:<repo>, un path completo, http(s)://... o un nome di modello in AIPATH (ricevuto '{source}')");
    }

    /// <summary>Registra un modello embedded scaricandolo da HuggingFace (scheme hf:). Il
    /// download è sincrono con progress callback (modelOrDest è la cartella di destinazione,
    /// default samples/ai/models/&lt;tipo&gt;/). Dopo il download auto-detecta la capability.</summary>
    private string RegisterHf(string alias, Basic.AI.Rag.AiCapability capability, string repo, string? destDir)
    {
        var dest = destDir ?? DefaultModelDir(capability);
        var opts = new Basic.ModelDownload.ModelDownloadOptions
        {
            RepoId = repo,
            Kind = Basic.ModelDownload.ModelDownloadKind.Hf,
            Dest = dest,
            Quiet = true,
        };
        var result = Basic.ModelDownload.ModelDownloader.DownloadAsync(opts, Basic.Providers.ProviderHttp.CreateClient(new ProviderDescriptor(ProviderRole.Completion, alias, "hf", null)),
            log: null, progress: p => { /* REPL/BASIC renderizzano il progress — qui best-effort */ },
            ct: System.Threading.CancellationToken.None).GetAwaiter().GetResult();
        if (!result.Success)
            throw new InvalidOperationException($"?download fallito per {repo}: {result.Error}");

        var detected = Basic.ModelDownload.ModelScanner.DetectConfig(result.DestDir, capability == Basic.AI.Rag.AiCapability.Chat ? null : capability);
        Register(capability.ToRole(), alias, detected.Kind, detected.Config);
        return alias;
    }

    private static string DefaultModelDir(Basic.AI.Rag.AiCapability capability)
    {
        var baseDir = Path.Combine(AppContext.BaseDirectory, "samples", "ai", "models");
        var sub = capability switch
        {
            Basic.AI.Rag.AiCapability.Ocr => "vision",
            Basic.AI.Rag.AiCapability.Audio => "audio",
            _ => "gguf",
        };
        return Path.Combine(baseDir, sub);
    }

    /// <summary>Classifica il kind remoto dall'URL: path /api/chat o porta :11434
    /// (default ollama) -> ollama, /v1/messages -> anthropic, altrimenti openai (default).</summary>
    private static string ClassifyRemoteKind(string baseUrl)
    {
        var lower = baseUrl.ToLowerInvariant();
        if (lower.Contains("/api/chat") || lower.Contains(":11434")) return "ollama";
        if (lower.Contains("/v1/messages") || lower.Contains("anthropic")) return "anthropic";
        return "openai";
    }

    /// <summary>True se il provider è registrato (per i caller che implementano il no-op
    /// silenzioso del runtime C su provider sconosciuti — parità llm_ai_set_config).</summary>
    public bool IsRegistered(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock) { return _providers.ContainsKey(name); }
    }

    public bool Unregister(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.Remove(name, out var descriptor))
                return false;

            _instances.Remove(name);
            if (_roleOrder.TryGetValue(descriptor.Role, out var order))
                order.RemoveAll(n => string.Equals(n, name, StringComparison.OrdinalIgnoreCase));
            return true;
        }
    }

    public void Configure(string name, string key, string value)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentException.ThrowIfNullOrWhiteSpace(key);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");

            descriptor.SetConfig(key, value);
            _instances.Remove(name);
        }
    }

    /// <summary>Legge una chiave di config del provider (layer config AI, GETTEMP/GETBASE/...).</summary>
    public string? GetConfig(string name, string key)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            return _providers.TryGetValue(name, out var descriptor)
                ? descriptor.GetConfig(key)
                : null;
        }
    }

    /// <summary>Restituisce l'intero config del provider (per AI.INFO).</summary>
    public IReadOnlyDictionary<string, string> GetConfigMap(string name)
    {
        lock (_lock)
        {
            return _providers.TryGetValue(name, out var descriptor)
                ? descriptor.Config
                : new Dictionary<string, string>();
        }
    }

    public void SetPriority(string name, int priority)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            descriptor.Priority = priority;
        }
        ModelResidencyRegistry.SetPriority(name, priority);
    }

    public int GetPriority(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            return descriptor.Priority;
        }
    }

    /// <summary>Sets the relative weight used by <see cref="Rotate"/> to distribute load among
    /// providers tied on <see cref="ProviderDescriptor.Priority"/>. Only takes effect once at
    /// least one provider in a tied group has a non-default weight — with everyone at the
    /// default (1), <c>Rotate</c> behaves exactly as before (plain round-robin).</summary>
    public void SetWeight(string name, int weight)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            descriptor.Weight = weight;
        }
    }

    public int GetWeight(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            return descriptor.Weight;
        }
    }

    /// <summary>True se il provider è un modello gratuito (scoperto da DISCOVER, input+output==0).</summary>
    public bool IsFreeModel(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            return descriptor.IsFree;
        }
    }

    /// <summary>PROVIDER.DISCOVER — carica tutti i modelli associati a un'API key (es.
    /// OpenRouter) e li registra come provider "<prefix>:<model-id>". Port C# di
    /// unicorn-server core/providers/remote.py fetch_remote_models: interroga
    /// GET {baseUrl}/models (formato OpenAI data[] o OpenRouter models[]), estrae
    /// pricing (is_free = input+output==0, costo per 1M token) e context_window.
    /// I modelli free ricevono priority 10 (free-first), i pagati priority 0.
    /// onlyFree=true salta i modelli a pagamento. Ritorna gli alias registrati.</summary>
    public List<string> DiscoverModels(string prefix, string? capability,
                                       string baseUrl, string apiKey, bool onlyFree = false,
                                       HttpClient? http = null)
    {
        var cap = Basic.AI.Rag.AiCapabilityExtensions.FromConfig(capability);
        var kind = ClassifyRemoteKind(baseUrl);
        http ??= ProviderHttp.CreateClient(new ProviderDescriptor(cap.ToRole(), prefix, kind, null), null);
        http.DefaultRequestHeaders.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", apiKey);

        var url = baseUrl.TrimEnd('/') + "/models";
        var json = ProviderHttp.GetJson(http, url);
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        var items = doc.RootElement.TryGetProperty("data", out var data) && data.ValueKind == System.Text.Json.JsonValueKind.Array
            ? data
            : doc.RootElement.TryGetProperty("models", out var models) && models.ValueKind == System.Text.Json.JsonValueKind.Array
                ? models
                : throw new InvalidOperationException("?risposta /models senza data[] o models[]");

        var registered = new List<string>();
        foreach (var item in items.EnumerateArray())
        {
            if (!item.TryGetProperty("id", out var idEl) || idEl.ValueKind != System.Text.Json.JsonValueKind.String)
                continue;
            var modelId = idEl.GetString() ?? "";
            if (modelId.Length == 0) continue;

            var (isFree, costIn, costOut, ctx) = ParseModelPricing(item);
            if (onlyFree && !isFree) continue;

            var alias = $"{prefix}:{modelId}";
            var config = new Dictionary<string, string>
            {
                ["baseUrl"] = baseUrl,
                ["model"] = modelId,
            };
            if (ctx > 0) config["contextWindow"] = ctx.ToString();

            var descriptor = new ProviderDescriptor(cap.ToRole(), alias, kind, config,
                priority: isFree ? 10 : 0)
            {
                IsFree = isFree,
                CostInputPer1m = costIn,
                CostOutputPer1m = costOut,
            };
            lock (_lock)
            {
                EnsureDefaultsLocked();
                _providers[alias] = descriptor;
                if (!_roleOrder.TryGetValue(descriptor.Role, out var order))
                {
                    order = new List<string>();
                    _roleOrder[descriptor.Role] = order;
                }
                if (!order.Contains(alias, StringComparer.OrdinalIgnoreCase))
                    order.Add(alias);
            }
            registered.Add(alias);
        }
        return registered;
    }

    /// <summary>Parsa pricing/context dal singolo elemento di /models (port di _parse_cost
    /// e della logica di remote.py). Formato: pricing.input/.output (numero o stringa),
    /// context_window o max_context_length. Ritorna (isFree, costIn, costOut, ctx).</summary>
    private static (bool isFree, decimal costIn, decimal costOut, long ctx) ParseModelPricing(System.Text.Json.JsonElement item)
    {
        decimal ParseCost(System.Text.Json.JsonElement e)
        {
            switch (e.ValueKind)
            {
                case System.Text.Json.JsonValueKind.Number: return e.GetDecimal();
                case System.Text.Json.JsonValueKind.String:
                    return decimal.TryParse(e.GetString(), System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out var d) ? d : 0m;
                default: return 0m;
            }
        }

        var costIn = 0m;
        var costOut = 0m;
        if (item.TryGetProperty("pricing", out var pricing) && pricing.ValueKind == System.Text.Json.JsonValueKind.Object)
        {
            if (pricing.TryGetProperty("input", out var i) || pricing.TryGetProperty("prompt", out i))
                costIn = ParseCost(i);
            if (pricing.TryGetProperty("output", out var o) || pricing.TryGetProperty("completion", out o))
                costOut = ParseCost(o);
        }
        var ctx = 0L;
        if (item.TryGetProperty("context_window", out var cw) && cw.ValueKind == System.Text.Json.JsonValueKind.Number)
            ctx = cw.GetInt64();
        else if (item.TryGetProperty("max_context_length", out var mc) && mc.ValueKind == System.Text.Json.JsonValueKind.Number)
            ctx = mc.GetInt64();
        return (costIn == 0m && costOut == 0m, costIn, costOut, ctx);
    }

    /// <summary>PROVIDER.SETCREDITS — imposta il budget di crediti gratuiti del provider.
    /// -1 = illimitato. Il credit tracker decrementa a ogni completamento; a 0 il provider
    /// va Blocked e la rotazione passa al successivo.</summary>
    public void SetCredits(string name, int credits)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            descriptor.CreditsRemaining = credits;
            if (credits == 0)
                descriptor.Status = ProviderStatus.Blocked;
            else if (credits > 0)
                descriptor.Status = ProviderStatus.Available;
        }
    }

    public int GetCredits(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            return descriptor.CreditsRemaining;
        }
    }

    /// <summary>Credit tracker: decrementa i crediti del provider dopo un completamento
    /// riuscito (tokensUsed per il costo; per MVP ogni richiesta consuma un credito se
    /// non si conosce il costo). A 0 -> Status Blocked, così Rotate/GetNextHealthyLocked
    /// passano al successivo automaticamente.</summary>
    public void RecordCompletion(string role, string name, int tokensUsed)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor)) return;
            if (descriptor.CreditsRemaining < 0) return; // illimitato

            var remaining = descriptor.CreditsRemaining;
            if (tokensUsed > 0 && descriptor.CostInputPer1m > 0)
            {
                // costo proporzionale ai token (approssimato: tutto input)
                var costUnits = (int)Math.Ceiling((decimal)tokensUsed * descriptor.CostInputPer1m / 1_000_000m);
                remaining -= Math.Max(1, costUnits);
            }
            else
            {
                remaining -= 1;
            }
            descriptor.CreditsRemaining = Math.Max(0, remaining);
            if (descriptor.CreditsRemaining == 0)
                descriptor.Status = ProviderStatus.Blocked;
        }
    }

    public void MarkFailed(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (_providers.TryGetValue(name, out var descriptor))
            {
                descriptor.Status = ProviderStatus.Unavailable;
                _instances.Remove(name);
            }
        }
    }

    public void ResetFailed(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (_providers.TryGetValue(name, out var descriptor))
                descriptor.Status = ProviderStatus.Available;
        }
    }

    /// <summary>Resets every currently-Unavailable provider back to Available. Blocked/Damaged
    /// (deliberate manual overrides) are left untouched.</summary>
    public void ResetAllFailed()
    {
        lock (_lock)
        {
            foreach (var desc in _providers.Values)
            {
                if (desc.Status == ProviderStatus.Unavailable)
                    desc.Status = ProviderStatus.Available;
            }
        }
    }

    /// <summary>Sets the operational status explicitly. Used both at registration time and later.</summary>
    public void SetStatus(string name, ProviderStatus status)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            descriptor.Status = status;
            if (status != ProviderStatus.Available)
                _instances.Remove(name);
        }
    }

    public ProviderStatus GetStatus(string name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                throw new KeyNotFoundException($"Provider '{name}' is not registered.");
            return descriptor.Status;
        }
    }

    /// <summary>Reports an observed health result (background health check, or a live call outcome).
    /// Only toggles between Available/Unavailable — never overrides a Blocked/Damaged manual state.</summary>
    internal void ReportHealthResult(string name, bool healthy)
    {
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out var descriptor))
                return;
            if (descriptor.Status is ProviderStatus.Blocked or ProviderStatus.Damaged)
                return;

            descriptor.Status = healthy ? ProviderStatus.Available : ProviderStatus.Unavailable;
        }
    }

    /// <summary>Snapshot of every registered (role, name) pair, for the background health monitor.</summary>
    internal IReadOnlyList<(string Role, string Name)> GetAllProviderKeys()
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            return _providers.Values.Select(d => (d.Role, d.Name)).ToList();
        }
    }

    public void SetActive(string role, string name)
    {
        var normalizedRole = ProviderRole.Normalize(role);
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var order = GetRoleOrderLocked(normalizedRole);
            var index = order.FindIndex(n => string.Equals(n, name, StringComparison.OrdinalIgnoreCase));
            if (index < 0)
                throw new KeyNotFoundException($"Provider '{name}' is not registered for role '{role}'.");

            _activeIndex[normalizedRole] = index;
        }
    }

    /// <summary>ACTIVE del piano unificato: attiva un provider per capability. Se capability
    /// è omessa, deduce il ruolo dal provider registrato (cerca per alias tra tutti i ruoli).
    /// A differenza della vecchia REGISTERLOCAL che forzava l'attivo, qui chat/ocr/audio
    /// coesistono: ognuno ha il proprio attivo.</summary>
    public string Activate(string alias, string? capability = null)
    {
        var cap = Basic.AI.Rag.AiCapabilityExtensions.FromConfig(capability);
        SetActive(cap.ToRole(), alias);
        return alias;
    }

    /// <summary>Advances active provider for <paramref name="role"/> to the next Available one
    /// (priority desc, round-robin among equal priority), skipping Unavailable/Blocked/Damaged.</summary>
    public string Rotate(string role)
    {
        var normalizedRole = ProviderRole.Normalize(role);
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var order = GetRoleOrderLocked(normalizedRole);
            if (order.Count == 0)
                throw new InvalidOperationException($"No providers registered for role '{role}'.");

            var candidates = SortByPriorityLocked(order);
            var hasCustomWeights = candidates.Any(c => _providers.TryGetValue(c, out var d) && d.Weight != 1);

            if (!hasCustomWeights)
            {
                // Unchanged plain round-robin: advance from the current position, wrap, skip
                // Unavailable/Blocked/Damaged. This is the path every existing caller/test uses
                // as long as nobody has called SETWEIGHT.
                var startIdx = GetCurrentCandidateIndexLocked(normalizedRole, order, candidates);
                for (var i = 1; i <= candidates.Count; i++)
                {
                    var candidate = candidates[(startIdx + i) % candidates.Count];
                    if (_providers.TryGetValue(candidate, out var desc) && desc.Status == ProviderStatus.Available)
                    {
                        var orderIdx = order.FindIndex(n => string.Equals(n, candidate, StringComparison.OrdinalIgnoreCase));
                        _activeIndex[normalizedRole] = orderIdx;
                        return candidate;
                    }
                }

                throw new InvalidOperationException($"No Available provider to rotate to for role '{role}'.");
            }

            // Weighted mode: priority still governs which tier is eligible (highest priority
            // tier with at least one Available member wins); weight only breaks ties *within*
            // that tier, via smooth weighted round-robin (same algorithm nginx uses for
            // weighted load balancing) — proportional distribution without bursts.
            var tiers = candidates
                .GroupBy(c => _providers.TryGetValue(c, out var d) ? d.Priority : 0)
                .OrderByDescending(g => g.Key);

            foreach (var tier in tiers)
            {
                var availableInTier = tier
                    .Where(c => _providers.TryGetValue(c, out var d) && d.Status == ProviderStatus.Available)
                    .ToList();
                if (availableInTier.Count == 0)
                    continue;

                var winner = SelectWeightedRoundRobinLocked(availableInTier);
                var orderIdx = order.FindIndex(n => string.Equals(n, winner, StringComparison.OrdinalIgnoreCase));
                _activeIndex[normalizedRole] = orderIdx;
                return winner;
            }

            throw new InvalidOperationException($"No Available provider to rotate to for role '{role}'.");
        }
    }

    /// <summary>Smooth weighted round-robin: each candidate accumulates its weight every call;
    /// the highest accumulator wins and is reduced by the group's total weight. With N
    /// candidates all at weight 1 this reduces to plain round-robin (each visited once per N
    /// calls) — proportional, evenly-spread distribution otherwise (e.g. weights 3:1 produce
    /// A,A,B,A,A,A,B,A over 8 calls, not a burst of 6 A's followed by 2 B's).</summary>
    private string SelectWeightedRoundRobinLocked(List<string> names)
    {
        if (names.Count == 1)
            return names[0];

        string? winner = null;
        var maxCurrent = int.MinValue;
        var totalWeight = 0;
        foreach (var name in names)
        {
            var weight = _providers.TryGetValue(name, out var d) ? d.Weight : 1;
            totalWeight += weight;
            var current = _rotationCurrentWeight.GetValueOrDefault(name) + weight;
            _rotationCurrentWeight[name] = current;
            if (current > maxCurrent)
            {
                maxCurrent = current;
                winner = name;
            }
        }

        _rotationCurrentWeight[winner!] -= totalWeight;
        return winner!;
    }

    /// <summary>Tests provider connectivity: validates it exists and creates instance.</summary>
    public string TestConnection(string role, string name)
    {
        var normalizedRole = ProviderRole.Normalize(role);
        ProviderDescriptor? descriptor;
        lock (_lock)
        {
            EnsureDefaultsLocked();
            if (!_providers.TryGetValue(name, out descriptor))
                return $"ERROR: Provider '{name}' not found for role '{role}'.";
            if (descriptor.Role != normalizedRole)
                return $"ERROR: Provider '{name}' is registered for role '{descriptor.Role}', not '{role}'.";
        }

        try
        {
            var (ok, detail) = RunMinimalTestCore(normalizedRole, name, descriptor);
            return ok ? $"OK: {detail}" : $"ERROR: {detail}";
        }
        catch (Exception ex)
        {
            return $"ERROR: {name} failed: {ex.Message}";
        }
    }

    /// <summary>Runs the same minimal connectivity test as <see cref="TestConnection"/> but returns
    /// only a bool, and reports the outcome via <see cref="ReportHealthResult"/> — used by the
    /// background health monitor and by explicit BASIC-triggered health checks.</summary>
    internal bool RunMinimalTest(string role, string name)
    {
        var normalizedRole = ProviderRole.Normalize(role);
        ProviderDescriptor? descriptor;
        lock (_lock)
        {
            if (!_providers.TryGetValue(name, out descriptor) || descriptor.Role != normalizedRole)
                return false;
        }

        bool ok;
        try
        {
            (ok, _) = RunMinimalTestCore(normalizedRole, name, descriptor);
        }
        catch
        {
            ok = false;
        }

        ReportHealthResult(name, ok);
        return ok;
    }

    private static (bool Ok, string Detail) RunMinimalTestCore(string normalizedRole, string name, ProviderDescriptor descriptor)
    {
        if (normalizedRole == ProviderRole.Embedding)
        {
            var provider = ProviderFactory.CreateEmbedding(descriptor);
            var testVec = provider.Embed("test", 4);
            return (true, $"embedding provider '{name}' ({descriptor.Kind}), dims={testVec.Length}");
        }
        else if (normalizedRole == ProviderRole.Reranker)
        {
            var reranker = ProviderFactory.CreateReranker(descriptor);
            var score = reranker.Score("test query", "test document");
            return (true, $"reranker '{name}' ({descriptor.Kind}), score={score:F4}");
        }
        else
        {
            var completer = ProviderFactory.CreateCompletion(descriptor);
            var result = completer.Complete("test", "test");
            return (true, $"completion provider '{name}' ({descriptor.Kind}), response={result.Length} chars");
        }
    }

    private List<string> SortByPriorityLocked(List<string> order) =>
        order
            .Select((name, idx) => (name, idx))
            .OrderByDescending(x => _providers.TryGetValue(x.name, out var d) ? d.Priority : 0)
            .ThenBy(x => x.idx)
            .Select(x => x.name)
            .ToList();

    /// <summary>Returns the provider that would actually be used right now for <paramref name="role"/>
    /// — priority desc, skipping Unavailable/Blocked/Damaged — consistent with <see cref="Rotate"/>
    /// and <see cref="GetActiveEmbedding"/>/<see cref="GetActiveCompletion"/>.</summary>
    public string? GetActiveName(string role)
    {
        var normalizedRole = ProviderRole.Normalize(role);
        lock (_lock)
        {
            EnsureDefaultsLocked();
            return GetNextHealthyLocked(normalizedRole);
        }
    }

    public IReadOnlyList<string> List(string role)
    {
        var normalizedRole = ProviderRole.Normalize(role);
        lock (_lock)
        {
            EnsureDefaultsLocked();
            return GetRoleOrderLocked(normalizedRole).ToList();
        }
    }

    public IEmbeddingProvider GetActiveEmbedding()
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var name = GetNextHealthyLocked(ProviderRole.Embedding)
                       ?? throw new InvalidOperationException("No active embedding provider available (all failed or none registered).");
            return (IEmbeddingProvider)GetOrCreateInstanceLocked(name);
        }
    }

    /// <summary>Variante bootstrap-tolerante di <see cref="GetActiveEmbedding"/>: un provider
    /// configurato ma NON caricabile (assembly `Databases.Onnx` non distribuito — NativeAOT lazy,
    /// modello mancante, tipo non risolvibile) viene marcato Unavailable e SI PASSA AL SUCCESSIVO
    /// healthy (la caricabilità è parte della salute: failover reale, non solo stato dichiarato).
    /// Nessuno caricabile → null (assente): l'errore esplicito spetta alle superfici che lo USANO
    /// (AI RAG / MEMORY / pattern <c>BrainVaultBuild</c>), mai al bootstrap del runtime —
    /// todo Tokenizer-WordPiece P0a2 («deve restare null se non c'è, non diventare fixture»).
    /// `GetActiveEmbedding()` conserva la semantica throw (protetta da ProviderServerTests).
    /// Misura 2026-09-13: senza questa via, 197 test IlBackend morivano in `IlRuntime.SetCurrent`,
    /// e un `my-onnx` priority-200 non caricabile lasciava Default=null nonostante un
    /// `primary-emb` fixture healthy attivato esplicitamente (cross-test pollution).</summary>
    public IEmbeddingProvider? TryGetActiveEmbedding()
    {
        while (true)
        {
            string? name;
            lock (_lock)
            {
                EnsureDefaultsLocked();
                name = GetNextHealthyLocked(ProviderRole.Embedding);
            }

            if (name is null)
                return null;   // nessun provider healthy: assente

            try
            {
                lock (_lock)
                {
                    return (IEmbeddingProvider)GetOrCreateInstanceLocked(name);
                }
            }
            catch (Exception ex) when (ex is InvalidOperationException
                                       or FileNotFoundException
                                       or TypeLoadException
                                       or NotSupportedException
                                       or System.Reflection.TargetInvocationException)
            {
                MarkFailed(name);   // non caricabile → Unavailable; il loop passa al successivo (termina: insieme finito)
            }
        }
    }

    public IEmbeddingProvider? GetEmbeddingProviderByModel(string modelVersion)
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            foreach (var kvp in _providers)
            {
                if (string.Equals(kvp.Value.Role, ProviderRole.Embedding, StringComparison.OrdinalIgnoreCase))
                {
                    if (string.Equals(kvp.Key, modelVersion, StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(kvp.Value.GetConfig("modelVersion"), modelVersion, StringComparison.OrdinalIgnoreCase))
                    {
                        return (IEmbeddingProvider)GetOrCreateInstanceLocked(kvp.Key);
                    }
                }
            }
            if (modelVersion.StartsWith("fixture", StringComparison.OrdinalIgnoreCase))
            {
                return new FixtureEmbeddingProvider(modelVersion);
            }
            return null;
        }
    }

    public ILlmCompleter GetActiveCompletion()
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var name = GetNextHealthyLocked(ProviderRole.Completion)
                       ?? throw new InvalidOperationException("No active completion provider available (all failed or none registered).");
            return (ILlmCompleter)GetOrCreateInstanceLocked(name);
        }
    }

    /// <summary>GetActiveCompletion per ruolo (completion/ocr/audio) — usato da AI.OCR e
    /// AI.TRANSCRIBE che ora leggono il ruolo della propria capability, non il completion
    /// globale. Così chat/ocr/audio coesistono con attivi indipendenti.</summary>
    public ILlmCompleter GetActiveCompletion(string role)
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var normalizedRole = ProviderRole.Normalize(role);
            var name = GetNextHealthyLocked(normalizedRole)
                       ?? throw new InvalidOperationException($"No active '{normalizedRole}' provider available.");
            return (ILlmCompleter)GetOrCreateInstanceLocked(name);
        }
    }

    /// <summary>Resolves a specific, named completion provider directly, WITHOUT touching the
    /// shared "active" pointer for the role (no SetActive, no rotation). Exists because
    /// SetActive(role, name) + GetActiveCompletion() as two separate calls is only atomic if
    /// no other thread's SetActive can interleave between them -- true under GMBASIC's real
    /// GOMAXPROCS goroutine parallelism (see PCodeVm.cs) whenever two agents each pin a
    /// different provider via AGENTNAME.SET("provider", ...) and ASK concurrently: agent A's
    /// SetActive("modelA") could be immediately overwritten by agent B's concurrent
    /// SetActive("modelB") before A's GetActiveCompletion() runs, handing A modelB's completer.
    /// AgentServer.ResolveCompleter uses this instead whenever an agent has an explicit
    /// ProviderName pinned, reserving GetActiveCompletion() for the "whatever's globally
    /// active right now" case, where reading the shared pointer is the intended semantics.</summary>
    public ILlmCompleter GetCompletionByName(string name)
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            return (ILlmCompleter)GetOrCreateInstanceLocked(name);
        }
    }

    /// <summary>Active reranker — always available (bootstrap default is <c>keyword-reranker</c>,
    /// the same zero-dependency role "fixture" plays for embedding/completion), so this only
    /// throws if every registered reranker (including the bootstrap one) is Unavailable/Blocked/
    /// Damaged, which would require the caller to have explicitly broken the default.</summary>
    public IReranker GetActiveReranker()
    {
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var name = GetNextHealthyLocked(ProviderRole.Reranker)
                       ?? throw new InvalidOperationException("No active reranker available (all failed or none registered).");
            return (IReranker)GetOrCreateInstanceLocked(name);
        }
    }

    public int ActiveDimensions
    {
        get
        {
            lock (_lock)
            {
                EnsureDefaultsLocked();
                var name = GetNextHealthyLocked(ProviderRole.Embedding);
                if (name is not null && _providers.TryGetValue(name, out var descriptor)
                    && int.TryParse(descriptor.GetConfig("dimensions"), out var d) && d > 0)
                    return d;
            }

            return EmbeddingProviderResolver.DefaultDimensions;
        }
    }

    /// <summary>Persists registry state to a JSON file (secrets redacted to <c>$env:VAR</c>).</summary>
    public void Save(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        lock (_lock)
        {
            EnsureDefaultsLocked();
            var snapshot = ProviderRegistryPersistence.Capture(this);
            ProviderRegistryPersistence.SaveToFile(path, snapshot);
        }
    }

    /// <summary>Restores registry state from a JSON file (replaces current registry).</summary>
    public void Load(string path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var snapshot = ProviderRegistryPersistence.LoadFromFile(path);
        lock (_lock)
        {
            RestoreSnapshotLocked(snapshot);
            _defaultsLoaded = true;
        }
    }

    internal ProviderRegistryPersistence.Snapshot CaptureSnapshot() =>
        ProviderRegistryPersistence.CreateSnapshot(_providers, _roleOrder, _activeIndex);

    internal void RestoreSnapshot(ProviderRegistryPersistence.Snapshot snapshot)
    {
        lock (_lock)
        {
            RestoreSnapshotLocked(snapshot);
            _defaultsLoaded = true;
        }
    }

    internal static void ResetForTests()
    {
        ProviderHealthMonitor.Instance.Stop();
        lock (Instance._lock)
        {
            Instance.DisposeInstancesLocked();
            Instance._providers.Clear();
            Instance._roleOrder.Clear();
            Instance._activeIndex.Clear();
            Instance._rotationCurrentWeight.Clear();
            Instance._defaultsLoaded = false;
        }
    }

    /// <summary>Rilascia tutte le istanze (completer llama, embedding, ...) PRIMA che il processo
    /// termini. Un modello llama ancora caricato a exit lascia il ctx Metal vivo: a teardown del
    /// processo libggml-metal viene scaricata PRIMA della nostra dylib (ordine inverso di
    /// caricamento, poiche' llama_backend_init -> ggml_backend_load_all la carica dopo) e il
    /// distruttore del device fa scattare 'GGML_ASSERT([rsets->data count] == 0)' in
    /// ggml-metal-device.m:657. L'unica via robusta e' chiamare esplicitamente unload (via
    /// Dispose del completer) da un handler di ProcessExit prima che il processo esca.</summary>
    public static void Shutdown()
    {
        ProviderHealthMonitor.Instance.Stop();
        lock (Instance._lock)
        {
            Instance.DisposeInstancesLocked();
        }
        // Teardown canonico del runtime llama (llm_ai_shutdown): libera qualunque sessione/modello
        // residuo (anche istanze non tracciate qui) e chiama llama_backend_free PRIMA che il
        // processo termini. Senza, libggml-metal viene scaricata per prima a exit e il suo
        // distruttore trova i residency set vivi -> GGML_ASSERT([rsets->data count] == 0)
        // (ggml-metal-device.m:657), tipicamente DOPO un RAG build su Metal.
        // SOLO se il motore è stato caricato davvero: altrimenti il DllImport resolver lo
        // caricherebbe ORA (6,4 MB) per spegnere qualcosa che non è mai stato acceso.
        if (AiNativeResolver.WasLoaded("llm_ai_llamacpp"))
            Basic.Providers.LlamaCppNative.llm_ai_shutdown();
    }
    private void EnsureDefaultsLocked()
    {
        if (_defaultsLoaded)
            return;

        RegisterLocked(ProviderRole.Embedding, "fixture-embedding", "fixture", null);
        RegisterLocked(ProviderRole.Completion, "fixture-completion", "fixture", null);
        RegisterLocked(ProviderRole.Reranker, "keyword-reranker", "keyword", null);
        _activeIndex[ProviderRole.Embedding] = 0;
        _activeIndex[ProviderRole.Completion] = 0;
        _activeIndex[ProviderRole.Reranker] = 0;

        var settingsPath = Path.Combine(AppContext.BaseDirectory, "app.settings.json");
        var settings = VectorSettings.TryLoadFromFile(settingsPath);
        var modelPath = Environment.GetEnvironmentVariable("GMBASIC_ONNX_MODEL") ?? settings?.OnnxModelPath;
        if (!string.IsNullOrWhiteSpace(modelPath))
        {
            var config = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["model"] = modelPath,
                ["modelVersion"] = Environment.GetEnvironmentVariable("GMBASIC_ONNX_VERSION")
                                   ?? settings?.ModelVersion
                                   ?? "onnx-v1",
                ["dimensions"] = EmbeddingProviderResolver.DefaultDimensions.ToString()
            };
            var vocab = Environment.GetEnvironmentVariable("GMBASIC_ONNX_VOCAB") ?? settings?.OnnxVocabPath;
            if (!string.IsNullOrWhiteSpace(vocab))
                config["vocab"] = vocab;

            if (int.TryParse(Environment.GetEnvironmentVariable("GMBASIC_ONNX_DIMS"), out var envDim) && envDim > 0)
                config["dimensions"] = envDim.ToString();
            else if (settings?.Dimensions is > 0)
                config["dimensions"] = settings.Dimensions.Value.ToString();

            RegisterLocked(ProviderRole.Embedding, "onnx-default", "onnx", config);
            _activeIndex[ProviderRole.Embedding] = _roleOrder[ProviderRole.Embedding].Count - 1;
        }

        var rerankerModelPath = Environment.GetEnvironmentVariable("GMBASIC_RERANKER_MODEL")
                                 ?? settings?.RerankerModelPath;
        if (!string.IsNullOrWhiteSpace(rerankerModelPath))
        {
            var rerankerConfig = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["model"] = rerankerModelPath,
                ["modelVersion"] = Environment.GetEnvironmentVariable("GMBASIC_RERANKER_VERSION")
                                   ?? settings?.RerankerModelVersion
                                   ?? "onnx-reranker-v1"
            };
            var rerankerVocab = Environment.GetEnvironmentVariable("GMBASIC_RERANKER_VOCAB")
                                 ?? settings?.RerankerVocabPath;
            if (!string.IsNullOrWhiteSpace(rerankerVocab))
                rerankerConfig["vocab"] = rerankerVocab;

            RegisterLocked(ProviderRole.Reranker, "onnx-reranker-default", "onnx", rerankerConfig);
            _activeIndex[ProviderRole.Reranker] = _roleOrder[ProviderRole.Reranker].Count - 1;
        }

        _defaultsLoaded = true;
    }

    private void RegisterLocked(string role, string name, string kind,
        IReadOnlyDictionary<string, string>? config)
    {
        var descriptor = new ProviderDescriptor(role, name, kind, config);
        _providers[name] = descriptor;
        if (!_roleOrder.TryGetValue(role, out var order))
        {
            order = [];
            _roleOrder[role] = order;
        }

        if (!order.Contains(name, StringComparer.OrdinalIgnoreCase))
            order.Add(name);
    }

    private List<string> GetRoleOrderLocked(string normalizedRole) =>
        _roleOrder.GetValueOrDefault(normalizedRole) ?? [];

    /// <summary><paramref name="order"/> and <paramref name="candidates"/> are two different
    /// permutations of the same names (registration order vs. priority-sorted order) — this
    /// converts <c>_activeIndex</c> (an index into <paramref name="order"/>) into the matching
    /// index into <paramref name="candidates"/>, so callers never mix the two index spaces.</summary>
    private int GetCurrentCandidateIndexLocked(string normalizedRole, List<string> order, List<string> candidates)
    {
        var orderIdx = _activeIndex.GetValueOrDefault(normalizedRole) % order.Count;
        var currentName = order[orderIdx];
        var candidateIdx = candidates.FindIndex(n => string.Equals(n, currentName, StringComparison.OrdinalIgnoreCase));
        return candidateIdx >= 0 ? candidateIdx : 0;
    }

    /// <summary>Picks the best Available provider for <paramref name="normalizedRole"/>: strictly
    /// higher priority always wins regardless of any pin, but among providers tied on priority the
    /// one pinned via <see cref="SetActive"/>/<see cref="Rotate"/> (<c>_activeIndex</c>) is preferred
    /// — this is what lets SETACTIVE choose deliberately among equal-priority peers while priority
    /// still governs the overall evaluation order. Falls through to lower-priority providers only
    /// when higher ones are Unavailable/Blocked/Damaged.</summary>
    private string? GetNextHealthyLocked(string normalizedRole)
    {
        var order = GetRoleOrderLocked(normalizedRole);
        if (order.Count == 0)
            return null;

        var pinnedName = order[_activeIndex.GetValueOrDefault(normalizedRole) % order.Count];
        var candidates = order
            .Select((name, idx) => (name, idx))
            .OrderByDescending(x => _providers.TryGetValue(x.name, out var d) ? d.Priority : 0)
            .ThenByDescending(x => string.Equals(x.name, pinnedName, StringComparison.OrdinalIgnoreCase) ? 1 : 0)
            .ThenBy(x => x.idx)
            .Select(x => x.name)
            .ToList();

        foreach (var candidate in candidates)
        {
            if (_providers.TryGetValue(candidate, out var desc) && desc.Status == ProviderStatus.Available)
            {
                // Update active index to point to this provider
                var orderIdx = order.FindIndex(n => string.Equals(n, candidate, StringComparison.OrdinalIgnoreCase));
                if (orderIdx >= 0)
                    _activeIndex[normalizedRole] = orderIdx;
                return candidate;
            }
        }
        return null; // none Available
    }

    private object GetOrCreateInstanceLocked(string name)
    {
        if (_instances.TryGetValue(name, out var cached))
            return cached;

        if (!_providers.TryGetValue(name, out var descriptor))
            throw new KeyNotFoundException($"Provider '{name}' is not registered.");

        object instance = descriptor.Role switch
        {
            ProviderRole.Embedding => ProviderFactory.CreateEmbedding(descriptor),
            ProviderRole.Completion => ProviderFactory.CreateCompletion(descriptor),
            ProviderRole.Reranker => ProviderFactory.CreateReranker(descriptor),
            // OCR/Audio: ILlmCompleter che implementa IVision/IAudio (llamacpp-vision/audio, openai...).
            ProviderRole.Ocr => ProviderFactory.CreateCompletion(descriptor),
            ProviderRole.Audio => ProviderFactory.CreateCompletion(descriptor),
            _ => throw new InvalidOperationException($"Unknown role '{descriptor.Role}'.")
        };

        _instances[name] = instance;
        return instance;
    }

    private void DisposeInstancesLocked()
    {
        foreach (var inst in _instances.Values)
        {
            if (inst is IDisposable d)
                d.Dispose();
        }
        _instances.Clear();
    }

    private void RestoreSnapshotLocked(ProviderRegistryPersistence.Snapshot snapshot)
    {
        DisposeInstancesLocked();
        _providers.Clear();
        _roleOrder.Clear();
        _activeIndex.Clear();
        _instances.Clear();

        foreach (var (name, entry) in snapshot.Providers)
        {
            RegisterLocked(entry.Role, name, entry.Kind, entry.Config);
            var descriptor = _providers[name];
            descriptor.Priority = entry.Priority;
            descriptor.Status = ProviderStatusExtensions.ParseStatus(entry.Status);
            descriptor.Weight = entry.Weight;
            descriptor.IsFree = entry.IsFree;
            descriptor.CostInputPer1m = entry.CostInputPer1m;
            descriptor.CostOutputPer1m = entry.CostOutputPer1m;
            descriptor.CreditsRemaining = entry.CreditsRemaining;
        }

        foreach (var (role, order) in snapshot.RoleOrder)
        {
            if (!_roleOrder.TryGetValue(role, out var existing))
            {
                existing = [];
                _roleOrder[role] = existing;
            }

            existing.Clear();
            existing.AddRange(order);
        }

        foreach (var (role, index) in snapshot.ActiveIndex)
            _activeIndex[role] = index;
    }
}
