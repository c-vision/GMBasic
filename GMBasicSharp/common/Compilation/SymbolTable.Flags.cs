// FILE: Interpreter.SymbolTable.Flags.cs

using System.Text;

namespace Basic;

/// <summary>
/// Gestisce tutti i simboli e gli scope del programma utilizzando un sistema basato su ID e SSA.
/// Questa classe è l'unica fonte di verità per la definizione e la risoluzione dei simboli.
/// </summary>
public sealed partial class SymbolTable
{

    /// <summary>
    /// Identifica la categoria semantica principale di un simbolo.
    /// A differenza di <see cref="SymbolFlags"/>, che rappresenta una combinazione di proprietà
    /// dettagliate, <c>SymbolKind</c> fornisce una classificazione di alto livello,
    /// facile da usare per operazioni di dispatch o diagnostica.
    /// </summary>
    /// <remarks>
    /// Questo enum viene utilizzato principalmente dalla proprietà calcolata <c>Symbol.Kind</c>,
    /// che astrae la logica di controllo dei flag (es. `IsFunction | IsMethod` viene mappato
    /// a `SymbolKind.Function`).
    /// </remarks>
    public enum SymbolKind
    {
        Unknown,
        Variable,
        Constant,
        Parameter,
        Function,
        Type,
        Namespace,
        Module
    }

