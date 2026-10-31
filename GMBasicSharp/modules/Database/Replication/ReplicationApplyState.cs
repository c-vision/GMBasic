// FILE: Basic.Databases.Replication.ReplicationApplyState

using System.Text.Json;

namespace Basic.Databases.Replication;

/// <summary>
/// Mutable replay state shared by ISAM and DBF replication appliers.
/// </summary>
internal sealed class ReplicationApplyState
{
    public HashSet<long> AppliedAppends { get; } = [];
    public Dictionary<long, long> RecMap { get; } = [];
    public List<string> PendingTxnLines { get; } = [];
    public bool InTxn { get; set; }
    public long LastPackGeneration { get; set; }
    public long LastSchemaGeneration { get; set; }
    public long LastReindexGeneration { get; set; }

    /// <summary>
    /// Clears RecMap after PACK — physical record numbers are compacted; identity fallback applies.
    /// </summary>
    public void ResetAfterPack(long generation)
    {
        RecMap.Clear();
        AppliedAppends.Clear();
        LastPackGeneration = generation;
    }

    public void MarkSchemaApplied(long generation) => LastSchemaGeneration = generation;

    public void MarkReindexApplied(long generation) => LastReindexGeneration = generation;

    public void SaveCheckpoint(string checkpointPath, long byteOffset)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(checkpointPath);
        var payload = JsonSerializer.Serialize(new CheckpointPayload(
            byteOffset,
            RecMap,
            LastPackGeneration,
            LastSchemaGeneration,
            LastReindexGeneration));
        File.WriteAllText(checkpointPath, payload);
    }

    public static (long ByteOffset, ReplicationApplyState State) LoadCheckpoint(string checkpointPath)
    {
        var state = new ReplicationApplyState();
        if (!File.Exists(checkpointPath))
        {
            return (0, state);
        }

        var payload = JsonSerializer.Deserialize<CheckpointPayload>(File.ReadAllText(checkpointPath));
        if (payload is null)
        {
            return (0, state);
        }

        foreach (var (key, value) in payload.RecMap)
        {
            state.RecMap[key] = value;
        }

        state.LastPackGeneration = payload.LastPackGeneration;
        state.LastSchemaGeneration = payload.LastSchemaGeneration;
        state.LastReindexGeneration = payload.LastReindexGeneration;
        return (payload.Offset, state);
    }

    private sealed record CheckpointPayload(
        long Offset,
        Dictionary<long, long> RecMap,
        long LastPackGeneration = 0,
        long LastSchemaGeneration = 0,
        long LastReindexGeneration = 0);
}
