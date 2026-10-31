// FILE: Basic.Databases.Speed.SpeedDurabilityMode

using Basic.Databases.Speed;

namespace Basic.SPEED;

/// <summary>
/// Controlla quando i file WAL SPEED vengono sincronizzati su disco.
/// </summary>
public enum SpeedDurabilityMode
{
    /// <summary>
    /// Ogni commit esegue fsync su WAL (comportamento legacy).
    /// </summary>
    Full,

    /// <summary>
    /// Scrive su disco senza fsync ad ogni operazione; usare <see cref="Database.FlushDurability"/> o dispose.
    /// </summary>
    Buffered,
}
