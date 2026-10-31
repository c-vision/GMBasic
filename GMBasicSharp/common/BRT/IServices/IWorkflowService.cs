// FILE: Basic.Runtime.IServices.IWorkflowService.cs

namespace Basic.Runtime;

public interface IWorkflowService
{
    void DefineWorkflow(string name);
    void AddStep(string name, string action, int retryCount, int timeoutMs);
    void EndWorkflow();
    void ExecuteWorkflow(string name);
}
