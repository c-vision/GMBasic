// FILE: Basic.Databases.WalLogSummary

using System.Text;

namespace Basic.Databases;

/// <summary>
/// Read-only WAL inspection snapshot for ISAM.LOG and offline tooling.
/// </summary>
public sealed record WalLogSummary
{
    /// <summary>Path to the main .isdb file.</summary>
    public required string DatabasePath { get; init; }

    /// <summary>Path to the companion .wal file.</summary>
    public required string WalPath { get; init; }

    /// <summary>True when the .wal file exists on disk.</summary>
    public required bool Exists { get; init; }

    /// <summary>Current .wal file size in bytes.</summary>
    public required long SizeBytes { get; init; }

    /// <summary>End offset in the WAL stream (byte position of last record).</summary>
    public required long LastPosition { get; init; }

    /// <summary>Total data-bearing WAL operations scanned.</summary>
    public required int PendingRecords { get; init; }

    /// <summary>Operations in an uncommitted transaction block.</summary>
    public required int UncommittedTransactionOps { get; init; }

    /// <summary>Committed transaction blocks present in the WAL.</summary>
    public required int CommittedTransactionBlocks { get; init; }

    /// <summary>Direct-write operations not yet checkpointed.</summary>
    public required int DirectWritesPending { get; init; }

    /// <summary>True when the WAL ends with a direct checkpoint marker.</summary>
    public required bool EndsWithCheckpoint { get; init; }

    /// <summary>True when a BeginTransaction was seen without a matching commit.</summary>
    public required bool InOpenTransaction { get; init; }

    /// <summary>Active durability mode (runtime only).</summary>
    public string? DurabilityMode { get; init; }

    /// <summary>True when this process holds an open WAL writer (runtime only).</summary>
    public bool? WalHandleActive { get; init; }

    /// <summary>True when direct WAL writes are buffered but not yet synced (runtime only).</summary>
    public bool? DirectWalDirty { get; init; }

    /// <summary>Formats a multi-line key: value summary for BASIC and CLI consumers.</summary>
    public string Format()
    {
        var sb = new StringBuilder();
        sb.AppendLine($"file: {DatabasePath}");
        sb.AppendLine($"wal_path: {WalPath}");
        sb.AppendLine($"wal_exists: {Exists}");
        sb.AppendLine($"wal_size_bytes: {SizeBytes}");
        sb.AppendLine($"last_position: {LastPosition}");
        sb.AppendLine($"pending_records: {PendingRecords}");
        sb.AppendLine($"uncommitted_ops: {UncommittedTransactionOps}");
        sb.AppendLine($"committed_blocks: {CommittedTransactionBlocks}");
        sb.AppendLine($"direct_writes_pending: {DirectWritesPending}");
        sb.AppendLine($"ends_with_checkpoint: {EndsWithCheckpoint}");
        sb.AppendLine($"open_transaction: {InOpenTransaction}");
        if (DurabilityMode is not null)
        {
            sb.AppendLine($"durability_mode: {DurabilityMode}");
        }

        if (WalHandleActive is not null)
        {
            sb.AppendLine($"wal_handle_active: {WalHandleActive}");
        }

        if (DirectWalDirty is not null)
        {
            sb.AppendLine($"direct_wal_dirty: {DirectWalDirty}");
        }

        return sb.ToString().TrimEnd();
    }
}
