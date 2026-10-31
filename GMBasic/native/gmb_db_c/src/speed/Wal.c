/* FILE: native/gmb_db_c/src/SPEED/Wal.c */
/* Port 1:1 di modules/Database/SPEED/Wal.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Wal.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Text;
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Gestisce il Write-Ahead Log per il motore Speed.
///// Tutte le operazioni di modifica vengono prima scritte in modo sequenziale su questo log
///// per garantire la durabilità e il recupero in caso di crash.
///// </summary>
//internal class WriteAheadLog : IDisposable
//{
//    private readonly FileStream _logStream;
//    private readonly BinaryWriter _writer;
//    private readonly string _walFilePath;
//
//    /// <summary>
//    /// Inizializza o apre un file WAL per un dato database.
//    /// </summary>
//    /// <param name="databasePath">La directory radice del database.</param>
//    /// <param name="logSequenceNumber">Il numero sequenziale di questo file di log.</param>
//    public WriteAheadLog(string databasePath, ulong logSequenceNumber)
//    {
//        var walDirectory = Path.Combine(databasePath, Config.WalDirectoryName);
//        Directory.CreateDirectory(walDirectory);
//
//        _walFilePath = Path.Combine(walDirectory, $"{logSequenceNumber:D6}.wal");
//        _logStream = new FileStream(_walFilePath, FileMode.Append, FileAccess.Write, FileShare.None);
//        _writer = new BinaryWriter(_logStream, Encoding.UTF8, false);
//    }
//
//    /// <summary>
//    /// Restituisce il percorso completo del file di log gestito da questa istanza.
//    /// </summary>
//    /// <returns>La stringa del percorso del file .wal.</returns>
//    public string GetWalPath() => _walFilePath;
//
//    /// <summary>
//    /// Registra un'operazione di inserimento/aggiornamento nel log.
//    /// </summary>
//    public void LogSet(int tableId, byte[] key, byte[] value)
//    {
//        lock (_writer)
//        {
//            _writer.Write((byte)WalOperationType.Set);
//            _writer.Write(tableId);
//            _writer.Write(key.Length);
//            _writer.Write(key);
//            _writer.Write(value.Length);
//            _writer.Write(value);
//        }
//    }
//
//    /// <summary>
//    /// Registra un'operazione di cancellazione nel log.
//    /// </summary>
//    public void LogDelete(int tableId, byte[] key)
//    {
//        lock (_writer)
//        {
//            _writer.Write((byte)WalOperationType.Delete);
//            _writer.Write(tableId);
//            _writer.Write(key.Length);
//            _writer.Write(key);
//        }
//    }
//
//    /// <summary>
//    /// Forza la scrittura di tutti i dati bufferizzati su disco.
//    /// </summary>
//    public void Flush()
//    {
//        _logStream.Flush(true);
//    }
//
//    /// <summary>
//    /// Esegue il recupero ripopolando le Memtable dai file di log esistenti.
//    /// </summary>
//    /// <param name="databasePath">La directory radice del database.</param>
//    /// <param name="memtables">Il dizionario di Memtable da popolare.</param>
//    public static void Recover(string databasePath, Dictionary<int, Memtable> memtables)
//    {
//        var walDirectory = Path.Combine(databasePath, Config.WalDirectoryName);
//        if (!Directory.Exists(walDirectory)) return;
//
//        var walFiles = Directory.GetFiles(walDirectory, "*.wal").OrderBy(f => f).ToList();
//
//        foreach (var walFile in walFiles)
//        {
//            using var stream = new FileStream(walFile, FileMode.Open, FileAccess.Read);
//            using var reader = new BinaryReader(stream, Encoding.UTF8);
//
//            while (stream.Position < stream.Length)
//            {
//                var opType = (WalOperationType)reader.ReadByte();
//                var tableId = reader.ReadInt32();
//
//                if (!memtables.TryGetValue(tableId, out var memtable))
//                {
//                    memtable = new Memtable();
//                    memtables[tableId] = memtable;
//                }
//
//                var keyLength = reader.ReadInt32();
//                var key = reader.ReadBytes(keyLength);
//
//                if (opType == WalOperationType.Set)
//                {
//                    var valueLength = reader.ReadInt32();
//                    var value = reader.ReadBytes(valueLength);
//                    memtable.Set(key, value);
//                }
//                else if (opType == WalOperationType.Delete)
//                {
//                    memtable.Delete(key);
//                }
//            }
//        }
//    }
//
//    public void Dispose()
//    {
//        _writer.Dispose();
//        _logStream.Dispose();
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>

static int __attribute__((unused)) wr_i32_le(FILE* f, int32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}
static int __attribute__((unused)) rd_i32_le(FILE* f, int32_t* v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
    return 0;
}
static int __attribute__((unused)) wr_i64_le(FILE* f, int64_t v)
{
    uint8_t b[8]; int i;
    for (i = 0; i < 8; i++) b[i] = (uint8_t)((uint64_t)v >> (8 * i));
    return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}
static int __attribute__((unused)) rd_i64_le(FILE* f, int64_t* v)
{
    uint8_t b[8]; uint64_t u = 0; int i;
    if (fread(b, 1, 8, f) != 8) return -1;
    for (i = 0; i < 8; i++) u |= (uint64_t)b[i] << (8 * i);
    *v = (int64_t)u;
    return 0;
}

struct SpeedWriteAheadLog {
    FILE* logStream;
    char walFilePath[PATH_MAX];
    pthread_mutex_t writerLock;
};

SpeedWriteAheadLog* SpeedWriteAheadLog_New(const char* databasePath, uint64_t logSequenceNumber)
{
    SpeedWriteAheadLog* self = (SpeedWriteAheadLog*)calloc(1, sizeof(*self));
    char walDirectory[PATH_MAX];
    snprintf(walDirectory, sizeof(walDirectory), "%s/%s", databasePath, Config_WalDirectoryName);
    mkdir(walDirectory, 0777);
    snprintf(self->walFilePath, sizeof(self->walFilePath), "%s/%06llu.wal", walDirectory,
             (unsigned long long)logSequenceNumber);
    self->logStream = fopen(self->walFilePath, "ab");
    pthread_mutex_init(&self->writerLock, NULL);
    return self;
}

void SpeedWriteAheadLog_Dispose(SpeedWriteAheadLog* self)
{
    if (!self) return;
    if (self->logStream) fclose(self->logStream);
    pthread_mutex_destroy(&self->writerLock);
    free(self);
}

const char* SpeedWriteAheadLog_GetWalPath(const SpeedWriteAheadLog* self) { return self->walFilePath; }

void SpeedWriteAheadLog_LogSet(SpeedWriteAheadLog* self, int tableId, const uint8_t* key, int keyLen,
                               const uint8_t* value, int valueLen)
{
    uint8_t op = (uint8_t)SpeedWalOperationType_Set;
    pthread_mutex_lock(&self->writerLock);
    fwrite(&op, 1, 1, self->logStream);
    wr_i32_le(self->logStream, tableId);
    wr_i32_le(self->logStream, keyLen);
    fwrite(key, 1, (size_t)keyLen, self->logStream);
    wr_i32_le(self->logStream, valueLen);
    fwrite(value, 1, (size_t)valueLen, self->logStream);
    pthread_mutex_unlock(&self->writerLock);
}

void SpeedWriteAheadLog_LogDelete(SpeedWriteAheadLog* self, int tableId, const uint8_t* key, int keyLen)
{
    uint8_t op = (uint8_t)SpeedWalOperationType_Delete;
    pthread_mutex_lock(&self->writerLock);
    fwrite(&op, 1, 1, self->logStream);
    wr_i32_le(self->logStream, tableId);
    wr_i32_le(self->logStream, keyLen);
    fwrite(key, 1, (size_t)keyLen, self->logStream);
    pthread_mutex_unlock(&self->writerLock);
}

void SpeedWriteAheadLog_Flush(SpeedWriteAheadLog* self)
{
    fflush(self->logStream);
    fsync(fileno(self->logStream));
}

int SpeedWriteAheadLog_Recover(const char* databasePath, Memtable*** memtables, int** tableIds, int* count)
{
    char walDirectory[PATH_MAX];
    DIR* dir;
    struct dirent* ent;
    char** files = NULL;
    int nfiles = 0, i, t;
    Memtable** tables = NULL;
    int* ids = NULL;
    int ntables = 0, cap = 0;

    snprintf(walDirectory, sizeof(walDirectory), "%s/%s", databasePath, Config_WalDirectoryName);
    dir = opendir(walDirectory);
    if (!dir) { *memtables = NULL; *tableIds = NULL; *count = 0; return GMB_DB_OK; }
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".wal") == 0) {
            files = (char**)realloc(files, (size_t)(nfiles + 1) * sizeof(char*));
            files[nfiles] = (char*)malloc(PATH_MAX);
            snprintf(files[nfiles], PATH_MAX, "%s/%s", walDirectory, ent->d_name);
            nfiles++;
        }
    }
    closedir(dir);
    /* OrderBy filename */
    for (i = 0; i < nfiles; i++)
        for (t = i + 1; t < nfiles; t++)
            if (strcmp(files[t], files[i]) < 0) {
                char* tmp = files[i]; files[i] = files[t]; files[t] = tmp;
            }
    for (i = 0; i < nfiles; i++) {
        FILE* stream = fopen(files[i], "rb");
        long end;
        if (!stream) continue;
        fseek(stream, 0, SEEK_END);
        end = ftell(stream);
        fseek(stream, 0, SEEK_SET);
        while (ftell(stream) < end) {
            uint8_t op;
            int32_t tableId, keyLength, valueLength;
            uint8_t *key, *value;
            Memtable* mem = NULL;
            int found = -1, k;
            if (fread(&op, 1, 1, stream) != 1) break;
            if (rd_i32_le(stream, &tableId) != 0) break;
            for (k = 0; k < ntables; k++) if (ids[k] == tableId) { found = k; break; }
            if (found < 0) {
                if (ntables >= cap) { cap = cap ? cap * 2 : 4; tables = (Memtable**)realloc(tables, (size_t)cap * sizeof(*tables)); ids = (int*)realloc(ids, (size_t)cap * sizeof(int)); }
                tables[ntables] = Memtable_New();
                ids[ntables] = tableId;
                found = ntables++;
            }
            mem = tables[found];
            if (rd_i32_le(stream, &keyLength) != 0) break;
            key = (uint8_t*)malloc((size_t)keyLength);
            if (keyLength > 0 && fread(key, 1, (size_t)keyLength, stream) != (size_t)keyLength) { free(key); break; }
            if (op == SpeedWalOperationType_Set) {
                if (rd_i32_le(stream, &valueLength) != 0) { free(key); break; }
                value = (uint8_t*)malloc((size_t)valueLength);
                if (valueLength > 0 && fread(value, 1, (size_t)valueLength, stream) != (size_t)valueLength) { free(key); free(value); break; }
                Memtable_Set(mem, key, keyLength, value, valueLength);
                free(value);
            } else if (op == SpeedWalOperationType_Delete) {
                Memtable_Delete(mem, key, keyLength);
            }
            free(key);
        }
        fclose(stream);
        free(files[i]);
    }
    free(files);
    *memtables = tables;
    *tableIds = ids;
    *count = ntables;
    return GMB_DB_OK;
}
