// FILE: Basic.Agents.AgentToolLoop
using Basic.Mcp;
using Basic.SQLite.Vector;
using Basic.AI.Rag;
using Basic.AI.Uncertainty;

namespace Basic.Agents;

/// <summary>ASK loop with tool-call round-trips for agents with registered MCP tools.</summary>
public static class AgentToolLoop
{
    public const int DefaultMaxRounds = 5;

    /// <summary>Run completion with tool dispatch until final text or max rounds.</summary>
    public static string AskWithTools(
        AgentRecord agent,
        ILlmCompleter completer,
        IMcpToolDispatcher dispatcher,
        string userPrompt,
        int maxRounds = DefaultMaxRounds,
        CancellationToken cancellationToken = default,
        int? sessionId = null,
        Action<string, long>? boundary = null)
    {
        ArgumentNullException.ThrowIfNull(agent);
        ArgumentNullException.ThrowIfNull(completer);
        ArgumentNullException.ThrowIfNull(dispatcher);
        ArgumentException.ThrowIfNullOrWhiteSpace(userPrompt);

        // Il prompt utente entra nella history SUBITO (prima di ogni path): AskWithToolsNative
        // lo legge per applicarlo alla sessione esterna e per l'euristica di forcing, e
        // AskWithToolsSession lo riusa. I path interni NON ri-aggiungono (niente duplicati).
        agent.History.Add(new AgentChatMessage("user", userPrompt));
        boundary?.Invoke("user", CtxChars(agent));

        // Percorso NATIVO (FASE 3): il modello (es. MLX con constrained decoding) decide il
        // tool direttamente nel decoding, senza istruzioni JSON nel prompt. Consuma la sessione
        // (DispatchTool registra il turno da solo — niente GenerateFromSession dopo). DEVE
        // venire PRIMA del check sessione generico: AskWithToolsSession (JSON parse) non usa
        // il constrained decode, e i completer nativi sarebbero sempre instradati li' quando
        // una sessione e' attiva (che e' sempre, via EnsureSessionLocked) — FASE 3 morta.
        var nativeTools = ResolveAgentTools(agent, userPrompt);
        if (completer is INativeToolDispatchCompleter native && nativeTools.Count > 0)
        {
            return AskWithToolsNative(agent, native, dispatcher, nativeTools,
                agent.ResolveSystemPrompt(userPrompt), maxRounds, cancellationToken, sessionId, boundary);
        }

        // Sessione persistente (KV riusata tra i turni): round via AppendMessage/
        // GenerateFromSession, niente ri-tokenizzazione del transcript.
        if (sessionId is { } sid && completer is ISessionAwareLlmCompleter sac)
            return AskWithToolsSession(agent, sac, dispatcher, userPrompt, sid, maxRounds, cancellationToken, boundary);

        var tools = ResolveAgentTools(agent, userPrompt);

        var systemPrompt = BuildSystemPrompt(agent.ResolveSystemPrompt(userPrompt), tools);

        var rounds = 0;
        var executedTools = new List<ToolExecution>();
        var attempted = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var lastAnswer = "";

        while (rounds < maxRounds)
        {
            cancellationToken.ThrowIfCancellationRequested();
            rounds++;

            var userTranscript = BuildUserContext(agent);
            if (attempted.Count > 0)
                userTranscript += "\nAzioni gia' tentate in questo task (NON ripeterle): " + string.Join(", ", attempted.Keys) + "\n";

            string answer;
            IReadOnlyList<ToolCallRequest>? toolCalls;
            if (completer is IToolResultAwareCompleter tra)
            {
                // Multi-round NATIVO con risultati strutturati (Anthropic tool_result).
                var outcome = executedTools.Count == 0
                    ? tra.CompleteWithTools(systemPrompt, userTranscript, tools)
                    : tra.CompleteWithToolResults(systemPrompt, userTranscript, tools, executedTools);
                answer = outcome.Text ?? "";
                toolCalls = outcome.ToolCalls;
            }
            else if (completer is IToolCallingCompleter tcc)
            {
                // Tool-calling NATIVO dell'API (OpenAI/Ollama): decisione strutturata,
                // niente JSON inventato da parsare.
                var outcome = tcc.CompleteWithTools(systemPrompt, userTranscript, tools);
                answer = outcome.Text ?? "";
                toolCalls = outcome.ToolCalls;
            }
            else
            {
                answer = completer.Complete(systemPrompt, userTranscript);
                toolCalls = ToolCallParser.TryParse(answer);
            }

            // Fallback: modelli che emettono il JSON della tool-call come testo (invece del
            // tool_calls strutturato dell'API) vengono comunque eseguiti. Il parser gestisce
            // anche il formato fenced (```json ... ```).
            if (toolCalls is null)
                toolCalls = ToolCallParser.TryParse(answer);

            // Self-Consistency check (P3): verify answer consistency via K-path sampling.
            // If risk is high or collapse_suspected, attempt RAG grounding escalation.
            if (!CheckConsistencyAndEscalate(agent, completer, dispatcher, userPrompt, answer, maxRounds, cancellationToken, boundary, ref rounds, ref lastAnswer))
            {
                // Escalation succeeded - use grounded answer (already handled in CheckConsistencyAndEscalate)
                // Continue to next round with grounded answer
                continue;
            }

            // State probe: output identico a quello del round precedente -> nessun progresso.
            if (answer.Length > 0 && answer.Equals(lastAnswer, StringComparison.Ordinal))
            {
                var msg = "Loop rilevato: risposta identica senza progresso. Interrompo.";
                agent.History.Add(new AgentChatMessage("assistant", answer));
                agent.History.Add(new AgentChatMessage("assistant", msg));
                return msg;
            }
            lastAnswer = answer;

            if (toolCalls is null || toolCalls.Count == 0)
            {
                agent.History.Add(new AgentChatMessage("assistant", answer));
                boundary?.Invoke("final", CtxChars(agent));
                return answer;
            }

            agent.History.Add(new AgentChatMessage("assistant", answer));
            boundary?.Invoke("toolCall", CtxChars(agent));

            foreach (var call in toolCalls)
            {
                var key = call.Name + "|" + call.ArgumentsJson;
                attempted[key] = attempted.TryGetValue(key, out var c) ? c + 1 : 1;
                if (attempted[key] >= 3)
                {
                    var msg = $"Loop rilevato: l'azione '{call.Name}' e' stata tentata 3 volte senza progresso. Interrompo.";
                    agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {msg}"));
                    agent.History.Add(new AgentChatMessage("assistant", msg));
                    boundary?.Invoke("loopAbort", CtxChars(agent));
                    return msg;
                }
                var resultText = InvokeToolCall(agent, call, tools, dispatcher, agent.ClientToolsEnabled, cancellationToken);
                executedTools.Add(new ToolExecution(call.Id, call.Name, resultText));
                agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                boundary?.Invoke("toolResult", CtxChars(agent));
                AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, agent.Name,
                    $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
            }
        }

        throw new InvalidOperationException($"Tool-calling loop exceeded {maxRounds} rounds.");
    }

