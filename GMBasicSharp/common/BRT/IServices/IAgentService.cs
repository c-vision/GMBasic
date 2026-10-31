// FILE: Basic.Runtime.IServices.IAgentService.cs

namespace Basic.Runtime;

public interface IAgentService
{
    void CreateAgent(string name, string? configJson);
    void DeleteAgent(string name);
    string SaveAgent(string name);
    void LoadAgent(string name, string configJson);
}
