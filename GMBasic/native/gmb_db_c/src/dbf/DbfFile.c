/* FILE: native/gmb_db_c/src/DBF/DbfFile.c */
/* Port 1:1 di modules/Database/DBF/DbfFile.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfFile.cs
//
//using System.Buffers;
//using System.Runtime.InteropServices;
//using System.Text;
//using Basic.Databases;
//
//// ReSharper disable InconsistentNaming
//
//namespace Basic.DBF;
//
///// <summary>
///// Fornisce accesso in lettura e scrittura a basso livello a un file DBF.
///// Gestisce l'apertura del file, la lettura/scrittura dell'header,
///// le definizioni dei campi e l'iterazione/modifica dei record grezzi.
///// </summary>
//public class DbfFile : IDisposable
//{
//    private readonly FileStream _stream;
//    private readonly BinaryReader _reader;
//    private readonly BinaryWriter _writer;
//    private bool _disposed;
//    private bool _headerDirty;
//
//    /// <summary>
//    /// Ottiene l'header del file DBF.
//    /// </summary>
//    public DbfHeader Header { get; private set; }
//
//    /// <summary>
//    /// Ottiene la lista delle definizioni dei campi del file DBF.
//    /// </summary>
//    public IReadOnlyList<DbfField> Fields { get; }
//
//    /// <summary>
//    /// Ottiene il percorso completo del file DBF.
//    /// </summary>
//    public string FilePath { get; }
//    
//    /// <summary>
//    /// Apre un file DBF per lettura e scrittura.
//    /// </summary>
//    public DbfFile(string filePath, bool exclusive, LockingStrategy lockingStrategy = LockingStrategy.CrossProcess)
//    {
//        FilePath = filePath;
//        var fileShare = exclusive switch
//        {
//            // ponytail: multi work area / cross-process — record locks via ILockManager, not OS exclusive FileShare
//            true when lockingStrategy is LockingStrategy.IntraProcess or LockingStrategy.CrossProcess => FileShare.Read,
//            true => FileShare.None,
//            _ => FileShare.ReadWrite,
//        };
//        _stream = new FileStream(filePath, FileMode.Open, FileAccess.ReadWrite, fileShare, 4096, FileOptions.RandomAccess);
//        _reader = new BinaryReader(_stream, Encoding.ASCII, leaveOpen: true);
//        _writer = new BinaryWriter(_stream, Encoding.ASCII, leaveOpen: true);
//        
//        Header = DbfHeader.Read(_reader);
//        Fields = DbfField.ReadAll(_reader, Header.Encoding);
//    }
//    
//    /// <summary>
//    /// Ottiene la lunghezza totale in byte del file DBF su disco.
//    /// </summary>
//    /// <returns>La lunghezza del file.</returns>
//    public long GetFileLength() => _stream.Length;
//    
//    /// <summary>
//    /// Legge i dati grezzi di un record specifico.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based).</param>
//    /// <returns>Una tupla contenente il flag di cancellazione e i byte grezzi del record.</returns>
//    public (bool isDeleted, byte[] data) ReadRawRecord(int recNo)
//    {
//        var recordLength = Header.RecordLength;
//        var buffer = ArrayPool<byte>.Shared.Rent(recordLength);
//        try
//        {
//            ReadRawRecordInto(recNo, buffer.AsSpan(0, recordLength), out var isDeleted);
//            return (isDeleted, buffer.AsSpan(1, recordLength - 1).ToArray());
//        }
//        finally
//        {
//            ArrayPool<byte>.Shared.Return(buffer);
//        }
//    }
//
//    /// <summary>
//    /// Legge un record grezzo in un buffer pre-allocato (flag cancellazione + payload).
//    /// </summary>
//    public void ReadRawRecordInto(int recNo, Span<byte> buffer, out bool isDeleted)
//    {
//        ArgumentOutOfRangeException.ThrowIfLessThan(buffer.Length, Header.RecordLength);
//        ValidateRecordNumber(recNo);
//        var offset = Header.HeaderLength + ((long)(recNo - 1) * Header.RecordLength);
//        _stream.Seek(offset, SeekOrigin.Begin);
//        _stream.ReadExactly(buffer[..Header.RecordLength]);
//        isDeleted = buffer[0] == '*';
//    }
//
//    /// <summary>
//    /// Scrive i dati grezzi in un record esistente.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based) da sovrascrivere.</param>
//    /// <param name="rawData">I byte del record (escluso il flag di cancellazione).</param>
//    public void WriteRawRecord(int recNo, ReadOnlySpan<byte> rawData)
//    {
//        ValidateRecordNumber(recNo);
//        var offset = Header.HeaderLength + ((long)(recNo - 1) * Header.RecordLength) + 1; // +1 per saltare il flag
//        _stream.Seek(offset, SeekOrigin.Begin);
//        _stream.Write(rawData);
//    }
//
//    /// <summary>
//    /// Aggiunge un nuovo record alla fine del file.
//    /// </summary>
//    /// <param name="rawData">I byte del nuovo record (escluso il flag di cancellazione).</param>
//    /// <returns>Il numero del nuovo record aggiunto.</returns>
//    public int AppendRawRecord(ReadOnlySpan<byte> rawData)
//    {
//        var offset = Header.HeaderLength + (long)Header.NumberOfRecords * Header.RecordLength;
//        var requiredLength = offset + Header.RecordLength;
//        if (_stream.Length < requiredLength)
//        {
//            _stream.SetLength(requiredLength);
//        }
//
//        _stream.Seek(offset, SeekOrigin.Begin);
//        _writer.Write((byte)' ');
//        _stream.Write(rawData);
//
//        Header = BumpHeaderRecordCount(Header);
//        _headerDirty = true;
//        return (int)Header.NumberOfRecords;
//    }
//
//    /// <summary>
//    /// Aggiunge <paramref name="count"/> record consecutivi con un solo aggiornamento header.
//    /// ponytail: payload = count × recordPayloadLength byte (senza flag cancellazione).
//    /// </summary>
//    /// <returns>Il numero del primo record inserito (1-based).</returns>
//    public int AppendRawRecordsBatch(ReadOnlySpan<byte> rawPayload, int recordPayloadLength, int count)
//    {
//        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(count);
//        if (rawPayload.Length < (long)recordPayloadLength * count)
//        {
//            throw new ArgumentException("Payload batch troppo corto.", nameof(rawPayload));
//        }
//
//        var startRecNo = (int)Header.NumberOfRecords + 1;
//        var offset = Header.HeaderLength + (long)Header.NumberOfRecords * Header.RecordLength;
//        var recordLength = Header.RecordLength;
//        var totalBytes = (long)recordLength * count;
//        var requiredLength = offset + totalBytes;
//
//        if (_stream.Length < requiredLength)
//        {
//            _stream.SetLength(requiredLength);
//        }
//
//        _stream.Seek(offset, SeekOrigin.Begin);
//        for (var i = 0; i < count; i++)
//        {
//            _writer.Write((byte)' ');
//            _stream.Write(rawPayload.Slice(i * recordPayloadLength, recordPayloadLength));
//        }
//
//        Header = BumpHeaderRecordCount(Header, (uint)count);
//        _headerDirty = true;
//        return startRecNo;
//    }
//
//    /// <summary>
//    /// Rilegge l'header dal file (cross-process: altri handle possono aver aggiornato il conteggio record).
//    /// ponytail: FileStream separato — evita cache stale sullo stream di scrittura condiviso.
//    /// </summary>
//    public void ReloadHeaderFromDisk()
//    {
//        using (var probe = new FileStream(FilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
//        using (var reader = new BinaryReader(probe, Encoding.ASCII))
//        {
//            Header = DbfHeader.Read(reader);
//        }
//
//        _headerDirty = false;
//        var expectedLength = Header.HeaderLength + (long)Header.NumberOfRecords * Header.RecordLength;
//        if (_stream.Length < expectedLength)
//        {
//            _stream.SetLength(expectedLength);
//        }
//    }
//
//    /// <summary>
//    /// Scrive l'header pendente su disco se modificato in memoria.
//    /// ponytail: chiamato solo su FinalizeCommit/EndBulkLoad — upgrade: group-commit multi-record.
//    /// </summary>
//    public void FlushPendingHeader()
//    {
//        if (!_headerDirty)
//        {
//            return;
//        }
//
//        WriteHeaderToDisk(Header);
//        _headerDirty = false;
//    }
//
//    /// <summary>
//    /// Forza il flush dello stream sottostante (durabilità bulk-load).
//    /// </summary>
//    /// <param name="durable">True per fsync; false per flush buffer OS only.</param>
//    /// <param name="flushHeaderFirst">False se <see cref="FlushPendingHeader"/> è già stato chiamato.</param>
//    public void FlushStream(bool durable = true, bool flushHeaderFirst = true)
//    {
//        if (flushHeaderFirst)
//        {
//            FlushPendingHeader();
//        }
//
//        _stream.Flush(durable);
//    }
//
//    /// <summary>
//    /// Scrive il flag di cancellazione per un record specifico.
//    /// </summary>
//    /// <param name="recNo">Il numero del record (1-based).</param>
//    /// <param name="isDeleted">True per marcare come cancellato ('*'), False per marcare come attivo (' ').</param>
//    public void SetDeletedFlag(int recNo, bool isDeleted)
//    {
//        ValidateRecordNumber(recNo);
//        var offset = Header.HeaderLength + ((long)(recNo - 1) * Header.RecordLength);
//        _stream.Seek(offset, SeekOrigin.Begin);
//        
//        _writer.Write(isDeleted ? (byte)'*' : (byte)' ');
//    }
//    
//    /// <summary>
//    /// Crea un nuovo file DBF vuoto con la stessa struttura (header e definizioni dei campi) di questo file.
//    /// Il nuovo file avrà zero record e la data dell'ultimo aggiornamento impostata al momento della creazione.
//    /// </summary>
//    /// <param name="newFilePath">Il percorso del nuovo file da creare. Se il file esiste già, sarà sovrascritto.</param>
//    public void CloneStructure(string newFilePath)
//    {
//        using var newStream = new FileStream(newFilePath, FileMode.Create, FileAccess.Write, FileShare.None);
//        
//        // Legge l'intero blocco dell'header (inclusi i campi) dal file originale.
//        var headerSize = Header.HeaderLength;
//        var headerBuffer = new byte[headerSize];
//        _stream.Seek(0, SeekOrigin.Begin);
//        _stream.ReadExactly(headerBuffer, 0, headerSize);
//
//        // Modifica il buffer dell'header per il nuovo file.
//        var now = DateTime.Now;
//        headerBuffer[1] = (byte)(now.Year - 1900); // Anno (offset 1)
//        headerBuffer[2] = (byte)now.Month;         // Mese (offset 2)
//        headerBuffer[3] = (byte)now.Day;           // Giorno (offset 3)
//
//        // Azzera il numero di record (uint a 4 byte all'offset 4).
//        BitConverter.GetBytes((uint)0).CopyTo(headerBuffer, 4);
//        
//        // Scrive l'header modificato nel nuovo file.
//        newStream.Write(headerBuffer, 0, headerSize);
//    }
//    
//    /// <summary>
//    /// Rimuove tutti i record dal file, troncandolo dopo l'header.
//    /// </summary>
//    public void Zap()
//    {
//        // Tronca il file alla fine dell'header.
//        _stream.SetLength(Header.HeaderLength);
//
//        // Aggiorna l'header in memoria e su disco per riflettere 0 record.
//        var newHeader = Header;
//        newHeader.NumberOfRecords = 0;
//        newHeader.Year = (byte)(DateTime.Now.Year % 100);
//        newHeader.Month = (byte)DateTime.Now.Month;
//        newHeader.Day = (byte)DateTime.Now.Day;
//        
//        Header = newHeader;
//
//        _stream.Seek(0, SeekOrigin.Begin);
//        var headerBytes = new byte[DbfHeader.Size];
//        var handle = GCHandle.Alloc(headerBytes, GCHandleType.Pinned);
//        try
//        {
//            Marshal.StructureToPtr(newHeader, handle.AddrOfPinnedObject(), false);
//            _stream.Write(headerBytes, 0, headerBytes.Length);
//        }
//        finally
//        {
//            handle.Free();
//        }
//        _stream.Flush();
//    }
//    
//    /// <summary>
//    /// Crea un nuovo file DBF vuoto su disco con una struttura di campi specificata.
//    /// Se il file esiste già, verrà sovrascritto.
//    /// </summary>
//    /// <param name="filePath">Il percorso completo del nuovo file .dbf da creare.</param>
//    /// <param name="encoding">La codifica dei caratteri da utilizzare per il nuovo file.</param>
//    /// <param name="fieldCollection">Una collezione di oggetti <see cref="DbfField"/> che definisce la struttura del file.</param>
//    public static void CreateNew(string filePath, Encoding encoding, IEnumerable<DbfField> fieldCollection)
//    {
//        var fields = fieldCollection.ToList();
//        
//        var headerLength = (ushort)(32 + fields.Count * 32 + 1);
//        var recordLength = (ushort)(1 + fields.Sum(f => f.Length));
//
//        var languageDriver = DbfCodePage.GetLanguageDriver(encoding) ?? 0x03;
//
//        using var fs = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None);
//        using var writer = new BinaryWriter(fs, encoding);
//        var now = DateTime.Now;
//        writer.Write((byte)DbfVersion.VisualFoxPro);
//        writer.Write((byte)(now.Year - 1900));
//        writer.Write((byte)now.Month);
//        writer.Write((byte)now.Day);
//        writer.Write((uint)0);
//        writer.Write(headerLength);
//        writer.Write(recordLength);
//        writer.Write(new byte[17]);
//        writer.Write(languageDriver);
//        writer.Write(new byte[2]);
//
//        uint fieldOffset = 1;
//        foreach (var field in fields)
//        {
//            var nameBytes = new byte[11];
//            var fieldNameBytes = encoding.GetBytes(field.Name);
//            Buffer.BlockCopy(fieldNameBytes, 0, nameBytes, 0, Math.Min(fieldNameBytes.Length, 10));
//            writer.Write(nameBytes);
//            writer.Write((byte)field.Type);
//            writer.Write(fieldOffset);
//            writer.Write(field.Length);
//            writer.Write(field.DecimalCount);
//            writer.Write(new byte[14]);
//            fieldOffset += field.Length;
//        }
//
//        writer.Write((byte)0x0D);
//        writer.Write((byte)0x1A);
//    }
//        
//    private static DbfHeader BumpHeaderRecordCount(DbfHeader header, uint add = 1)
//    {
//        header.NumberOfRecords += add;
//        header.Year = (byte)(DateTime.Now.Year % 100);
//        header.Month = (byte)DateTime.Now.Month;
//        header.Day = (byte)DateTime.Now.Day;
//        return header;
//    }
//
//    private void WriteHeaderToDisk(DbfHeader header)
//    {
//        _stream.Seek(0, SeekOrigin.Begin);
//        var headerBytes = new byte[DbfHeader.Size];
//        var handle = GCHandle.Alloc(headerBytes, GCHandleType.Pinned);
//        try
//        {
//            Marshal.StructureToPtr(header, handle.AddrOfPinnedObject(), false);
//            _stream.Write(headerBytes, 0, headerBytes.Length);
//        }
//        finally
//        {
//            handle.Free();
//        }
//    }
//
//    private void ValidateRecordNumber(int recNo)
//    {
//        if (recNo <= 0 || recNo > Header.NumberOfRecords)
//            throw new ArgumentOutOfRangeException(nameof(recNo), "Numero di record non valido.");
//    }
//    
//    /// <summary>
//    /// Rilascia le risorse associate al lettore.
//    /// </summary>
//    public void Dispose()
//    {
//        if (_disposed) return;
//        FlushPendingHeader();
//        _writer.Dispose();
//        _reader.Dispose();
//        _stream.Dispose();
//        _disposed = true;
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

struct DbfFile {
    FILE* stream;
    int disposed;
    int headerDirty;
    DbfHeader Header;
    DbfField* Fields;
    int FieldCount;
    char FilePath[PATH_MAX];
};

static void DbfFile_now(uint8_t* y, uint8_t* m, uint8_t* d)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    *y = (uint8_t)((tm.tm_year + 1900) % 100);
    *m = (uint8_t)(tm.tm_mon + 1);
    *d = (uint8_t)tm.tm_mday;
}

static DbfHeader DbfFile_BumpHeaderRecordCount(DbfHeader header, uint32_t add)
{
    uint8_t y, m, d;
    header.NumberOfRecords += add;
    DbfFile_now(&y, &m, &d);
    header.Year = y;
    header.Month = m;
    header.Day = d;
    return header;
}

static int DbfFile_ValidateRecordNumber(const DbfFile* self, int recNo)
{
    if (recNo <= 0 || recNo > (int)self->Header.NumberOfRecords) return GMB_DB_ARG;
    return GMB_DB_OK;
}

static int DbfFile_WriteHeaderToDisk(DbfFile* self, DbfHeader header)
{
    uint8_t headerBytes[DbfHeader_Size];
    fseek(self->stream, 0, SEEK_SET);
    DbfHeader_ToBytes(&header, headerBytes);
    return fwrite(headerBytes, 1, DbfHeader_Size, self->stream) == DbfHeader_Size ? GMB_DB_OK : GMB_DB_ERR;
}

static long DbfFile_file_len(FILE* f)
{
    long cur = ftell(f), end;
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

DbfFile* DbfFile_New(const char* filePath, int exclusive, LockingStrategy lockingStrategy)
{
    DbfFile* self;
    (void)exclusive;
    (void)lockingStrategy;
    /* ponytail: multi work area / cross-process — record locks via ILockManager, not OS exclusive FileShare */
    self = (DbfFile*)calloc(1, sizeof(*self));
    snprintf(self->FilePath, sizeof(self->FilePath), "%s", filePath);
    self->stream = fopen(filePath, "rb+");
    if (!self->stream) { free(self); return NULL; }
    if (DbfHeader_Read(self->stream, &self->Header) != GMB_DB_OK) {
        fclose(self->stream);
        free(self);
        return NULL;
    }
    if (DbfField_ReadAll(self->stream, &self->Fields, &self->FieldCount) != GMB_DB_OK) {
        fclose(self->stream);
        free(self);
        return NULL;
    }
    return self;
}

