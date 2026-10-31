/* FILE: native/gmb_db_c/src/Databases/WriteAheadLog.c */
/* Port 1:1 di common/Databases/WriteAheadLog.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.WriteAheadLog.cs
//using System.Collections.Concurrent;
//using System.Text;
//
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases;
//
///// <summary>
///// Definisce i tipi di operazione che possono essere registrati nel Write-Ahead Log (WAL).
///// Ogni operazione ha un codice byte univoco per l'identificazione durante il processo di recupero.
///// </summary>
//internal enum WalOperationType : byte
//{
//    /// <summary>
//    /// Rappresenta un'operazione di scrittura di un intero record di dati.
//    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
//    /// </summary>
//    WriteRecord = 1,
//
//    /// <summary>
//    /// Rappresenta un'operazione di scrittura di un intero nodo B+Tree.
//    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
//    /// </summary>
//    WriteNode = 2,
//
//    /// <summary>
//    /// Rappresenta un'operazione di aggiornamento dell'header del file di database.
//    /// Formato: [OpCode(1)] [Offset(8, sempre 0)] [Lunghezza(4)] [Dati(...)]
//    /// </summary>
//    UpdateHeader = 3,
//
//    /// <summary>
//    /// Rappresenta un'operazione di scrittura di un blocco arbitrario di byte a un offset specifico.
//    /// Usato per aggiornamenti parziali come i metadati di un record.
//    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
//    /// </summary>
//    WriteBytes = 6,
//
//    /// <summary>
//    /// Segnala l'inizio di una transazione. Tutte le operazioni successive appartengono a questa transazione
//    /// fino a quando non viene incontrato un marcatore CommitTransaction.
//    /// Formato: [OpCode(1)]
//    /// </summary>
//    BeginTransaction = 10,
//
//    /// <summary>
//    /// Segnala il completamento con successo di una transazione. Solo le operazioni all'interno di un blocco
//    /// Begin/Commit verranno ripristinate in caso di recupero.
//    /// Formato: [OpCode(1)]
//    /// </summary>
//    CommitTransaction = 11,
//
//    /// <summary>
//    /// Snapshot dell'header e della mappa indici secondari all'inizio di una transazione.
//    /// Formato: [OpCode(1)] [HeaderLen(4)] [Header(...)] [MapLen(4)] [Map(...)]
//    /// </summary>
//    TransactionSnapshot = 12,
//
//    /// <summary>
//    /// Scrittura diretta auto-commit (durabilità Full): loggata prima del file principale, replay se manca checkpoint.
//    /// Formato: [OpCode(1)] [Offset(8)] [Lunghezza(4)] [Dati(...)]
//    /// </summary>
//    DirectWrite = 7,
//
//    /// <summary>
//    /// Marcatore fine sessione auto-commit sincronizzata su disco. Se assente al recover, si replayano le DirectWrite.
//    /// Formato: [OpCode(1)]
//    /// </summary>
//    DirectCheckpoint = 8,
//}
//
///// <summary>
///// Gestisce la scrittura sequenziale delle operazioni di modifica su un file di log prima che vengano
///// applicate al file di database principale. Questa classe è il fulcro per garantire le proprietà
///// di Atomicità e Durabilità (A e D di ACID).
///// </summary>
//internal class WriteAheadLog : IDisposable
//{
//    private static readonly ConcurrentDictionary<string, object> WalLocks = new(StringComparer.OrdinalIgnoreCase);
//    private static readonly ConcurrentDictionary<string, Mutex> CrossProcessWalMutexes =
//        new(StringComparer.OrdinalIgnoreCase);
//
//    private readonly object _intraProcessWalLock;
//    private readonly Mutex _crossProcessWalMutex;
//    private FileStream _logStream = null!;
//    private BinaryWriter _writer = null!;
//    private readonly string _walFilePath;
//    private bool _disposed;
//
//    /// <summary>
//    /// Inizializza una nuova istanza della classe <see cref="WriteAheadLog"/>, creando o aprendo
//    /// il file .wal corrispondente al file di database specificato.
//    /// </summary>
//    /// <param name="dbFilePath">Il percorso del file di database principale.</param>
//    public WriteAheadLog(string dbFilePath)
//    {
//        _walFilePath = Path.ChangeExtension(dbFilePath, ".wal");
//        _intraProcessWalLock = WalLocks.GetOrAdd(_walFilePath, static _ => new object());
//        _crossProcessWalMutex = GetOrCreateCrossProcessWalMutex(_walFilePath);
//        _crossProcessWalMutex.WaitOne();
//        try
//        {
//            lock (_intraProcessWalLock)
//            {
//                _logStream = new FileStream(_walFilePath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite, 4096,
//                    useAsync: false);
//                _writer = new BinaryWriter(_logStream, Encoding.UTF8, leaveOpen: true);
//            }
//        }
//        finally
//        {
//            _crossProcessWalMutex.ReleaseMutex();
//        }
//    }
//
//    private static Mutex GetOrCreateCrossProcessWalMutex(string walPath)
//    {
//        return CrossProcessWalMutexes.GetOrAdd(walPath, static path =>
//        {
//            var name = "GMBASIC_WAL_" + Convert.ToBase64String(
//                    Encoding.UTF8.GetBytes(Path.GetFullPath(path).ToUpperInvariant()))
//                .Replace('/', '_').Replace('+', '-');
//            if (OperatingSystem.IsWindows())
//            {
//                name = $"Global\\{name}";
//            }
//
//            return new Mutex(false, name);
//        });
//    }
//
//    private void EnsureOpen()
//    {
//        if (!_disposed)
//        {
//            return;
//        }
//
//        _logStream = new FileStream(_walFilePath, FileMode.Append, FileAccess.Write, FileShare.ReadWrite, 4096,
//            useAsync: false);
//        _writer = new BinaryWriter(_logStream, Encoding.UTF8, leaveOpen: true);
//        _disposed = false;
//    }
//
//    private void WithWalLock(Action action)
//    {
//        _crossProcessWalMutex.WaitOne();
//        try
//        {
//            lock (_intraProcessWalLock)
//            {
//                EnsureOpen();
//                action();
//            }
//        }
//        finally
//        {
//            _crossProcessWalMutex.ReleaseMutex();
//        }
//    }
//
//    /// <summary>
//    /// Registra un marcatore di inizio transazione nel log.
//    /// </summary>
//    public void LogBeginTransaction()
//    {
//        WithWalLock(() => _writer.Write((byte)WalOperationType.BeginTransaction));
//    }
//
//    /// <summary>
//    /// Registra lo snapshot di header e mappa indici all'inizio di una transazione.
//    /// </summary>
//    public void LogTransactionSnapshot(byte[] headerData, byte[] indexMapData)
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.TransactionSnapshot);
//            _writer.Write(headerData.Length);
//            _writer.Write(headerData);
//            _writer.Write(indexMapData.Length);
//            _writer.Write(indexMapData);
//        });
//    }
//
//    /// <summary>
//    /// Registra un marcatore di commit della transazione nel log e forza la scrittura dei buffer
//    /// su disco per garantire la durabilità.
//    /// </summary>
//    public void LogCommitTransaction()
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.CommitTransaction);
//            FlushInternal();
//        });
//    }
//
//    /// <summary>
//    /// Registra un'operazione di scrittura di un record di dati nel log.
//    /// </summary>
//    /// <param name="offset">L'offset fisico nel file di database dove il record deve essere scritto.</param>
//    /// <param name="recordData">L'array di byte che rappresenta il record completo.</param>
//    public void LogWriteRecord(long offset, byte[] recordData)
//    {
//        LogWriteRecord(offset, recordData.AsSpan());
//    }
//
//    /// <summary>
//    /// Registra un'operazione di scrittura di un record di dati nel log.
//    /// </summary>
//    /// <param name="offset">L'offset fisico nel file di database dove il record deve essere scritto.</param>
//    /// <param name="recordData">I byte del record completo.</param>
//    public void LogWriteRecord(long offset, ReadOnlySpan<byte> recordData)
//    {
//        var payload = recordData.ToArray();
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.WriteRecord);
//            _writer.Write(offset);
//            _writer.Write(payload.Length);
//            _writer.Write(payload);
//        });
//    }
//
//    /// <summary>
//    /// Registra un'operazione di scrittura di un nodo B+Tree nel log.
//    /// </summary>
//    /// <param name="offset">L'offset fisico nel file di database dove il nodo deve essere scritto.</param>
//    /// <param name="nodeData">L'array di byte che rappresenta il nodo serializzato.</param>
//    public void LogWriteNode(long offset, byte[] nodeData)
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.WriteNode);
//            _writer.Write(offset);
//            _writer.Write(nodeData.Length);
//            _writer.Write(nodeData);
//        });
//    }
//
//    /// <summary>
//    /// Registra un'operazione di aggiornamento dell'header del file di database nel log.
//    /// </summary>
//    /// <param name="headerData">L'array di byte che rappresenta l'header completo.</param>
//    public void LogUpdateHeader(byte[] headerData)
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.UpdateHeader);
//            _writer.Write(0L);
//            _writer.Write(headerData.Length);
//            _writer.Write(headerData);
//        });
//    }
//
//    /// <summary>
//    /// Inspects the on-disk WAL for an ISAM database without modifying it.
//    /// </summary>
//    /// <param name="dbFilePath">Path to the main .isdb file.</param>
//    public static WalLogSummary Inspect(string dbFilePath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(dbFilePath);
//        var walPath = Path.ChangeExtension(dbFilePath, ".wal");
//        if (!File.Exists(walPath))
//        {
//            return new WalLogSummary
//            {
//                DatabasePath = dbFilePath,
//                WalPath = walPath,
//                Exists = false,
//                SizeBytes = 0,
//                LastPosition = 0,
//                PendingRecords = 0,
//                UncommittedTransactionOps = 0,
//                CommittedTransactionBlocks = 0,
//                DirectWritesPending = 0,
//                EndsWithCheckpoint = false,
//                InOpenTransaction = false,
//            };
//        }
//
//        var walInfo = new FileInfo(walPath);
//        if (walInfo.Length == 0)
//        {
//            return new WalLogSummary
//            {
//                DatabasePath = dbFilePath,
//                WalPath = walPath,
//                Exists = true,
//                SizeBytes = 0,
//                LastPosition = 0,
//                PendingRecords = 0,
//                UncommittedTransactionOps = 0,
//                CommittedTransactionBlocks = 0,
//                DirectWritesPending = 0,
//                EndsWithCheckpoint = false,
//                InOpenTransaction = false,
//            };
//        }
//
//        var committedBlocks = 0;
//        var activeTransactionOps = 0;
//        var directWrites = 0;
//        var pendingRecords = 0;
//        var inTransaction = false;
//        var endsWithCheckpoint = false;
//        long lastPosition;
//
//        using (var walStream = new FileStream(walPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
//        using (var reader = new BinaryReader(walStream, Encoding.UTF8))
//        {
//            lastPosition = walStream.Length;
//            try
//            {
//                while (reader.BaseStream.Position < reader.BaseStream.Length)
//                {
//                    var opType = (WalOperationType)reader.ReadByte();
//                    switch (opType)
//                    {
//                        case WalOperationType.TransactionSnapshot:
//                            var headerLen = reader.ReadInt32();
//                            if (headerLen < 0 || headerLen > reader.BaseStream.Length - reader.BaseStream.Position)
//                            {
//                                break;
//                            }
//
//                            reader.ReadBytes(headerLen);
//                            var mapLen = reader.ReadInt32();
//                            if (mapLen < 0 || mapLen > reader.BaseStream.Length - reader.BaseStream.Position)
//                            {
//                                break;
//                            }
//
//                            reader.ReadBytes(mapLen);
//                            break;
//
//                        case WalOperationType.BeginTransaction:
//                            activeTransactionOps = 0;
//                            inTransaction = true;
//                            break;
//
//                        case WalOperationType.CommitTransaction:
//                            if (inTransaction)
//                            {
//                                committedBlocks++;
//                                activeTransactionOps = 0;
//                                inTransaction = false;
//                            }
//
//                            break;
//
//                        case WalOperationType.DirectCheckpoint:
//                            directWrites = 0;
//                            endsWithCheckpoint = true;
//                            break;
//
//                        case WalOperationType.WriteRecord:
//                        case WalOperationType.WriteNode:
//                        case WalOperationType.UpdateHeader:
//                        case WalOperationType.WriteBytes:
//                        case WalOperationType.DirectWrite:
//                            var offset = reader.ReadInt64();
//                            var length = reader.ReadInt32();
//                            if (length < 0 || length > reader.BaseStream.Length - reader.BaseStream.Position)
//                            {
//                                break;
//                            }
//
//                            reader.ReadBytes(length);
//                            pendingRecords++;
//                            if (inTransaction)
//                            {
//                                activeTransactionOps++;
//                            }
//                            else if (opType == WalOperationType.DirectWrite)
//                            {
//                                directWrites++;
//                                endsWithCheckpoint = false;
//                            }
//
//                            _ = offset;
//                            break;
//
//                        default:
//                            break;
//                    }
//                }
//            }
//            catch (EndOfStreamException)
//            {
//                lastPosition = reader.BaseStream.Position;
//            }
//        }
//
//        return new WalLogSummary
//        {
//            DatabasePath = dbFilePath,
//            WalPath = walPath,
//            Exists = true,
//            SizeBytes = walInfo.Length,
//            LastPosition = lastPosition,
//            PendingRecords = pendingRecords,
//            UncommittedTransactionOps = activeTransactionOps,
//            CommittedTransactionBlocks = committedBlocks,
//            DirectWritesPending = directWrites,
//            EndsWithCheckpoint = endsWithCheckpoint,
//            InOpenTransaction = inTransaction,
//        };
//    }
//
//    /// <summary>
//    /// Esegue il processo di recupero (roll-forward) riproducendo le operazioni da un file WAL
//    /// sul file di database principale. Questo metodo garantisce che il database venga riportato
//    /// a uno stato consistente dopo un crash, applicando solo le transazioni completamente commesse.
//    /// </summary>
//    /// <param name="dbFilePath">Il percorso del file di database da recuperare.</param>
//    /// <remarks>
//    /// Questa è un'operazione critica per l'atomicità e la durabilità (ACID). La logica opera in due fasi:
//    /// 1. Analisi del WAL: Il log viene letto per intero. Solo le operazioni all'interno di un blocco
//    ///    `BeginTransaction`/`CommitTransaction` vengono considerate valide per il recupero.
//    /// 2. Applicazione: Le operazioni validate vengono scritte in sequenza sul file di database principale.
//    /// Al termine, il file WAL viene eliminato.
//    /// </remarks>
//    public static void Recover(string dbFilePath)
//    {
//        var walPath = Path.ChangeExtension(dbFilePath, ".wal");
//        var walMutex = GetOrCreateCrossProcessWalMutex(walPath);
//        walMutex.WaitOne();
//        try
//        {
//            RecoverCore(dbFilePath, walPath);
//        }
//        finally
//        {
//            walMutex.ReleaseMutex();
//        }
//    }
//
//    private static void RecoverCore(string dbFilePath, string walPath)
//    {
//        if (!File.Exists(walPath))
//        {
//            return; // Nessun recupero necessario.
//        }
//
//        var walInfo = new FileInfo(walPath);
//        if (walInfo.Length == 0)
//        {
//            File.Delete(walPath);
//            return;
//        }
//
//        var committedTransactions = new List<List<(WalOperationType op, long offset, byte[] data)>>();
//        var activeTransactionOps = new List<(WalOperationType op, long offset, byte[] data)>();
//        var directWrites = new List<(long offset, byte[] data)>();
//        var inTransaction = false;
//        var endsWithDirectCheckpoint = false;
//        byte[]? pendingSnapshotHeader = null;
//        byte[]? pendingSnapshotMap = null;
//        byte[]? openTransactionSnapshotHeader = null;
//        byte[]? openTransactionSnapshotMap = null;
//
//        using (var walStream = new FileStream(walPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
//        using (var reader = new BinaryReader(walStream, Encoding.UTF8))
//        {
//            try
//            {
//                while (reader.BaseStream.Position < reader.BaseStream.Length)
//                {
//                    var opType = (WalOperationType)reader.ReadByte();
//                    switch (opType)
//                    {
//                        case WalOperationType.TransactionSnapshot:
//                            var headerLen = reader.ReadInt32();
//                            if (headerLen < 0 || headerLen > reader.BaseStream.Length - reader.BaseStream.Position)
//                            {
//                                break;
//                            }
//
//                            pendingSnapshotHeader = reader.ReadBytes(headerLen);
//                            var mapLen = reader.ReadInt32();
//                            if (mapLen < 0 || mapLen > reader.BaseStream.Length - reader.BaseStream.Position)
//                            {
//                                break;
//                            }
//
//                            pendingSnapshotMap = reader.ReadBytes(mapLen);
//                            break;
//
//                        case WalOperationType.BeginTransaction:
//                            activeTransactionOps.Clear();
//                            inTransaction = true;
//                            openTransactionSnapshotHeader = pendingSnapshotHeader;
//                            openTransactionSnapshotMap = pendingSnapshotMap;
//                            pendingSnapshotHeader = null;
//                            pendingSnapshotMap = null;
//                            break;
//
//                        case WalOperationType.CommitTransaction:
//                            if (inTransaction)
//                            {
//                                committedTransactions.Add([..activeTransactionOps]);
//                                activeTransactionOps.Clear();
//                                inTransaction = false;
//                                openTransactionSnapshotHeader = null;
//                                openTransactionSnapshotMap = null;
//                            }
//
//                            break;
//
//                        case WalOperationType.DirectCheckpoint:
//                            directWrites.Clear();
//                            endsWithDirectCheckpoint = true;
//                            break;
//
//                        case WalOperationType.WriteRecord:
//                        case WalOperationType.WriteNode:
//                        case WalOperationType.UpdateHeader:
//                        case WalOperationType.WriteBytes:
//                        case WalOperationType.DirectWrite:
//                            var offset = reader.ReadInt64();
//                            var length = reader.ReadInt32();
//                            if (length < 0 || length > reader.BaseStream.Length - reader.BaseStream.Position)
//                            {
//                                break;
//                            }
//
//                            var data = reader.ReadBytes(length);
//                            var operation = (opType, offset, data);
//
//                            if (inTransaction)
//                            {
//                                activeTransactionOps.Add(operation);
//                            }
//                            else if (opType == WalOperationType.DirectWrite)
//                            {
//                                directWrites.Add((offset, data));
//                                endsWithDirectCheckpoint = false;
//                            }
//
//                            break;
//
//                        default:
//                            break;
//                    }
//                }
//            }
//            catch (EndOfStreamException)
//            {
//                // ponytail: WAL tail troncato — uscita dal loop, replay parziale già raccolto
//            }
//        }
//
//        using (var dbStream = new FileStream(dbFilePath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite))
//        {
//            if (committedTransactions.Count > 0)
//            {
//                foreach (var transaction in committedTransactions)
//                {
//                    foreach (var (_, offset, data) in transaction)
//                    {
//                        dbStream.Seek(offset, SeekOrigin.Begin);
//                        dbStream.Write(data, 0, data.Length);
//                    }
//                }
//            }
//
//            if (!endsWithDirectCheckpoint)
//            {
//                directWrites.Clear();
//            }
//            else
//            {
//                foreach (var (offset, data) in directWrites)
//                {
//                    dbStream.Seek(offset, SeekOrigin.Begin);
//                    dbStream.Write(data, 0, data.Length);
//                }
//            }
//
//            if (inTransaction && openTransactionSnapshotHeader is { Length: > 0 } &&
//                openTransactionSnapshotMap is { Length: > 0 })
//            {
//                dbStream.Seek(0, SeekOrigin.Begin);
//                dbStream.Write(openTransactionSnapshotHeader, 0, openTransactionSnapshotHeader.Length);
//                if (openTransactionSnapshotHeader.Length >= 44)
//                {
//                    var mapOffset = BitConverter.ToInt64(openTransactionSnapshotHeader, 36);
//                    if (mapOffset > 0)
//                    {
//                        dbStream.Seek(mapOffset, SeekOrigin.Begin);
//                        dbStream.Write(openTransactionSnapshotMap, 0, openTransactionSnapshotMap.Length);
//                    }
//                }
//            }
//
//            dbStream.Flush(true);
//        }
//
//        File.Delete(walPath);
//    }
//
//    /// <summary>
//    /// Registra un'operazione di scrittura di un blocco di byte arbitrario nel log.
//    /// </summary>
//    /// <param name="offset">L'offset fisico nel file di database dove i byte devono essere scritti.</param>
//    /// <param name="data">L'array di byte da scrivere.</param>
//    public void LogWriteBytes(long offset, byte[] data)
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.WriteBytes);
//            _writer.Write(offset);
//            _writer.Write(data.Length);
//            _writer.Write(data);
//        });
//    }
//
//    /// <summary>
//    /// Registra una scrittura auto-commit da replayare in recovery se manca <see cref="WalOperationType.DirectCheckpoint"/>.
//    /// </summary>
//    public void LogDirectWrite(long offset, byte[] data)
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.DirectWrite);
//            _writer.Write(offset);
//            _writer.Write(data.Length);
//            _writer.Write(data);
//        });
//    }
//
//    /// <summary>
//    /// Segnala che tutte le DirectWrite precedenti sono state applicate e sincronizzate sul file principale.
//    /// </summary>
//    public void LogDirectCheckpoint()
//    {
//        WithWalLock(() =>
//        {
//            _writer.Write((byte)WalOperationType.DirectCheckpoint);
//            FlushInternal();
//        });
//    }
//
//    /// <summary>
//    /// Sincronizza i buffer del log su disco per garantire che le operazioni registrate siano persistenti.
//    /// </summary>
//    public void Flush()
//    {
//        WithWalLock(FlushInternal);
//    }
//
//    private void FlushInternal()
//    {
//        _writer.Flush();
//        if (_logStream.CanWrite)
//        {
//            _logStream.Flush(true);
//        }
//    }
//
//    /// <summary>
//    /// Rilascia le risorse gestite, assicurando che tutti i dati del log siano scritti su disco.
//    /// </summary>
//    public void Dispose()
//    {
//        if (_disposed)
//        {
//            return;
//        }
//
//        WithWalLock(() =>
//        {
//            FlushInternal();
//            _writer.Dispose();
//            _logStream.Dispose();
//        });
//        _disposed = true;
//    }
//
//    /// <summary>
//    /// Trunca il WAL in-place dopo un commit andato a buon fine, mantenendo lo stream aperto per ulteriori scritture.
//    /// </summary>
//    /// <param name="skipFlush">True se il chiamante ha già fsyncato (es. LogDirectCheckpoint).</param>
//    public void Delete(bool skipFlush = false)
//    {
//        if (_disposed)
//        {
//            if (File.Exists(_walFilePath))
//            {
//                File.Delete(_walFilePath);
//            }
//
//            return;
//        }
//
//        WithWalLock(() =>
//        {
//            if (_disposed)
//            {
//                return;
//            }
//
//            if (!skipFlush)
//            {
//                FlushInternal();
//            }
//
//            _writer.Dispose();
//            _logStream.Dispose();
//            _disposed = true;
//
//            if (File.Exists(_walFilePath))
//            {
//                File.Delete(_walFilePath);
//            }
//        });
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

/* C#: internal class WriteAheadLog */

