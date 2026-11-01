// FILE: Basic.Databases.Rdd.IDatabaseDriver.cs
// Interfaccia unificata per driver database (pattern RDD stile XSharp/Clipper).
// Ogni motore (ISAM, DBF, SPEED, LEDGER, SQLite, VectorDB) implementa questa interfaccia.
// Il P-CODE VM interagisce con i database SOLO attraverso questa interfaccia.

using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Informazioni su un campo del database (posizione, tipo, offset).</summary>
public sealed class RddFieldInfo
{
    public string Name { get; init; } = "";
    public DataType DataType { get; init; } = new StringType(0);
    public int Length { get; init; }
    public int Offset { get; init; }
    public int SymbolId { get; init; }  // Per compatibilità con il compilatore GMBasic

    public RddFieldInfo(string name, DataType dataType, int length, int offset, int symbolId = 0)
    {
        Name = name;
        DataType = dataType;
        Length = length;
        Offset = offset;
        SymbolId = symbolId;
    }
}

/// <summary>Informazioni per l'apertura di un database.</summary>
public sealed class DbOpenInfo
{
    public string FileName { get; init; } = "";
    public string Alias { get; init; } = "";
    public string Driver { get; init; } = "";  // "ISAM", "DBF", "SPEED", "SQLITE"
    public bool Shared { get; init; }
    public bool ReadOnly { get; init; }
    public int Area { get; set; }  // Assegnato da DataSession dopo l'apertura
}

/// <summary>Informazioni per la creazione di un indice.</summary>
public sealed class DbOrderInfo
{
    public string Name { get; init; } = "";
    public string KeyExpression { get; init; } = "";
    public int KeyLength { get; init; }
    public bool Unique { get; init; }
    public bool Descending { get; init; }
}

/// <summary>Informazioni per un'operazione SEEK.</summary>
public sealed class DbSeekInfo
{
    public object? Key { get; init; }
    public bool SoftSeek { get; init; }
    public bool Last { get; init; }
}

/// <summary>Interfaccia unificata per tutti i driver database GMBasic.
/// Modellata su XSharp IRdd, adattata al type system e al compilatore GMBasic.</summary>
public interface IDatabaseDriver
{
    // ═══════════════════════════════════════════════════════════
    // Apertura / Chiusura
    // ═══════════════════════════════════════════════════════════
    void Open(DbOpenInfo info);
    void Close();

    // ═══════════════════════════════════════════════════════════
    // Navigazione
    // ═══════════════════════════════════════════════════════════
    void GoTop();
    void GoBottom();
    void GoTo(long recordNumber);
    void Skip(int count);
    void SkipRaw(int count);

    // ═══════════════════════════════════════════════════════════
    // Stato cursore
    // ═══════════════════════════════════════════════════════════
    bool Eof { get; }
    bool Bof { get; }
    bool Found { get; }
    long RecNo { get; }
    long RecCount { get; }

    // ═══════════════════════════════════════════════════════════
    // CRUD
    // ═══════════════════════════════════════════════════════════
    /// <summary>Appende un record. Campi: SymbolId → valore. Ritorna il numero record.</summary>
    long Append(Dictionary<int, object?> fields);
    void Delete();
    void Recall();
    void Pack();
    void Zap();

    /// <summary>Legge il record corrente in un dizionario SymbolId → valore.</summary>
    Dictionary<int, object?>? GetCurrentRecord();

    /// <summary>Aggiorna il record corrente. Campi: SymbolId → valore.</summary>
    void PutCurrentRecord(Dictionary<int, object?> fields);

    // ═══════════════════════════════════════════════════════════
    // Schema
    // ═══════════════════════════════════════════════════════════
    RddFieldInfo[] Fields { get; }
    int FieldCount { get; }
    int GetFieldIndex(string name);
    object? GetFieldValue(int index);
    void SetFieldValue(int index, object? value);
    void SetFields(RddFieldInfo[] fields);

    // ═══════════════════════════════════════════════════════════
    // Lock
    // ═══════════════════════════════════════════════════════════
    bool Lock(long recordNumber);
    void Unlock(long recordNumber);
    bool FileLock();
    void FileUnlock();

