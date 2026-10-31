// FILE: Basic.Agents.PlanRegistry — Orchestrazione agenti (A2): Planner.
// Un piano = obiettivo + insieme di task (dal TaskGraphRegistry) + approvazione opzionale.
// Separa la PIANIFICAZIONE dall'ESECUZIONE: PLANRUN rifiuta un piano non approvato e, se
// approvato, esegue il Task Graph riusando Basic.Agents.TaskGraphRegistry.
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_plan.c).
// La decomposizione LLM dell'obiettivo (interview-mode) è un follow-on: v1 registra i task
// esplicitamente via PLANADD.
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe di piani. DEFINE/ADD/APPROVE scrivono, STATUS/LIST leggono,
/// RUN esegue il Task Graph associato (solo se approvato).</summary>
public static class PlanRegistry
{
    private sealed class Plan
    {
        public string Name = "";
        public string Goal = "";
        public bool Approved;
        public string Status = "draft";
        public string Result = "";
        public readonly List<string> Tasks = new();
    }

    private static readonly ConcurrentDictionary<string, Plan> _plans =
        new(StringComparer.OrdinalIgnoreCase);
    private static readonly object _runLock = new();

    /// <summary>Definisce un piano (nome unico). Ridefinire resetta stato/task. 1 = ok.</summary>
    public static double Define(string name, string goal)
    {
        if (string.IsNullOrWhiteSpace(name)) return 0.0;
        _plans[name] = new Plan { Name = name, Goal = goal ?? "" };
        return 1.0;
    }

    /// <summary>Associa un task al piano. 1 = ok, 0 = piano/task vuoto.</summary>
    public static double Add(string name, string taskId)
    {
        if (string.IsNullOrWhiteSpace(name) || string.IsNullOrWhiteSpace(taskId)) return 0.0;
        if (!_plans.TryGetValue(name, out var p)) return 0.0;
        lock (p.Tasks) { if (!p.Tasks.Contains(taskId, StringComparer.OrdinalIgnoreCase)) p.Tasks.Add(taskId); }
        return 1.0;
    }

    /// <summary>Approva il piano (passa a "approved"). 1 = ok, 0 = piano inesistente.</summary>
    public static double Approve(string name)
    {
        if (string.IsNullOrWhiteSpace(name) || !_plans.TryGetValue(name, out var p)) return 0.0;
        p.Approved = true;
        p.Status = "approved";
        return 1.0;
    }

    /// <summary>
    /// Elenco dei task di un piano, separati da virgola e in ORDINE di inserimento
    /// ("" se il piano non esiste o non ha task). Usato dal MANAGER LOOP (`AI WORKER MANAGE "p"`)
    /// per limitare il set di lavoro ai task del piano.
    /// </summary>
    public static string TasksCsv(string name)
    {
        if (string.IsNullOrWhiteSpace(name) || !_plans.TryGetValue(name, out var p)) return "";
        lock (p.Tasks) return string.Join(",", p.Tasks);
    }

    /// <summary>True se il piano esiste ed è approvato (`AI PLAN APPROVE`): il manager loop lo usa
    /// per decidere se può lavorare sul piano (un piano in bozza non è eseguibile).</summary>
    public static bool IsApproved(string name)
        => !string.IsNullOrWhiteSpace(name) && _plans.TryGetValue(name, out var p) && p.Approved;

    /// <summary>JSON dello stato del piano ({} se assente).</summary>
    public static string Status(string name)
        => !string.IsNullOrWhiteSpace(name) && _plans.TryGetValue(name, out var p) ? PlanJson(p) : "{}";

    /// <summary>JSON di tutti i piani, ordinati per nome.</summary>
    public static string ListJson()
    {
        var keys = _plans.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"plans\":[");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append(PlanJson(_plans[keys[i]]));
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Esegue il piano: richiede l'approvazione; esegue il Task Graph associato.
    /// Ritorna il JSON del Task Graph (o {"error":...} se non approvato/inesistente).</summary>
    public static string Run(string name)
    {
        lock (_runLock)
        {
            if (string.IsNullOrWhiteSpace(name) || !_plans.TryGetValue(name, out var p))
                return Err("piano inesistente");
            if (!p.Approved)
                return Err("piano non approvato (usare PLANAPPROVE)");
            p.Status = "running";
            var result = TaskGraphRegistry.Run();
            p.Result = result;
            p.Status = result.Contains("\"error\"", StringComparison.Ordinal) ? "failed" : "done";
            return result;
        }
    }

    /// <summary>Svuota il registro (test/harness).</summary>
    public static void Clear() => _plans.Clear();

    /// <summary>Persistenza: `name\x1fgoal\x1fapproved\x1fstatus\x1fresult\x1ftasks(\x1d)` per piano.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _plans)
        {
            if (!first) sb.Append('\u001e');
            first = false;
            var p = kv.Value;
            string tasks;
            lock (p.Tasks) tasks = string.Join("\u001d", p.Tasks);
            sb.Append(p.Name).Append('\u001f').Append(p.Goal).Append('\u001f').Append(p.Approved ? "1" : "0")
              .Append('\u001f').Append(p.Status).Append('\u001f').Append(p.Result).Append('\u001f').Append(tasks);
        }
        return sb.ToString();
    }

    /// <summary>Ripristina lo stato da <see cref="ExportState"/>.</summary>
    public static void ImportState(string state)
    {
        if (string.IsNullOrEmpty(state)) return;
        foreach (var rec in state.Split('\u001e'))
        {
            var f = rec.Split('\u001f');
            if (f.Length < 6) continue;
            var p = new Plan { Name = f[0], Goal = f[1], Approved = f[2] == "1", Status = f[3], Result = f[4] };
            foreach (var t in f[5].Split('\u001d')) if (t.Length > 0) p.Tasks.Add(t);
            _plans[f[0]] = p;
        }
    }

    private static string PlanJson(Plan p)
    {
        var sb = new StringBuilder();
        sb.Append("{\"name\":\"").Append(Escape(p.Name))
          .Append("\",\"goal\":\"").Append(Escape(p.Goal))
          .Append("\",\"approved\":").Append(p.Approved ? "true" : "false")
          .Append(",\"status\":\"").Append(Escape(p.Status))
          .Append("\",\"result\":\"").Append(Escape(p.Result))
          .Append("\",\"tasks\":[");
        lock (p.Tasks)
        {
            for (var i = 0; i < p.Tasks.Count; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append('"').Append(Escape(p.Tasks[i])).Append('"');
            }
        }
        sb.Append("]}");
        return sb.ToString();
    }

    private static string Err(string msg) => "{\"error\":\"" + Escape(msg) + "\"}";

    private static string Escape(string s)
    {
        var sb = new StringBuilder(s.Length + 2);
        foreach (var c in s)
        {
            switch (c)
            {
                case '"': sb.Append("\\\""); break;
                case '\\': sb.Append("\\\\"); break;
                case '\n': sb.Append("\\n"); break;
                case '\r': sb.Append("\\r"); break;
                case '\t': sb.Append("\\t"); break;
                default: sb.Append(c); break;
            }
        }
        return sb.ToString();
    }
}