static int wr_u8(FILE* f, uint8_t v) { return fwrite(&v, 1, 1, f) == 1 ? 0 : -1; }
static int wr_i32(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}
static int wr_i64(FILE* f, int64_t v)
{
    uint8_t b[8];
    int i;
    for (i = 0; i < 8; i++) b[i] = (uint8_t)((uint64_t)v >> (8 * i));
    return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}
static int wr_bytes(FILE* f, const uint8_t* p, int n)
{
    if (n <= 0) return 0;
    return fwrite(p, 1, (size_t)n, f) == (size_t)n ? 0 : -1;
}
static int rd_u8(FILE* f, uint8_t* v) { return fread(v, 1, 1, f) == 1 ? 0 : -1; }
static int rd_i32(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}
static int rd_i64(FILE* f, int64_t* v)
{
    uint8_t b[8];
    uint64_t u = 0;
    int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

typedef struct WalLockEntry {
    char path[PATH_MAX];
    pthread_mutex_t intra;
    int lockFd;
    struct WalLockEntry* next;
} WalLockEntry;

static pthread_mutex_t g_walTableMu = PTHREAD_MUTEX_INITIALIZER;
static WalLockEntry* g_walTable;

static void wal_change_ext(const char* db, char* out, size_t cap)
{
    size_t n = strlen(db);
    const char* dot = strrchr(db, '.');
    const char* slash = strrchr(db, '/');
    if (dot && (!slash || dot > slash))
        n = (size_t)(dot - db);
    snprintf(out, cap, "%.*s.wal", (int)n, db);
}

static WalLockEntry* wal_get_locks(const char* walPath)
{
    WalLockEntry* e;
    char lockPath[PATH_MAX];
    pthread_mutex_lock(&g_walTableMu);
    for (e = g_walTable; e; e = e->next) {
        if (gmb_strcasecmp(e->path, walPath) == 0) {
            pthread_mutex_unlock(&g_walTableMu);
            return e;
        }
    }
    e = (WalLockEntry*)calloc(1, sizeof(*e));
    snprintf(e->path, sizeof(e->path), "%s", walPath);
    pthread_mutex_init(&e->intra, NULL);
    snprintf(lockPath, sizeof(lockPath), "%s.xlock", walPath);
    e->lockFd = open(lockPath, O_RDWR | O_CREAT, 0666);
    e->next = g_walTable;
    g_walTable = e;
    pthread_mutex_unlock(&g_walTableMu);
    return e;
}

struct WriteAheadLog {
    char walFilePath[PATH_MAX];
    FILE* logStream;
    WalLockEntry* locks;
    int disposed;
};

static void WriteAheadLog_EnsureOpen(WriteAheadLog* self)
{
    if (!self->disposed) return;
    self->logStream = fopen(self->walFilePath, "ab+");
    self->disposed = 0;
}

static void WriteAheadLog_FlushInternal(WriteAheadLog* self)
{
    if (!self->logStream) return;
    fflush(self->logStream);
    fsync(fileno(self->logStream));
}

WriteAheadLog* WriteAheadLog_New(const char* dbFilePath)
{
    WriteAheadLog* self = (WriteAheadLog*)calloc(1, sizeof(*self));
    wal_change_ext(dbFilePath, self->walFilePath, sizeof(self->walFilePath));
    self->locks = wal_get_locks(self->walFilePath);
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    self->logStream = fopen(self->walFilePath, "ab+");
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
    return self;
}

void WriteAheadLog_LogBeginTransaction(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_BeginTransaction);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogTransactionSnapshot(WriteAheadLog* self, const uint8_t* headerData, int headerLen,
                                          const uint8_t* indexMapData, int mapLen)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_TransactionSnapshot);
    wr_i32(self->logStream, headerLen);
    wr_bytes(self->logStream, headerData, headerLen);
    wr_i32(self->logStream, mapLen);
    wr_bytes(self->logStream, indexMapData, mapLen);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogCommitTransaction(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_CommitTransaction);
    WriteAheadLog_FlushInternal(self);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogWriteRecordSpan(WriteAheadLog* self, int64_t offset, const uint8_t* recordData, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_WriteRecord);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, recordData, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogWriteRecord(WriteAheadLog* self, int64_t offset, const uint8_t* recordData, int len)
{
    WriteAheadLog_LogWriteRecordSpan(self, offset, recordData, len);
}

