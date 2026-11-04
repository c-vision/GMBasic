// FILE: Interpreter.Parser.cs

using System.Collections.Immutable;

// ReSharper disable NotAccessedField.Local
// ReSharper disable ReturnValueOfPureMethodIsNotUsed
namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
       GUIDA DI RIFERIMENTO A GM-BASIC - REGOLE SINTATTICHE (PARSER)
------------------------------------------------------------------------------------------------------------------------

Il Parser riceve i token dal Lexer e li organizza in un Abstract Syntax Tree (AST), un modello gerarchico che rappresenta
la struttura logica del programma.

1. ARCHITETTURA DEL PARSER
===========================
- Tipo: Parser discendente ricorsivo.
- Processo di Compilazione: Il parser fa parte di un processo di compilazione a più fasi che trasforma un insieme di file
  sorgente in un programma eseguibile.
 1.  Scansione dei File: L'interprete identifica tutti i file sorgente (`.bas`) del progetto.
 2.  Parsing e Mappatura: Ogni file viene analizzato per costruire un AST e per mappare le sue definizioni (procedure,
     tipi, globali) ai namespace dichiarati.
 3.  Linking e Validazione: Tutte le definizioni vengono aggregate in una mappa globale dei namespace. Il linker valida
     le istruzioni `IMPORT`, risolve le dipendenze e rileva collisioni di nomi.

2. STRUTTURA DEL PROGRAMMA
===========================
- Entry Point: L'esecuzione inizia dalla `SUB Main()` o dalla prima riga numerata trovata nel file specificato sulla
               riga di comando.
- Definizioni Top-Level: Lo scope esterno di un file può contenere dichiarazioni `NAMESPACE`, `IMPORT`, `TYPE`,
                         `FUNCTION`, `SUB`, `GLOBAL`, `DIM`, o codice con numeri di riga.

3. AMBIGUITÀ E DISAMBIGUAZIONE
==============================
- `Nome(...)`: È l'ambiguità principale. Il Parser la risolve usando una pre-scansione dei nomi delle `FUNCTION` definite
               in ogni file. Se 'Nome' è una funzione conosciuta, genera un nodo `FunctionExpr`; altrimenti, genera un
               `ArrayAccessExpr`.
- `Identificatore ...`: All'inizio di un'istruzione, un identificatore può essere una chiamata a `SUB` o un'assegnazione
                        implicita (`LET`). Il Parser esegue un "lookahead": se dopo l'identificatore (e i suoi eventuali
                        indici) c'è un `=`, lo tratta come un'assegnazione; altrimenti, come una chiamata a `SUB`.

4. APPIATTIMENTO DEI CICLI (FLATTENING)
========================================
- Obiettivo: Semplificare l'esecuzione e la compilazione in P-CODE.
- Meccanismo: I costrutti di ciclo (`FOR`, `DO`, `WHILE`) vengono scomposti in una sequenza lineare di istruzioni
              atomiche (salti condizionali, etichette, etc.), rendendo l'esecuzione più granulare e interrompibile.

5. BLOCCHI STRUTTURATI (IF, SELECT)
=====================================
- A differenza dei cicli, i costrutti `IF...END IF` e `SELECT CASE` vengono parsati in un modo
 più tradizionale, creando un singolo nodo AST (`IfStmt`, `SelectStmt`) che contiene al suo interno i rami di esecuzione.

6. MODULI E NAMESPACE (SISTEMA DI COMPILAZIONE)
======================================================
GM-BASIC supera il limite dei programmi a singolo file introducendo un sistema di moduli e namespace, che permette
di organizzare il codice in librerie logiche e riutilizzabili.

6.1. Il Concetto
L'intero progetto non è più un singolo file, ma un insieme di file `.bas` che collaborano. Il **namespace** è il meccanismo
per raggruppare logicamente codice correlato, anche se si trova in file diversi. Il compilatore analizza l'intero progetto
per costruire una mappa di questi namespace prima di eseguire il codice.

6.2. Dichiarazione di un Namespace (`namespace`)
La keyword `namespace` assegna tutto il codice che la segue a un gruppo logico specifico.

- Sintassi: `namespace <NomeNamespace>`
- Regole:
 - Un file può contenere più dichiarazioni di namespace. Ogni dichiarazione cambia il namespace attivo per tutte le
   definizioni successive.
 - Le definizioni (SUB, FUNCTION, TYPE, GLOBAL) appartengono al namespace attivo al momento della loro dichiarazione.
 - Il codice non preceduto da una dichiarazione `namespace` appartiene a un **namespace globale di default** chiamato
   `_global`.

