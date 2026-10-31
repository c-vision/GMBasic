// FILE: Basic.ISAM.IsamReplicationApplier

using Basic.Databases;
using Basic.Databases.Replication;

namespace Basic.ISAM;

internal static class IsamReplicationApplier
{
    public static int Apply(string replLogPath, Engine target, KeyExpressionEvaluatorCallback evaluator)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(evaluator);
        if (!File.Exists(replLogPath))
        {
            throw new FileNotFoundException("Replication log not found.", replLogPath);
        }

        target.SetReplication(string.Empty);
        var state = new ReplicationApplyState();
        var applied = 0;
        foreach (var line in File.ReadLines(replLogPath))
        {
            if (ApplyLine(line, state, target, evaluator))
            {
                applied++;
            }
        }

        return applied;
    }

    public static int Follow(string replLogPath, Engine target, KeyExpressionEvaluatorCallback evaluator, string? checkpointPath = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
        ArgumentNullException.ThrowIfNull(target);
        ArgumentNullException.ThrowIfNull(evaluator);
        target.SetReplication(string.Empty);
        checkpointPath ??= ReplicationLogFollower.DefaultCheckpointPath(replLogPath);
        return ReplicationLogFollower.FollowOnce(replLogPath, (state, line) => ApplyLine(line, state, target, evaluator), checkpointPath);
    }

    internal static bool ApplyLine(string line, ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator)
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

                    if (ApplyDataLine(pending, state, target, evaluator, pendingOp, pendingRecNo, pendingValues))
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
                    ReplicationOp.Pack => ApplyPack(state, target, evaluator, primaryRecNo),
                    ReplicationOp.Schema => ApplySchema(state, target, evaluator, values),
                    ReplicationOp.Reindex => ApplyReindex(state, target, evaluator, 0),
                    _ => ApplyDataLine(line, state, target, evaluator, op, primaryRecNo, values),
                };
        }
    }

    private static bool ApplyPack(ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator, long generation)
    {
        if (generation <= state.LastPackGeneration)
        {
            return false;
        }

        target.Pack(evaluator);
        state.ResetAfterPack(generation);
        return true;
    }

    private static bool ApplySchema(
        ReplicationApplyState state,
        Engine target,
        KeyExpressionEvaluatorCallback evaluator,
        Dictionary<string, object?>? values)
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

        var ischBytes = ReplicationSchemaMetadata.RequireBase64(values, ReplicationSchemaMetadata.IschKey);
        target.ApplyReplicatedSchema(ischBytes, evaluator);
        state.MarkSchemaApplied(generation);
        return true;
    }

    private static bool ApplyReindex(ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator, long generation)
    {
        if (generation <= state.LastReindexGeneration)
        {
            return false;
        }

        target.Reindex(evaluator);
        state.MarkReindexApplied(generation);
        return true;
    }

    private static bool ApplyDataLine(string line, ReplicationApplyState state, Engine target, KeyExpressionEvaluatorCallback evaluator, ReplicationOp op, long primaryRecNo, Dictionary<string, object?>? values)
    {
        switch (op)
        {
            case ReplicationOp.Append:
                if (TrySkipExistingAppend(target, state.RecMap, primaryRecNo) || !state.AppliedAppends.Add(primaryRecNo))
                {
                    return false;
                }

                var appendRecord = values is null ? BlankRecordFromUdt(target.UdtDefinition) : BuildRecord(target.UdtDefinition, ReplicationLogLine.NormalizeValues(values));
                target.Append(appendRecord, evaluator);
                state.RecMap[primaryRecNo] = target.GetCurrentRecordNumber();
                return true;
            case ReplicationOp.Put:
                if (values is null)
                {
                    throw new InvalidDataException($"PUT requires values: {line}");
                }

                var putRecord = BuildRecord(target.UdtDefinition, ReplicationLogLine.NormalizeValues(values));
                var putRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
                target.PositionAtRecord(putRecNo);
                _ = target.GetCurrentRecordWithMetadata(out RecordMetadata putMetadata);
                if (putMetadata.Status == Config.RecordStatusDeleted)
                {
                    throw new InvalidOperationException($"Record {putRecNo} not found for PUT.");
                }

                target.Put(putRecord, evaluator);
                return true;
            case ReplicationOp.Delete:
                var deleteRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
                target.PositionAtRecord(deleteRecNo);
                _ = target.GetCurrentRecordWithMetadata(out RecordMetadata deleteMetadata);
                if (deleteMetadata.Status == Config.RecordStatusDeleted)
                {
                    return false;
                }

                target.DeleteCurrent(evaluator);
                return true;
            case ReplicationOp.Recall:
                var recallRecNo = ResolveRecNo(state.RecMap, primaryRecNo);
                target.PositionAtRecord(recallRecNo);
                _ = target.GetCurrentRecordWithMetadata(out RecordMetadata recallMetadata);
                if (recallMetadata.Status == Config.RecordStatusActive)
                {
                    return false;
                }

                target.RecallCurrent(evaluator);
                return true;
            default:
                throw new InvalidDataException($"Unsupported replication op: {op}");
        }
    }

    private static long ResolveRecNo(IReadOnlyDictionary<long, long> recMap, long primaryRecNo) =>
        recMap.TryGetValue(primaryRecNo, out var mapped) ? mapped : primaryRecNo;

    private static bool TrySkipExistingAppend(Engine target, Dictionary<long, long> recMap, long primaryRecNo)
    {
        if (primaryRecNo > target.LastPhysicalRecNumber)
        {
            return false;
        }

        target.PositionAtRecord(primaryRecNo);
        _ = target.GetCurrentRecordWithMetadata(out RecordMetadata metadata);
        if (metadata.Status == Config.RecordStatusDeleted)
        {
            return false;
        }

        recMap[primaryRecNo] = primaryRecNo;
        return true;
    }

    private static UdtInstance BuildRecord(UserDefinedType udt, IReadOnlyDictionary<string, object?> values)
    {
        var fields = new Dictionary<int, object?>();
        foreach (var field in udt.Fields)
        {
            fields[field.SymbolId] = values.TryGetValue(field.Name.Lexeme, out var value) ? value : DefaultFieldValue(field.DataType);
        }

        return new UdtInstance(udt, fields);
    }

    private static UdtInstance BlankRecordFromUdt(UserDefinedType udt)
    {
        var fields = new Dictionary<int, object?>();
        foreach (var field in udt.Fields)
        {
            fields[field.SymbolId] = DefaultFieldValue(field.DataType);
        }

        return new UdtInstance(udt, fields);
    }

    private static object? DefaultFieldValue(DataType dataType) => dataType switch
    {
        StringType { FixedLength: { } len } => new string(' ', len),
        StringType => string.Empty,
        LongType => 0L,
        JsonType => "{}",
        BitType => false,
        _ => null,
    };
}
