// FILE: Basic.ISAM.IsamBackupRestore

using System.Text;
using System.Text.Json;
using Basic.Tools.Isam;
using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Backup/restore file-level MVP (Phase G4): copia <c>.isdb</c> + <c>.wal</c> con manifest fingerprint.
/// </summary>
public static class IsamBackupRestore
{
    private const string ManifestFileName = "manifest.json";
    private const string DatabaseCopyName = "database.isdb";
    private const string WalCopyName = "database.wal";

    /// <summary>
    /// Crea backup incrementale file-level nella cartella destinazione.
    /// Se header fingerprint e WAL fingerprint coincidono con l'ultimo manifest, salta la copia.
    /// Se solo il WAL è cambiato, copia solo <c>database.wal</c> (non delta a blocchi).
    /// </summary>
    /// <returns><see langword="true"/> se i file sono stati copiati; <see langword="false"/> se già aggiornato.</returns>
    public static bool BackupIncremental(string sourceIsdb, string destinationDirectory)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(sourceIsdb);
        ArgumentException.ThrowIfNullOrWhiteSpace(destinationDirectory);

        if (!File.Exists(sourceIsdb))
        {
            throw new FileNotFoundException("ISAM file not found.", sourceIsdb);
        }

        Directory.CreateDirectory(destinationDirectory);
        var snapshot = IsamOfflineToolkit.ReadSnapshot(sourceIsdb);
        var fingerprint = BuildFingerprint(sourceIsdb, snapshot.Header);
        var walSource = Path.ChangeExtension(sourceIsdb, ".wal");
        var walDest = Path.Combine(destinationDirectory, WalCopyName);
        var walFingerprint = BuildWalFingerprint(walSource);
        var previous = ReadLatestManifest(destinationDirectory);

        if (previous is not null && FingerprintsEqual(previous.Fingerprint, fingerprint))
        {
            if (WalFingerprintsEqual(previous.Wal, walFingerprint))
            {
                return false;
            }

            CopyWalOnly(walSource, walDest, destinationDirectory, sourceIsdb, fingerprint, walFingerprint);
            return true;
        }

        var dbDest = Path.Combine(destinationDirectory, DatabaseCopyName);
        File.Copy(sourceIsdb, dbDest, overwrite: true);

        if (File.Exists(walSource))
        {
            File.Copy(walSource, walDest, overwrite: true);
        }
        else if (File.Exists(walDest))
        {
            File.Delete(walDest);
        }

        var manifest = new BackupManifest(
            DateTimeOffset.UtcNow,
            Path.GetFullPath(sourceIsdb),
            fingerprint,
            File.Exists(walSource),
            walFingerprint);

        WriteManifest(destinationDirectory, manifest);
        return true;
    }

    private static void CopyWalOnly(
        string walSource,
        string walDest,
        string destinationDirectory,
        string sourceIsdb,
        BackupFingerprint fingerprint,
        WalFingerprint walFingerprint)
    {
        if (File.Exists(walSource))
        {
            File.Copy(walSource, walDest, overwrite: true);
        }
        else if (File.Exists(walDest))
        {
            File.Delete(walDest);
        }

        var manifest = new BackupManifest(
            DateTimeOffset.UtcNow,
            Path.GetFullPath(sourceIsdb),
            fingerprint,
            File.Exists(walSource),
            walFingerprint);
        WriteManifest(destinationDirectory, manifest);
    }

    /// <summary>
    /// Ripristina <c>.isdb</c> (e <c>.wal</c> se presente) dal backup verso <paramref name="targetIsdb"/>.
    /// </summary>
    public static void Restore(string backupDirectory, string targetIsdb)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(backupDirectory);
        ArgumentException.ThrowIfNullOrWhiteSpace(targetIsdb);

        if (!Directory.Exists(backupDirectory))
        {
            throw new DirectoryNotFoundException($"Backup directory not found: {backupDirectory}");
        }

        var manifest = ReadLatestManifest(backupDirectory)
                       ?? throw new InvalidOperationException("Backup manifest not found.");

        var dbSource = Path.Combine(backupDirectory, DatabaseCopyName);
        if (!File.Exists(dbSource))
        {
            throw new FileNotFoundException("Backup database copy missing.", dbSource);
        }

        var targetDir = Path.GetDirectoryName(Path.GetFullPath(targetIsdb));
        if (!string.IsNullOrEmpty(targetDir))
        {
            Directory.CreateDirectory(targetDir);
        }

        File.Copy(dbSource, targetIsdb, overwrite: true);

        var walTarget = Path.ChangeExtension(targetIsdb, ".wal");
        var walSource = Path.Combine(backupDirectory, WalCopyName);
        if (File.Exists(walSource))
        {
            File.Copy(walSource, walTarget, overwrite: true);
        }
        else if (File.Exists(walTarget))
        {
            File.Delete(walTarget);
        }

        IsamOfflineToolkit.RecoverWal(targetIsdb);
        _ = manifest;
    }

    internal static BackupFingerprint BuildFingerprint(string sourceIsdb, Config.FileHeader header)
    {
        var info = new FileInfo(sourceIsdb);
        return new BackupFingerprint(
            header.ActiveRecordCount,
            header.PrimaryKeyRootOffset,
            info.LastWriteTimeUtc.Ticks,
            info.Length);
    }

    internal static WalFingerprint BuildWalFingerprint(string walPath)
    {
        if (!File.Exists(walPath))
        {
            return new WalFingerprint(0, 0);
        }

        var info = new FileInfo(walPath);
        return new WalFingerprint(info.Length, info.LastWriteTimeUtc.Ticks);
    }

    private static void WriteManifest(string directory, BackupManifest manifest)
    {
        var path = Path.Combine(directory, ManifestFileName);
        var json = JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(path, json, Encoding.UTF8);
    }

    private static BackupManifest? ReadLatestManifest(string directory)
    {
        var path = Path.Combine(directory, ManifestFileName);
        if (!File.Exists(path))
        {
            return null;
        }

        return JsonSerializer.Deserialize<BackupManifest>(File.ReadAllText(path, Encoding.UTF8));
    }

    private static bool FingerprintsEqual(BackupFingerprint a, BackupFingerprint b) =>
        a.ActiveRecordCount == b.ActiveRecordCount
        && a.PrimaryKeyRootOffset == b.PrimaryKeyRootOffset
        && a.SourceLastWriteUtcTicks == b.SourceLastWriteUtcTicks
        && a.SourceFileLength == b.SourceFileLength;

    private static bool WalFingerprintsEqual(WalFingerprint? previous, WalFingerprint current) =>
        previous is not null
        && previous.Length == current.Length
        && previous.LastWriteUtcTicks == current.LastWriteUtcTicks;
}

/// <summary>Fingerprint header per backup incrementale MVP.</summary>
public sealed record BackupFingerprint(
    long ActiveRecordCount,
    long PrimaryKeyRootOffset,
    long SourceLastWriteUtcTicks,
    long SourceFileLength);

/// <summary>Fingerprint file WAL per backup wal-only.</summary>
public sealed record WalFingerprint(long Length, long LastWriteUtcTicks);

/// <summary>Manifest JSON scritto nella cartella backup.</summary>
public sealed record BackupManifest(
    DateTimeOffset CreatedUtc,
    string SourcePath,
    BackupFingerprint Fingerprint,
    bool IncludesWal,
    WalFingerprint? Wal = null);
