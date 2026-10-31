// AiWorkerRuntime.cs — AI WORKER + workspace (P3 AI-Runtime consolidato, asse C).
// Worker con context isolation: workspace per-task su filesystem come memoria
// esterna (GVS5H), state save/load, contatori run-level (rounds/rescued/spend) e
// INVOCAZIONE PULITA (`AI WORKER "<TASK>"`): la vista della chiamata è SOLO la memoria
// esterna del run (task/plan/notes/solution + state del workspace), normalizzata dal
// Context Engine — MAI la chat history (C1/C2: "context isolation").
// Senza completer attivo l'invocazione NON è silenziosa: esito = errore esplicito
// (stessa sentinella di Enricher/BrainVault → gmb_llm_complete) + rescued.
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Basic.AI.Agents;

public static class AiWorkerRuntime
{
    /// <summary>Esito esplicito quando non c'è un provider LLM attivo: STESSA stringa di
    /// Enricher.NoCompleter / BrainVault / gmb_llm_complete (parità C#↔C, mai un esito finto).</summary>
    public const string NoCompleter = "Error: nessun provider LLM configurato (AI SET ACTIVE)";

    /// <summary>Codici di ritorno di <see cref="WorkerInvoke"/> (speculari a gmb_worker_invoke).
    /// 1 = invocazione eseguita · 0 = rifiutata dal budget (policy error) · -1 = nessun worker
    /// corrente · -2 = errore provider (sentinella NoCompleter).</summary>
    public const double InvokeOk = 1, InvokeBudgetRefused = 0, InvokeNoWorker = -1, InvokeProviderError = -2;
    /// <summary>Loop VERIFICATO: nessun round ha superato l'audit REQUIRE entro i round/budget.</summary>
    public const double InvokeAuditFailed = -3;
    /// <summary>MANAGER loop: cicli esauriti con task ancora `pending` (manager non convergente).</summary>
    public const double InvokeCyclesExhausted = -3;
    /// <summary>MANAGER loop: task `pending` ma con dipendenze non soddisfatte (ciclo/deadlock).</summary>
    public const double InvokeDeadlock = -4;
    /// <summary>MANAGER loop con scoping su piano: piano inesistente o non approvato.</summary>
    public const double InvokePlanNotApproved = -5;

    /// <summary>Round di default di `AI WORKER LOOP` quando non specificato.</summary>
    public const int DefaultLoopRounds = 3;

    private static readonly object Gate = new();
    private static readonly Dictionary<string, AiWorker> Workers = new();
    private static string _current = "";                       // ultimo worker creato (AI WORKER "…")
    private static long _rounds;    // run-level
    private static long _rescued;
    private static long _spendChars;

    public sealed class AiWorker
    {
        public string Name = "";
        public string Workspace = "";  // work/<name>/
        public long Budget = 0;
    }

    /// <summary>AI WORKER NEW name, {workspace, budget}: crea un worker + cartella workspace.</summary>
    public static double WorkerNew(string name, string opts)
    {
        if (string.IsNullOrWhiteSpace(name)) return -1;
        var budget = 0L; var workspace = "work/" + Sanitize(name);
        if (!string.IsNullOrWhiteSpace(opts))
        {
            var ws = ExtractJsonString(opts, "workspace");
            if (!string.IsNullOrEmpty(ws)) workspace = ws;
            budget = ExtractJsonLong(opts, "budget");
        }
        lock (Gate)
        {
            if (Workers.ContainsKey(name)) return -1;
            Workers[name] = new AiWorker { Name = name, Workspace = workspace, Budget = budget };
            _current = name;   // `AI WORKER "<TASK>"` agisce sul worker corrente (spec §2.1)
        }
        try { Directory.CreateDirectory(workspace); } catch { /* best effort */ }
        return 1;
    }