    /// <summary>Tool-loop su sessione persistente: ogni round genera dalla KV (riuso tra i
    /// turni dell'agente), i risultati dei tool rientrano nella sessione. La sessione NON viene
    /// distrutta — resta viva per il turno successivo dell'agente.</summary>
    private static string AskWithToolsSession(
        AgentRecord agent,
        ISessionAwareLlmCompleter sac,
        IMcpToolDispatcher dispatcher,
        string userPrompt,
        int sessionId,
        int maxRounds,
        CancellationToken cancellationToken,
        Action<string, long>? boundary = null)
    {
        // NB: il messaggio "user" e' gia' stato aggiunto alla history da AskWithTools.
        sac.AppendMessage(sessionId, "user", userPrompt);
        boundary?.Invoke("user", CtxChars(agent));

        var tools = ResolveAgentTools(agent, userPrompt);

        for (var round = 0; round < maxRounds; round++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            var answer = sac.GenerateFromSession(sessionId, agent.MaxTokens);
            var toolCalls = ToolCallParser.TryParse(answer);

            if (toolCalls is null || toolCalls.Count == 0)
            {
                agent.History.Add(new AgentChatMessage("assistant", answer));
                boundary?.Invoke("final", CtxChars(agent));
                return answer;
            }

            agent.History.Add(new AgentChatMessage("assistant", answer));
            boundary?.Invoke("toolCall", CtxChars(agent));
            foreach (var call in toolCalls)
            {
                var resultText = InvokeToolCall(agent, call, tools, dispatcher, agent.ClientToolsEnabled, cancellationToken);
                agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, agent.Name,
                    $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
                sac.AppendMessage(sessionId, "tool", resultText);
                boundary?.Invoke("toolResult", CtxChars(agent));
            }
        }

        throw new InvalidOperationException($"Tool-calling loop exceeded {maxRounds} rounds.");
    }

