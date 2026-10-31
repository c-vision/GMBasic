// FILE: Interpreter.Parser.Statements.Json.cs

// ReSharper disable InvalidXmlDocComment
namespace Basic;

/// <summary>
/// Contiene i metodi di parsing per i comandi del namespace JSON.
/// Implementa la doppia sintassi: legacy (`JSON COMANDO ...`) e moderna
/// qualificata fusa (`JSON.COMANDO ...`, senza parentesi — AGENTS.md §2.2).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [DISPATCHER] Parsa un'istruzione che inizia con la keyword `JSON`.
    /// Agisce come un router che determina il sotto-comando (OPEN, SAVE, etc.)
    /// e invoca il parser specifico.
    /// </summary>
    public static Stmt ParseJsonStatement(Parser parser, string ns)
    {
        // Se c'è un punto dopo JSON (es. JSON.SAVE), significa che è una chiamata
        // a funzione/comando qualificata. Il dispatcher principale gestirà la chiamata
        // a `DispatchRegisteredCommand`, che a sua volta invocherà il metodo di parsing
        // corretto con il 'qualifier' non nullo.
        if (parser.Match(TokenKind.Dot))
        {
            var commandNode = parser.DispatchRegisteredCommand();
            if (commandNode != null)
            {
                return commandNode;
            }

            // Se non è un comando registrato, potrebbe essere un'espressione (es. JSON.Parse)
            // che viene gestita dal parser di espressioni.
            throw CreateError(parser.Peek(), "Comando JSON non riconosciuto dopo '.'.");
        }

        // Se non c'è il punto, è la sintassi legacy "JSON COMANDO ..."
        // Consumiamo il token JSON che ha attivato questo dispatcher.
        parser.Consume(TokenKind.Json, "Internal parser error: Expected 'JSON' keyword.");

        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            TokenKind.Save => ParseJsonSaveStatement(parser, ns),
            TokenKind.WriteValue => ParseJsonWriteValueStatement(parser, ns),
            TokenKind.WriteSection => ParseJsonWriteSectionStatement(parser, ns),
            TokenKind.Open => ParseJsonOpenStatement(parser, ns),
            _ => throw CreateError(commandToken, $"Comando non riconosciuto dopo 'JSON': '{commandToken.Lexeme}'.")
        };
    }

    /// <summary>
    /// Parsa il comando `JSON SAVE` in entrambe le sue forme, legacy e qualificata,
    /// per serializzare un oggetto (JSON, UDT, DICT, etc.) e salvarlo in un file di testo.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("JSON").</param>
    /// <param name="qualifier">Il token qualificatore ("JSON"), se la chiamata è moderna (es. JSON.SAVE).</param>
    /// <returns>Un nodo AST <see cref="JsonSaveStmt"/>.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `JSON SAVE <sorgente>, <percorso_file$>`
    /// 
    /// <code>
    /// DIM Dati AS JSON
    /// Dati = JSON.Parse("{'id':1}")
    /// 
    /// ' Salva l'oggetto Dati nel file "output.json"
    /// JSON SAVE Dati, "output.json"
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `JSON.SAVE source, path$`
    /// 
    /// <code>
    /// DIM MioUDT AS Config
    /// ' ... popola MioUDT ...
    /// 
    /// ' Salva l'istanza dell'UDT nel file "config.json"
    /// JSON.SAVE MioUDT, "config.json"
    /// </code>
    /// 
    /// </example>
    public static Stmt ParseJsonSaveStatement(Parser parser, string ns)
    {
        var saveToken = parser.Consume(TokenKind.Save, "Internal parser error: Expected 'SAVE' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): source, path ---
        var sourceExpr = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' to separate source and file path.");
        var filePathExpr = parser.ParseExpression();

        return new JsonSaveStmt(sourceExpr, filePathExpr, saveToken);
    }

    /// <summary>
    /// Parsa il comando `JSON WRITEVALUE` in entrambe le sue forme, legacy e qualificata,
    /// per scrivere una singola coppia chiave-valore nel file di configurazione `app.settings.json`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("JSON").</param>
    /// <param name="qualifier">Il token qualificatore ("JSON"), se la chiamata è moderna (es. JSON.WRITEVALUE).</param>
    /// <returns>Un nodo AST <see cref="JsonWriteValueStmt"/>.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `JSON WRITEVALUE <chiave$>, <valore> [, <sezione$>]`
    /// 
    /// <code>
    /// ' Scrive una chiave al livello radice
    /// JSON WRITEVALUE "ApiKey", "XYZ123"
    /// 
    /// ' Scrive una chiave all'interno della sezione "ConnectionStrings"
    /// JSON WRITEVALUE "DefaultConnection", "server=localhost;...", "ConnectionStrings"
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `JSON.WRITEVALUE key$, value [, section$]`
    /// 
    /// <code>
    /// ' Scrive una chiave al livello radice
    /// JSON.WRITEVALUE "LogLevel", "Debug"
    /// 
    /// ' Scrive una chiave all'interno di una sezione
    /// JSON.WRITEVALUE "Timeout", 30000, "AppSettings"
    /// </code>
    /// 
    /// </example>
    public static Stmt ParseJsonWriteValueStatement(Parser parser, string ns)
    {
        var writeValueToken =
            parser.Consume(TokenKind.WriteValue, "Internal parser error: Expected 'WRITEVALUE' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): key, value [, section] ---
        var keyExpr = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after key.");
        var valueExpr = parser.ParseExpression();

        Expr? sectionExpr = null;
        if (parser.Match(TokenKind.Comma))
        {
            sectionExpr = parser.ParseExpression();
        }

        return new JsonWriteValueStmt(keyExpr, valueExpr, sectionExpr, writeValueToken);
    }

    /// <summary>
    /// Parsa il comando `JSON WRITESECTION` in entrambe le sue forme, legacy e qualificata,
    /// per scrivere i dati da una variabile (UDT o DICT) in una sezione del file di
    /// configurazione `app.settings.json`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("JSON").</param>
    /// <param name="qualifier">Il token qualificatore ("JSON"), se la chiamata è moderna (es. JSON.WRITESECTION).</param>
    /// <returns>Un nodo AST <see cref="JsonWriteSectionStmt"/>.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `JSON WRITESECTION sorgente [, sezione$]`
    /// 
    /// <code>
    /// DIM MioUDT AS Config
    /// ' ... popola MioUDT ...
    /// 
    /// ' Scrive nella radice del file di configurazione
    /// JSON WRITESECTION MioUDT
    /// 
    /// ' Scrive in una sezione specifica chiamata "Database"
    /// JSON WRITESECTION MioUDT, "Database"
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `JSON.WRITESECTION source [, section$]`
    /// 
    /// <code>
    /// DIM MioDizionario AS DICT<STRING, STRING>
    /// ' ... popola MioDizionario ...
    /// 
    /// ' Scrive nella radice del file
    /// JSON.WRITESECTION MioDizionario
    /// 
    /// ' Scrive in una sezione specifica
    /// JSON.WRITESECTION MioDizionario, "Logging"
    /// </code>
    /// 
    /// </example>
    public static Stmt ParseJsonWriteSectionStatement(Parser parser, string ns)
    {
        var writeSectionToken =
            parser.Consume(TokenKind.WriteSection, "Internal parser error: Expected 'WRITESECTION' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): source [, section] ---
        var sourceExpr = parser.ParseExpression();

        Expr? sectionExpr = null;
        if (parser.Match(TokenKind.Comma))
        {
            sectionExpr = parser.ParseExpression();
        }

        return new JsonWriteSectionStmt(sourceExpr, sectionExpr, writeSectionToken);
    }

    /// <summary>
    /// Parsa il comando `JSON OPEN` in entrambe le sue forme, legacy e qualificata, per aprire
    /// un file in modalità JSON. Questo metodo funge da "adattatore sintattico", trasformando
    /// la sintassi specifica del namespace JSON in un nodo AST `OpenStmt` standard, che
    /// il resto del sistema di I/O può comprendere.
    /// Imposta automaticamente il `FileContentType` su `JsonType`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto ("JSON").</param>
    /// <param name="qualifier">Il token qualificatore ("JSON"), se la chiamata è moderna (es. JSON.OPEN).</param>
    /// <returns>Un nodo AST <see cref="OpenStmt"/> configurato per l'accesso a file JSON.</returns>
    /// <example>
    /// 
    /// --- SINTASSI LEGACY (NON QUALIFICATA) ---
    /// 
    /// `JSON OPEN file$ FOR modo$ AS #handle`
    /// 
    /// <code>
    /// ' Aprire un file in lettura
    /// JSON OPEN "dati.json" FOR "READ" AS #1
    /// 
    /// ' Aprire un file in scrittura (usando sinonimi)
    /// JSON OPEN "output.json" FOR "W" AS #mioHandle
    /// </code>
    /// 
    /// --- SINTASSI MODERNA (QUALIFICATA FUSA) ---
    /// 
    /// `JSON.OPEN file$ FOR mode$ AS #handleVar`
    /// 
    /// <code>
    /// DIM handle AS LONG
    /// 
    /// ' Aprire un file in lettura
    /// JSON.OPEN "dati.json" FOR "READ" AS #handle
    /// 
    /// ' Aprire un file in scrittura (usando sinonimi)
    /// JSON.OPEN "output.json" FOR "W" AS #handle
    /// </code>
    /// 
    /// --- MODALITÀ DI ACCESSO SUPPORTATE ---
    /// - "READ", "INPUT", "R" (Lettura)
    /// - "WRITE", "OUTPUT", "W" (Scrittura, sovrascrive)
    /// - "APPEND", "A" (Aggiunta in coda)
    /// 
    /// </example>
    public static Stmt ParseJsonOpenStatement(Parser parser, string ns)
    {
        var openToken = parser.Consume(TokenKind.Open, "Internal parser error: Expected 'OPEN' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): file FOR mode AS #handleVar ---
        var fileNameExpr = parser.ParseExpression();
        parser.Consume(TokenKind.For, "Expected 'FOR' after file path.");
        var modeExpr = parser.ParseExpression(); // READ, WRITE, APPEND come identificatori o stringhe
        parser.Consume(TokenKind.As, "Expected 'AS' before handle.");
        parser.Consume(TokenKind.Hash, "Expected '#' before handle variable.");
        var handleTarget = parser.ParseTarget();

        // Estrae il valore della stringa e il token originale per il reporting degli errori.
        string modeString;
        Token modeToken;

        switch (modeExpr)
        {
            case StringExpr se:
                modeString = se.Value;
                modeToken = se.Token;
                break;
            case VariableExpr ve:
                modeString = ve.NameCleaned.Lexeme;
                modeToken = ve.NameCleaned;
                break;
            default:
                throw CreateError(parser.Previous(), "Access mode for JSON OPEN must be a constant string or identifier: READ, WRITE, or APPEND.");
        }
        
        // Determina la modalità di apertura, gestendo i sinonimi.
        var mode = modeString.ToUpperInvariant() switch
        {
            "READ" or "INPUT" or "R" => FileOpenMode.Input,
            "WRITE" or "OUTPUT" or "W" => FileOpenMode.Output,
            "APPEND" or "A" => FileOpenMode.Append,
            _ => throw CreateError(modeToken, "Invalid access mode. Use READ, WRITE, APPEND or their single-letter synonyms (R, W, A).")
        };

        // Converte il Target dell'handle in un Expr in modo sicuro.
        // La sintassi OPEN richiede una variabile semplice per l'handle.
        if (handleTarget is not VariableTarget handleVarTarget)
        {
            throw CreateError(parser.Previous(), "File handle for OPEN must be a simple variable.");
        }

        Expr handleExpr = new VariableExpr(-1, handleVarTarget.NameSuffixed, handleVarTarget.NameCleaned);

        // Costruisce un nodo OpenStmt standard, pre-compilando i campi specifici per JSON.
        return new OpenStmt(
            FileName: fileNameExpr,
            Mode: mode,
            Locking: DatabaseLockingMode.Default,
            LockMode: FileLockMode.Default,
            Access: FileAccessMode.Default,
            FileNumber: handleExpr, 
            FileContentType: new JsonType(), // Imposta il tipo a JSON
            RecordLength: null,
            IndexedBy: null,
            IndexPaths: System.Collections.Immutable.ImmutableArray<Expr>.Empty,
            EncodingExpr: null,
            IsUnsafe: false,
            Token: openToken
        );
    }
}