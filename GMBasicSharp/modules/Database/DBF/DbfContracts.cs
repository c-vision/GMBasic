// FILE: Basic.DBF.DbfContracts.cs

using System.Collections;
using System.Runtime.InteropServices;
using System.Text;
using Basic.Databases;

// ReSharper disable InconsistentNaming

namespace Basic.DBF;

#region Enumerations

// All'inizio del file, aggiungi l'enum mancante
public enum PageType : ushort 
{
    Internal = 0,
    Leaf = 1
}

/// <summary>
/// Rappresenta le diverse versioni del formato di file DBF.
/// </summary>
public enum DbfVersion
{
    Unknown = 0,
    DBase2 = 0x02,
    DBase3PlusWithMemo = 0x83,
    DBase4WithMemo = 0x8B,
    VisualFoxPro = 0x30,
    VisualFoxProWithMemo = 0xF5,
}

/// <summary>
/// Rappresenta i diversi tipi di campo supportati nei file DBF.
/// </summary>
public enum DbfFieldType : byte
{
    Character = (byte)'C',
    Date = (byte)'D',
    Float = (byte)'F',
    Logical = (byte)'L',
    Memo = (byte)'M',
    Numeric = (byte)'N',
    Integer = (byte)'I',
    Currency = (byte)'Y',
    DateTime = (byte)'T',
    Double = (byte)'B',
    General = (byte)'G',
    // VFP-specific
    Varchar = (byte)'V',
    Varbinary = (byte)'Q',
    Blob = (byte)'W',
    AutoIncrement = (byte)'+',
}

#endregion

#region Enum Extensions

/// <summary>
/// Metodi di estensione per gli enum del mondo DBF.
/// </summary>
internal static class DbfVersionExtensions
{
    /// <summary>
    /// Determina se una versione specifica del formato DBF supporta i campi memo.
    /// </summary>
    public static bool SupportsMemoFields(this DbfVersion version)
    {
        // ponytail: VFP 0x30 tables use .fpt sidecars like 0xF5 — both need memo I/O
        return version is DbfVersion.DBase3PlusWithMemo or DbfVersion.DBase4WithMemo
            or DbfVersion.VisualFoxPro or DbfVersion.VisualFoxProWithMemo;
    }
}

#endregion

#region Data Model

/// <summary>
/// Rappresenta l'header di un file DBF, contenente i metadati principali.
/// QUESTA VERSIONE È MODIFICABILE.
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct DbfHeader
{
    public const int Size = 32;

    public byte VersionByte;
    public byte Year;
    public byte Month;
    public byte Day;
    public uint NumberOfRecords;
    public ushort HeaderLength;
    public ushort RecordLength;
    private ushort _reserved1;
    private byte _incompleteTransaction;
    private byte _encryptionFlag;
    private uint _freeRecordThread;
    private ulong _reserved2;
    private byte _mdxFlag;
    public byte LanguageDriver;
    private ushort _reserved4;

    public DbfVersion Version => (DbfVersion)VersionByte;
    public Encoding Encoding => DbfCodePage.GetEncoding(LanguageDriver);

    public static DbfHeader Read(BinaryReader reader)
    {
        var headerBytes = reader.ReadBytes(Size);
        if (headerBytes.Length != Size)
            throw new EndOfStreamException($"Expected {Size} bytes for DBF header, got {headerBytes.Length}");
        
        var handle = GCHandle.Alloc(headerBytes, GCHandleType.Pinned);
        try
        {
            return (DbfHeader)Marshal.PtrToStructure(handle.AddrOfPinnedObject(), typeof(DbfHeader))!;
        }
        finally
        {
            handle.Free();
        }
    }
    
    /// <summary>
    /// Serializza questa istanza della struct DbfHeader in un array di byte.
    /// </summary>
    /// <param name="header">L'istanza dell'header da serializzare.</param>
    /// <returns>Un array di byte che rappresenta l'header.</returns>
    public static byte[] ToBytes(DbfHeader header)
    {
        var buffer = new byte[Size];
        var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
        try
        {
            Marshal.StructureToPtr(header, handle.AddrOfPinnedObject(), false);
            return buffer;
        }
        finally
        {
            handle.Free();
        }
    }
}

