/* FILE: native/gmb_db_c/src/Databases/WalLogSummary.c */
/* Port 1:1 di common/Databases/WalLogSummary.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.WalLogSummary
//
//using System.Text;
//
//namespace Basic.Databases;
//
///// <summary>
///// Read-only WAL inspection snapshot for ISAM.LOG and offline tooling.
///// </summary>
//public sealed record WalLogSummary
//{
//    /// <summary>Path to the main .isdb file.</summary>
//    public required string DatabasePath { get; init; }
//
//    /// <summary>Path to the companion .wal file.</summary>
//    public required string WalPath { get; init; }
//
//    /// <summary>True when the .wal file exists on disk.</summary>
//    public required bool Exists { get; init; }
//
//    /// <summary>Current .wal file size in bytes.</summary>
//    public required long SizeBytes { get; init; }
//
//    /// <summary>End offset in the WAL stream (byte position of last record).</summary>
//    public required long LastPosition { get; init; }
//
//    /// <summary>Total data-bearing WAL operations scanned.</summary>
//    public required int PendingRecords { get; init; }
//
//    /// <summary>Operations in an uncommitted transaction block.</summary>
//    public required int UncommittedTransactionOps { get; init; }
//
//    /// <summary>Committed transaction blocks present in the WAL.</summary>
//    public required int CommittedTransactionBlocks { get; init; }
//
//    /// <summary>Direct-write operations not yet checkpointed.</summary>
//    public required int DirectWritesPending { get; init; }
//
//    /// <summary>True when the WAL ends with a direct checkpoint marker.</summary>
//    public required bool EndsWithCheckpoint { get; init; }
//
//    /// <summary>True when a BeginTransaction was seen without a matching commit.</summary>
//    public required bool InOpenTransaction { get; init; }
//
//    /// <summary>Active durability mode (runtime only).</summary>
//    public string? DurabilityMode { get; init; }
//
//    /// <summary>True when this process holds an open WAL writer (runtime only).</summary>
//    public bool? WalHandleActive { get; init; }
//
//    /// <summary>True when direct WAL writes are buffered but not yet synced (runtime only).</summary>
//    public bool? DirectWalDirty { get; init; }
//
//    /// <summary>Formats a multi-line key: value summary for BASIC and CLI consumers.</summary>
//    public string Format()
//    {
//        var sb = new StringBuilder();
//        sb.AppendLine($"file: {DatabasePath}");
//        sb.AppendLine($"wal_path: {WalPath}");
//        sb.AppendLine($"wal_exists: {Exists}");
//        sb.AppendLine($"wal_size_bytes: {SizeBytes}");
//        sb.AppendLine($"last_position: {LastPosition}");
//        sb.AppendLine($"pending_records: {PendingRecords}");
//        sb.AppendLine($"uncommitted_ops: {UncommittedTransactionOps}");
//        sb.AppendLine($"committed_blocks: {CommittedTransactionBlocks}");
//        sb.AppendLine($"direct_writes_pending: {DirectWritesPending}");
//        sb.AppendLine($"ends_with_checkpoint: {EndsWithCheckpoint}");
//        sb.AppendLine($"open_transaction: {InOpenTransaction}");
//        if (DurabilityMode is not null)
//        {
//            sb.AppendLine($"durability_mode: {DurabilityMode}");
//        }
//
//        if (WalHandleActive is not null)
//        {
//            sb.AppendLine($"wal_handle_active: {WalHandleActive}");
//        }
//
//        if (DirectWalDirty is not null)
//        {
//            sb.AppendLine($"direct_wal_dirty: {DirectWalDirty}");
//        }
//
//        return sb.ToString().TrimEnd();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public sealed record WalLogSummary — tipo in gmb_db_port.h */

/* Formats a multi-line key: value summary for BASIC and CLI consumers. */
void WalLogSummary_Format(const WalLogSummary* s, char* out, int cap)
{
    int n = 0;
    n += snprintf(out + n, (size_t)(cap - n), "file: %s\n", s->DatabasePath);
    n += snprintf(out + n, (size_t)(cap - n), "wal_path: %s\n", s->WalPath);
    n += snprintf(out + n, (size_t)(cap - n), "wal_exists: %s\n", s->Exists ? "True" : "False");
    n += snprintf(out + n, (size_t)(cap - n), "wal_size_bytes: %lld\n", (long long)s->SizeBytes);
    n += snprintf(out + n, (size_t)(cap - n), "last_position: %lld\n", (long long)s->LastPosition);
    n += snprintf(out + n, (size_t)(cap - n), "pending_records: %d\n", s->PendingRecords);
    n += snprintf(out + n, (size_t)(cap - n), "uncommitted_ops: %d\n", s->UncommittedTransactionOps);
    n += snprintf(out + n, (size_t)(cap - n), "committed_blocks: %d\n", s->CommittedTransactionBlocks);
    n += snprintf(out + n, (size_t)(cap - n), "direct_writes_pending: %d\n", s->DirectWritesPending);
    n += snprintf(out + n, (size_t)(cap - n), "ends_with_checkpoint: %s\n", s->EndsWithCheckpoint ? "True" : "False");
    n += snprintf(out + n, (size_t)(cap - n), "open_transaction: %s", s->InOpenTransaction ? "True" : "False");
    if (s->DurabilityMode[0])
        n += snprintf(out + n, (size_t)(cap - n), "\ndurability_mode: %s", s->DurabilityMode);
    if (s->WalHandleActive >= 0)
        n += snprintf(out + n, (size_t)(cap - n), "\nwal_handle_active: %s", s->WalHandleActive ? "True" : "False");
    if (s->DirectWalDirty >= 0)
        n += snprintf(out + n, (size_t)(cap - n), "\ndirect_wal_dirty: %s", s->DirectWalDirty ? "True" : "False");
    (void)n;
}

