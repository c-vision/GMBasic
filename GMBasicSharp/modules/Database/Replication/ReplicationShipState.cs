// FILE: Basic.Databases.Replication.ReplicationShipState

using System.Text.Json;

namespace Basic.Databases.Replication;

/// <summary>
/// Byte offset checkpoint for the TCP shipper (primary side).
/// </summary>
internal static class ReplicationShipState
{
    public static string DefaultCheckpointPath(string replLogPath) => replLogPath + ".ship.ckpt";

    public static long LoadOffset(string checkpointPath)
    {
        if (!File.Exists(checkpointPath))
        {
            return 0;
        }

        var payload = JsonSerializer.Deserialize<ShipCheckpointPayload>(File.ReadAllText(checkpointPath));
        return payload?.Offset ?? 0;
    }

    public static void SaveOffset(string checkpointPath, long offset)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(checkpointPath);
        var payload = JsonSerializer.Serialize(new ShipCheckpointPayload(offset));
        File.WriteAllText(checkpointPath, payload);
    }

    private sealed record ShipCheckpointPayload(long Offset);
}
