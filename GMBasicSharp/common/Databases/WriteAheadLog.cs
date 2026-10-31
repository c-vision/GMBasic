// FILE: Basic.Databases.WriteAheadLog.cs
using System.Collections.Concurrent;
using System.Text;

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases;

/// <summary>
/// Definisce i tipi di operazione che possono essere registrati nel Write-Ahead Log (WAL).
/// Ogni operazione ha un codice byte univoco per l'identificazione durante il processo di recupero.
/// </summary>
internal enum WalOperationType : byte
{
    /// <summary>
    /// Rappresenta un'operazione di scrittura di un intero record di dati.
    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
    /// </summary>
    WriteRecord = 1,

    /// <summary>
    /// Rappresenta un'operazione di scrittura di un intero nodo B+Tree.
    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
    /// </summary>
    WriteNode = 2,

    /// <summary>
    /// Rappresenta un'operazione di aggiornamento dell'header del file di database.
    /// Formato: [OpCode(1)] [Offset(8, sempre 0)] [Lunghezza(4)] [Dati(...)]
    /// </summary>
    UpdateHeader = 3,

    /// <summary>
    /// Rappresenta un'operazione di scrittura di un blocco arbitrario di byte a un offset specifico.
    /// Usato per aggiornamenti parziali come i metadati di un record.
    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
    /// </summary>
    WriteBytes = 6,

    /// <summary>
    /// Segnala l'inizio di una transazione. Tutte le operazioni successive appartengono a questa transazione
    /// fino a quando non viene incontrato un marcatore CommitTransaction.
    /// Formato: [OpCode(1)]
    /// </summary>
    BeginTransaction = 10,

    /// <summary>
    /// Segnala il completamento con successo di una transazione. Solo le operazioni all'interno di un blocco
    /// Begin/Commit verranno ripristinate in caso di recupero.
    /// Formato: [OpCode(1)]
    /// </summary>
    CommitTransaction = 11,

    /// <summary>
    /// Snapshot dell'header e della mappa indici secondari all'inizio di una transazione.
    /// Formato: [OpCode(1)] [HeaderLen(4)] [Header(...)] [MapLen(4)] [Map(...)]
    /// </summary>
    TransactionSnapshot = 12,

    /// <summary>
    /// Scrittura diretta auto-commit (durabilità Full): loggata prima del file principale, replay se manca checkpoint.
    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
    /// </summary>
    DirectWrite = 7,

    /// <summary>
    /// Marcatore fine sessione auto-commit sincronizzata su disco. Se assente al recover, si replayano le DirectWrite.
    /// Formato: [OpCode(1)]
    /// </summary>
    DirectCheckpoint = 8,
}

/// <summary>
/// Gestisce la scrittura sequenziale delle operazioni di modifica su un file di log prima che vengano
/// applicate al file di database principale. Questa classe è il fulcro per garantire le proprietà
/// di Atomicità e Durabilità (A e D di ACID).
/// </summary>
internal class WriteAheadLog : IDisposable
{
    private static readonly ConcurrentDictionary<string, object> WalLocks = new(StringComparer.OrdinalIgnoreCase);
    private static readonly ConcurrentDictionary<string, Mutex> CrossProcessWalMutexes =
        new(StringComparer.OrdinalIgnoreCase);

    private readonly object _intraProcessWalLock;
    private readonly Mutex _crossProcessWalMutex;
    private FileStream _logStream = null!;
    private BinaryWriter _writer = null!;
    private readonly string _walFilePath;
    private bool _disposed;

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="WriteAheadLog"/>, creando o aprendo
    /// il file .wal corrispondente al file di database specificato.
    /// </summary>
    /// <param name="dbFilePath">Il percorso del file di database principale.</param>
    public WriteAheadLog(string dbFilePath)
    {
        _walFilePath = Path.ChangeExtension(dbFilePath, ".wal");
        _intraProcessWalLock = WalLocks.GetOrAdd(_walFilePath, static _ => new object());
        _crossProcessWalMutex = GetOrCreateCrossProcessWalMutex(_walFilePath);
        _crossProcessWalMutex.WaitOne();
        try
        {
            lock (_intraProcessWalLock)
            {
                _logStream = new FileStream(_walFilePath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite, 4096,
                    useAsync: false);
                _writer = new BinaryWriter(_logStream, Encoding.UTF8, leaveOpen: true);
            }
        }
        finally
        {
            _crossProcessWalMutex.ReleaseMutex();
        }
    }

