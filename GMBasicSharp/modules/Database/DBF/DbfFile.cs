// FILE: Basic.DBF.DbfFile.cs

using System.Buffers;
using System.Runtime.InteropServices;
using System.Text;
using Basic.Databases;

// ReSharper disable InconsistentNaming

namespace Basic.DBF;

/// <summary>
/// Fornisce accesso in lettura e scrittura a basso livello a un file DBF.
/// Gestisce l'apertura del file, la lettura/scrittura dell'header,
/// le definizioni dei campi e l'iterazione/modifica dei record grezzi.
/// </summary>
public class DbfFile : IDisposable
{
    private readonly FileStream _stream;
    private readonly BinaryReader _reader;
    private readonly BinaryWriter _writer;
    private bool _disposed;
    private bool _headerDirty;

    /// <summary>
    /// Ottiene l'header del file DBF.
    /// </summary>
    public DbfHeader Header { get; private set; }

    /// <summary>
    /// Ottiene la lista delle definizioni dei campi del file DBF.
    /// </summary>
    public IReadOnlyList<DbfField> Fields { get; }

    /// <summary>
    /// Ottiene il percorso completo del file DBF.
    /// </summary>
    public string FilePath { get; }
    
    /// <summary>
    /// Apre un file DBF per lettura e scrittura.
    /// </summary>
    public DbfFile(string filePath, bool exclusive, LockingStrategy lockingStrategy = LockingStrategy.CrossProcess)
    {
        FilePath = filePath;
        var fileShare = exclusive switch
        {
            // ponytail: multi work area / cross-process — record locks via ILockManager, not OS exclusive FileShare
            true when lockingStrategy is LockingStrategy.IntraProcess or LockingStrategy.CrossProcess => FileShare.Read,
            true => FileShare.None,
            _ => FileShare.ReadWrite,
        };
        _stream = new FileStream(filePath, FileMode.Open, FileAccess.ReadWrite, fileShare, 4096, FileOptions.RandomAccess);
        _reader = new BinaryReader(_stream, Encoding.ASCII, leaveOpen: true);
        _writer = new BinaryWriter(_stream, Encoding.ASCII, leaveOpen: true);
        
        Header = DbfHeader.Read(_reader);
        Fields = DbfField.ReadAll(_reader, Header.Encoding);
    }
    
    /// <summary>
    /// Ottiene la lunghezza totale in byte del file DBF su disco.
    /// </summary>
    /// <returns>La lunghezza del file.</returns>
    public long GetFileLength() => _stream.Length;
    
    /// <summary>
    /// Legge i dati grezzi di un record specifico.
    /// </summary>
    /// <param name="recNo">Il numero del record (1-based).</param>
    /// <returns>Una tupla contenente il flag di cancellazione e i byte grezzi del record.</returns>
    public (bool isDeleted, byte[] data) ReadRawRecord(int recNo)
    {
        var recordLength = Header.RecordLength;
        var buffer = ArrayPool<byte>.Shared.Rent(recordLength);
        try
        {
            ReadRawRecordInto(recNo, buffer.AsSpan(0, recordLength), out var isDeleted);
            return (isDeleted, buffer.AsSpan(1, recordLength - 1).ToArray());
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(buffer);
        }
    }