    /// <summary>Tool-calling con STREAMING della risposta finale. I round intermedi (tool
    /// call) non vengono inoltrati: i loro chunk sono accumulati e scartati; solo la risposta
    /// finale (senza tool call) viene consegnata chunk per chunk via <paramref name="onToken"/>.
    /// Con completer non-streaming o nessun tool ripiega a <see cref="AskWithTools"/> (chunk
    /// unico). Il percorso NATIVO (INativeToolDispatchCompleter) resta bloccante.</summary>
    public static string AskWithToolsStream(
        AgentRecord agent,
        ILlmCompleter completer,
        IMcpToolDispatcher dispatcher,
        string userPrompt,
        Action<string> onToken,
        int maxRounds = DefaultMaxRounds,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(agent);
        ArgumentNullException.ThrowIfNull(completer);
        ArgumentNullException.ThrowIfNull(dispatcher);
        ArgumentException.ThrowIfNullOrWhiteSpace(userPrompt);

        if (completer is not IStreamingLlmCompleter sc)
            return AskWithTools(agent, completer, dispatcher, userPrompt, maxRounds, cancellationToken);

        agent.History.Add(new AgentChatMessage("user", userPrompt));

        var tools = ResolveAgentTools(agent, userPrompt);

        var systemPrompt = BuildSystemPrompt(agent.ResolveSystemPrompt(userPrompt), tools);
        var rounds = 0;
        var attempted = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var lastAnswer = "";

        while (rounds < maxRounds)
        {
            cancellationToken.ThrowIfCancellationRequested();
            rounds++;

            var userTranscript = BuildUserContext(agent);
            if (attempted.Count > 0)
                userTranscript += "\nAzioni gia' tentate in questo task (NON ripeterle): " + string.Join(", ", attempted.Keys) + "\n";
            var chunks = new List<string>();
            foreach (var chunk in sc.CompleteStream(systemPrompt, userTranscript))
                if (!string.IsNullOrEmpty(chunk)) chunks.Add(chunk);
            var answer = string.Concat(chunks);
            var toolCalls = ToolCallParser.TryParse(answer);

            if (answer.Length > 0 && answer.Equals(lastAnswer, StringComparison.Ordinal))
            {
                var msg = "Loop rilevato: risposta identica senza progresso. Interrompo.";
                agent.History.Add(new AgentChatMessage("assistant", msg));
                onToken?.Invoke(msg);
                return msg;
            }
            lastAnswer = answer;

            if (toolCalls is null || toolCalls.Count == 0)
            {
                // Risposta finale: consegnala in streaming, chunk per chunk.
                agent.History.Add(new AgentChatMessage("assistant", answer));
                foreach (var chunk in chunks) onToken?.Invoke(chunk);
                return answer;
            }

            agent.History.Add(new AgentChatMessage("assistant", answer));
            foreach (var call in toolCalls)
            {
                var key = call.Name + "|" + call.ArgumentsJson;
                attempted[key] = attempted.TryGetValue(key, out var c) ? c + 1 : 1;
                if (attempted[key] >= 3)
                {
                    var msg = $"Loop rilevato: l'azione '{call.Name}' e' stata tentata 3 volte senza progresso. Interrompo.";
                    agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {msg}"));
                    agent.History.Add(new AgentChatMessage("assistant", msg));
                    onToken?.Invoke(msg);
                    return msg;
                }
                var resultText = InvokeToolCall(agent, call, tools, dispatcher, agent.ClientToolsEnabled, cancellationToken);
                agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, agent.Name,
                    $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
            }
        }

        throw new InvalidOperationException($"Tool-calling loop exceeded {maxRounds} rounds.");
    }