    private static Mutex GetOrCreateCrossProcessWalMutex(string walPath)
    {
        return CrossProcessWalMutexes.GetOrAdd(walPath, static path =>
        {
            var name = "GMBASIC_WAL_" + Convert.ToBase64String(
                    Encoding.UTF8.GetBytes(Path.GetFullPath(path).ToUpperInvariant()))
                .Replace('/', '_').Replace('+', '-');
            if (OperatingSystem.IsWindows())
            {
                name = $"Global\\{name}";
            }

            return new Mutex(false, name);
        });
    }

    private void EnsureOpen()
    {
        if (!_disposed)
        {
            return;
        }

        _logStream = new FileStream(_walFilePath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite, 4096,
            useAsync: false);
        _writer = new BinaryWriter(_logStream, Encoding.UTF8, leaveOpen: true);
        _disposed = false;
    }

    private void WithWalLock(Action action)
    {
        _crossProcessWalMutex.WaitOne();
        try
        {
            lock (_intraProcessWalLock)
            {
                EnsureOpen();
                action();
            }
        }
        finally
        {
            _crossProcessWalMutex.ReleaseMutex();
        }
    }

    /// <summary>
    /// Registra un marcatore di inizio transazione nel log.
    /// </summary>
    public void LogBeginTransaction()
    {
        WithWalLock(() => _writer.Write((byte)WalOperationType.BeginTransaction));
    }