/// <summary>
/// Rappresenta la definizione di un campo (colonna) in un file DBF.
/// </summary>
public readonly struct DbfField
{
    public const int StructSize = 32;
    public readonly string Name;
    public readonly DbfFieldType Type;
    public readonly byte Length;
    public readonly byte DecimalCount;
    private readonly uint _address; // Usato da VFP, ignorato per ora

    public DbfField(string name, DbfFieldType type, byte length, byte decimalCount, uint address)
    {
        Name = name;
        Type = type;
        Length = length;
        DecimalCount = decimalCount;
        _address = address;
    }

    /// <summary>Crea una definizione campo per nuovi file DBF (interop ISAM export).</summary>
    internal static DbfField Create(string name, DbfFieldType type, byte length, byte decimalCount = 0) =>
        new(name, type, length, decimalCount, 0);

    public static DbfField[] ReadAll(BinaryReader reader, Encoding encoding)
    {
        var fields = new List<DbfField>();
        // I campi iniziano subito dopo l'header e terminano con il byte 0x0D
        while (reader.BaseStream.Position < reader.BaseStream.Length && reader.PeekChar() != 0x0D)
        {
            // Usa ReadBytes per leggere la definizione del campo
            var buffer = reader.ReadBytes(StructSize);
            if (buffer.Length < StructSize || buffer[0] == 0x0D) break; // Sicurezza aggiuntiva

            var nameBytes = buffer.AsSpan(0, 11).TrimEnd((byte)0);
            var name = encoding.GetString(nameBytes);
            var type = (DbfFieldType)buffer[11];
            var address = BitConverter.ToUInt32(buffer, 12);
            var length = buffer[16];
            var decimalCount = buffer[17];
            
            fields.Add(new DbfField(name, type, length, decimalCount, address));
        }
        
        // Consuma il terminatore finale dell'header, se presente
        if (reader.BaseStream.Position < reader.BaseStream.Length && reader.PeekChar() == 0x0D)
        {
            reader.ReadByte();
        }
        return fields.ToArray();
    }
}

