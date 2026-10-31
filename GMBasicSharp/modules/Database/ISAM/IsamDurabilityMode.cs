// FILE: Basic.ISAM.IsamDurabilityMode

using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Controlla quando header e stream vengono sincronizzati su disco.
/// </summary>
public enum IsamDurabilityMode
{
    /// <summary>
    /// Ogni <see cref="IsamFileHandle.Commit"/> esegue fsync (comportamento legacy).
    /// </summary>
    Full,

    /// <summary>
    /// Scrive header e dati su disco senza fsync ad ogni commit; usare <see cref="IsamFileHandle.FlushDurability"/> o dispose.
    /// </summary>
    Buffered,
}
