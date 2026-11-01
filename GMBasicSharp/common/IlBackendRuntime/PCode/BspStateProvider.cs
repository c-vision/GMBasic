// FILE: Basic.IlBackend.PCode.BspStateProvider.cs
//
// Ponte di STATO fra il runtime managed e il motore BSP in C (`libgmb_bsp`, VM/C via P/Invoke).
//
// PERCHÉ. La VM/C ha un proprio registro di agenti/team/workflow/memoria (gmb_agent.c,
// gmb_team.c, gmb_workflow.c, gmb_memory.c): è quello che usa quando gira da sola. Quando però
// è OSPITATA da questo processo (motore BSP in C), lo stato "vero" è quello managed
// (`AgentServer`, `TeamServer`, `WorkflowServer`, `AgentMemoryStore`): senza un ponte la stessa
// pagina resa dai due motori mostrerebbe tabelle diverse (bug visto: `index.bsp` sulla VM/C
// rendeva "No agents" mentre il managed elencava gli agenti).
//
// COME. Il runtime C espone un hook OPZIONALE (gmb_state_hook.h): il client C# registra una
// funzione, il C la interroga prima del proprio registro e usa la risposta solo se c'è. Se
// nessuno la registra (C99Native, gmb_vm_host) il C resta com'era. Direzione C#→C
// (registrazione del puntatore); il C non linka né conosce il managed (puntatore opaco).
//
// LETTURE (testo, liste separate da `\n`) e MUTAZIONI ("1"/"0"): entrambe delegano alle
// STESSE API che usano le gambe managed (IlRuntime / TeamServer / WorkflowServer), così lo
// stato è uno solo e i due motori restano allineati anche quando una pagina SCRIVE.

namespace Basic.IlBackend.PCode;

/// <summary>Provider dello stato letto/scritto dal motore BSP in C (una sola registrazione per processo).</summary>
internal static class BspStateProvider
{
    private static int _registered;

    /// <summary>Registra il provider sul motore C (idempotente, thread-safe).</summary>
    public static void EnsureRegistered()
    {
        if (Interlocked.Exchange(ref _registered, 1) != 0) return;
        CommonLib.Http.BspC.StateProvider = Query;
    }

    /// <summary>
    /// Risponde a una superficie dell'hook. `null` = "non gestita" → il C usa il proprio
    /// registro (ed è così che restano identici i messaggi d'errore per un agente inesistente).
    /// </summary>
    private static string? Query(string surface, string? a, string? b, string? c)
    {
        try
        {
            switch (surface)
            {
                // ── LETTURE ──────────────────────────────────────────────────────────────
                case "agent.list":
                    return string.Join("\n", IlRuntime.AiAgentList());
                case "agent.get":
                    return AgentExists(a) ? IlRuntime.AiAgentGet(a!, b ?? "") : null;
                case "agent.status":
                    return AgentExists(a) ? IlRuntime.AiAgentStatus(a!) : null;
                case "agent.events":
                    return string.Join("\n", IlRuntime.AiAgentEvents());
                case "team.list":
                    return IlRuntime.TeamList();
                case "team.agents":
                    return IlRuntime.TeamAgents(a ?? "");
                case "workflow.list":
                    return IlRuntime.WorkflowList();
                case "workflow.status":
                    return IlRuntime.WorkflowStatus(a ?? "");
                case "workflow.steps":
                    return IlRuntime.WorkflowSteps(a ?? "");
                case "memory.list":
                    return IlRuntime.MemoryList();
                case "memory.load":
                    return IlRuntime.MemoryLoad(a ?? "");
                case "memory.search":
                    return IlRuntime.MemorySearch(a ?? "");

                // ── MUTAZIONI (stesse API delle gambe managed) ───────────────────────────
                case "agent.create":
                    return Ok(IlRuntime.AiAgentCreate(a ?? "") != 0);
                case "agent.delete":
                    return Ok(IlRuntime.AiAgentDelete(a ?? "") != 0);
                case "agent.set":
                    return Ok(IlRuntime.AiAgentSet(a ?? "", b ?? "", c ?? "") != 0);

                case "team.create":
                    return Ok(Basic.Agents.TeamServer.Instance.Create(a ?? ""));
                case "team.delete":
                    return Ok(Basic.Agents.TeamServer.Instance.Delete(a ?? ""));
                case "team.add":
                    return Ok(Basic.Agents.TeamServer.Instance.AddAgent(a ?? "", b ?? ""));
                case "team.remove":
                    return Ok(Basic.Agents.TeamServer.Instance.RemoveAgent(a ?? "", b ?? ""));
                case "team.set_role":
                    IlRuntime.TeamSetRole(a ?? "", b ?? "", c ?? "");
                    return "1";
                case "team.set_strategy":
                    IlRuntime.TeamSetStrategy(a ?? "", b ?? "");
                    return "1";
                case "team.set_coordinator":
                    return Ok(Basic.Agents.TeamServer.Instance.SetCoordinator(a ?? "", b));

                case "workflow.begin":
                    return Ok(Basic.Workflow.WorkflowServer.Instance.BeginDefinition(a ?? ""));
                case "workflow.step":
                {
                    // c = "retry,timeout" (numeri) — vedi gmb_state_hook.h
                    int retry = 0, timeout = 0;
                    if (!string.IsNullOrEmpty(c))
                    {
                        var parts = c.Split(',');
                        if (parts.Length > 0) int.TryParse(parts[0], out retry);
                        if (parts.Length > 1) int.TryParse(parts[1], out timeout);
                    }
                    return Ok(Basic.Workflow.WorkflowServer.Instance.AddStep(a ?? "", b ?? "", retry, timeout));
                }
                case "workflow.end":
                    return Ok(Basic.Workflow.WorkflowServer.Instance.EndDefinition());
                case "workflow.execute":
                    return Ok(Basic.Workflow.WorkflowServer.Instance.Execute(a ?? ""));
                case "workflow.delete":
                    return Ok(Basic.Workflow.WorkflowServer.Instance.Delete(a ?? ""));

                case "memory.store":
                {
                    double lt = c == "1" ? 1.0 : 0.0;
                    return Ok(IlRuntime.MemoryStore(a ?? "", b ?? "", lt) != 0);
                }
                case "memory.delete":
                    return Ok(IlRuntime.MemoryDelete(a ?? "") != 0);

                default:
                    return null;
            }
        }
        catch
        {
            // Mai propagare un'eccezione managed attraverso il confine P/Invoke: si risponde
            // "non gestita" (letture) o "rifiutata" (mutazioni).
            return surface.StartsWith("agent.", StringComparison.Ordinal)
                || surface.StartsWith("team.", StringComparison.Ordinal)
                || surface.StartsWith("workflow.", StringComparison.Ordinal)
                || surface.StartsWith("memory.", StringComparison.Ordinal)
                    ? (IsRead(surface) ? null : "0")
                    : null;
        }
    }

    private static string Ok(bool ok) => ok ? "1" : "0";

    private static bool IsRead(string surface) => surface.EndsWith(".list", StringComparison.Ordinal)
        || surface is "agent.get" or "agent.status" or "agent.events" or "team.agents"
            or "workflow.status" or "workflow.steps" or "memory.load" or "memory.search";

    private static bool AgentExists(string? name)
    {
        if (string.IsNullOrEmpty(name)) return false;
        foreach (var n in IlRuntime.AiAgentList())
            if (string.Equals(n, name, StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }
}
