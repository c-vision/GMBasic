// FILE: Interpreter.Parser.Expressions.Complex.cs

using System.Collections.Immutable;

// ReSharper disable RedundantArgumentDefaultValue

// ReSharper disable InvalidXmlDocComment
namespace Basic;

/// <summary>
///     Contiene i metodi per il parsing di espressioni, gestendo la precedenza degli operatori.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [NUOVO HELPER] Parsa la "base" di un target, ovvero un L-value complesso
    /// come `myVar` o `myObject.myArray`, ma si ferma PRIMA di consumare eventuali
    /// parentesi di accesso a indici come `(...)` o `[...]`.
    /// Questo è necessario per comandi come `REDIM` che hanno la loro sintassi per le dimensioni.
    /// </summary>
    /// <returns>Un nodo `Target` che rappresenta la base dell'array da ridimensionare.</returns>
    private Target ParseTargetBase()
    {
        // Questa logica è una versione semplificata di ParsePrimary
        // che gestisce solo l'accesso ai membri con '.'
        var expr = ParsePrimaryAtom();

        while (Match(TokenKind.Dot, TokenKind.Arrow))
        {
            var memberToken = Peek();
            if (!IsPermissiveNameToken(memberToken))
            {
                throw CreateError(memberToken, "Expected a member name after '.' or '->'.");
            }

            Advance();

            expr = new FieldAccessExpr(expr, memberToken);
        }

        // Converte l'espressione risultante (che non ha indici) in un Target.
        return ParseTargetFromExpr(expr);
    }

    /// <summary>
    /// [FASE 1 - CORREZIONE DEFINITIVA] Parsa una destinazione di assegnazione (L-value).
    /// Riconosce `Nome()` come un riferimento all'intero array e crea direttamente un `VariableTarget`,
    /// eliminando l'ambiguità dell' `ArrayElementTarget` con zero indici.
    /// </summary>
    /// <returns>Un nodo `Target` che rappresenta la destinazione dell'assegnazione.</returns>
    private Target ParseTarget()
    {
        const bool log = false;
        Log($"[ParseTarget] ENTER. Current: {Peek()}", log);

        // Controlla se il target è una macro.
        if (Match(TokenKind.Ampersand))
        {
            var macroToken = Previous();
            var macroExpression = new MacroExpr(ParsePrimary(), macroToken);
            var macroTarget = new MacroTarget(macroExpression);
            Log($"[ParseTarget] EXIT. Parsed MacroTarget: '{macroTarget}'. Next: {Peek()}", log);
            return macroTarget;
        }

        // FASE 1: Parsa l'atomo base del target (es. il nome della variabile 'DATI').
        var expr = ParsePrimaryAtom();
        Log($"[ParseTarget]   -> Atomo base parsato: {expr.GetType().Name} ('{expr}')", log);

        // --- INIZIO BLOCCO DI CORREZIONE ---
        // Se l'atomo base è una variabile ed è immediatamente seguito da parentesi vuote `()`,
        // lo interpretiamo come un riferimento all'intero array e terminiamo subito.
        // Questo è il modo corretto di gestire la sintassi `NomeArray()` per passarla a una funzione.
        if (expr is VariableExpr baseVar && Check(TokenKind.LeftParen) && At(1).Kind == TokenKind.RightParen)
        {
            Advance(); // Consuma '('
            Advance(); // Consuma ')'
            Log(
                $"[ParseTarget]   -> Riconosciuta sintassi 'Nome()' come riferimento ad array. Creo VariableTarget e termino.",
                log);
            // Non si prosegue nel loop di chaining. Restituiamo direttamente il VariableTarget.
            return new VariableTarget(baseVar.SymbolId, baseVar.NameSuffixed, baseVar.NameCleaned);
        }
        // --- FINE BLOCCO DI CORREZIONE ---

        // FASE 2: Applica la logica di chaining per costruire il target complesso (per es. a.b(i))
        while (true)
        {
            if (Match(TokenKind.Dot, TokenKind.Arrow))
            {
                var memberToken = Peek();
                if (!IsPermissiveNameToken(memberToken))
                {
                    throw CreateError(memberToken, "Expected a member name after '.' or '->'.");
                }

                Advance(); // Il token è valido, consumalo.
                expr = new FieldAccessExpr(expr, memberToken);
                Log($"[ParseTarget]   -> Chaining with DOT/ARROW. Nuovo expr: FieldAccessExpr ('{expr}')", log);
            }
            else if (Match(TokenKind.LeftParen))
            {
                var args = ParseExpressionList(TokenKind.RightParen);
                Consume(TokenKind.RightParen, "Expected ')' after arguments.");

                if (expr is VariableExpr ve)
                {
                    expr = new ArrayAccessExpr(ve.SymbolId, ve.NameSuffixed, ve.NameCleaned, args);
                    Log($"[ParseTarget]   -> Chaining with '()'. Nuovo expr: ArrayAccessExpr ('{expr}')", log);
                }
                else
                {
                    expr = new IndexedAccessExpr(expr, args);
                    Log($"[ParseTarget]   -> Chaining with '()'. Nuovo expr: IndexedAccessExpr ('{expr}')", log);
                }
            }
            else if (Match(TokenKind.LeftBracket))
            {
                var indices = ParseExpressionList(TokenKind.RightBracket);
                Consume(TokenKind.RightBracket, "Expected ']' after indices.");
                expr = new IndexedAccessExpr(expr, indices);
                Log($"[ParseTarget]   -> Chaining with '[]'. Nuovo expr: IndexedAccessExpr ('{expr}')", log);
            }
            else
            {
                break;
            }
        }

        // FASE 3: Converte l'espressione finale (che rappresenta l'L-value) in un Target.
        var target = ParseTargetFromExpr(expr);
        Log($"[ParseTarget] EXIT. Created Target: {target.GetType().Name} ('{target}'). Next: {Peek()}", log);
        return target;
    }

    /// <summary>
    /// [FASE 1] Converte ricorsivamente un'espressione AST leggibile (`Expr`)
    /// nel suo equivalente target di assegnazione scrivibile (`Target`).
    /// Questo metodo non esegue alcuna risoluzione semantica.
    /// </summary>
    /// <param name="expr">Il nodo `Expr` da convertire. Deve rappresentare un L-value valido.</param>
    /// <returns>Un nuovo nodo `Target` che mantiene i SymbolId fittizi (-1).</returns>
    private static Target ParseTargetFromExpr(Expr expr)
    {
        return expr switch
        {
            // Copia anche lo ScopeId del VariableExpr: senza, il target di un `++`/`--` (o di un
            // accesso convertito) nasce con ScopeId -1 e il Resolver lo risolve nel posto
            // SBAGLIATO (es. `param--` in un metodo di classe → symbol scorrelato dal parametro,
            // l'IL lo trattava come un locale).
            VariableExpr v => new VariableTarget(v.SymbolId, v.NameSuffixed, v.NameCleaned) { ScopeId = v.ScopeId },
            ArrayAccessExpr a => new ArrayElementTarget(a.SymbolId, a.NameSuffixed, a.NameCleaned, a.Indices),
            FieldAccessExpr f => new FieldTarget(ParseTargetFromExpr(f.Structure), -1, f.Field),
            IndexedAccessExpr i => new IndexedTarget(i.Base, i.Indices),
            // WITH block implicit member access: .Member = value
            WithMemberAccessExpr w => new FieldTarget(null, -1, w.MemberNameToken),
            // Le chiamate a metodo non sono target validi in Fase 1
            MethodCallExpr => throw CreateError(
                new Token(TokenKind.Unknown, expr.ToString(), null, expr.Position),
                "A method call cannot be the target of an assignment."),
            FunctionExpr or BuiltinFunctionCallExpr or UnresolvedCallExpr => throw CreateError(
                new Token(TokenKind.Unknown, expr.ToString(), null, expr.Position),
                "A function call cannot be the target of an assignment."),
            _ => throw CreateError(
                new Token(TokenKind.Unknown, expr.ToString(), null, expr.Position),
                "Invalid expression as an assignment target.")
        };
    }

    /// <summary>
    /// Parsa una lista di destinazioni (variabili, array) separate da virgola.
    /// </summary>
    /// <returns>Un array immutabile di <see cref="Target"/>.</returns>
    private ImmutableArray<Target> ParseTargetList()
    {
        var targets = ImmutableArray.CreateBuilder<Target>();
        do
        {
            targets.Add(ParseTarget());
        } while (Match(TokenKind.Comma));

        return targets.ToImmutable();
    }

    /// <summary>
    /// [FASE 1] Parsa una lista di parametri per una FUNCTION o SUB, senza registrare i SymbolId.
    /// Supporta i modificatori OPTIONAL, BYREF/BYVAL e il nuovo PARAMARRAY per parametri variadici (TODO #80).
    /// </summary>
    /// <returns>Un array immutabile di nodi <see cref="Parameter"/>.</returns>
    private ImmutableArray<Parameter> ParseParameters()
    {
        const bool log = false;
        Log($"[ParseParameters] ENTER. Current: {Peek()}", log);

        Consume(TokenKind.LeftParen, "Expected '(' after procedure name.");
        var parameters = ImmutableArray.CreateBuilder<Parameter>();

        if (Check(TokenKind.RightParen))
        {
            Advance();
            Log("[ParseParameters] EXIT. Empty parameter list.", log);
            return parameters.ToImmutable();
        }

        var inOptionalSection = false;
        var hasParamArray = false;

        do
        {
            // 1. Controllo validità posizionale: nulla può seguire un PARAMARRAY.
            if (hasParamArray)
            {
                throw CreateError(Peek(),
                    "Il parametro variadico 'PARAMARRAY' deve essere l'ultimo parametro nella firma della procedura.");
            }

            // 2. Parsing dei modificatori.
            var isParamArray = Match(TokenKind.ParamArray);
            var isOptional = !isParamArray && Match(TokenKind.Optional);

            if (isOptional)
            {
                inOptionalSection = true;
            }
            else if (inOptionalSection && !isParamArray)
            {
                throw CreateError(Peek(), "Un parametro obbligatorio non può seguire un parametro opzionale.");
            }

            var isByRef = false;
            if (!isParamArray)
            {
                isByRef = Match(TokenKind.ByRef);
                if (!isByRef) Match(TokenKind.ByVal); // BYVAL è il default.
            }

            // 2bis. Modalità IN/OUT/INOUT (ADR-MATRIX-Valore §3). OUT/INOUT non sono
            // keyword globali: sono riconosciute QUI solo se seguite da un nome di
            // parametro (non da AS/','/')'/'='), così un parametro chiamato `out` resta
            // valido. IN usa il TokenKind.In già esistente (le keyword non possono essere
            // nomi di parametro).
            var mode = ParameterMode.In;
            if (!isParamArray)
            {
                /* `IN` è un modificatore solo se seguito da un ALTRO nome: `in() AS DOUBLE`
                 * è un NOME di parametro (firme FFI reali, es. gfx2d.inc). Prima veniva
                 * consumato sempre e il parser falliva con «'(' è una keyword riservata …». */
                if (Check(TokenKind.In)
                    && At(1).Kind != TokenKind.As && At(1).Kind != TokenKind.Comma
                    && At(1).Kind != TokenKind.RightParen && At(1).Kind != TokenKind.LeftParen
                    && At(1).Kind != TokenKind.Equal)
                {
                    Advance();
                    mode = ParameterMode.In;
                }
                else if (Check(TokenKind.Identifier)
                         && (At(0).Lexeme.Equals("OUT", StringComparison.OrdinalIgnoreCase)
                             || At(0).Lexeme.Equals("INOUT", StringComparison.OrdinalIgnoreCase))
                         && At(1).Kind != TokenKind.As && At(1).Kind != TokenKind.Comma
                         && At(1).Kind != TokenKind.RightParen && At(1).Kind != TokenKind.LeftParen
                         && At(1).Kind != TokenKind.Equal)
                {
                    mode = At(0).Lexeme.Equals("INOUT", StringComparison.OrdinalIgnoreCase)
                        ? ParameterMode.InOut : ParameterMode.Out;
                    Advance();
                }
            }

            // 3. Parsing del nome e normalizzazione.
            /* Un parametro può chiamarsi `in` (parola riservata ma nome C reale nelle firme
             * FFI, es. `in() AS DOUBLE` in gfx2d.inc): in posizione di nome è accettato. */
            var originalNameToken = Check(TokenKind.In) ? Advance() : ConsumeProcedureName();
            var (baseName, _) = SplitNameAndSuffix(originalNameToken.Lexeme);
            var normalizedNameToken = originalNameToken with { Lexeme = baseName };
            _declaredParameterNames.Add(baseName);   // vedi ParsePrimaryAtom: shadowing delle keyword a 0 argomenti

            // 4. Controllo per array: parentesi vuote `()` = rank 1. Il rank delle matrici
            // è nel tipo (`AS MATRIX OF (…, T)`), non nelle virgole dei parametri.
            var isArray = false;
            if (Match(TokenKind.LeftParen))
            {
                Consume(TokenKind.RightParen, "Expected ')' to close empty parentheses for array parameter.");
                isArray = true;
            }

            // Validazione specifica per PARAMARRAY: deve essere dichiarato come array (es. Variabile()).
            if (isParamArray)
            {
                if (!isArray)
                    throw CreateError(originalNameToken,
                        "Un parametro 'PARAMARRAY' deve essere dichiarato come un array utilizzando la sintassi 'Nome()'.");
                hasParamArray = true;
            }

            // Se è un array normale (non ParamArray), forziamo il passaggio per riferimento.
            if (isArray && !isParamArray)
            {
                isByRef = true;
            }

            // 5. Parsing della clausola di tipo 'AS'.
            bool hasAsClause = false;
            DataType baseDataType;
            if (Match(TokenKind.As))
            {
                hasAsClause = true;
                baseDataType = ParseTypeName();
            }
            else
            {
                baseDataType = DeduceTypeFromSuffix(originalNameToken.Lexeme);
            }

            DataType finalDataType;
            if (baseDataType is ArrayType baseArray)
            {
                if (baseArray.ElementType is ArrayType)
                    throw CreateError(originalNameToken,
                        "ARRAY OF ARRAY non è supportato: usa AS ARRAY OF T o AS MATRIX OF (assi…, T).");
                /* `v() AS ARRAY OF T` = parametro rank-1 (le `()` indicano solo il
                 * passaggio per riferimento implicito, non un array annidato). */
                finalDataType = baseArray;
            }
            else
            {
                finalDataType = isArray ? new ArrayType(baseDataType, 1) : baseDataType;
            }

            // 6. Parsing del valore di default (vietato per PARAMARRAY).
            Expr? defaultValue = null;
            if (Match(TokenKind.Equal))
            {
                if (isParamArray)
                    throw CreateError(Previous(), "Un parametro 'PARAMARRAY' non può avere un valore di default.");
                if (!isOptional)
                    throw CreateError(Previous(),
                        "Il valore di default può essere specificato solo per i parametri opzionali.");

                defaultValue = ParseExpression();
            }

            // 7. Costruzione dei flag e aggiunta del parametro.
            var flags = SymbolFlags.None;
            if (isByRef) flags |= SymbolFlags.IsByRef;
            if (isOptional) flags |= SymbolFlags.IsOptional;
            if (isArray) flags |= SymbolFlags.IsArray;
            if (isParamArray) flags |= SymbolFlags.IsParamArray;

            parameters.Add(
                new Parameter(-1, normalizedNameToken, finalDataType, flags, defaultValue, hasAsClause)
                {
                    OriginalName = originalNameToken,
                    Mode = mode
                });
        } while (Match(TokenKind.Comma));

        Consume(TokenKind.RightParen, "Expected ')' after parameters.");
        Log($"[ParseParameters] EXIT. Parsed {parameters.Count} parameters. Next: {Peek()}", log);
        return parameters.ToImmutable();
    }

    /// <summary>
    /// Helper riutilizzabile per parsare una lista di argomenti (espressioni) racchiusi tra parentesi.
    /// Gestisce argomenti posizionali, la sintassi speciale dei file handle (#) e il nuovo supporto
    /// per gli argomenti nominali tramite l'operatore ':=' (TODO #81).
    /// </summary>
    /// <returns>Un array immutabile di nodi <see cref="Expr"/> che rappresentano gli argomenti della chiamata.</returns>
    private ImmutableArray<Expr> ParseCommandArgumentList()
    {
        // Forma comando senza parentesi: gli argomenti separati da virgole fino alla fine
        // dell'istruzione (es. `ALERT msg` — msg è l'argomento).
        var args = new List<Expr>();
        if (!IsAtEndOfStatement())
        {
            do
            {
                args.Add(ParseExpression());
            } while (Match(TokenKind.Comma) && !IsAtEndOfStatement());
        }
        return [.. args];
    }

    private ImmutableArray<Expr> ParseArgumentListInParentheses()
    {
        Consume(TokenKind.LeftParen, "Expected '(' to begin argument list.");
        var args = new List<Expr>();

        if (!Check(TokenKind.RightParen))
        {
            do
            {
                // Supporto per il passaggio esplicito di parametri (TODO #41)
                // Consuma i modificatori BYREF o BYVAL se presenti prima dell'argomento.
                Match(TokenKind.ByRef, TokenKind.ByVal);

                // Rilevamento argomenti nominali (es. Altezza := 100).
                // Se il token corrente è un nome valido e il successivo è ':=', parsa come NamedArgumentExpr.
                if (IsValidNameToken(Peek()) && At(1).Kind == TokenKind.InlineAssign)
                {
                    var nameToken = Advance();
                    Advance(); // Consuma il token ':=' (InlineAssign)
                    var valueExpr = ParseExpression();
                    args.Add(new NamedArgumentExpr(nameToken, valueExpr));
                }
                else
                {
                    // Gestisce la sintassi speciale '#filenum' per le funzioni di I/O legacy.
                    // Il Match consuma il token '#' ma non fa parte dell'espressione.
                    Match(TokenKind.Hash);

                    // Parsa l'espressione dell'argomento posizionale.
                    args.Add(ParseExpression());
                }
            } while (Match(TokenKind.Comma));
        }

        Consume(TokenKind.RightParen, "Expected ')' to end argument list.");
        return [.. args];
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni che iniziano con una keyword, come GO, NEW, FETCH, BASE, SELF.
    /// CORRETTO: Utilizza ParseArgumentListInParentheses.
    /// </summary>
    private Expr? ParseKeywordExpression()
    {
        if (Match(TokenKind.Go))
        {
            Consume(TokenKind.Function, "Expected 'FUNCTION' after 'GO'.");
            var functionNameToken = Consume(TokenKind.Identifier, "Expected a function name after GO FUNCTION.");
            var args = ParseArgumentListInParentheses();
            Expr? waitGroup = null;
            if (Match(TokenKind.In)) waitGroup = ParseExpression();
            return new GoFunctionExpr(-1, functionNameToken, args, waitGroup, new FutureType());
        }

        if (Match(TokenKind.New)) return ParseNewExpression();

        if (Match(TokenKind.Fetch))
        {
            // Rimuovo le parentesi ridondanti che venivano aggiunte qui.
            return ParseFetchExpression();
        }

        if (Match(TokenKind.Base)) return new BaseExpr(Previous());
        return Match(TokenKind.Self) ? new VariableExpr(-1, Previous(), Previous()) : null;
    }

    /// <summary>
    /// Parsa un'espressione di istanziazione, come `NEW NOMETIPO [OF ...] [{...}]`.
    /// </summary>
    /// <remarks>
    /// Questa funzione è il punto di ingresso per la creazione di tutti gli oggetti complessi
    /// e le collezioni native in GM-BASIC. Analizza il tipo di dato completo, che può
    /// includere clausole come `OF` per i tipi generici (collezioni, canali), e una
    /// lista di inizializzazione opzionale racchiusa tra parentesi graffe `{}` per le collezioni.
    /// </remarks>
    /// <example>
    /// Sintassi gestite da questo parser:
    /// <code>
    /// ' Oggetti di sistema
    /// DIM wg AS WAITGROUP
    /// wg = NEW WAITGROUP
    ///
    /// DIM opts AS HTTPOPTIONS
    /// opts = NEW HTTPOPTIONS
    ///
    /// ' Oggetti di concorrenza (con clausola OF)
    /// DIM messages AS CHANNEL OF STRING
    /// messages = NEW CHANNEL OF STRING
    ///
    /// ' Collezioni con tipo a singolo elemento
    /// DIM names AS LIST OF STRING
    /// names = NEW LIST OF STRING
    ///
    /// DIM numbers AS HASHSET OF DOUBLE
    /// numbers = NEW HASHSET OF DOUBLE
    /// 
    /// ' Collezioni con lista di inizializzazione
    /// DIM items AS QUEUE OF INTEGER
    /// items = NEW QUEUE OF INTEGER {10, 20, 30}
    ///
    /// DIM history AS STACK OF STRING
    /// history = NEW STACK OF STRING {"start", "middle", "end"}
    ///
    /// ' Collezioni chiave-valore (con clausola OF a due tipi)
    /// DIM prices AS DICT OF STRING, DOUBLE
    /// prices = NEW DICT OF STRING, DOUBLE
    ///
    /// DIM sortedIndex AS SORTED OF LONG, STRING
    /// sortedIndex = NEW SORTED OF LONG, STRING
    /// </code>
    /// </example>
    /// <returns>Un nodo AST <see cref="NewExpr"/>.</returns>
    private NewExpr ParseNewExpression()
    {
        // 1. Il token 'NEW' è già stato consumato da ParseKeywordExpression.
        var newToken = Previous();

        // 2. Chiama il parser dei tipi di Fase 1.
        var parsedType = ParseTypeName();

        // 3. Controlla la presenza di una lista di argomenti del costruttore opzionale (es. (...)).
        ImmutableArray<Expr>? constructorArgs = null;
        if (Match(TokenKind.LeftParen))
        {
            // Se la parentesi è vuota, è una lista di argomenti vuota.
            constructorArgs = !Check(TokenKind.RightParen)
                ? ParseExpressionList(TokenKind.RightParen)
                : ImmutableArray<Expr>.Empty;

            Consume(TokenKind.RightParen, "Expected ')' after constructor arguments.");
        }

        // 4. Controlla la presenza di una lista di inizializzazione opzionale (es. {...} o [...]).
        ImmutableArray<Expr>? initialValues = null;
        if (Match(TokenKind.LeftBrace))
        {
            initialValues = ParseExpressionList(TokenKind.RightBrace);
            Consume(TokenKind.RightBrace, "Expected '}' to close collection initializer list.");
        }
        else if (Match(TokenKind.LeftBracket))
        {
            initialValues = ParseExpressionList(TokenKind.RightBracket);
            Consume(TokenKind.RightBracket, "Expected ']' to close collection initializer list.");
        }

        // 5. Validazione: non si possono avere sia argomenti di costruttore che inizializzatori di collezione.
        if (constructorArgs.HasValue && initialValues.HasValue)
        {
            throw CreateError(newToken,
                "Cannot specify both constructor arguments '(...)' and a collection initializer '{...}' or '[...]' in a NEW expression.");
        }

        // 6. Crea il nuovo nodo AST con tutte le informazioni parsate.
        return new NewExpr(parsedType, constructorArgs, initialValues, newToken)
            { SourceLineIndex = _currentSourceLineIndex };
    }

    /// <summary>
    /// [FASE 1] Parsa la sintassi complessa della funzione FETCH.
    /// </summary>
    private HttpFetchExpr ParseFetchExpression()
    {
        Consume(TokenKind.LeftParen, "Expected '(' after FETCH.");
        var url = ParseExpression();
        Expr? options = null;
        Token? callbackName = null;
        if (Match(TokenKind.Comma))
        {
            if (At(1).Kind == TokenKind.LeftParen && At(2).Kind == TokenKind.RightParen)
            {
                callbackName = Consume(TokenKind.Identifier, "Expected callback subroutine name.");
                Consume(TokenKind.LeftParen, "Expected '('.");
                Consume(TokenKind.RightParen, "Expected ')'.");
            }
            else
            {
                options = ParseExpression();
                if (Match(TokenKind.Comma))
                {
                    callbackName = Consume(TokenKind.Identifier, "Expected callback subroutine name.");
                    Consume(TokenKind.LeftParen, "Expected '('.");
                    Consume(TokenKind.RightParen, "Expected ')'.");
                }
            }
        }

        Consume(TokenKind.RightParen, "Expected ')' to close FETCH arguments.");
        if (callbackName is not null)
        {
            throw CreateError(callbackName,
                "A callback is only allowed when FETCH is used as a standalone statement.");
        }

        return new HttpFetchExpr(url, options, callbackName);
    }

    /// <summary>
    /// [FASE 1] Parsa una funzione anonima (lambda) senza registrare i suoi parametri nella SymbolTable.
    /// QUESTA VERSIONE RIPRISTINA LA STRUTTURA CORRETTA CHE UTILIZZA ParseBlockBodyUntil e aggiunge log.
    /// </summary>
    /// <remarks>
    /// Sintassi Lambda Supportate:
    /// 1. Espressione singola (arrow function):
    ///    LET miaLambda = FN(x) => x * 2
    ///
    /// 2. Blocco con parentesi graffe (stile moderno):
    ///    LET miaLambda = SUB(x) {
    ///        PRINT "Valore:", x
    ///    }
    ///
    /// 3. Blocco tradizionale con due punti (stile legacy):
    ///    LET miaLambda = FN(x) AS INTEGER:
    ///        DIM risultato = x + 10
    ///        RETURN risultato
    ///    ENDFN
    ///
    /// Nota: La keyword 'SUB' è un alias sintattico per 'FN' quando non viene specificato un tipo di ritorno con 'AS'.
    /// 'ENDFN' è un alias per 'END FUNCTION' in questo contesto.
    /// </remarks>
    private LambdaExpr ParseLambdaExpression(Token funcToken)
    {
        const bool log = false;
        Log($"[DEBUG] >>> ENTER ParseLambdaExpression. Current: {Peek()}", log);

        var parametersSyntax = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;
        // Bug Lambda-Arrow-ArrayLiteral-Indicizzato-IL (AiWiki, trovato 2026-09-16): ParseParameters
        // è condivisa con SUB/FUNCTION nominate, dove un parametro senza "AS" deduce comunque un
        // tipo CONCRETO (DoubleType di default, DeduceTypeFromSuffix) — corretto per una procedura
        // nominata, ma per una lambda ARROW/blocco (FN(t, y) => ...) questo default concreto
        // impedisce la propagazione del tipo REALE dal contesto chiamante: RewriteSingleArgument/
        // LambdaScope (Linker.Rewrites.Expressions.cs) propaga il FunctionPointerType del callback
        // SOLO quando "param.DataType is InferredType" — con DoubleType già assegnato qui, quel
        // controllo fallisce sempre e un parametro realmente ARRAY (es. `y` in un callback ODE)
        // resta scambiato per uno scalare, producendo CIL non verificabile o un tipo InferredType
        // mai risolto. La forma CODEBLOCK `{|t, y| ...}` (ParseCodeBlockBody sotto) non passa da
        // ParseParameters e non soffriva di questo. Riporta a InferredType ogni parametro SENZA
        // "AS" esplicito e SENZA "()" di array esplicito (che già porta un segnale di tipo più
        // forte, lasciato invariato) — solo per una lambda, mai per una procedura nominata.
        parametersSyntax = parametersSyntax.Select(p =>
            !p.HasAsClause && (p.Flags & SymbolFlags.IsArray) == 0
                ? p with { DataType = new InferredType() }
                : p
        ).ToImmutableArray();
        Log($"[DEBUG]     Parsed parameters. Next: {Peek()}", log);

        var hasExplicitReturnType = Check(TokenKind.As);
        // Return type NON esplicito → InferredType (il Linker lo deduce dal corpo): prima
        // era DoubleType (DeduceTypeFromSuffix del default numerico) → il delegate delle
        // lambda diventava Func<decimal,decimal,double> anche per `acc + v` su CURRENCY
        // (AREDUCE<decimal> richiede il return decimal → IL invalido).
        var returnType = hasExplicitReturnType ? ParseTypeClause(funcToken) : new InferredType();
        Log($"[DEBUG]     Parsed return type '{DataTypeToString(returnType)}'. Next: {Peek()}", log);

        if (Match(TokenKind.FatArrow))
        {
            Log("[DEBUG]     Matched '=>'. Parsing expression body...", log);
            var expressionBody = ParseExpression();
            // SUB(x) => ... : la parola chiave SUB marca una lambda VOID (come la VM).
            // FN(x) => <confronto> : il corpo booleano marca il ReturnType BIT (usato da
            // ACOUNT/AFILTER — il delegate atteso è Func<T,bool>, mai Func<T,double>).
            if (!hasExplicitReturnType)
            {
                if (funcToken.Lexeme.Equals("SUB", StringComparison.OrdinalIgnoreCase))
                    returnType = new VoidType();
                else if (expressionBody is BinaryExpr { Op.Kind: TokenKind.Equal or TokenKind.NotEqual
                            or TokenKind.LessThan or TokenKind.LessEqual or TokenKind.GreaterThan
                            or TokenKind.GreaterEqual or TokenKind.ExactlyEqual or TokenKind.Is or TokenKind.In }
                    or UnaryExpr { Op.Kind: TokenKind.Not })
                    returnType = new BitType();
            }
            var result = new LambdaExpr(parametersSyntax, returnType, null, expressionBody, funcToken);
            Log($"[DEBUG] <<< EXIT ParseLambdaExpression (Arrow). Next: {Peek()}", log);
            return result;
        }

        var finalParameters = parametersSyntax.Select(p => p with { SymbolId = -1 }).ToImmutableArray();

        var dummyProcedureContext = new FunctionDefinition(
            -1,
            new Token(TokenKind.Identifier, $"__lambda_{funcToken.Position.Line}", null, funcToken.Position),
            finalParameters,
            returnType,
            ImmutableArray<Stmt>.Empty,
            SymbolFlags.None
        );

        ImmutableArray<Stmt> blockBody;

        if (Match(TokenKind.LeftBrace))
        {
            Log("[DEBUG]     Matched '{'. Calling ParseBlockBodyUntil with stop token 'RightBrace'.", log);
            blockBody = ParseBlockBodyUntil(dummyProcedureContext, TokenKind.RightBrace);
            Log($"[DEBUG]     Returned from ParseBlockBodyUntil. Parsed {blockBody.Length} statements. Next: {Peek()}",
                log);
            Consume(TokenKind.RightBrace, "Expected '}' to close lambda body.");
        }
        else
        {
            Consume(TokenKind.Colon, "Expected ':', '{', or '=>' after lambda signature.");
            Log("[DEBUG]     Matched ':'. Calling ParseBlockBodyUntil with stop token 'EndFunction'.", log);
            blockBody = ParseBlockBodyUntil(dummyProcedureContext, TokenKind.EndFunction);
            Log($"[DEBUG]     Returned from ParseBlockBodyUntil. Parsed {blockBody.Length} statements. Next: {Peek()}",
                log);
            Consume(TokenKind.EndFunction, "Expected 'END FUNCTION' or 'END FN' to close the lambda block.");
        }

        var blockResult = new LambdaExpr(finalParameters, returnType, blockBody, null, funcToken);
        Log($"[DEBUG] <<< EXIT ParseLambdaExpression (Block). Next: {Peek()}", log);
        return blockResult;
    }

    /// <summary>
    /// [FASE 1 - NUOVO] Parsa un Code Block in stile Clipper (es. { |param| corpo })
    /// e lo trasforma in un nodo LambdaExpr standard.
    /// Viene chiamato dopo che la parentesi graffa aperta '{' è stata consumata
    /// e il lookahead ha confermato la presenza di '|'.
    /// </summary>
    /// <param name="openBrace">Il token '{' che ha iniziato il blocco, usato per la posizione.</param>
    /// <returns>Un nodo AST <see cref="LambdaExpr"/> che rappresenta il code block.</returns>
    private LambdaExpr ParseCodeBlockBody(Token openBrace)
    {
        var parameters = ImmutableArray.CreateBuilder<Parameter>();
        // `{ || corpo }`: parametri VUOTI. Il lexer emette BitOr per la doppia barra
        // (è anche l'operatore OR bit a bit in espressione), quindi qui la lista vuota
        // non ha né la Bar di apertura né quella di chiusura da consumare.
        var emptyParams = Match(TokenKind.BitOr);
        if (!emptyParams)
        {
            Consume(TokenKind.Bar, "Expected '|' to start code block parameter list.");
        }
        else
        {
            // niente parametri: si va direttamente al corpo
        }
        if (!emptyParams && !Check(TokenKind.Bar)) // Controlla se ci sono parametri
        {
            do
            {
                var nameToken = Consume(TokenKind.Identifier, "Expected parameter name in code block.");

                // NUOVA LOGICA: Controlla la clausola AS opzionale per il tipo esplicito.
                var hasAsClause = Match(TokenKind.As);
                var paramType = hasAsClause
                    ? ParseTypeName()
                    : // Usa il parser di tipi esistente.
                    new InferredType(); // Mantiene il comportamento di fallback per l'inferenza.

                var param = new Parameter(
                    -1,
                    nameToken,
                    paramType,
                    SymbolFlags.IsParameter, // I parametri sono sempre per valore di default
                    null,
                    hasAsClause)
                {
                    OriginalName = nameToken
                };
                parameters.Add(param);
            } while (Match(TokenKind.Comma));
        }

        if (!emptyParams)
        {
            Consume(TokenKind.Bar, "Expected '|' to end code block parameter list.");
        }

        // Corpo del code block: PRIMA gli statement (RETURN/IF/LET/assegnazioni — un `=` a
        // livello statement È un'assegnazione, mai un confronto), POI, se lo statement non
        // consuma fino al terminatore, un'ESPRESSIONE nuda = ritorno implicito Clipper 5.3
        // (`{ |x| x + 1 }` ≡ `{ |x| RETURN x + 1 }`, `{ || 3.14 }` → 3.14). L'ordine inverso
        // (espressione-first) interpretava `{ |x| r(i) = v }` come confronto di uguaglianza,
        // perdendo l'assegnazione — la grammatica degli statement disambigua correttamente.
        var dummyProc = new FunctionDefinition(-1, openBrace, parameters.ToImmutable(), new InferredType(), [],
            SymbolFlags.None);
        var body = new List<Stmt>();
        var savedBlockFlag = _insideMultiLineBlock;
        _insideMultiLineBlock = true;
        try
        {
            while (!Check(TokenKind.RightBrace) && !IsAtEnd())
            {
                /* Separatori di statement in un code block: newline, ':' e — DENTRO le
                 * graffe — DictColon (il lexer emette DictColon per ':' dentro '{...}',
                 * vedi ConsumeColonOrDictColon): `{ |x| c = c + x : RETURN c }` senza
                 * DictColon falliva con "Expected '}' after expression in code block". */
                if (Match(TokenKind.Newline) || Match(TokenKind.Colon) || Match(TokenKind.DictColon)) continue;

                var positionBeforeParse = _current;
                var parsedOk = false;
                try
                {
                    var parsedNodes = ParseStatement(dummyProc);
                    // Uno statement valido termina su un separatore o sul '}' di chiusura.
                    if (Check(TokenKind.Newline) || Check(TokenKind.Colon) || Check(TokenKind.DictColon) ||
                        Check(TokenKind.RightBrace) || IsAtEnd())
                    {
                        foreach (var node in parsedNodes)
                            if (node is Stmt stmtNode)
                                body.Add(stmtNode);
                        parsedOk = true;
                    }
                }
                catch (BasicException)
                {
                    // Non è uno statement: si tenta sotto come espressione (ritorno implicito).
                }

                if (parsedOk) continue;

                _current = positionBeforeParse;
                var valueToken = Peek();
                var exprBody = ParseExpression();
                if (!Check(TokenKind.RightBrace))
                    throw CreateError(Peek(), "Expected '}' after expression in code block.");
                body.Add(new ReturnStmt(exprBody, valueToken));
            }
        }
        finally
        {
            _insideMultiLineBlock = savedBlockFlag;
        }

        Consume(TokenKind.RightBrace, "Expected '}' to close the code block.");
        return new LambdaExpr(parameters.ToImmutable(), new InferredType(), body.ToImmutableArray(), null, openBrace);
    }

    /// <summary>
    /// Converte un Target in un'espressione equivalente.
    /// Funzione helper per ParseTarget, necessaria per le assegnazioni a catena.
    /// </summary>
    private static Expr ConvertTargetToExpr(Target target)
    {
        return target switch
        {
            VariableTarget vt => new VariableExpr(vt.SymbolId, vt.NameSuffixed, vt.NameCleaned),
            ArrayElementTarget aet => new ArrayAccessExpr(aet.SymbolId, aet.NameSuffixed, aet.NameCleaned, aet.Indices),
            FieldTarget ft => new FieldAccessExpr(ConvertTargetToExpr(ft.Structure!), ft.OriginalToken),
            IndexedTarget it => new IndexedAccessExpr(it.Base, it.Indices),
            _ => throw new InvalidOperationException("Conversione da Target a Expr non supportata per questo tipo.")
        };
    }

    /// <summary>
    /// Consuma ':' sia come Colon che come DictColon (dentro le graffe il lexer produce DictColon).
    /// </summary>
    private void ConsumeColonOrDictColon()
    {
        if (Check(TokenKind.Colon) || Check(TokenKind.DictColon))
        {
            Advance();
        }
        else
        {
            throw CreateError(Peek(), "Expected ':'.");
        }
    }

    /// <summary>
    /// [FASE 1] Parsa un'espressione SWITCH.
    /// Sintassi: SWITCH(expr) { CASE val: result, CASE val: result, ELSE: default }
    /// </summary>
    private SwitchExpr ParseSwitchExpression(Token switchToken)
    {
        Consume(TokenKind.LeftParen, "Expected '(' after SWITCH.");
        var value = ParseExpression();
        Consume(TokenKind.RightParen, "Expected ')' after SWITCH value.");

        Consume(TokenKind.LeftBrace, "Expected '{' to start SWITCH cases.");

        var cases = ImmutableArray.CreateBuilder<SwitchCaseClause>();
        Expr? elseCase = null;

        while (!Check(TokenKind.RightBrace) && !IsAtEnd())
        {
            while (Match(TokenKind.Newline) || Match(TokenKind.Comma)) { /* skip */ }
            if (Check(TokenKind.RightBrace)) break;

            if (Match(TokenKind.Else))
            {
                ConsumeColonOrDictColon();
                elseCase = ParseExpression();
                break;
            }

            Consume(TokenKind.Case, "Expected 'CASE' inside SWITCH expression.");
            var condition = ParseExpression();
            ConsumeColonOrDictColon();
            var result = ParseExpression();

            cases.Add(new SwitchCaseClause(condition, result));
        }

        Consume(TokenKind.RightBrace, "Expected '}' to close SWITCH expression.");

        return new SwitchExpr(value, cases.ToImmutable(), elseCase);
    }
}