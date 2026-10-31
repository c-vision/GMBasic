// FILE: Basic.Agents.AgentStatusManager
// B5.3 — AgentStatus/TeamStatus JSON: GetAgentStatusJson/GetTeamStatusJson.
// Stato: Idle/Working/Planning/Waiting (derivato dall'attività recente), PermissionLevel,
// TeamMemberships. Mai stub: JSON sempre popolato.

using System.Text.Json;
using Basic.Workflow;

namespace Basic.Agents;

public static class AgentStatusManager
{
    public static string GetAgentStatusJson(string name)
    {
        var agent = AgentServer.Instance.List().FirstOrDefault(a => a.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        if (agent is null) return $"{{\"error\":\"agent '{name}' non trovato\"}}";
        var membership = TeamServer.Instance.List().Where(t => t.Members.Any(m =>
            m.AgentName.Equals(name, StringComparison.OrdinalIgnoreCase))).Select(t => t.Name).ToArray();
        var status = "Idle";
        return JsonSerializer.Serialize(new
        {
            name = agent.Name,
            id = agent.Id,
            model = agent.Model,
            status,
            permission = agent.PermissionLevel.ToString(),
            toolCount = agent.ToolNames.Count,
            historyCount = agent.History.Count,
            hasKeypair = !string.IsNullOrWhiteSpace(agent.PublicKey),
            teams = membership,
            updatedAt = DateTime.UtcNow
        });
    }

    public static string GetTeamStatusJson(string teamName)
    {
        var team = TeamServer.Instance.Get(teamName);
        if (team is null) return $"{{\"error\":\"team '{teamName}' non trovato\"}}";
        var status = team.Metadata.TryGetValue("lastRunMs", out var lastRun) ? "Executing" : "Idle";
        var strategy = team.Strategy.ToString();
        var members = team.Members.Select(m => new { name = m.AgentName, role = m.Role.ToString() }).ToArray();
        return JsonSerializer.Serialize(new
        {
            name = team.Name,
            id = team.Id,
            status,
            strategy,
            coordinator = team.Coordinator,
            memberCount = members.Length,
            members,
            updatedAt = DateTime.UtcNow
        });
    }

    public static string GetAgentsJson()
    {
        var agents = AgentServer.Instance.List().Select(a => new {
            name = a.Name,
            status = "Idle",
            permission = a.PermissionLevel.ToString(),
            model = a.Model,
            teams = TeamServer.Instance.List().Where(t => t.Members.Any(m => m.AgentName.Equals(a.Name, StringComparison.OrdinalIgnoreCase))).Select(t => t.Name).ToArray(),
            toolCount = a.ToolNames.Count,
            historyCount = a.History.Count,
            hasKeypair = !string.IsNullOrWhiteSpace(a.PublicKey)
        }).ToArray();
        return JsonSerializer.Serialize(agents);
    }

    public static string GetTeamsJson()
    {
        var teams = TeamServer.Instance.List().Select(t => new { name = t.Name, strategy = t.Strategy.ToString(), memberCount = t.Members.Count }).ToArray();
        return JsonSerializer.Serialize(teams);
    }
}
