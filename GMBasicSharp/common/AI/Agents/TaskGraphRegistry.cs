// FILE: Basic.Agents.TaskGraphRegistry — Orchestrazione agenti (B1): registro di task con
// dipendenze (DAG) e esecuzione in ordine topologico, ogni nodo delegato all'agente assegnato.
// Espone al BASIC il concetto di Task Graph riusando il runtime agent esistente (AgentServer.Ask).
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_taskgraph.c):
// stesse operazioni, stesso formato JSON. Esecuzione SEQUENZIALE in ordine topologico
// (GPM-safe: nessun Task.Run da worker; il parallelismo bounded via AgentDag.Execute è un
// follow-on — vedi Todo orchestrazione §7).
using System.Collections.Concurrent;
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe di task con dipendenze. DEFINE/DEPENDS/ASSIGN scrivono,
/// STATUS/LIST leggono, RUN esegue il DAG in ordine topologico (agente vuoto = passthrough).</summary>
public static class TaskGraphRegistry
{
    private sealed class Node
    {
        public string Id = "";
        public string Desc = "";
        public string Agent = "";
        public string Status = "pending";
        public string Result = "";
        public readonly List<string> Deps = new();
    }

    private static readonly ConcurrentDictionary<string, Node> _nodes =
        new(StringComparer.OrdinalIgnoreCase);
    private static readonly object _runLock = new();

    /// <summary>Definisce un task (id unico). Ridefinire resetta desc/stato. 1 = ok, 0 = id vuoto.</summary>
    public static double Define(string id, string desc)
    {
        if (string.IsNullOrWhiteSpace(id)) return 0.0;
        _nodes[id] = new Node { Id = id, Desc = desc ?? "" };
        return 1.0;
    }

    /// <summary>Aggiunge una dipendenza (dep) al task id. 1 = ok, 0 = task/dep inesistente.</summary>
    public static double Depends(string id, string dep)
    {
        if (string.IsNullOrWhiteSpace(id) || string.IsNullOrWhiteSpace(dep)) return 0.0;
        if (!_nodes.TryGetValue(id, out var n)) return 0.0;
        if (!_nodes.ContainsKey(dep)) return 0.0;
        lock (n.Deps) { if (!n.Deps.Contains(dep, StringComparer.OrdinalIgnoreCase)) n.Deps.Add(dep); }
        return 1.0;
    }

    /// <summary>Assegna un agente al task. 1 = ok, 0 = task inesistente.</summary>
    public static double Assign(string id, string agent)
    {
        if (string.IsNullOrWhiteSpace(id) || !_nodes.TryGetValue(id, out var n)) return 0.0;
        n.Agent = agent ?? "";
        return 1.0;
    }

    /// <summary>
    /// Prossimo task lavorabile dal MANAGER loop: il primo task `pending` in ordine alfabetico di id
    /// le cui dipendenze sono tutte `done` (o senza dipendenze). Determininistico.
    /// Ritorna "" se non c'è nessun task lavorabile; `deadlock=true` se restano task `pending` ma
    /// nessuno ha le dipendenze soddisfatte (ciclo nell'ordine di dipendenza) — i due casi vanno
    /// distinti: "finito" non è "bloccato".
    /// </summary>
    public static string NextPending(out string desc, out bool deadlock)
        => NextPending(null, out desc, out deadlock);

    /// <summary>Come <see cref="NextPending(out string, out bool)"/> ma limitato ai soli task in
    /// <paramref name="onlyIds"/> (null/"" = tutti): è lo SCOPING del manager loop su un piano
    /// (`AI WORKER MANAGE "<plan>"`).</summary>
    public static string NextPending(string? onlyIds, out string desc, out bool deadlock)
    {
        desc = ""; deadlock = false;
        var filter = string.IsNullOrWhiteSpace(onlyIds)
            ? null
            : new System.Collections.Generic.HashSet<string>(
                onlyIds!.Split(',', System.StringSplitOptions.RemoveEmptyEntries | System.StringSplitOptions.TrimEntries),
                System.StringComparer.OrdinalIgnoreCase);
        var keys = _nodes.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var pending = 0;
        foreach (var k in keys)
        {
            var n = _nodes[k];
            if (!string.Equals(n.Status, "pending", StringComparison.OrdinalIgnoreCase)) continue;
            if (filter is not null && !filter.Contains(n.Id)) continue;
            pending++;
            var ready = true;
            lock (n.Deps)
                foreach (var d in n.Deps)
                {
                    if (!_nodes.TryGetValue(d, out var dn)) { ready = false; break; }
                    if (!string.Equals(dn.Status, "done", StringComparison.OrdinalIgnoreCase)) { ready = false; break; }
                }
            if (!ready) continue;
            desc = n.Desc; return n.Id;
        }
        deadlock = pending > 0;
        return "";
    }

    /// <summary>Segna il task come `done` con il risultato accettato dall'auditor (1 = ok).</summary>
    public static double SetDone(string id, string result)
    {
        if (string.IsNullOrWhiteSpace(id) || !_nodes.TryGetValue(id, out var n)) return 0.0;
        n.Status = "done"; n.Result = result ?? "";
        return 1.0;
    }

