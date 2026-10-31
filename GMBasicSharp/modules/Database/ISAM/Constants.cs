// FILE: Basic.ISAM.Constants.cs
using System.Runtime.InteropServices;
using System.Text;
using Basic.ISAM;

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace
// ReSharper disable CollectionNeverQueried.Global

namespace Basic.ISAM;

/// <summary>
/// Contiene le costanti, le strutture dati del file e le enumerazioni
/// fondamentali per il funzionamento del motore ISAM.
/// Questa classe definisce il layout fisico del file .isdb.
/// </summary>
public static class Config
{
    #region File Constants

    /// <summary>
    /// Identificatore univoco ("Magic Number") per i file ISAM, scritto all'inizio dell'header.
    /// Utilizzato per verificare rapidamente che un file sia un database ISAM valido.
    /// </summary>
    private const string MagicString = "ISAM_V10";

    /// <summary>
    /// La rappresentazione in byte del Magic Number, pronta per essere scritta/letta dal file.
    /// </summary>
    internal static readonly byte[] MagicBytes = Encoding.ASCII.GetBytes(MagicString);

    /// <summary>
    /// Versione corrente del formato file. Vedi <see cref="IsamFormatPolicy"/>.
    /// </summary>
    internal const int FormatVersion = IsamFormatPolicy.CurrentFormatVersion;

    /// <summary>
    /// Dimensione fissa dell'header del file in byte.
    /// 8 KB è uno spazio generoso per metadati e puntatori futuri.
    /// </summary>
    internal const int HeaderSize = 8 * 1024;

    /// <summary>
    /// Dimensione in byte del prefisso campi fissi di <see cref="FileHeader"/> prima del blocco <c>Reserved</c>.
    /// </summary>
    internal const int FileHeaderFieldsPrefixSize = 216;

    /// <summary>
    /// Byte di stato per un record attivo e valido. Corrisponde al carattere ASCII ' ' (spazio).
    /// </summary>
    internal const byte RecordStatusActive = 0x20;

    /// <summary>
    /// Byte di stato per un record marcato come cancellato (soft delete). Corrisponde al carattere ASCII '*'.
    /// </summary>
    internal const byte RecordStatusDeleted = 0x2A;

    /// <summary>
    /// Numero massimo di chiavi che un nodo del B+Tree può contenere prima di dividersi.
    /// </summary>
    internal const int MaxKeysInNode = 128;
        
    #endregion

    #region Enumerations
    
    /// <summary>
    /// Definisce le direzioni per il movimento del cursore all'interno del file ISAM.
    /// Utilizzato dai comandi GO e SKIP.
    /// </summary>
    public enum CursorMoveOperation
    {
        Top,
        Bottom,
        Next,
        Previous,
    }

    #endregion

    #region File Structures

    /// <summary>
    /// Rappresenta la struttura binaria esatta dell'header di un file .isdb.
    /// L'attributo StructLayout garantisce che non ci sia padding e che il layout in memoria
    /// corrisponda esattamente a quello su disco, per letture/scritture dirette.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Ansi)]
    public struct FileHeader
    {
        /// <summary>
        /// 8 byte per il Magic Number.
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public byte[] Magic;

        /// <summary>
        /// 4 byte per la versione del formato.
        /// </summary>
        public int Version;

        /// <summary>
        /// 8 byte - Offset all'interno del file dove inizia il blocco dello schema.
        /// </summary>
        public long SchemaOffset;
        
        /// <summary>
        /// 8 byte - Lunghezza del blocco dello schema.
        /// </summary>
        public long SchemaLength;

        /// <summary>
        /// 8 byte - Offset all'interno del file dove inizia la radice del B+Tree della chiave primaria.
        /// </summary>
        public long PrimaryKeyRootOffset;

        /// <summary>
        /// 8 byte - Offset all'interno del file dove inizia la mappa degli indici secondari.
        /// </summary>
        public long SecondaryIndexMapOffset;

        /// <summary>
        /// 8 byte - Offset del primo record nella lista concatenata dei record cancellati (free list).
        /// -1 se la lista è vuota.
        /// </summary>
        public long FreeListHeadOffset;

