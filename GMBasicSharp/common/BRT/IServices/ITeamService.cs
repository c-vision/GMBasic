// FILE: Basic.Runtime.IServices.ITeamService.cs

namespace Basic.Runtime;

public interface ITeamService
{
    void CreateTeam(string name);
    void AddAgentToTeam(string teamName, string agentName);
    void RemoveAgentFromTeam(string teamName, string agentName);
    void SendMessageToTeam(string teamName, string message);
}