    private static string AskWithToolsNative(
        AgentRecord agent,
        INativeToolDispatchCompleter native,
        IMcpToolDispatcher dispatcher,
        IReadOnlyList<McpToolDefinition> tools,
        string systemPrompt,
        int maxRounds,
        CancellationToken cancellationToken,
        int? sessionId = null,
        Action<string, long>? boundary = null)
    {
        var ownsSession = sessionId is null;
        var sid = sessionId ?? native.CreateSession();
        try
        {
            if (ownsSession)
            {
                native.AppendMessage(sid, "system", systemPrompt);
                native.AppendMessage(sid, "user", BuildUserContext(agent));
            }
            else
            {
                // Sessione esterna (EnsureSessionLocked ha gia' applicato il system prompt):
                // il prompt utente va applicato alla sessione (DispatchTool lo legge da
                // state.Messages per l'euristica di forcing, e il modello lo vede nel KV).
                var lastUser = agent.History.LastOrDefault(m => m.Role == "user")?.Content;
                if (!string.IsNullOrEmpty(lastUser))
                    native.AppendMessage(sid, "user", lastUser);
            }

            for (var round = 0; round < maxRounds; round++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                var decision = native.DispatchTool(sid, tools);

                if (decision.FinalAnswer is { Length: > 0 } answer)
                {
                    agent.History.Add(new AgentChatMessage("assistant", answer));
                    boundary?.Invoke("final", CtxChars(agent));
                    return answer;
                }

                if (decision.ToolCall is not { } call)
                    throw new InvalidOperationException("Native tool dispatch returned neither a tool call nor a final answer.");

                var resultText = InvokeToolCall(agent, call, tools, dispatcher, agent.ClientToolsEnabled, cancellationToken);
                agent.History.Add(new AgentChatMessage("tool", $"[{call.Name}] {resultText}"));
                AgentEventLog.Instance.Record(AgentEventKind.ToolCalled, agent.Name,
                    $"tool={call.Name} args={call.ArgumentsJson} result={resultText}");
                native.AppendMessage(sid, "tool", resultText);
                boundary?.Invoke("toolResult", CtxChars(agent));
            }

            throw new InvalidOperationException($"Native tool-calling loop exceeded {maxRounds} rounds.");
        }
        finally
        {
            if (ownsSession) native.DestroySession(sid);
        }
    }

