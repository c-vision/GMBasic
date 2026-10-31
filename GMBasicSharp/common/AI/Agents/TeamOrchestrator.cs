// FILE: Basic.Agents.TeamOrchestrator
using System.Collections.Concurrent;
using System.Text.Json.Nodes;
using Basic.AI.Rag;
using Basic.Agents.Recursive;
using Basic.Providers;

namespace Basic.Agents;

/// <summary>Esegue un task su un team secondo la strategia configurata.
/// Usa AgentServer.Ask per ogni membro (con tool-loop se il membro ha tool registrati).</summary>
public static class TeamOrchestrator
{
    /// <summary>Streaming per-turn (item 1): invocato quando un membro completa il suo turno
    /// (risposta o errore). Un consumer (host/GPM INTO Sub) può sottoscriversi per consumare i
    /// risultati in streaming invece di aspettare la sintesi finale. Mai null-safe richiesta.</summary>
    public static event System.Action<string, string, string>? MemberCompleted;

    private static void EmitMember(string teamName, string member, string text)
    {
        try { MemberCompleted?.Invoke(teamName, member, text); } catch { /* il consumer non deve far fallire il team */ }
    }


    /// <summary>Esegue il task sul team e ritorna l'ultima risposta (o la sintesi coordinator).</summary>
    public static string Run(TeamRecord team, string task)
    {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        // A.1: metrica token del CONTESTO (ciò che ogni agente LEGGE, non l'output) — il segnale
        // del token-saving latente: in text mode il contesto cresce (answer piene), in embed/native
        // resta bounded (fingerprint).
        team.Metadata["_ctxTokens"] = "0";
        // R1: se RecursionRounds>1, delega al loop ricorsivo (text baseline, closure A_N->A_1)
        string result = team.RecursionRounds > 1
            ? RunRecursive(team, task)
            : RunOnce(team, task);
        sw.Stop();
        // R6: metriche leggere (round, ms, ask count) — esposte via team.Metadata.
        team.Metadata["lastRunMs"] = sw.ElapsedMilliseconds.ToString(System.Globalization.CultureInfo.InvariantCulture);
        team.Metadata["lastRunRounds"] = team.RecursionRounds.ToString(System.Globalization.CultureInfo.InvariantCulture);
        return result;
    }

    /// <summary>Accumula i token di contesto letti dagli agenti (chars/4) per la metrica del round.</summary>
    private static void NoteContextTokens(TeamRecord team, string context)
    {
        if (string.IsNullOrEmpty(context)) return;
        var acc = team.Metadata.TryGetValue("_ctxTokens", out var v) && int.TryParse(v, out var n) ? n : 0;
        team.Metadata["_ctxTokens"] = (acc + context.Length / 4).ToString(System.Globalization.CultureInfo.InvariantCulture);
    }

    /// <summary>Token di contesto accumulati nel round corrente.</summary>
    private static int CtxTokens(TeamRecord team)
        => team.Metadata.TryGetValue("_ctxTokens", out var v) && int.TryParse(v, out var n) ? n : 0;

    private static string RunOnce(TeamRecord team, string task)
    {
        var server = AgentServer.Instance;
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var asksBefore = team.Conversation.Count;
        string result = "";
        try
        {
            result = team.Strategy switch
            {
                TeamStrategy.Sequential => RunSequential(team, task, server),
                TeamStrategy.RoundRobin => RunRoundRobin(team, task, server),
                TeamStrategy.Coordinator => RunCoordinator(team, task, server),
                TeamStrategy.Mixture => RunMixture(team, task, server),
                TeamStrategy.Distillation => RunDistillation(team, task, server),
                TeamStrategy.Deliberation => RunDeliberation(team, task, server),
                TeamStrategy.Dag => RunDag(team, task, server),
                _ => RunSequential(team, task, server)
            };
        }
        finally
        {
            sw.Stop();
            // R6: singolo round (r=1) — stessa metrica dei round ricorsivi (token = contesto letto).
            Basic.Agents.Recursive.RecursiveTrainer.RecordRun(team.Name, 1, sw.ElapsedMilliseconds,
                CtxTokens(team), team.Conversation.Count - asksBefore);
        }
        return result;
    }

