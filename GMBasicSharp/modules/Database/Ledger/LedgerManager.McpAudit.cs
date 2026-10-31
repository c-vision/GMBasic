// FILE: Basic.Ledger.LedgerManager.McpAudit
namespace Basic.Ledger;

public sealed partial class LedgerManager
{
    /// <summary>Append a zero-sum audit transaction to a Ledger journal.</summary>
    public static void AppendAuditLogEntry(LedgerContext context, string description)
    {
        ArgumentNullException.ThrowIfNull(context);
        ArgumentException.ThrowIfNullOrWhiteSpace(description);
        AppendImportedTransaction(
            context,
            DateOnly.FromDateTime(DateTime.UtcNow),
            description,
            "Audit:MCP",
            0m,
            "Audit:MCP:Clearing");
    }
}