    /// <summary>Segna il task come `dissented` con la risposta SCARTATA dall'auditor (anti-anchoring):
    /// il task resta lavorabile ma l'esito scartato è registrato (1 = ok).</summary>
    public static double SetDissented(string id, string rejected)
    {
        if (string.IsNullOrWhiteSpace(id) || !_nodes.TryGetValue(id, out var n)) return 0.0;
        n.Status = "pending"; n.Result = "dissented: " + (rejected ?? "");
        return 1.0;
    }

    /// <summary>JSON dello stato del task ({} se assente).</summary>
    public static string Status(string id)
        => !string.IsNullOrWhiteSpace(id) && _nodes.TryGetValue(id, out var n) ? NodeJson(n) : "{}";

    /// <summary>JSON di tutti i task, ordinati per id.</summary>
    public static string ListJson()
    {
        var keys = _nodes.Keys.ToArray();
        System.Array.Sort(keys, StringComparer.OrdinalIgnoreCase);
        var sb = new StringBuilder();
        sb.Append("{\"tasks\":[");
        for (var i = 0; i < keys.Length; i++)
        {
            if (i > 0) sb.Append(',');
            sb.Append(NodeJson(_nodes[keys[i]]));
        }
        sb.Append("]}");
        return sb.ToString();
    }

    /// <summary>Esegue il DAG in ordine topologico. Ogni nodo con agente chiama AgentServer.Ask
    /// (prompt = desc + risultati delle dipendenze); senza agente il risultato è la descrizione.
    /// Ritorna il JSON finale; su errore di struttura un JSON {"error":...}.</summary>
    public static string Run()
    {
        lock (_runLock)
        {
            var nodes = _nodes.Values.ToList();
            foreach (var n in nodes) { n.Status = "pending"; n.Result = ""; }

            var byId = new Dictionary<string, Node>(StringComparer.OrdinalIgnoreCase);
            foreach (var n in nodes) byId[n.Id] = n;
            foreach (var n in nodes)
                foreach (var d in n.Deps)
                    if (!byId.ContainsKey(d)) return Err($"dipendenza '{d}' di '{n.Id}' inesistente");

            var indeg = nodes.ToDictionary(n => n.Id, n => n.Deps.Count, StringComparer.OrdinalIgnoreCase);
            var queue = new Queue<string>(nodes.Where(n => n.Deps.Count == 0).Select(n => n.Id));
            var order = new List<Node>();
            while (queue.Count > 0)
            {
                var id = queue.Dequeue();
                var node = byId[id];
                order.Add(node);
                foreach (var other in nodes)
                    if (other.Deps.Contains(id, StringComparer.OrdinalIgnoreCase))
                        if (--indeg[other.Id] == 0) queue.Enqueue(other.Id);
            }
            if (order.Count != nodes.Count) return Err("ciclo nel DAG");

            foreach (var n in order)
            {
                var input = new StringBuilder();
                foreach (var d in n.Deps) if (byId.TryGetValue(d, out var dn)) input.AppendLine(dn.Result);
                var depText = input.ToString().TrimEnd();
                var prompt = string.IsNullOrEmpty(depText) ? n.Desc : n.Desc + "\n" + depText;
                try
                {
                    n.Result = string.IsNullOrWhiteSpace(n.Agent)
                        ? n.Desc
                        : AgentServer.Instance.Ask(n.Agent, prompt);
                    n.Status = "done";
                }
                catch (System.Exception ex)
                {
                    n.Status = "failed";
                    n.Result = "Error: " + ex.Message;
                }
            }
            return ListJson();
        }
    }

    /// <summary>Svuota il registro (test/harness).</summary>
    public static void Clear() => _nodes.Clear();

    /// <summary>Persistenza: `id\x1fdesc\x1fagent\x1fstatus\x1fresult\x1fdeps(\x1d)` per nodo, \x1e tra nodi.</summary>
    public static string ExportState()
    {
        var sb = new StringBuilder();
        var first = true;
        foreach (var kv in _nodes)
        {
            if (!first) sb.Append('\u001e');
            first = false;
            var n = kv.Value;
            string deps;
            lock (n.Deps) deps = string.Join("\u001d", n.Deps);
            sb.Append(n.Id).Append('\u001f').Append(n.Desc).Append('\u001f').Append(n.Agent)
              .Append('\u001f').Append(n.Status).Append('\u001f').Append(n.Result).Append('\u001f').Append(deps);
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
            var n = new Node { Id = f[0], Desc = f[1], Agent = f[2], Status = f[3], Result = f[4] };
            foreach (var d in f[5].Split('\u001d')) if (d.Length > 0) n.Deps.Add(d);
            _nodes[f[0]] = n;
        }
    }

    private static string NodeJson(Node n)
    {
        var sb = new StringBuilder();
        sb.Append("{\"id\":\"").Append(Escape(n.Id))
          .Append("\",\"desc\":\"").Append(Escape(n.Desc))
          .Append("\",\"agent\":\"").Append(Escape(n.Agent))
          .Append("\",\"status\":\"").Append(Escape(n.Status))
          .Append("\",\"result\":\"").Append(Escape(n.Result))
          .Append("\",\"deps\":[");
        lock (n.Deps)
        {
            for (var i = 0; i < n.Deps.Count; i++)
            {
                if (i > 0) sb.Append(',');
                sb.Append('"').Append(Escape(n.Deps[i])).Append('"');
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