void WriteAheadLog_LogWriteNode(WriteAheadLog* self, int64_t offset, const uint8_t* nodeData, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_WriteNode);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, nodeData, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogUpdateHeader(WriteAheadLog* self, const uint8_t* headerData, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_UpdateHeader);
    wr_i64(self->logStream, 0);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, headerData, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogWriteBytes(WriteAheadLog* self, int64_t offset, const uint8_t* data, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_WriteBytes);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, data, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogDirectWrite(WriteAheadLog* self, int64_t offset, const uint8_t* data, int len)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_DirectWrite);
    wr_i64(self->logStream, offset);
    wr_i32(self->logStream, len);
    wr_bytes(self->logStream, data, len);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_LogDirectCheckpoint(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    wr_u8(self->logStream, (uint8_t)WalOperationType_DirectCheckpoint);
    WriteAheadLog_FlushInternal(self);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_Flush(WriteAheadLog* self)
{
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_EnsureOpen(self);
    WriteAheadLog_FlushInternal(self);
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

void WriteAheadLog_Dispose(WriteAheadLog* self)
{
    if (!self || self->disposed) return;
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    WriteAheadLog_FlushInternal(self);
    if (self->logStream) { fclose(self->logStream); self->logStream = NULL; }
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
    self->disposed = 1;
}

void WriteAheadLog_Delete(WriteAheadLog* self, int skipFlush)
{
    if (!self) return;
    if (self->disposed) {
        unlink(self->walFilePath);
        return;
    }
    flock(self->locks->lockFd, LOCK_EX);
    pthread_mutex_lock(&self->locks->intra);
    if (!self->disposed) {
        if (!skipFlush) WriteAheadLog_FlushInternal(self);
        if (self->logStream) { fclose(self->logStream); self->logStream = NULL; }
        self->disposed = 1;
        unlink(self->walFilePath);
    }
    pthread_mutex_unlock(&self->locks->intra);
    flock(self->locks->lockFd, LOCK_UN);
}

static void wal_empty_summary(WalLogSummary* s, const char* db, const char* wal, int exists, int64_t size)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->DatabasePath, sizeof(s->DatabasePath), "%s", db);
    snprintf(s->WalPath, sizeof(s->WalPath), "%s", wal);
    s->Exists = exists;
    s->SizeBytes = size;
    s->WalHandleActive = -1;
    s->DirectWalDirty = -1;
}