    /// <summary>R1: loop ricorsivo testuale — r round, closure A_N->A_1, solo ultimo round è finale.</summary>
    public static string RunRecursive(TeamRecord team, string task)
    {
        var server = AgentServer.Instance;
        var r = Math.Clamp(team.RecursionRounds, 1, 8);
        var ctx = task;
        for (var round = 0; round < r; round++)
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            var asksBefore = team.Conversation.Count;
            ctx = team.Strategy switch
            {
                TeamStrategy.Sequential => RunSequential(team, ctx, server),
                TeamStrategy.RoundRobin => RunRoundRobinSingle(team, ctx, server),
                TeamStrategy.Coordinator => RunCoordinator(team, ctx, server),
                TeamStrategy.Mixture => RunMixture(team, ctx, server),
                TeamStrategy.Distillation => RunDistillation(team, ctx, server),
                TeamStrategy.Deliberation => RunDeliberation(team, ctx, server),
                TeamStrategy.Dag => RunDag(team, ctx, server),
                _ => RunSequential(team, ctx, server)
            };
            sw.Stop();
            // R6: metrica per round (Fig.5-6) — token = CONTESTO letto dagli agenti (chars/4),
            // ask = messaggi aggiunti.
            Basic.Agents.Recursive.RecursiveTrainer.RecordRun(team.Name, round + 1, sw.ElapsedMilliseconds,
                CtxTokens(team), team.Conversation.Count - asksBefore);
        }
        return ctx;
    }

    /// <summary>R3: Mixture — gli esperti ragionano in parallelo sul task, lo summarizer
    /// (membro Coordinator oppure l'ultimo) aggrega i risultati. Pattern paper §5.3.</summary>
    private static string RunMixture(TeamRecord team, string task, AgentServer server)
    {
        var summarizer = team.Members.FirstOrDefault(m => m.Role == TeamRole.Coordinator)?.AgentName
            ?? team.Members.LastOrDefault()?.AgentName;
        var experts = team.Members.Where(m => !m.AgentName.Equals(summarizer, StringComparison.OrdinalIgnoreCase)).ToList();
        if (summarizer is null || experts.Count == 0)
            return RunSequential(team, task, server);

        var results = new List<string>();
        foreach (var expert in experts)
        {
            var prompt = BuildPrompt(team, expert, task, "");
            var answer = server.Ask(expert.AgentName, prompt);
            team.Conversation.Add(new TeamMessage(expert.AgentName, answer, DateTimeOffset.UtcNow));
            results.Add($"{expert.AgentName}: {answer}");
        }

        var summary = string.Join("\n", results);
        var synth = server.Ask(summarizer,
            $"Sei l'aggregatore di un team di esperti. Sintetizza i seguenti risultati di {experts.Count} esperti per il task '{task}':\n{summary}");
        team.Conversation.Add(new TeamMessage(summarizer, synth, DateTimeOffset.UtcNow));
        return synth;
    }

    /// <summary>R3: Distillation — l'Expert (primo membro) produce la risposta, il Learner
    /// (ultimo membro) la raffina. Pattern paper §5.3 (expert→learner).</summary>
    private static string RunDistillation(TeamRecord team, string task, AgentServer server)
    {
        if (team.Members.Count < 2)
            return RunSequential(team, task, server);
        var expert = team.Members[0];
        var learner = team.Members[^1];

        var expertAnswer = server.Ask(expert.AgentName, BuildPrompt(team, expert, task, ""));
        team.Conversation.Add(new TeamMessage(expert.AgentName, expertAnswer, DateTimeOffset.UtcNow));

        var learnerPrompt =
            $"{learner.RolePrompt ?? "Sei il learner del team: raffina e semplifica la risposta dell'expert."}\n" +
            $"Team: {team.Name}\nTask: {task}\nRisposta dell'expert: {expertAnswer}\nMigliora la risposta conservando i fatti.";
        var learnerAnswer = server.Ask(learner.AgentName, learnerPrompt);
        team.Conversation.Add(new TeamMessage(learner.AgentName, learnerAnswer, DateTimeOffset.UtcNow));
        return learnerAnswer;
    }

    /// <summary>R3: Deliberation — il Reflector (primo membro) critica/riflette, il ToolCaller
    /// (secondo membro) agisce con i tool registrati. Loop per RecursionRounds (default 2).</summary>
    private static string RunDeliberation(TeamRecord team, string task, AgentServer server)
    {
        if (team.Members.Count < 2)
            return RunSequential(team, task, server);
        var reflector = team.Members[0];
        var toolCaller = team.Members[1];
        var rounds = Math.Clamp(team.RecursionRounds, 1, 4);
        var ctx = task;

        for (var round = 0; round < rounds; round++)
        {
            var critique = server.Ask(reflector.AgentName,
                BuildPrompt(team, reflector, task,
                    $"{ctx}\nValuta criticamente la soluzione corrente e indica gli errori (round {round + 1})."));
            team.Conversation.Add(new TeamMessage(reflector.AgentName, critique, DateTimeOffset.UtcNow));

            var action = server.Ask(toolCaller.AgentName,
                BuildPrompt(team, toolCaller, task, $"Critica del reflector:\n{critique}\nAgisci: correggi/risolvi."));
            team.Conversation.Add(new TeamMessage(toolCaller.AgentName, action, DateTimeOffset.UtcNow));
            ctx = action;
        }
        return ctx;
    }

    /// <summary>Ask a un membro con gestione errori (item 2): se il membro fallisce
    /// (provider non configurato / timeout / eccezione) NON abortisce il team: logga un messaggio
    /// di errore nella conversazione, raccoglie in team.Metadata["memberErrors"] e ritorna un
    /// marcatore — gli altri membri continuano. Fallback testuale dell'ultimo answer per i pattern
    /// che ne dipendono.</summary>
    private static string AskMember(TeamRecord team, TeamMember member, string prompt, AgentServer server)
    {
        try
        {
            var answer = server.Ask(member.AgentName, prompt);
            team.Conversation.Add(new TeamMessage(member.AgentName, answer, DateTimeOffset.UtcNow));
            EmitMember(team.Name, member.AgentName, answer);
            return answer;
        }
        catch (Exception ex)
        {
            var msg = $"[membro {member.AgentName} errore: {(ex is InvalidOperationException or ArgumentException ? ex.Message : ex.GetType().Name)}]";
            team.Conversation.Add(new TeamMessage(member.AgentName, msg, DateTimeOffset.UtcNow));
            EmitMember(team.Name, member.AgentName, msg);
            var acc = team.Metadata.TryGetValue("memberErrors", out var v) && int.TryParse(v, out var n) ? n : 0;
            team.Metadata["memberErrors"] = (acc + 1).ToString(System.Globalization.CultureInfo.InvariantCulture);
            return msg;
        }
    }

    private static string RunRoundRobinSingle(TeamRecord team, string task, AgentServer server)
    {
        var context = task;
        string lastAnswer = task;
        foreach (var member in team.Members)
        {
            NoteContextTokens(team, context);
            var prompt = BuildPrompt(team, member, task, context);
            var answer = AskMember(team, member, prompt, server);
            lastAnswer = answer;
            context = WithLatent(team, member, answer);
        }
        return lastAnswer;
    }

    /// <summary>9.4 (AGoT): DAG ricorsivo — decomponi il task in sotto-problemi (DMax/LMax/NMax),
    /// risolvi ogni nodo con un membro (round-robin) aggregando i genitori, sintesi finale del
    /// coordinatore. I nodi con complessità &gt; soglia vengono ESPANSI ricorsivamente (AGoT:
    /// complexity-check + expand), con profondità limitata da DMax.</summary>
    private static string RunDag(TeamRecord team, string task, AgentServer server)
    {
        if (team.Members.Count == 0) return task;
        var decomposer = team.Coordinator ?? team.Members.FirstOrDefault(m => m.Role == TeamRole.Coordinator)?.AgentName
            ?? team.Members[0].AgentName;
        var workers = team.Members.Where(m => !m.AgentName.Equals(decomposer, StringComparison.OrdinalIgnoreCase)).ToList();
        if (workers.Count == 0) workers = team.Members.ToList();
        return SolveDagNode(team, task, decomposer, workers, server, depth: 0);
    }

    private static string SolveDagNode(TeamRecord team, string task, string decomposer, List<TeamMember> workers,
        AgentServer server, int depth)
    {
        // decompose
        var decompPrompt = $"Decomponi il seguente task in al più {Math.Clamp(team.LMax, 1, Math.Max(1, team.NMax))} sotto-problemi indipendenti (uno per riga):\n{task}";
        var decomposition = server.Ask(decomposer, decompPrompt);
        team.Conversation.Add(new TeamMessage(decomposer, decomposition, DateTimeOffset.UtcNow));
        var subProblems = decomposition.Split('\n', StringSplitOptions.RemoveEmptyEntries)
            .Select(l => l.Trim(' ', '-', '*', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.', ')', ':'))
            .Where(l => l.Length > 3 && !l.StartsWith("Task", StringComparison.OrdinalIgnoreCase))
            .Take(Math.Max(1, team.LMax))
            .ToList();
        if (subProblems.Count == 0) subProblems.Add(task);

        // risolvi ogni nodo: espansione ricorsiva se complesso e sotto DMax, altrimenti ask diretto
        var nodeResults = new List<string>();
        for (var i = 0; i < subProblems.Count; i++)
        {
            var worker = workers[i % workers.Count];
            var parentCtx = i > 0 ? string.Join("\n", nodeResults) : task;
            string ans;
            if (depth < team.DMax && Complexity(team, subProblems[i]) > team.ComplexityThreshold)
                ans = SolveDagNode(team, subProblems[i], decomposer, workers, server, depth + 1);
            else
            {
                ans = server.Ask(worker.AgentName, BuildPrompt(team, worker, subProblems[i], parentCtx));
                team.Conversation.Add(new TeamMessage(worker.AgentName, ans, DateTimeOffset.UtcNow));
            }
            nodeResults.Add($"{subProblems[i]} → {ans}");
        }

        // aggregate (GraphOfThoughts): sintesi del coordinatore/decomposer
        var synth = server.Ask(decomposer,
            $"Sintetizza una risposta unica e corretta al task '{task}' dai seguenti risultati dei sotto-problemi:\n{string.Join("\n", nodeResults)}");
        team.Conversation.Add(new TeamMessage(decomposer, synth, DateTimeOffset.UtcNow));
        return synth;
    }

    /// <summary>AGoT complexity-check proxy: complessità = lunghezza normalizzata del nodo
    /// (caratteri/512) + numero di operatori numerici presenti — soglia configurabile.</summary>
    private static double Complexity(TeamRecord team, string node)
    {
        if (string.IsNullOrEmpty(node)) return 0;
        var chars = node.Length / 512.0;
        var ops = System.Text.RegularExpressions.Regex.Matches(node, @"[+\-*/]|=|^|√|×|÷").Count / 3.0;
        return Math.Min(2.0, chars + ops);
    }

    private static string RunSequential(TeamRecord team, string task, AgentServer server)
    {
        var context = task;
        string lastAnswer = task;
        foreach (var member in team.Members)
        {
            NoteContextTokens(team, context);
            var prompt = BuildPrompt(team, member, task, context);
            var answer = AskMember(team, member, prompt, server);
            lastAnswer = answer;
            context = WithLatent(team, member, answer);
        }
        return lastAnswer;
    }

    private static string RunRoundRobin(TeamRecord team, string task, AgentServer server)
    {
        var context = task;
        // 2 giri: ogni membro risponde al contesto aggiornato
        for (var round = 0; round < 2; round++)
        {
            foreach (var member in team.Members)
            {
                var prompt = BuildPrompt(team, member, task, context);
                var answer = AskMember(team, member, prompt, server);
                context = answer;
            }
        }
        return context;
    }

    private static string RunCoordinator(TeamRecord team, string task, AgentServer server)
    {
        var coordinator = team.Coordinator ?? team.Members.FirstOrDefault(m => m.Role == TeamRole.Coordinator)?.AgentName;
        var workers = team.Members.Where(m => !m.AgentName.Equals(coordinator, StringComparison.OrdinalIgnoreCase)).ToList();
        if (coordinator is null || workers.Count == 0)
            return RunSequential(team, task, server);

        var results = new List<string>();
        foreach (var worker in workers)
        {
            var prompt = BuildPrompt(team, worker, task, $"Task: {task}");
            var answer = AskMember(team, new TeamMember { AgentName = worker.AgentName }, prompt, server);
            results.Add($"{worker.AgentName}: {answer}");
        }

        // Il coordinator sintetizza i risultati
        var summary = string.Join("\n", results);
        var synth = server.Ask(coordinator, $"Sintetizza i seguenti risultati di {workers.Count} agenti per il task '{task}':\n{summary}");
        team.Conversation.Add(new TeamMessage(coordinator, synth, DateTimeOffset.UtcNow));
        return synth;
    }

    private static string BuildPrompt(TeamRecord team, TeamMember member, string task, string context)
    {
        var role = member.RolePrompt ?? member.Role switch
        {
            TeamRole.Coordinator => "Sei il coordinatore del team.",
            TeamRole.Critic => "Sei un critico: analizza e migliora.",
            TeamRole.Observer => "Sei un osservatore: riporta solo i fatti.",
            _ => "Sei un membro del team."
        };
        return $"{role}\nTeam: {team.Name}\nTask: {task}\nContesto: {context}";
    }

    // ── R2: modalità latent "embed" (proxy) ───────────────────────────────────
    // In embed mode, dopo ogni risposta di un membro si computa un embedding
    // (proxy deterministico per ora; AI.EMBED provider in R5 nativo), lo si proietta
    // con l'outer RecursiveLink (se OuterLinkConfig presente) e si appende una
    // fingerprint testuale al contesto per l'agente successivo. Il path nativo
    // (last_hidden) arriva in R5 — qui resta tutto deterministico e testabile.

    private static readonly ConcurrentDictionary<string, (int Handle, int Dim)> _outerLinks = new(StringComparer.OrdinalIgnoreCase);

    private static string WithLatent(TeamRecord team, TeamMember member, string answer)
    {
        if (string.Equals(team.LatentMode, "text", StringComparison.OrdinalIgnoreCase)) return answer;
        // 9.1 (LatentMAS): in modalità "kv" il contesto latente è la WORKING MEMORY del team
        // (transfer K→V tra agenti) — se già presente, lo si riusa senza ricalcolo.
        if (string.Equals(team.LatentMode, "kv", StringComparison.OrdinalIgnoreCase))
        {
            if (Basic.Agents.Recursive.LatentWorkingMemory.Get(team.Name) is { Length: > 0 } cached)
                return cached;
        }
        var h = EmbedFor(team, member, answer);
        // R_in: proiezione inner (residual) sull'hidden prima dell'outer — loop auto-regressivo.
        if (GetInnerLink(team, h.Length) is { } inner)
            h = RecursiveLink.Forward(inner, h) ?? h;
        var hLat = h;
        if (GetOuterLink(team, h.Length) is { } outer)
            hLat = RecursiveLink.Forward(outer, h) ?? h;
        var fp = RecursiveLink.Fingerprint(hLat);
        team.Metadata["lastLatentFp"] = fp;
        // A.1 (R5.4): pass AUTO-REGRESSIVO — al membro successivo va SOLO il latente compatto
        // (fingerprint), NON l'answer testuale: il contesto tra agenti non ri-tokenizza la storia.
        // La risposta finale del team resta l'ultimo answer (RunSequential/RunRoundRobinSingle lo
        // ritornano esplicitamente). Il tag è bounded (<200 char) → token saving misurabile.
        var latent = "\n⟨latent⟩" + fp + "⟨/latent⟩";
        if (string.Equals(team.LatentMode, "kv", StringComparison.OrdinalIgnoreCase))
            Basic.Agents.Recursive.LatentWorkingMemory.Put(team.Name, latent);
        return latent;
    }

    /// <summary>R5: in modalità "native" usa prima l'hidden REALE della sessione del membro
    /// (ILatentCompleter.LastHidden), poi EmbedText del testo; se il provider non lo supporta
    /// degrada all'embedding proxy deterministico. Mai errore.</summary>
    private static double[] EmbedFor(TeamRecord team, TeamMember member, string text)
    {
        if (string.Equals(team.LatentMode, "native", StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                if (ProviderServer.Instance.GetActiveCompletion() is ILatentCompleter lc)
                {
                    var rec = AgentServer.Instance.List()
                        .FirstOrDefault(a => a.Name.Equals(member.AgentName, StringComparison.OrdinalIgnoreCase));
                    if (rec?.SessionId is int sid)
                    {
                        var hidden = lc.LastHidden(sid);
                        if (hidden is { Length: > 0 }) return Array.ConvertAll(hidden, x => (double)x);
                    }
                    var f = lc.EmbedText(text);
                    if (f is { Length: > 0 }) return Array.ConvertAll(f, x => (double)x);
                }
            }
            catch { /* degrada al proxy */ }
        }
        return RecursiveLink.SimpleEmbed(text);
    }

    private static readonly ConcurrentDictionary<string, (int Handle, int Dim)> _innerLinks = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Inner link del team (da InnerLinkConfig, default hidden 128) ricreato se la
    /// dimensione dell'input cambia (SimpleEmbed 768 vs EmbDim nativo).</summary>
    private static int? GetInnerLink(TeamRecord team, int dim)
    {
        if (string.IsNullOrWhiteSpace(team.InnerLinkConfig)) return null;
        if (_innerLinks.TryGetValue(team.Id, out var e) && e.Dim == dim) return e.Handle;
        var hidden = 128;
        try
        {
            var cfg = JsonNode.Parse(team.InnerLinkConfig);
            if (cfg is JsonObject o && o["hidden"] is System.Text.Json.Nodes.JsonValue hv && hv.TryGetValue<int>(out var h) && h > 0)
                hidden = h;
        }
        catch { /* default */ }
        var handle = RecursiveLink.CreateInner(dim, hidden);
        _innerLinks[team.Id] = (handle, dim);
        return handle;
    }

    private static int? GetOuterLink(TeamRecord team, int dim)
    {
        if (string.IsNullOrWhiteSpace(team.OuterLinkConfig)) return null;
        if (_outerLinks.TryGetValue(team.Id, out var e) && e.Dim == dim) return e.Handle;
        var hidden = 128;
        try
        {
            var cfg = JsonNode.Parse(team.OuterLinkConfig);
            if (cfg is JsonObject o && o["hidden"] is System.Text.Json.Nodes.JsonValue hv && hv.TryGetValue<int>(out var h) && h > 0)
                hidden = h;
        }
        catch { /* default */ }
        var handle = RecursiveLink.CreateOuter(dim, dim, hidden);
        _outerLinks[team.Id] = (handle, dim);
        return handle;
    }

    /// <summary>L1 — Broadcast messaggio a tutti gli agenti del team via coda.</summary>
    public static int Broadcast(string teamName, string message)
    {
        var team = TeamServer.Instance.Get(teamName);
        if (team == null) return 0;
        var count = 0;
        foreach (var m in team.Members)
        {
            if (AgentQueueManager.Instance.Enqueue(m.AgentName, "broadcast", message, message)) count++;
            team.Conversation.Add(new TeamMessage(m.AgentName, message, DateTimeOffset.UtcNow));
        }
        return count;
    }

    /// <summary>L1 — Redefine team in-flight (sostituisce i membri con la nuova definizione CSV).</summary>
    public static int Redefine(string teamName, string definition)
    {
        var team = TeamServer.Instance.Get(teamName);
        if (team == null) return 0;
        // definition è CSV di nomi agente: "A1,A2,A3" o JSON like "A1,A2"
        var names = definition.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (names.Length == 0) return 0;
        team.Members.Clear();
        foreach (var n in names)
            team.Members.Add(new TeamMember { AgentName = n });
        return team.Members.Count;
    }

}