    /// <summary>
    /// Registra lo snapshot di header e mappa indici all'inizio di una transazione.
    /// </summary>
    public void LogTransactionSnapshot(byte[] headerData, byte[] indexMapData)
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.TransactionSnapshot);
            _writer.Write(headerData.Length);
            _writer.Write(headerData);
            _writer.Write(indexMapData.Length);
            _writer.Write(indexMapData);
        });
    }

    /// <summary>
    /// Registra un marcatore di commit della transazione nel log e forza la scrittura dei buffer
    /// su disco per garantire la durabilità.
    /// </summary>
    public void LogCommitTransaction()
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.CommitTransaction);
            FlushInternal();
        });
    }

    /// <summary>
    /// Registra un'operazione di scrittura di un record di dati nel log.
    /// </summary>
    /// <param name="offset">L'offset fisico nel file di database dove il record deve essere scritto.</param>
    /// <param name="recordData">L'array di byte che rappresenta il record completo.</param>
    public void LogWriteRecord(long offset, byte[] recordData)
    {
        LogWriteRecord(offset, recordData.AsSpan());
    }

    /// <summary>
    /// Registra un'operazione di scrittura di un record di dati nel log.
    /// </summary>
    /// <param name="offset">L'offset fisico nel file di database dove il record deve essere scritto.</param>
    /// <param name="recordData">I byte del record completo.</param>
    public void LogWriteRecord(long offset, ReadOnlySpan<byte> recordData)
    {
        var payload = recordData.ToArray();
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.WriteRecord);
            _writer.Write(offset);
            _writer.Write(payload.Length);
            _writer.Write(payload);
        });
    }

    /// <summary>
    /// Registra un'operazione di scrittura di un nodo B+Tree nel log.
    /// </summary>
    /// <param name="offset">L'offset fisico nel file di database dove il nodo deve essere scritto.</param>
    /// <param name="nodeData">L'array di byte che rappresenta il nodo serializzato.</param>
    public void LogWriteNode(long offset, byte[] nodeData)
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.WriteNode);
            _writer.Write(offset);
            _writer.Write(nodeData.Length);
            _writer.Write(nodeData);
        });
    }

    /// <summary>
    /// Registra un'operazione di aggiornamento dell'header del file di database nel log.
    /// </summary>
    /// <param name="headerData">L'array di byte che rappresenta l'header completo.</param>
    public void LogUpdateHeader(byte[] headerData)
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.UpdateHeader);
            _writer.Write(0L);
            _writer.Write(headerData.Length);
            _writer.Write(headerData);
        });
    }

    /// <summary>
    /// Inspects the on-disk WAL for an ISAM database without modifying it.
    /// </summary>
    /// <param name="dbFilePath">Path to the main .isdb file.</param>
    public static WalLogSummary Inspect(string dbFilePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(dbFilePath);
        var walPath = Path.ChangeExtension(dbFilePath, ".wal");
        if (!File.Exists(walPath))
        {
            return new WalLogSummary
            {
                DatabasePath = dbFilePath,
                WalPath = walPath,
                Exists = false,
                SizeBytes = 0,
                LastPosition = 0,
                PendingRecords = 0,
                UncommittedTransactionOps = 0,
                CommittedTransactionBlocks = 0,
                DirectWritesPending = 0,
                EndsWithCheckpoint = false,
                InOpenTransaction = false,
            };
        }

        var walInfo = new FileInfo(walPath);
        if (walInfo.Length == 0)
        {
            return new WalLogSummary
            {
                DatabasePath = dbFilePath,
                WalPath = walPath,
                Exists = true,
                SizeBytes = 0,
                LastPosition = 0,
                PendingRecords = 0,
                UncommittedTransactionOps = 0,
                CommittedTransactionBlocks = 0,
                DirectWritesPending = 0,
                EndsWithCheckpoint = false,
                InOpenTransaction = false,
            };
        }

        var committedBlocks = 0;
        var activeTransactionOps = 0;
        var directWrites = 0;
        var pendingRecords = 0;
        var inTransaction = false;
        var endsWithCheckpoint = false;
        long lastPosition;

        using (var walStream = new FileStream(walPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
        using (var reader = new BinaryReader(walStream, Encoding.UTF8))
        {
            lastPosition = walStream.Length;
            try
            {
                while (reader.BaseStream.Position < reader.BaseStream.Length)
                {
                    var opType = (WalOperationType)reader.ReadByte();
                    switch (opType)
                    {
                        case WalOperationType.TransactionSnapshot:
                            var headerLen = reader.ReadInt32();
                            if (headerLen < 0 || headerLen > reader.BaseStream.Length - reader.BaseStream.Position)
                            {
                                break;
                            }

                            reader.ReadBytes(headerLen);
                            var mapLen = reader.ReadInt32();
                            if (mapLen < 0 || mapLen > reader.BaseStream.Length - reader.BaseStream.Position)
                            {
                                break;
                            }

                            reader.ReadBytes(mapLen);
                            break;

                        case WalOperationType.BeginTransaction:
                            activeTransactionOps = 0;
                            inTransaction = true;
                            break;

                        case WalOperationType.CommitTransaction:
                            if (inTransaction)
                            {
                                committedBlocks++;
                                activeTransactionOps = 0;
                                inTransaction = false;
                            }

                            break;

                        case WalOperationType.DirectCheckpoint:
                            directWrites = 0;
                            endsWithCheckpoint = true;
                            break;

                        case WalOperationType.WriteRecord:
                        case WalOperationType.WriteNode:
                        case WalOperationType.UpdateHeader:
                        case WalOperationType.WriteBytes:
                        case WalOperationType.DirectWrite:
                            var offset = reader.ReadInt64();
                            var length = reader.ReadInt32();
                            if (length < 0 || length > reader.BaseStream.Length - reader.BaseStream.Position)
                            {
                                break;
                            }

                            reader.ReadBytes(length);
                            pendingRecords++;
                            if (inTransaction)
                            {
                                activeTransactionOps++;
                            }
                            else if (opType == WalOperationType.DirectWrite)
                            {
                                directWrites++;
                                endsWithCheckpoint = false;
                            }

                            _ = offset;
                            break;

                        default:
                            break;
                    }
                }
            }
            catch (EndOfStreamException)
            {
                lastPosition = reader.BaseStream.Position;
            }
        }

        return new WalLogSummary
        {
            DatabasePath = dbFilePath,
            WalPath = walPath,
            Exists = true,
            SizeBytes = walInfo.Length,
            LastPosition = lastPosition,
            PendingRecords = pendingRecords,
            UncommittedTransactionOps = activeTransactionOps,
            CommittedTransactionBlocks = committedBlocks,
            DirectWritesPending = directWrites,
            EndsWithCheckpoint = endsWithCheckpoint,
            InOpenTransaction = inTransaction,
        };
    }

    /// <summary>
    /// Esegue il processo di recupero (roll-forward) riproducendo le operazioni da un file WAL
    /// sul file di database principale. Questo metodo garantisce che il database venga riportato
    /// a uno stato consistente dopo un crash, applicando solo le transazioni completamente commesse.
    /// </summary>
    /// <param name="dbFilePath">Il percorso del file di database da recuperare.</param>
    /// <remarks>
    /// Questa è un'operazione critica per l'atomicità e la durabilità (ACID). La logica opera in due fasi:
    /// 1. Analisi del WAL: Il log viene letto per intero. Solo le operazioni all'interno di un blocco
    ///    `BeginTransaction`/`CommitTransaction` vengono considerate valide per il recupero.
    /// 2. Applicazione: Le operazioni validate vengono scritte in sequenza sul file di database principale.
    /// Al termine, il file WAL viene eliminato.
    /// </remarks>
    public static void Recover(string dbFilePath)
    {
        var walPath = Path.ChangeExtension(dbFilePath, ".wal");
        var walMutex = GetOrCreateCrossProcessWalMutex(walPath);
        walMutex.WaitOne();
        try
        {
            RecoverCore(dbFilePath, walPath);
        }
        finally
        {
            walMutex.ReleaseMutex();
        }
    }

    private static void RecoverCore(string dbFilePath, string walPath)
    {
        if (!File.Exists(walPath))
        {
            return; // Nessun recupero necessario.
        }

        var walInfo = new FileInfo(walPath);
        if (walInfo.Length == 0)
        {
            File.Delete(walPath);
            return;
        }

        var committedTransactions = new List<List<(WalOperationType op, long offset, byte[] data)>>();
        var activeTransactionOps = new List<(WalOperationType op, long offset, byte[] data)>();
        var directWrites = new List<(long offset, byte[] data)>();
        var inTransaction = false;
        var endsWithDirectCheckpoint = false;
        byte[]? pendingSnapshotHeader = null;
        byte[]? pendingSnapshotMap = null;
        byte[]? openTransactionSnapshotHeader = null;
        byte[]? openTransactionSnapshotMap = null;

        using (var walStream = new FileStream(walPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
        using (var reader = new BinaryReader(walStream, Encoding.UTF8))
        {
            try
            {
                while (reader.BaseStream.Position < reader.BaseStream.Length)
                {
                    var opType = (WalOperationType)reader.ReadByte();
                    switch (opType)
                    {
                        case WalOperationType.TransactionSnapshot:
                            var headerLen = reader.ReadInt32();
                            if (headerLen < 0 || headerLen > reader.BaseStream.Length - reader.BaseStream.Position)
                            {
                                break;
                            }

                            pendingSnapshotHeader = reader.ReadBytes(headerLen);
                            var mapLen = reader.ReadInt32();
                            if (mapLen < 0 || mapLen > reader.BaseStream.Length - reader.BaseStream.Position)
                            {
                                break;
                            }

                            pendingSnapshotMap = reader.ReadBytes(mapLen);
                            break;

                        case WalOperationType.BeginTransaction:
                            activeTransactionOps.Clear();
                            inTransaction = true;
                            openTransactionSnapshotHeader = pendingSnapshotHeader;
                            openTransactionSnapshotMap = pendingSnapshotMap;
                            pendingSnapshotHeader = null;
                            pendingSnapshotMap = null;
                            break;

                        case WalOperationType.CommitTransaction:
                            if (inTransaction)
                            {
                                committedTransactions.Add([..activeTransactionOps]);
                                activeTransactionOps.Clear();
                                inTransaction = false;
                                openTransactionSnapshotHeader = null;
                                openTransactionSnapshotMap = null;
                            }

                            break;

                        case WalOperationType.DirectCheckpoint:
                            directWrites.Clear();
                            endsWithDirectCheckpoint = true;
                            break;

                        case WalOperationType.WriteRecord:
                        case WalOperationType.WriteNode:
                        case WalOperationType.UpdateHeader:
                        case WalOperationType.WriteBytes:
                        case WalOperationType.DirectWrite:
                            var offset = reader.ReadInt64();
                            var length = reader.ReadInt32();
                            if (length < 0 || length > reader.BaseStream.Length - reader.BaseStream.Position)
                            {
                                break;
                            }

                            var data = reader.ReadBytes(length);
                            var operation = (opType, offset, data);

                            if (inTransaction)
                            {
                                activeTransactionOps.Add(operation);
                            }
                            else if (opType == WalOperationType.DirectWrite)
                            {
                                directWrites.Add((offset, data));
                                endsWithDirectCheckpoint = false;
                            }

                            break;

                        default:
                            break;
                    }
                }
            }
            catch (EndOfStreamException)
            {
                // ponytail: WAL tail troncato — uscita dal loop, replay parziale già raccolto
            }
        }

        using (var dbStream = new FileStream(dbFilePath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite))
        {
            if (committedTransactions.Count > 0)
            {
                foreach (var transaction in committedTransactions)
                {
                    foreach (var (_, offset, data) in transaction)
                    {
                        dbStream.Seek(offset, SeekOrigin.Begin);
                        dbStream.Write(data, 0, data.Length);
                    }
                }
            }

            if (!endsWithDirectCheckpoint)
            {
                directWrites.Clear();
            }
            else
            {
                foreach (var (offset, data) in directWrites)
                {
                    dbStream.Seek(offset, SeekOrigin.Begin);
                    dbStream.Write(data, 0, data.Length);
                }
            }

            if (inTransaction && openTransactionSnapshotHeader is { Length: > 0 } &&
                openTransactionSnapshotMap is { Length: > 0 })
            {
                dbStream.Seek(0, SeekOrigin.Begin);
                dbStream.Write(openTransactionSnapshotHeader, 0, openTransactionSnapshotHeader.Length);
                if (openTransactionSnapshotHeader.Length >= 44)
                {
                    var mapOffset = BitConverter.ToInt64(openTransactionSnapshotHeader, 36);
                    if (mapOffset > 0)
                    {
                        dbStream.Seek(mapOffset, SeekOrigin.Begin);
                        dbStream.Write(openTransactionSnapshotMap, 0, openTransactionSnapshotMap.Length);
                    }
                }
            }

            dbStream.Flush(true);
        }

        File.Delete(walPath);
    }

    /// <summary>
    /// Registra un'operazione di scrittura di un blocco di byte arbitrario nel log.
    /// </summary>
    /// <param name="offset">L'offset fisico nel file di database dove i byte devono essere scritti.</param>
    /// <param name="data">L'array di byte da scrivere.</param>
    public void LogWriteBytes(long offset, byte[] data)
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.WriteBytes);
            _writer.Write(offset);
            _writer.Write(data.Length);
            _writer.Write(data);
        });
    }

    /// <summary>
    /// Registra una scrittura auto-commit da replayare in recovery se manca <see cref="WalOperationType.DirectCheckpoint"/>.
    /// </summary>
    public void LogDirectWrite(long offset, byte[] data)
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.DirectWrite);
            _writer.Write(offset);
            _writer.Write(data.Length);
            _writer.Write(data);
        });
    }

    /// <summary>
    /// Segnala che tutte le DirectWrite precedenti sono state applicate e sincronizzate sul file principale.
    /// </summary>
    public void LogDirectCheckpoint()
    {
        WithWalLock(() =>
        {
            _writer.Write((byte)WalOperationType.DirectCheckpoint);
            FlushInternal();
        });
    }

    /// <summary>
    /// Sincronizza i buffer del log su disco per garantire che le operazioni registrate siano persistenti.
    /// </summary>
    public void Flush()
    {
        WithWalLock(FlushInternal);
    }

    private void FlushInternal()
    {
        _writer.Flush();
        if (_logStream.CanWrite)
        {
            _logStream.Flush(true);
        }
    }

    /// <summary>
    /// Rilascia le risorse gestite, assicurando che tutti i dati del log siano scritti su disco.
    /// </summary>
    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        WithWalLock(() =>
        {
            FlushInternal();
            _writer.Dispose();
            _logStream.Dispose();
        });
        _disposed = true;
    }

    /// <summary>
    /// Trunca il WAL in-place dopo un commit andato a buon fine, mantenendo lo stream aperto per ulteriori scritture.
    /// </summary>
    /// <param name="skipFlush">True se il chiamante ha già fsyncato (es. LogDirectCheckpoint).</param>
    public void Delete(bool skipFlush = false)
    {
        if (_disposed)
        {
            if (File.Exists(_walFilePath))
            {
                File.Delete(_walFilePath);
            }

            return;
        }

        WithWalLock(() =>
        {
            if (_disposed)
            {
                return;
            }

            if (!skipFlush)
            {
                FlushInternal();
            }

            _writer.Dispose();
            _logStream.Dispose();
            _disposed = true;

            if (File.Exists(_walFilePath))
            {
                File.Delete(_walFilePath);
            }
        });
    }
}