Esempio (`matematica.bas`):

NAMESPACE Matematica

' Questa funzione appartiene al namespace "Matematica"
PUBLIC FUNCTION Somma(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
 FUNCTION = a + b
END FUNCTION

' Anche questa SUB appartiene a "Matematica"
SUB PrivataHelper()
 ' ...
END SUB

NAMESPACE Grafica

' Questa SUB appartiene al namespace "Grafica"
PUBLIC SUB DisegnaCerchio()
 ' ...
END SUB

6.3. Utilizzo di un Namespace (IMPORT)
Per utilizzare le definizioni di un namespace in un altro file, è necessario importarlo.

Sintassi: IMPORT <NomeNamespace> [AS <Alias>]

Funzionamento: IMPORT rende visibili i simboli PUBLIC di un altro namespace. Importa un nome logico, non un percorso di file.
Alias: Se AS <Alias> viene omesso, il nome del namespace stesso viene usato come alias.

Esempio (main.bas):

' Importa il namespace "Matematica" e gli assegna l'alias "Math"
IMPORT Matematica AS Math

SUB Main()
 DIM risultato AS DOUBLE
 risultato = Math.Somma(10, 5) ' Accesso qualificato tramite alias
 PRINT risultato
END SUB

6.4. Discovery dei File e Costruzione dei Namespace
L'interprete scopre e assembla i namespace automaticamente attraverso un processo a 3 fasi:

Scansione: Quando si esegue MG-BASIC main.bas, l'interprete identifica la directory di main.bas come la "radice del progetto".
           Scansiona ricorsivamente questa directory e tutte le sue sottodirectory per trovare tutti i file .bas disponibili.
Parsing e Mappatura: Il compilatore legge e parsa ogni file .bas trovato. Durante il parsing, costruisce una mappa interna
                     di tutti i namespace e associa a ciascuno le definizioni (SUB, FUNCTION, etc.) trovate nei vari file.
Linking e Validazione: Una volta che tutti i file sono stati parsati, il linker valida le istruzioni IMPORT (verificando
                       che i namespace richiesti esistano) e controlla la presenza di collisioni (es. due PUBLIC FUNCTION
                       Somma() con la stessa firma nello stesso namespace).

6.5. Accesso ai Simboli Importati
Sintassi: <Alias>.<NomeSimbolo>

Regola di Visibilità: Solo i simboli (procedure e tipi) dichiarati come PUBLIC in un namespace sono accessibili dall'esterno.
                      Le variabili globali (DIM/GLOBAL a livello di modulo) sono considerate implicitamente pubbliche
                      all'interno del loro namespace.

6.6. Esempio Completo Multi-File
Struttura del Progetto:

/progetto/
 |-- main.bas
 |-- utils.bas

Codice (utils.bas):

NAMESPACE Utilities

PUBLIC CONST PI = 3.14159

PUBLIC FUNCTION PadString(s AS STRING, lunghezza AS INTEGER) AS STRING
 ' ... logica ...
END FUNCTION

' Questa SUB non è PUBLIC, quindi non sarà visibile dall'esterno
SUB InternalLog(msg AS STRING)
 PRINT "LOG: "; msg
END SUB

Codice (main.bas):

' Importa il namespace definito in utils.bas
IMPORT Utilities

SUB Main()
 ' Accede a una costante pubblica tramite il nome del namespace
 PRINT "Pi greco è circa "; Utilities.PI

 ' Chiama una funzione pubblica tramite il nome del namespace
 DIM testo AS STRING
 testo = Utilities.PadString("Ciao", 10)
 PRINT "'"; testo; "'"

 ' Il seguente codice causerebbe un errore di compilazione
 ' perché InternalLog non è stata dichiarata PUBLIC
 ' Utilities.InternalLog("Messaggio segreto")
END SUB

6.7. Ciclo di Vita della Compilazione (Riassunto)

Esecuzione: L'utente lancia MG-BASIC main.bas.
Scansione: Il compilatore trova main.bas e utils.bas.
Parsing/Mappatura: Legge entrambi i file. Scopre che utils.bas definisce i simboli PI e PadString nel namespace Utilities.
                   main.bas definisce Main nel namespace di default _global.
Linking: Analizza main.bas, vede IMPORT Utilities. Verifica che il namespace Utilities esista nella mappa creata al passo 3.
         La validazione ha successo.
Esecuzione: L'interprete esegue SUB Main. Quando incontra Utilities.PadString, sa di dover cercare la funzione PadString
           nel BasicProcedures del namespace Utilities.
*/
/// <summary>
///     The parser for the BASIC language, responsible for transforming a sequence of tokens
///     into an Abstract Syntax Tree (AST).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Il testo completo del codice sorgente del modulo corrente.
    /// </summary>
    private readonly string _sourceText;

    /// <summary>
    /// Una mappa che associa ogni indice di riga alle informazioni complete sulla sua origine (file, numero di riga originale, testo).
    /// </summary>
    private readonly ImmutableArray<SourceLine> _sourceMap;

    /// <summary>
    /// L'indice corrente nella `_sourceMap`, usato per marcare i nodi AST con la loro riga di origine.
    /// </summary>
    private int _currentSourceLineIndex = -1;

    /// <summary>
    /// La sequenza immutabile di token generata dal Lexer per il modulo corrente.
    /// </summary>
    private readonly ImmutableArray<Token> _tokens;

    /// <summary>
    /// L'indice del token corrente nel flusso `_tokens`. Funge da puntatore di lettura per il parser.
    /// </summary>
    private int _current;

    /// <summary>
    /// Memorizza l'ultimo token `LineNumber` incontrato, per contestualizzare gli errori.
    /// </summary>
    private Token? _currentLineNumberToken;

    /// <summary>
    /// True mentre il parser è dentro il corpo di un blocco multilinea (IF/ELSEIF/ELSE/END IF,
    /// DO/LOOP, FOR/NEXT...). Disabilita la "forma mista" dell'IF inline: un IF inline DENTRO un
    /// blocco è SEMPRE completo sulla sua riga — l'END IF che segue appartiene al blocco esterno.
    /// (La forma mista "IF c THEN stmt" + ELSE/END IF su riga separata è valida solo a top-level.)
    /// </summary>
    private bool _insideMultiLineBlock;

    /// <summary>Modalità schermo: true = grafica/pixel (SCREEN n!=0), false = testo/TUI (SCREEN 0 o default).
    /// Determina il renderer dei controlli nel namespace CONTROLS (pipeline unica C#/C).</summary>
    public bool GuiMode { get; internal set; }

    /// <summary>
    /// Memorizza l'ultimo numero di riga parsato, per gestire correttamente le linee di continuazione (`:`).
    /// </summary>

    /// <summary>
    /// Attributes in attesa di essere associati al prossimo statement (DIM, FUNCTION, CLASS, ecc.).
    /// Viene popolato da ParseStatement quando trova &lt;AttributeName&gt;.
    /// </summary>
    private ImmutableArray<Token> _pendingAttributes = ImmutableArray<Token>.Empty;
    private int? _lastParsedLineNumber;

    /// <summary>
    /// Il percorso completo del file di ingresso del programma o del modulo corrente.
    /// </summary>
    private readonly string? _entryFilePath;

    /// <summary>
    /// Uno stack che tiene traccia del contesto delle procedure in cui il parser si trova attualmente.
    /// Viene usato per distinguere tra lo scope di modulo e lo scope di procedura.
    /// Quando lo stack non è vuoto, significa che il parser sta analizzando il corpo di una `FUNCTION`,
    /// `SUB`, `GENERATOR`, o un membro procedurale di una `CLASS` o `TYPE`.
    /// Questo è cruciale per la logica di registrazione dei simboli (es. per distinguere
    /// `DIM` a livello di modulo da `DIM` locale) e per validare la sintassi
    /// (es. `STATIC` è permesso solo all'interno di una procedura).
    /// </summary>
    private readonly Stack<Token> _functionContextStack = new();
    // Parallelo a _functionContextStack: true se la procedura corrente RESTITUISCE un valore
    // (FUNCTION/GENERATOR/method non-void). Serve a NON trattare `FUNCTION = expr` come
    // assegnazione al valore di ritorno dentro una SUB (dove `FUNCTION` è una VARIABILE,
    // parità col parser C — Bugs/(open) - Parser-STEP-...).
    private readonly Stack<bool> _functionContextReturnsValue = new();

    /// <summary>
    /// Mantiene un riferimento all'ultimo token consumato dal metodo `Advance()`.
    /// Questo campo sarà `null` prima che almeno un token sia stato consumato.
    /// Insieme a `_twoTokensBack`, forma una finestra di lookbehind di dimensione 2,
    /// essenziale per il parsing contestuale.
    /// </summary>
    /// <remarks>
    /// Esempio di utilizzo: Riconoscere un'etichetta di ciclo (`ETICHETTA: FOR...`).
    /// Quando il parser del `FOR` viene invocato, il token corrente è `FOR`.
    /// `_previousToken` conterrà il token `:`, permettendo al parser di
    /// ispezionare `_twoTokensBack` (dopo aver controllato che non sia `null`)
    /// per verificare se era un `Identifier`, confermando così la presenza di
    /// un'etichetta associata al ciclo.
    /// </remarks>
    private Token? _previousToken;

    /// <summary>
    /// Mantiene un riferimento al token consumato prima di `_previousToken`.
    /// Questo campo sarà `null` prima che almeno due token siano stati consumati.
    /// Insieme a `_previousToken`, forma una cronologia degli ultimi due token
    /// processati.
    /// </summary>
    /// <remarks>
    /// Questo token è la chiave per implementare in modo pulito il riconoscimento
    /// di costrutti complessi senza alterare la logica di dispatching principale.
    /// Qualsiasi codice che accede a questo campo deve prima verificare la sua nullabilità,
    /// poiché i primi due token del flusso non avranno una cronologia completa.
    /// </remarks>
    private Token? _twoTokensBack;

    /// <summary>
    /// Traccia il nome del namespace attivo corrente durante il parsing di un modulo.
    /// Viene impostato dalla direttiva `NAMESPACE` e letto dai metodi di parsing
    /// delle dichiarazioni (`ParseTypeDefinition`, `ParseFunctionDefinition`, etc.)
    /// per annotare i nodi AST con il loro namespace di appartenenza.
    /// Viene resettato a `null` all'inizio di `ParseModuleSource`.
    /// </summary>
    private string? _currentNamespace;
    /// <summary>Nome della libreria dichiarata con `LIBRARY "nome"` (null = eseguibile).</summary>
    private string? _libraryName;
    
    /// <summary>
    /// Nomi delle variabili dichiarate finora (DIM/AS) — serve a disambiguare un nome con
    /// suffisso $ (es. LIST$) dalla funzione builtin senza $ (es. PROVIDER.LIST): al parse
    /// le variabili non sono ancora nella SymbolTable (SymbolId -1), quindi il lookup dei
    /// simboli non è affidabile (bug StringsExamples.bas).
    /// </summary>
    private readonly HashSet<string> _declaredVariableNames = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Sottoinsieme di <see cref="_declaredVariableNames"/>: nomi dichiarati come ARRAY
    /// (es. DIM amax()) — serve a dare alla variabile-array la priorità sulla funzione
    /// builtin omonima in forma nuda (es. amax(0) è accesso all'elemento, non chiamata ad
    /// AMAX): senza, una variabile omonima di un builtin (amax/AMAX, argmax/ARGMAX...)
    /// rende illeggibile l'array (bug MATRIX.ARGMAX/ARGMIN). Solo gli array: uno scalare
    /// omonimo non devia le chiamate al builtin (comportamento invariato).
    /// Stesso limite noto del set principale: flat sul modulo, non per-scope.
    /// </summary>
    private readonly HashSet<string> _declaredArrayNames = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Ultima dichiarazione vista per nome: true se è uno SCALARE "sicuro" (tipo primitivo
    /// esplicito, oppure inizializzatore chiaramente scalare come un numero/stringa/bool).
    /// Serve a non far deviare le chiamate al builtin per uno scalare omonimo
    /// (`DIM amax = 5` + `AMAX(v)`), mantenendo lo shadowing per nomi non tipizzati che
    /// possono contenere array (`DIM voci` riempita da STRINGS.SPLIT). Bug AMAX/ARGMIN
    /// shadow su VM-C: prima ogni variabile dichiarata shadowava il builtin.
    /// </summary>
    private readonly Dictionary<string, bool> _declaredVariableIsScalar = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Variabili dichiarate AS JSON (o AS DECISION, alias di JSON): `AI DECIDE ... INTO d`
    /// va avvolto in JSON.PARSE, così `d.VALUE/.PROBABILITY/.CONFIDENCE` funzionano davvero
    /// (senza, IL/PCode conservavano la stringa JSON e il membro tornava vuoto).</summary>
    private readonly HashSet<string> _declaredJsonNames = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>
    /// Nomi delle procedure (FUNCTION/SUB/GENERATOR e metodi di classe) definite in questo
    /// file fino a questo punto del parsing. Serve a dare loro la precedenza sui builtin
    /// quando il nome coincide con una keyword: `FUNCTION Sum()` rende `Sum(...)` una chiamata
    /// UTENTE, non `MATH.Sum(...)` (prima il dispatch per TokenKind del parser vinceva sempre,
    /// bug types/class_array_member: `Sum = arr(0) + ...` veniva letto come target di una
    /// chiamata a funzione). Flat sul file, non per-scope: stesso limite noto degli altri set.
    /// </summary>
    private readonly HashSet<string> _userProcedureNames = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Registra un nome di procedura utente (con e senza suffisso `$`, perché il
    /// lookup dei builtin normalizza `Nome$` → `Nome`).</summary>
    internal void RegisterUserProcedureName(string lexeme)
    {
        if (string.IsNullOrEmpty(lexeme)) return;
        _userProcedureNames.Add(lexeme);
        if (lexeme.EndsWith('$') && lexeme.Length > 1) _userProcedureNames.Add(lexeme[..^1]);
    }

    /// <summary>
    /// Il programma in fase di compilazione, che funge da contesto centrale.
    /// </summary>
    private readonly BasicProgram _program;

    /// <summary>
    /// Registry per l'accesso ai nomi di funzioni/comandi built-in.
    /// </summary>
    private readonly IParserRegistry _registry;

    /// <summary>
    /// Inizializza un parser per un flusso di token già esistente.
    /// </summary>
    private Parser(ImmutableArray<Token> tokens)
    {
        _tokens = tokens;
        _sourceText = string.Empty;
        _sourceMap = ImmutableArray<SourceLine>.Empty;
        _current = 0;
        // Crea un'istanza di BasicProgram temporanea e isolata.
        var effectiveSettings = new CommonLib.EffectiveSettings();
        var programRootDirectory = Directory.GetCurrentDirectory();
        _program = new BasicProgram(effectiveSettings, null, programRootDirectory);
        _registry = null!;
    }

    /// <summary>
    /// Inizializza un parser per un flusso di token già esistente, utilizzando un
    /// programma come contesto. Usato internamente dal ContextBuilder per le scansioni dei file
    /// e per il parsing completo dei moduli pre-elaborati.
    /// </summary>
    public Parser(ImmutableArray<Token> tokens, ImmutableArray<SourceLine> sourceMap, BasicProgram program,
        string filePath, IParserRegistry? registry = null)
    {
        _tokens = tokens;
        _sourceText = string.Empty;
        _sourceMap = sourceMap;
        _current = 0;
        _program = program;
        _entryFilePath = filePath;
        _registry = registry!;
    }

    /// <summary>
    /// Esegue una scansione leggera sul flusso di token per estrarre tutte le direttive REQUIRE.
    /// Questo metodo è statico e riutilizza la logica di parsing a basso livello.
    /// </summary>
    /// <param name="tokens">La sequenza di token pre-elaborati.</param>
    /// <param name="sourceMap">La mappa del sorgente per il reporting degli errori.</param>
    /// <param name="program">Il contesto del programma.</param>
    /// <param name="filePath">Il percorso del file sorgente.</param>
    /// <returns>La lista di istruzioni RequireStmt trovate.</returns>
    public static List<RequireStmt> ScanRequirementsFromTokens(
        ImmutableArray<Token> tokens,
        ImmutableArray<SourceLine> sourceMap,
        BasicProgram program,
        string filePath,
        IParserRegistry? registry = null)
    {
        var requirements = new List<RequireStmt>();

        var scanner = new Parser(tokens, sourceMap, program, filePath, registry);

        // REQUIRE è una direttiva di modulo SOLO a inizio statement (inizio file o dopo
        // Newline/Colon): dentro un comando (es. `AI VERIFY ... REQUIRE <expr>`, direttiva
        // sintassi 2026-09-09) il token REQUIRE è una clausola e NON va importato.
        var atStatementStart = true;
        while (!scanner.IsAtEnd())
        {
            var current = scanner.Peek();
            if (atStatementStart && current.Kind == TokenKind.Require)
            {
                requirements.Add(scanner.ParseRequireStatement());
                // ParseRequireStatement consuma la direttiva; eventuali Newline separatori
                // lasciano atStatementStart già vero all'iterazione successiva.
                continue;
            }

            scanner.Advance();
            if (current.Kind == TokenKind.Newline || current.Kind == TokenKind.Colon)
                atStatementStart = true;
            else if (atStatementStart)
                atStatementStart = false;
        }

        return requirements;
    }

    /// <summary>
    /// Determina se un TokenKind rappresenta una keyword "strutturale" che non può
    /// mai essere usata come nome di variabile, parametro o procedura.
    /// </summary>
    /// <param name="kind">Il TokenKind da controllare.</param>
    /// <returns>True se la keyword è strutturale e riservata; altrimenti, false.</returns>
    public static bool IsStructuralKeyword(TokenKind kind)
    {
        return kind switch
        {
            // Tipi e dichiarazioni: parole che definiscono la struttura ma non sono comandi eseguibili
            TokenKind.As or TokenKind.Dim or TokenKind.Global or TokenKind.Local or TokenKind.Static or
                TokenKind.Const or TokenKind.Declare or
                TokenKind.Function or TokenKind.Generator or TokenKind.Type or TokenKind.Class or
                TokenKind.EndFunction or TokenKind.EndGenerator or TokenKind.EndType or TokenKind.EndClass => true,

            TokenKind.Interface or TokenKind.EndInterface or TokenKind.Implements => true,
            
            // Delimitatori di blocco: mai usabili come identificatori
            TokenKind.EndEnum or TokenKind.EndGet or TokenKind.EndMatch or TokenKind.EndOperator or
                TokenKind.EndProperty or TokenKind.EndSet or TokenKind.EndSpritedef or
                TokenKind.EndTick or TokenKind.EndUsing or TokenKind.EndWith => true,
            
            // Strutture di controllo di flusso
            TokenKind.If or TokenKind.Then or TokenKind.Else or TokenKind.ElseIf or TokenKind.EndIf or
                TokenKind.For or TokenKind.To or TokenKind.Step or // NEXT viene rimosso
                TokenKind.Do or TokenKind.Loop or TokenKind.While or TokenKind.Wend or TokenKind.Until or
                TokenKind.Case or TokenKind.EndSelect or // SELECT viene rimosso
                TokenKind.Try or TokenKind.Catch or TokenKind.Finally or TokenKind.EndTry => true,

            // Comandi di Salto incondizionati
            TokenKind.Goto or TokenKind.Gosub or TokenKind.Return or TokenKind.End => true,

            // Parole chiave che possono far parte di comandi più grandi ma sono intrinsecamente strutturali
            TokenKind.On => true,

            _ => false
        };
    }

    /// <summary>
    /// Nome valido in senso PERMISSIVO (parità col parser C, che accetta ogni token come nome —
    /// `gmb_p_term_from_token` → VAR; vedi Bugs/(open) - Parser-STEP-Variabile-Accettata-C-
    /// Rifiutata-CSharp): Identifier, keyword non-strutturale O keyword STRUTTURALE.
    /// Usare SOLO nei siti "nome di variabile/parametro/procedura/membro", MAI dove la keyword
    /// è il discriminante di uno statement (lì serve IsStructuralKeyword/il dispatch).
    /// </summary>
    private static bool IsPermissiveNameToken(Token token)
        => IsValidNameToken(token) || IsStructuralKeyword(token.Kind);

    /// <summary>
    /// True se il token può CONTINUARE un'espressione (operatore binario, accesso, separatore
    /// o terminatore di statement). Serve a distinguere una keyword-VARIABILE in posizione di
    /// operando (`PRINT step + x`) da una keyword che inizia un nuovo statement
    /// (`PRINT "x" IF …`). Parità col parser C (keyword come nomi).
    /// </summary>
    private static bool CanContinueExpression(TokenKind k) => k switch
    {
        TokenKind.Plus or TokenKind.Minus or TokenKind.Multiply or TokenKind.Divide
            or TokenKind.IntegerDivide or TokenKind.Power or TokenKind.Mod or TokenKind.Ampersand
            or TokenKind.Equal or TokenKind.NotEqual or TokenKind.LessThan or TokenKind.LessEqual
            or TokenKind.GreaterThan or TokenKind.GreaterEqual
            or TokenKind.And or TokenKind.Or or TokenKind.Xor or TokenKind.Is or TokenKind.In
            or TokenKind.Dot or TokenKind.LeftParen or TokenKind.LeftBracket
            or TokenKind.RightParen or TokenKind.RightBracket or TokenKind.RightBrace
            or TokenKind.Semicolon or TokenKind.Comma or TokenKind.Colon
            or TokenKind.Newline or TokenKind.Eof
            or TokenKind.Else or TokenKind.ElseIf or TokenKind.EndIf or TokenKind.Case or TokenKind.EndSelect
            => true,
        _ => false
    };

    /// <summary>
    /// Verifica se un token può essere considerato un nome valido (Identifier o keyword non strutturale).
    /// Esclude keyword strutturali e delimitatori/operatori.
    /// </summary>
    private static bool IsValidNameToken(Token token)
    {
        // Condizione 1: Non può essere una keyword strutturale (IF, FOR, etc.)
        if (IsStructuralKeyword(token.Kind))
        {
            return false;
        }

        // Condizione 2: Non può essere un delimitatore o un operatore che non può essere anche un nome.
        // Multiply, Add, etc., sono ammessi perché possono essere nomi di funzione.
        return token.Kind switch
        {
            TokenKind.LeftParen or TokenKind.RightParen or
                TokenKind.LeftBracket or TokenKind.RightBracket or
                TokenKind.LeftBrace or TokenKind.RightBrace or
                TokenKind.Comma or TokenKind.Semicolon or TokenKind.Colon or TokenKind.DictColon or
                TokenKind.Dot or TokenKind.Equal or TokenKind.Arrow or TokenKind.FatArrow or
                TokenKind.Plus or TokenKind.Minus or TokenKind.Divide or
                TokenKind.IntegerDivide or TokenKind.Power or TokenKind.Hash or
                TokenKind.Ampersand or
                TokenKind.NotEqual or TokenKind.LessThan or TokenKind.LessEqual or
                TokenKind.GreaterThan or TokenKind.GreaterEqual => false,

            // Tutti gli altri (Identifier, keyword non-strutturali come GET, SELECT, PRINT, MULTIPLY etc.) sono validi.
            _ => true
        };
    }

    /// <summary>
    /// Consuma il token corrente se è un identificatore valido per un nome di procedura,
    /// utilizzando l'helper IsValidNameToken.
    /// </summary>
    private Token ConsumeProcedureName()
    {
        var token = Peek();
        if (IsPermissiveNameToken(token))
        {
            return Advance();
        }

        throw CreateError(token,
            $"'{token.Lexeme}' è una keyword riservata o un token non valido e non può essere usata come nome di procedura.");
    }

    /// <summary>
    /// Consuma il token corrente se è un nome valido (Identifier o keyword non-strutturale
    /// contestuale come ID/VIA/OFFSET). Usato per nomi di membri/proprietà/type fields.
    /// </summary>
    private Token ConsumeNameToken(string errorMessage)
    {
        var token = Peek();
        if (IsPermissiveNameToken(token))
        {
            return Advance();
        }

        throw CreateError(token, errorMessage);
    }

    /// <summary>
    /// Controlla se il token corrente corrisponde al tipo atteso. Se sì, lo consuma e lo restituisce.
    /// Altrimenti, lancia un'eccezione di errore di sintassi.
    /// </summary>
    private Token Consume(TokenKind kind, string message)
    {
        return Check(kind) ? Advance() : throw CreateError(Peek(), message);
    }

    /// <summary>
    /// Controlla se il token corrente corrisponde a uno dei due tipi attesi. Se sì, lo consuma e lo restituisce.
    /// Altrimenti, lancia un'eccezione di errore di sintassi.
    /// </summary>
    private Token Consume(TokenKind kind1, TokenKind kind2)
    {
        if (Check(kind1) || Check(kind2)) return Advance();
        throw CreateError(Peek(), $"Expected {kind1} or {kind2} but found {Peek().Kind}.");
    }

    /// <summary>
    /// Consuma il token corrente se corrisponde a uno qualsiasi dei tipi specificati, altrimenti lancia errore.
    /// </summary>
    private Token Consume(string message, params TokenKind[] kinds)
    {
        foreach (var kind in kinds)
        {
            if (Check(kind)) return Advance();
        }
        throw CreateError(Peek(), message);
    }

    /// <summary>
    /// Controlla se il token corrente corrisponde a uno qualsiasi dei tipi specificati.
    /// Se trova una corrispondenza, consuma il token e restituisce `true`.
    /// </summary>
    private bool Match(params TokenKind[] kinds)
    {
        // ReSharper disable once LoopCanBeConvertedToQuery
        foreach (var kind in kinds)
        {
            if (!Check(kind)) continue;
            Advance();
            return true;
        }

        return false;
    }

    /// <summary>
    /// Controlla se il token corrente è del tipo specificato, senza consumarlo.
    /// </summary>
    private bool Check(TokenKind kind)
    {
        return !IsAtEnd() && Peek().Kind == kind;
    }

    /// <summary>
    /// Consuma il token corrente, avanza il puntatore del parser al token successivo,
    /// e aggiorna la finestra di lookbehind (`_previousToken`, `_twoTokensBack`).
    /// </summary>
    /// <returns>Il token che è stato appena consumato.</returns>
    /// <summary>
    /// True se il token può essere il NOME di un namespace/modulo: una parola (lettera/_ iniziale,
    /// poi lettere/cifre/_). Il nome è la GRAFIA, quindi una keyword con TokenKind dedicato
    /// (es. `DASHBOARD`) resta un nome valido per `NAMESPACE`/`IMPORT`.
    /// </summary>
    private static bool IsModuleName(Token t)
    {
        var s = t.Lexeme;
        if (string.IsNullOrEmpty(s)) return false;
        if (!char.IsLetter(s[0]) && s[0] != '_') return false;
        foreach (var c in s)
            if (!char.IsLetterOrDigit(c) && c != '_') return false;
        return true;
    }

    private Token Advance()
    {
        if (IsAtEnd())
            return _previousToken ??
                   throw new InvalidOperationException("Advance called on an empty or exhausted token stream.");
        // Aggiorna la cronologia prima di avanzare il puntatore
        _twoTokensBack = _previousToken;
        _previousToken = Peek(); // Il token corrente sta per diventare il precedente
        _current++;

        // Se IsAtEnd() è true, _previousToken manterrà il suo ultimo valore valido,
        // che è il comportamento corretto.

        // Restituisce l'ultimo token consumato, che ora è in _previousToken.
        // Aggiungiamo un controllo di nullità per sicurezza, anche se in un flusso
        // normale non dovrebbe mai essere nullo dopo la prima chiamata.
        return _previousToken ??
               throw new InvalidOperationException("Advance called on an empty or exhausted token stream.");
    }

    /// <summary>
    /// Verifica se il parser ha raggiunto la fine del flusso di token.
    /// </summary>
    private bool IsAtEnd()
    {
        return Peek().Kind == TokenKind.Eof;
    }

    /// <summary>
    /// Metodo helper per verificare se siamo alla fine di un'istruzione logica.
    /// </summary>
    private bool IsAtEndOfStatement()
    {
        return Check(TokenKind.Newline) || Check(TokenKind.Colon) || IsAtEnd();
    }
    
    /// <summary>
    /// Restituisce il token corrente senza consumarlo (lookahead di 0).
    /// </summary>
    public Token Peek()
    {
        return _tokens[_current];
    }

    /// <summary>
    /// Restituisce un token a un dato scostamento rispetto alla posizione corrente, senza consumarlo (lookahead).
    /// </summary>
    private Token At(int offset)
    {
        var index = _current + offset;
        return index >= _tokens.Length ? _tokens[^1] : _tokens[index];
    }

    /// <summary>
    /// Restituisce il token più recentemente consumato.
    /// </summary>
    private Token Previous()
    {
        return _tokens[_current - 1];
    }

    /// <summary>
    /// Consuma tutti i token Newline consecutivi dalla posizione corrente.
    /// </summary>
    private static void SkipNewlines(Parser parser)
    {
        while (parser.Match(TokenKind.Newline))
        {
        }
    }
    
    /// <summary>
    /// Aggiorna l'indice della riga di codice sorgente corrente in base alla posizione del token attuale.
    /// </summary>
    private void SynchronizeSourceLine()
    {
        var currentTokenLine = Peek().Line;
        if (currentTokenLine - 1 < _sourceMap.Length)
        {
            _currentSourceLineIndex = currentTokenLine - 1;
        }
    }
    
}