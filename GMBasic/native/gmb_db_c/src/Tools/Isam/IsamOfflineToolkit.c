/* FILE: native/gmb_db_c/src/Tools/Isam/IsamOfflineToolkit.c */
/* Port 1:1 di modules/Database/Tools/Isam/IsamOfflineToolkit.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../../include/gmb_db_port.h"
#include <unistd.h>

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamOfflineToolkit
//
//using System.Collections.Immutable;
//using System.Runtime.InteropServices;
//using System.Text.Json;
//using Basic.Databases;
//using Basic.Databases.Replication;
//using Basic.ISAM;
//
//namespace Basic.Tools.Isam;
//
///// <summary>
///// Offline read helpers for ISAM tooling (CLI, REPL, tests) without the BASIC interpreter.
///// Schema is always loaded from the embedded ISCH block in the file.
///// </summary>
//public static class IsamOfflineToolkit
//{
//    public const int PageSize = 4096;
//
//    /// <summary>
//    /// Replays WAL entries into the main ISAM file (same as engine startup recovery).
//    /// </summary>
//    /// <param name="filePath">Path to the .isdb file.</param>
//    public static void RecoverWal(string filePath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
//        WriteAheadLog.Recover(filePath);
//    }
//
//    /// <summary>
//    /// Repairs a corrupt ISAM file offline: WAL recover, header prep, manifest refresh, index rebuild.
//    /// </summary>
//    /// <param name="filePath">Path to the .isdb file.</param>
//    /// <param name="evaluator">Key expression evaluator for index operations.</param>
//    public static void Repair(string filePath, KeyExpressionEvaluatorCallback evaluator)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//        IsamRepair.PrepareForOpen(filePath);
//        using var engine = OpenEngine(filePath, evaluator);
//        engine.Repair(evaluator);
//    }
//
//    /// <summary>
//    /// Reads header, schema, and file size without opening a full engine.
//    /// </summary>
//    /// <param name="filePath">Path to the .isdb file.</param>
//    /// <returns>Header + schema snapshot.</returns>
//    public static IsamFileSnapshot ReadSnapshot(string filePath)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
//        if (!File.Exists(filePath))
//        {
//            throw new FileNotFoundException("ISAM file not found.", filePath);
//        }
//
//        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
//        var header = ReadHeader(stream);
//        ValidateMagic(header);
//
//        stream.Seek(header.SchemaOffset, SeekOrigin.Begin);
//        var schemaBuffer = new byte[header.SchemaLength];
//        stream.ReadExactly(schemaBuffer, 0, schemaBuffer.Length);
//        var schema = SchemaManager.DeserializeSchema(schemaBuffer);
//
//        return new IsamFileSnapshot(
//            filePath,
//            header,
//            schema,
//            stream.Length);
//    }
//
//    /// <summary>
//    /// Opens an <see cref="Engine"/> using schema from disk and a synthetic UDT for record I/O.
//    /// </summary>
//    /// <param name="filePath">Path to the .isdb file.</param>
//    /// <param name="evaluator">Key expression evaluator for index operations.</param>
//    /// <returns>Opened engine; caller must dispose.</returns>
//    public static Engine OpenEngine(string filePath, KeyExpressionEvaluatorCallback evaluator)
//    {
//        var snapshot = ReadSnapshot(filePath);
//        var udt = BuildMinimalUdt(snapshot.Schema);
//        var handle = new IsamFileHandle(filePath, snapshot.Schema, udt, LockingStrategy.IntraProcess);
//        return new Engine(handle) { Evaluator = evaluator };
//    }
//
//    /// <summary>
//    /// Replays a <c>.repl</c> sidecar log onto a secondary ISAM file.
//    /// </summary>
//    /// <param name="replLogPath">Path to the replication log.</param>
//    /// <param name="targetIsdbPath">Path to the secondary .isdb file.</param>
//    /// <param name="evaluator">Key expression evaluator for index operations.</param>
//    /// <returns>Number of log lines applied.</returns>
//    public static int ApplyReplicationLog(
//        string replLogPath,
//        string targetIsdbPath,
//        KeyExpressionEvaluatorCallback evaluator)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentException.ThrowIfNullOrWhiteSpace(targetIsdbPath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//        using var engine = OpenEngine(targetIsdbPath, evaluator);
//        return IsamReplicationApplier.Apply(replLogPath, engine, evaluator);
//    }
//
//    public static int FollowReplicationLog(
//        string replLogPath,
//        string targetIsdbPath,
//        KeyExpressionEvaluatorCallback evaluator,
//        string? checkpointPath = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(replLogPath);
//        ArgumentException.ThrowIfNullOrWhiteSpace(targetIsdbPath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//        using var engine = OpenEngine(targetIsdbPath, evaluator);
//        return IsamReplicationApplier.Follow(replLogPath, engine, evaluator, checkpointPath);
//    }
//
//    /// <summary>
//    /// Ships pending <c>.repl</c> lines to a TCP receiver (primary side).
//    /// </summary>
//    public static int ShipReplicationTcp(
//        string replLogPath,
//        string host,
//        int port,
//        string? shipCheckpointPath = null) =>
//        ReplicationTcpShipper.ShipOnce(replLogPath, host, port, shipCheckpointPath);
//
//    /// <summary>
//    /// Listens once on <paramref name="port"/> and applies received replication lines to a secondary ISAM file.
//    /// </summary>
//    public static int ReceiveReplicationTcp(
//        int port,
//        string targetIsdbPath,
//        KeyExpressionEvaluatorCallback evaluator,
//        string? checkpointPath = null)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(targetIsdbPath);
//        ArgumentNullException.ThrowIfNull(evaluator);
//        using var engine = OpenEngine(targetIsdbPath, evaluator);
//        engine.SetReplication(string.Empty);
//        return ReplicationTcpReceiver.ReceiveOnce(
//            port,
//            (state, line) => IsamReplicationApplier.ApplyLine(line, state, engine, evaluator),
//            checkpointPath);
//    }
//
//    /// <summary>
//    /// Builds a minimal <see cref="UserDefinedType"/> from an on-disk schema (field names + CLR types only).
//    /// </summary>
//    /// <param name="schema">On-disk ISAM schema.</param>
//    /// <returns>Runtime UDT for record I/O.</returns>
//    public static UserDefinedType BuildMinimalUdt(Config.IsamSchema schema)
//    {
//        ArgumentNullException.ThrowIfNull(schema);
//        var fields = new List<TypeField>(schema.Fields.Count);
//        var nextId = 3000;
//        foreach (var field in schema.Fields)
//        {
//            fields.Add(new TypeField(
//                nextId++,
//                Token(field.Name),
//                field.DataType,
//                ImmutableArray<Expr>.Empty,
//                null));
//        }
//
//        var typeToken = Token(schema.TypeName);
//        return new UserDefinedType(nextId++, typeToken, [..fields], SymbolFlags.None)
//        {
//            OriginalName = typeToken,
//        };
//    }
//
//    /// <summary>
//    /// Reads one B+ page (4096 bytes) and checksum metadata.
//    /// </summary>
//    /// <param name="filePath">Path to the .isdb file.</param>
//    /// <param name="pageNumber">Zero-based page index.</param>
//    /// <returns>Page bytes and checksum validation result.</returns>
//    public static IsamPageInspection InspectPage(string filePath, int pageNumber)
//    {
//        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);
//        if (pageNumber < 0)
//        {
//            throw new ArgumentOutOfRangeException(nameof(pageNumber));
//        }
//
//        var offset = (long)pageNumber * PageSize;
//        var page = new byte[PageSize];
//        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
//        if (offset + PageSize > stream.Length)
//        {
//            throw new ArgumentOutOfRangeException(
//                nameof(pageNumber),
//                $"Page {pageNumber} at offset {offset} extends past file length {stream.Length}.");
//        }
//
//        stream.Seek(offset, SeekOrigin.Begin);
//        stream.ReadExactly(page, 0, PageSize);
//
//        var checksumOk = IsamPageChecksum.TryValidate(page, out var storedChecksum);
//        return new IsamPageInspection(pageNumber, offset, page, storedChecksum, checksumOk);
//    }
//
//    /// <summary>
//    /// Serializes schema metadata to JSON for CLI export.
//    /// </summary>
//    /// <param name="schema">ISAM schema.</param>
//    /// <returns>Indented JSON.</returns>
//    public static string SchemaToJson(Config.IsamSchema schema)
//    {
//        ArgumentNullException.ThrowIfNull(schema);
//        var payload = new
//        {
//            schema.TypeName,
//            PrimaryKey = IndexToJson(schema.PrimaryKey),
//            SecondaryKeys = schema.SecondaryKeys.Select(IndexToJson).ToArray(),
//            Fields = schema.Fields.Select(f => new
//            {
//                f.Name,
//                Type = f.DataType.GetType().Name,
//                f.Length,
//            }).ToArray(),
//        };
//
//        return JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true });
//    }
//
//    private static object IndexToJson(Config.IndexDefinition index) => new
//    {
//        index.Name,
//        index.KeyExpression,
//        index.KeyLength,
//        index.IsUnique,
//        index.IsUnsafe,
//    };
//
//    private static Config.FileHeader ReadHeader(Stream stream)
//    {
//        stream.Seek(0, SeekOrigin.Begin);
//        var headerSize = Marshal.SizeOf<Config.FileHeader>();
//        var buffer = new byte[headerSize];
//        stream.ReadExactly(buffer, 0, buffer.Length);
//        using var ms = new MemoryStream(buffer);
//        return MarshalHelper.ReadStructure<Config.FileHeader>(ms);
//    }
//
//    private static void ValidateMagic(Config.FileHeader header)
//    {
//        for (var i = 0; i < Config.MagicBytes.Length; i++)
//        {
//            if (header.Magic[i] != Config.MagicBytes[i])
//            {
//                throw new InvalidDataException("Not a valid ISAM file.");
//            }
//        }
//    }
//
//    private static Token Token(string lexeme) =>
//        new(TokenKind.Identifier, lexeme, null, new SourcePosition(0, 1, 1));
//}
//
///// <summary>Header + schema snapshot for offline tooling.</summary>
//public sealed record IsamFileSnapshot(
//    string FilePath,
//    Config.FileHeader Header,
//    Config.IsamSchema Schema,
//    long FileSize);
//
///// <summary>Single B+ page inspection result.</summary>
//public sealed record IsamPageInspection(
//    int PageNumber,
//    long Offset,
//    byte[] PageBytes,
//    uint StoredChecksum,
//    bool ChecksumValid);

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

int IsamOfflineToolkit_RecoverWal(const char* filePath)
{
    if (!filePath || !filePath[0]) return GMB_DB_ARG;
    WriteAheadLog_Recover(filePath);
    return GMB_DB_OK;
}

int IsamOfflineToolkit_ReadHeader(const char* filePath, IsamFileHeader* out)
{
    FILE* f;
    int rc;
    if (!filePath || !out) return GMB_DB_ARG;
    f = fopen(filePath, "rb");
    if (!f) return GMB_DB_ERR;
    rc = IsamFileHeader_Read(f, out);
    fclose(f);
    return rc;
}

Engine* IsamOfflineToolkit_OpenEngine(const char* filePath)
{
    if (!filePath || !filePath[0]) return NULL;
    return Engine_Open(filePath, NULL);
}

int IsamOfflineToolkit_Repair(const char* filePath)
{
    Engine* e;
    int rc;
    if (!filePath || !filePath[0]) return GMB_DB_ARG;
    rc = IsamRepair_PrepareForOpen(filePath);
    if (rc != GMB_DB_OK) return rc;
    e = IsamOfflineToolkit_OpenEngine(filePath);
    if (!e) return GMB_DB_ERR;
    rc = Engine_Reindex(e);
    Engine_Dispose(e);
    return rc;
}

int IsamOfflineToolkit_ApplyReplicationLog(const char* replLogPath, const char* targetIsdbPath)
{
    Engine* e;
    int n;
    if (!replLogPath || !targetIsdbPath) return -1;
    e = IsamOfflineToolkit_OpenEngine(targetIsdbPath);
    if (!e) return -1;
    n = IsamReplicationApplier_Apply(replLogPath, e);
    Engine_Dispose(e);
    return n;
}

int IsamOfflineToolkit_FollowReplicationLog(const char* replLogPath, const char* targetIsdbPath,
                                            const char* checkpointPath)
{
    Engine* e;
    int n;
    if (!replLogPath || !targetIsdbPath) return -1;
    e = IsamOfflineToolkit_OpenEngine(targetIsdbPath);
    if (!e) return -1;
    n = IsamReplicationApplier_Follow(replLogPath, e, checkpointPath);
    Engine_Dispose(e);
    return n;
}

int IsamOfflineToolkit_SelfCheck(void)
{
    char path[] = "native/gmb_db_c/build/isam_toolkit_selfcheck.isdb";
    IsamFieldSchema fields[2];
    IsamSchema schema;
    Engine *e, *opened;
    IsamFieldValue vals[2];
    IsamFileHeader hdr;
    memset(&schema, 0, sizeof(schema));
    memset(fields, 0, sizeof(fields));
    snprintf(schema.TypeName, sizeof(schema.TypeName), "%s", "Person");
    snprintf(fields[0].Name, sizeof(fields[0].Name), "%s", "Id");
    snprintf(fields[0].DataTypeCode, sizeof(fields[0].DataTypeCode), "%s", "IntegerType");
    snprintf(fields[1].Name, sizeof(fields[1].Name), "%s", "Name");
    snprintf(fields[1].DataTypeCode, sizeof(fields[1].DataTypeCode), "%s", "StringType");
    fields[1].Length = 20;
    schema.Fields = fields;
    schema.FieldCount = 2;
    snprintf(schema.PrimaryKey.Name, sizeof(schema.PrimaryKey.Name), "%s", "Id");
    snprintf(schema.PrimaryKey.KeyExpression, sizeof(schema.PrimaryKey.KeyExpression), "%s", "Id");
    schema.PrimaryKey.KeyLength = 10;
    schema.PrimaryKey.IsUnique = 1;
    unlink(path);
    if (IsamFileHandle_CreateNew(path, &schema) != GMB_DB_OK) return -460;
    e = Engine_Open(path, &schema);
    if (!e) return -461;
    memset(vals, 0, sizeof(vals));
    vals[0].i64 = 1;
    vals[1].text = "N1";
    if (Engine_Append(e, vals, 2) != GMB_DB_OK) { Engine_Dispose(e); return -462; }
    Engine_Dispose(e);
    if (IsamOfflineToolkit_RecoverWal(path) != GMB_DB_OK) return -463;
    if (IsamOfflineToolkit_ReadHeader(path, &hdr) != GMB_DB_OK) return -464;
    if (!IsamFileHeader_ValidateMagic(&hdr)) return -465;
    opened = IsamOfflineToolkit_OpenEngine(path);
    if (!opened) return -466;
    if (Engine_GetTotalRecordCount(opened) != 1) { Engine_Dispose(opened); return -467; }
    Engine_Dispose(opened);
    if (IsamOfflineToolkit_Repair(path) != GMB_DB_OK) return -468;
    unlink(path);
    return GMB_DB_OK;
}
