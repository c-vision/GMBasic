/* FILE: native/gmb_db_c/src/Replication/ReplicationShipState.c */
/* Port 1:1 di modules/Database/Replication/ReplicationShipState.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Replication.ReplicationShipState
//
//using System.Text.Json;
//
//namespace Basic.Databases.Replication;
//
///// <summary>
///// Byte offset checkpoint for the TCP shipper (primary side).
///// </summary>
//internal static class ReplicationShipState
//{
//    public static string DefaultCheckpointPath(string replLogPath) => replLogPath + ".ship.ckpt";
//
//    public static long LoadOffset(string checkpointPath)
//    {
//        if (!File.Exists(checkpointPath))
//        {
//            return 0;
//        }
//
//        var payload = JsonSerializer.Deserialize<ShipCheckpointPayload>(File.ReadAllText(checkpointPath));
//        return payload?.Offset ?? 0;
//    }
//
//    public static void SaveOffset(string checkpointPath, long offset)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(checkpointPath);
//        var payload = JsonSerializer.Serialize(new ShipCheckpointPayload(offset));
//        File.WriteAllText(checkpointPath, payload);
//    }
//
//    private sealed record ShipCheckpointPayload(long Offset);
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: internal static class ReplicationShipState */
void ReplicationShipState_DefaultCheckpointPath(const char* replLogPath, char* out, int cap)
{
    snprintf(out, (size_t)cap, "%s.ship.ckpt", replLogPath ? replLogPath : "");
}

int64_t ReplicationShipState_LoadOffset(const char* checkpointPath)
{
    FILE* f;
    char buf[128];
    const char* p;
    if (!checkpointPath) return 0;
    f = fopen(checkpointPath, "rb");
    if (!f) return 0;
    buf[0] = 0;
    if (!fgets(buf, (int)sizeof(buf), f)) { fclose(f); return 0; }
    fclose(f);
    p = strstr(buf, "Offset");
    if (!p) p = strstr(buf, "offset");
    if (!p) return 0;
    p = strchr(p, ':');
    return p ? (int64_t)atoll(p + 1) : 0;
}

void ReplicationShipState_SaveOffset(const char* checkpointPath, int64_t offset)
{
    FILE* f;
    if (!checkpointPath || !checkpointPath[0]) return;
    f = fopen(checkpointPath, "wb");
    if (!f) return;
    fprintf(f, "{\"Offset\":%lld}", (long long)offset);
    fclose(f);
}