    /// <summary>
    /// Contesto utente per la PROSSIMA chiamata. OPT-IN al Context Engine (P2, asse B): se l'agente
    /// ha una configurazione (`AI CONTEXT SET/V IEW …`), il contesto è la **view curata** normalizzata
    /// da budget+policy (`AiContextEngine.BuildView`) — GVS5H: CONTEXT = VIEW curata sullo STATE, non
    /// chat-history. Senza configurazione resta il **transcript legacy** (comportamento invariato).
    /// La policy `error` propaga l'eccezione (overflow esplicito, mai taglio silenzioso).
    /// </summary>
    private static string BuildUserContext(AgentRecord agent)
    {
        var legacy = BuildTranscript(agent.History);
        if (!Basic.AI.Context.AiContextEngine.IsConfigured(agent.Name)) return legacy;
        var view = Basic.AI.Context.AiContextEngine.BuildView(agent.Name, legacy);
        return view.Sections.Count > 0 ? string.Join("\n", view.Sections) : legacy;
    }

    private static long CtxChars(AgentRecord agent) => BuildUserContext(agent).Length;

    /// <summary>Tool dell'agente = registry LOCALE (i tool dei plugin abilitati, es. il browser,
    /// disponibili a TUTTI gli agenti locali SENZA binding esplicito) + binding esplicito
    /// (<c>AI AGENT REGISTER TOOL</c>, deduplicato) + client tools dei server MCP abilitati
    /// (auto-discovery, <c>server.tool</c>) SOLO se l'agente le ha abilitate
    /// (<c>AI AGENT SET nome, clienttools, ON</c>).</summary>
    private static List<McpToolDefinition> ResolveAgentTools(AgentRecord agent, string prompt)
    {
        var tools = new List<McpToolDefinition>();
        foreach (var t in McpToolRegistry.Instance.List())
        {
            if (t is not null && !tools.Any(x => string.Equals(x.Name, t.Name, StringComparison.OrdinalIgnoreCase)))
                tools.Add(t);
        }
        foreach (var n in agent.ToolNames)
        {
            var t = McpToolRegistry.Instance.Get(n);
            if (t is not null && !tools.Any(x => string.Equals(x.Name, t.Name, StringComparison.OrdinalIgnoreCase)))
                tools.Add(t);
        }
        if (agent.ClientToolsEnabled)
            tools.AddRange(Basic.Mcp.McpMarketplace.GetClientToolDefinitions());

        // Connettori F1: tutti i tool dei connettori abilitati sono auto-disponibili a TUTTI gli agenti
        // (bridge ConnectorRegistry → McpToolRegistry). Assicura la registrazione lazy per i connettori
        // già in config/connectors.json dopo un restart (dove _discovered è vuoto).
        foreach (var conn in Basic.Connectors.ConnectorRegistry.Instance.ListEnabled())
        {
            var ctools = Basic.Connectors.ConnectorRegistry.Instance.GetTools(conn.Name);
            if (ctools is null) continue;
            foreach (var ct in ctools)
            {
                var qname = $"{conn.Name}.{ct.Name}";
                var existing = McpToolRegistry.Instance.Get(qname);
                if (existing is null)
                {
                    var mcpDef = new McpToolDefinition { Name = qname, Description = ct.Description, QualifiedProcedureName = $"CONNECTOR:{qname}", InputSchema = new System.Text.Json.Nodes.JsonObject() };
                    McpToolRegistry.Instance.Register(mcpDef);
                    existing = mcpDef;
                }
                if (!tools.Any(x => string.Equals(x.Name, existing.Name, StringComparison.OrdinalIgnoreCase)))
                    tools.Add(existing);
            }
        }

        // ACQUISIZIONE AUTOMATICA DELLA SKILL (trigger nel prompt, per-ask, senza gonfiare
        // il contesto iniziale): i plugin NOTI (PluginIndex) che NON hanno tool abilitati nel
        // registry entrano in questo turno SOLO se il prompt matcha il loro trigger (es.
        // "naviga"/"apri pagina"/"browser"). Niente registrazione permanente: il contesto di
        // default resta minimo, i tool del plugin arrivano solo quando il task li richiede.
        var haveNames = new HashSet<string>(tools.Select(t => t.Name), StringComparer.OrdinalIgnoreCase);
        foreach (var plugin in Basic.AI.PluginIndex.MatchByTrigger(prompt ?? string.Empty))
            foreach (var t in Basic.AI.PluginRegistry.ToolsFor(plugin.Name))
                if (t is not null && haveNames.Add(t.Name))
                    tools.Add(t);

        // N1 Needle3 (tool router a retrieval, opt-in per-agente spento di default):
        // con catalogo grande, al prompt vanno solo i top-k rilevanti invece di tutto il
        // catalogo (si accende con AI AGENT SET nome, toolretrieval, ON). Se il recupero
        // non trova candidati, si tiene la lista completa (comportamento attuale, nessuna
        // regressione); il ramo escalate vive nel pipeline offline (ToolRetrieval.SelectTools).
        if (agent.ToolRetrievalEnabled && tools.Count >= ToolRetrieval.MinCatalogSize)
        {
            var sel = ToolRetrieval.SelectTools(prompt ?? string.Empty, tools);
            if (!sel.Escalated && sel.Tools.Count > 0)
                return new List<McpToolDefinition>(sel.Tools);
        }
        return tools;
    }