int WriteAheadLog_Inspect(const char* dbFilePath, WalLogSummary* out)
{
    char walPath[PATH_MAX];
    struct stat st;
    FILE* f;
    int committedBlocks = 0, activeTransactionOps = 0, directWrites = 0, pendingRecords = 0;
    int inTransaction = 0, endsWithCheckpoint = 0;
    int64_t lastPosition;
    if (!dbFilePath || !dbFilePath[0] || !out) return GMB_DB_ARG;
    wal_change_ext(dbFilePath, walPath, sizeof(walPath));
    if (stat(walPath, &st) != 0) {
        wal_empty_summary(out, dbFilePath, walPath, 0, 0);
        return GMB_DB_OK;
    }
    if (st.st_size == 0) {
        wal_empty_summary(out, dbFilePath, walPath, 1, 0);
        return GMB_DB_OK;
    }
    f = fopen(walPath, "rb");
    if (!f) return GMB_DB_ERR;
    lastPosition = st.st_size;
    for (;;) {
        uint8_t op;
        long pos = ftell(f);
        if (pos < 0 || pos >= st.st_size) break;
        if (rd_u8(f, &op) != 0) { lastPosition = pos; break; }
        switch ((WalOperationType)op) {
        case WalOperationType_TransactionSnapshot: {
            int32_t headerLen, mapLen;
            if (rd_i32(f, &headerLen) != 0) goto done;
            if (headerLen < 0 || headerLen > st.st_size - ftell(f)) goto done;
            fseek(f, headerLen, SEEK_CUR);
            if (rd_i32(f, &mapLen) != 0) goto done;
            if (mapLen < 0 || mapLen > st.st_size - ftell(f)) goto done;
            fseek(f, mapLen, SEEK_CUR);
            break;
        }
        case WalOperationType_BeginTransaction:
            activeTransactionOps = 0;
            inTransaction = 1;
            break;
        case WalOperationType_CommitTransaction:
            if (inTransaction) {
                committedBlocks++;
                activeTransactionOps = 0;
                inTransaction = 0;
            }
            break;
        case WalOperationType_DirectCheckpoint:
            directWrites = 0;
            endsWithCheckpoint = 1;
            break;
        case WalOperationType_WriteRecord:
        case WalOperationType_WriteNode:
        case WalOperationType_UpdateHeader:
        case WalOperationType_WriteBytes:
        case WalOperationType_DirectWrite: {
            int64_t offset;
            int32_t length;
            if (rd_i64(f, &offset) != 0) goto done;
            if (rd_i32(f, &length) != 0) goto done;
            if (length < 0 || length > st.st_size - ftell(f)) goto done;
            fseek(f, length, SEEK_CUR);
            pendingRecords++;
            if (inTransaction) activeTransactionOps++;
            else if (op == WalOperationType_DirectWrite) {
                directWrites++;
                endsWithCheckpoint = 0;
            }
            (void)offset;
            break;
        }
        default:
            break;
        }
    }
done:
    fclose(f);
    wal_empty_summary(out, dbFilePath, walPath, 1, st.st_size);
    out->LastPosition = lastPosition;
    out->PendingRecords = pendingRecords;
    out->UncommittedTransactionOps = activeTransactionOps;
    out->CommittedTransactionBlocks = committedBlocks;
    out->DirectWritesPending = directWrites;
    out->EndsWithCheckpoint = endsWithCheckpoint;
    out->InOpenTransaction = inTransaction;
    return GMB_DB_OK;
}