    // ═══════════════════════════════════════════════════════════
    // Indici (Ordini)
    // ═══════════════════════════════════════════════════════════
    void CreateIndex(DbOrderInfo info);
    void OpenIndex(string name, string filePath);
    void CloseIndex(string name);
    void SetOrder(string name);
    bool Seek(DbSeekInfo info);
    string? CurrentKey();
    int CurrentKeyNo();

    // ═══════════════════════════════════════════════════════════
    // Filtri e Relazioni
    // ═══════════════════════════════════════════════════════════
    void SetFilter(string? expression);
    void ClearFilter();
    void SetDeleted(bool on);
    void SetRelation(IDatabaseDriver parent, string keyExpression);
    /// <summary>Toglie la relazione attiva (SET RELATION TO senza INTO).</summary>
    void ClearRelation();

    /// <summary>Vero se SetRelation è stato chiamato con un parent non nullo — vedi BaseDriver.</summary>
    bool HasActiveRelation { get; }
    /// <summary>Il driver padre impostato dall'ultima SetRelation (null se nessuna relazione attiva) —
    /// serve a chi orchestra la navigazione (PCodeVm) per trovare tutti i figli di UN CERTO padre
    /// quando quel padre si sposta, senza dover riesporre l'espressione chiave stessa.</summary>
    IDatabaseDriver? ParentRelationDriver { get; }

    /// <summary>Espressione chiave dell'ultima SetRelation (null se nessuna relazione attiva).
    /// Serve a DBRELATION(), che in Clipper ritorna l'ESPRESSIONE della relazione (stringa):
    /// la <c>_relationKeyExpression</c> di BaseDriver era assegnata ma mai esposta.</summary>
    string? RelationKeyExpression { get; }
    /// <summary>Riposiziona questo driver (figlio) sul record corrispondente al valore corrente del
    /// campo chiave nel driver padre — vedi BaseDriver/IsamDriver.</summary>
    bool SyncRelation();

    // ═══════════════════════════════════════════════════════════
    // Informazioni e Diagnostica
    // ═══════════════════════════════════════════════════════════
    string? Alias { get; }
    string FileName { get; }
    bool IsOpen { get; }
    string DriverName { get; }
    bool IsReadOnly { get; }
    object? Info(int infoType, object? param);

    // ═══════════════════════════════════════════════════════════
    // SQL Raw (solo driver SQL; default → NotSupportedException)
    // ═══════════════════════════════════════════════════════════

    /// <summary>Esegue DML/DDL (INSERT, UPDATE, DELETE, CREATE, ...). Ritorna righe affette.</summary>
    long ExecRaw(string sql, IReadOnlyList<object?>? positional, IReadOnlyDictionary<string, object?>? named);

    /// <summary>Esegue una SELECT e restituisce un result-set ID per iterazione cursore.</summary>
    int QueryRaw(string sql, IReadOnlyList<object?>? positional, IReadOnlyDictionary<string, object?>? named);

    /// <summary>Avanza al prossimo record del result-set. Ritorna false a EOF.</summary>
    bool Fetch(int resultSetId);

    /// <summary>Record corrente del result-set come dizionario colonna→valore.</summary>
    IReadOnlyDictionary<string, object?>? GetCurrentRow(int resultSetId);

    /// <summary>Rilascia le risorse del result-set.</summary>
    void CloseResultSet(int resultSetId);

    /// <summary>ATTACH DATABASE per SQLite.</summary>
    void Attach(string path, string? schema);

    /// <summary>DETACH DATABASE per SQLite.</summary>
    void Detach(string? schema);

    /// <summary>BEGIN TRANSACTION.</summary>
    void BeginTransaction();

    /// <summary>COMMIT.</summary>
    void Commit();

    /// <summary>ROLLBACK.</summary>
    void Rollback();

    /// <summary>Ultimo rowid inserito (SQLite).</summary>
    long LastInsertRowId();

    /// <summary>Righe modificate dall'ultima operazione (SQLite).</summary>
    long Changes();
}
