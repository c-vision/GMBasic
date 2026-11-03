// FILE: Basic.Agents.AgentRecord
using System.Text;
using Basic.AI.Rag;

namespace Basic.Agents;

/// <summary>In-memory agent state: prompt, provider binding, chat history.</summary>
public sealed class AgentRecord
{
    public string Id { get; init; } = Guid.NewGuid().ToString("N");

    public required string Name { get; init; }

    public string Model { get; set; } = string.Empty;

    public double Temperature { get; set; } = 0.7;

    public int MaxTokens { get; set; } = 2048;

    /// <summary>Prompt di sistema dell'agente. Default: il prompt di base dalla configurazione AI
    /// (AiConfig.SystemPrompt, modificabile da config/ai.json); se l'utente lo imposta via
    /// AI.AGENTSET agente, systemPrompt, ... questo valore prevale.</summary>
    public string SystemPrompt { get; set; } = Basic.AI.AiConfig.SystemPrompt;

    /// <summary>Costruisce il prompt di sistema finale dell'agente. Se le skills sono abilitate
    /// (SkillsEnabled), il prompt viene arricchito con l'indice delle skills + descrizione e
    /// percorso esatto di ogni skill. Usato sia dal path semplice (Complete) sia dal tool loop.</summary>
    public string ResolveSystemPrompt()
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine(SystemPrompt);
        sb.AppendLine();

        // Cartella log configurabile (AI LOG PATH): l'agente deve sapere dove scrivere.
        try
        {
            sb.AppendLine($"## Agent log folder");
            sb.AppendLine($"Scrivi i log nella cartella: `{Basic.AI.AiConfig.LogPath}`");
            sb.AppendLine("(impostata con AI LOG PATH; default ai-logs/)");
        }
        catch { /* best effort */ }

        if (MemoryEnabled)
        {
            sb.AppendLine();
            sb.AppendLine("## Memoria (second brain)");
            sb.AppendLine("Hai accesso a una memoria condivisa (fatti, note, episodi) con tutti gli agenti/team.");
            sb.AppendLine("Prima di rispondere, se serve, consulta i fatti rilevanti con AI.MEMORYASK.");
            sb.AppendLine("Dopo un task, salva i fatti durevoli e verificati con MEMORY SAVE (non opinioni momentanee).");
            sb.AppendLine("Non ripetere azioni gia' tentate in questo task.");
        }

        // F2 — livello AGENT dello scope cervelli: se l'agente DICHIARA i brain, il prompt lo dice
        // esplicitamente (nome + eventuale alias) e indica i tool che li usano. Se non dichiara nulla
        // la sezione NON compare (prompt invariato per gli agenti esistenti).
        var declaredBrains = Agents.AgentBrains.Describe(Brains);
        if (declaredBrains.Length > 0 && declaredBrains != "(set vuoto)")
        {
            sb.AppendLine();
            sb.AppendLine("## Cervelli (second brain) assegnati a te");
            sb.AppendLine($"Puoi interrogare SOLO questi cervelli: {declaredBrains}");
            sb.AppendLine("Usa i tool `rag.search` / `rag.context` / `rag.ask` (indice semantico) e "
                        + "`graph.query` / `graph.neighbors` / `graph.path` (knowledge graph); "
                        + "senza argomento `brain` restano nel tuo scope.");
        }

        if (PluginsEnabled)
        {
            sb.AppendLine();
            sb.AppendLine("## Plugin disponibili");
            sb.AppendLine("Hai a disposizione le seguenti capacita' operative (plugin). Quando una richiesta");
            sb.AppendLine("corrisponde a un plugin, leggi il suo PLUGIN.md e usa la capacita' (tool o AI.PLUGINCALL).");
            sb.AppendLine();
            try
            {
                foreach (var p in Basic.AI.PluginRegistry.List())
                {
                    var state = p.Enabled ? "attivo" : "disattivo";
                    sb.AppendLine($"- **{p.Name}**: {p.Description} · stato: {state} · trigger: {p.Trigger}");
                }
            }
            catch { /* best effort */ }
        }

