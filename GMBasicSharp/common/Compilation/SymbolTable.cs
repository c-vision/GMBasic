// FILE: Interpreter.SymbolTable.cs

namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
      GUIDA ARCHITETTURALE ALLA SYMBOL TABLE ID-BASED (Rev. 2)
------------------------------------------------------------------------------------------------------------------------
Questa classe implementa il cuore del nostro compilatore: la gestione dei simboli e degli scope.
L'architettura è basata su principi moderni di progettazione dei compilatori per massimizzare le
prestazioni, la robustezza e la chiarezza dell'analisi semantica.

========================================================================================================================
                                     1. FILOSOFIA ARCHITETTURALE
========================================================================================================================
L'obiettivo è duplice:
1.  PERFORMANCE MASSIMA: Sostituire le costose operazioni basate su stringhe (hashing, confronti) con
   accessi diretti a liste tramite indici interi. Un `SymbolId` è un indice in un array, l'operazione
   di accesso più veloce possibile (O(1)), fondamentale per un compilatore ad alte prestazioni.
2.  ANALISI SEMANTICA ROBUSTA: Creare una rappresentazione intermedia (l'AST annotato con ID) che sia
   completamente risolta, non ambigua e facile da analizzare per le fasi successive (linking,
   ottimizzazione, esecuzione). Questo disaccoppia il "cosa" (la semantica di un simbolo) dal "come"
   (il suo nome originale).

========================================================================================================================
                                     2. IL MODELLO A DATABASE RELAZIONALE
========================================================================================================================
La SymbolTable si comporta come un mini database relazionale in memoria, con tabelle primarie e indici per
accelerare le query.

---
2.1. LE TABELLE MASTER (Le Fonti di Verità)
---
L'intera struttura si basa su due liste master "piatte":
- `_symbols` (List<Symbol>): La tabella di tutti i simboli unici. L'indice di un simbolo è il suo `SymbolId`.
- `_scopes` (List<Scope>): La tabella di tutti gli scope lessicali. L'indice di uno scope è il suo `ScopeId`.

---
2.2. LE RELAZIONI (La Struttura Gerarchica)
---
- Albero degli Scope: Gli scope sono organizzati in un albero. Ogni `Scope` ha un `ParentId` che si riferisce
 all'indice di un altro scope nella lista `_scopes`. La risoluzione dei nomi risale questa gerarchia di interi.
- Appartenenza dei Simboli: Ogni `Symbol` ha uno `ScopeId` che indica lo scope in cui è stato dichiarato.
 Ogni `Scope` ha una mappa `SymbolsByName` che collega i nomi (`string`) a una lista di `SymbolId`,
 gestendo l'overloading (più ID per lo stesso nome).

---
2.3. GLI INDICI OTTIMIZZATI (Le Viste Materializzate)
---
Per evitare scansioni lente sulle tabelle master, la `SymbolTable` mantiene diversi indici pre-calcolati.
Questi vengono popolati durante la registrazione (`RegisterSymbol`) e forniscono risposte O(1) a query comuni:
- `_symbolIdToNamespaceName`: Risponde a "In quale namespace si trova il simbolo X?".
- `_globalSymbolIdsByNamespace`: Risponde a "Quali sono tutti i simboli del namespace Y?".
- `_symbolsByFileId`: Risponde a "Quali simboli sono definiti nel file Z?".
- `_globalSymbolIds`, `_staticSymbolIds`, `_procedureSymbolIds`, etc.: Rispondono a "Dammi tutti i simboli di
 una certa categoria (globali, statici, procedure, etc.)".

========================================================================================================================
                                     3. FLUSSO DI LAVORO (I RUOLI DEL COMPILATORE)
========================================================================================================================
Il processo è diviso in passate, ognuna con un compito specifico.

---
3.1. PRE-SCAN (Passata 1): "IL REGISTRATORE DI FIRME"
---
- Il `PreScan` esegue una scansione veloce sui token.
- Il suo unico scopo è trovare le dichiarazioni di alto livello (`FUNCTION`, `CLASS`, `TYPE`, `DIM` globale).
- Per ognuna, chiama `RegisterSymbol` creando un `Symbol` con un nodo AST *placeholder*. Questo popola la
 `SymbolTable` con tutti i nomi e i loro ID prima che inizi il vero parsing.

---
3.2. PARSER (Passata 2): "IL TRADUTTORE E COMPLETATORE"
---
Il Parser ora lavora con una `SymbolTable` già parzialmente popolata.
1.  RISOLUZIONE: Quando incontra un nome (`X`), chiama `FindSymbolInScope`. Se trova un `SymbolId`
   pre-registrato, lo usa per annotare l'AST (es. `VariableExpr(123)`).
2.  REGISTRAZIONE LOCALE: Se `FindSymbolInScope` fallisce, significa che è un simbolo locale (es. `DIM` in una `SUB`).
   Chiama `RegisterSymbol` per creare un nuovo simbolo locale.
3.  COMPLETAMENTO: Quando parsa il corpo di una funzione o classe pre-registrata, costruisce il nodo AST completo
   e chiama `UpdateSymbolDeclaration` per sostituire il placeholder nella `SymbolTable`.

Alla fine del parsing, l'AST non contiene più nomi ambigui, ma solo riferimenti a ID univoci.

---
3.3. IL LINKER (Passata 3): "L'INVESTIGATORE SEMANTICO"
---
Il Linker riceve l'AST annotato e svolge il lavoro "intelligente":
1.  Risoluzione degli Overload: Quando incontra una chiamata di funzione creata dal Parser (es. `UnresolvedCallExpr`),
   ottiene la lista di candidati.
2.  Selezione del Candidato: Filtra i candidati usando le regole di visibilità (interrogando la `SymbolTable` per
   i namespace) e la compatibilità della firma. Sceglie un solo e unico `SymbolId` corretto.
3.  Riscrittura dell'AST: Sostituisce il nodo `UnresolvedCallExpr` con un `FunctionExpr` (o `BuiltinFunctionCallExpr`)
   finale, inserendo il `SymbolId` risolto.
4.  Inferenza dei Tipi: Per variabili come `DIM A = "test"`, il `Parser` potrebbe aver assegnato un tipo di default.
   Il `Linker` analizza il valore, inferisce `StringType` e chiama `UpdateSymbolType` per correggere la `SymbolTable`.

---
3.4. L'EXECUTOR: "LA MACCHINA STUPIDA"
---
L'Executor riceve un'immagine del programma semanticamente valida e completamente risolta.
- Quando incontra un `VariableExpr(SymbolId: 123)`, chiede alla `SymbolTable`: `GetSymbol(123)` per ottenere i
 metadati (es. tipo, flag) e poi accede alla memoria di runtime (`VariableScope`) usando l'ID `123` come chiave.
- NON ESEGUE MAI LOOKUP BASATI SU STRINGHE. Lavora solo con ID, garantendo la massima velocità.

------------------------------------------------------------------------------------------------------------------------
*/
    /// <summary>
    /// Gestisce tutti i simboli e gli scope del programma utilizzando un sistema basato su ID e SSA.
    /// Questa classe è l'unica fonte di verità per la definizione e la risoluzione dei simboli.
    /// </summary>
    public sealed partial class SymbolTable
    {
        /// <summary>Nomi dei namespace built-in da escludere dalle enumerazioni user-code.</summary>
        public static HashSet<string> BuiltinNamespaceNames { get; set; } = [];
    /// <summary>
    /// Rappresenta l'ID di un simbolo che non è ancora stato risolto dal Binder o dal Linker.
    /// Questo è lo stato iniziale di tutti i riferimenti a simboli creati dal Parser.
    /// </summary>
    public const int UnresolvedSymbolId = -1;

    /// <summary>
    /// ID speciale per il simbolo fittizio `_dbRecord`. Indica all'Executor di accedere
    /// al buffer del record di database attivo per il late-binding in stile Clipper.
    /// </summary>
    public const int DbfRecordBufferSymbolId = -2;

    /// <summary>
    /// ID speciale che indica all'Executor che una variabile non è stata trovata nello
    /// scope lessicale e che deve essere cercata nello scope dinamico (lo stack delle variabili DYNAMIC).
    /// </summary>
    public const int DynamicScopeSymbolId = -3;

    /// <summary>
    /// ID speciale per un nome che ESISTE nel programma ma è stato dichiarato in un BLOCCO
    /// ormai chiuso: non è in scope, quindi vale il **default del tipo** (`0`, `""`) come una
    /// variabile implicita (specifica Variables-Scope §2.1/§6; con `OPTION STRICT ON` il linker
    /// produce un errore prima di arrivare qui). Distinto da <see cref="DynamicScopeSymbolId"/>
    /// (nome mai dichiarato → scope dinamico) e da <see cref="UnresolvedSymbolId"/> (late-binding
    /// DBF): i backend devono trattarlo come "leggi il default", MAI cercarlo per nome fra i
    /// locali (era il bug: `DIM x` in un blocco, `PRINT x` dopo stampava il valore del blocco su IL).
    /// </summary>
    public const int OutOfScopeSymbolId = -4;

    /// <summary>
    /// L'elenco master di tutti i simboli unici nel programma.
    /// L'indice di un simbolo in questa lista corrisponde al suo <see cref="Symbol.Id"/>.
    /// Questa è la fonte di verità assoluta per i metadati di ogni simbolo (nome, tipo, flag, scope).
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Centralizzare tutte le definizioni in un unico "database" per
    /// garantire l'unicità e fornire un accesso in tempo costante (O(1)) ai metadati di un simbolo
    /// una volta che il suo ID è noto. Questo elimina completamente la necessità di ricerche basate
    /// su stringhe durante le fasi finali della compilazione e durante l'esecuzione.
    /// 
    /// Fonte di Verità: Questa lista è il cuore della `SymbolTable`. Viene popolata in modo
    /// append-only dal metodo `RegisterSymbol`. Un `Symbol.Id` non è altro che l'indice
    /// di un oggetto `Symbol` in questa `List`.
    ///
    /// Utilizzo Chiave (Dall'AST alla Semantica):
    /// Il flusso di lavoro del compilatore è:
    /// 1. Il `Parser` incontra una dichiarazione (es. `DIM A`), chiama `RegisterSymbol` e ottiene un nuovo ID (es. 123).
    /// 2. Il `Parser` annota il nodo AST (`VariableExpr`) con l'ID `123`.
    /// 3. Successivamente, il `Linker` o l'`Executor` incontrano il nodo `VariableExpr(123)`.
    /// 4. Per ottenere qualsiasi informazione su quella variabile (il suo tipo, se è globale, etc.),
    ///    eseguono una singola e velocissima operazione di lookup: `program.Symbols.GetSymbol(123)`,
    ///    che restituisce l'oggetto `Symbol` da questa lista.
    /// </remarks>
    private readonly List<Symbol> _symbols = [];

    /// <summary>
    /// L'elenco master di tutti gli scope lessicali (global, namespace, procedure, blocchi) nel programma.
    /// L'indice di uno scope in questa lista corrisponde al suo <see cref="Scope.Id"/>.
    /// Insieme a `_symbols`, forma il database relazionale del compilatore.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Rappresentare la struttura gerarchica del codice sorgente. Ogni scope
    /// definisce un "contesto" o un "livello di visibilità". La gerarchia è implementata tramite
    /// la proprietà `ParentId` in ogni oggetto `Scope`, che punta all'indice di un altro scope
    /// in questa stessa lista, formando un albero.
    /// 
    /// Fonte di Verità: Questa lista è la fonte di verità per la struttura degli scope del programma.
    /// Viene popolata esclusivamente dai metodi `EnterScope` e `ExitScope` chiamati dal `Parser`
    /// durante l'analisi sintattica.
    ///
    /// Utilizzo Chiave (Risoluzione dei Simboli):
    /// Quando il compilatore deve trovare un simbolo (es. una variabile `X`), parte dallo scope
    /// corrente e risale l'albero:
    /// 1. Cerca "X" nella mappa `SymbolsByName` dello scope corrente.
    /// 2. Se non lo trova, usa il `ParentId` dello scope corrente per trovare lo scope genitore in questa lista
    ///    (es. `_scopes[currentScope.ParentId]`).
    /// 3. Ripete il processo fino a trovare il simbolo o a raggiungere lo scope radice (con `ParentId = -1`).
    /// </remarks>
    private readonly List<Scope> _scopes = [];

    /// <summary>
    /// L'ID dello scope attualmente attivo. Qualsiasi nuovo simbolo o scope verrà creato
    /// come figlio di questo scope. Viene gestito dai metodi EnterScope/ExitScope.
    /// </summary>
    private int _currentScopeId;

    /// <summary>
    /// Definisce il nome canonico dello scope globale (radice).
    /// </summary>
    public const string GlobalScopeName = "_global";

    /// <summary>
    /// Mappa piatta e globale per le etichette, che indicizza una chiave qualificata
    /// e univoca alla sua posizione fisica (file, riga, procedura) nell'AST.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Gestire la risoluzione dei salti (`GOTO`, `GOSUB`) in modo efficiente.
    /// Le etichette in BASIC seguono regole di scope diverse dai simboli (possono essere locali
    /// a una procedura o a livello di modulo). Per evitare ricerche complesse nell'albero
    /// degli scope, tutte le etichette vengono "appiattite" in un unico dizionario.
    /// L'unicità è garantita da una chiave qualificata generata da `GetQualifiedLabelName`.
    /// 
    /// Fonte di Verità: Questa mappa è la fonte di verità per la posizione di tutte le etichette
    /// dopo che il `Parser` (e le eventuali passate di ottimizzazione) ha finito di costruire l'AST.
    /// Viene popolata e aggiornata dai metodi `AddOrUpdateLabel`.
    ///
    /// Utilizzo Chiave (Linker/Executor):
    /// Quando deve validare o eseguire un `GOTO :miaEtichetta`, il compilatore/interprete:
    /// 1. Costruisce la chiave qualificata per `:miaEtichetta` in base al contesto corrente
    ///    (file e procedura).
    /// 2. Cerca la chiave in questa mappa per ottenere la `LabelLocation` di destinazione in O(1).
    /// 3. Usa la `LabelLocation` per saltare direttamente all'istruzione corretta nell'AST.
    /// </remarks>
    private readonly Dictionary<string, LabelLocation> _labels = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Indice secondario che mappa un `FileId` a una lista di tutti i `SymbolId` definiti in quel file.
    /// Questa struttura è fondamentale per tutte le operazioni a livello di modulo, come recuperare
    /// tutte le procedure di un file o trovare il file di origine di un simbolo.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Creare un ponte efficiente tra la rappresentazione fisica del
    /// progetto (i file sorgente) e la rappresentazione logica (i simboli). Permette di rispondere
    /// in tempo costante (O(1)) alla domanda: "Quali simboli sono stati dichiarati in `main.bas`?".
    /// Viene popolato in `RegisterSymbol` usando il `FileId` associato alla posizione del token
    /// originale di ogni simbolo.
    /// 
    /// Fonte di Verità: È una vista materializzata. La vera associazione file-simbolo è
    /// implicitamente contenuta nella `SourcePosition` di ogni `Symbol.OriginalToken`, ma questo
    /// indice rende la ricerca inversa estremamente veloce.
    ///
    /// Utilizzo Chiave (Esempio nell'Orchestrator/Linker):
    /// 1.  Analisi Modulo-specifica: Quando il Linker deve processare un `BasicModule`, può ottenere
    ///     immediatamente tutte le procedure e le variabili globali definite in quel modulo senza
    ///     scansionare l'intera `SymbolTable`:
    ///     `var moduleSymbols = program.Symbols.GetSymbolsByFileId(fileId);`
    /// 2.  Debugging e Messaggi di Errore: Per trovare dove un simbolo è stato definito, si può
    ///     recuperare il simbolo tramite il suo ID, ottenere il `FileId` dal suo token e usare
    ///     la mappa `FileIdToPathMap` di `BasicProgram` per ottenere il nome del file.
    /// </remarks>
    private readonly Dictionary<int, List<int>> _symbolsByFileId = [];

    /// <summary>
    /// Indice secondario per un recupero O(1) di tutti i simboli globali.
    /// Contiene la lista dei SymbolId dei simboli con flag IsGlobal.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Ottimizzare drasticamente la fase di avvio dell'Executor.
    /// Invece di dover scansionare l'intera SymbolTable per trovare le variabili globali,
    /// l'Executor ottiene questa lista pre-filtrata in tempo costante per allocare la memoria iniziale.
    /// 
    /// Fonte di Verità: Questo indice viene popolato in `RegisterSymbol` ogni volta che un
    /// simbolo viene registrato con il flag `SymbolFlags.IsGlobal`. È una vista materializzata
    /// che accelera una query comune.
    ///
    /// Utilizzo Chiave (Executor):
    /// 1. Chiama `program.Symbols.GetGlobalSymbolIds()` per ottenere questa lista.
    /// 2. Itera sulla lista di ID per allocare le `VariableInfo` nello scope globale.
    /// Questo processo è significativamente più veloce rispetto a un'iterazione completa della SymbolTable.
    /// </remarks>
    private readonly List<int> _globalSymbolIds = [];

    /// <summary>
    /// Indice secondario per un recupero O(1) di tutti i simboli statici.
    /// Contiene la lista dei SymbolId dei simboli con flag IsStaticStorage.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Ottimizzare l'inizializzazione degli scope statici da parte dell'Executor.
    /// Le variabili `STATIC` all'interno delle procedure mantengono il loro valore tra le chiamate.
    /// L'Executor usa questa lista pre-filtrata per creare e inizializzare gli scope statici
    /// per ogni procedura all'avvio del programma, senza dover scansionare l'intera SymbolTable.
    /// 
    /// Fonte di Verità: Questo indice viene popolato in `RegisterSymbol` ogni volta che un
    /// simbolo viene registrato con il flag `SymbolFlags.IsStaticStorage`. È una vista materializzata.
    ///
    /// Utilizzo Chiave (Executor):
    /// 1. Chiama `program.Symbols.GetStaticSymbolIds()` per ottenere questa lista.
    /// 2. Itera sulla lista di ID e raggruppa i simboli per procedura contenitrice.
    /// 3. Per ogni procedura con variabili statiche, crea un `VariableScope` dedicato in `BasicRuntime`.
    /// </remarks>
    private readonly List<int> _staticSymbolIds = [];

    /// <summary>
    /// Indice di lookup primario per l'accesso basato su namespace. Mappa il nome di un namespace
    /// a una lista di tutti i `SymbolId` dei simboli definiti direttamente in quello scope.
    /// Questa struttura è il cuore della risoluzione dei nomi qualificati (es. `IO.KILL`).
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un punto di accesso O(1) a tutti i simboli di un dato
    /// namespace, senza dover scansionare l'intera `SymbolTable` o navigare l'albero degli scope.
    /// Viene popolato in `RegisterSymbol` solo per i simboli di alto livello (quelli il cui
    /// scope genitore è lo scope radice).
    ///
    /// Fonte di Verità: Come gli altri indici, è una vista ottimizzata dei dati presenti
    /// nelle liste `_symbols` e `_scopes`. Non contiene definizioni, ma solo riferimenti (ID).
    ///
    /// Utilizzo Chiave (Esempio nel Linker/Executor):
    /// Quando il compilatore deve risolvere `Math.Sin`, la logica è:
    /// 1.  Ottenere la lista di ID per il namespace "Math":
    ///     `var mathSymbolIds = program.Symbols.GetSymbolIdsByNamespace("Math");` (che usa questo dizionario).
    /// 2.  Iterare su questa lista (tipicamente corta) di `SymbolId` per trovare quello il cui
    ///     nome è "Sin" e la cui firma corrisponde agli argomenti.
    /// Questo approccio è esponenzialmente più veloce che scansionare tutti i simboli del programma.
    /// </remarks>
    private readonly Dictionary<string, List<int>> _globalSymbolIdsByNamespace = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Indice di lookup inverso che mappa un SymbolId al nome del suo namespace contenitore.
    /// Questa è una struttura dati di performance critica, popolata durante la registrazione
    /// di ogni simbolo (`RegisterSymbol`), che denormalizza la relazione gerarchica degli scope.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Centralizzare e ottimizzare la risoluzione del namespace di un simbolo.
    /// Invece di dover risalire l'albero degli scope a runtime o durante il linking (`FindParentScopeOfType`),
    /// operazione potenzialmente lenta (O(profondità_scope)), questo dizionario fornisce una risposta
    /// in tempo costante (O(1)).
    /// 
    /// Fonte di Verità: La `SymbolTable` rimane l'unica fonte di verità. Questo indice è una
    /// vista materializzata e ottimizzata delle relazioni già presenti nella gerarchia degli scope,
    /// non una fonte di dati indipendente.
    /// 
    /// Utilizzo Chiave (Esempio nel Linker):
    /// Durante la risoluzione di una chiamata a funzione non qualificata (es. `MIA_FUNZIONE()`), il Linker
    /// ottiene una lista di simboli candidati. Per ogni candidato, deve verificare se il suo namespace
    /// è "visibile" (es. importato o globale). Invece di navigare l'albero degli scope per ogni
    /// candidato, il Linker può semplicemente fare:
    /// `var namespaceName = program.Symbols.GetNamespaceNameForSymbol(candidate.SymbolId);`
    /// Questa operazione O(1) è fondamentale per un processo di linking ad alte prestazioni.
    /// </remarks>
    private readonly Dictionary<int, string> _symbolIdToNamespaceName = new();

    /// <summary>
    /// Indice per un recupero ottimizzato di tutti i simboli di tipo Procedura.
    /// Contiene la lista dei <c>SymbolId</c> di tutti i simboli il cui <c>DeclarationNode</c>
    /// è un <see cref="ProcedureDefinition"/>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Evitare scansioni complete della `SymbolTable` quando un componente
    /// necessita di operare su tutte le procedure del programma (es. funzioni, sub, generatori).
    /// Questo indice fornisce un accesso diretto e pre-filtrato a questi simboli.
    /// 
    /// Fonte di Verità: Viene popolato in `RegisterSymbol` ogni volta che viene registrato
    /// un nodo di tipo `ProcedureDefinition`. È una vista materializzata per l'ottimizzazione
    /// delle query.
    ///
    /// Utilizzo Chiave (Linker / BasicProgram):
    /// Il metodo pubblico `GetAllProcedures()` utilizza questo indice per restituire
    /// la lista di tutte le procedure in modo molto efficiente, iterando solo sugli ID
    /// pertinenti invece che sull'intera collezione `_symbols`.
    /// </remarks>
    private readonly List<int> _procedureSymbolIds = [];

    /// <summary>
    /// Indice per un recupero ottimizzato di tutti i simboli di tipo Classe.
    /// Contiene la lista dei <c>SymbolId</c> di tutti i simboli il cui <c>DeclarationNode</c>
    /// è una <see cref="ClassDefinition"/>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un accesso rapido a tutte le definizioni di classe
    /// senza la necessità di scansionare l'intera `SymbolTable`. Questo è cruciale per
    /// operazioni a livello di programma come la risoluzione dell'ereditarietà.
    /// 
    /// Fonte di Verità: Viene popolato in `RegisterSymbol` quando viene registrato
    /// un nodo di tipo `ClassDefinition`. È una vista materializzata per ottimizzare
    /// una query comune.
    ///
    /// Utilizzo Chiave (Linker):
    /// Il metodo pubblico `GetAllClasses()` usa questo indice per fornire al `Linker`
    /// una lista completa di tutte le classi del progetto. Il `Linker` poi utilizza
    /// questa lista per costruire il grafo di ereditarietà, validare gli `override` e
    /// risolvere i tipi delle classi base.
    /// </remarks>
    private readonly List<int> _classSymbolIds = [];

    /// <summary>
    /// Indice per un recupero ottimizzato di tutti i simboli di tipo UDT (User-Defined Type).
    /// Contiene la lista dei <c>SymbolId</c> di tutti i simboli il cui <c>DeclarationNode</c>
    /// è un <see cref="UserDefinedType"/>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Accelerare l'accesso a tutte le definizioni <c>TYPE...END TYPE</c>
    /// del programma, evitando scansioni complete della `SymbolTable`.
    /// 
    /// Fonte di Verità: Viene popolato in `RegisterSymbol` ogni volta che viene registrato
    /// un nodo di tipo `UserDefinedType`. È una vista materializzata per ottimizzare
    /// una query comune.
    ///
    /// Utilizzo Chiave (Linker / Executor):
    /// Il metodo pubblico `GetAllUserDefinedTypes()` usa questo indice per fornire al `Linker`
    /// un elenco di tutti i tipi personalizzati per la validazione e la risoluzione dei tipi
    /// in espressioni e dichiarazioni. L'Executor può usarlo per ottenere informazioni
    /// sulla struttura dei tipi a runtime.
    /// </remarks>
    private readonly List<int> _udtSymbolIds = [];

    /// <summary>
    /// Indice per un recupero ottimizzato di tutti i simboli di tipo Costante.
    /// Contiene la lista dei <c>SymbolId</c> di tutti i simboli il cui <c>DeclarationNode</c>
    /// è una <see cref="ConstDeclaration"/>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un accesso rapido a tutte le costanti definite nel
    /// programma. Questo è particolarmente utile per la fase di "constant folding" nel
    /// Linker e per la diagnostica.
    /// 
    /// Fonte di Verità: Viene popolato in `RegisterSymbol` ogni volta che viene registrato
    /// un nodo di tipo `ConstDeclaration`.
    ///
    /// Utilizzo Chiave (Linker):
    /// Il metodo `GetAllConstants()` userà questo indice per ottenere una lista di tutte le
    /// costanti, che il Linker può poi utilizzare per sostituire i riferimenti alle costanti
    /// con i loro valori letterali in tutto l'AST.
    /// </remarks>
    private readonly List<int> _constSymbolIds = [];

    /// <summary>
    /// Indice per un recupero ottimizzato di tutti i simboli di tipo ENUM.
    /// Contiene la lista dei <c>SymbolId</c> di tutti i simboli il cui <c>DeclarationNode</c>
    /// è una <see cref="EnumDefinition"/>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Accelerare l'accesso a tutte le definizioni <c>ENUM...END ENUM</c>
    /// del programma, evitando scansioni complete della `SymbolTable`. Questo è cruciale per
    /// il Linker durante la fase di risoluzione dei tipi e per l'analisi statica.
    /// 
    /// Fonte di Verità: Viene popolato in `RegisterSymbol` ogni volta che viene registrato
    /// un nodo di tipo `EnumDefinition`. È una vista materializzata per ottimizzare
    /// una query comune.
    ///
    /// Utilizzo Chiave (Linker):
    /// Il metodo pubblico `GetAllEnums()` usa questo indice per fornire al `Linker`
    /// un elenco di tutte le enumerazioni per la validazione e la risoluzione dei tipi,
    /// in particolare per risolvere espressioni come `MyEnum.MyMember` in valori costanti.
    /// </remarks>
    private readonly List<int> _enumSymbolIds = [];

    /// <summary>
    /// Indice per un recupero ottimizzato di tutti i simboli di tipo Interfaccia.
    /// Contiene la lista dei <c>SymbolId</c> di tutti i simboli il cui <c>DeclarationNode</c>
    /// è una <see cref="InterfaceDefinition"/>.
    /// </summary>
    private readonly List<int> _interfaceSymbolIds = [];

    /// <summary>
    /// Inizializza una nuova SymbolTable, creando lo scope radice.
    /// </summary>
    public SymbolTable()
    {
        // Lo scope radice ha sempre ID 0 e nessun genitore (-1).
        var rootScope = new Scope(0, GlobalScopeName, ScopeType.Global, -1);
        _scopes.Add(rootScope);
        _currentScopeId = 0;
    }

    /// <summary>
    /// Ottiene il numero totale di simboli registrati nella tabella.
    /// </summary>
    public int Count => _symbols.Count;

    /// <summary>Tutti i simboli registrati, in ordine di dichiarazione (l'indice coincide col
    /// SymbolId per gli id non negativi). Usata dalla finalizzazione deterministica LIFO degli
    /// oggetti globali (IlProgramCompiler.EmitFinalizeGlobalObjects).</summary>
    public IReadOnlyList<Symbol> AllSymbols => _symbols;

    // ========================================================================
    // --- API PER LA GESTIONE DEGLI SCOPE (Usata dal Parser) ---
    // ========================================================================

    /// <summary>
    /// Recupera un oggetto <see cref="Scope"/> tramite il suo ID univoco.
    /// </summary>
    /// <param name="id">L'ID dello scope da recuperare.</param>
    /// <returns>L'istanza dello <see cref="Scope"/> corrispondente.</returns>
    public Scope GetScope(int id) => _scopes[id];

    /// <summary>
    /// Recupera l'oggetto <see cref="Scope"/> attualmente attivo, in cui verranno registrati i nuovi simboli.
    /// </summary>
    /// <returns>L'istanza dello <see cref="Scope"/> corrente.</returns>
    public Scope GetCurrentScope() => _scopes[_currentScopeId];

    /// <summary>
    /// Crea un nuovo scope figlio di quello corrente e lo imposta come attivo.
    /// Questo metodo viene chiamato dal parser quando entra in un nuovo blocco lessicale
    /// (procedura, blocco IF, ciclo FOR, etc.).
    /// </summary>
    /// <param name="name">Il nome del nuovo scope (es. nome della procedura o un nome generato per un blocco).</param>
    /// <param name="type">Il tipo semantico del nuovo scope.</param>
    /// <param name="ownerSymbolId">L'ID opzionale del simbolo che "possiede" questo scope (es. il SymbolId di una procedura o di una classe).</param>
    public void EnterScope(string name, ScopeType type, int ownerSymbolId = -1)
    {
        // Cerca tra i figli dello scope corrente se esiste già uno scope con lo stesso nome, tipo e proprietario.
        var parentScope = GetCurrentScope();

        var existingChildScopeId = parentScope.ChildrenIds.FirstOrDefault(childId =>
        {
            var childScope = _scopes[childId];
            return childScope.Name.Equals(name, StringComparison.OrdinalIgnoreCase) &&
                   childScope.Type == type &&
                   childScope.OwnerSymbolId == ownerSymbolId;
        }, -1);

        if (existingChildScopeId != -1)
        {
            // Se uno scope corrispondente esiste già, imposta quello come corrente senza crearne uno nuovo.
            _currentScopeId = existingChildScopeId;
            return;
        }

        // Se non esiste, crea un nuovo scope come prima.
        var newScope = new Scope(_scopes.Count, name, type, _currentScopeId, ownerSymbolId);
        _scopes.Add(newScope);
        parentScope.ChildrenIds.Add(newScope.Id);
        _currentScopeId = newScope.Id;
    }

    /// <summary>
    /// Esce dallo scope corrente, impostando lo scope genitore come nuovo scope attivo.
    /// Questo metodo viene chiamato dal parser quando esce da un blocco lessicale.
    /// L'operazione non può andare oltre lo scope radice (globale).
    /// </summary>
    public void ExitScope()
    {
        var parentId = GetCurrentScope().ParentId;
        _currentScopeId = parentId >= 0 ? parentId : 0; // Non si può uscire dallo scope globale
    }

    /// <summary>
    /// Reimposta lo scope corrente allo scope radice (globale).
    /// Questo metodo è un'operazione di "reset" critico per il processo di compilazione a passate.
    /// </summary>
    /// <remarks>
    /// Contesto di Utilizzo Obbligatorio: Questo metodo DEVE essere chiamato dall'Orchestrator
    /// all'inizio del parsing di ogni nuovo file sorgente durante la FASE 1 (Parsing Puro).
    /// Poiché la stessa istanza di `SymbolTable` viene riutilizzata per parsare più file,
    /// è fondamentale garantire che il parsing di un nuovo file inizi sempre dallo scope
    /// radice (`_global`), prevenendo che lo stato dello scope di un file precedente "contamini"
    /// la registrazione dei simboli del file corrente.
    /// La mancata chiamata a questo metodo prima di ogni `parser.ParseModuleSource()` è un errore architetturale.
    /// </remarks>
    public void ResetScope()
    {
        _currentScopeId = 0; // 0 è sempre l'ID dello scope radice (_global)
    }

    // ========================================================================
    // --- API PER LA CREAZIONE E RISOLUZIONE DEI SIMBOLI ---
    // ========================================================================

    /// <summary>
    /// Metodo centrale per registrare un nuovo simbolo (versione 0) nello scope corrente.
    /// Gestisce la creazione e l'aggiunta a liste di overload se il nome è già presente.
    /// Questa è l'unica API autorizzata a modificare la `SymbolTable` aggiungendo nuovi simboli.
    /// </summary>
    /// <remarks>
    /// Responsabilità:
    /// 1. Crea una nuova istanza di `Symbol` e le assegna un ID univoco.
    /// 2. Aggiunge il simbolo alla lista master `_symbols`.
    /// 3. Aggiunge l'ID del simbolo alla mappa `SymbolsByName` dello scope corrente, gestendo l'overloading.
    /// 4. Popola tutti gli indici secondari (`_symbolsByFileId`, `_globalSymbolIds`, etc.) per ottimizzare le ricerche.
    /// 5. Valida e lancia un errore se viene dichiarato un simbolo duplicato (non funzione) nello stesso scope.
    /// 
    /// Flusso di Lavoro (Parser):
    /// Il Parser chiama questo metodo per ogni dichiarazione. L'ID restituito viene usato per
    /// annotare i nodi AST, trasformando un nome testuale in un riferimento numerico univoco.
    /// </remarks>
    /// <param name="originalToken">Il token del codice sorgente che definisce il nome del simbolo. Fondamentale per la diagnostica.</param>
    /// <param name="flags">I flag semantici che descrivono le proprietà del simbolo (visibilità, storage, tipo di simbolo, etc.).</param>
    /// <param name="dataType">Il tipo di dato del simbolo, che può essere concreto o <see cref="InferredType"/>.</param>
    /// <param name="declarationNode">Il nodo AST che rappresenta la dichiarazione completa del simbolo. Può essere un placeholder durante la pre-scansione.</param>
    /// <returns>L'ID univoco assegnato al nuovo simbolo.</returns>
    public int RegisterSymbol(Token originalToken, SymbolFlags flags, DataType dataType, AstNode declarationNode)
    {
        var scope = GetCurrentScope();
        var name = originalToken.Lexeme;

        // Se il simbolo è una procedura, non controlliamo i duplicati qui, ma li aggiungiamo alla lista.
        // Il controllo di duplicazione per le procedure deve basarsi sulla firma completa e viene fatto a un livello superiore.
        // Per i simboli non procedurali (variabili, tipi, costanti), il duplicato è un errore.
        //
        // ECCEZIONE (2026-10-01, fix collisione nome TYPE/variabile a livello di modulo — vedi
        // Bugs/(open) - TYPE-TopLevel-NonRegistrato-Come-Tipo-Modulo.md): una VARIABILE/
        // COSTANTE può coesistere con un TYPE/CLASS/PROCEDURE/INTERFACE/ENUM dello stesso nome
        // case-insensitive nello STESSO scope — es. `TYPE P ... END TYPE` + `DIM p AS P` in uno
        // script top-level, dove entrambi vivono nello scope di modulo (dentro una procedura le
        // variabili sono in uno scope separato, nessuna collisione mai). Il nome di un tipo e il
        // nome di una variabile non sono mai sintatticamente ambigui nel punto d'uso (uno segue
        // sempre "AS", l'altro è un riferimento-valore in espressione — risolti da meccanismi
        // completamente separati, vedi TryResolveTypeName/FindSymbolIdInScopeChain). Un vero
        // duplicato nella STESSA categoria (due variabili, due TYPE, ecc.) resta un errore.
        var isValueDeclaration = declarationNode is VariableDeclaration or ConstDeclaration;
        var existingAreAllModuleTypes = scope.SymbolsByName.TryGetValue(name, out var existingIdsForCheck)
            && existingIdsForCheck.Count > 0
            && existingIdsForCheck.All(id => _symbols[id].DeclarationNode is UserDefinedType or ClassDefinition
                or ProcedureDefinition or InterfaceDefinition or EnumDefinition);
        if (scope.SymbolsByName.TryGetValue(name, out var existingIds) && !flags.HasFlag(SymbolFlags.IsFunction)
            && !(isValueDeclaration && existingAreAllModuleTypes))
        {
            var existingSymbol = _symbols[existingIds[0]];
            var st = new System.Diagnostics.StackTrace(1, true);
            var trace = string.Join(" | ", st.GetFrames()?.Take(6).Select(f => $"{f.GetMethod()?.Name}:{f.GetFileLineNumber()}") ?? []);
            throw new BasicException(
                originalToken,
                $"Un simbolo con nome '{name}' è già stato dichiarato in questo scope (ID:{scope.Id}) " +
                $"alla riga {existingSymbol.OriginalToken.Line}. Trace: {trace}",
                ErrorCode.DuplicateDefinition);
        }

        var newId = _symbols.Count;

        // Diagnostica
        if (newId == -1 && flags.HasFlag(SymbolFlags.IsFunction))
        {
            Console.WriteLine(
                $"[CRITICAL ERROR - REGISTRATION] Tentativo di registrare FUNCTION '{name}' con ID {newId} (ID NEGATIVO!). CHIAMATA FALLITA.");
        }

        var newSymbol = new Symbol(newId, originalToken, flags, dataType, scope.Id, declarationNode);
        _symbols.Add(newSymbol);

        if (!scope.SymbolsByName.TryGetValue(name, out var value))
        {
            value = [];
            scope.SymbolsByName[name] = value;
        }

        value.Add(newId);

        // Popola l'indice per FileId per ottimizzare le ricerche per modulo.
        var fileId = originalToken.Position.FileId;
        if (fileId != -1)
        {
            if (!_symbolsByFileId.TryGetValue(fileId, out var fileSymbols))
            {
                fileSymbols = [];
                _symbolsByFileId[fileId] = fileSymbols;
            }

            fileSymbols.Add(newId);
        }

        // Popola gli indici di categoria di storage per un avvio rapido dell'Executor.
        if (flags.HasFlag(SymbolFlags.IsGlobal))
        {
            _globalSymbolIds.Add(newId);
        }
        else if (flags.HasFlag(SymbolFlags.IsStaticStorage))
        {
            _staticSymbolIds.Add(newId);
        }

        // Popola i nuovi indici per tipo per ottimizzare i metodi GetAll...()
        switch (declarationNode)
        {
            case ProcedureDefinition:
                _procedureSymbolIds.Add(newId);
                break;
            case ClassDefinition:
                _classSymbolIds.Add(newId);
                break;
            case InterfaceDefinition:
                _interfaceSymbolIds.Add(newId);
                break;
            case UserDefinedType:
                _udtSymbolIds.Add(newId);
                break;
            case EnumDefinition:
                _enumSymbolIds.Add(newId);
                break;
            case ConstDeclaration:
                _constSymbolIds.Add(newId);
                break;
        }

        // 1. Trova il namespace (risalita dello scope, ma solo al momento della registrazione).
        var nsScope = FindParentScopeOfType(scope.Id, ScopeType.Namespace);
        var nsName = nsScope?.Name ?? GlobalScopeName;

        // 2. Registra l'associazione SymbolId -> NamespaceName
        _symbolIdToNamespaceName[newId] = nsName;

        // 3. Registra l'associazione NamespaceName -> SymbolId (solo per i simboli di alto livello)
        if (scope.Type != ScopeType.Namespace) return newId;
        //if (scope.Type != ScopeType.Namespace && scope.Type != ScopeType.Global) return newId;

        if (!_globalSymbolIdsByNamespace.TryGetValue(nsName, out var nsSymbols))
        {
            nsSymbols = [];
            _globalSymbolIdsByNamespace[nsName] = nsSymbols;
        }

        nsSymbols.Add(newId);

        return newId;
    }

    /// <summary>
    /// Overload per registrare un simbolo in uno scope specifico, invece di quello corrente.
    /// Utilizzato da componenti stateless come il Rewriter per la creazione di variabili implicite.
    /// </summary>
    public int RegisterSymbol(Token originalToken, SymbolFlags flags, DataType dataType, AstNode declarationNode,
        int targetScopeId)
    {
        // Usa lo scope target invece di quello corrente.
        var scope = GetScope(targetScopeId);
        var name = originalToken.Lexeme;

        if (scope.SymbolsByName.TryGetValue(name, out var existingIds) && !flags.HasFlag(SymbolFlags.IsFunction))
        {
            var existingSymbol = _symbols[existingIds[0]];
            throw new BasicException(
                originalToken,
                $"REGISTER-DUP {existingSymbol.OriginalToken.Line}.",
                ErrorCode.DuplicateDefinition);
        }

        var newId = _symbols.Count;
        var newSymbol = new Symbol(newId, originalToken, flags, dataType, scope.Id, declarationNode);
        _symbols.Add(newSymbol);

        if (!scope.SymbolsByName.TryGetValue(name, out var value))
        {
            value = [];
            scope.SymbolsByName[name] = value;
        }

        value.Add(newId);

        var fileId = originalToken.Position.FileId;
        if (fileId != -1)
        {
            if (!_symbolsByFileId.TryGetValue(fileId, out var fileSymbols))
            {
                fileSymbols = [];
                _symbolsByFileId[fileId] = fileSymbols;
            }

            fileSymbols.Add(newId);
        }

        if (flags.HasFlag(SymbolFlags.IsGlobal))
        {
            _globalSymbolIds.Add(newId);
        }
        else if (flags.HasFlag(SymbolFlags.IsStaticStorage))
        {
            _staticSymbolIds.Add(newId);
        }

        switch (declarationNode)
        {
            case ProcedureDefinition:
                _procedureSymbolIds.Add(newId);
                break;
            case ClassDefinition:
                _classSymbolIds.Add(newId);
                break;
            case InterfaceDefinition:
                _interfaceSymbolIds.Add(newId);
                break;
            case UserDefinedType:
                _udtSymbolIds.Add(newId);
                break;
            case EnumDefinition:
                _enumSymbolIds.Add(newId);
                break;
            case ConstDeclaration:
                _constSymbolIds.Add(newId);
                break;
        }

        var nsScope = FindParentScopeOfType(scope.Id, ScopeType.Namespace);
        var nsName = nsScope?.Name ?? GlobalScopeName;

        _symbolIdToNamespaceName[newId] = nsName;

        if (scope.Type == ScopeType.Namespace)
        {
            if (!_globalSymbolIdsByNamespace.TryGetValue(nsName, out var nsSymbols))
            {
                nsSymbols = [];
                _globalSymbolIdsByNamespace[nsName] = nsSymbols;
            }

            nsSymbols.Add(newId);
        }

        return newId;
    }

    /// <summary>
    /// Overload universale per registrare un simbolo basato su un nodo di dichiarazione (Decl).
    /// Questo metodo astrae la logica di estrazione del token e dei flag specifici per ogni
    /// tipo di dichiarazione, semplificando il codice nel Parser.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un punto di ingresso unico e a più alto livello per la registrazione
    /// dei simboli quando il nodo AST della dichiarazione è già stato costruito.
    /// 
    /// Logica Interna:
    /// 1. Estrae il `Token` del nome dal `declarationNode` (es. `OriginalName` da `VariableDeclaration`).
    /// 2. Deriva i flag semantici appropriati dal tipo del `declarationNode` (es. aggiunge `SymbolFlags.IsParameter`
    ///    se il nodo è un `Parameter`).
    /// 3. Combina i flag derivati con quelli passati esplicitamente.
    /// 4. Delega il lavoro di registrazione vero e proprio all'overload principale di `RegisterSymbol`.
    ///
    /// Utilizzo Chiave (Parser):
    /// Invece di scrivere logica `switch` duplicata per estrarre token e flag, il Parser può
    /// semplicemente chiamare questo metodo dopo aver costruito un nodo `Decl`, rendendo il codice
    /// più pulito e meno propenso a errori.
    /// </remarks>
    /// <param name="declarationNode">Il nodo AST della dichiarazione (`VariableDeclaration`, `Parameter`, etc.).</param>
    /// <param name="explicitFlags">Flag semantici aggiuntivi da combinare con quelli derivati dal nodo.</param>
    /// <param name="dataType">Il tipo di dato del simbolo.</param>
    /// <returns>L'ID univoco assegnato al nuovo simbolo.</returns>
    public int RegisterSymbol(Decl declarationNode, SymbolFlags explicitFlags, DataType dataType)
    {
        Token originalToken;
        try
        {
            originalToken = declarationNode switch
            {
                VariableDeclaration vd => vd.NameCleaned,
                ConstDeclaration cd => cd.OriginalToken,
                Parameter p => p.Name,
                PropertyDefinition pd => pd.Name,
                ProcedureDefinition pd => pd.Name,
                TypeField tf => tf.Name,
                EnumMember em => em.Name,
                _ => throw new InvalidOperationException(
                    $"Cannot determine primary token for declaration node {declarationNode.GetType().Name}")
            };
        }
        catch (InvalidOperationException ex)
        {
            throw new InvalidOperationException($"Impossibile registrare il simbolo. Errore interno: {ex.Message}");
        }

        var derivedFlags = explicitFlags;
        derivedFlags |= declarationNode switch
        {
            Parameter p => SymbolFlags.IsParameter | p.Flags,
            VariableDeclaration vd => SymbolFlags.IsLocal | vd.Flags |
                                      (vd.Dimensions.Any() ? SymbolFlags.IsArray : SymbolFlags.None),
            PropertyDefinition pd => SymbolFlags.IsProperty | pd.Flags,
            ConstDeclaration => SymbolFlags.IsConst,
            ProcedureDefinition p => p.Flags,
            TypeField => SymbolFlags.IsField,
            EnumMember => SymbolFlags.IsEnumMember | SymbolFlags.IsConst,
            _ => SymbolFlags.None
        };
        if (declarationNode is ProcedureDefinition)
        {
            derivedFlags |= SymbolFlags.IsFunction;
        }

        // Delega tutto il lavoro di registrazione (validazione, aggiunta agli indici, etc.)
        // all'overload principale per centralizzare la logica ed evitare duplicazioni.
        return RegisterSymbol(originalToken, derivedFlags, dataType, declarationNode);
    }

    /// <summary>
    /// Overload universale per registrare un simbolo basato su un nodo di dichiarazione di tipo (DataType).
    /// Questo metodo astrae la logica per Tipi (UDT), Classi e Interfacce, che sono tutti sottotipi di DataType.
    /// </summary>
    /// <remarks>
    /// <b>Ruolo Architetturale:</b>
    /// Questo metodo è un helper cruciale per il Binder (Fase 2). Quando il Binder incontra una dichiarazione
    /// di un tipo complesso (`TYPE`, `CLASS`, `INTERFACE`), invoca questo metodo per centralizzare la logica
    /// di estrazione del nome e dei flag, e per delegare la registrazione alla funzione `RegisterSymbol` di base.
    /// Questo approccio garantisce che tutti i tipi di primo livello vengano registrati in modo coerente.
    /// </remarks>
    /// <param name="declarationNode">Il nodo AST della dichiarazione, che deve essere un sottotipo di `DataType` (es. `UserDefinedType`, `ClassDefinition`, `InterfaceDefinition`).</param>
    /// <param name="explicitFlags">Flag semantici aggiuntivi da combinare con quelli derivati dal nodo stesso.</param>
    /// <returns>L'ID univoco assegnato al nuovo simbolo di tipo.</returns>
    /// <exception cref="InvalidOperationException">
    /// Lanciata se il `declarationNode` fornito non è un tipo di dichiarazione riconosciuto (es. un tipo primitivo come `StringType`).
    /// </exception>
    public int RegisterSymbol(DataType declarationNode, SymbolFlags explicitFlags)
    {
        Token originalToken;
        var derivedFlags = explicitFlags;

        switch (declarationNode)
        {
            case UserDefinedType udt:
                originalToken = udt.OriginalName;
                derivedFlags |= udt.Flags; // Flags include IsStruct
                break;
            case ClassDefinition cd:
                originalToken = cd.OriginalName;
                derivedFlags |= cd.Flags; // Flags include IsClass
                break;
            case InterfaceDefinition idef:
                originalToken = idef.OriginalName;
                derivedFlags |= idef.Flags; // Flags include IsInterface
                break;
            default:
                throw new InvalidOperationException(
                    $"Cannot register symbol for a DataType node of type {declarationNode.GetType().Name} that is not a user-defined type declaration.");
        }

        // Per i tipi, il DataType del simbolo è il nodo di dichiarazione stesso.
        // Questo permette di accedere alla definizione completa (campi, metodi)
        // direttamente dal simbolo.
        return RegisterSymbol(originalToken, derivedFlags, declarationNode, declarationNode);
    }

    /// <summary>
    /// Overload per registrare un simbolo quando il nodo di dichiarazione completo non è ancora disponibile.
    /// Usa il token originale come placeholder per il nodo di dichiarazione.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Gestire i casi in cui un simbolo deve essere registrato prima che il
    /// suo intero nodo AST di dichiarazione sia stato parsato. Questo è comune durante la
    /// pre-scansione o per le dichiarazioni implicite di variabili.
    /// 
    /// Logica Interna:
    /// Questo metodo è un semplice helper che delega la chiamata all'overload principale,
    /// passando `originalToken` stesso come `declarationNode` temporaneo. Sarà compito di una
    /// fase successiva del compilatore (tipicamente il Parser completo) richiamare
    /// `UpdateSymbolDeclaration` per sostituire questo placeholder con il nodo AST definitivo.
    ///
    /// Utilizzo Chiave (Pre-Scan e Dichiarazioni Implicite):
    /// 1. Pre-Scan: Quando si scopre la firma di una funzione, si registra il simbolo usando solo il suo token
    ///    prima di averne parsato il corpo.
    /// 2. Dichiarazione Implicita: Quando il Parser incontra `A = 10` senza una `DIM A` precedente,
    ///    registra `A` come nuovo simbolo usando questo overload.
    /// </remarks>
    /// <param name="originalToken">Il token del codice sorgente che definisce il nome del simbolo.</param>
    /// <param name="flags">I flag semantici che descrivono le proprietà del simbolo.</param>
    /// <param name="dataType">Il tipo di dato del simbolo.</param>
    /// <returns>L'ID univoco assegnato al nuovo simbolo.</returns>
    public int RegisterSymbol(Token originalToken, SymbolFlags flags, DataType dataType)
    {
        // Delega al metodo principale, usando il token come nodo di dichiarazione (fallback).
        return RegisterSymbol(originalToken, flags, dataType, originalToken);
    }

    /// <summary>
    /// Restituisce gli ID di tutti i simboli globali registrati nel programma.
    /// Questo metodo fornisce un accesso O(1) all'elenco pre-filtrato <c>_globalSymbolIds</c>,
    /// ottimizzando l'inizializzazione del programma da parte dell'Executor.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Offrire un'API pubblica e sicura per accedere all'indice dei simboli
    /// globali senza esporre la lista interna. Questo astrae l'implementazione (una `List{int}`)
    /// e fornisce un punto di accesso chiaro per l'Executor.
    /// 
    /// Utilizzo Chiave (Executor):
    /// Questo è uno dei primi metodi chiamati dall'Executor all'avvio. Utilizza gli ID restituiti
    /// per preparare lo scope delle variabili globali in `BasicRuntime` prima che venga eseguita
    /// la prima riga di codice.
    /// </remarks>
    /// <returns>Una collezione enumerabile contenente gli ID univoci di tutti i simboli globali.</returns>
    public IEnumerable<int> GetGlobalSymbolIds() => _globalSymbolIds;

    /// <summary>
    /// Restituisce gli ID di tutti i simboli statici registrati nel programma.
    /// Questo metodo fornisce un accesso O(1) all'elenco pre-filtrato <c>_staticSymbolIds</c>,
    /// ottimizzando l'inizializzazione del programma da parte dell'Executor.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Offrire un'API pubblica per accedere all'indice dei simboli
    /// con storage statico. Questo permette all'Executor di identificare e preparare in anticipo
    /// la memoria per le variabili `STATIC` definite all'interno delle procedure.
    /// 
    /// Utilizzo Chiave (Executor):
    /// All'avvio, l'Executor chiama questo metodo per ottenere la lista di tutti i simboli statici.
    /// Successivamente, raggruppa questi simboli per procedura contenitrice e crea i `VariableScope`
    /// statici dedicati in `BasicRuntime.StaticScopesByProcedure`, assicurando che la memoria per
    /// queste variabili persista tra le chiamate alle rispettive procedure.
    /// </remarks>
    /// <returns>Una collezione enumerabile contenente gli ID univoci di tutti i simboli statici.</returns>
    public IEnumerable<int> GetStaticSymbolIds() => _staticSymbolIds;

    /// <summary>
    /// Recupera tutti i simboli definiti in un dato file in modo efficiente,
    /// utilizzando l'indice pre-calcolato <c>_symbolsByFileId</c>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un'API pubblica per interrogare la `SymbolTable` su base
    /// per-file. Questo è fondamentale per l'architettura a moduli, dove il `Linker` e l'`Orchestrator`
    /// devono spesso operare sui simboli appartenenti a un singolo file sorgente. L'uso dell'indice
    /// garantisce che questa operazione sia estremamente veloce.
    /// 
    /// Utilizzo Chiave (Orchestrator/Linker):
    /// 1. `BasicProgram.GetProceduresForModule(string filePath)`: Questo metodo helper usa `GetSymbolsByFileId`
    ///    per trovare rapidamente tutte le procedure definite in un file specifico, senza scansionare
    ///    l'intera tabella dei simboli.
    /// 2. Analisi delle Dipendenze: Durante il linking, può essere usato per trovare tutti i simboli
    ///    esportati da un modulo per risolvere gli `IMPORT` di altri moduli.
    /// </remarks>
    /// <param name="fileId">L'ID del file da cui recuperare i simboli.</param>
    /// <returns>Una collezione enumerabile di tutti i simboli definiti in quel file, o una collezione vuota se il file non contiene simboli.</returns>
    public IEnumerable<Symbol> GetSymbolsByFileId(int fileId)
    {
        return _symbolsByFileId.TryGetValue(fileId, out var symbolIds) ? symbolIds.Select(id => _symbols[id]) : [];
    }

    /// <summary>
    /// Restituisce il nome del namespace contenitore (o "_global") per un dato <c>SymbolId</c>.
    /// Questa è un'operazione in tempo costante (O(1)) grazie all'indice pre-calcolato <c>_symbolIdToNamespaceName</c>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Rispondere in modo ultra-veloce a una delle domande più frequenti
    /// del Linker: "A quale namespace appartiene questo simbolo?". La capacità di risolvere
    /// questa informazione senza risalire l'albero degli scope è una delle ottimizzazioni
    /// fondamentali di questa architettura.
    /// 
    /// Utilizzo Chiave (Linker):
    /// Durante la risoluzione di una chiamata a funzione non qualificata (es. `MIA_FUNZIONE()`),
    /// il `Linker` ottiene una lista di simboli candidati. Per ogni candidato, chiama questo metodo
    /// per ottenere il suo namespace di origine. Successivamente, usa questa informazione per
    /// applicare le regole di visibilità (es. "questo namespace è importato nel modulo corrente?")
    /// e scartare i candidati non validi.
    /// </remarks>
    /// <param name="symbolId">L'ID del simbolo da interrogare.</param>
    /// <returns>Il nome del namespace come stringa.</returns>
    public string GetNamespaceNameForSymbol(int symbolId)
    {
        return _symbolIdToNamespaceName.GetValueOrDefault(symbolId, GlobalScopeName);
    }

    /// <summary>
    /// Restituisce la lista di tutti i <c>SymbolId</c> per i simboli di alto livello registrati in un dato namespace.
    /// Questa è un'operazione in tempo costante (O(1)) grazie all'indice pre-calcolato <c>_globalSymbolIdsByNamespace</c>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un punto di accesso rapido a tutti i membri di un namespace.
    /// È il metodo complementare a `GetNamespaceNameForSymbol` e serve per le interrogazioni
    /// che partono dal nome di un namespace invece che da un simbolo.
    /// 
    /// Utilizzo Chiave (Linker e Analisi Statica):
    /// Quando il Linker deve risolvere un nome qualificato come `Math.Sin`, per prima cosa
    /// chiama questo metodo per ottenere tutti gli ID dei simboli nel namespace "Math".
    /// Successivamente, itera su questa lista (tipicamente corta) per trovare il simbolo
    /// il cui nome è "Sin" e la cui firma corrisponde alla chiamata. Questo evita di dover
    /// scansionare l'intera SymbolTable.
    /// </remarks>
    /// <param name="namespaceName">Il nome del namespace da interrogare.</param>
    /// <returns>Una lista di <c>SymbolId</c> se il namespace esiste e contiene simboli; altrimenti, <c>null</c>.</returns>
    public List<int>? GetSymbolIdsByNamespace(string namespaceName)
    {
        _globalSymbolIdsByNamespace.TryGetValue(namespaceName, out var symbolIds);
        return symbolIds;
    }

    /// <summary>
    /// Ottiene un simbolo tramite il suo ID univoco. Questa è l'operazione di lookup fondamentale
    /// dell'architettura ID-based e viene eseguita in tempo costante (O(1)).
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire l'unico punto di accesso per recuperare i metadati completi
    /// di un simbolo una volta che il suo ID è stato risolto. Qualsiasi componente del compilatore
    /// (Parser, Linker, Executor) che possiede un `SymbolId` (ottenuto da un nodo AST o da un altro
    /// indice) deve usare questo metodo per accedere alle informazioni semantiche del simbolo.
    /// 
    /// Fonte di Verità: Questo metodo legge direttamente dalla lista master `_symbols`, garantendo
    /// che l'informazione restituita sia sempre autorevole.
    ///
    /// Utilizzo Chiave (Universale):
    /// Questo metodo è onnipresente. Esempi:
    /// - Il Linker, dopo aver scelto un `SymbolId` candidato per una chiamata di funzione, usa `GetSymbol`
    ///   per recuperare la `ProcedureDefinition` completa e verificarne la firma.
    /// - L'Executor, quando incontra un `VariableExpr(123)`, chiama `GetSymbol(123)` per ottenere il
    ///   tipo di dato e i flag della variabile prima di leggerne o scriverne il valore.
    /// </remarks>
    /// <param name="id">L'ID univoco del simbolo da recuperare.</param>
    /// <returns>L'istanza di <see cref="Symbol"/> corrispondente all'ID.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Lanciata se l'ID non è valido (negativo o fuori dai limiti).</exception>
    public Symbol GetSymbol(int id)
    {
        if (id < 0 || id >= _symbols.Count)
        {
            // SymbolId negativo e' usato dal linker come placeholder per keyword BASIC
            // non risolte (es. OUTPUT/INPUT come argomento di OPEN). Restituire un dummy
            // invece di throw evita crash a cascata in ExecuteArrayAccessStep e simili.
            return new Symbol(-1,
                new Token(TokenKind.Identifier, "__dummy", null, SourcePosition.Unknown),
                SymbolFlags.None, new InferredType(), -1, null!);
        }

        return _symbols[id];
    }

    /// <summary>
    /// Ottiene un simbolo tramite il suo ID univoco e verifica che possieda i flag semantici richiesti.
    /// Questo overload garantisce la sicurezza dei tipi a livello semantico durante la compilazione.
    /// </summary>
    /// <remarks>
    /// Utilizzo Chiave: Questo metodo viene usato quando il codice chiamante si aspetta che un
    /// `SymbolId` rappresenti un tipo specifico di simbolo. Ad esempio, quando il Linker risolve
    /// una chiamata di funzione, può usare questo metodo per assicurarsi che l'ID trovato
    /// punti effettivamente a un simbolo con il flag `IsFunction`.
    /// </remarks>
    /// <param name="id">L'ID univoco del simbolo da recuperare.</param>
    /// <param name="expectedFlags">I flag che il simbolo deve possedere.</param>
    /// <returns>L'istanza di <see cref="Symbol"/> se l'ID è valido e i flag corrispondono.</returns>
    /// <exception cref="ArgumentOutOfRangeException">Lanciata se l'ID non è valido.</exception>
    /// <exception cref="InvalidOperationException">Lanciata se il simbolo trovato non possiede i flag richiesti.</exception>
    public Symbol GetSymbol(int id, SymbolFlags expectedFlags)
    {
        var symbol = GetSymbol(id); // Chiama l'overload base per il recupero e la validazione dell'ID.

        if (!symbol.Has(expectedFlags))
        {
            throw new InvalidOperationException(
                $"Il simbolo '{symbol.OriginalName}' (ID: {id}) non possiede i flag richiesti. " +
                $"Attesi: [{expectedFlags}], Trovati: [{symbol.Flags}].");
        }

        return symbol;
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di interfaccia (<c>InterfaceDefinition</c>) nel programma.
    /// Questa operazione è ottimizzata tramite un indice pre-calcolato.
    /// </summary>
    /// <param name="flags">Un filtro opzionale per restituire solo le interfacce che possiedono tutti i flag specificati.</param>
    /// <returns>Una collezione enumerabile di <see cref="InterfaceDefinition"/> che corrispondono al filtro.</returns>
    public IEnumerable<InterfaceDefinition> GetAllInterfaces(SymbolFlags? flags = null)
    {
        var query = _interfaceSymbolIds.Select(id => _symbols[id]);

        if (flags.HasValue)
        {
            query = query.Where(s => s.Has(flags.Value));
        }

        return query.Select(s => (InterfaceDefinition)s.DeclarationNode);
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di classe (<c>ClassDefinition</c>) nel programma,
    /// con la possibilità di filtrare per flag semantici.
    /// Questa operazione è ottimizzata tramite un indice pre-calcolato.
    /// </summary>
    /// <param name="flags">Un filtro opzionale per restituire solo le classi che possiedono tutti i flag specificati (es. `IsPublic`).</param>
    /// <returns>Una collezione enumerabile di <see cref="ClassDefinition"/> che corrispondono al filtro.</returns>
    public IEnumerable<ClassDefinition> GetAllClasses(SymbolFlags? flags = null)
    {
        var query = _classSymbolIds.Select(id => _symbols[id]);

        if (flags.HasValue)
        {
            query = query.Where(s => s.Has(flags.Value));
        }

        return query.Select(s => (ClassDefinition)s.DeclarationNode);
    }

    /// <summary>
    /// Restituisce un elenco di tutti i tipi definiti dall'utente (<c>UserDefinedType</c>) nel programma,
    /// con la possibilità di filtrare per flag semantici.
    /// Questa operazione è ottimizzata tramite un indice pre-calcolato.
    /// </summary>
    /// <param name="flags">Un filtro opzionale per restituire solo i tipi che possiedono tutti i flag specificati.</param>
    /// <returns>Una collezione enumerabile di <see cref="UserDefinedType"/> che corrispondono al filtro.</returns>
    public IEnumerable<UserDefinedType> GetAllUserDefinedTypes(SymbolFlags? flags = null)
    {
        var query = _udtSymbolIds.Select(id => _symbols[id]);

        if (flags.HasValue)
        {
            query = query.Where(s => s.Has(flags.Value));
        }

        // `_udtSymbolIds` può includere anche SIMBOLI il cui DataType è un UDT (es. una var
        // `DIM p AS P` a livello di modulo): il cast incondizionato lanciava
        // InvalidCastException (VariableDeclaration → UserDefinedType). Si tengono solo le
        // vere DEFINIZIONI di tipo.
        return query.Select(s => s.DeclarationNode).OfType<UserDefinedType>();
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di enumerazione (<c>EnumDefinition</c>) nel programma,
    /// con la possibilità di filtrare per flag semantici.
    /// Questa operazione è ottimizzata tramite un indice pre-calcolato.
    /// </summary>
    /// <param name="flags">Un filtro opzionale per restituire solo le enumerazioni che possiedono tutti i flag specificati.</param>
    /// <returns>Una collezione enumerabile di <see cref="EnumDefinition"/> che corrispondono al filtro.</returns>
    public IEnumerable<EnumDefinition> GetAllEnums(SymbolFlags? flags = null)
    {
        var query = _enumSymbolIds.Select(id => _symbols[id]);

        if (flags.HasValue)
        {
            query = query.Where(s => s.Has(flags.Value));
        }

        return query.Select(s => (EnumDefinition)s.DeclarationNode);
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di procedura (<c>ProcedureDefinition</c>) nel programma,
    /// con la possibilità di filtrare per flag semantici.
    /// Questa operazione è ottimizzata tramite un indice pre-calcolato.
    /// </summary>
    /// <param name="flags">Un filtro opzionale per restituire solo le procedure che possiedono tutti i flag specificati.</param>
    /// <returns>Una collezione enumerabile di <see cref="ProcedureDefinition"/> che corrispondono al filtro.</returns>
    public IEnumerable<ProcedureDefinition> GetAllProcedures(SymbolFlags? flags = null)
    {
        var query = _procedureSymbolIds.Select(id => _symbols[id]);

        if (flags.HasValue)
        {
            query = query.Where(s => s.Has(flags.Value));
        }

        return query.Select(s => (ProcedureDefinition)s.DeclarationNode);
    }

    /// <summary>
    /// Restituisce un elenco di tutte le dichiarazioni di costante (<c>ConstDeclaration</c>) nel programma,
    /// con la possibilità di filtrare per flag semantici.
    /// Questa operazione è ottimizzata tramite un indice pre-calcolato.
    /// </summary>
    /// <param name="flags">Un filtro opzionale per restituire solo le costanti che possiedono tutti i flag specificati.</param>
    /// <returns>Una collezione enumerabile di <see cref="ConstDeclaration"/> che corrispondono al filtro.</returns>
    public IEnumerable<ConstDeclaration> GetAllConstants(SymbolFlags? flags = null)
    {
        var query = _constSymbolIds.Select(id => _symbols[id]);

        if (flags.HasValue)
        {
            query = query.Where(s => s.Has(flags.Value));
        }

        return query.Select(s => (ConstDeclaration)s.DeclarationNode);
    }

    /// <summary>
    /// Risale l'albero degli scope a partire da uno scope iniziale per trovare il primo
    /// antenato (incluso lo scope di partenza) che corrisponde a un dato tipo.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Permettere ai componenti del compilatore di determinare il contesto
    /// semantico in cui si trova un simbolo o un'istruzione. Questo è fondamentale per applicare
    /// regole specifiche del linguaggio che dipendono dal contesto (es. `STATIC` è valido solo
    /// dentro una procedura).
    /// 
    /// Logica Interna: Esegue un'iterazione efficiente sulla gerarchia degli scope, utilizzando
    /// la proprietà `ParentId` per navigare all'indietro fino allo scope radice.
    ///
    /// Utilizzo Chiave (Parser/Linker):
    /// 1. Risoluzione del Namespace: Durante la registrazione di un simbolo (`RegisterSymbol`),
    ///    questo metodo viene usato per trovare lo `Scope` del `Namespace` contenitore e associare
    ///    correttamente il simbolo al suo namespace.
    /// 2. Validazione Sintattica: Il `Parser` può usarlo per verificare se si trova all'interno
    ///    di uno `ScopeType.Procedure` prima di accettare una keyword come `STATIC` o `LOCAL`.
    /// </remarks>
    /// <param name="startScopeId">L'ID dello scope da cui iniziare la ricerca.</param>
    /// <param name="targetType">Il tipo di scope da trovare (es. `ScopeType.Namespace`, `ScopeType.Procedure`).</param>
    /// <returns>L'istanza di <see cref="Scope"/> trovata, o <c>null</c> se non viene trovato nessun antenato di quel tipo.</returns>
    public Scope? FindParentScopeOfType(int startScopeId, ScopeType targetType)
    {
        var currentId = startScopeId;
        while (currentId != -1)
        {
            var currentScope = _scopes[currentId];
            if (currentScope.Type == targetType)
            {
                return currentScope;
            }

            currentId = currentScope.ParentId;
        }

        return null;
    }

    /// <summary>
    /// Aggiorna il tipo di dato di un simbolo esistente. L'operazione di accesso è in O(1).
    /// Utilizza una mutazione non distruttiva (espressione `with`) per creare una nuova istanza
    /// del simbolo con il tipo aggiornato, preservando l'immutabilità.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un'API controllata per modificare un simbolo dopo la sua
    /// registrazione iniziale. Questo è fondamentale per il processo di inferenza dei tipi,
    /// dove un simbolo può essere registrato con un tipo generico (`InferredType`) che viene
    /// poi specializzato dal `Linker`.
    /// 
    /// Utilizzo Chiave (Linker):
    /// Durante la fase di `Rewrite`, il `Linker` analizza le dichiarazioni di variabili con
    /// inizializzazione (es. `DIM A = "ciao"`).
    /// 1. Il `Parser` ha registrato `A` con un tipo di default (es. `DoubleType`).
    /// 2. Il `Linker` inferisce `StringType` dal valore `"ciao"`.
    /// 3. Il `Linker` chiama `UpdateSymbolType(id_di_A, new StringType())` per correggere
    ///    il tipo del simbolo nella `SymbolTable`, che è la fonte di verità.
    /// </remarks>
    /// <param name="symbolId">L'ID del simbolo da aggiornare.</param>
    /// <param name="newType">Il nuovo <see cref="DataType"/> da assegnare al simbolo.</param>
    public void UpdateSymbolType(int symbolId, DataType newType)
    {
        if (symbolId >= 0 && symbolId < _symbols.Count)
        {
            _symbols[symbolId] = _symbols[symbolId] with { DataType = newType };
        }
    }

    /// <summary>
    /// Aggiorna i flag semantici di un simbolo esistente.
    /// Utilizza una mutazione non distruttiva (espressione `with`) per creare una nuova istanza
    /// del simbolo con i flag aggiunti (operazione di OR bit a bit).
    /// </summary>
    /// <param name="symbolId">L'ID del simbolo da aggiornare.</param>
    /// <param name="flagsToAdd">I flag da aggiungere a quelli esistenti.</param>
    public void UpdateSymbolFlags(int symbolId, SymbolFlags flagsToAdd)
    {
        if (symbolId >= 0 && symbolId < _symbols.Count)
        {
            // Aggiunge i flag usando il metodo helper del record Symbol
            _symbols[symbolId] = _symbols[symbolId].Add(flagsToAdd);
        }
    }

    /// <summary>
    /// Rimuove uno o più flag semantici da un simbolo esistente.
    /// Utilizza una mutazione non distruttiva preservando l'immutabilità del record Symbol.
    /// </summary>
    public void RemoveSymbolFlags(int symbolId, SymbolFlags flagsToRemove)
    {
        if (symbolId >= 0 && symbolId < _symbols.Count)
        {
            _symbols[symbolId] = _symbols[symbolId].Remove(flagsToRemove);
        }
    }
    
    /// <summary>
    /// Aggiorna il nodo di dichiarazione AST di un simbolo esistente. Operazione in O(1).
    /// Utilizza una mutazione non distruttiva (espressione `with`) per creare una nuova istanza del simbolo.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Questo metodo è il pilastro dell'architettura di compilazione a
    /// più passate (es. Pre-Scan -> Parse). Permette di registrare un simbolo con un
    /// `declarationNode` placeholder durante una passata iniziale e di sostituirlo con il
    /// nodo AST completo e dettagliato in una passata successiva.
    /// 
    /// Utilizzo Chiave (Parser):
    /// 1. Pre-Scan (Passata 1): Quando incontra `FUNCTION MyFunc(...)`, il Pre-Scan crea un
    ///    `FunctionDefinition` placeholder (senza corpo) e lo registra, ottenendo un `SymbolId`.
    /// 2. Parsing Completo (Passata 2): Il Parser analizza l'intera `FUNCTION...END FUNCTION`,
    ///    costruendo un nuovo nodo `FunctionDefinition` completo di corpo.
    /// 3. Aggiornamento: Il Parser chiama `UpdateSymbolDeclaration(id_di_MyFunc, nodo_completo)`
    ///    per "riempire" le informazioni mancanti nella `SymbolTable`, completando la definizione
    ///    del simbolo senza alterarne l'ID.
    /// </remarks>
    /// <param name="symbolId">L'ID del simbolo da aggiornare.</param>
    /// <param name="newDeclarationNode">Il nuovo nodo AST <see cref="AstNode"/> che rappresenta la dichiarazione completa.</param>
    public void UpdateSymbolDeclaration(int symbolId, AstNode newDeclarationNode)
    {
        if (symbolId >= 0 && symbolId < _symbols.Count)
        {
            _symbols[symbolId] = _symbols[symbolId] with { DeclarationNode = newDeclarationNode };

            // Aggiorna gli indici per tipo (GetAllClasses/GetAllUserDefinedTypes/etc.).
            // Necessario per classi sintetiche (es. __ClosureCtx_*) create dal linker
            // dopo la registrazione iniziale con token placeholder.
            switch (newDeclarationNode)
            {
                case ClassDefinition when !_classSymbolIds.Contains(symbolId):
                    _classSymbolIds.Add(symbolId);
                    break;
                case InterfaceDefinition when !_interfaceSymbolIds.Contains(symbolId):
                    _interfaceSymbolIds.Add(symbolId);
                    break;
                case UserDefinedType when !_udtSymbolIds.Contains(symbolId):
                    _udtSymbolIds.Add(symbolId);
                    break;
                case EnumDefinition when !_enumSymbolIds.Contains(symbolId):
                    _enumSymbolIds.Add(symbolId);
                    break;
            }
        }
    }

    /// <summary>
    /// Restituisce un elenco di tutte le definizioni di procedura (SUB, FUNCTION, etc.)
    /// definite direttamente in uno specifico namespace o nello scope globale.
    /// Questa operazione è ottimizzata per interrogare direttamente lo scope corretto.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire un modo efficiente per recuperare tutti gli overload di
    /// procedure appartenenti a un namespace, senza dover scansionare tutti i simboli.
    /// È la controparte "per namespace" di `GetAllProcedures`.
    /// 
    /// Utilizzo Chiave (Linker):
    /// Quando il Linker deve risolvere una chiamata qualificata come `Math.Sin(...)`, usa questo
    /// metodo per ottenere una lista ristretta di candidati:
    /// 1. Chiama `GetProceduresForNamespace("Math")`.
    /// 2. Itera sulla lista restituita (che contiene tutti gli overload in `Math`) per trovare
    ///    la `ProcedureDefinition` specifica il cui nome è "Sin" e la cui firma è compatibile.
    /// Questo è molto più efficiente che ottenere tutte le procedure del programma e filtrarle.
    /// </remarks>
    /// <param name="namespaceName">Il nome del namespace da cui estrarre le procedure, o `_global` per lo scope globale.</param>
    /// <returns>Un elenco di <see cref="ProcedureDefinition"/> definite nello scope richiesto, o un elenco vuoto se non trovato.</returns>
    public IEnumerable<ProcedureDefinition> GetProceduresForNamespace(string namespaceName)
    {
        Scope? targetScope;

        // Gestisce il caso speciale dello scope globale.
        if (namespaceName.Equals(GlobalScopeName, StringComparison.OrdinalIgnoreCase))
        {
            targetScope = _scopes[0];
        }
        else
        {
            // Logica esistente per i namespace nominati.
            var rootScope = _scopes[0];
            targetScope = rootScope.ChildrenIds
                .Select(id => _scopes[id])
                .FirstOrDefault(s =>
                    s.Type == ScopeType.Namespace && s.Name.Equals(namespaceName, StringComparison.OrdinalIgnoreCase));
        }

        if (targetScope == null)
        {
            return [];
        }

        // La logica di estrazione dei simboli è corretta e rimane invariata.
        return targetScope.SymbolsByName.Values
            .SelectMany(idList => idList) // Appiattisce la lista di liste di ID in una singola lista di ID.
            .Select(symbolId => _symbols[symbolId])
            .Select(symbol => symbol.DeclarationNode)
            .OfType<ProcedureDefinition>();
    }

    /// <summary>
    /// Trova la lista di ID candidati per un simbolo con un dato nome, risalendo la gerarchia
    /// degli scope a partire da quello corrente.
    /// Questo è il metodo fondamentale usato dal Parser per la risoluzione dei nomi non qualificati.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Implementare le regole di scope lessicale del linguaggio. La ricerca
    /// parte dallo scope più interno e risale verso l'esterno (`blocco -> procedura -> namespace -> globale`),
    /// fermandosi al primo scope in cui viene trovato uno o più simboli con il nome richiesto.
    /// Questo garantisce che le variabili locali "nascondano" (shadowing) quelle globali con lo stesso nome.
    /// 
    /// Logica Interna: Esegue un loop che parte da `_currentScopeId` e risale la catena dei `ParentId`
    /// fino a raggiungere lo scope radice. Ad ogni livello, controlla la mappa `SymbolsByName`.
    ///
    /// Utilizzo Chiave (Parser):
    /// Quando il Parser incontra un nome di variabile non qualificato come `X` in un'espressione,
    /// chiama `FindSymbolInScope("X")`.
    /// - Se restituisce una lista di ID, il Parser sa che il simbolo è già stato dichiarato e usa
    ///   quell'ID per annotare il nodo `VariableExpr`.
    /// - Se restituisce `null`, il Parser sa che si tratta di una dichiarazione implicita e procede
    ///   a registrare un nuovo simbolo.
    /// </remarks>
    /// <param name="name">Il nome originale del simbolo da cercare.</param>
    /// <returns>La lista di ID dei simboli candidati trovati nel primo scope accessibile, o <c>null</c> se il simbolo non viene trovato.</returns>
    public List<int>? FindSymbolInScope(string name)
    {
        //Console.WriteLine($"[FINDSYMBOL] --- Inizio ricerca per '{name}' dallo scope corrente (ID: {_currentScopeId}, Nome: '{_scopes[_currentScopeId].Name}') ---");
        var currentId = _currentScopeId;

        while (currentId != -1)
        {
            var currentScope = _scopes[currentId];
            //Console.WriteLine($"[FINDSYMBOL]   - Controllo scope (ID: {currentScope.Id}, Nome: '{currentScope.Name}', Tipo: {currentScope.Type})");

            if (currentScope.SymbolsByName.TryGetValue(name, out var symbolIds))
            {
                //Console.WriteLine($"[FINDSYMBOL]   -> TROVATO! Trovati {symbolIds.Count} candidati (ID: {string.Join(", ", symbolIds)}) nello scope '{currentScope.Name}'. Ricerca terminata.");
                return symbolIds;
            }

            //Console.WriteLine($"[FINDSYMBOL]   -> Non trovato. Risalgo al genitore (ParentID: {currentScope.ParentId}).");
            currentId = currentScope.ParentId;
        }

        //Console.WriteLine($"[FINDSYMBOL] --- Ricerca per '{name}' fallita. Raggiunto lo scope radice senza successo. ---");
        return null;
    }

    /// <summary>
    /// Overload (2026-09-24): quando un nome risolve a PIÙ candidati nello stesso scope
    /// (procedure omonime) e la risoluzione avviene dentro il corpo di una delle procedure
    /// omonime, preferisce il simbolo della procedura contenitrice — è l'assegnazione o
    /// lettura del return-slot (`NomeFunzione = ...` / `PRINT NomeFunzione`), mai un
    /// riferimento all'altro overload. Ritorna -1 se non applicabile (candidato unico,
    /// fuori dal corpo omonimo): il chiamante usa il pick esistente ([0]).
    /// </summary>
    /// <param name="name">Il nome cercato (stesso usato per FindSymbolInScope).</param>
    /// <param name="candidates">Gli ID trovati nello stesso scope.</param>
    /// <returns>L'ID della procedura contenitrice, o -1.</returns>
    public int PreferEnclosingProcedureSymbol(string name, List<int> candidates)
    {
        if (candidates == null || candidates.Count < 2) return -1;
        var scope = _scopes[_currentScopeId];
        while (true)
        {
            if (scope.Type == ScopeType.Procedure && scope.OwnerSymbolId >= 0
                && candidates.Contains(scope.OwnerSymbolId)
                && _symbols[scope.OwnerSymbolId].OriginalName.Equals(name, StringComparison.OrdinalIgnoreCase))
                return scope.OwnerSymbolId;
            if (scope.ParentId < 0) break;
            scope = _scopes[scope.ParentId];
        }
        return -1;
    }

    /// <summary>
    /// Variante con scope di partenza esplicito (per i resolver che non usano il cursore,
    /// es. FindSymbolIdInScopeChain): stessa semantica di PreferEnclosingProcedureSymbol.
    /// </summary>
    public int PreferEnclosingProcedureSymbolFrom(string name, List<int> candidates, int startScopeId)
    {
        if (candidates == null || candidates.Count < 2) return -1;
        if (startScopeId < 0 || startScopeId >= _scopes.Count) return -1;
        var scope = _scopes[startScopeId];
        while (true)
        {
            if (scope.Type == ScopeType.Procedure && scope.OwnerSymbolId >= 0
                && candidates.Contains(scope.OwnerSymbolId)
                && _symbols[scope.OwnerSymbolId].OriginalName.Equals(name, StringComparison.OrdinalIgnoreCase))
                return scope.OwnerSymbolId;
            if (scope.ParentId < 0) break;
            if (scope.ParentId >= _scopes.Count) break;
            scope = _scopes[scope.ParentId];
        }
        return -1;
    }

    /// <summary>
    /// Restituisce una lista di tutti i simboli, attraverso tutti gli scope, che corrispondono a un dato nome.
    /// A differenza di `FindSymbolInScope`, questo metodo non si ferma al primo scope trovato,
    /// ma colleziona tutti i candidati possibili dall'intero programma.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire al `Linker` una lista completa di tutti i possibili candidati
    /// per un nome di funzione non qualificato, prima di applicare le regole di visibilità.
    /// Questo è necessario perché una chiamata a `MyFunc()` potrebbe potenzialmente riferirsi a
    /// `NamespaceA.MyFunc` o `NamespaceB.MyFunc`, e il `Linker` ha bisogno di entrambi i candidati
    /// per determinare se la chiamata è ambigua.
    /// 
    /// Logica Interna: Esegue una scansione O(N) su tutti gli scope definiti nella `SymbolTable`,
    /// aggregando tutti gli `SymbolId` associati al nome richiesto.
    ///
    /// Utilizzo Chiave (Parser/Linker):
    /// 1. Il `Parser`, quando incontra una chiamata a funzione non qualificata (es. `SYS("...")`),
    ///    non avendo il contesto completo di visibilità, chiama `FindSymbolsByName("SYS")` per
    ///    ottenere tutti i candidati possibili.
    /// 2. Passa questa lista di candidati al `Linker` all'interno di un nodo `UnresolvedCallExpr`.
    /// 3. Il `Linker` prenderà questa lista grezza e applicherà la logica di visibilità per filtrarla.
    /// </remarks>
    /// <param name="name">Il nome del simbolo da cercare.</param>
    /// <returns>Una lista di tutti i simboli candidati trovati nel programma.</returns>
    public List<Symbol> FindSymbolsByName(string name)
    {
        var candidates = new List<Symbol>();
        // Itera su tutti gli scope per trovare le corrispondenze.
        foreach (var scope in _scopes)
        {
            if (!scope.SymbolsByName.TryGetValue(name, out var symbolIds)) continue;
            candidates.AddRange(symbolIds.Select(id => _symbols[id]));
        }

        return candidates;
    }

    /// <summary>
    /// Verifica se la firma di una procedura è compatibile con un dato numero di argomenti,
    /// tenendo conto dei parametri obbligatori, opzionali e variadici (`ParamArray`).
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Centralizzare la logica di validazione del numero di argomenti,
    /// che è una parte fondamentale dell'overload resolution e della validazione delle chiamate.
    /// Questo metodo astrae le regole del linguaggio per la compatibilità degli argomenti.
    /// 
    /// Logica Interna:
    /// 1.  Calcola il numero di parametri obbligatori (non opzionali e non variadici).
    /// 2.  Controlla se il numero di argomenti forniti (`argCount`) è almeno pari a quello dei parametri obbligatori.
    /// 3.  Se la procedura ha un parametro `ParamArray` (l'ultimo parametro con il flag `IsParamArray`),
    ///     la chiamata è sempre compatibile finché i parametri obbligatori sono soddisfatti,
    ///     poiché può accettare un numero qualsiasi di argomenti aggiuntivi.
    /// 4.  Se la procedura NON ha un `ParamArray`, il numero di argomenti non deve superare il numero totale
    ///     di parametri definiti.
    ///
    /// Utilizzo Chiave (Linker):
    /// Quando il `Linker` deve risolvere una chiamata a funzione (es. `MyFunc(a, b)`), ottiene una
    /// lista di procedure candidate con il nome "MyFunc". Per ogni candidato, chiama
    /// `IsProcedureSignatureCompatible(candidate, 2)` per scartare rapidamente gli overload che
    /// non corrispondono al numero di argomenti forniti.
    /// </remarks>
    /// <param name="procedure">La definizione della procedura da controllare.</param>
    /// <param name="argCount">Il numero di argomenti forniti nella chiamata.</param>
    /// <returns><c>true</c> se il numero di argomenti è compatibile con la firma; altrimenti, <c>false</c>.</returns>
    public static bool IsProcedureSignatureCompatible(ProcedureDefinition procedure, int argCount)
    {
        var totalParams = procedure.Parameters.Length;

        // Distingue correttamente tra un parametro array normale (es. BYREF Arr()) e un
        // parametro variadico ParamArray.
        var isVariadic = totalParams > 0 && procedure.Parameters.Last().Flags.HasFlag(SymbolFlags.IsParamArray);

        var requiredParams = procedure.Parameters.Count(p =>
            !p.Flags.HasFlag(SymbolFlags.IsOptional) &&
            !p.Flags.HasFlag(SymbolFlags.IsParamArray));

        // Un numero di argomenti inferiore ai parametri obbligatori non è mai valido.
        if (argCount < requiredParams) return false;

        // Se la funzione è variadica, accetta qualsiasi numero di argomenti
        // a partire da quelli obbligatori.
        if (isVariadic) return true;

        // Se non è variadica, il numero di argomenti non può superare il totale dei parametri.
        return argCount <= totalParams;
    }

    // ========================================================================
    // --- API PER LE ETICHETTE (Invariata) ---
    // ========================================================================

    /// <summary>
    /// Aggiunge o aggiorna la posizione di un'etichetta nella mappa globale.
    /// Le etichette sono gestite in una mappa piatta separata dai simboli principali
    /// perché seguono regole di visibilità diverse (locali alla procedura o al modulo).
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire l'unica API per popolare e aggiornare la mappa delle etichette.
    /// Questo metodo viene chiamato dal `Parser` quando incontra una definizione di etichetta
    /// (`:miaEtichetta`) e dal `Linker` (tramite `RebuildLabelMap`) dopo passate di ottimizzazione
    /// che potrebbero aver alterato la posizione delle istruzioni e quindi delle etichette.
    /// 
    /// Logica Interna: Esegue una semplice operazione di inserimento/aggiornamento nel dizionario
    /// <c>_labels</c> usando la chiave qualificata generata da <c>GetQualifiedLabelName</c>.
    /// </remarks>
    /// <param name="key">La chiave qualificata e univoca dell'etichetta.</param>
    /// <param name="location">L'oggetto <see cref="LabelLocation"/> che descrive la nuova posizione dell'etichetta.</param>
    public void AddOrUpdateLabel(string key, LabelLocation location) => _labels[key] = location;

    /// <summary>
    /// Cerca un'etichetta nella mappa globale tramite la sua chiave qualificata. Operazione in O(1).
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire l'API di lookup principale per la risoluzione dei salti (`GOTO`, `GOSUB`).
    /// Questo metodo astrae l'accesso al dizionario interno <c>_labels</c>.
    /// 
    /// Utilizzo Chiave (Linker/Executor):
    /// Quando il `Linker` valida o l'`Executor` esegue un'istruzione `GOTO :miaEtichetta`, per prima cosa
    /// costruisce la chiave qualificata per `:miaEtichetta` in base al contesto corrente.
    /// Successivamente, chiama `FindLabel` con questa chiave per ottenere la `LabelLocation` di
    /// destinazione. Se questo metodo restituisce `null`, significa che l'etichetta non è
    /// definita o non è visibile, e viene lanciato un errore di compilazione/runtime.
    /// </remarks>
    /// <param name="key">La chiave qualificata dell'etichetta da cercare.</param>
    /// <returns>La <see cref="LabelLocation"/> se l'etichetta viene trovata; altrimenti, <c>null</c>.</returns>
    public LabelLocation? FindLabel(string key) => _labels.GetValueOrDefault(key);

    /// <summary>
    /// Ottiene tutte le label registrate (per debug/diagnostica e fallback lookup).
    /// </summary>
    public IEnumerable<KeyValuePair<string, LabelLocation>> GetAllLabelsDebug() =>
        _labels.AsEnumerable();

    /// <summary>
    /// Genera una chiave qualificata e univoca per un'etichetta, usata come chiave nella mappa globale delle etichette.
    /// La chiave include il nome del file e, se applicabile, la firma della procedura contenitrice
    /// per garantire l'unicità e rispettare le regole di scope delle etichette.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Centralizzare l'algoritmo di "mangling" dei nomi delle etichette.
    /// Poiché le etichette possono avere lo stesso nome in procedure diverse o a livello di modulo,
    /// è necessario un modo per renderle univoche a livello di programma. Questa funzione implementa
    /// tale logica in un unico punto, garantendo coerenza tra il momento in cui le etichette vengono
    /// registrate (`AddOrUpdateLabel`) e quello in cui vengono cercate (`FindLabel`).
    /// 
    /// Formato della Chiave:
    /// - Etichetta di Modulo: `NOMEFILE:NOMEETICHETTA` (es. `MAIN.BAS:LOOP_PRINCIPALE`)
    /// - Etichetta di Procedura: `NOMEFILE:FIRMAPROCEDURA:NOMEETICHETTA` (es. `UTILS.BAS:SOMMA(INTEGER,INTEGER):ERRORE_INPUT`)
    /// </remarks>
    /// <param name="fileName">Il nome del file sorgente (es. "MAIN.BAS") da includere nella chiave.</param>
    /// <param name="procedureContext">La procedura contenitrice, se l'etichetta è locale a una procedura; altrimenti, <c>null</c>.</param>
    /// <param name="labelName">Il nome dell'etichetta come appare nel codice sorgente.</param>
    /// <returns>Una stringa che rappresenta la chiave univoca per l'etichetta.</returns>
    public static string GetQualifiedLabelName(string fileName, ProcedureDefinition? procedureContext, string labelName)
    {
        return procedureContext != null
            ? $"{fileName}:{procedureContext.Signature}:{labelName}".ToUpperInvariant()
            : $"{fileName}:{labelName}".ToUpperInvariant();
    }

    // ========================================================================
    // --- METODI DI DUMP (Riscritto per la nuova architettura) ---
    // ========================================================================

    /// <summary>
    /// Stampa sulla console una rappresentazione testuale, gerarchica e dettagliata dell'intera tabella dei simboli.
    /// Questo metodo è uno strumento di diagnostica fondamentale per il debug del compilatore.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Fornire una vista "human-readable" dello stato interno della `SymbolTable`
    /// in un dato momento del processo di compilazione. Permette di ispezionare come gli scope sono
    /// annidati e quali simboli sono stati registrati in ciascuno di essi, inclusi i loro ID, tipi e flag.
    /// 
    /// Logica Interna: Utilizza il metodo helper ricorsivo `DumpScopeRecursive` per navigare l'albero
    /// degli scope a partire dalla radice (ID 0) e stampare il contenuto di ogni scope in modo indentato.
    /// </remarks>
    /// <param name="scopeFilter">Filtro opzionale per visualizzare solo gli scope i cui nomi corrispondono (case-insensitive, supporta '*' alla fine).</param>
    /// <param name="symbolNameFilter">Filtro opzionale per visualizzare solo i simboli il cui nome contiene questa stringa (case-insensitive).</param>
    /// <param name="flagsFilter">Filtro opzionale per visualizzare solo i simboli che possiedono tutti i flag specificati.</param>
    /// <param name="userCodeOnly">Se <c>true</c>, esclude dall'output tutti i namespace built-in della libreria standard (es. SYSTEM, IO, etc.).</param>
    public void Dump(string? scopeFilter = null, string? symbolNameFilter = null, SymbolFlags? flagsFilter = null,
        bool userCodeOnly = false)
    {
        Console.WriteLine("\n======================================================================");
        Console.WriteLine("=== SYMBOL TABLE DUMP (ID-based) ===");
        Console.WriteLine("======================================================================");

        DumpScopeRecursive(0, "", scopeFilter, symbolNameFilter, flagsFilter, userCodeOnly);

        Console.WriteLine("======================================================================");
    }

    /// <summary>
    /// Metodo helper ricorsivo che naviga l'albero degli scope e stampa i simboli,
    /// applicando i filtri specificati. È il motore di rendering per il metodo pubblico <see cref="Dump"/>.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Implementare la logica di attraversamento dell'albero degli scope
    /// e di formattazione dell'output gerarchico. Questo metodo è privato per nascondere i
    /// dettagli implementativi (come la gestione dell'ID e dell'indentazione) dall'API pubblica.
    /// 
    /// Logica Interna:
    /// 1. Applica i filtri allo scope corrente. Se lo scope corrisponde, ne stampa le informazioni.
    /// 2. Itera sui simboli dello scope corrente, applica i filtri rimanenti e stampa i simboli che corrispondono.
    /// 3. Chiama se stesso ricorsivamente per ognuno dei `ChildrenIds` dello scope corrente,
    ///    incrementando il livello di indentazione.
    /// </remarks>
    /// <param name="scopeId">L'ID dello scope da cui iniziare a stampare.</param>
    /// <param name="indent">La stringa di indentazione per la formattazione gerarchica.</param>
    /// <param name="scopeFilter">Filtro opzionale per visualizzare solo gli scope i cui nomi corrispondono.</param>
    /// <param name="symbolNameFilter">Filtro opzionale per visualizzare solo i simboli il cui nome contiene questa stringa.</param>
    /// <param name="flagsFilter">Filtro opzionale per visualizzare solo i simboli che possiedono i flag specificati.</param>
    /// <param name="userCodeOnly">Se true, esclude dall'output i namespace built-in della libreria standard.</param>
    private void DumpScopeRecursive(int scopeId, string indent, string? scopeFilter, string? symbolNameFilter,
        SymbolFlags? flagsFilter, bool userCodeOnly)
    {
        var scope = _scopes[scopeId];

        if (userCodeOnly && scope.Type == ScopeType.Namespace)
        {
            if (BuiltinNamespaceNames.Contains(scope.Name))
            {
                return;
            }
        }

        var scopeFilters = scopeFilter?.Split(',').Select(s => s.Trim()).ToList();
        var scopeNameMatches = scopeFilters == null || scopeFilters.Any(f =>
            scope.Name.Equals(f, StringComparison.OrdinalIgnoreCase) ||
            (f.EndsWith('*') && scope.Name.StartsWith(f.TrimEnd('*'), StringComparison.OrdinalIgnoreCase))
        );

        if (scopeNameMatches)
        {
            var parentName = scope.ParentId >= 0
                ? $" -> Parent: '{_scopes[scope.ParentId].Name}' (ID: {scope.ParentId})"
                : "";
            Console.WriteLine(
                $"{indent}SCOPE ID: {scope.Id,-3} Name: {scope.Name,-20} (Type: {scope.Type,-10}, OwnerID: {scope.OwnerSymbolId,-4}){parentName}");

            var symbolsInScope = scope.SymbolsByName.Values
                .SelectMany(idList => idList)
                .Select(id => _symbols[id])
                .Where(s => string.IsNullOrEmpty(symbolNameFilter) ||
                            s.OriginalName.Contains(symbolNameFilter, StringComparison.OrdinalIgnoreCase))
                .Where(s => flagsFilter == null || s.Has(flagsFilter.Value))
                .OrderBy(s => s.OriginalName);

            foreach (var symbol in symbolsInScope)
            {
                var nsName = GetNamespaceNameForSymbol(symbol.Id);
                Console.WriteLine($"{indent}  -> {symbol} (NS: {nsName})");
            }

            if (scope.SymbolsByName.Count == 0)
            {
                Console.WriteLine($"{indent}  (Nessun simbolo definito direttamente in questo scope)");
            }
        }

        foreach (var childId in scope.ChildrenIds.OrderBy(id => _scopes[id].Name))
        {
            DumpScopeRecursive(childId, indent + "  ", scopeFilter, symbolNameFilter, flagsFilter, userCodeOnly);
        }
    }

    /// <summary>
    /// Stampa una rappresentazione dettagliata della mappa delle etichette globale.
    /// Questo metodo è uno strumento di diagnostica per il debug del parser e del linker.
    /// </summary>
    public void DumpLabelMap()
    {
        Console.WriteLine("======================================================================");
        Console.WriteLine("=== LABEL MAP DUMP ===");
        Console.WriteLine("======================================================================");

        if (_labels.Count == 0)
        {
            Console.WriteLine("  (Mappa delle etichette vuota)");
        }
        else
        {
            foreach (var (key, location) in _labels.OrderBy(kvp => kvp.Key))
            {
                var context = location.Procedure != null
                    ? $"in proc '{location.Procedure.OriginalName.Lexeme}'"
                    : "at module level";
                Console.WriteLine(
                    $"  - Chiave: '{key}' -> Riga: {location.Line}, Indice: {location.StatementIndex} ({context})");
            }
        }

        Console.WriteLine("======================================================================");
    }
}