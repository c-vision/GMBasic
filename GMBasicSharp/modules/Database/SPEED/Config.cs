// FILE: Basic.Databases.Speed.Config.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

namespace Basic.Databases.Speed;

/// <summary>
/// Contiene le costanti e le enumerazioni per il motore LSM-Tree Speed.
/// </summary>
public static class Config
{
    /// <summary>
    /// Nome del file manifesto che contiene i metadati del database.
    /// </summary>
    public const string ManifestFileName = "manifest.json";

    /// <summary>
    /// Nome della sottodirectory per i file Write-Ahead Log.
    /// </summary>
    public const string WalDirectoryName = "wal";

    /// <summary>
    /// Nome della sottodirectory per i file SSTable.
    /// </summary>
    public const string SstDirectoryName = "tables";

    #region Schema Structures (per il motore Speed)

    /// <summary>
    /// Rappresenta la definizione logica di un singolo campo all'interno dello schema Speed.
    /// </summary>
    [Serializable]
    public record SpeedFieldSchema(
        string Name,
        DataType DataType,
        int Length // Per le stringhe a lunghezza fissa, altrimenti 0
    );

    /// <summary>
    /// Rappresenta la definizione logica e le proprietà di un singolo indice per il motore Speed.
    /// </summary>
    [Serializable]
    public record SpeedIndexDefinition(
        string Name,
        string KeyExpression,
        DataType DataType,
        int KeyLength,
        bool IsUnique,
        bool IsUnsafe
    );

    /// <summary>
    /// Rappresenta la definizione logica completa dello schema di una tabella o vista Speed.
    /// </summary>
    [Serializable]
    public record SpeedSchema(
        string TypeName,
        List<SpeedFieldSchema> Fields,
        SpeedIndexDefinition PrimaryKey,
        List<SpeedIndexDefinition> SecondaryKeys,
        // NUOVO: Contiene la stringa della query SELECT se questa tabella è una vista.
        string? ViewDefinitionQuery = null 
    );

    /// <summary>
    /// Confronto fra schemi per RICONOSCERE UNA TABELLA GIA' ESISTENTE, usato da
    /// <c>Manifest.CreateTable</c>: dice se lo schema richiesto e' lo stesso di quello
    /// memorizzato.
    /// </summary>
    /// <remarks>
    /// <para>
    /// Non si puo' usare l'uguaglianza generata dal record, per due motivi indipendenti:
    /// </para>
    /// <list type="number">
    /// <item><description><c>Fields</c> e <c>SecondaryKeys</c> sono <see cref="List{T}"/> e
    /// <see cref="List{T}"/> non sovrascrive <c>Equals</c>: l'uguaglianza del record confrontava
    /// le liste per IDENTITA', quindi due schemi con lo stesso contenuto erano sempre diversi.</description></item>
    /// <item><description>un <see cref="DataType"/> e' un nodo AST e porta
    /// <c>Position</c> e <c>SourceToken</c>: uno schema costruito dal sorgente BASIC ha
    /// posizioni reali, uno schema riletto dal manifest le ha a zero.</description></item>
    /// </list>
    /// <para>
    /// Il confronto e' per FORMA LOGICA, non letterale: rifiuta solo divergenze vere.
    /// Le tolleranze sono quelle che il motore stesso adotta:
    /// </para>
    /// <list type="bullet">
    /// <item><description><b>Nomi senza caso</b>: tabelle, campi e indici sono confrontati
    /// come fa il motore, che cerca tabelle e indici con <c>OrdinalIgnoreCase</c>. Un
    /// programma che riapre dichiarando <c>articoli</c> invece di <c>ARTICOLI</c> non viene
    /// respinto.</description></item>
    /// <item><description><b>Tipo per natura, non per dimensione</b>: la larghezza e' gia' un
    /// campo (<c>Length</c>), quindi <c>StringType()</c> e <c>StringType(8)</c> con la stessa
    /// <c>Length</c> sono lo stesso campo. Si confronta il tipo CLR, non la sua
    /// formattazione.</description></item>
    /// <item><description><b>Lunghezza della chiave derivata</b>: <c>KeyLength</c> vale 0
    /// quando non e' dichiarata e il motore la deriva dal campo; in quel caso non puo'
    /// contare come divergenza.</description></item>
    /// <item><description><b>Ordine degli indici secondari</b>: due elenchi con gli stessi
    /// indici in ordine diverso descrivono lo stesso schema.</description></item>
    /// <item><description><b>Testo della vista</b> confrontato senza righi e senza caso.</description></item>
    /// </list>
    /// <para>
    /// Restano divergenze vere, e quindi rifiutate: un campo aggiunto o tolto, una
    /// lunghezza diversa, un tipo diverso, l'ordine dei CAMPI (decide gli offset e quindi la
    /// disposizione dei dati), una chiave primaria con un nome o un'univocita' diversi,
    /// <c>IsUnsafe</c> diverso (l'indice non e' mantenuto) e una vista con query diversa.
    /// </para>
    /// </remarks>
    public static bool SchemaEquals(SpeedSchema? a, SpeedSchema? b)
    {
        if (ReferenceEquals(a, b)) return true;
        if (a is null || b is null) return false;
        return NameEquals(a.TypeName, b.TypeName)
            && TextEquals(a.ViewDefinitionQuery, b.ViewDefinitionQuery)
            && IndexEquals(a.PrimaryKey, b.PrimaryKey)
            && FieldsEqual(a.Fields, b.Fields)
            && IndexSetEquals(a.SecondaryKeys, b.SecondaryKeys);
    }