    /// <summary>
    /// Rappresenta un simbolo unico all'interno del programma: una variabile, funzione, tipo, etc.
    /// È un record per promuovere l'immutabilità; gli aggiornamenti avvengono tramite
    /// la sintassi non distruttiva `with` o i metodi `Add`/`Remove`.
    /// </summary>
    public sealed record Symbol(
        int Id,
        Token OriginalToken,
        SymbolFlags Flags,
        DataType DataType,
        int ScopeId,
        AstNode DeclarationNode)
    {
        /// <summary>Il nome originale del simbolo, come appare nel codice sorgente.</summary>
        public string OriginalName => OriginalToken.Lexeme;

        // --- Proprietà Derivate / Helper Semantici ---

        /// <summary>
        /// Ottiene la categoria semantica principale del simbolo (es. Variabile, Funzione, Tipo).
        /// Questa è una proprietà calcolata che offre una classificazione di alto livello basata sui flag.
        /// </summary>
        private SymbolKind Kind =>
            Flags switch
            {
                _ when Has(SymbolFlags.IsFunction) => SymbolKind.Function,
                _ when Has(SymbolFlags.IsClass | SymbolFlags.IsStruct | SymbolFlags.IsInterface | SymbolFlags.IsEnum) => SymbolKind.Type,
                _ when Has(SymbolFlags.IsParameter) => SymbolKind.Parameter,
                _ when Has(SymbolFlags.IsConst) => SymbolKind.Constant,
                _ when Has(SymbolFlags.IsNamespace) => SymbolKind.Namespace,
                _ when Has(SymbolFlags.IsModule) => SymbolKind.Module,
                _ => SymbolKind.Variable
            };
        
        /// <summary>
        /// Ottiene un valore che indica se il simbolo rappresenta un'entità di storage in memoria
        /// (es. una variabile locale, globale o statica).
        /// </summary>
        public bool IsStorageSymbol => Has(SymbolFlags.IsLocal | SymbolFlags.IsGlobal | SymbolFlags.IsStaticStorage);

        /// <summary>
        /// Ottiene un valore che indica se il simbolo è visibile esternamente al suo modulo di definizione
        /// (es. dichiarato `PUBLIC` o `EXPORTED` e non `HIDDEN`).
        /// </summary>
        public bool IsVisibleExternally => Has(SymbolFlags.IsExported) && !Has(SymbolFlags.VisibilityHidden);

        /// <summary>
        /// Ottiene un valore che indica se il simbolo è stato generato automaticamente dal compilatore
        /// e non ha una corrispondenza diretta nel codice sorgente scritto dall'utente.
        /// </summary>
        public bool IsCompilerGenerated => Has(SymbolFlags.IsSynthetic);

        /// <summary>
        /// Ottiene un valore che indica se il simbolo si comporta come una funzione o una procedura
        /// (es. una funzione, un metodo, un costruttore, un distruttore o un operatore).
        /// </summary>
        public bool IsFunctionLike => Has(SymbolFlags.IsFunction | SymbolFlags.IsMethod | SymbolFlags.IsConstructor |
                                          SymbolFlags.IsDestructor | SymbolFlags.IsOperator);

        /// <summary>
        /// Ottiene un valore che indica se il simbolo rappresenta un valore costante il cui
        /// valore è noto a tempo di compilazione.
        /// </summary>
        public bool IsConstant => Has(SymbolFlags.IsConst | SymbolFlags.IsEnumMember);

        /// <summary>
        /// Ottiene un valore che indica se il simbolo è un alias per un altro simbolo o tipo.
        /// </summary>
        public bool IsAlias => Has(SymbolFlags.IsAlias | SymbolFlags.IsTypeAlias);

        /// <summary>
        /// Ottiene un valore che indica se l'uso di questo simbolo dovrebbe generare un avviso di deprecazione.
        /// </summary>
        public bool IsDeprecated => Has(SymbolFlags.IsDeprecated);

        /// <summary>Ottiene un valore che indica se il simbolo è un parametro opzionale.</summary>
        public bool IsOptional => Has(SymbolFlags.IsOptional);
        
        /// <summary>Ottiene un valore che indica se il simbolo è passato per riferimento (BYREF).</summary>
        public bool IsByRef => Has(SymbolFlags.IsByRef);
        
        /// <summary>Ottiene un valore che indica se il simbolo è un array (o un parametro variadico).</summary>
        public bool IsArray => Has(SymbolFlags.IsArray);
        
        /// <summary>Ottiene un valore che indica se il simbolo ha visibilità pubblica.</summary>
        public bool IsPublic => Has(SymbolFlags.IsPublic);
        
        /// <summary>Ottiene un valore che indica se il simbolo ha visibilità privata.</summary>
        public bool IsPrivate => Has(SymbolFlags.IsPrivate);
        
        /// <summary>Ottiene un valore che indica se il simbolo ha visibilità protetta.</summary>
        public bool IsProtected => Has(SymbolFlags.IsProtected);
        
        // --- Metodi Utili ---

        /// <summary>
        /// Restituisce una rappresentazione testuale completa e dettagliata del simbolo,
        /// utile per il debugging e il dump della SymbolTable.
        /// </summary>
        /// <returns>Una stringa formattata come "ID: Kind OriginalName AS DataType (Scope: ScopeId) [Flags]".</returns>
        public override string ToString()
        {
            var sb = new StringBuilder();
            sb.Append($"{Id,-4}: {Kind,-10} {OriginalName,-20}");
            if (DataType is not VoidType)
                sb.Append($" AS {DataType,-25}");
            sb.Append($" (Scope: {ScopeId,-3}, FileID: {OriginalToken.Position.FileId,-2}) [{Flags}]");
            return sb.ToString();
        }

        /// <summary>
        /// Verifica se il simbolo possiede tutti i flag specificati.
        /// </summary>
        /// <param name="flag">Uno o più flag da controllare, combinati con l'operatore OR bit a bit.</param>
        /// <returns><c>true</c> se tutti i flag specificati sono presenti; altrimenti, <c>false</c>.</returns>
        public bool Has(SymbolFlags flag) => (Flags & flag) == flag;

        /// <summary>
        /// Restituisce una nuova istanza del simbolo con uno o più flag semantici aggiunti.
        /// </summary>
        /// <param name="flagsToAdd">Uno o più flag da aggiungere, combinati con l'operatore OR bit a bit.</param>
        /// <returns>Una nuova istanza di `Symbol` con i flag aggiornati.</returns>
        public Symbol Add(SymbolFlags flagsToAdd) => this with { Flags = Flags | flagsToAdd };

        /// <summary>
        /// Restituisce una nuova istanza del simbolo con uno o più flag semantici rimossi.
        /// </summary>
        /// <param name="flagsToRemove">Uno o più flag da rimuovere, combinati con l'operatore OR bit a bit.</param>
        /// <returns>Una nuova istanza di `Symbol` con i flag aggiornati.</returns>
        public Symbol Remove(SymbolFlags flagsToRemove) => this with { Flags = Flags & ~flagsToRemove };
    }

