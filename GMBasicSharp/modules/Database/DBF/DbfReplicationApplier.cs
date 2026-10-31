// FILE: Basic.DBF.DbfReplicationApplier

using Basic.Databases.Replication;

namespace Basic.DBF;

internal static class DbfReplicationApplier
{
    public static int Apply(string replLogPath, DbfEngine target)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
        ArgumentNullException.ThrowIfNull(target);
        if (!File.Exists(replLogPath))
        {
            throw new FileNotFoundException("Replication log not found.", replLogPath);
        }

        target.SetReplication(string.Empty);
        var state = new ReplicationApplyState();
        var applied = 0;
        foreach (var line in File.ReadLines(replLogPath))
        {
            if (ApplyLine(line, state, target))
            {
                applied++;
            }
        }

        return applied;
    }

    public static int Follow(string replLogPath, DbfEngine target, string? checkpointPath = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
        ArgumentNullException.ThrowIfNull(target);
        target.SetReplication(string.Empty);
        checkpointPath ??= ReplicationLogFollower.DefaultCheckpointPath(replLogPath);
        return ReplicationLogFollower.FollowOnce(replLogPath, (state, line) => ApplyLine(line, state, target), checkpointPath);
    }

    internal static bool ApplyLine(string line, ReplicationApplyState state, DbfEngine target)
    {
        if (string.IsNullOrWhiteSpace(line))
        {
            return false;
        }

        if (!ReplicationLogLine.TryParse(line, out var op, out var primaryRecNo, out var values))
        {
            throw new InvalidDataException($"Invalid replication line: {line}");
        }

        switch (op)
        {
            case ReplicationOp.TxnBegin:
                state.InTxn = true;
                state.PendingTxnLines.Clear();
                return false;
            case ReplicationOp.TxnCommit:
                state.InTxn = false;
                var applied = 0;
                foreach (var pending in state.PendingTxnLines)
                {
                    if (!ReplicationLogLine.TryParse(pending, out var pendingOp, out var pendingRecNo, out var pendingValues))
                    {
                        throw new InvalidDataException($"Invalid replication line: {pending}");
                    }

                    if (ApplyDataLine(pending, state, target, pendingOp, pendingRecNo, pendingValues))
                    {
                        applied++;
                    }
                }

                state.PendingTxnLines.Clear();
                return applied > 0;
            default:
                if (state.InTxn)
                {
                    state.PendingTxnLines.Add(line);
                    return false;
                }

                return op switch
                {
                    ReplicationOp.Pack => ApplyPack(state, target, primaryRecNo),
                    ReplicationOp.Schema => ApplySchema(state, target, values),
                    ReplicationOp.Reindex => ApplyReindex(state, target, primaryRecNo),
                    _ => ApplyDataLine(line, state, target, op, primaryRecNo, values),
                };
        }
    }

    private static bool ApplyPack(ReplicationApplyState state, DbfEngine target, long generation)
    {
        if (generation <= state.LastPackGeneration)
        {
            return false;
        }

        target.Pack();
        state.ResetAfterPack(generation);
        return true;
    }

    private static bool ApplySchema(ReplicationApplyState state, DbfEngine target, Dictionary<string, object?>? values)
    {
        if (values is null)
        {
            throw new InvalidDataException("SCHEMA requires metadata payload.");
        }

        var generation = ReplicationSchemaMetadata.RequireGeneration(values);
        if (generation <= state.LastSchemaGeneration)
        {
            return false;
        }

        var gmsBytes = ReplicationSchemaMetadata.RequireBase64(values, ReplicationSchemaMetadata.GmsKey);
        DbfSchemaManager.WriteSchemaBytes(target.FilePath, gmsBytes);
        state.MarkSchemaApplied(generation);
        return true;
    }

    private static bool ApplyReindex(ReplicationApplyState state, DbfEngine target, long generation)
    {
        if (generation <= state.LastReindexGeneration)
        {
            return false;
        }

        target.Reindex();
        state.MarkReindexApplied(generation);
        return true;
    }

    private static bool ApplyDataLine(string line, ReplicationApplyState state, DbfEngine target, ReplicationOp op, long primaryRecNo, Dictionary<string, object?>? values)
    {
        switch (op)
        {
            case ReplicationOp.Append:
                if (TrySkipExistingAppend(target, state.RecMap, primaryRecNo) || !state.AppliedAppends.Add(primaryRecNo))
                {
                    return false;
                }

                var appendValues = values is null ? BlankValuesFromSchema(target) : ReplicationLogLine.NormalizeValues(values);
                target.AppendRecord(appendValues, target.CalculateKeys(appendValues, target));
                state.RecMap[primaryRecNo] = target.GetCurrentRecordNumber();
                return true;
            case ReplicationOp.Put:
                if (values is null)
                {
                    throw new InvalidDataException($"PUT requires values: {line}");
                }

                var putValues = ReplicationLogLine.NormalizeValues(values);
                var putRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
                target.PositionAtRecord((int)putRecNo);
                var oldValues = target.GetCurrentRecordValues() ?? throw new InvalidOperationException($"Record {putRecNo} not found for PUT.");
                target.PutCurrentRecord(putValues, target.CalculateKeys(oldValues, target), target.CalculateKeys(putValues, target));
                return true;
            case ReplicationOp.Delete:
                var deleteRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
                target.PositionAtRecord((int)deleteRecNo);
                var deleteValues = target.GetCurrentRecordValues() ?? throw new InvalidOperationException($"Record {deleteRecNo} not found for DELETE.");
                target.DeleteCurrent(target.CalculateKeys(deleteValues, target));
                return true;
            case ReplicationOp.Recall:
                var recallRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
                target.PositionAtRecord((int)recallRecNo);
                var recallValues = target.GetCurrentRecordValues() ?? throw new InvalidOperationException($"Record {recallRecNo} not found for RECALL.");
                target.RecallCurrent(target.CalculateKeys(recallValues, target));
                return true;
            default:
                throw new InvalidDataException($"Unsupported replication op: {op}");
        }
    }

    private static long ResolveRecNo(IReadOnlyDictionary<long, long> recMap, long primaryRecNo) =>
        recMap.TryGetValue(primaryRecNo, out var mapped) ? mapped : primaryRecNo;

    private static bool TrySkipExistingAppend(DbfEngine target, Dictionary<long, long> recMap, long primaryRecNo)
    {
        if (primaryRecNo > target.GetTotalRecordCount())
        {
            return false;
        }

        target.PositionAtRecord((int)primaryRecNo);
        if (target.GetCurrentRecordValues() is null)
        {
            return false;
        }

        recMap[primaryRecNo] = primaryRecNo;
        return true;
    }

    private static Dictionary<string, object?> BlankValuesFromSchema(DbfEngine engine)
    {
        var values = new Dictionary<string, object?>(StringComparer.OrdinalIgnoreCase);
        foreach (var field in engine.GetFields())
        {
            values[field.Name] = DbfFieldWriter.GetBlankValue(field.Type);
        }

        return values;
    }
}