void DbfFile_Dispose(DbfFile* self)
{
    if (!self || self->disposed) return;
    DbfFile_FlushPendingHeader(self);
    if (self->stream) fclose(self->stream);
    free(self->Fields);
    self->disposed = 1;
}

int64_t DbfFile_GetFileLength(const DbfFile* self)
{
    return self ? (int64_t)DbfFile_file_len(self->stream) : 0;
}

int DbfFile_ReadRawRecordInto(DbfFile* self, int recNo, uint8_t* buffer, int bufferLen, int* isDeleted)
{
    int64_t offset;
    if (!self || !buffer || !isDeleted) return GMB_DB_ARG;
    if (bufferLen < self->Header.RecordLength) return GMB_DB_ARG;
    if (DbfFile_ValidateRecordNumber(self, recNo) != GMB_DB_OK) return GMB_DB_ARG;
    offset = (int64_t)self->Header.HeaderLength + ((int64_t)(recNo - 1) * self->Header.RecordLength);
    fseek(self->stream, (long)offset, SEEK_SET);
    if (fread(buffer, 1, self->Header.RecordLength, self->stream) != self->Header.RecordLength) return GMB_DB_ERR;
    *isDeleted = buffer[0] == '*';
    return GMB_DB_OK;
}

int DbfFile_ReadRawRecord(DbfFile* self, int recNo, int* isDeleted, uint8_t** data, int* dataLen)
{
    int recLen = self->Header.RecordLength;
    uint8_t* buffer = (uint8_t*)malloc((size_t)recLen);
    int rc = DbfFile_ReadRawRecordInto(self, recNo, buffer, recLen, isDeleted);
    if (rc != GMB_DB_OK) { free(buffer); return rc; }
    *dataLen = recLen - 1;
    *data = (uint8_t*)malloc((size_t)*dataLen);
    memcpy(*data, buffer + 1, (size_t)*dataLen);
    free(buffer);
    return GMB_DB_OK;
}

