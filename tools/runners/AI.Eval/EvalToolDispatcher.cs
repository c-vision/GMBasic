// FILE: AI.Eval.EvalToolDispatcher
// Dispatcher deterministico per l'eval: registra le chiamate e ritorna valori noti
// per i tool del caso (cosi' l'attesa verifica che il MODELLO abbia scelto il tool
// giusto e non il risultato del dispatch).
using Basic.Mcp;

namespace AI.Eval;

public sealed class EvalToolDispatcher : IMcpToolDispatcher
{
    public List<string> Calls { get; } = [];

    public Task<McpToolCallResult> DispatchAsync(
        string qualifiedProcedureName,
        string argumentsJson,
        CancellationToken cancellationToken)
    {
        Calls.Add(qualifiedProcedureName);
        var text = qualifiedProcedureName.Equals("add_numbers", StringComparison.OrdinalIgnoreCase)
            ? "5"
            : $"called:{qualifiedProcedureName}";
        return Task.FromResult(new McpToolCallResult(IsError: false, TextContent: text));
    }
}
