// FILE: Basic.DBF.DbfDurabilityMode

namespace Basic.DBF;

/// <summary>
/// Controlla quando header e stream DBF vengono sincronizzati su disco.
/// </summary>
public enum DbfDurabilityMode
{
    /// <summary>
    /// Ogni auto-commit esegue fsync (comportamento legacy).
    /// </summary>
    Full,

    /// <summary>
    /// Scrive su disco senza fsync ad ogni commit; usare <see cref="DbfEngine.FlushDurability"/> o dispose.
    /// </summary>
    Buffered,
}