typedef struct {
    uint8_t op;
    int64_t offset;
    int32_t len;
    uint8_t* data;
} WalLoggedOp;

static void RecoverCore(const char* dbFilePath, const char* walPath)
{
    struct stat st;
    FILE *wal, *db;
    WalLoggedOp* active = NULL;
    int activeN = 0, activeCap = 0;
    WalLoggedOp** committed = NULL;
    int* committedCounts = NULL;
    int committedN = 0, committedCap = 0;
    WalLoggedOp* directs = NULL;
    int directsN = 0, directsCap = 0;
    int inTransaction = 0, endsWithDirectCheckpoint = 0;
    uint8_t *pendingSnapH = NULL, *pendingSnapM = NULL, *openSnapH = NULL, *openSnapM = NULL;
    int pendingSnapHLen = 0, pendingSnapMLen = 0, openSnapHLen = 0, openSnapMLen = 0;
    int i, t;

    if (stat(walPath, &st) != 0) return;
    if (st.st_size == 0) { unlink(walPath); return; }

    wal = fopen(walPath, "rb");
    if (!wal) return;
    for (;;) {
        uint8_t op;
        long pos = ftell(wal);
        if (pos < 0 || pos >= st.st_size) break;
        if (rd_u8(wal, &op) != 0) break;
        switch ((WalOperationType)op) {
        case WalOperationType_TransactionSnapshot: {
            int32_t headerLen, mapLen;
            if (rd_i32(wal, &headerLen) != 0) goto parse_done;
            if (headerLen < 0 || headerLen > st.st_size - ftell(wal)) goto parse_done;
            free(pendingSnapH);
            pendingSnapH = (uint8_t*)malloc((size_t)headerLen);
            pendingSnapHLen = headerLen;
            if (headerLen > 0 && fread(pendingSnapH, 1, (size_t)headerLen, wal) != (size_t)headerLen) goto parse_done;
            if (rd_i32(wal, &mapLen) != 0) goto parse_done;
            if (mapLen < 0 || mapLen > st.st_size - ftell(wal)) goto parse_done;
            free(pendingSnapM);
            pendingSnapM = (uint8_t*)malloc((size_t)mapLen);
            pendingSnapMLen = mapLen;
            if (mapLen > 0 && fread(pendingSnapM, 1, (size_t)mapLen, wal) != (size_t)mapLen) goto parse_done;
            break;
        }
        case WalOperationType_BeginTransaction:
            for (i = 0; i < activeN; i++) free(active[i].data);
            activeN = 0;
            inTransaction = 1;
            free(openSnapH); free(openSnapM);
            openSnapH = pendingSnapH; openSnapHLen = pendingSnapHLen; pendingSnapH = NULL; pendingSnapHLen = 0;
            openSnapM = pendingSnapM; openSnapMLen = pendingSnapMLen; pendingSnapM = NULL; pendingSnapMLen = 0;
            break;
        case WalOperationType_CommitTransaction:
            if (inTransaction) {
                if (committedN >= committedCap) {
                    committedCap = committedCap ? committedCap * 2 : 4;
                    committed = (WalLoggedOp**)realloc(committed, (size_t)committedCap * sizeof(*committed));
                    committedCounts = (int*)realloc(committedCounts, (size_t)committedCap * sizeof(int));
                }
                committed[committedN] = (WalLoggedOp*)malloc((size_t)activeN * sizeof(WalLoggedOp));
                memcpy(committed[committedN], active, (size_t)activeN * sizeof(WalLoggedOp));
                committedCounts[committedN] = activeN;
                committedN++;
                activeN = 0;
                inTransaction = 0;
                free(openSnapH); free(openSnapM);
                openSnapH = openSnapM = NULL;
                openSnapHLen = openSnapMLen = 0;
            }
            break;
        case WalOperationType_DirectCheckpoint:
            for (i = 0; i < directsN; i++) free(directs[i].data);
            directsN = 0;
            endsWithDirectCheckpoint = 1;
            break;
        case WalOperationType_WriteRecord:
        case WalOperationType_WriteNode:
        case WalOperationType_UpdateHeader:
        case WalOperationType_WriteBytes:
        case WalOperationType_DirectWrite: {
            int64_t offset;
            int32_t length;
            WalLoggedOp rec;
            if (rd_i64(wal, &offset) != 0) goto parse_done;
            if (rd_i32(wal, &length) != 0) goto parse_done;
            if (length < 0 || length > st.st_size - ftell(wal)) goto parse_done;
            rec.op = op;
            rec.offset = offset;
            rec.len = length;
            rec.data = (uint8_t*)malloc((size_t)length);
            if (length > 0 && fread(rec.data, 1, (size_t)length, wal) != (size_t)length) {
                free(rec.data);
                goto parse_done;
            }
            if (inTransaction) {
                if (activeN >= activeCap) {
                    activeCap = activeCap ? activeCap * 2 : 8;
                    active = (WalLoggedOp*)realloc(active, (size_t)activeCap * sizeof(*active));
                }
                active[activeN++] = rec;
            } else if (op == WalOperationType_DirectWrite) {
                if (directsN >= directsCap) {
                    directsCap = directsCap ? directsCap * 2 : 8;
                    directs = (WalLoggedOp*)realloc(directs, (size_t)directsCap * sizeof(*directs));
                }
                directs[directsN++] = rec;
                endsWithDirectCheckpoint = 0;
            } else {
                free(rec.data);
            }
            break;
        }
        default:
            break;
        }
    }
parse_done:
    fclose(wal);

    db = fopen(dbFilePath, "rb+");
    if (db) {
        for (t = 0; t < committedN; t++) {
            for (i = 0; i < committedCounts[t]; i++) {
                fseek(db, committed[t][i].offset, SEEK_SET);
                fwrite(committed[t][i].data, 1, (size_t)committed[t][i].len, db);
            }
        }
        if (!endsWithDirectCheckpoint) {
            for (i = 0; i < directsN; i++) { free(directs[i].data); directs[i].data = NULL; }
            directsN = 0;
        } else {
            for (i = 0; i < directsN; i++) {
                fseek(db, directs[i].offset, SEEK_SET);
                fwrite(directs[i].data, 1, (size_t)directs[i].len, db);
            }
        }
        if (inTransaction && openSnapHLen > 0 && openSnapMLen > 0) {
            fseek(db, 0, SEEK_SET);
            fwrite(openSnapH, 1, (size_t)openSnapHLen, db);
            if (openSnapHLen >= 44) {
                int64_t mapOffset = 0;
                int k;
                for (k = 0; k < 8; k++) mapOffset |= (int64_t)openSnapH[36 + k] << (8 * k);
                if (mapOffset > 0) {
                    fseek(db, mapOffset, SEEK_SET);
                    fwrite(openSnapM, 1, (size_t)openSnapMLen, db);
                }
            }
        }
        fflush(db);
        fsync(fileno(db));
        fclose(db);
    }

    for (t = 0; t < committedN; t++) {
        for (i = 0; i < committedCounts[t]; i++) free(committed[t][i].data);
        free(committed[t]);
    }
    free(committed); free(committedCounts);
    for (i = 0; i < activeN; i++) free(active[i].data);
    free(active);
    for (i = 0; i < directsN; i++) free(directs[i].data);
    free(directs);
    free(pendingSnapH); free(pendingSnapM); free(openSnapH); free(openSnapM);
    unlink(walPath);
}