int DbfFile_WriteRawRecord(DbfFile* self, int recNo, const uint8_t* rawData, int rawLen)
{
    int64_t offset;
    (void)rawLen;
    if (DbfFile_ValidateRecordNumber(self, recNo) != GMB_DB_OK) return GMB_DB_ARG;
    offset = (int64_t)self->Header.HeaderLength + ((int64_t)(recNo - 1) * self->Header.RecordLength) + 1;
    fseek(self->stream, (long)offset, SEEK_SET);
    fwrite(rawData, 1, (size_t)(self->Header.RecordLength - 1), self->stream);
    return GMB_DB_OK;
}

int DbfFile_AppendRawRecord(DbfFile* self, const uint8_t* rawData, int rawLen)
{
    int64_t offset = (int64_t)self->Header.HeaderLength + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    int64_t requiredLength = offset + self->Header.RecordLength;
    uint8_t space = (uint8_t)' ';
    (void)rawLen;
    if (DbfFile_file_len(self->stream) < requiredLength) {
        /* SetLength */
        int fd = fileno(self->stream);
        if (ftruncate(fd, (off_t)requiredLength) != 0) return GMB_DB_ERR;
    }
    fseek(self->stream, (long)offset, SEEK_SET);
    fwrite(&space, 1, 1, self->stream);
    fwrite(rawData, 1, (size_t)(self->Header.RecordLength - 1), self->stream);
    self->Header = DbfFile_BumpHeaderRecordCount(self->Header, 1);
    self->headerDirty = 1;
    return (int)self->Header.NumberOfRecords;
}