        if (!SkillsEnabled) return sb.ToString();

        sb.AppendLine();
        sb.AppendLine("## Skills disponibili");
        sb.AppendLine("Hai a disposizione le seguenti skills. Quando una richiesta corrisponde a una skill,");
        sb.AppendLine("LEGGI il file SKILL.md al percorso indicato e segui le sue istruzioni.");
        sb.AppendLine();

        try
        {
            foreach (var skill in Basic.AI.SkillIndex.Scan())
            {
                sb.AppendLine($"- **{skill.Item1}**: {(string.IsNullOrEmpty(skill.Item3) ? "vedi file" : skill.Item3)}");
                sb.AppendLine($"  Percorso: `{skill.Item2}`");
            }
        }
        catch { /* best effort */ }

        return sb.ToString();
    }

    /// <summary>Prompt di sistema arricchito con il RETRIEVAL contestuale della memoria:
    /// i top-k fatti/note rilevanti per <paramref name="memoryQuery"/> (in genere l'ultimo
    /// prompt utente) vengono iniettati nella sezione memoria. Solo se <c>MemoryEnabled</c>.</summary>
    public string ResolveSystemPrompt(string? memoryQuery)
    {
        var basePrompt = ResolveSystemPrompt();
        if (!MemoryEnabled || string.IsNullOrWhiteSpace(memoryQuery)) return basePrompt;
        try
        {
            var facts = Basic.AI.MemoryBrain.Search(memoryQuery, topK: 5);
            if (string.IsNullOrWhiteSpace(facts)) return basePrompt;
            return basePrompt
                 + "\n\n## Memoria rilevante (contesto attuale)\n"
                 + facts;
        }
        catch { return basePrompt; }
    }

    /// <summary>Se true, il prompt di sistema dell'agente viene arricchito (in ASK) con il
    /// riepilogo delle skills disponibili (SKILLS_PROMPT.md) + descrizione e percorso esatto
    /// di ogni skill, così l'agente può attivarla quando serve. Opzionale (default false).</summary>
    public bool SkillsEnabled { get; set; }

    /// <summary>Se true, il prompt di sistema dell'agente include la sezione "Plugin disponibili"
    /// (PluginRegistry.List + PLUGINS_PROMPT.md): i plugin operativi attivi con descrizione e
    /// trigger, così l'agente può usare le loro capacità (tool o AI.PLUGINCALL).</summary>
    public bool PluginsEnabled { get; set; }

    /// <summary>Streaming per-token nelle sessioni native (ISessionStreamingCompleter).
    /// Gate via <c>AI AGENT SET nome, sessionstreaming, ON|OFF</c> — default OFF per
    /// l'overhead per-chiamata del binding nativo.</summary>
    public bool SessionStreamingEnabled { get; set; }

    /// <summary>Tool dei server MCP abilitati (auto-discovery, nomi <c>server.tool</c>) visibili
    /// a questo agente. Gate via <c>AI AGENT SET nome, clienttools, ON|OFF</c> — default OFF
    /// per non gonfiare il prompt di ogni agente con tutti i tool dei server.</summary>
    public bool ClientToolsEnabled { get; set; }

    /// <summary>Router a retrieval per il catalogo tool (N1, Needle3): con catalogo grande,
    /// al prompt vanno solo i top-k rilevanti invece di tutto il catalogo. Gate via
    /// <c>AI AGENT SET nome, toolretrieval, ON|OFF</c> — default OFF, il comportamento resta
    /// quello storico (tutto il catalogo nel prompt) finche' non lo si accende.</summary>
    public bool ToolRetrievalEnabled { get; set; }

    /// <summary>Livello di permesso dell'agente (Sandbox/Supervised/YOLO — vedi
    /// AgentPermissionService). Default YOLO in v1 per non rompere i sample esistenti;
    /// il flip a Sandbox-default è un follow-up (Piano Agent-Permessi).</summary>
    public AgentPermissionLevel PermissionLevel { get; set; } = AgentPermissionLevel.YOLO;

    /// <summary>G4: governance workflow profile — 1=FAST (default, approval solo upload/high-risk),
    /// 2=GOVERNED (ogni non-read richiede approval). Impostato con AI AGENT SET nome PROFILE %FAST|%GOVERNED.</summary>
    public int GovernanceProfile { get; set; } = 1;

    /// <summary>Override per-tool sulla policy (vince sul livello): "allow" | "deny" | "approve".
    /// Impostato con `AI AGENT SET nome, toolpolicy, <tool>, allow|deny|approve`.</summary>
    public Dictionary<string, string> ToolPolicies { get; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Second brain accessibile a questo agente (retrieval nel prompt). Gate via
    /// <c>AI AGENT SET nome, memory, ON|OFF</c> — default OFF finche' il retrieval nel
    /// prompt (Fase 4) non e' attivo.</summary>
    public bool MemoryEnabled { get; set; }
    public string? ProviderName { get; set; }

    /// <summary>B5.1 — Identità (ECDSA secp256k1, PEM). Popolate da AgentIdentity.AttachKeypair.</summary>
    public string? PublicKey { get; set; }
    public string? PrivateKey { get; set; }

    /// <summary>Browser per le sessioni di questo agente: "chromium"|"obscura"|"auto".
    /// Configurato da BASIC con <c>AI AGENT SET nome, BROWSER, "obscura"</c> (default "auto"
    /// → BrowserProviderRegistry sceglie obscura se cached, altrimenti chromium).
    /// Verificato: il plugin browser usa questo valore per decidere quale binario lanciare.</summary>
    public string BrowserProvider { get; set; } = "auto";

    // ── Estensioni RecursiveMAS §9 ───────────────────────────────────────────
    /// <summary>Quiet-STaR (9.3): se true, ASK wrappa il prompt con <c>&lt;|start_of_thought|&gt;</c>
    /// (token di pensiero prima dell'output). Gate via <c>AI AGENT SET nome, thinking, ON|OFF</c>.</summary>
    public bool ThinkingEnabled { get; set; }

    /// <summary>Quiet-STaR (9.3): numero di token di pensiero per output (default 8).</summary>
    public int ThinkingTokens { get; set; } = 8;

    /// <summary>T1.1 Reasoning Regimes (2026-09-09): con ThinkingEnabled e ReasoningSamples&gt;1
    /// ASK esegue Best-of-N sampling+voting (voto di maggioranza) sul percorso senza tool.
    /// Gate via <c>AI AGENT SET nome, samples, n</c>.</summary>
    public int ReasoningSamples { get; set; } = 1;

    // ── T3.1 Differential Router (AI-Research Core-FollowOn R1, 2026-09-09) ──
    /// <summary>Router TES per-task: ON ⇒ il path ASK senza tool sceglie tra
    /// <see cref="SmallProvider"/> (locale) e <see cref="FrontierProvider"/> (grande/remoto)
    /// tramite <c>Basic.AI.Providers.DifferentialRouter</c>. Default OFF = invariato.</summary>
    public bool RouterEnabled { get; set; }

    /// <summary>Nome del provider locale/piccolo usato dal router quando Decide → Local.</summary>
    public string? SmallProvider { get; set; }

    /// <summary>Nome del provider grande/frontier usato dal router quando Decide → Frontier.</summary>
    public string? FrontierProvider { get; set; }

    /// <summary>A1/B5: categoria di task dell'agente (deep/quick/ultrabrain/…). Se impostata e
    /// il routing automatico è attivo, <see cref="AgentServer.PickRoutedProvider"/> consulta
    /// <c>Basic.AI.Providers.CategoryRegistry</c> prima del Differential Router.</summary>
    public string? Category { get; set; }

    /// <summary>MAR/MIRROR (9.5): modalità riflessione (intra|inter|both|off).</summary>
    public string ReflectionMode { get; set; } = "off";

    /// <summary>Critic persona usata dall'agent in modalità reflection (es. spec|edge|logic).</summary>
    public string? CriticPersona { get; set; }

    public List<AgentChatMessage> History { get; } = [];

    /// <summary>MCP tool names bound to this agent for ASK tool-calling loop.</summary>
    public HashSet<string> ToolNames { get; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Native session state (FASE 2) -- not persisted (SAVE/LOAD, AgentPersistence):
    /// a native handle is only valid for the lifetime of the process/completer that created
    /// it, so a fresh session is created on first ASK after every load/restart.</summary>
    internal ISessionAwareLlmCompleter? SessionOwner { get; set; }

    internal int? SessionId { get; set; }

    /// <summary>FASE 2b: non-null when this agent's session has been swapped to disk (via
    /// <see cref="IDiskSwappableSessionCompleter"/>) to make room under the owning completer's
    /// <c>kvSwapMaxHotSessions</c> cap -- <see cref="SessionId"/> is null while this is set.
    /// AgentServer.Ask swaps it back in lazily on the next call for this agent.</summary>
    internal string? SwappedPathPrefix { get; set; }

    /// <summary>Per-agent lock guarding every mutable field above plus the actual native
    /// ASK call -- GMBASIC's PCodeVm runs real GOMAXPROCS worker threads (see PCodeVm.cs),
    /// so concurrent goroutines can legitimately call AIAGENT.ASK on different agents at the
    /// same time. Locking per-agent (instead of one lock for the whole AgentServer) lets two
    /// different agents' sessions actually decode in parallel on separate cores, instead of
    /// queuing behind each other for no reason -- same per-object locking discipline already
    /// used elsewhere for goroutine-safe state (see BRT/Gpm/GpmCore.cs). AgentServer's own
    /// directory lock (_dirLock) is always the innermost lock -- never acquired while holding
    /// this one for a DIFFERENT agent -- to keep lock ordering acyclic; see AgentServer.cs's
    /// class doc comment for the full discipline.</summary>
    internal readonly Lock StateLock = new();

    // ── Self-Consistency / Semantic Entropy (P3) ─────────────────────────────
    /// <summary>Se true, abilita il controllo di self-consistency (AI.CONSISTENCY) dopo ogni risposta dell'agente.</summary>
    public bool CheckConsistency { get; set; }

    /// <summary>Soglia di risk per considerare una risposta consistente (default 0.7). Se risk >= threshold, trigger escalation.</summary>
    public double ConsistencyRiskThreshold { get; set; } = 0.7;

    /// <summary>Numero di campioni K per AI.CONSISTENCY (default 5).</summary>
    public int ConsistencySampleCount { get; set; } = 5;

    /// <summary>Temperatura per il sampling K× (default 0.7).</summary>
    public float ConsistencyTemperature { get; set; } = 0.7f;

    /// <summary>Max token per risposta nel sampling K× (default 1024).</summary>
    public int ConsistencyMaxTokens { get; set; } = 1024;

    /// <summary>
    /// Livello AGENT dello scope dei cervelli (F2 di `Knowledge/Brain-Scope-Modello.md`): dichiarazione CSV
    /// dei brain che questo agente può interrogare, con alias opzionali — es. <c>"finance AS fin, airline"</c>.
    /// Vuoto (default) = nessuna dichiarazione → i tool restano al comportamento precedente (brain attivati
    /// globalmente). Un valore con soli separatori = dichiarazione ESPLICITA del set vuoto (nessun brain).
    /// Impostabile con `AI AGENT SET &lt;agente&gt;, brains, "finance AS fin"`.
    /// </summary>
    public string Brains { get; set; } = string.Empty;

    /// <summary>Alias del vector DB per RAG grounding (es. "mydb").</summary>
    public string VectorDbAlias { get; set; } = string.Empty;

    /// <summary>Nome della collection nel vector DB per RAG grounding.</summary>
    public string VectorCollection { get; set; } = string.Empty;

    /// <summary>Ultimo prompt utente inviato all'agente (usato per RAG grounding).</summary>
    public string LastUserPrompt { get; set; } = string.Empty;
}

/// <summary>Single chat turn stored on an agent.</summary>
public sealed record AgentChatMessage(string Role, string Content);
