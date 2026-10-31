/* FILE: native/gmb_db_c/src/Ledger/LedgerManager.McpAudit.c */
/* Port 1:1 di modules/Database/Ledger/LedgerManager.McpAudit.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Ledger.LedgerManager.McpAudit
//namespace Basic.Ledger;
//
//public sealed partial class LedgerManager
//{
//    /// <summary>Append a zero-sum audit transaction to a Ledger journal.</summary>
//    public static void AppendAuditLogEntry(LedgerContext context, string description)
//    {
//        ArgumentNullException.ThrowIfNull(context);
//        ArgumentException.ThrowIfNullOrWhiteSpace(description);
//        AppendImportedTransaction(
//            context,
//            DateOnly.FromDateTime(DateTime.UtcNow),
//            description,
//            "Audit:MCP",
//            0m,
//            "Audit:MCP:Clearing");
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

