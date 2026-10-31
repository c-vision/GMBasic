// FILE: Basic.IlBackend.IlMcpToolDispatcher
// IMcpToolDispatcher production per il backend IL: risolve il nome della procedura BASIC
// (registrata a compile-time via IlRuntime.RegisterBasicProcedure) e la invoca con gli
// argomenti JSON del tool (mappati per nome parametro). Cablato in McpRuntimeBridge da
// IlRuntime.McpServe — prima non esisteva nessuna implementazione in produzione.
using Basic.Mcp;

namespace Basic.IlBackend;

public sealed class IlMcpToolDispatcher : IMcpToolDispatcher
{
    public Task<McpToolCallResult> DispatchAsync(
        string qualifiedProcedureName,
        string argumentsJson,
        CancellationToken cancellationToken)
    {
        try
        {
            // §5 Camoufox: i tool browser_* nativi NON passano dal GPM (nessun codice BASIC —
            // solo BrowserRuntime → CDP, come ogni azione browser su thread HTTP listener).
            // Percorso puro C# (aggira anche il limite UnresolvedCallExpr di BROWSER.* come
            // RHS/ritorno di FUNCTION, che impedirebbe wrapper BASIC).
            if (qualifiedProcedureName.StartsWith("browser_", System.StringComparison.OrdinalIgnoreCase))
                return Task.FromResult(new McpToolCallResult(IsError: false, TextContent: IlRuntime.BrowserToolDispatch(qualifiedProcedureName, argumentsJson)));
            // GPM-aware: la procedura gira come task GPM (in-context) quando lo scheduler del
            // programma è attivo, MAI in diretta sul thread del listener MCP (iniezione esterna
            // nel GPM — la GPM è l'unico esecutore del codice BASIC). Fallback col scope applicato.
            var text = IlRuntime.InvokeBasicProcedureGpm(qualifiedProcedureName, argumentsJson);
            return Task.FromResult(new McpToolCallResult(IsError: false, TextContent: text));
        }
        catch (Exception ex)
        {
            return Task.FromResult(new McpToolCallResult(IsError: true, TextContent: ex.Message));
        }
    }
}
