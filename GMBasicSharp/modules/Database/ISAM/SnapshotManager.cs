// FILE: Basic.ISAM.SnapshotManager.cs

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.ISAM;

/// <summary>
/// Fornisce la funzionalità per creare una copia di backup sicura e consistente
/// di un file di database ISAM.
/// </summary>
internal static class SnapshotManager
{
    /// <summary>
    /// Crea uno snapshot consistente di un file di database ISAM.
    /// La procedura garantisce che il backup sia atomico e non corrotto,
    /// gestendo il flush dello stream e il locking del file.
    /// </summary>
    /// <param name="handle">L'handle del file ISAM aperto di cui fare il backup.</param>
    /// <param name="backupFilePath">Il percorso completo del file di backup da creare.</param>
    public static void CreateSnapshot(IsamFileHandle handle, string backupFilePath)
    {
        handle.LockFile();
        try
        {
            if (IsamMemPath.IsMemPath(handle.FilePath))
            {
                throw new NotSupportedException("MEM: tables cannot be snapshotted via file copy.");
            }

            handle.FlushPhysical(true);
            File.Copy(handle.FilePath, backupFilePath, overwrite: true);
        }
        finally
        {
            handle.UnlockFile();
        }
    }
}