    /// <summary>Invoca un tool: se il nome e' <c>server.tool</c> con server abilitato nella
    /// lista client -> McpClient.CallTool (auto-discovery); altrimenti -> dispatcher del
    /// registry locale. Gli errori dei tool client diventano un risultato ERROR, non un throw.</summary>
    private static string InvokeToolCall(AgentRecord agent, ToolCallRequest call, IReadOnlyList<McpToolDefinition> tools, IMcpToolDispatcher dispatcher, bool allowClientTools, CancellationToken cancellationToken)
    {
        // Connettori F1: se il tool è connector.tool (es. gmail.search) con connettore abilitato, dispatch diretto a ConnectorRuntime
        var cdot = call.Name.IndexOf('.');
        if (cdot > 0 && Basic.Connectors.ConnectorRegistry.Instance.Get(call.Name[..cdot]) is { Enabled: true } conn)
        {
            var ctool = call.Name[(cdot + 1)..];
            try { return Basic.Connectors.ConnectorRuntimeHelper.CallForAgent(conn.Name, ctool, call.ArgumentsJson, agent); }
            catch (Exception ex) { return $"ERROR: {ex.Message}"; }
        }
        var dot = call.Name.IndexOf('.');
        if (dot > 0 && McpConfig.Get(call.Name[..dot]) is { Enabled: true } server)
        {
            if (!allowClientTools)
                return $"ERROR: client tools disabilitati per questo agente (AI AGENT SET nome, clienttools, ON).";
            var toolName = call.Name[(dot + 1)..];
            try { return Basic.Mcp.McpClient.CallTool(server, toolName, call.ArgumentsJson); }
            catch (Exception ex) { return $"ERROR: {ex.Message}"; }
        }

        // I tool acquisiti per-ask dal trigger NON sono registrati nel registry (per non
        // gonfiare il contesto di default): il lookup guarda PRIMA la lista dei tool risolti
        // per questo turno, poi il registry locale.
        var tool = tools.FirstOrDefault(t => string.Equals(t.Name, call.Name, StringComparison.OrdinalIgnoreCase))
                   ?? McpToolRegistry.Instance.Get(call.Name)
                   ?? throw new InvalidOperationException($"Agent referenced unknown tool '{call.Name}'.");

        // PERMESSI (Sandbox/Supervised/YOLO — vedi AgentPermissionService): l'unico gate per
        // le invocazioni locali dell'agente. Deny → errore esplicito; Approve → attesa
        // human-in-the-loop (fail-closed). YOLO/Safe → passa.
        var verdict = Basic.Agents.AgentPermissionService.Instance.Evaluate(agent, tool.Name);
        if (verdict == Basic.Agents.AgentVerdict.Deny)
        {
            AgentEventLog.Instance.Record(AgentEventKind.PermissionDenied, agent.Name,
                $"tool={tool.Name} (sandbox)");
            return $"ERROR: permesso negato (sandbox) per il tool '{tool.Name}'.";
        }
        if (verdict == Basic.Agents.AgentVerdict.Approve)
        {
            AgentEventLog.Instance.Record(AgentEventKind.PermissionRequested, agent.Name, $"tool={tool.Name}");
            var approved = Basic.Agents.AgentPermissionService.Instance.RequestApprovalAndWait(agent, tool.Name);
            AgentEventLog.Instance.Record(approved ? AgentEventKind.PermissionApproved : AgentEventKind.PermissionDenied,
                agent.Name, $"tool={tool.Name}");
            if (!approved)
                return $"ERROR: permesso negato (approvazione rifiutata o timeout) per il tool '{tool.Name}'.";
        }

        var result = dispatcher.DispatchAsync(tool.QualifiedProcedureName, call.ArgumentsJson, cancellationToken)
            .GetAwaiter().GetResult();
        return result.IsError ? $"ERROR: {result.TextContent}" : result.TextContent;
    }

