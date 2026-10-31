// FILE: IlBackend.Tests.AgentSwarmTests
// Fase 1 reale: AI.AGENTSWARM lancia N AgentServer.Ask reali con concorrenza bounded (AgentBatch).
// Verifica che N agenti rispondano (provider fixture-completion) e che il risultato contenga N righe.
using System;
using System.Linq;
using Basic.Agents;
using Basic.IlBackend;
using Xunit;

namespace GMBasic.IlBackend.Tests;

public sealed class AgentSwarmTests
{
    [Fact]
    public void SwarmRunsNRealAgentAsks_Bounded()
    {
        var tag = Guid.NewGuid().ToString("N")[..6];
        var prefix = "sw" + tag + "_";
        // Gli agenti usano il provider fixture-completion (rispondono deterministicamente), cosi'
        // AgentServer.Ask produce un output reale per ciascuno.
        var r = IlRuntime.AiAgentSwarm(5, prefix, "rispondi una riga", 2);
        var lines = r.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        Assert.Equal(5, lines.Length);               // 5 agenti -> 5 risultati
        Assert.DoesNotContain(lines, l => l.StartsWith("Error:"));  // nessuno e' fallito silenziosamente
        // pulizia agenti creati dallo swarm
        foreach (var a in AgentServer.Instance.List().Where(x => x.Name.StartsWith(prefix)).ToList())
            AgentServer.Instance.Delete(a.Name);
    }
}
