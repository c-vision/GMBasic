// FILE: Basic.Agents.GoalRegistry — Orchestrazione agenti (A4): Goal + idle continuation.
// Obiettivo PERSISTENTE del thread: quando l'agente va idle, il runtime può riprenderlo
// ("idle continuation"). Questo registro è la policy deterministica: obiettivo, stato attivo,
// contatore di passi di continuazione. L'aggancio alla GPM (SpawnGoSub quando idle) è un
// follow-on: qui TICK rappresenta un impulso di continuazione (deterministico, testabile).
//
// Parita' di comportamento con il runtime C (native/gmb_runtime_c/src/gmb_goal.c):
// stesse operazioni, stesso formato JSON. Thread-safe.
using System.Text;

namespace Basic.Agents;

/// <summary>Registro thread-safe del goal persistente. SET/TICK/CLEAR scrivono,
/// GET/STATUS leggono.</summary>
public static class GoalRegistry
{
    private static readonly object _lock = new();
    private static string _objective = "";
    private static bool _active;
    private static int _steps;
    private static string _agent = "";
    private static string _last = "";

    /// <summary>Associa un agente al goal (idle continuation). 1 = ok, 0 = nome vuoto.</summary>
    public static double SetAgent(string agent)
    {
        if (string.IsNullOrWhiteSpace(agent)) return 0.0;
        lock (_lock) { _agent = agent; }
        return 1.0;
    }

    /// <summary>Imposta l'obiettivo persistente (attivo, passi azzerati). 1 = ok, 0 = vuoto.</summary>
    public static double Set(string objective)
    {
        if (string.IsNullOrWhiteSpace(objective)) return 0.0;
        lock (_lock) { _objective = objective; _active = true; _steps = 0; }
        return 1.0;
    }

    /// <summary>Obiettivo corrente ("" se nessuno).</summary>
    public static string Get()
    {
        lock (_lock) { return _objective; }
    }

    /// <summary>JSON dello stato: {"objective":..,"active":bool,"steps":n}.</summary>
    public static string Status()
    {
        lock (_lock)
        {
            var sb = new StringBuilder();
            sb.Append("{\"objective\":\"").Append(Escape(_objective))
              .Append("\",\"active\":").Append(_active ? "true" : "false")
              .Append(",\"steps\":").Append(_steps)
              .Append(",\"agent\":\"").Append(Escape(_agent))
              .Append("\",\"last\":\"").Append(Escape(_last)).Append("\"}");
            return sb.ToString();
        }
    }

    /// <summary>Impulso di continuazione idle: se attivo incrementa i passi e ritorna il
    /// conteggio; se inattivo ritorna 0.</summary>
    public static double Tick()
    {
        lock (_lock)
        {
            if (!_active) return 0.0;
            _steps++;
            if (_agent.Length > 0 && AgentServer.Instance.Exists(_agent))
            {
                try { _last = AgentServer.Instance.Ask(_agent, _objective); }
                catch (Exception ex) { _last = "error: " + ex.Message; }
            }
            return _steps;
        }
    }

    /// <summary>Svuota il goal (disattiva). 1 = c'era un goal, 0 = nessuno.</summary>
    public static double Clear()
    {
        lock (_lock)
        {
            var had = _active || _objective.Length > 0;
            _objective = ""; _active = false; _steps = 0; _agent = ""; _last = "";
            return had ? 1.0 : 0.0;
        }
    }

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
