// FILE: Interpreter.Parser.Dispatchers.cs

namespace Basic;

/// <summary>
/// Contiene i metodi "dispatcher" per la Fase 1 del parsing.
/// Questi metodi sono responsabili del lookahead e dell'invocazione
/// del parser corretto per i comandi polimorfici, assicurando
/// che vengano chiamate solo le implementazioni pure.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Helper per distinguere `SELECT CASE` da altri comandi `SELECT` (es. DBF.SELECT),
    /// chiamando il parser corretto di Fase 1.
    /// </summary>
    /// <returns>Il nodo AST per `SELECT CASE` o null per il fallback.</returns>
    private static List<Stmt>? DispatchSelectStatement(Parser parser, ProcedureDefinition? procedureContext)
    {
        // La logica di lookahead è puramente sintattica e rimane invariata.
        if (parser.At(1).Kind == TokenKind.Case)
        {
            // Invochiamo la versione di Fase 1 del parser di SELECT CASE.
            return [parser.ParseSelectCaseStatement(procedureContext)];
        }

        // Restituisce null per indicare al chiamante (DispatchRegisteredCommand)
        // che questo parser non ha gestito il token e che deve continuare a cercare
        // altre registrazioni per il comando SELECT (es. in DBF o ISAM).
        return null;
    }

    /// <summary>
    /// Helper per distinguere `USE NEXT` (HTTP) da altri comandi `USE` (es. DBF.USE).
    /// Già implementato con logica di lookahead corretta.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <returns>Il nodo AST per `USE NEXT` o null per il fallback.</returns>
    public static List<Stmt>? DispatchUseStatement(Parser parser)
    {
        if (parser.At(1).Kind != TokenKind.Next) return null; // Indica al chiamante di procedere con il fallback.
        // Consuma il token 'USE' prima di chiamare il parser specifico.
        var useToken = parser.Consume(TokenKind.Use, "Internal error");
        return [ParseHttpUseNextStatement(parser, "HTTP")];
    }
    
    /// <summary>
    /// Agisce come router per il comando polimorfico `GET`.
    /// Se la sintassi è inequivocabile (es. GET #), chiama il parser corretto.
    /// Altrimenti, crea sempre un nodo `AmbiguousMethodStmt` da risolvere semanticamente dal Linker.
    /// </summary>
    public static Stmt DispatchGetStatement(Parser parser, string expectedNamespace)
    {
        // CASO 1: La chiamata è qualificata. Nessuna ambiguità.
        
        // --- CASO 2: La chiamata non è qualificata. ---

        // 2a. Priorità alla sintassi inequivocabile dei file (con '#').
        if (parser.At(1).Kind == TokenKind.Hash)
        {
            Log("[DispatchGetStatement] Disambiguazione sintattica: Trovato '#', indirizzo a IO.GET.");
            return ParseIoGetStatement(parser, "IO");
        }

        // 2b. La sintassi è ambigua (es. GET variabile).
        // PER ARCHITETTURA, stabiliamo che il default non qualificato è CONSOLE.GET.
        // DBF.GET richiederà la sintassi qualificata per essere invocato senza ambiguità.
        // Questo elimina la necessità di AmbiguousMethodStmt per GET.
        Log("[DispatchGetStatement] Sintassi non qualificata. Default a CONSOLE.GET.");
        return ParseConsoleGetStatement(parser, "CONSOLE");
    }

    /// <summary>
    /// [FASE 1] Agisce come router per `PRINT`, chiamando i parser di Fase 1 appropriati.
    /// La logica di lookahead per '#' viene mantenuta.
    /// </summary>
    public static Stmt DispatchPrintStatement(Parser parser, string expectedNamespace)
    {
        
        // Per la sintassi non qualificata, il lookahead rimane invariato.
        return parser.At(1).Kind == TokenKind.Hash
            // Chiama il parser di Fase 1 per la stampa su file.
            ? ParsePrintFileStatement(parser, "IO")
            // Chiama il parser di Fase 1 per la stampa su console/grafica.
            : ParsePrintStatement(parser, expectedNamespace);
    }

    /// <summary>
    /// Agisce come router per `CLS`. La sintassi è identica per CONSOLE e GRAPHICS.
    /// L'ambiguità è semantica, non sintattica. Si invoca il parser comune e si delega
    /// la decisione all'Executor a runtime.
    /// </summary>
    public static Stmt DispatchClsStatement(Parser parser, string expectedNamespace)
    {
        // Chiama il parser unico, che creerà un nodo ClsStmt generico.
        return ParseClsStatement(parser, expectedNamespace);
    }

    /// <summary>
    /// Agisce come router per `COLOR`. L'ambiguità è semantica e delegata a runtime.
    /// </summary>
    public static Stmt DispatchColorStatement(Parser parser, string expectedNamespace)
    {
        return ParseColorStatement(parser, expectedNamespace);
    }

    /// <summary>
    /// Agisce come router per `LOCATE`. L'ambiguità è semantica e delegata a runtime.
    /// </summary>
    public static Stmt DispatchLocateStatement(Parser parser, string expectedNamespace)
    {
        return ParseLocateStatement(parser, expectedNamespace);
    }

    /// <summary>
    /// Agisce come router intelligente per il comando polimorfico `INPUT`.
    /// Utilizza un lookahead non distruttivo per il token '#' per distinguere
    /// la versione per file I/O (`INPUT #...`) da quella per la console.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace che ha attivato la chiamata (usato per la sintassi qualificata).</param>
    /// <param name="qualifier">Il token del namespace se la chiamata era qualificata; altrimenti, null.</param>
    /// <returns>Un nodo AST <see cref="InputFileStmt"/> o <see cref="InputStmt"/>.</returns>
    public static Stmt DispatchInputStatement(Parser parser, string expectedNamespace)
    {
        // Per la sintassi qualificata, non c'è ambiguità.
        
        // Per la sintassi non qualificata (legacy), il lookahead per '#' risolve l'ambiguità.
        // Dobbiamo saltare la keyword INPUT stessa per il lookahead.
        return parser.At(1).Kind == TokenKind.Hash
            ? ParseInputFileStatement(parser, "IO")
            : ParseInputStatement(parser, "CONSOLE");
    }

    /// <summary>
    /// Agisce come router intelligente per `LINE INPUT`. Utilizza il lookahead per
    /// distinguere la versione per file da quella per console.
    /// </summary>
    public static Stmt DispatchLineInputStatement(Parser parser, string expectedNamespace)
    {
        
        // Per la sintassi non qualificata, il lookahead per '#' risolve l'ambiguità.
        // Il token LINE INPUT è ANCORA corrente al dispatch (il dispatcher lo consuma lui):
        // il '#' è il token SUCCESSIVO → At(1), non Check (che guarderebbe LINE INPUT).
        // Con Check, `LINE INPUT #n, var` cadeva nel ramo CONSOLE → "Expected an expression
        // atom" su IL/PCode/VM-C (C99Native lo parsava correttamente: gap cross-backend).
        return parser.At(1).Kind == TokenKind.Hash
            ? ParseLineInputFileStatement(parser, "IO")
            : ParseLineInputStatement(parser, "CONSOLE");
    }

    /// <summary>
    /// Analizza il token successivo per decidere se
    /// l'istruzione è un comando di controllo audio (PAUSE, RESUME, STOP) o
    /// un'istruzione di riproduzione standard (SOUND freq,dur o PLAY music$).
    /// </summary>
    /// <returns>Una lista di nodi Stmt contenente l'istruzione parsata corretta.</returns>
    private static IReadOnlyList<Stmt> DispatchAudioStatement(Parser parser)
    {
        var commandToken = parser.Peek(); // SOUND o PLAY

        // Esegue un lookahead per determinare il tipo di comando audio.
        var nextTokenKind = parser.At(1).Kind;
        if (nextTokenKind is TokenKind.Pause or TokenKind.Resume or TokenKind.Stop)
        {
            return [ParseAudioControlStatement(parser)];
        }

        if (nextTokenKind == TokenKind.Load)
        {
            return [ParseSoundLoadStatement(parser)];
        }

        if (nextTokenKind == TokenKind.Play)
        {
            return [ParseSoundPlayFileStatement(parser)];
        }

        if (nextTokenKind == TokenKind.Volume)
        {
            return [ParseSoundVolumeStatement(parser)];
        }

        var parsedStmt = parser.DispatchRegisteredCommand();
        return parsedStmt != null
            ? [parsedStmt]
            : throw CreateError(commandToken,
                $"Errore interno nel dispatching del comando audio '{commandToken.Lexeme}'.");
    }
}