    /// <summary>
    /// Legge un record grezzo in un buffer pre-allocato (flag cancellazione + payload).
    /// </summary>
    public void ReadRawRecordInto(int recNo, Span<byte> buffer, out bool isDeleted)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(buffer.Length, Header.RecordLength);
        ValidateRecordNumber(recNo);
        var offset = Header.HeaderLength + ((long)(recNo - 1) * Header.RecordLength);
        _stream.Seek(offset, SeekOrigin.Begin);
        _stream.ReadExactly(buffer[..Header.RecordLength]);
        isDeleted = buffer[0] == '*';
    }

    /// <summary>
    /// Scrive i dati grezzi in un record esistente.
    /// </summary>
    /// <param name="recNo">Il numero del record (1-based) da sovrascrivere.</param>
    /// <param name="rawData">I byte del record (escluso il flag di cancellazione).</param>
    public void WriteRawRecord(int recNo, ReadOnlySpan<byte> rawData)
    {
        ValidateRecordNumber(recNo);
        var offset = Header.HeaderLength + ((long)(recNo - 1) * Header.RecordLength) + 1; // +1 per saltare il flag
        _stream.Seek(offset, SeekOrigin.Begin);
        _stream.Write(rawData);
    }

    /// <summary>
    /// Aggiunge un nuovo record alla fine del file.
    /// </summary>
    /// <param name="rawData">I byte del nuovo record (escluso il flag di cancellazione).</param>
    /// <returns>Il numero del nuovo record aggiunto.</returns>
    public int AppendRawRecord(ReadOnlySpan<byte> rawData)
    {
        var offset = Header.HeaderLength + (long)Header.NumberOfRecords * Header.RecordLength;
        var requiredLength = offset + Header.RecordLength;
        if (_stream.Length < requiredLength)
        {
            _stream.SetLength(requiredLength);
        }

        _stream.Seek(offset, SeekOrigin.Begin);
        _writer.Write((byte)' ');
        _stream.Write(rawData);

        Header = BumpHeaderRecordCount(Header);
        _headerDirty = true;
        return (int)Header.NumberOfRecords;
    }

    /// <summary>
    /// Aggiunge <paramref name="count"/> record consecutivi con un solo aggiornamento header.
    /// ponytail: payload = count × recordPayloadLength byte (senza flag cancellazione).
    /// </summary>
    /// <returns>Il numero del primo record inserito (1-based).</returns>
    public int AppendRawRecordsBatch(ReadOnlySpan<byte> rawPayload, int recordPayloadLength, int count)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
        if (rawPayload.Length < (long)recordPayloadLength * count)
        {
            throw new ArgumentException("Payload batch troppo corto.", nameof(rawPayload));
        }

        var startRecNo = (int)Header.NumberOfRecords + 1;
        var offset = Header.HeaderLength + (long)Header.NumberOfRecords * Header.RecordLength;
        var recordLength = Header.RecordLength;
        var totalBytes = (long)recordLength * count;
        var requiredLength = offset + totalBytes;

        if (_stream.Length < requiredLength)
        {
            _stream.SetLength(requiredLength);
        }

        _stream.Seek(offset, SeekOrigin.Begin);
        for (var i = 0; i < count; i++)
        {
            _writer.Write((byte)' ');
            _stream.Write(rawPayload.Slice(i * recordPayloadLength, recordPayloadLength));
        }

        Header = BumpHeaderRecordCount(Header, (uint)count);
        _headerDirty = true;
        return startRecNo;
    }

    /// <summary>
    /// Rilegge l'header dal file (cross-process: altri handle possono aver aggiornato il conteggio record).
    /// ponytail: FileStream separato — evita cache stale sullo stream di scrittura condiviso.
    /// </summary>
    public void ReloadHeaderFromDisk()
    {
        using (var probe = new FileStream(FilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
        using (var reader = new BinaryReader(probe, Encoding.ASCII))
        {
            Header = DbfHeader.Read(reader);
        }

        _headerDirty = false;
        var expectedLength = Header.HeaderLength + (long)Header.NumberOfRecords * Header.RecordLength;
        if (_stream.Length < expectedLength)
        {
            _stream.SetLength(expectedLength);
        }
    }

    /// <summary>
    /// Scrive l'header pendente su disco se modificato in memoria.
    /// ponytail: chiamato solo su FinalizeCommit/EndBulkLoad — upgrade: group-commit multi-record.
    /// </summary>
    public void FlushPendingHeader()
    {
        if (!_headerDirty)
        {
            return;
        }

        WriteHeaderToDisk(Header);
        _headerDirty = false;
    }

    /// <summary>
    /// Forza il flush dello stream sottostante (durabilità bulk-load).
    /// </summary>
    /// <param name="durable">True per fsync; false per flush buffer OS only.</param>
    /// <param name="flushHeaderFirst">False se <see cref="FlushPendingHeader"/> è già stato chiamato.</param>
    public void FlushStream(bool durable = true, bool flushHeaderFirst = true)
    {
        if (flushHeaderFirst)
        {
            FlushPendingHeader();
        }

        _stream.Flush(durable);
    }

    /// <summary>
    /// Scrive il flag di cancellazione per un record specifico.
    /// </summary>
    /// <param name="recNo">Il numero del record (1-based).</param>
    /// <param name="isDeleted">True per marcare come cancellato ('*'), False per marcare come attivo (' ').</param>
    public void SetDeletedFlag(int recNo, bool isDeleted)
    {
        ValidateRecordNumber(recNo);
        var offset = Header.HeaderLength + ((long)(recNo - 1) * Header.RecordLength);
        _stream.Seek(offset, SeekOrigin.Begin);
        
        _writer.Write(isDeleted ? (byte)'*' : (byte)' ');
    }
    
    /// <summary>
    /// Crea un nuovo file DBF vuoto con la stessa struttura (header e definizioni dei campi) di questo file.
    /// Il nuovo file avrà zero record e la data dell'ultimo aggiornamento impostata al momento della creazione.
    /// </summary>
    /// <param name="newFilePath">Il percorso del nuovo file da creare. Se il file esiste già, sarà sovrascritto.</param>
    public void CloneStructure(string newFilePath)
    {
        using var newStream = new FileStream(newFilePath, FileMode.Create, FileAccess.Write, FileShare.None);
        
        // Legge l'intero blocco dell'header (inclusi i campi) dal file originale.
        var headerSize = Header.HeaderLength;
        var headerBuffer = new byte[headerSize];
        _stream.Seek(0, SeekOrigin.Begin);
        _stream.ReadExactly(headerBuffer, 0, headerSize);

        // Modifica il buffer dell'header per il nuovo file.
        var now = DateTime.Now;
        headerBuffer[1] = (byte)(now.Year - 1900); // Anno (offset 1)
        headerBuffer[2] = (byte)now.Month;         // Mese (offset 2)
        headerBuffer[3] = (byte)now.Day;           // Giorno (offset 3)

        // Azzera il numero di record (uint a 4 byte all'offset 4).
        BitConverter.GetBytes((uint)0).CopyTo(headerBuffer, 4);
        
        // Scrive l'header modificato nel nuovo file.
        newStream.Write(headerBuffer, 0, headerSize);
    }
    
    /// <summary>
    /// Rimuove tutti i record dal file, troncandolo dopo l'header.
    /// </summary>
    public void Zap()
    {
        // Tronca il file alla fine dell'header.
        _stream.SetLength(Header.HeaderLength);

        // Aggiorna l'header in memoria e su disco per riflettere 0 record.
        var newHeader = Header;
        newHeader.NumberOfRecords = 0;
        newHeader.Year = (byte)(DateTime.Now.Year % 100);
        newHeader.Month = (byte)DateTime.Now.Month;
        newHeader.Day = (byte)DateTime.Now.Day;
        
        Header = newHeader;

        _stream.Seek(0, SeekOrigin.Begin);
        var headerBytes = new byte[DbfHeader.Size];
        var handle = GCHandle.Alloc(headerBytes, GCHandleType.Pinned);
        try
        {
            Marshal.StructureToPtr(newHeader, handle.AddrOfPinnedObject(), false);
            _stream.Write(headerBytes, 0, headerBytes.Length);
        }
        finally
        {
            handle.Free();
        }
        _stream.Flush();
    }
    
    /// <summary>
    /// Crea un nuovo file DBF vuoto su disco con una struttura di campi specificata.
    /// Se il file esiste già, verrà sovrascritto.
    /// </summary>
    /// <param name="filePath">Il percorso completo del nuovo file .dbf da creare.</param>
    /// <param name="encoding">La codifica dei caratteri da utilizzare per il nuovo file.</param>
    /// <param name="fieldCollection">Una collezione di oggetti <see cref="DbfField"/> che definisce la struttura del file.</param>
    public static void CreateNew(string filePath, Encoding encoding, IEnumerable<DbfField> fieldCollection)
    {
        var fields = fieldCollection.ToList();
        
        var headerLength = (ushort)(32 + fields.Count * 32 + 1);
        var recordLength = (ushort)(1 + fields.Sum(f => f.Length));

        var languageDriver = DbfCodePage.GetLanguageDriver(encoding) ?? 0x03;

        using var fs = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None);
        using var writer = new BinaryWriter(fs, encoding);
        var now = DateTime.Now;
        writer.Write((byte)DbfVersion.VisualFoxPro);
        writer.Write((byte)(now.Year - 1900));
        writer.Write((byte)now.Month);
        writer.Write((byte)now.Day);
        writer.Write((uint)0);
        writer.Write(headerLength);
        writer.Write(recordLength);
        writer.Write(new byte[17]);
        writer.Write(languageDriver);
        writer.Write(new byte[2]);

        uint fieldOffset = 1;
        foreach (var field in fields)
        {
            var nameBytes = new byte[11];
            var fieldNameBytes = encoding.GetBytes(field.Name);
            Buffer.BlockCopy(fieldNameBytes, 0, nameBytes, 0, Math.Min(fieldNameBytes.Length, 10));
            writer.Write(nameBytes);
            writer.Write((byte)field.Type);
            writer.Write(fieldOffset);
            writer.Write(field.Length);
            writer.Write(field.DecimalCount);
            writer.Write(new byte[14]);
            fieldOffset += field.Length;
        }

        writer.Write((byte)0x0D);
        writer.Write((byte)0x1A);
    }
        
    private static DbfHeader BumpHeaderRecordCount(DbfHeader header, uint add = 1)
    {
        header.NumberOfRecords += add;
        header.Year = (byte)(DateTime.Now.Year % 100);
        header.Month = (byte)DateTime.Now.Month;
        header.Day = (byte)DateTime.Now.Day;
        return header;
    }

    private void WriteHeaderToDisk(DbfHeader header)
    {
        _stream.Seek(0, SeekOrigin.Begin);
        var headerBytes = new byte[DbfHeader.Size];
        var handle = GCHandle.Alloc(headerBytes, GCHandleType.Pinned);
        try
        {
            Marshal.StructureToPtr(header, handle.AddrOfPinnedObject(), false);
            _stream.Write(headerBytes, 0, headerBytes.Length);
        }
        finally
        {
            handle.Free();
        }
    }

    private void ValidateRecordNumber(int recNo)
    {
        if (recNo <= 0 || recNo > Header.NumberOfRecords)
            throw new ArgumentOutOfRangeException(nameof(recNo), "Numero di record non valido.");
    }
    
    /// <summary>
    /// Rilascia le risorse associate al lettore.
    /// </summary>
    public void Dispose()
    {
        if (_disposed) return;
        FlushPendingHeader();
        _writer.Dispose();
        _reader.Dispose();
        _stream.Dispose();
        _disposed = true;
    }
}