void WriteAheadLog_Recover(const char* dbFilePath)
{
    char walPath[PATH_MAX];
    WalLockEntry* locks;
    wal_change_ext(dbFilePath, walPath, sizeof(walPath));
    locks = wal_get_locks(walPath);
    flock(locks->lockFd, LOCK_EX);
    RecoverCore(dbFilePath, walPath);
    flock(locks->lockFd, LOCK_UN);
}

int WriteAheadLog_SelfCheck(void)
{
    char dir[] = "/tmp/gmb_wal_selfcheck_XXXXXX";
    char db[PATH_MAX], wal[PATH_MAX];
    uint8_t payload[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t verify[8];
    FILE* f;
    WriteAheadLog* w;
    WalLogSummary s;
    if (!mkdtemp(dir)) return GMB_DB_ERR;
    snprintf(db, sizeof(db), "%s/t.dbf", dir);
    f = fopen(db, "wb");
    if (!f) return GMB_DB_ERR;
    fwrite("XXXXXXXX", 1, 8, f);
    fclose(f);
    w = WriteAheadLog_New(db);
    WriteAheadLog_LogBeginTransaction(w);
    WriteAheadLog_LogWriteRecord(w, 0, payload, 8);
    WriteAheadLog_LogCommitTransaction(w);
    WriteAheadLog_Dispose(w);
    free(w);
    wal_change_ext(db, wal, sizeof(wal));
    WriteAheadLog_Inspect(db, &s);
    if (!s.Exists || s.CommittedTransactionBlocks != 1) return GMB_DB_ERR;
    WriteAheadLog_Recover(db);
    f = fopen(db, "rb");
    if (!f || fread(verify, 1, 8, f) != 8) return GMB_DB_ERR;
    fclose(f);
    if (memcmp(verify, payload, 8) != 0) return GMB_DB_ERR;
    if (access(wal, F_OK) == 0) return GMB_DB_ERR;
    return GMB_DB_OK;
}