int DbfFile_AppendRawRecordsBatch(DbfFile* self, const uint8_t* rawPayload, int recordPayloadLength, int count)
{
    int startRecNo, i;
    int64_t offset, totalBytes, requiredLength;
    int recordLength;
    uint8_t space = (uint8_t)' ';
    if (count <= 0) return GMB_DB_ARG;
    if (rawPayload == NULL) return GMB_DB_ARG;
    startRecNo = (int)self->Header.NumberOfRecords + 1;
    offset = (int64_t)self->Header.HeaderLength + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    recordLength = self->Header.RecordLength;
    totalBytes = (int64_t)recordLength * count;
    requiredLength = offset + totalBytes;
    if (DbfFile_file_len(self->stream) < requiredLength) {
        if (ftruncate(fileno(self->stream), (off_t)requiredLength) != 0) return GMB_DB_ERR;
    }
    fseek(self->stream, (long)offset, SEEK_SET);
    for (i = 0; i < count; i++) {
        fwrite(&space, 1, 1, self->stream);
        fwrite(rawPayload + (size_t)i * (size_t)recordPayloadLength, 1, (size_t)recordPayloadLength, self->stream);
    }
    self->Header = DbfFile_BumpHeaderRecordCount(self->Header, (uint32_t)count);
    self->headerDirty = 1;
    return startRecNo;
}