    /// <summary>
    /// Definisce il tipo di uno scope per una gestione più robusta.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Permettere alla `SymbolTable` e al `Linker` di comprendere il contesto
    /// di uno scope. Questo è fondamentale per la risoluzione dei simboli e la validazione semantica
    /// (es. `STATIC` è permesso solo in uno scope `Procedure`).
    /// </remarks>
    public enum ScopeType
    {
        /// <summary>Lo scope radice del programma.</summary>
        Global,
        /// <summary>Uno scope che definisce un namespace.</summary>
        Namespace,
        /// <summary>Lo scope interno di una `FUNCTION`, `SUB` o `GENERATOR`.</summary>
        Procedure,
        /// <summary>Lo scope interno di una `CLASS`.</summary>
        Class,
        /// <summary>Lo scope interno di un `TYPE` (UDT).</summary>
        Type,
        /// <summary>Uno scope generico per un blocco di codice (es. `IF`, `FOR`, `DO`).</summary>
        Block
    }

    /// <summary>
    /// Rappresenta un singolo scope lessicale (livello di visibilità) nella tabella dei simboli.
    /// In questa architettura, uno scope non contiene i simboli stessi, ma funge da "indice"
    /// che mappa i nomi originali dei simboli ai loro ID univoci.
    /// </summary>
    /// <remarks>
    /// Gli oggetti Scope formano un albero che rappresenta la struttura del codice. La radice è lo scope
    /// `Global`, i suoi figli sono i `Namespace`, i figli dei namespace sono le `Procedure`, e così via.
    /// Questa struttura è il fondamento per la risoluzione dei nomi secondo le regole dello scope lessicale.
    /// </remarks>
    public sealed class Scope(int id, string name, ScopeType type, int parentId, int ownerSymbolId = -1)
    {
        /// <summary>L'ID univoco di questo scope, corrispondente al suo indice nella lista `_scopes`.</summary>
        public int Id { get; } = id;

        /// <summary>L'ID dello scope genitore. Vale -1 solo per lo scope radice (`Global`).</summary>
        public int ParentId { get; } = parentId;

        /// <summary>Il nome descrittivo dello scope (es. "_global", "MyNamespace", "MyFunction").</summary>
        public string Name { get; } = name;

        /// <summary>Il tipo semantico dello scope, definito da <see cref="ScopeType"/>.</summary>
        public ScopeType Type { get; } = type;

        /// <summary>
        /// L'ID del simbolo che definisce e "possiede" questo scope.
        /// Ad esempio, per uno scope di tipo `Procedure`, questo sarà il `SymbolId` della procedura stessa.
        /// Vale -1 se lo scope non è posseduto da un simbolo specifico (es. scope globale o di blocco).
        /// </summary>
        public int OwnerSymbolId { get; } = ownerSymbolId;

        /// <summary>
        /// La mappa dei nomi dei simboli dichiarati ESCLUSIVAMENTE all'interno di questo scope.
        /// La chiave è il nome originale del simbolo (case-insensitive).
        /// Il valore è una lista di `SymbolId`, che conterrà un solo ID per simboli non sovraccaricabili
        /// (variabili, tipi) e molteplici ID per quelli sovraccaricabili (procedure).
        /// </summary>
        public Dictionary<string, List<int>> SymbolsByName { get; } = new(StringComparer.OrdinalIgnoreCase);

        /// <summary>La lista degli ID degli scope figli diretti, usata per navigare l'albero in avanti.</summary>
        public List<int> ChildrenIds { get; } = [];
    }
}