    private static string BuildSystemPrompt(string basePrompt, IReadOnlyList<McpToolDefinition> tools)
    {
        if (tools.Count == 0)
            return basePrompt;

        var sb = new System.Text.StringBuilder(basePrompt);
        sb.AppendLine();
        sb.AppendLine("## Available tools (respond with JSON {\"tool_calls\":[...]} to invoke):");
        foreach (var tool in tools)
            sb.AppendLine($"- {tool.Name}: {tool.Description}");
        return sb.ToString();
    }

    private static string BuildTranscript(IReadOnlyList<AgentChatMessage> history)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var message in history)
            sb.AppendLine($"{message.Role}: {message.Content}");
        return sb.ToString().TrimEnd();
    }

    /// <summary>
    /// Checks self-consistency of the given answer using K-path sampling (AI.CONSISTENCY).
    /// Returns true if the answer is consistent (risk below threshold) or if consistency check is disabled.
    /// If inconsistent (high risk or collapse_suspected), attempts escalation via RAG grounding.
    /// </summary>
    private static bool CheckConsistencyAndEscalate(
        AgentRecord agent,
        ILlmCompleter completer,
        IMcpToolDispatcher dispatcher,
        string question,
        string answer,
        int maxRounds,
        CancellationToken cancellationToken,
        Action<string, long>? boundary,
        ref int rounds,
        ref string lastAnswer)
    {
        if (!agent.CheckConsistency) return true; // disabled

        var threshold = agent.ConsistencyRiskThreshold > 0 ? agent.ConsistencyRiskThreshold : 0.7;
        var k = agent.ConsistencySampleCount > 0 ? agent.ConsistencySampleCount : 5;
        var temp = agent.ConsistencyTemperature > 0 ? agent.ConsistencyTemperature : 0.7f;
        var maxTokens = agent.ConsistencyMaxTokens > 0 ? agent.ConsistencyMaxTokens : 1024;

        // Get active completer (chat provider)
        var activeCompleter = Basic.AI.Rag.LlmCompleterResolver.Default;
        if (activeCompleter == null) return true; // no provider, skip check

        // Sample K responses using AskMulti
        var sysPrompt = "You are a helpful assistant. Answer concisely.";
        var responses = activeCompleter.AskMulti(sysPrompt, question, k, temp, maxTokens);
        if (responses == null || responses.Length == 0) return true;

        // Compute consistency risk
        var risk = SemanticEntropy.ConsistencyRisk(responses);
        var detail = SemanticEntropy.LastDetailJson();
        var collapseWarning = detail.Contains("\"collapse_warning\":true");

        boundary?.Invoke("consistencyCheck", (long)(risk * 1000));

        if (risk < threshold && !detail.Contains("\"collapse_warning\":true"))
        {
            // Consistent enough
            return true;
        }

        // High risk or collapse warning -> escalate
        boundary?.Invoke("consistencyEscalate", (long)(risk * 1000));

        // Attempt RAG grounding escalation
        var groundedAnswer = TryGroundWithRag(agent, completer, dispatcher, cancellationToken);
        if (!string.IsNullOrEmpty(groundedAnswer) && !groundedAnswer.Equals(answer, StringComparison.Ordinal))
        {
            // Use grounded answer and re-check
            boundary?.Invoke("consistencyEscalated", 1);
            // The caller should use the grounded answer instead
            return false; // signal to use grounded answer
        }

        // If no RAG available, log warning but proceed
        boundary?.Invoke("consistencyWarning", (long)(risk * 1000));
        return true; // proceed with warning
    }

    /// <summary>
    /// Attempts to ground the answer using RAG (AI.VECTOR.ASK via dispatcher).
    /// Returns grounded answer or empty string if RAG not available.
    /// </summary>
    private static string TryGroundWithRag(
        AgentRecord agent,
        ILlmCompleter completer,
        IMcpToolDispatcher dispatcher,
        CancellationToken cancellationToken)
    {
        try
        {
            // F2 — livello AGENT dello scope cervelli: se l'agente DICHIARA i brain (`AI AGENT SET … brains`)
            // il grounding usa il set effettivo dello scope via MultiBrainSearch (fusione RRF + provenance),
            // senza richiedere il vecchio `VectorDbAlias`/`VectorCollection` (che nessuna API valorizzava:
            // questo ramo era codice morto). Se l'agente NON dichiara nulla → percorso precedente invariato.
            var scopeBrains = AgentBrains.ResolveForTool(agent, null);
            if (scopeBrains is { Length: > 0 })
            {
                var bundle = Basic.AI.Rag.MultiBrainSearch.RagContextMulti(agent.LastUserPrompt, 5, brains: scopeBrains);
                if (string.IsNullOrEmpty(bundle.Context)) return string.Empty;
                var scopeGroundedPrompt = $"Context:\n{bundle.Context}\n\nQuestion: {agent.LastUserPrompt}\nAnswer using only the provided context.";
                return completer.Complete("You are a helpful assistant. Answer using only the provided context.", scopeGroundedPrompt);
            }

            // Percorso legacy: vector DB esplicito dell'agente.
            // Check if agent has vector DB configured
            if (string.IsNullOrEmpty(agent.VectorDbAlias) || string.IsNullOrEmpty(agent.VectorCollection))
                return string.Empty;

            // Search vector DB for relevant context
            var searchResult = dispatcher.DispatchAsync("AIVECTOR.SEARCH",
                System.Text.Json.JsonSerializer.Serialize(new
                {
                    db = agent.VectorDbAlias,
                    coll = agent.VectorCollection,
                    query = agent.LastUserPrompt,
                    topK = 5
                }), cancellationToken).GetAwaiter().GetResult();

            if (searchResult.IsError || string.IsNullOrEmpty(searchResult.TextContent))
                return string.Empty;

            // Use RAG context for completion
            var ragContext = dispatcher.DispatchAsync("AIVECTOR.RAGCONTEXT",
                System.Text.Json.JsonSerializer.Serialize(new
                {
                    db = agent.VectorDbAlias,
                    coll = agent.VectorCollection,
                    query = agent.LastUserPrompt,
                    topK = 5
                }), cancellationToken).GetAwaiter().GetResult();

            if (ragContext.IsError || string.IsNullOrEmpty(ragContext.TextContent))
                return string.Empty;

            // Ask completer with grounded context
            var groundedPrompt = $"Context:\n{ragContext.TextContent}\n\nQuestion: {agent.LastUserPrompt}\nAnswer using only the provided context.";
            return completer.Complete("You are a helpful assistant. Answer using only the provided context.", groundedPrompt);
        }
        catch
        {
            return string.Empty;
        }
    }
}