int DbfFile_ReloadHeaderFromDisk(DbfFile* self)
{
    FILE* probe = fopen(self->FilePath, "rb");
    int64_t expectedLength;
    if (!probe) return GMB_DB_ERR;
    if (DbfHeader_Read(probe, &self->Header) != GMB_DB_OK) { fclose(probe); return GMB_DB_ERR; }
    fclose(probe);
    self->headerDirty = 0;
    expectedLength = (int64_t)self->Header.HeaderLength + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    if (DbfFile_file_len(self->stream) < expectedLength)
        ftruncate(fileno(self->stream), (off_t)expectedLength);
    return GMB_DB_OK;
}

int DbfFile_FlushPendingHeader(DbfFile* self)
{
    if (!self->headerDirty) return GMB_DB_OK;
    if (DbfFile_WriteHeaderToDisk(self, self->Header) != GMB_DB_OK) return GMB_DB_ERR;
    self->headerDirty = 0;
    return GMB_DB_OK;
}

int DbfFile_RestoreHeader(DbfFile* self, const DbfHeader* header)
{
    int64_t expectedLength;
    if (!self || !header) return GMB_DB_ARG;
    self->Header = *header;
    self->headerDirty = 1;
    if (DbfFile_FlushPendingHeader(self) != GMB_DB_OK) return GMB_DB_ERR;
    expectedLength = (int64_t)self->Header.HeaderLength
        + (int64_t)self->Header.NumberOfRecords * self->Header.RecordLength;
    if (ftruncate(fileno(self->stream), (off_t)expectedLength) != 0) return GMB_DB_ERR;
    return GMB_DB_OK;
}