    /// <summary>AI WORKER name: esegue/round di lavoro sul worker (v1: tocca i contatori).</summary>
    public static double WorkerRun(string name)
    {
        lock (Gate)
        {
            if (!Workers.TryGetValue(name, out var w)) return -1;
            _rounds++;
            // rescue/spend aggiornati dal loop superiore via WorkComplete; qui contatore base.
        }
        return 1;
    }

    /// <summary>AI STATE SAVE key, value: salva stato nel worker/state del worker corrente.</summary>
    public static double StateSave(string worker, string key, string value)
    {
        lock (Gate)
        {
            if (!Workers.TryGetValue(worker, out var w)) return -1;
            return StateSaveToFile(Sanitize(key), value, w.Workspace);
        }
    }

    /// <summary>AI STATE LOAD key: legge stato dal worker corrente.</summary>
    public static string StateLoad(string worker, string key)
    {
        lock (Gate)
        {
            if (!Workers.TryGetValue(worker, out var w)) return "";
            return StateLoadFromFile(Sanitize(key), w.Workspace);
        }
    }

    /// <summary>Completer usato dall'invocazione pulita. Default null → sentinella NoCompleter
    /// (identico a LlmCompleterResolver.Default null). Iniettabile dal runtime e dai test.</summary>
    public static System.Func<string, string, string>? Completer { get; set; }

    /// <summary>Valutatore del REQUIRE dell'audit (loop verificato). Lo registra il backend
    /// (IlRuntime) perché l'evaluator delle espressioni BASIC vive lì; la firma è
    /// (require, result) → true se il REQUIRE è soddisfatto con RESULT$ = result.
    /// Se un loop chiede un REQUIRE senza valutatore registrato → errore ESPLICITO
    /// (mai un loop che si dichiara verificato senza aver valutato nulla).</summary>
    public static Func<string, string, bool>? RequireEvaluator { get; set; }

    /// <summary>Nome del worker corrente (l'ultimo creato). "" se nessuno.</summary>
    public static string CurrentWorker() { lock (Gate) return _current; }

    /// <summary>Chiave del Context Engine del worker ("worker:&lt;name&gt;"): permette
    /// `AI CONTEXT SET BUDGET worker:name, N` / policy per-worker.</summary>
    public static string ContextKey(string worker) => "worker:" + worker;

