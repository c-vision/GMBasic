// FILE: Interpreter.Parser.Stmt.Speed.cs

using System.Collections.Immutable;

// ReSharper disable SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
// ReSharper disable SwitchStatementHandlesSomeKnownEnumValuesWithDefault

// ReSharper disable UnusedParameter.Local
// ReSharper disable InconsistentNaming
namespace Basic;

/// <summary>
/// Contiene i metodi del Parser per analizzare le istruzioni specifiche del motore di database SPEED.
/// Supporta la doppia sintassi: la forma legacy multi-parola (es. `SPEED SET ORDER TO ... ON ...`)
/// e la forma qualificata fusa SENZA parentesi (direttiva AGENTS.md §2.2, es. `SPEED.SETORDER ...`),
/// che riusa le STESSE clausole della forma legacy. I comandi qualificati fusi che omettono l'alias
/// della work area operano sull'area attiva.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi del sottosistema SPEED che iniziano con la keyword 'SPEED'.
    /// Gestisce esclusivamente la sintassi legacy non qualificata (es. `SPEED USE ...`).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Dovrebbe essere sempre null per la sintassi legacy.</param>
    /// <returns>Il nodo AST per l'istruzione SPEED parsata.</returns>
    /// <remarks>
    /// I verbi fusi (es. `SPEED SETORDER` senza punto) NON sono riconosciuti qui e
    /// producono l'errore "Comando ... non riconosciuto per il motore SPEED.".
    /// </remarks>
    private static Stmt ParseSpeedStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Speed, "Internal parser error: Expected 'SPEED' keyword.");
        var commandToken = parser.Peek();

        // Disambigua i comandi composti basandosi sul token successivo.
        // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
        return ParseSpeedSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Nuovo helper che agisce come sub-dispatcher per i comandi `SET` del namespace SPEED.
    /// Viene chiamato solo dopo che il parser ha già consumato la sequenza `SPEED SET`.
    /// VERSIONE CORRETTA: Non consuma 'SET', ma delega il consumo ai metodi specifici.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED") che ha attivato la chiamata.</param>
    /// <param name="qualifier">Il token 'SPEED' della chiamata legacy.</param>
    /// <returns>Il nodo Stmt corrispondente al comando SET di SPEED parsato.</returns>
    private static Stmt ParseSpeedSetSubCommand(Parser parser, string expectedNamespace)
    {
        // NON consuma SET. Guarda il token successivo (dopo SET) per decidere.
        var subCommandToken = parser.At(1);
        // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
        return subCommandToken.Kind switch
        {
            TokenKind.Order => ParseSpeedSetOrderStatement(parser, expectedNamespace),
            TokenKind.Filter => ParseSpeedSetFilterStatement(parser, expectedNamespace),
            TokenKind.Relation => ParseSpeedSetRelationStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando '{subCommandToken.Lexeme}' non riconosciuto per SPEED SET.")
        };
    }

    /// <summary>
    /// Crea l'espressione "area attiva" (handle -1) usata dalla forma qualificata fusa
    /// dei comandi SPEED che omettono l'alias della work area (es. `SPEED.GO TOP`,
    /// `SPEED.GET rec`): l'alias corrente viene risolto a runtime dal motore.
    /// </summary>
    private static NumberExpr SpeedActiveArea(Token token) =>
        new(-1, new Token(TokenKind.Number, "-1", -1.0, token.Position));

    /// <summary>
    /// Parsa `SPEED OPEN` e `SPEED.OPEN`.
    /// La sintassi, sia legacy che qualificata fusa, usa le STESSE clausole: richiede un
    /// alias letterale (identificatore non quotato) e accetta le clausole opzionali
    /// `FROM`/`OF` (consumate per compatibilità con la forma USE).
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedOpenStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED OPEN "C:\DATI" FROM 1 OF MioTipo ALIAS MioDb
    /// Moderna: SPEED.OPEN "C:\DATI" FROM 1 OF MioTipo ALIAS MioDb
    /// </example>
    public static SpeedOpenStmt ParseSpeedOpenStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Open, "Expected 'OPEN'.");
        Expr dbPath;
        Token aliasToken;

        // Sintassi unificata (legacy e qualificata fusa): OPEN "path" [FROM dbRef OF typeName] ALIAS alias
        dbPath = parser.ParseExpression();

        // Clausole opzionali FROM/OF: il riferimento al database e il tipo sono gestiti
        // semanticamente da USE; qui vengono solo consumati per compatibilità di sintassi.
        if (parser.Match(TokenKind.From))
        {
            parser.ParseExpression();
        }

        if (parser.Match(TokenKind.Of))
        {
            parser.Consume(TokenKind.Identifier, "Expected a TYPE name after 'OF'.");
        }

        parser.Consume(TokenKind.Alias, "Expected 'ALIAS' after database path.");
        aliasToken = parser.Consume(TokenKind.Identifier, "Expected database alias name.");

        return new SpeedOpenStmt(dbPath, aliasToken, commandToken);
    }
    internal static Stmt ParseSpeedSubcommand(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Peek();
        return commandToken.Kind switch
        {
            // ReSharper disable once SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
            TokenKind.Create => parser.At(1).Kind switch
            {
                TokenKind.Index => ParseSpeedCreateIndexStatement(parser, expectedNamespace),
                TokenKind.View => ParseSpeedCreateViewStatement(parser, expectedNamespace),
                _ => throw CreateError(parser.At(1), "Expected INDEX or VIEW after CREATE.")
            },
            TokenKind.Drop => parser.At(1).Kind switch
            {
                TokenKind.Index => ParseSpeedDropIndexStatement(parser, expectedNamespace),
                TokenKind.View => ParseSpeedDropViewStatement(parser, expectedNamespace),
                _ => throw CreateError(parser.At(1), "Expected INDEX or VIEW after DROP.")
            },
            TokenKind.Refresh => parser.At(1).Kind == TokenKind.View
                ? ParseSpeedRefreshViewStatement(parser, expectedNamespace)
                : throw CreateError(parser.At(1), "Expected VIEW after REFRESH."),
            TokenKind.Set =>
                // Delega al nuovo sub-dispatcher per i comandi SET.
                ParseSpeedSetSubCommand(parser, expectedNamespace),
            // Per i comandi a parola singola, invoca direttamente il parser.
            TokenKind.Open => ParseSpeedOpenStatement(parser, expectedNamespace),
            TokenKind.Use => ParseSpeedUseStatement(parser, expectedNamespace),
            TokenKind.Get => ParseSpeedGetStatement(parser),
            TokenKind.Put => ParseSpeedPutStatement(parser),
            TokenKind.Delete => ParseSpeedDeleteStatement(parser),
            TokenKind.Seek => ParseSpeedSeekStatement(parser),
            TokenKind.Go => ParseSpeedGoStatement(parser),
            TokenKind.Skip => ParseSpeedSkipStatement(parser, expectedNamespace),
            TokenKind.Select => ParseSpeedSelectStatement(parser, expectedNamespace),
            TokenKind.Close => ParseSpeedCloseStatement(parser, expectedNamespace),
            _ => throw CreateError(commandToken,
                $"Comando '{commandToken.Lexeme}' non riconosciuto per il motore SPEED.")
        };
    }

    /// <summary>
    /// Parsa `SPEED CLOSE [alias | DATABASES]`: chiude il database SPEED per alias,
    /// l'area corrente (senza argomento), oppure tutti (`DATABASES`).
    /// </summary>
    public static SpeedCloseStmt ParseSpeedCloseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");
        if (parser.Match(TokenKind.Databases))
            return new SpeedCloseStmt(true, null, commandToken);
        if (parser.Peek().Kind == TokenKind.Identifier)
        {
            var alias = parser.Consume(TokenKind.Identifier, "Expected database alias name.");
            return new SpeedCloseStmt(false, alias, commandToken);
        }
        return new SpeedCloseStmt(false, null, commandToken);
    }



    /// <summary>
    /// Parsa `SPEED USE` e `SPEED.USE`.
    /// La sintassi, sia legacy che qualificata fusa, usa le STESSE clausole:
    /// `USE tabella FROM dbAlias OF tipo ALIAS tblAlias [INDEXED BY ...] [UNSAFE]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo AST di tipo <see cref="SpeedUseStmt"/>.</returns>
    /// <example>
    /// Legacy:
    /// <code>
    /// SPEED USE "Clienti" FROM MioDb OF Cliente ALIAS ClientiTbl INDEXED BY ID
    /// </code>
    /// Sintassi Qualificata fusa (stesse clausole, senza parentesi):
    /// <code>
    /// SPEED.USE "Clienti" FROM MioDb OF Cliente ALIAS ClientiTbl INDEXED BY ID
    /// </code>
    /// </example>
    public static SpeedUseStmt ParseSpeedUseStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Use, "Expected 'USE'.");
        Expr tableName;
        Expr databaseReference;
        Token typeNameToken;
        Token aliasToken;
        IsamIndexedByClause? indexedBy = null;
        var isUnsafe = false;

        // Sintassi unificata (legacy e qualificata fusa): USE tabella FROM dbAlias OF tipo ALIAS tblAlias [opzioni]
        tableName = parser.ParseExpression();
        parser.Consume(TokenKind.From, "Expected 'FROM' after table name to specify the database alias.");
        var dbRefToken =
            parser.Consume(TokenKind.Identifier, "Database reference must be a valid alias identifier.");

        var (baseName, _) = SplitNameAndSuffix(dbRefToken.Lexeme);
        var cleanedToken = dbRefToken with { Lexeme = baseName };
        databaseReference = new VariableExpr(-1, dbRefToken, cleanedToken);

        parser.Consume(TokenKind.Of, "Expected 'OF' after database reference to specify the TYPE.");
        typeNameToken = parser.Consume(TokenKind.Identifier, "Expected a TYPE name after 'OF'.");
        parser.Consume(TokenKind.Alias, "Expected 'ALIAS' to name the table work area.");
        aliasToken = parser.Consume(TokenKind.Identifier, "Expected an alias name after 'ALIAS'.");

        while (!parser.IsAtEndOfStatement())
        {
            if (parser.Match(TokenKind.Indexed))
            {
                parser.Consume(TokenKind.By, "Expected 'BY' after 'INDEXED'.");
                indexedBy = ParseIsamIndexedByClause(parser);
            }
            else if (parser.Match(TokenKind.Unsafe))
            {
                isUnsafe = true;
            }
            else
            {
                break;
            }
        }

        return new SpeedUseStmt(tableName, databaseReference, -1, typeNameToken, indexedBy, -1, aliasToken, isUnsafe,
            commandToken);
    }

    /// <summary>
    /// Parsa `SPEED CREATE INDEX` e `SPEED.CREATEINDEX`.
    /// La sintassi, sia legacy che qualificata fusa, usa le STESSE clausole:
    /// `CREATE INDEX indexName ON alias KEY (expr) [LEN n]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedCreateIndexStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED CREATE INDEX "CittaIdx" ON "Clienti" KEY (Citta)
    /// Moderna: SPEED.CREATEINDEX "CittaIdx" ON "Clienti" KEY (Citta)
    /// </example>
    public static Stmt ParseSpeedCreateIndexStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: CREATE INDEX indexName ON alias KEY (expr) [LEN n]
        var commandToken = parser.Consume(TokenKind.Create, "Expected CREATE or CREATEINDEX.");
        parser.Consume(TokenKind.Index, "Expected INDEX after CREATE.");

        // indexName ON alias KEY (expr) [LEN n]
        var indexName = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected ON after index name.");
        var alias = parser.ParseExpression();
        parser.Consume(TokenKind.Key, "Expected KEY after alias.");
        parser.Consume(TokenKind.LeftParen, "Expected '(' before key expression.");
        var keyExpr = parser.ParseExpression();
        parser.Consume(TokenKind.RightParen, "Expected ')' after key expression.");

        // LEN è accettato per compatibilità con ISAM CREATE INDEX; non è esposto nell'AST SPEED.
        if (parser.Match(TokenKind.Len))
        {
            parser.ParseExpression();
        }

        return new SpeedCreateIndexStmt(indexName, alias, keyExpr, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED DROP INDEX` e `SPEED.DROPINDEX`.
    /// La sintassi, sia legacy che qualificata fusa, usa le STESSE clausole:
    /// `DROP INDEX indexName ON alias`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedDropIndexStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED DROP INDEX "CittaIdx" ON "Clienti"
    /// Moderna: SPEED.DROPINDEX "CittaIdx" ON "Clienti"
    /// </example>
    public static Stmt ParseSpeedDropIndexStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: DROP INDEX indexName ON alias
        var commandToken = parser.Consume(TokenKind.Drop, "Expected DROP or DROPINDEX.");
        parser.Consume(TokenKind.Index, "Expected INDEX after DROP.");

        // indexName ON alias
        var indexName = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected ON after index name.");
        var alias = parser.ParseExpression();

        return new SpeedDropIndexStmt(indexName, alias, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED GET` e `SPEED.GET`.
    /// La forma legacy richiede l'alias (`SPEED GET alias, destination`); la forma qualificata
    /// fusa opera sull'area attiva: `SPEED.GET destination`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedGetStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED GET Clienti, clienteRec
    /// Moderna: SPEED.GET clienteRec
    /// </example>
    public static SpeedGetStmt ParseSpeedGetStatement(Parser parser)
    {
        var commandToken = parser.Consume(TokenKind.Get, "Expected GET.");
        Expr alias;
        Target destination;

        // Sintassi unificata (legacy e qualificata fusa): GET alias, destination
        alias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after alias.");
        destination = parser.ParseTarget();

        return new SpeedGetStmt(alias, destination, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED PUT` e `SPEED.PUT`.
    /// La forma legacy richiede l'alias (`SPEED PUT alias [, source]`); la forma qualificata
    /// fusa opera sull'area attiva: `SPEED.PUT [source]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedPutStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED PUT Clienti, clienteRec
    /// Moderna: SPEED.PUT clienteRec
    /// </example>
    public static SpeedPutStmt ParseSpeedPutStatement(Parser parser)
    {
        var commandToken = parser.Consume(TokenKind.Put, "Expected PUT.");
        Expr alias;
        Expr? source = null;

        // Sintassi unificata (legacy e qualificata fusa): PUT alias [, source]
        alias = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma)) source = parser.ParseExpression();

        return new SpeedPutStmt(alias, source, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED DELETE` e `SPEED.DELETE`.
    /// La forma legacy richiede l'alias (`SPEED DELETE alias [, source]`); la forma qualificata
    /// fusa opera sull'area attiva: `SPEED.DELETE [FOR cond]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedDeleteStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED DELETE Clienti, recordToDelete
    /// Moderna: SPEED.DELETE FOR Saldo = 0
    /// </example>
    public static SpeedDeleteStmt ParseSpeedDeleteStatement(Parser parser)
    {
        var commandToken = parser.Consume(TokenKind.Delete, "Expected DELETE.");
        Expr alias;
        Expr? source = null;

        // Sintassi unificata (legacy e qualificata fusa): DELETE alias [, source]
        alias = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma)) source = parser.ParseExpression();

        return new SpeedDeleteStmt(alias, source, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED SEEK` e `SPEED.SEEK`.
    /// La forma legacy richiede l'alias (`SPEED SEEK alias, keyValue [USING indexName]`); la
    /// forma qualificata fusa opera sull'area attiva: `SPEED.SEEK keyValue [USING indexName]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedSeekStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED SEEK Clienti, 101 USING "ID"
    /// Moderna: SPEED.SEEK 101 USING "ID"
    /// </example>
    public static SpeedSeekStmt ParseSpeedSeekStatement(Parser parser)
    {
        var commandToken = parser.Consume(TokenKind.Seek, "Expected SEEK.");
        Expr alias, keyValue;
        Expr? indexName = null;

        // Sintassi unificata (legacy e qualificata fusa): SEEK alias, keyValue [USING indexName]
        alias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after alias.");
        keyValue = parser.ParseExpression();
        if (parser.Match(TokenKind.Using)) indexName = parser.ParseExpression();

        return new SpeedSeekStmt(alias, keyValue, indexName, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED GO` e `SPEED.GO`.
    /// La forma legacy richiede l'alias (`SPEED GO alias, TOP|BOTTOM`); la forma qualificata
    /// fusa opera sull'area attiva: `SPEED.GO TOP | SPEED.GO n`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedGoStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED GO Clienti, TOP
    /// Moderna: SPEED.GO TOP
    /// </example>
    public static SpeedGoStmt ParseSpeedGoStatement(Parser parser)
    {
        var commandToken = parser.Consume(TokenKind.Go, "Expected GO.");
        Expr alias;
        Token location;

        // Sintassi unificata (legacy e qualificata fusa): GO alias, TOP | BOTTOM
        alias = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after alias.");
        location = parser.Consume(TokenKind.Top, TokenKind.Bottom);

        return new SpeedGoStmt(alias, location, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED SKIP` e `SPEED.SKIP`.
    /// La forma legacy richiede l'alias (`SPEED SKIP alias [, n]`); la forma qualificata
    /// fusa opera sull'area attiva: `SPEED.SKIP [n]`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedSkipStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED SKIP Clienti, -1
    /// Moderna: SPEED.SKIP -1
    /// </example>
    public static SpeedSkipStmt ParseSpeedSkipStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Skip, "Expected SKIP.");
        Expr alias;
        Expr? count = null;

        // Sintassi unificata (legacy e qualificata fusa): SKIP alias [, count]
        alias = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma)) count = parser.ParseExpression();

        return new SpeedSkipStmt(alias, count, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED SET ORDER` e `SPEED.SETORDER`.
    /// La forma legacy usa le clausole `SET ORDER TO indexName ON alias`; la forma qualificata
    /// fusa accetta `[TO] indexName` opzionale e opera sull'area attiva: `SPEED.SETORDER [TO] val`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedSetOrderStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED SET ORDER TO "NomeIdx" ON Clienti
    /// Moderna: SPEED.SETORDER "NomeIdx"
    /// </example>
    public static SpeedSetOrderStmt ParseSpeedSetOrderStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: SET ORDER TO indexName ON alias
        var commandToken = parser.Consume(TokenKind.Set, "Expected 'SET'.");
        parser.Consume(TokenKind.Order, "Expected ORDER after SET.");
        parser.Consume(TokenKind.To, "Expected TO after SET ORDER.");

        var indexName = parser.ParseExpression();
        parser.Consume(TokenKind.On, "Expected ON after index name.");
        var alias = parser.ParseExpression();

        return new SpeedSetOrderStmt(alias, indexName, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED SET FILTER` e `SPEED.SETFILTER`.
    /// La forma legacy usa le clausole `SET FILTER TO cond ON alias`; la forma qualificata
    /// fusa accetta `[TO] cond` opzionale e opera sull'area attiva: `SPEED.SETFILTER [TO] cond`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedSetFilterStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED SET FILTER TO Saldo > 0 ON Clienti
    /// Moderna: SPEED.SETFILTER Saldo > 0
    /// </example>
    public static SpeedSetFilterStmt ParseSpeedSetFilterStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: SET FILTER TO [cond] ON alias
        var commandToken = parser.Consume(TokenKind.Set, "Expected 'SET'.");
        parser.Consume(TokenKind.Filter, "Expected FILTER after SET.");
        parser.Consume(TokenKind.To, "Expected TO after SET FILTER.");

        Expr? condition = null;
        if (!parser.Check(TokenKind.On))
        {
            condition = parser.ParseExpression();
        }

        parser.Consume(TokenKind.On, "Expected ON after filter condition.");
        var alias = parser.ParseExpression();

        return new SpeedSetFilterStmt(alias, condition, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED SET RELATION` e `SPEED.SETRELATION`.
    /// La forma legacy usa le clausole `SET RELATION ON parent TO key INTO child`; la forma
    /// qualificata fusa accetta `[TO] key INTO child` opzionale e opera sull'area attiva (padre):
    /// `SPEED.SETRELATION [TO] key INTO child`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedSetRelationStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED SET RELATION ON Clienti TO ID INTO Ordini
    /// Moderna: SPEED.SETRELATION ID INTO Ordini
    /// </example>
    public static SpeedSetRelationStmt ParseSpeedSetRelationStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: SET RELATION ON parentAlias [TO key INTO child]
        var commandToken = parser.Consume(TokenKind.Set, "Expected 'SET'.");
        parser.Consume(TokenKind.Relation, "Expected RELATION after SET.");

        parser.Consume(TokenKind.On, "Expected ON after RELATION.");
        var parentAlias = parser.ParseExpression();
        Expr? keyExpr = null;
        Expr? childAlias = null;
        if (!parser.Match(TokenKind.To))
            return new SpeedSetRelationStmt(parentAlias, keyExpr, childAlias, commandToken);
        keyExpr = parser.ParseExpression();
        parser.Consume(TokenKind.Into, "Expected INTO after key expression.");
        if (parser.Check(TokenKind.Identifier) || parser.Check(TokenKind.StringLiteral))
        {
            childAlias = parser.ParseExpression();
        }
        else
        {
            throw CreateError(parser.Peek(),
                "Expected child alias name (identifier or string) after INTO.");
        }

        return new SpeedSetRelationStmt(parentAlias, keyExpr, childAlias, commandToken);
    }

    /// <summary>
    /// Parsa l'istruzione `SPEED CREATE VIEW` in entrambe le sue forme, legacy e qualificata fusa.
    /// Entrambe le sintassi richiedono che il nome della vista e il nome del tipo siano identificatori non quotati.
    /// </summary>
    /// <remarks>
    /// Sintassi Legacy:
    /// <code>
    /// SPEED CREATE VIEW nomeVista OF NomeTipo AS SELECT ...
    /// </code>
    /// 
    /// Sintassi Qualificata fusa (stesse clausole, senza parentesi):
    /// <code>
    /// SPEED.CREATEVIEW nomeVista OF NomeTipo AS SELECT ...
    /// </code>
    /// 
    /// In entrambi i casi, il parser non valida l'esistenza di 'NomeTipo'; si limita a catturarne il nome.
    /// </remarks>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace risolto dal dispatcher.</param>
    /// <param name="qualifier">Il token del qualificatore (es. 'SPEED'), se presente.</param>
    /// <returns>Un nodo AST <see cref="SpeedCreateViewStmt"/>.</returns>
    /// <exception cref="BasicException">Lanciata se la sintassi non è valida.</exception>
    public static SpeedCreateViewStmt ParseSpeedCreateViewStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: CREATE VIEW nome OF tipo AS SELECT ...
        var commandToken = parser.Consume(TokenKind.Create, "Expected 'CREATE'.");
        parser.Consume(TokenKind.View, "Expected 'VIEW' after 'CREATE'.");

        var viewNameToken = parser.Consume(TokenKind.Identifier, "Expected view name.");
        parser.Consume(TokenKind.Of, "Expected 'OF' after view name.");
        var typeNameToken = parser.Consume(TokenKind.Identifier, "Expected type name after 'OF'.");
        parser.Consume(TokenKind.As, "Expected 'AS' after type name.");

        SpeedSelectStmt? selectStmt = null;
        Expr? queryExpr = null;
        if (parser.Check(TokenKind.Select))
        {
            selectStmt = ParseSpeedSelectStatement(parser, expectedNamespace) as SpeedSelectStmt;
            if (selectStmt == null)
                throw CreateError(parser.Previous(), "Failed to parse SELECT statement within CREATE VIEW.");
        }
        else
        {
            queryExpr = parser.ParseExpression();
        }

        return new SpeedCreateViewStmt(-1, viewNameToken, -1, typeNameToken, selectStmt, queryExpr, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED DROP VIEW` e `SPEED.DROPVIEW`.
    /// Il nome della vista deve essere un identificatore non quotato in entrambe le sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedDropViewStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED DROP VIEW ClientiRoma
    /// Moderna: SPEED.DROPVIEW ClientiRoma
    /// </example>
    public static Stmt ParseSpeedDropViewStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: DROP VIEW nome
        var commandToken = parser.Consume(TokenKind.Drop, "Expected 'DROP'.");
        parser.Consume(TokenKind.View, "Expected 'VIEW' after 'DROP'.");

        var viewNameToken = parser.Consume(TokenKind.Identifier, "Expected view name as an identifier.");

        return new SpeedDropViewStmt(viewNameToken, commandToken);
    }

    /// <summary>
    /// Parsa `SPEED REFRESH VIEW` e `SPEED.REFRESHVIEW`.
    /// Il nome della vista deve essere un identificatore non quotato in entrambe le sintassi.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="expectedNamespace">Il namespace ("SPEED").</param>
    /// <param name="qualifier">Il token 'SPEED' se la chiamata è qualificata.</param>
    /// <returns>Un nodo <see cref="SpeedRefreshViewStmt"/>.</returns>
    /// <example>
    /// Legacy:  SPEED REFRESH VIEW ClientiRoma
    /// Moderna: SPEED.REFRESHVIEW ClientiRoma
    /// </example>
    public static Stmt ParseSpeedRefreshViewStatement(Parser parser, string expectedNamespace)
    {
        // Sintassi legacy: REFRESH VIEW nome
        var commandToken = parser.Consume(TokenKind.Refresh, "Expected 'REFRESH'.");
        parser.Consume(TokenKind.View, "Expected 'VIEW' after 'REFRESH'.");

        var viewNameToken = parser.Consume(TokenKind.Identifier, "Expected view name as an identifier.");

        return new SpeedRefreshViewStmt(viewNameToken, commandToken);
    }

    /// <summary>
    /// Parsa un'istruzione `SPEED SELECT`, gestendo sia la sintassi legacy (statica) che quella qualificata fusa (dinamica).
    /// </summary>
    /// <remarks>
    /// Sintassi Legacy (Statica):
    /// SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto INTO ris()
    /// OF RisultatoJoin FROM Clienti JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente GROUP BY Clienti.Nome
    /// 
    /// Sintassi Qualificata fusa (Dinamica, senza parentesi):
    /// `SPEED.SELECT "SELECT ..."`
    /// </remarks>
    public static Stmt ParseSpeedSelectStatement(Parser parser, string expectedNamespace)
    {
        var selectToken = parser.Consume(TokenKind.Select, "Expected 'SELECT' command name.");

        // --- Sintassi Legacy (Statica) ---
        var selectStmt = parser.ParsePureSelectQuery(selectToken);
        if (!parser.IsAtEnd() && parser.Check(TokenKind.Newline))
        {
            parser.Advance();
        }

        return selectStmt;
    }

    /// <summary>
    /// Parsa la clausola `SELECT`, che consiste in una lista di campi (espressioni)
    /// con alias opzionali (`AS`).
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// SELECT *
    /// SELECT Nome, Citta
    /// SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS Totale
    /// </code>
    /// </example>
    private static ImmutableArray<SelectField> ParseSelectClause(Parser parser)
    {
        var fields = ImmutableArray.CreateBuilder<SelectField>();
        do
        {
            var expr =
                parser.Match(TokenKind.Multiply)
                    ? new StringExpr("*", parser.Previous())
                    : parser.ParseExpressionFragment(TokenKind.As, TokenKind.Comma, TokenKind.Into);

            Token? alias = null;
            if (parser.Match(TokenKind.As))
            {
                alias = parser.Consume(TokenKind.Identifier, "Expected an alias name after AS.");
            }

            fields.Add(new SelectField(expr, alias));
        } while (parser.Match(TokenKind.Comma));

        return fields.ToImmutable();
    }

    /// <summary>
    /// Parsa la clausola `FROM`, che specifica la tabella o vista principale della query.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// FROM Clienti
    /// FROM Clienti AS C
    /// </code>
    /// </example>
    private static FromClause ParseFromClause(Parser parser)
    {
        parser.Consume(TokenKind.From, "Expected FROM clause.");
        var tableAlias = parser.Consume(TokenKind.Identifier, "Expected table alias in FROM clause.");
        Token? asAlias = null;
        if (parser.Match(TokenKind.As))
        {
            asAlias = parser.Consume(TokenKind.Identifier, "Expected alias name after AS.");
        }

        return new FromClause(tableAlias, asAlias);
    }

    /// <summary>
    /// Parsa la clausola `JOIN`, che specifica una tabella secondaria e la condizione di join.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// JOIN Ordini ON Clienti.ID = Ordini.ID_Cliente
    /// JOIN Prodotti AS P ON Dettagli.ID_Prodotto = P.ID
    /// </code>
    /// </example>
    private static JoinClause ParseJoinClause(Parser parser)
    {
        parser.Consume(TokenKind.Join, "Expected JOIN clause.");
        var tableAlias = parser.Consume(TokenKind.Identifier, "Expected table alias in JOIN clause.");
        Token? asAlias = null;
        if (parser.Match(TokenKind.As))
        {
            asAlias = parser.Consume(TokenKind.Identifier, "Expected alias name after AS.");
        }

        parser.Consume(TokenKind.On, "Expected ON in JOIN clause.");
        var condition = parser.ParseExpression();
        return new JoinClause(tableAlias, asAlias, condition);
    }

    /// <summary>
    /// Parsa la clausola `WHERE`, che definisce la condizione di filtro per i record.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// WHERE Clienti.Attivo = TRUE AND Clienti.Saldo > 0
    /// </code>
    /// </example>
    private static Expr ParseWhereClause(Parser parser)
    {
        parser.Consume(TokenKind.Where, "Expected WHERE clause.");
        return parser.ParseExpressionFragment(TokenKind.Group, TokenKind.Having, TokenKind.Order);
    }

    /// <summary>
    /// Parsa la clausola `GROUP BY`, che raggruppa i record in base a una o più espressioni.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// GROUP BY Clienti.Citta
    /// GROUP BY YEAR(Ordini.DataOrdine)
    /// </code>
    /// </example>
    private static ImmutableArray<Expr> ParseGroupByClause(Parser parser)
    {
        parser.Consume(TokenKind.Group, "Expected GROUP keyword.");
        parser.Consume(TokenKind.By, "Expected BY after GROUP.");
        var fields = ImmutableArray.CreateBuilder<Expr>();
        do
        {
            fields.Add(parser.ParseExpressionFragment(TokenKind.Comma, TokenKind.Having, TokenKind.Order));
        } while (parser.Match(TokenKind.Comma));

        return fields.ToImmutable();
    }

    /// <summary>
    /// Parsa la clausola `HAVING`, che filtra i risultati dopo il raggruppamento.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// HAVING COUNT(*) > 5
    /// HAVING SUM(Ordini.Importo) > 1000
    /// </code>
    /// </example>
    private static Expr ParseHavingClause(Parser parser)
    {
        parser.Consume(TokenKind.Having, "Expected HAVING keyword.");
        return parser.ParseExpressionFragment(TokenKind.Order);
    }

    /// <summary>
    /// Parsa la clausola `ORDER BY`, che definisce l'ordinamento del set di risultati finale.
    /// </summary>
    /// <example>
    /// Sintassi gestite:
    /// <code>
    /// ORDER BY Clienti.Nome
    /// ORDER BY TotaleImporto DESC
    /// ORDER BY Clienti.Citta ASC, Clienti.Saldo DESC
    /// </code>
    /// </example>
    private static ImmutableArray<OrderByField> ParseOrderByClause(Parser parser)
    {
        parser.Consume(TokenKind.Order, "Expected ORDER keyword.");
        parser.Consume(TokenKind.By, "Expected BY after ORDER.");
        var fields = ImmutableArray.CreateBuilder<OrderByField>();
        do
        {
            var expr = parser.ParseExpressionFragment(TokenKind.Comma, TokenKind.Identifier);
            var descending = false;
            if (parser.Match(TokenKind.Desc))
            {
                descending = true;
            }
            else
            {
                parser.Match(TokenKind.Asc);
            }

            fields.Add(new OrderByField(expr, descending));
        } while (parser.Match(TokenKind.Comma));

        return fields.ToImmutable();
    }

    /// <summary>
    /// Parsa una query `SPEED SELECT` completa e statica, escludendo la keyword iniziale `SPEED`.
    /// Questo metodo è il cuore del parsing per la sintassi legacy e per le viste.
    /// Si occupa di analizzare tutte le clausole (`SELECT`, `INTO`, `FROM`, `WHERE`, etc.) e, crucialmente,
    /// di risolvere il tipo di destinazione della clausola `INTO ... OF {TypeName}` per annotare il
    /// `SymbolId` corretto nell'AST.
    /// </summary>
    /// <param name="selectTokenOverride">
    /// Il token `SELECT` che ha avviato il parsing, fornito dal metodo chiamante.
    /// </param>
    /// <returns>Un nodo AST <see cref="SpeedSelectStmt"/> completo e semanticamente arricchito.</returns>
    /// <exception cref="BasicException">
    /// Se la sintassi della query non è valida, se mancano clausole obbligatorie (`FROM`, `INTO`),
    /// o se il tipo specificato nella clausola `OF` non è definito.
    /// </exception>
    public SpeedSelectStmt ParsePureSelectQuery(Token? selectTokenOverride = null)
    {
        Log($"[ParsePureSelectQuery] ENTER. Current token: {Peek()}");
        var selectToken = selectTokenOverride ?? Consume(TokenKind.Select, "Expected SELECT.");
        Log($"[ParsePureSelectQuery]   Consumed SELECT. Next token: {Peek()}");

        var fields = ParseSelectClause(this);
        Log($"[ParsePureSelectQuery]   Parsed SELECT fields. Next token: {Peek()}");

        Target? intoTarget = null;
        var destinationUdtSymbolId = -1;
        Token? typeNameToken = null;
        FromClause? from = null;
        JoinClause? join = null;
        Expr? where = null;
        ImmutableArray<Expr>? groupBy = null;
        Expr? having = null;
        ImmutableArray<OrderByField>? orderBy = null;

        Log($"[ParsePureSelectQuery]   Starting clause parsing loop. Current token: {Peek()}");
        while (true)
        {
            SkipNewlines(this);
            if (IsAtEnd()) break;

            var clauseToken = Peek();
            Log($"[ParsePureSelectQuery]     > Loop iteration. Clause token: {clauseToken}");

            switch (clauseToken.Kind)
            {
                case TokenKind.Into:
                    if (intoTarget != null) throw CreateError(clauseToken, "Clausola INTO duplicata.");
                    Consume(TokenKind.Into, "Internal parser error.");
                    intoTarget = ParseTarget();

                    Consume(TokenKind.Of, "Expected 'OF <TypeName>' after INTO clause.");
                    typeNameToken = Consume(TokenKind.Identifier, "Expected a TYPE name after OF.");
                    destinationUdtSymbolId = -1; // Placeholder per Fase 1

                    Log($"[ParsePureSelectQuery]       Parsed INTO ... OF. Next token: {Peek()}");
                    break;

                // ... resto dei case per FROM, JOIN, WHERE, etc. (invariati) ...
                case TokenKind.From:
                    if (from != null) throw CreateError(clauseToken, "Clausola FROM duplicata.");
                    from = ParseFromClause(this);
                    Log($"[ParsePureSelectQuery]       Parsed FROM. Next token: {Peek()}");
                    break;

                case TokenKind.Join:
                    if (join != null) throw CreateError(clauseToken, "Clausola JOIN duplicata.");
                    join = ParseJoinClause(this);
                    Log($"[ParsePureSelectQuery]       Parsed JOIN. Next token: {Peek()}");
                    break;

                case TokenKind.Where:
                    if (where != null) throw CreateError(clauseToken, "Clausola WHERE duplicata.");
                    where = ParseWhereClause(this);
                    Log($"[ParsePureSelectQuery]       Parsed WHERE. Next token: {Peek()}");
                    break;

                case TokenKind.Group when At(1).Kind == TokenKind.By:
                    if (groupBy != null) throw CreateError(clauseToken, "Clausola GROUP BY duplicata.");
                    groupBy = ParseGroupByClause(this);
                    Log($"[ParsePureSelectQuery]       Parsed GROUP BY. Next token: {Peek()}");
                    break;

                case TokenKind.Having:
                    if (having != null) throw CreateError(clauseToken, "Clausola HAVING duplicata.");
                    having = ParseHavingClause(this);
                    Log($"[ParsePureSelectQuery]       Parsed HAVING. Next token: {Peek()}");
                    break;

                case TokenKind.Order when At(1).Kind == TokenKind.By:
                    if (orderBy != null) throw CreateError(clauseToken, "Clausola ORDER BY duplicata.");
                    orderBy = ParseOrderByClause(this);
                    Log($"[ParsePureSelectQuery]       Parsed ORDER BY. Next token: {Peek()}");
                    break;


                default:
                    Log($"[ParsePureSelectQuery]     > Unrecognized clause token {clauseToken}. Exiting loop.");
                    goto end_clause_loop;
            }
        }

        end_clause_loop: ;

        Log($"[ParsePureSelectQuery]   Clause parsing loop finished. Current token: {Peek()}");

        if (from == null)
            throw CreateError(selectToken, "Clausola FROM mancante nell'istruzione SELECT.");
        if (intoTarget == null)
            throw CreateError(selectToken, "Clausola INTO mancante nell'istruzione SELECT.");

        Log("[ParsePureSelectQuery] EXIT. Validation passed.");

        return new SpeedSelectStmt(fields, intoTarget, destinationUdtSymbolId, typeNameToken, from, join, where,
            groupBy, having,
            orderBy, null, selectToken);
    }
}