int DbfFile_FlushStream(DbfFile* self, int durable, int flushHeaderFirst)
{
    if (flushHeaderFirst) DbfFile_FlushPendingHeader(self);
    fflush(self->stream);
    if (durable) fsync(fileno(self->stream));
    return GMB_DB_OK;
}

int DbfFile_SetDeletedFlag(DbfFile* self, int recNo, int isDeleted)
{
    int64_t offset;
    uint8_t flag;
    if (DbfFile_ValidateRecordNumber(self, recNo) != GMB_DB_OK) return GMB_DB_ARG;
    offset = (int64_t)self->Header.HeaderLength + ((int64_t)(recNo - 1) * self->Header.RecordLength);
    fseek(self->stream, (long)offset, SEEK_SET);
    flag = isDeleted ? (uint8_t)'*' : (uint8_t)' ';
    fwrite(&flag, 1, 1, self->stream);
    return GMB_DB_OK;
}

int DbfFile_CloneStructure(DbfFile* self, const char* newFilePath)
{
    FILE* newStream;
    int headerSize = self->Header.HeaderLength;
    uint8_t* headerBuffer;
    uint8_t y, m, d;
    time_t t = time(NULL);
    struct tm tm;
    newStream = fopen(newFilePath, "wb");
    if (!newStream) return GMB_DB_ERR;
    headerBuffer = (uint8_t*)calloc(1, (size_t)headerSize);
    fseek(self->stream, 0, SEEK_SET);
    if (fread(headerBuffer, 1, (size_t)headerSize, self->stream) != (size_t)headerSize) {
        free(headerBuffer); fclose(newStream); return GMB_DB_ERR;
    }
    localtime_r(&t, &tm);
    y = (uint8_t)(tm.tm_year); /* Year - 1900: tm_year is already years since 1900 */
    m = (uint8_t)(tm.tm_mon + 1);
    d = (uint8_t)tm.tm_mday;
    headerBuffer[1] = y;
    headerBuffer[2] = m;
    headerBuffer[3] = d;
    memset(headerBuffer + 4, 0, 4);
    fwrite(headerBuffer, 1, (size_t)headerSize, newStream);
    free(headerBuffer);
    fclose(newStream);
    return GMB_DB_OK;
}