    /// <summary>Nomi senza caso e senza righi: e' come il motore cerca tabelle e indici.</summary>
    private static bool NameEquals(string? a, string? b) =>
        string.Equals(a?.Trim(), b?.Trim(), StringComparison.OrdinalIgnoreCase);

    /// <summary>Testo libero (query di una vista): senza righi e senza caso.</summary>
    private static bool TextEquals(string? a, string? b) =>
        string.Equals(a?.Trim(), b?.Trim(), StringComparison.OrdinalIgnoreCase);

    private static bool FieldsEqual(List<SpeedFieldSchema>? a, List<SpeedFieldSchema>? b)
    {
        if (a is null || b is null) return a is null && b is null;
        // l'ordine dei campi NON e' tollerato: e' lui che fissa gli offset nel record
        if (a.Count != b.Count) return false;
        for (var i = 0; i < a.Count; i++)
        {
            if (!NameEquals(a[i].Name, b[i].Name)) return false;
            if (a[i].Length != b[i].Length) return false;
            if (!TypeEquals(a[i].DataType, b[i].DataType)) return false;
        }
        return true;
    }

    /// <summary>Gli indici secondari sono confrontati come INSIEME: l'ordine non cambia nulla.</summary>
    private static bool IndexSetEquals(List<SpeedIndexDefinition>? a, List<SpeedIndexDefinition>? b)
    {
        if (a is null || b is null) return a is null && b is null;
        if (a.Count != b.Count) return false;
        foreach (var index in a)
            if (!b.Any(other => IndexEquals(index, other)))
                return false;
        return true;
    }

    private static bool IndexEquals(SpeedIndexDefinition? a, SpeedIndexDefinition? b)
    {
        if (a is null || b is null) return a is null && b is null;
        // KeyLength derivata (0) non e' una divergenza: il motore la calcola dal campo
        var keyLengthOk = a.KeyLength == 0 || b.KeyLength == 0 || a.KeyLength == b.KeyLength;
        return keyLengthOk
            && a.IsUnique == b.IsUnique
            && a.IsUnsafe == b.IsUnsafe
            && NameEquals(a.Name, b.Name)
            && NameEquals(a.KeyExpression, b.KeyExpression)
            && TypeEquals(a.DataType, b.DataType);
    }

    /// <summary>
    /// Il tipo per NATURA, non per dimensione: <c>StringType()</c> e <c>StringType(8)</c>
    /// sono lo stesso tipo, e la larghezza e' gia' confrontata come <c>Length</c> del campo.
    /// Un tipo null (lo schema senza tipo dell'area chiave/valore) e' uguale solo a un altro
    /// null: passare da "senza tipo" a "tipizzato" e' una divergenza vera.
    /// </summary>
    private static bool TypeEquals(DataType? a, DataType? b)
    {
        if (a is null || b is null) return a is null && b is null;
        return a.GetType() == b.GetType();
    }

    #endregion
}

/// <summary>
/// Definisce i tipi di operazione che possono essere registrati nel Write-Ahead Log (WAL) del motore Speed.
/// </summary>
internal enum WalOperationType : byte
{
    /// <summary>
    /// Rappresenta un'operazione di inserimento o aggiornamento di una coppia chiave-valore.
    /// </summary>
    Set = 1,

    /// <summary>
    /// Rappresenta un'operazione di cancellazione di una chiave (scrittura di una "tombstone").
    /// </summary>
    Delete = 2,
}