    /// <summary>
    /// Vista curata del run (C1/C2): SOLO memoria esterna dal workspace, nell'ordine
    /// task → plan.md → notes.md → solution → state/* (alfabetico). Nessuna chat history.
    /// Deterministico (stesso contenuto ⇒ stessa vista) e identico al runtime C.
    /// </summary>
    public static string BuildCuratedView(AiWorker w, string task)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append("TASK: ").Append(task ?? "").Append('\n');
        void Section(string label, string path, string fallbackName = "")
        {
            var text = ReadIfExists(path);
            if (string.IsNullOrEmpty(text)) return;
            sb.Append('\n').Append(label).Append(":\n").Append(text.TrimEnd('\n')).Append('\n');
        }
        Section("PLAN", Path.Combine(w.Workspace, "plan.md"));
        Section("NOTES", Path.Combine(w.Workspace, "notes.md"));
        Section("SOLUTION", Path.Combine(w.Workspace, "solution"));
        Section("BEST", Path.Combine(w.Workspace, "best"));
        try
        {
            var stateDir = Path.Combine(w.Workspace, "state");
            if (Directory.Exists(stateDir))
                foreach (var f in Directory.GetFiles(stateDir, "*.txt").OrderBy(x => x, System.StringComparer.Ordinal))
                    sb.Append("\nSTATE ").Append(Path.GetFileNameWithoutExtension(f)).Append(":\n")
                      .Append(File.ReadAllText(f).TrimEnd('\n')).Append('\n');
        }
        catch { /* best effort: la vista non deve fallire per un file illeggibile */ }
        return sb.ToString();
    }

    /// <summary>
    /// `AI WORKER "&lt;TASK&gt;"` — invocazione PULITA del worker corrente: vista curata sopra
    /// (mai la chat history), normalizzata dal Context Engine (budget del worker, policy);
    /// salvata in &lt;ws&gt;/context.md (memoria esterna ispezionabile); chiamata al completer;
    /// esito in &lt;ws&gt;/solution; contatori rounds/spend (+rescued sui fallimenti).
    /// </summary>
    public static double WorkerInvoke(string task)
    {
        AiWorker w;
        lock (Gate)
        {
            if (string.IsNullOrEmpty(_current) || !Workers.TryGetValue(_current, out var cw)) return InvokeNoWorker;
            w = cw;
        }
        var key = ContextKey(w.Name);
        var raw = BuildCuratedView(w, task);
        if (w.Budget > 0) Basic.AI.Context.AiContextEngine.SetBudget(key, w.Budget);
        var rc = InvokeWithView(w, key, raw);
        if (rc == InvokeOk) PromoteBest(w);
        return rc;
    }

    /// <summary>
    /// `AI WORKER LOOP "&lt;TASK&gt"[, rounds]` — LOOP ISOLATO multi-round (assi C + D1):
    /// fino a N invocazioni pulite consecutive, ognuna con la vista curata AGGIORNATA dal round
    /// precedente (memoria esterna del run + `BEST`), e con **budget CUMULATIVO del run**: il
    /// budget del worker è il tetto dell'intero run, non della singola vista (ogni round vede
    /// `budget − speso` come cap). Stop ESPLICITI, mai silenziosi: `-2` errore provider (round 1
    /// senza provider), `0` budget del run esaurito (`last_error = "run budget exhausted: …"`),
    /// `1` completati tutti i round. Il best-so-far dei round riusciti resta in `&lt;ws&gt;/best`.
    /// </summary>
    public static double WorkerLoop(string task, double rounds, string require = "")
    {
        AiWorker w;
        lock (Gate)
        {
            if (string.IsNullOrEmpty(_current) || !Workers.TryGetValue(_current, out var cw)) return InvokeNoWorker;
            w = cw;
        }
        var hasAudit = !string.IsNullOrWhiteSpace(require);
        if (hasAudit && RequireEvaluator is null)
            throw new InvalidOperationException(
                "AI WORKER LOOP REQUIRE: nessun valutatore REQUIRE registrato (loop verificato non disponibile su questo backend)");
        var key = ContextKey(w.Name);
        if (w.Budget > 0) Basic.AI.Context.AiContextEngine.SetBudget(key, w.Budget);
        var runBudget = w.Budget > 0 ? w.Budget : Basic.AI.Context.AiContextEngine.GetBudget(key);
        var n = rounds > 0 ? (int)rounds : DefaultLoopRounds;
        long runSpent = 0;
        for (var round = 1; round <= n; round++)
        {
            var raw = BuildCuratedView(w, task);
            var remaining = runBudget > 0 ? runBudget - runSpent : 0;
            if (runBudget > 0)
            {
                if (remaining < raw.Length && Basic.AI.Context.AiContextEngine.GetPolicyFor(key) == "error" || remaining <= 0)
                {
                    var msg = $"run budget exhausted: spent={runSpent} budget={runBudget}";
                    lock (Gate) { _rescued++; }
                    WriteWorkspaceFile(w, "last_error", msg);
                    StateSaveToFile("last_error", msg, w.Workspace);
                    return InvokeBudgetRefused;
                }
                Basic.AI.Context.AiContextEngine.SetBudget(key, remaining);   // cap del round = residuo
            }
            var rc = InvokeWithView(w, key, raw);
            if (rc == InvokeBudgetRefused) return rc;      // rifiuto di policy: già annotato
            runSpent += Basic.AI.Context.AiContextEngine.GetUsed(key);
            PromoteBest(w);
            var answer = StateLoadFromFile("solution", w.Workspace);
            if (hasAudit)
            {
                // L'AUDITOR giudica OGNI risposta, compresa l'eventuale risposta-errore esplicita
                // del provider: se la REQUIRE è soddisfatta il run è VERIFICATO e si ferma subito.
                if (RequireEvaluator!(require, answer))
                {
                    WriteWorkspaceFile(w, "best", answer);
                    StateSaveToFile("best", answer, w.Workspace);
                    StateSaveToFile("verified", "1", w.Workspace);
                    StateSaveToFile("audit_error", "", w.Workspace);
                    WriteWorkspaceFile(w, "last_error", "");
                    return InvokeOk;
                }
                // REWORK: l'esito dell'audit entra nello stato → il round successivo lo VEDE nella
                // vista curata (sezione `STATE audit_error`): il rework è informato, non cieco.
                StateSaveToFile("audit_error", answer, w.Workspace);
            }
            // Audit (se presente) RIFIUTATO e risposta in errore: stop esplicito, senza ripetere
            // a vuoto un'invocazione che il provider non riesce a servire.
            if (rc == InvokeProviderError) return rc;
        }
        if (!hasAudit) return InvokeOk;
        lock (Gate) { _rescued++; }
        var auditFail = $"audit failed after {n} round(s): {StateLoadFromFile("audit_error", w.Workspace)}";
        WriteWorkspaceFile(w, "last_error", auditFail);
        StateSaveToFile("last_error", auditFail, w.Workspace);
        return InvokeAuditFailed;
    }

    /// <summary>
    /// `AI WORKER MANAGE [, cycles] [REQUIRE "<expr>"]` — MANAGER LOOP (P4): il manager prende dal
    /// **task graph** il prossimo task lavorabile (`TaskGraphRegistry.NextPending`: primo `pending`
    /// in ordine alfabetico con dipendenze `done`) e lo assegna al worker con l'**invocazione
    /// pulita** (vista curata sul workspace, budget CUMULATIVO del run, come `WorkerLoop`).
    /// L'**auditor** (REQUIRE, stesso valutatore di AI VERIFY) giudica la risposta:
    ///   • soddisfatta → task `done` con quel risultato, si passa al task successivo;
    ///   • rifiutata  → **DISSENSO loggato** (`state/dissent` + conteggio `state/dissents`, visibile
    ///     al ciclo successivo perché la vista curata include `state/*`) e si ritenta lo stesso task.
    /// Esiti: `1` nessun task pendente (fatto) · `0` budget del run esaurito ·
    /// `-1` nessun worker · `-2` errore provider · `-3` cicli esauriti con task pendenti ·
    /// `-4` deadlock di dipendenze. L'ultimo esito è in `state/manage_exit` (osservabile).
    /// </summary>
    public static double WorkerManage(double cycles, string require = "", string plan = "")
    {
        AiWorker w;
        lock (Gate)
        {
            if (string.IsNullOrEmpty(_current) || !Workers.TryGetValue(_current, out var cw)) return InvokeNoWorker;
            w = cw;
        }
        var hasAudit = !string.IsNullOrWhiteSpace(require);
        if (hasAudit && RequireEvaluator is null)
            throw new InvalidOperationException(
                "AI WORKER MANAGE REQUIRE: nessun valutatore REQUIRE registrato (manager loop verificato non disponibile su questo backend)");
        var key = ContextKey(w.Name);
        if (w.Budget > 0) Basic.AI.Context.AiContextEngine.SetBudget(key, w.Budget);
        var runBudget = w.Budget > 0 ? w.Budget : Basic.AI.Context.AiContextEngine.GetBudget(key);
        var n = cycles > 0 ? (int)cycles : DefaultLoopRounds;
        // SCOPING su AI.PLAN*: se è indicato un piano, il manager lavora SOLO sui suoi task e il
        // piano deve esistere ed essere APPROVATO (`AI PLAN APPROVE`) — un piano in bozza non è
        // eseguibile: esito esplicito -5, mai lavoro silenzioso su un piano non approvato.
        var planTasks = "";
        if (!string.IsNullOrWhiteSpace(plan))
        {
            if (!Basic.Agents.PlanRegistry.IsApproved(plan))
                return FinishManage(w, InvokePlanNotApproved, 0,
                    $"piano '{plan}' inesistente o non approvato (AI PLAN APPROVE \"{plan}\")");
            planTasks = Basic.Agents.PlanRegistry.TasksCsv(plan);
            if (string.IsNullOrWhiteSpace(planTasks))
                return FinishManage(w, InvokeOk, 0, "");   // piano senza task: niente da fare
        }
        long runSpent = 0;
        long dissents = 0;
        for (var cycle = 1; cycle <= n; cycle++)
        {
            var taskId = planTasks.Length > 0
                ? Basic.Agents.TaskGraphRegistry.NextPending(planTasks, out var taskDesc, out var deadlock)
                : Basic.Agents.TaskGraphRegistry.NextPending(out taskDesc, out deadlock);
            if (string.IsNullOrEmpty(taskId))
                return FinishManage(w, deadlock ? InvokeDeadlock : InvokeOk, dissents,
                    deadlock ? "task graph: dipendenze non soddisfatte (deadlock)" : "");
            var raw = BuildCuratedView(w, taskDesc.Length > 0 ? taskId + " — " + taskDesc : taskId);
            var remaining = runBudget > 0 ? runBudget - runSpent : 0;
            if (runBudget > 0)
            {
                if (remaining < raw.Length && Basic.AI.Context.AiContextEngine.GetPolicyFor(key) == "error" || remaining <= 0)
                {
                    var msg = $"run budget exhausted: spent={runSpent} budget={runBudget}";
                    lock (Gate) { _rescued++; }
                    WriteWorkspaceFile(w, "last_error", msg);
                    StateSaveToFile("last_error", msg, w.Workspace);
                    return FinishManage(w, InvokeBudgetRefused, dissents, msg);
                }
                Basic.AI.Context.AiContextEngine.SetBudget(key, remaining);
            }
            var rc = InvokeWithView(w, key, raw);
            if (rc == InvokeBudgetRefused) return FinishManage(w, rc, dissents, StateLoadFromFile("last_error", w.Workspace));
            runSpent += Basic.AI.Context.AiContextEngine.GetUsed(key);
            var answer = StateLoadFromFile("solution", w.Workspace);
            if (hasAudit)
            {
                if (RequireEvaluator!(require, answer))
                {
                    Basic.Agents.TaskGraphRegistry.SetDone(taskId, answer);
                    PromoteBest(w);
                    StateSaveToFile("verified", "1", w.Workspace);
                    StateSaveToFile("manager_task", taskId, w.Workspace);
                    WriteWorkspaceFile(w, "last_error", "");
                    continue;   // task successivo
                }
                // DISSENSO (D2 anti-anchoring): l'esito scartato è registrato e resta visibile
                // (state/* entra nella vista curata) — il rework non ripropone lo stesso approccio.
                dissents++;
                Basic.Agents.TaskGraphRegistry.SetDissented(taskId, answer);
                StateSaveToFile("dissent", answer, w.Workspace);
                StateSaveToFile("dissents", dissents.ToString(System.Globalization.CultureInfo.InvariantCulture), w.Workspace);
            }
            if (rc == InvokeProviderError)
                return FinishManage(w, rc, dissents, StateLoadFromFile("last_error", w.Workspace));
            if (!hasAudit)
            {
                // Senza auditor il manager considera il task concluso quando il worker ha prodotto
                // una risposta (la verifica è esplicitamente delegata all'utente).
                Basic.Agents.TaskGraphRegistry.SetDone(taskId, answer);
                PromoteBest(w);
                StateSaveToFile("manager_task", taskId, w.Workspace);
            }
        }
        var pend = planTasks.Length > 0
            ? Basic.Agents.TaskGraphRegistry.NextPending(planTasks, out _, out var dl)
            : Basic.Agents.TaskGraphRegistry.NextPending(out _, out dl);
        if (string.IsNullOrEmpty(pend) && !dl) return FinishManage(w, InvokeOk, dissents, "");
        var fail = dl
            ? "task graph: dipendenze non soddisfatte (deadlock)"
            : $"manager loop: cicli esauriti ({n}) con task ancora pending";
        lock (Gate) { _rescued++; }
        WriteWorkspaceFile(w, "last_error", fail);
        StateSaveToFile("last_error", fail, w.Workspace);
        return FinishManage(w, dl ? InvokeDeadlock : InvokeCyclesExhausted, dissents, fail);
    }

    /// <summary>Chiude il manager loop registrando l'esito osservabile in `state/manage_exit`.</summary>
    private static double FinishManage(AiWorker w, double exit, long dissents, string lastError)
    {
        StateSaveToFile("manage_exit", exit.ToString(System.Globalization.CultureInfo.InvariantCulture), w.Workspace);
        if (dissents > 0)
            StateSaveToFile("dissents", dissents.ToString(System.Globalization.CultureInfo.InvariantCulture), w.Workspace);
        if (!string.IsNullOrEmpty(lastError))
        {
            WriteWorkspaceFile(w, "last_error", lastError);
            StateSaveToFile("last_error", lastError, w.Workspace);
        }
        return exit;
    }

    /// <summary>Coproduce/aggiorna il best-so-far del run dal `solution` corrente.</summary>
    private static void PromoteBest(AiWorker w)
    {
        try
        {
            var sol = Path.Combine(w.Workspace, "solution");
            if (!File.Exists(sol)) return;
            var text = File.ReadAllText(sol);
            if (string.IsNullOrWhiteSpace(text) || text.StartsWith("Error:", System.StringComparison.Ordinal)) return;
            File.WriteAllText(Path.Combine(w.Workspace, "best"), text);
            StateSaveToFile("best", text, w.Workspace);
            // P4 "best-so-far checkpoint+artifact": il best-so-far è anche un ARTIFACT del run
            // (ArtifactStore), non solo un file: gli altri attori (team, handoff, replay) lo vedono.
            Basic.Agents.ArtifactStore.Put("best:" + w.Name, Path.Combine(w.Workspace, "best"));
        }
        catch { /* best effort */ }
    }

    /// <summary>Un round di invocazione con vista già costruita (condiviso da Invoke e Loop).</summary>
    private static double InvokeWithView(AiWorker w, string key, string raw)
    {
        // Normalizzazione dal Context Engine: policy error + overflow ⇒ invocazione RIFIUTATA.
        if (!Basic.AI.Context.AiContextEngine.SetView(key, raw))
        {
            var reason = $"context overflow: {raw.Length} > budget {Basic.AI.Context.AiContextEngine.GetBudget(key)}";
            // Il round è contato anche quando l'invocazione è rifiutata (parità col runtime C:
            // `AI WORKER` è un tentativo di round). rescued = fallimento recuperato/annotato.
            lock (Gate) { _rounds++; _rescued++; }
            WriteWorkspaceFile(w, "last_error", reason);
            StateSaveToFile("last_error", reason, w.Workspace);   // osservabile: AI.STATELOAD
            return InvokeBudgetRefused;
        }
        var view = Basic.AI.Context.AiContextEngine.GetView(key);
        WriteWorkspaceFile(w, "context.md", view);
        var active = Completer;
        if (active is null && Basic.AI.Rag.LlmCompleterResolver.Default is { } def)
            active = def.Complete;
        var completer = active;
        var answer = completer is null
            ? NoCompleter
            : completer(
                "Sei un worker isolato: rispondi SOLO con la vista fornita (task/plan/notes/solution). " +
                "Non inventare contesto non presente nella vista.",
                view) ?? "";
        lock (Gate) { _rounds++; _spendChars += view.Length; }
        WriteWorkspaceFile(w, "solution", answer);
        StateSaveToFile("solution", answer, w.Workspace);          // osservabile: AI.STATELOAD
        if (answer.StartsWith("Error:", System.StringComparison.Ordinal))
        {
            lock (Gate) { _rescued++; }
            WriteWorkspaceFile(w, "last_error", answer);
            StateSaveToFile("last_error", answer, w.Workspace);
            return InvokeProviderError;
        }
        return InvokeOk;
    }

    private static string ReadIfExists(string path)
    {
        try { return File.Exists(path) ? File.ReadAllText(path) : ""; } catch { return ""; }
    }

    private static void WriteWorkspaceFile(AiWorker w, string name, string content)
    {
        try { Directory.CreateDirectory(w.Workspace); File.WriteAllText(Path.Combine(w.Workspace, name), content ?? ""); }
        catch { /* best effort */ }
    }

    /// <summary>Segnala un round di lavoro riuscito (rescue/spend dal loop superiore).</summary>
    public static void TrackRound(bool rescued, long spendChars)
    {
        lock (Gate) { _rounds++; if (rescued) _rescued++; _spendChars += spendChars; }
    }

    // metriche run-level
    public static double WorkerRounds() { lock (Gate) return _rounds; }
    public static double WorkerRescued() { lock (Gate) return _rescued; }
    public static double WorkerSpend() { lock (Gate) return _spendChars; }

    public static List<string> WorkerList() { lock (Gate) { var l = new List<string>(); foreach (var k in Workers.Keys) l.Add(k); return l; } }
    public static bool WorkerExists(string name) { lock (Gate) return Workers.ContainsKey(name); }

    private static long ExtractJsonLong(string json, string key)
    {
        var idx = json.IndexOf("\"" + key + "\"", System.StringComparison.OrdinalIgnoreCase);
        if (idx < 0) return 0;
        var colon = json.IndexOf(':', idx);
        if (colon < 0) return 0;
        var start = colon + 1;
        while (start < json.Length && (json[start] == ' ' || json[start] == '\t')) start++;
        var end = start;
        while (end < json.Length && char.IsDigit(json[end])) end++;
        return end > start && long.TryParse(json[start..end], out var v) ? v : 0;
    }

    private static string ExtractJsonString(string json, string key)
    {
        var k = "\"" + key + "\"";
        var idx = json.IndexOf(k, System.StringComparison.OrdinalIgnoreCase);
        if (idx < 0) return "";
        var colon = json.IndexOf(':', idx);
        if (colon < 0) return "";
        var q1 = json.IndexOf('"', colon);
        if (q1 < 0) return "";
        var q2 = json.IndexOf('"', q1 + 1);
        if (q2 < 0) return "";
        return json[(q1 + 1)..q2];
    }

    private static string Sanitize(string s)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var c in s.ToCharArray())
            sb.Append(char.IsAsciiLetterOrDigit(c) || c == '-' || c == '_' ? c : '_');
        return sb.Length == 0 ? "w" : sb.ToString();
    }

    private static double StateSaveToFile(string key, string value, string workspace)
    {
        try
        {
            var dir = Path.Combine(workspace, "state");
            Directory.CreateDirectory(dir);
            File.WriteAllText(Path.Combine(dir, key + ".txt"), value ?? "");
            return 1;
        }
        catch { return -1; }
    }

    private static string StateLoadFromFile(string key, string workspace)
    {
        try
        {
            var p = Path.Combine(workspace, "state", key + ".txt");
            return File.Exists(p) ? File.ReadAllText(p) : "";
        }
        catch { return ""; }
    }

    public static void Reset()
    {
        lock (Gate) { Workers.Clear(); _current = ""; _rounds = 0; _rescued = 0; _spendChars = 0; }
    }
}