/// <summary>
/// Rappresenta un singolo record letto da un file DBF, contenente i valori dei campi parsati.
/// </summary>
public readonly struct DbfRecord : IEnumerable<KeyValuePair<string, object?>>
{
    private readonly IReadOnlyList<DbfField> _fields;
    private readonly object?[] _values;

    internal DbfRecord(IReadOnlyList<DbfField> fields, object?[] values)
    {
        _fields = fields;
        _values = values;
    }

    public object? this[int index] => _values[index];
    public object? this[string fieldName]
    {
        get
        {
            for (var i = 0; i < _fields.Count; i++)
            {
                if (_fields[i].Name.Equals(fieldName, StringComparison.OrdinalIgnoreCase))
                    return _values[i];
            }
            throw new KeyNotFoundException($"Campo '{fieldName}' non trovato.");
        }
    }

    public IEnumerator<KeyValuePair<string, object?>> GetEnumerator()
    {
        for (var i = 0; i < _fields.Count; i++)
        {
            yield return new KeyValuePair<string, object?>(_fields[i].Name, _values[i]);
        }
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}

#endregion

#region Interfaces and Exceptions

public interface IIndexHandle : IDisposable
{
    string Name { get; }
    bool Unique { get; }
    bool Descending { get; }
    int KeyLength { get; }

    /// <summary>
    /// Restituisce l'espressione di chiave originale come stringa,
    /// così come è memorizzata nell'header dell'indice.
    /// </summary>
    /// <returns>La stringa dell'espressione di chiave.</returns>
    string GetKeyExpressionString();
    
    /// <summary>
    /// Cerca una chiave nell'indice, applicando il locking appropriato per il livello `Serializable`.
    /// </summary>
    /// <param name="keyBytes">La chiave da cercare.</param>
    /// <param name="soft">Se true, restituisce la chiave successiva in caso di non corrispondenza.</param>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    /// <returns>Il numero del record corrispondente, o -1 se non trovato.</returns>
    int Seek(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context);

    /// <summary>
    /// Come <see cref="Seek"/>, ma restituisce anche i byte reali della chiave del record trovato,
    /// letti direttamente dalla struttura dell'indice (non ricalcolati rivalutando l'espressione
    /// di chiave). Serve alla validazione SET EXACT/prefix in DbfEngine.Seek senza dipendere da un
    /// evaluator runtime di espressioni chiave (che non sa valutare espressioni arbitrarie tipo
    /// STR(RECNO())). L'implementazione di default ricade su Seek e restituisce KeyBytes=null,
    /// segnalando al chiamante di usare il vecchio percorso di validazione basato sull'evaluator.
    /// </summary>
    (int Recno, byte[]? KeyBytes) SeekWithKeyBytes(ReadOnlySpan<byte> keyBytes, bool soft, TransactionContext? context)
        => (Seek(keyBytes, soft, context), null);

    /// <summary>
    /// Inserisce una nuova coppia chiave/record nell'indice in modo transazionalmente sicuro.
    /// </summary>
    /// <param name="keyBytes">La chiave da inserire.</param>
    /// <param name="recno">Il numero del record associato.</param>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    void Insert(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context);

    /// <summary>
    /// Inserisce più coppie chiave/record sotto un unico lock per indice.
    /// ponytail: default loop Insert — NTX sovrascrive con lock singolo.
    /// </summary>
    void InsertBatch(IReadOnlyList<(ReadOnlyMemory<byte> key, int recno)> entries, TransactionContext? context);
    
    /// <summary>
    /// Rimuove una coppia chiave/record dall'indice in modo transazionalmente sicuro.
    /// </summary>
    /// <param name="keyBytes">La chiave da rimuovere.</param>
    /// <param name="recno">Il numero di record specifico da rimuovere.</param>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    void Delete(ReadOnlySpan<byte> keyBytes, int recno, TransactionContext? context);
    
    /// <summary>
    /// Aggiorna una voce dell'indice in modo transazionalmente sicuro.
    /// </summary>
    /// <param name="oldKey">La vecchia chiave da rimuovere.</param>
    /// <param name="newKey">La nuova chiave da inserire.</param>
    /// <param name="recno">Il numero del record.</param>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    void Update(ReadOnlySpan<byte> oldKey, ReadOnlySpan<byte> newKey, int recno, TransactionContext? context);

    void Rebuild(Func<IEnumerable<(ReadOnlyMemory<byte> key, int recno)>> producer);

    /// <summary>
    /// Ottiene il numero del primo record secondo l'ordine dell'indice.
    /// </summary>
    /// <returns>Il numero del record (1-based), o 0 se l'indice è vuoto.</returns>
    int GetFirstRecordNumber();
    
    /// <summary>
    /// Ottiene il numero dell'ultimo record secondo l'ordine dell'indice.
    /// </summary>
    /// <returns>Il numero del record (1-based), o 0 se l'indice è vuoto.</returns>
    int GetLastRecordNumber();
    
    /// <summary>
    /// Restituisce un'enumerazione di tutti i numeri di record presenti nell'indice,
    /// nell'ordine corretto della chiave.
    /// </summary>
    IEnumerable<int> GetAllRecordNumbers();
    
    /// <summary>
    /// Ottiene la posizione del primo record, applicando il locking per il livello `Serializable`.
    /// </summary>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    /// <returns>Una tupla con la posizione del record.</returns>
    (int recno, uint pageNum, int indexInLeaf) GetFirstRecordPosition(TransactionContext? context);
    
    /// <summary>
    /// Ottiene la posizione dell'ultimo record, applicando il locking per il livello `Serializable`.
    /// </summary>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    /// <returns>Una tupla con la posizione del record.</returns>
    (int recno, uint pageNum, int indexInLeaf) GetLastRecordPosition(TransactionContext? context);
    
    /// <summary>
    /// Trova il record successivo o precedente, applicando il locking per il livello `Serializable`.
    /// </summary>
    /// <param name="currentPageNum">La pagina di partenza.</param>
    /// <param name="currentIndexInLeaf">L'indice di partenza.</param>
    /// <param name="direction">La direzione dello spostamento.</param>
    /// <param name="context">Il contesto della transazione attiva per la gestione dei lock.</param>
    /// <returns>Una tupla con la nuova posizione del record.</returns>
    (int recno, uint newPageNum, int newIndexInLeaf) GetNextRecord(uint currentPageNum, int currentIndexInLeaf, 
        int direction, TransactionContext? context);
    
    /// <summary>
    /// Invalida qualsiasi stato interno o cache mantenuto dall'handle dell'indice.
    /// </summary>
    void InvalidateCache();
}

/// <summary>
/// Marker per indici scrivibili (NTX Clipper, NDX, tag MDX/CDX).
/// </summary>
internal interface IWritableIndexHandle : IIndexHandle;

/// <summary>
/// Interfaccia per un lettore di file memo (es. .DBT, .FPT).
/// </summary>
internal interface IMemoFile : IDisposable
{
    MemoData? GetMemo(int index);
    int WriteMemo(ReadOnlySpan<byte> data, bool isText = true);
}

/// <summary>
/// Classe base astratta per i dati letti da un file memo.
/// </summary>
public abstract class MemoData(ReadOnlyMemory<byte> data)
{
    public ReadOnlyMemory<byte> Data { get; } = data;
}

/// <summary>
/// Rappresenta dati memo di tipo testo.
/// </summary>
public sealed class TextMemo(ReadOnlyMemory<byte> data) : MemoData(data);

/// <summary>
/// Rappresenta dati memo di tipo binario.
/// </summary>
public class BinaryMemo(ReadOnlyMemory<byte> data) : MemoData(data);

/// <summary>
/// Implementazione Null Object per IMemoFile.
/// </summary>
internal sealed class NullMemoFile : IMemoFile
{
    public static readonly NullMemoFile Instance = new();
    private NullMemoFile() { }
    public MemoData? GetMemo(int index) => null;
    public int WriteMemo(ReadOnlySpan<byte> data, bool isText = true) => 0;
    public void Dispose() { }
}

/// <summary>
/// Definisce l'interfaccia per un parser di campi DBF.
/// </summary>
internal interface IFieldParser
{
    object? Parse(DbfField field, ReadOnlySpan<byte> data, IMemoFile? memoFile, Encoding encoding);
}

/// <summary>
/// Rappresenta un valore di campo non valido o illeggibile.
/// </summary>
public sealed class InvalidValue(ReadOnlySpan<byte> rawData, DbfField field)
{
    public ReadOnlyMemory<byte> RawData { get; } = rawData.ToArray();
    public DbfField Field { get; } = field;
}

#endregion

#region CodePage Helper

/// <summary>
/// Fornisce la mappatura tra il language driver byte dell'header DBF e l'encoding .NET.
/// </summary>
internal static class DbfCodePage
{
    private static readonly Dictionary<byte, string> Mappings = new()
    {
        { 0x01, "ibm437" }, { 0x02, "ibm850" }, { 0x03, "windows-1252" }, { 0x04, "macintosh" },
        { 0x08, "ibm865" }, { 0x09, "ibm437" }, { 0x0A, "ibm850" }, { 0x0B, "ibm437" },
        { 0x0D, "ibm437" }, { 0x0E, "ibm850" }, { 0x0F, "ibm437" }, { 0x10, "ibm850" },
        { 0x11, "ibm437" }, { 0x12, "ibm850" }, { 0x13, "sjis" }, { 0x14, "ibm850" },
        { 0x15, "ibm437" }, { 0x16, "ibm850" }, { 0x17, "ibm865" }, { 0x18, "ibm437" },
        { 0x19, "ibm437" }, { 0x1A, "ibm850" }, { 0x1B, "ibm437" }, { 0x1C, "ibm863" },
        { 0x1D, "ibm850" }, { 0x1F, "ibm852" }, { 0x22, "ibm852" }, { 0x23, "ibm852" },
        { 0x24, "ibm860" }, { 0x25, "ibm850" }, { 0x26, "ibm866" }, { 0x37, "ibm850" },
        { 0x40, "ibm852" }, { 0x4D, "gb2312" }, { 0x4E, "euc-kr" }, { 0x4F, "big5" },
        { 0x50, "windows-874" }, { 0x57, "windows-1252" }, { 0x58, "windows-1252" },
        { 0x59, "windows-1252" }, { 0x64, "ibm852" }, { 0x65, "ibm866" }, { 0x66, "ibm865" },
        { 0x67, "ibm861" }, { 0x6A, "ibm737" }, { 0x6B, "ibm857" }, { 0x78, "big5" },
        { 0x79, "euc-kr" }, { 0x7A, "gb2312" }, { 0x7B, "sjis" }, { 0x7C, "windows-874" },
        { 0x7D, "windows-1255" }, { 0x7E, "windows-1256" }, { 0x96, "x-mac-cyrillic" },
        { 0x97, "x-mac-ce" }, { 0x98, "x-mac-greek" }, { 0xC8, "windows-1250" },
        { 0xC9, "windows-1251" }, { 0xCA, "windows-1254" }, { 0xCB, "windows-1253" },
        { 0xF0, "utf-8" },
    };

    public static Encoding GetEncoding(byte languageDriver)
    {
        try
        {
            return Mappings.TryGetValue(languageDriver, out var encodingName)
                ? Encoding.GetEncoding(encodingName)
                : Encoding.ASCII;
        }
        catch (Exception)
        {
            return Encoding.ASCII;
        }
    }
    
    /// <summary>
    /// Esegue la ricerca inversa per trovare il language driver byte associato a una codifica.
    /// </summary>
    /// <param name="encoding">L'encoding da cercare.</param>
    /// <returns>Il byte del language driver, o null se non trovato.</returns>
    public static byte? GetLanguageDriver(Encoding encoding)
    {
        // Cerca la prima corrispondenza per il nome della codifica.
        // Potrebbe non essere perfetto se più driver usano la stessa codifica,
        // ma è un approccio ragionevole.
        var encodingName = encoding.WebName;
        foreach (var mapping in Mappings)
        {
            if (string.Equals(mapping.Value, encodingName, StringComparison.OrdinalIgnoreCase))
            {
                return mapping.Key;
            }
        }
        return null;
    }
}

#endregion

/// <summary>Estensione header Visual FoxPro (bytes 32+). Long field names, auto-increment.</summary>
public sealed class VfpHeaderExtension
{
    public string? Backlink { get; set; }
    public System.Collections.Generic.Dictionary<int, string> LongFieldNames { get; } = new();
    public int AutoIncrementNext { get; set; }
    public int AutoIncrementStep { get; set; } = 1;

    public static VfpHeaderExtension Parse(byte[] headerBytes, int fieldCount)
    {
        var ext = new VfpHeaderExtension();
        if (headerBytes.Length < 264) return ext;
        var backlinkEnd = System.Array.IndexOf(headerBytes, (byte)0x00, 32);
        if (backlinkEnd < 0 || backlinkEnd > 263) backlinkEnd = 263;
        ext.Backlink = System.Text.Encoding.ASCII.GetString(headerBytes.AsSpan(32, backlinkEnd - 32)).TrimEnd('\0', ' ');
        int pos = 264;
        if (headerBytes.Length > 264)
        {
            int numNameBlocks = headerBytes[pos++];
            for (int i = 0; i < numNameBlocks && pos + 130 <= headerBytes.Length; i++)
            {
                int fieldIdx = System.BitConverter.ToInt16(headerBytes, pos); pos += 2;
                var nameEnd = System.Array.IndexOf<byte>(headerBytes, 0, pos, 128);
                if (nameEnd < 0) nameEnd = pos + 128;
                var name = System.Text.Encoding.ASCII.GetString(headerBytes, pos, nameEnd - pos).TrimEnd('\0', ' ');
                if (!string.IsNullOrEmpty(name) && fieldIdx >= 0 && fieldIdx < fieldCount)
                    ext.LongFieldNames[fieldIdx] = name;
                pos += 128;
            }
        }
        if (pos + 8 <= headerBytes.Length)
        {
            ext.AutoIncrementNext = System.BitConverter.ToInt32(headerBytes, pos);
            ext.AutoIncrementStep = System.BitConverter.ToInt32(headerBytes, pos + 4);
        }
        return ext;
    }
}