int DbfFile_Zap(DbfFile* self)
{
    uint8_t y, m, d;
    uint8_t headerBytes[DbfHeader_Size];
    if (ftruncate(fileno(self->stream), (off_t)self->Header.HeaderLength) != 0) return GMB_DB_ERR;
    self->Header.NumberOfRecords = 0;
    DbfFile_now(&y, &m, &d);
    self->Header.Year = y;
    self->Header.Month = m;
    self->Header.Day = d;
    fseek(self->stream, 0, SEEK_SET);
    DbfHeader_ToBytes(&self->Header, headerBytes);
    fwrite(headerBytes, 1, DbfHeader_Size, self->stream);
    fflush(self->stream);
    return GMB_DB_OK;
}

int DbfFile_CreateNew(const char* filePath, const char* encodingWebName, const DbfField* fields, int fieldCount)
{
    FILE* fs;
    uint16_t headerLength, recordLength;
    int languageDriver, i;
    uint32_t fieldOffset = 1;
    time_t t = time(NULL);
    struct tm tm;
    uint8_t zeros17[17] = {0}, zeros2[2] = {0}, zeros14[14] = {0};
    uint8_t term0d = 0x0D, eof1a = 0x1A, ver = (uint8_t)DbfVersion_VisualFoxPro;
    uint8_t y, mo, d;
    uint32_t recCount = 0;
    localtime_r(&t, &tm);
    y = (uint8_t)tm.tm_year;
    mo = (uint8_t)(tm.tm_mon + 1);
    d = (uint8_t)tm.tm_mday;
    headerLength = (uint16_t)(32 + fieldCount * 32 + 1);
    recordLength = 1;
    for (i = 0; i < fieldCount; i++) recordLength = (uint16_t)(recordLength + fields[i].Length);
    languageDriver = DbfCodePage_GetLanguageDriver(encodingWebName);
    if (languageDriver < 0) languageDriver = 0x03;
    fs = fopen(filePath, "wb");
    if (!fs) return GMB_DB_ERR;
    fwrite(&ver, 1, 1, fs);
    fwrite(&y, 1, 1, fs);
    fwrite(&mo, 1, 1, fs);
    fwrite(&d, 1, 1, fs);
    fwrite(&recCount, 4, 1, fs);
    fwrite(&headerLength, 2, 1, fs);
    fwrite(&recordLength, 2, 1, fs);
    fwrite(zeros17, 1, 17, fs);
    { uint8_t ld = (uint8_t)languageDriver; fwrite(&ld, 1, 1, fs); }
    fwrite(zeros2, 1, 2, fs);
    for (i = 0; i < fieldCount; i++) {
        uint8_t nameBytes[11] = {0};
        size_t nl = strlen(fields[i].Name);
        if (nl > 10) nl = 10;
        memcpy(nameBytes, fields[i].Name, nl);
        fwrite(nameBytes, 1, 11, fs);
        fwrite(&fields[i].Type, 1, 1, fs);
        fwrite(&fieldOffset, 4, 1, fs);
        fwrite(&fields[i].Length, 1, 1, fs);
        fwrite(&fields[i].DecimalCount, 1, 1, fs);
        fwrite(zeros14, 1, 14, fs);
        fieldOffset += fields[i].Length;
    }
    fwrite(&term0d, 1, 1, fs);
    fwrite(&eof1a, 1, 1, fs);
    fclose(fs);
    return GMB_DB_OK;
}

const DbfHeader* DbfFile_get_Header(const DbfFile* self) { return &self->Header; }
const DbfField* DbfFile_get_Fields(const DbfFile* self, int* count)
{
    if (count) *count = self->FieldCount;
    return self->Fields;
}
const char* DbfFile_get_FilePath(const DbfFile* self) { return self->FilePath; }
