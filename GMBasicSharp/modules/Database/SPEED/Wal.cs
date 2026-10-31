// FILE: Basic.Databases.Speed.Wal.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using System.Text;

namespace Basic.Databases.Speed;

/// <summary>
/// Gestisce il Write-Ahead Log per il motore Speed.
/// Tutte le operazioni di modifica vengono prima scritte in modo sequenziale su questo log
/// per garantire la durabilità e il recupero in caso di crash.
/// </summary>
internal class WriteAheadLog : IDisposable
{
    private readonly FileStream _logStream;
    private readonly BinaryWriter _writer;
    private readonly string _walFilePath;

    /// <summary>
    /// Inizializza o apre un file WAL per un dato database.
    /// </summary>
    /// <param name="databasePath">La directory radice del database.</param>
    /// <param name="logSequenceNumber">Il numero sequenziale di questo file di log.</param>
    public WriteAheadLog(string databasePath, ulong logSequenceNumber)
    {
        var walDirectory = Path.Combine(databasePath, Config.WalDirectoryName);
        Directory.CreateDirectory(walDirectory);

        _walFilePath = Path.Combine(walDirectory, $"{logSequenceNumber:D6}.wal");
        _logStream = new FileStream(_walFilePath, FileMode.Append, FileAccess.Write, FileShare.None);
        _writer = new BinaryWriter(_logStream, Encoding.UTF8, false);
    }

    /// <summary>
    /// Restituisce il percorso completo del file di log gestito da questa istanza.
    /// </summary>
    /// <returns>La stringa del percorso del file .wal.</returns>
    public string GetWalPath() => _walFilePath;

    /// <summary>
    /// Registra un'operazione di inserimento/aggiornamento nel log.
    /// </summary>
    public void LogSet(int tableId, byte[] key, byte[] value)
    {
        lock (_writer)
        {
            _writer.Write((byte)WalOperationType.Set);
            _writer.Write(tableId);
            _writer.Write(key.Length);
            _writer.Write(key);
            _writer.Write(value.Length);
            _writer.Write(value);
        }
    }

    /// <summary>
    /// Registra un'operazione di cancellazione nel log.
    /// </summary>
    public void LogDelete(int tableId, byte[] key)
    {
        lock (_writer)
        {
            _writer.Write((byte)WalOperationType.Delete);
            _writer.Write(tableId);
            _writer.Write(key.Length);
            _writer.Write(key);
        }
    }

    /// <summary>
    /// Forza la scrittura di tutti i dati bufferizzati su disco.
    /// </summary>
    public void Flush()
    {
        _logStream.Flush(true);
    }

    /// <summary>
    /// Esegue il recupero ripopolando le Memtable dai file di log esistenti.
    /// </summary>
    /// <param name="databasePath">La directory radice del database.</param>
    /// <param name="memtables">Il dizionario di Memtable da popolare.</param>
    public static void Recover(string databasePath, Dictionary<int, Memtable> memtables)
    {
        var walDirectory = Path.Combine(databasePath, Config.WalDirectoryName);
        if (!Directory.Exists(walDirectory)) return;

        var walFiles = Directory.GetFiles(walDirectory, "*.wal").OrderBy(f => f).ToList();

        foreach (var walFile in walFiles)
        {
            using var stream = new FileStream(walFile, FileMode.Open, FileAccess.Read);
            using var reader = new BinaryReader(stream, Encoding.UTF8);

            while (stream.Position < stream.Length)
            {
                var opType = (WalOperationType)reader.ReadByte();
                var tableId = reader.ReadInt32();

                if (!memtables.TryGetValue(tableId, out var memtable))
                {
                    memtable = new Memtable();
                    memtables[tableId] = memtable;
                }

                var keyLength = reader.ReadInt32();
                var key = reader.ReadBytes(keyLength);

                if (opType == WalOperationType.Set)
                {
                    var valueLength = reader.ReadInt32();
                    var value = reader.ReadBytes(valueLength);
                    memtable.Set(key, value);
                }
                else if (opType == WalOperationType.Delete)
                {
                    memtable.Delete(key);
                }
            }
        }
    }

    public void Dispose()
    {
        _writer.Dispose();
        _logStream.Dispose();
    }
}