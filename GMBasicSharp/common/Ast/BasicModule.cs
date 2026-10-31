// FILE: Interpreter.BasicModule.cs

namespace Basic;

/// <summary>
/// Rappresenta un singolo modulo BASIC compilato (un file sorgente).
/// È un contenitore leggero e immutabile che aggrega tutte le informazioni
/// statiche e specifiche di un file, come l'AST e le mappe per il debug,
/// pronte per essere utilizzate dalla VM durante l'esecuzione.
/// Questa classe non contiene più simboli, che sono ora gestiti centralmente da BasicProgram.
/// </summary>
public sealed class BasicModule
{
    /// <summary>
    /// Ottiene il percorso completo del file sorgente originale di questo modulo.
    /// </summary>
    public string FilePath { get; }

    /// <summary>
    /// Ottiene un valore che indica se per questo modulo è attiva la modalità `OPTION STRICT ON`.
    /// </summary>
    public bool IsStrict { get; }
    
    /// <summary>
    /// Ottiene la mappa sorgente che associa ogni indice di riga interno al suo contenuto e origine.
    /// È fondamentale per il debugging e per la formattazione dei messaggi di errore.
    /// </summary>
    public ImmutableArray<SourceLine> SourceMap { get; }

    /// <summary>
    /// Ottiene il dizionario delle istruzioni a livello di modulo, indicizzate per numero di riga.
    /// Rappresenta il codice eseguibile che non si trova all'interno di una procedura.
    /// </summary>
    public Dictionary<int, List<Stmt>> Statements { get; set; }

    /// <summary>
    /// Ordine di esecuzione delle righe (numeri di chiave in <see cref="Statements"/>).
    /// Include sia i numeri di riga espliciti che quelli sintetici (righe non numerate).
    /// L'esecuzione procede sequenzialmente seguendo quest'ordine, non l'ordinamento
    /// per chiave del dizionario. Questo garantisce che le righe non numerate vengano
    /// eseguite nella posizione corretta rispetto a quelle numerate.
    /// </summary>
    public ImmutableArray<int> ExecutionOrder { get; set; }

    /// <summary>
    /// Ottiene l'elenco di tutte le dichiarazioni di alto livello (FUNCTION, SUB, TYPE, CLASS, DIM, CONST)
    /// trovate in questo modulo durante la Fase 1 del parsing.
    /// </summary>
    public ImmutableArray<AstNode> Declarations { get; }
    
    /// <summary>
    /// Ottiene la lista di tutte le direttive `IMPORT` trovate in questo modulo.
    /// </summary>
    public ImmutableArray<ImportStmt> Imports { get; }

    /// <summary>
    /// Nome della libreria condivisa prodotta da questo modulo (`LIBRARY "nome"`), oppure
    /// <c>null</c> se il modulo è un normale eseguibile. In modalità libreria il backend C
    /// NON emette `main` e esporta i simboli PUBLIC con ABI stabile (`.gmblib`).
    /// </summary>
    public string? LibraryName { get; }

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="BasicModule"/>.
    /// Questo costruttore viene chiamato dal parser al termine dell'analisi di un file sorgente.
    /// </summary>
    public BasicModule(
        string filePath,
        bool isStrict,
        ImmutableArray<SourceLine> sourceMap,
        Dictionary<int, List<Stmt>> statements,
        IEnumerable<ImportStmt> imports,
        IEnumerable<AstNode> declarations,
        string? libraryName = null)
    {
        FilePath = filePath;
        IsStrict = isStrict;
        SourceMap = sourceMap;
        Statements = statements;
        Declarations = [..declarations];
        Imports = [..imports];
        LibraryName = libraryName;
        ExecutionOrder = [..statements.Keys.OrderBy(k => k)];
    }
}