        /// <summary>
        /// 4 byte - Dimensione fissa in byte di ogni record dati (incluso il byte di stato).
        /// </summary>
        public int RecordSize;

        /// <summary>
        /// 8 byte - Numero totale di record fisici allocati nel file.
        /// </summary>
        public long LastRecNumber;
        
        /// <summary>
        /// 8 byte - Numero di record attivi (non cancellati).
        /// </summary>
        public long ActiveRecordCount;
        
        /// <summary>
        /// 8 byte - Offset della prossima pagina libera da allocare per i dati o gli indici.
        /// </summary>
        public long NextAvailablePageOffset;

        /// <summary>
        /// Offset di inizio dell'area heap condivisa (LOB + pagine B+).
        /// <see cref="NextAvailablePageOffset"/> è il limite superiore (fine) dell'heap.
        /// </summary>
        public long LobHeapStartOffset; // NUOVO CAMPO

        /// <summary>
        /// 8 byte - Offset della prima pagina B+ nella free list delle pagine indice.
        /// -1 se la lista è vuota. Separata da <see cref="FreeListHeadOffset"/> (record cancellati).
        /// </summary>
        public long PageFreeListHeadOffset;

        /// <summary>
        /// Numero massimo di indici secondari supportati.
        /// </summary>
        private const int MaxSecondaryIndexes = 15;

        /// <summary>
        /// 15 * 8 byte = 120 byte - Array di offset alle radici dei B+Tree degli indici secondari.
        /// Un offset di -1 indica un indice non utilizzato.
        /// </summary>
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = MaxSecondaryIndexes)]
        public long[] SecondaryIndexRootOffsets;

        // Magic (8) + Version (4) + 10×long (80) + RecordSize (4) + SecondaryIndexRootOffsets (120) = 216 byte.
        // Riservato = 8192 - 216 = 7976 byte.
        private const int FileHeaderFieldsPrefixSize = Config.FileHeaderFieldsPrefixSize;

#pragma warning disable CS0414 // Il campo è assegnato ma il suo valore non viene mai usato
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = HeaderSize - FileHeaderFieldsPrefixSize)]
        public byte[] Reserved;
#pragma warning restore CS0414
    }

    #endregion

    #region Schema Structures (Logical)

    /// <summary>
    /// Rappresenta la definizione logica di un singolo campo all'interno dello schema ISAM.
    /// Questa struttura verrà serializzata e salvata nel blocco schema del file.
    /// </summary>
    [Serializable]
    public record IsamFieldSchema(
        string Name,
        DataType DataType,
        int Length // Per le stringhe a lunghezza fissa, altrimenti 0
    );

    /// <summary>
    /// Rappresenta la definizione logica e le proprietà di un singolo indice.
    /// </summary>
    [Serializable]
    public record IndexDefinition(
            
        // Il nome univoco dell'indice, tipicamente corrisponde al nome del campo primario.
        string Name,
            
        // L'espressione di chiave che definisce come viene calcolato il valore dell'indice.
        // Viene memorizzata come stringa per garantire la stabilità del formato del file.
        // Built-in engine-side: campo semplice, UPPER(c), JSON_VALUE(c,'$.p'), COMPOSITE(a,b).
        string KeyExpression,

        // La lunghezza fissa in byte della chiave binaria dell'indice.
        int KeyLength,

        // Se true, l'indice non ammette valori di chiave duplicati.
        bool IsUnique,
        
        // Se true, l'espressione di chiave richiede il valutatore GPM completo (unsafe).
        // Se false (default), viene usato il valutatore interno, veloce e sicuro.
        bool IsUnsafe,

        // Predicato opzionale per indice parziale (es. "Score > 0"). Null = tutti i record.
        string? PartialPredicate = null
    );

    /// <summary>
    /// Rappresenta la definizione logica completa dello schema di un file ISAM.
    /// Contiene la struttura del `TYPE` e la definizione degli indici.
    /// </summary>
    [Serializable]
    public record IsamSchema(
        string TypeName,
        List<IsamFieldSchema> Fields,
        IndexDefinition PrimaryKey,
        List<IndexDefinition> SecondaryKeys
    );

    #endregion
}