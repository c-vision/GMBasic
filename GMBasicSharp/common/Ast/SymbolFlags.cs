// FILE: Ast.SymbolFlags.cs

namespace Basic;

[Flags]
public enum SymbolFlags : ulong
    {
        // =====================================================================
        // --- FLAG ZERO E QUALIFICATORI DI BASE (0-10) ------------------------
        // =====================================================================

        /// <summary>Nessun flag specifico applicato.</summary>
        None = 0,

        /// <summary>Il valore è costante e non può essere modificato dopo l'inizializzazione.</summary>
        IsConst = 1UL << 0,

        /// <summary>La variabile o campo è mutabile (valore di default per le variabili).</summary>
        IsMutable = 1UL << 1,

        /// <summary>Il simbolo è dichiarato come readonly (accessibile solo in lettura dopo l'inizializzazione del costruttore).</summary>
        IsReadOnly = 1UL << 2,

        /// <summary>Il simbolo ha un’espressione di inizializzazione associata.</summary>
        HasInitializer = 1UL << 3,

        /// <summary>Il simbolo rappresenta uno spazio di memoria volatile il cui accesso non può essere ottimizzato dal compilatore.</summary>
        IsVolatile = 1UL << 4,

        /// <summary>Il simbolo è stato deprecato ed emette un warning durante la compilazione.</summary>
        IsDeprecated = 1UL << 5,

        /// <summary>Indica che la deprecazione è implicita e non richiede un messaggio esplicito.</summary>
        IsDeprecatedImplicit = 1UL << 6,

        /// <summary>Il simbolo è il rimpiazzo suggerito per un altro simbolo deprecato.</summary>
        IsDeprecatedReplacement = 1UL << 7,

        /// <summary>La funzione o il blocco è contrassegnato come unsafe (es. accesso a puntatori, FFI).</summary>
        IsUnsafe = 1UL << 8,

        /// <summary>La funzione è pura, ovvero non ha effetti collaterali osservabili e il suo output dipende solo dagli input.</summary>
        IsPure = 1UL << 9,

        /// <summary>La funzione è garantita per non ritornare mai (es. termina il programma o lancia sempre un'eccezione).</summary>
        IsNoReturn = 1UL << 10,

        // =====================================================================
        // --- VISIBILITÀ E ACCESSO (11-14) ------------------------------------
        // =====================================================================

        /// <summary>Il simbolo è visibile ovunque, equivalente a PUBLIC.</summary>
        IsPublic = 1UL << 11,

        /// <summary>Il simbolo è visibile solo nello scope di definizione e nested scope (default per variabili locali, equivalente a PRIVATE).</summary>
        IsPrivate = 1UL << 12,

        /// <summary>Il simbolo è visibile nella classe e nelle classi derivate (equivalente a PROTECTED).</summary>
        IsProtected = 1UL << 13,

        /// <summary>Il simbolo è nascosto nel linking (non esportato, visibilità interna al modulo).</summary>
        VisibilityHidden = 1UL << 14,

        // =====================================================================
        // --- STORAGE E LINKAGE (15-20) ---------------------------------------
        // =====================================================================

        /// <summary>La variabile o funzione ha durata statica (esiste per tutta l’esecuzione del programma).</summary>
        IsStaticStorage = 1UL << 15,

        /// <summary>La variabile è thread-local, ovvero ne esiste una copia per ogni thread.</summary>
        IsThreadLocal = 1UL << 16,

        /// <summary>Il simbolo è definito esternamente (proviene da un'altra libreria nativa, FFI).</summary>
        IsExtern = 1UL << 17,

        /// <summary>La funzione esterna usa la convenzione di chiamata C.</summary>
        IsExternC = 1UL << 18,

        /// <summary>Il simbolo è stato importato da un altro modulo o namespace.</summary>
        IsImported = 1UL << 19,

        /// <summary>Il simbolo è esportato e visibile ad altri moduli.</summary>
        IsExported = 1UL << 20,


        // =====================================================================
        // --- PROCEDURALI E PARALLELISMO (21-31) ------------------------------
        // =====================================================================

        /// <summary>Il simbolo rappresenta una funzione o una subroutine.</summary>
        IsFunction = 1UL << 21,

        /// <summary>La funzione è candidata all’inlining.</summary>
        IsInline = 1UL << 22,

        /// <summary>La funzione è una forte candidata all’inlining basata sull'euristica del compilatore.</summary>
        IsInlineCandidate = 1UL << 23,

        /// <summary>Il metodo è virtuale e può essere sovrascritto nelle classi derivate.</summary>
        IsVirtual = 1UL << 24,

        /// <summary>Il metodo sovrascrive un metodo virtuale di una classe base.</summary>
        IsOverride = 1UL << 25,

        /// <summary>Il metodo è astratto e deve essere implementato da una classe derivata.</summary>
        IsAbstract = 1UL << 26,

        /// <summary>Il metodo o la classe non può essere ulteriormente sovrascritto o ereditato.</summary>
        IsSealed = 1UL << 27,

        /// <summary>Il simbolo è un costruttore (es. SUB NEW).</summary>
        IsConstructor = 1UL << 28,

        /// <summary>Il simbolo è un distruttore (es. SUB DESTRUCTOR).</summary>
        IsDestructor = 1UL << 29,

        /// <summary>Il simbolo è un operatore sovraccaricato.</summary>
        IsOperator = 1UL << 30,

        /// <summary>La funzione è il punto di ingresso del programma (es. SUB Main).</summary>
        IsEntryPoint = 1UL << 31,

        // =====================================================================
        // --- TIPI E MEMBRI DI TIPO (32-43) -----------------------------------
        // =====================================================================

        /// <summary>Il simbolo rappresenta una classe.</summary>
        IsClass = 1UL << 32,

        /// <summary>Il simbolo rappresenta una struttura o UDT (tipo valore).</summary>
        IsStruct = 1UL << 33,

        /// <summary>Il simbolo rappresenta un'enumerazione.</summary>
        IsEnum = 1UL << 34,

        /// <summary>Il simbolo rappresenta un'interfaccia.</summary>
        IsInterface = 1UL << 35,

        /// <summary>Il simbolo è un template o un tipo generico.</summary>
        IsTemplate = 1UL << 36,

        /// <summary>Il simbolo è un'istanza concreta di un template o generico.</summary>
        IsInstantiation = 1UL << 37,

        /// <summary>Il simbolo è un alias per un altro simbolo (es. `IMPORT ns AS alias`).</summary>
        IsAlias = 1UL << 38,

        /// <summary>Il simbolo è un alias per un altro tipo (es. `TYPE a = INTEGER`).</summary>
        IsTypeAlias = 1UL << 39,

        /// <summary>Il simbolo è un membro di un'enumerazione.</summary>
        IsEnumMember = 1UL << 40,

        /// <summary>Il simbolo è un campo (variabile membro) di una classe o struttura.</summary>
        IsField = 1UL << 41,

        /// <summary>Il simbolo è una proprietà (con GET/SET) di una classe o struttura.</summary>
        IsProperty = 1UL << 42,

        /// <summary>Il simbolo è un metodo membro di una classe o struttura.</summary>
        IsMethod = 1UL << 43,

        /// <summary>Il simbolo è un metodo membro statico di una classe o struttura.</summary>
        IsStatic = 1UL << 44,

        // =====================================================================
        // --- SCOPE E CONTESTO (44-53) ----------------------------------------
        // =====================================================================

        /// <summary>Il simbolo rappresenta un namespace.</summary>
        IsNamespace = 1UL << 45,

        /// <summary>Il simbolo rappresenta un modulo (unità di compilazione).</summary>
        IsModule = 1UL << 46,

        /// <summary>La variabile è globale, accessibile in tutto il programma (soggetto alle regole dei namespace).</summary>
        IsGlobal = 1UL << 47,

        /// <summary>La variabile è locale a un blocco o funzione.</summary>
        IsLocal = 1UL << 48,

        /// <summary>Il simbolo è un parametro formale di una funzione.</summary>
        IsParameter = 1UL << 49,

        /// <summary>Il parametro è opzionale e può essere omesso nella chiamata.</summary>
        IsOptional = 1UL << 50,

        /// <summary>
        /// Indica che il parametro è un "parameter array" (ParamArray),
        /// che accetta un numero variabile di argomenti. Deve essere l'ultimo parametro.
        /// </summary>
        IsParamArray = 1UL << 51,

        /// <summary>Il parametro è passato per riferimento (BYREF).</summary>
        IsByRef = 1UL << 52,

        /// <summary>La variabile dichiarata è un array o una collezione.</summary>
        IsArray = 1UL << 53, 

        /// <summary>La variabile è stata catturata da una closure o lambda.</summary>
        IsCaptured = 1UL << 54,

        /// <summary>La variabile catturata è passata per riferimento.</summary>
        IsCapturedByRef = 1UL << 55,

        /// <summary>La variabile catturata è passata per valore.</summary>
        IsCapturedByValue = 1UL << 56,

        /// <summary>La variabile ha scope dinamico (PRIVATE/PUBLIC in stile Clipper) ed è visibile nella catena di chiamate.</summary>
        IsDynamicScope = 1UL << 57,
        
        // =====================================================================
        // --- SINTETICI E INTERNI AL COMPILATORE (55+) ------------------------
        // =====================================================================

        /// <summary>Il simbolo è stato generato sinteticamente dal compilatore e non ha una controparte diretta nel codice sorgente.</summary>
        IsSynthetic = 1UL << 58,

        /// <summary>Il simbolo rappresenta un blocco di assembly inline.</summary>
        IsInlineAsm = 1UL << 59,
        
        /// <summary>
        /// Il simbolo rappresenta un alias per una work area di un database (DBF, ISAM, SPEED).
        /// Viene utilizzato per risolvere comandi come `SELECT MioAlias`.
        /// </summary>
        //IsDatabaseAlias = 1UL << 60,
            
        IsEvent = 1UL << 60,        // Identifica un membro Evento

        IsTypeParam = 1UL << 61,    // Identifica un parametro di tipo generico
}
