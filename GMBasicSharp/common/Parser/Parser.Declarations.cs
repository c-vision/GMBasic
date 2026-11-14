// FILE: Interpreter.Parser.Declarations.cs

using System.Collections.Immutable;

// ReSharper disable SwitchStatementMissingSomeEnumCasesNoDefault

namespace Basic;

/// <summary>
/// Contiene i metodi del Parser per la Fase 1: Costruzione dell'AST Puro.
/// Questi metodi sono responsabili esclusivamente dell'analisi sintattica e della
/// creazione di un AST "spurio", senza alcuna interazione di scrittura con la SymbolTable.
/// Questa classe duplica e adatta solo i metodi il cui comportamento è specifico per la Fase 1,
/// riutilizzando gli helper di parsing puramente sintattici dalla classe Parser principale.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Parsa un'istruzione di dichiarazione (DIM, GLOBAL, LOCAL, STATIC, CONST, PRIVATE).
    /// Questo metodo gestisce le varie keyword di dichiarazione e i modificatori di visibilità opzionali.
    /// In Fase 1, crea i nodi AST con `SymbolId = -1` senza registrare simboli.
    /// La keyword `PRIVATE` viene trattata come una dichiarazione a sé stante per lo scope dinamico,
    /// distinta dal suo uso come modificatore di visibilità per i membri di una classe.
    /// </summary>
    /// <returns>Un nodo `Stmt` (`DeclarationStmt` o `ConstStmt`) che rappresenta l'intera istruzione.</returns>
    /// <example>
    /// <code>
    /// ' Dichiarazioni standard
    /// DIM A AS INTEGER, B$
    /// GLOBAL C AS DOUBLE
    /// 
    /// ' Dichiarazione con visibilità
    /// PUBLIC DIM X AS INTEGER
    /// 
    /// ' Dichiarazione a scope dinamico (nuova)
    /// PRIVATE cCurrentUser = "Admin"
    /// 
    /// ' Dichiarazione di costante
    /// CONST PI = 3.14159
    /// </code>
    /// </example>
    private Stmt ParseDeclarationStatement()
    {
        var flags = SymbolFlags.None;

        // Controlla se il primo token è un modificatore di visibilità.
        // DYNAMIC non è un modificatore, quindi non viene gestito qui.
        if (Match(TokenKind.Public))
        {
            flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
        }
        else if (Match(TokenKind.Private))
        {
            flags |= SymbolFlags.IsPrivate;
        }

        var keywordToken = Advance(); // Consuma DIM, GLOBAL, LOCAL, STATIC, CONST, o DYNAMIC
        Log($"[ParseDeclarationStatement] ENTER. Keyword: {keywordToken.Lexeme}, Current: {Peek()}");

        if (keywordToken.Kind == TokenKind.Const)
        {
            var constDeclarations = ParseConstDeclarationList(flags);
            var constStmt = new ConstStmt(keywordToken, constDeclarations);
            Log(
                $"[ParseDeclarationStatement] EXIT. Parsed ConstStmt with {constDeclarations.Length} declaration(s). Next: {Peek()}");
            return constStmt;
        }

        var isVar = keywordToken.Kind == TokenKind.Var;

        // Controlla se è una distrutturazione di tupla: DIM (x, y, z) = expr
        if (Check(TokenKind.LeftParen) && !isVar)
        {
            var savedPosition = _current;
            Advance(); // Consuma (

            // Verifica se sono identificatori separati da virgole
            var identifiers = ImmutableArray.CreateBuilder<Token>();
            if (Check(TokenKind.Identifier) || Check(TokenKind.Nil))
            {
                do
                {
                    identifiers.Add(Advance());
                } while (Match(TokenKind.Comma));

                if (Check(TokenKind.RightParen))
                {
                    Advance(); // Consuma )
                    if (Match(TokenKind.Equal))
                    {
                        var tupleExpression = ParseExpression();
                        // La registrazione dei simboli avviene nel Linker (fase di binding),
                        // dove lo scope locale corrente è quello corretto. Registrare qui
                        // userebbe lo scope 0 (module) e colliderebbe tra procedure diverse.
                        var tupleStmt = new TupleDestructuringStmt(identifiers.ToImmutable(), tupleExpression, keywordToken);
                        Log($"[ParseDeclarationStatement] EXIT. Parsed TupleDestructuringStmt with {identifiers.Count} variables. Next: {Peek()}");
                        return tupleStmt;
                    }
                }
            }

            // Non è una tupla — torna indietro
            _current = savedPosition;
        }

        var declarations = ParseDeclarationList(isVar, flags, _pendingAttributes);

        var declStmt = new DeclarationStmt(keywordToken, declarations);
        Log(
            $"[ParseDeclarationStatement] EXIT. Parsed DeclarationStmt with {declarations.Length} declaration(s). Next: {Peek()}");
        return declStmt;
    }

    /// <summary>
    /// [FASE 1 - CORRETTO E FINALIZZATO v2] Parsa la sintassi di una lista di dichiarazioni di variabili,
    /// normalizzando i nomi con suffisso di tipo (es. X$) prima di creare il nodo AST.
    /// Supporta array, 'lower TO upper', e flag di visibilità. Non registra simboli.
    /// </summary>
    /// <summary>
    /// True se la dichiarazione è certamente uno scalare: tipo primitivo esplicito oppure
    /// inizializzatore ovviamente scalare (numero/stringa/bool/nil, anche dentro un gruppo
    /// o un'operazione). Un nome non tipizzato senza inizializzatore resta "non scalare":
    /// può diventare un array/lista (es. `DIM voci` + STRINGS.SPLIT) e deve shadoware.
    /// </summary>
    private static bool IsDefinitelyScalarDeclaration(VariableDeclaration decl)
    {
        if (decl.DataType is ArrayType) return false;
        if (decl.DataType is not null and not InferredType)
            return decl.DataType is SingleType or DoubleType or CurrencyType or StringType
                or BitType or ByteType or SByteType or IntegerType or UIntegerType
                or LongType or ULongType or QuadType or UQuadType
                or DateType or TimeType or DateTimeType or TimeSpanType;
        return IsObviouslyScalarExpr(decl.InitialValue);
    }

    private static bool IsObviouslyScalarExpr(Expr? expr) => expr switch
    {
        null => false,
        NumberExpr or StringExpr or CheckedStringExpr or InterpolatedStringExpr
            or BooleanExpr or NilExpr => true,
        GroupingExpr g => IsObviouslyScalarExpr(g.Expression),
        UnaryExpr u => IsObviouslyScalarExpr(u.Right),
        BinaryExpr b => IsObviouslyScalarExpr(b.Left) && IsObviouslyScalarExpr(b.Right),
        _ => false
    };

    private ImmutableArray<VariableDeclaration> ParseDeclarationListSyntaxOnly(bool isVarDeclaration = false,
        SymbolFlags visibilityFlags = SymbolFlags.None,
        ImmutableArray<Token> attributes = default)
    {
        const bool log = false;
        var declarations = ImmutableArray.CreateBuilder<VariableDeclaration>();
        do
        {
            var originalNameToken = ConsumeProcedureName();
            Log($"[ParseDeclSyntaxOnly] Token originale consumato: '{originalNameToken.Lexeme}'", log);

            // --- LOGICA DI NORMALIZZAZIONE ---
            var (baseName, suffixType) = SplitNameAndSuffix(originalNameToken.Lexeme);
            var normalizedNameToken = originalNameToken with { Lexeme = baseName };
            Log(
                $"[ParseDeclSyntaxOnly]   -> Nome normalizzato: '{normalizedNameToken.Lexeme}', Tipo da suffisso: {suffixType}",
                log);
            // ---------------------------------

            var lowerBounds = ImmutableArray.CreateBuilder<Expr>();
            var upperBounds = ImmutableArray.CreateBuilder<Expr>();
            var isArrayDeclaration = false;

            if (Match(TokenKind.LeftParen))
            {
                isArrayDeclaration = true;
                if (!Check(TokenKind.RightParen))
                {
                    do
                    {
                        var firstExpr = ParseExpression();
                        if (Match(TokenKind.To))
                        {
                            var secondExpr = ParseExpression();
                            lowerBounds.Add(firstExpr);
                            upperBounds.Add(secondExpr);
                        }
                        else
                        {
                            lowerBounds.Add(
                                new NumberExpr(0, new Token(TokenKind.Number, "0", 0.0, firstExpr.Position)));
                            upperBounds.Add(firstExpr);
                        }
                    } while (Match(TokenKind.Comma));
                }

                Consume(TokenKind.RightParen, "Expected ')' after array dimensions.");
            }

            DataType? baseDataType = null;
            var hasAsClause = false;

            if (isVarDeclaration)
            {
                if (Check(TokenKind.As))
                    throw CreateError(Peek(), "Cannot use 'AS' clause in a 'VAR' declaration.");
                baseDataType = new InferredType();
            }
            else if (Match(TokenKind.As))
            {
                hasAsClause = true;
                baseDataType = ParseTypeName();
            }

            Expr? initialValue = null;
            if (Match(TokenKind.Equal))
            {
                initialValue = ParseExpression();
            }
            else if (isVarDeclaration)
            {
                throw CreateError(Peek(), "A 'VAR' declaration must be initialized.");
            }

            baseDataType ??= initialValue != null
                ? new InferredType()
                : suffixType ?? DeduceTypeFromSuffix(originalNameToken.Lexeme);
            Log($"[ParseDeclSyntaxOnly]   -> Tipo base determinato: {baseDataType}", log);

            DataType finalDataType;
            if (baseDataType is ArrayType baseArray)
            {
                /* Forma esplicita: `AS ARRAY OF T` (rank 1) oppure `AS MATRIX OF (I…, T)`.
                 * `ARRAY OF ARRAY` non esiste. Le parentesi sul nome dichiarano le
                 * ESTENSIONI dello stesso rank del tipo (DIM sugar), mai un array annidato:
                 * `DIM x(n) AS ARRAY OF T` è un ARRAY rank-1 di n+1 elementi. */
                if (baseArray.ElementType is ArrayType)
                    throw CreateError(originalNameToken, "ARRAY OF ARRAY non è supportato: usa AS ARRAY OF T o AS MATRIX OF (assi…, T).");
                if (isArrayDeclaration && upperBounds.Count > 0 && upperBounds.Count != baseArray.Rank)
                    throw CreateError(originalNameToken,
                        $"DIM {originalNameToken.Lexeme}: {upperBounds.Count} dimensioni dichiarate ma il tipo ha rank {baseArray.Rank}.");
                finalDataType = baseArray;
            }
            else
            {
                /* Zucchero BASIC: `DIM v(n) AS T` → ARRAY OF T; `DIM m(n,m) AS T` → MATRIX
                 * rank-2 (assi interi impliciti). Il rank viene dalla DICHIARAZIONE. */
                finalDataType = isArrayDeclaration ? new ArrayType(baseDataType, Math.Max(1, upperBounds.Count)) : baseDataType;
            }
            Log($"[ParseDeclSyntaxOnly]   -> Tipo finale (considerando array): {finalDataType}", log);

            // Usa il nuovo costruttore di VariableDeclaration.
            var declaration = new VariableDeclaration(
                -1,
                originalNameToken, // NameSuffixed
                normalizedNameToken, // NameCleaned
                upperBounds.ToImmutable(),
                finalDataType,
                initialValue,
                isArrayDeclaration && upperBounds.Count == 0,
                hasAsClause,
                visibilityFlags,
                _pendingAttributes)
            {
                LowerBounds = lowerBounds.ToImmutable(),
                DeclaredNamespace = _currentNamespace
            };

            Log($"[ParseDeclSyntaxOnly]   -> Creato VariableDeclaration: {declaration}", log);
            if (finalDataType is JsonType) _declaredJsonNames.Add(normalizedNameToken.Lexeme);
            declarations.Add(declaration);
        } while (Match(TokenKind.Comma));

        // `DIM a, b AS STRING`: l'`AS` della clausola FINALE si applica a TUTTE le variabili
        // della lista che non hanno un proprio `AS` né un suffisso di tipo (canone classico,
        // parity C99Native). Senza, `a` restava DOUBLE → `a = "x"` = errore di compilazione
        // (divergenza 4 backend: C99Native dava a/b entrambe STRING).
        if (declarations.Count > 1 && declarations[^1].HasAsClause
            && declarations[^1].DataType is { } sharedType && sharedType is not InferredType)
        {
            for (var k = 0; k < declarations.Count - 1; k++)
            {
                var d = declarations[k];
                if (d.HasAsClause) continue;
                var (_, suffix) = SplitNameAndSuffix(d.NameSuffixed.Lexeme);
                if (suffix is not null) continue; // es. `x$` → STRING dal suffisso
                declarations[k] = d with { DataType = sharedType };
            }
        }

        return declarations.ToImmutable();
    }

    /// <summary>
    /// [FASE 1] Parsa una lista di dichiarazioni di variabili, costruendo i nodi AST con `SymbolId = -1`.
    /// </summary>
    /// <summary>
    /// [FASE 1] Parsa una lista di dichiarazioni di variabili, costruendo i nodi AST con `SymbolId = -1`
    /// e propagando i flag di visibilità.
    /// </summary>
    private ImmutableArray<VariableDeclaration> ParseDeclarationList(bool isVarDeclaration = false,
        SymbolFlags visibilityFlags = SymbolFlags.None,
        ImmutableArray<Token> attributes = default)
    {
        Log(
            $"[ParseDeclarationList] ENTER. isVar={isVarDeclaration}, Flags={visibilityFlags}, Current: {Peek()}");
        var declarations = ImmutableArray.CreateBuilder<VariableDeclaration>();
        do
        {
            // Passa i flag di visibilità e gli attributes al parser di sintassi.
            var declsSyntax = ParseDeclarationListSyntaxOnly(isVarDeclaration, visibilityFlags, attributes);
            foreach (var decl in declsSyntax)
            {
                // Il SymbolId viene lasciato a -1 per il Linker.
                // I flag sono già stati impostati da ParseDeclarationListSyntaxOnly.
                _declaredVariableNames.Add(decl.NameSuffixed.Lexeme);
                if (decl.DataType is ArrayType)
                    _declaredArrayNames.Add(decl.NameSuffixed.Lexeme);
                // Ultima dichiarazione per nome: decide se una chiamata nuda omonima è
                // accesso a variabile (array/non tipizzata) o builtin (scalare sicuro).
                _declaredVariableIsScalar[decl.NameSuffixed.Lexeme] = IsDefinitelyScalarDeclaration(decl);
                declarations.Add(decl with { SymbolId = -1 });
                Log(
                    $"[ParseDeclarationList]   -> Parsed declaration for '{decl.NameSuffixed.Lexeme}' with flags '{decl.Flags}'.");
            }
        } while (Match(TokenKind.Comma));

        Log($"[ParseDeclarationList] EXIT. Parsed {declarations.Count} variable declaration(s). Next: {Peek()}");
        return declarations.ToImmutable();
    }

    /// <summary>
    /// [FASE 1] Parsa una lista di dichiarazioni di costanti, costruendo i nodi AST con `SymbolId = -1`
    /// e i flag di visibilità corretti.
    /// </summary>
    /// <summary>
    /// Costante stile PowerBASIC: <c>%NOME = valore</c>, equivalente a
    /// <c>CONST NOME AS INTEGER = valore</c>. Il '%' iniziale implica sempre INTEGER (stessa
    /// semantica del suffisso finale "nome%" già esistente, solo applicato come prefisso) — nessuna
    /// clausola AS ammessa. Il token corrente all'ingresso è già l'identificatore "%NOME" (lessema
    /// col '%' incluso, prodotto dal Lexer); il chiamante ha già verificato che sia seguito da '='.
    /// Riusa la stessa validazione "il valore dev'essere un letterale" di ParseConstDeclarationList,
    /// e si affida alla STESSA infrastruttura di binding (RegisterLocalConstants/
    /// RegisterGlobalConstants in Linker.Binding.cs, dispatchate su ConstStmt) di un CONST normale —
    /// nessuna nuova logica di registrazione simboli necessaria.
    /// </summary>
    private ConstStmt ParsePercentConstantDeclaration()
    {
        var nameToken = Advance(); // Consuma "%NOME".
        Consume(TokenKind.Equal, "A constant declaration must be initialized.");
        var initialValue = ParseExpression();

        if (initialValue is not NumberExpr)
        {
            throw CreateError(nameToken,
                "Una costante '%nome' deve avere un valore letterale numerico: il '%' iniziale implica sempre INTEGER.");
        }

        var declaration = new ConstDeclaration(-1, nameToken, new IntegerType(), initialValue, SymbolFlags.None)
            { DeclaredNamespace = _currentNamespace };
        return new ConstStmt(nameToken, [declaration]);
    }

    private ImmutableArray<ConstDeclaration> ParseConstDeclarationList(
        SymbolFlags visibilityFlags = SymbolFlags.None)
    {
        Log($"[ParseConstDeclarationList] ENTER. Flags={visibilityFlags}, Current: {Peek()}");
        var declarations = ImmutableArray.CreateBuilder<ConstDeclaration>();
        do
        {
            var nameToken = Consume(TokenKind.Identifier, "Expected constant name.");
            DataType? dataType = null;
            if (Match(TokenKind.As))
            {
                dataType = ParseTypeName();
            }

            Consume(TokenKind.Equal, "A constant declaration must be initialized.");
            var initialValue = ParseExpression();

            if (initialValue is not (NumberExpr or StringExpr or BooleanExpr))
            {
                throw CreateError(nameToken, "The value of a constant must be a literal.");
            }

            dataType ??= initialValue switch
            {
                StringExpr => new StringType(),
                BooleanExpr => new BitType(),
                _ => DeduceTypeFromSuffix(nameToken.Lexeme)
            };

            // Passa i flag di visibilità al nuovo costruttore di ConstDeclaration.
            declarations.Add(new ConstDeclaration(-1, nameToken, dataType, initialValue, visibilityFlags)
                { DeclaredNamespace = _currentNamespace });
            Log(
                $"[ParseConstDeclarationList]   -> Parsed constant '{nameToken.Lexeme}' with flags '{visibilityFlags}'.");
        } while (Match(TokenKind.Comma));

        Log(
            $"[ParseConstDeclarationList] EXIT. Parsed {declarations.Count} constant declaration(s). Next: {Peek()}");
        return declarations.ToImmutable();
    }

    /// <summary>
    /// [FASE 1] Parsa una definizione di `FUNCTION` o `SUB` senza registrare simboli.
    /// QUESTA VERSIONE RIPRISTINA LA LOGICA DI PARSING MANUALE DEL CORPO.
    /// </summary>
    /// <summary>
    /// [FASE 1] Vieta lo shadowing silenzioso di un builtin (2026-09-24): una FUNCTION/SUB
    /// definita DIRETTAMENTE in un namespace builtin (es. `NAMESPACE MATH`) con arità
    /// contestata da un membro builtin omonimo dello stesso namespace non sarebbe mai
    /// raggiungibile — `NS.MEMBRO(...)` è risolto per TokenKind e il builtin vince sempre
    /// in silenzio su tutte le gambe C#. Meglio un errore evidente subito.
    /// Regole: solo definizioni a livello namespace (mai membri TYPE/CLASS con IsMethod,
    /// mai procedure locali nidificate, mai scope globale); confronto per ARITÀ effettiva
    /// (stesso conteggio tenendo conto di OPTIONAL/PARAMARRAY), mai per tipo (nessuna gamba
    /// dispatcha utente-vs-builtin per tipo); arità disgiunte = vero overload, consentito
    /// (il linker lo risolve — vedi fallback in RewriteBuiltinFunctionCall).
    /// </summary>
    private void CheckBuiltinShadowing(Token nameToken, ImmutableArray<Parameter> parameters)
    {
        var ns = _currentNamespace;
        if (string.IsNullOrEmpty(ns)) return;
        if (!_registry.BuiltinProcedures.TryGetValue(ns, out var procs)) return;
        foreach (var native in procs.OfType<NativeProcedureDefinition>())
        {
            if (!native.OriginalName.Lexeme.Equals(nameToken.Lexeme, StringComparison.OrdinalIgnoreCase))
                continue;
            if (!ArityContested(native.Parameters, parameters))
                continue;
            throw CreateError(nameToken,
                $"La procedura '{nameToken.Lexeme}' nel namespace '{ns}' ridefinisce il builtin esistente " +
                $"con firma contestata (stessa arità effettiva): il builtin vincerebbe sempre in silenzio. " +
                $"Usare un nome diverso oppure una firma diversa (overload con arità non coperta dal builtin).");
        }
    }

    /// <summary>
    /// Due firme sono "contestate" se esiste almeno un'arità di chiamata accettata da
    /// entrambe (tenendo conto di parametri OPTIONAL e PARAMARRAY variadico).
    /// </summary>
    private static bool ArityContested(ImmutableArray<Parameter> nativeParams, ImmutableArray<Parameter> userParams)
    {
        static int RequiredCount(ImmutableArray<Parameter> ps)
        {
            var c = 0;
            foreach (var p in ps)
            {
                if (p.Flags.HasFlag(SymbolFlags.IsOptional) || p.Flags.HasFlag(SymbolFlags.IsParamArray))
                    break;
                c++;
            }
            return c;
        }
        static bool IsVariadic(ImmutableArray<Parameter> ps) =>
            ps.Length > 0 && ps[^1].Flags.HasFlag(SymbolFlags.IsParamArray);
        // NOTA: i required sono contati come prefisso — OPTIONAL/PARAMARRAY sono per
        // costruzione in coda; un OPTIONAL non-trailing è trattato in modo conservativo.
        var reqN = RequiredCount(nativeParams);
        var reqU = RequiredCount(userParams);
        long hiN = IsVariadic(nativeParams) ? long.MaxValue : nativeParams.Length;
        long hiU = IsVariadic(userParams) ? long.MaxValue : userParams.Length;
        return Math.Max(reqN, reqU) <= Math.Min(hiN, hiU);
    }

    private FunctionDefinition ParseFunctionDefinition(
        SymbolFlags initialFlags = SymbolFlags.None,
        bool isAsync = false)
    {
        var startToken = Peek(); // Mantiene il riferimento al token iniziale per la SourcePosition
        Log($"[ParseFunctionDefinition] ENTER. Current: {startToken}, isAsync={isAsync}");

        // Questo metodo si limita ad aggiungere il flag IsFunction ai flag ricevuti.
        var flags = initialFlags | SymbolFlags.IsFunction;
        // Applica la logica di visibilità solo se non è già stata
        if (initialFlags == SymbolFlags.None)
        {
            if (Match(TokenKind.Public))
            {
                flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
            }
            else
            {
                // Il default per una funzione a livello di modulo è privato al modulo.
                flags |= SymbolFlags.IsPrivate;
            }
        }

        // La keyword tokenizzata è sempre Function, ma teniamo traccia se era SUB.
        var isSub = Peek().Lexeme.Equals("SUB", StringComparison.OrdinalIgnoreCase);
        var keywordToken = Consume(TokenKind.Function, "Expected 'FUNCTION' or 'SUB' keyword.");
        var nameToken = ConsumeProcedureName();
        RegisterUserProcedureName(nameToken.Lexeme);

        // --- ATTRIBUTES: <Serializable>, <Required>, etc ---
        // Gli attributes vengono parsati da ParseStatement e salvati in _pendingAttributes.
        // Qui distinguiamo tra attributes e type parameters:
        // - <AttributeName> seguito da > = attribute (già consumato da ParseStatement)
        // - <T, U> o <T WHERE ...> = type parameters
        var functionAttributes = _pendingAttributes;

        // --- TYPE PARAMETERS: <T, U WHERE T IS IComparable> ---
        var typeParameters = Check(TokenKind.LessThan)
            ? ParseTypeParameterList()
            : ImmutableArray<TypeParameter>.Empty;

        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;

        // 2026-09-24: shadowing di builtin = errore evidente — solo definizioni a livello
        // namespace (mai membri TYPE/CLASS con IsMethod, mai locali nidificate).
        if (!initialFlags.HasFlag(SymbolFlags.IsMethod) && _functionContextStack.Count == 0)
            CheckBuiltinShadowing(nameToken, parameters);

        DataType returnType;
        if (Match(TokenKind.As))
        {
            // Se c'è 'AS', DEVE essere una FUNCTION.
            if (isSub)
            {
                throw CreateError(Previous(), "A SUB procedure cannot have a return type specified with 'AS'.");
            }

            returnType = ParseTypeName();
        }
        else
        {
            // Se non c'è 'AS', deve essere una SUB.
            // Le FUNCTION richiedono 'AS', quindi se non siamo in una SUB, è un errore.
            if (!isSub)
            {
                throw CreateError(nameToken,
                    "A FUNCTION declaration must have an explicit return type using the 'AS' clause.");
            }

            returnType = new VoidType();
        }

        // --- RESOLVE TYPE PARAMETERS in parameters and return type ---
        if (typeParameters.Length > 0)
        {
            var typeParamNames = new HashSet<string>(typeParameters.Select(tp => tp.Name.Lexeme), StringComparer.OrdinalIgnoreCase);
            parameters = ResolveTypeParametersInParameters(parameters, typeParameters, typeParamNames);
            returnType = ResolveTypeParametersInType(returnType, typeParameters, typeParamNames);
        }

        Consume(TokenKind.Newline, "Expected newline after function/sub signature.");
        Log($"[ParseFunctionDefinition]   -> Parsed signature for '{nameToken.Lexeme}'. Now parsing body...");

        var bodyStmts = new List<Stmt>();
        var bodyDecls = new List<AstNode>();

        _functionContextStack.Push(nameToken);
        _functionContextReturnsValue.Push(returnType is not VoidType);
        _errDeferScopes.Push(new List<Stmt>());   // ERRDEFER della procedura (LIFO)
        try
        {
            // Crea una definizione di procedura fittizia per fornire il contesto ai metodi interni.
            var dummyProc = new FunctionDefinition(-1, nameToken, parameters, returnType, [], flags, typeParameters);

            while (!Check(TokenKind.EndFunction) && !IsAtEnd())
            {
                while (Match(TokenKind.Newline))
                {
                }

                if (Check(TokenKind.EndFunction) || IsAtEnd()) break;

                if (Peek().Kind == TokenKind.LineNumber)
                {
                    throw CreateError(Peek(), "Line numbers are not allowed inside a FUNCTION or SUB block.");
                }

                // Passa il delegato corretto: ParseStatement.
                var posBefore = _current;
                ParseLogicalLine(bodyStmts, bodyDecls, dummyProc);
                if (_current == posBefore) ThrowIfStrayBlockTerminator();   // END IF/ELSE/CASE orfano: errore, non HANG
            }
        }
        finally
        {
            _functionContextStack.Pop();
            _functionContextReturnsValue.Pop();
        }        var endToken = Consume(TokenKind.EndFunction, "Expected 'END FUNCTION' or 'END SUB'.");
        // ERRDEFER: il corpo (senza END) va avvolto in TRY/CATCH+RILANCIO; END PROCEDURE resta
        // FUORI dal TRY (il `ret` non deve stare in un blocco protetto).
        var errDefers = _errDeferScopes.Pop();
        if (errDefers.Count > 0) bodyStmts = WrapErrDefers(bodyStmts, errDefers, endToken);
        bodyStmts.Add(new EndProcedureStmt(endToken));

        // Il primo token della signature (es. 'PUBLIC' o 'SUB'/'FUNCTION') viene usato
        // per costruire il nodo AST, in modo che la sua SourcePosition (con il FileId corretto) venga propagata.
        // Il token 'nameToken' contiene già la posizione corretta, ma usare il token d'inizio (startToken o keywordToken)
        // è più robusto se ci sono modificatori.
        var result = new FunctionDefinition(-1, keywordToken, parameters, returnType, [..bodyStmts], flags, typeParameters, functionAttributes)
            { OriginalName = nameToken, DeclaredNamespace = _currentNamespace };
        Log(
            $"[ParseFunctionDefinition] EXIT. Parsed function '{nameToken.Lexeme}' with {bodyStmts.Count} statements. Next: {Peek()}");
        return result;
    }

    /// <summary>
    /// [FASE 1] Parsa una definizione di `GENERATOR` senza registrare simboli.
    /// QUESTA VERSIONE RIPRISTINA LA LOGICA DI PARSING MANUALE DEL CORPO.
    /// </summary>
    private GeneratorDefinition ParseGeneratorDefinition()
    {
        Log($"[ParseGeneratorDefinition] ENTER. Current: {Peek()}");
        var flags = SymbolFlags.IsFunction;
        if (Match(TokenKind.Public)) flags |= SymbolFlags.IsPublic;
        else flags |= SymbolFlags.IsPrivate;

        // Salva il token 'GENERATOR' che contiene la posizione corretta.
        var keywordToken = Consume(TokenKind.Generator, "Expected 'GENERATOR' or 'GEN' keyword.");
        var nameToken = ConsumeProcedureName();
        RegisterUserProcedureName(nameToken.Lexeme);
        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;
        var yieldType = ParseTypeClause(nameToken, isFunctionDeclaration: true);
        Consume(TokenKind.Newline, "Expected newline after generator signature.");
        Log($"[ParseGeneratorDefinition]   -> Parsed signature for '{nameToken.Lexeme}'. Now parsing body...");

        var bodyStmts = new List<Stmt>();
        var bodyDecls = new List<AstNode>();

        _functionContextStack.Push(nameToken);
        _functionContextReturnsValue.Push(true);   // i generator restituiscono sempre un valore
        try
        {
            // Crea una definizione di generatore fittizia per fornire il contesto.
            var dummyProc = new GeneratorDefinition(-1, flags, nameToken, parameters, yieldType, []);

            while (!Check(TokenKind.EndGenerator) && !IsAtEnd())
            {
                while (Match(TokenKind.Newline))
                {
                }

                if (Check(TokenKind.EndGenerator) || IsAtEnd()) break;

                if (Peek().Kind == TokenKind.LineNumber)
                {
                    throw CreateError(Peek(), "Line numbers are not allowed inside a GENERATOR block.");
                }

                var genPosBefore = _current;
                ParseLogicalLine(bodyStmts, bodyDecls, dummyProc);
                if (_current == genPosBefore) ThrowIfStrayBlockTerminator();
            }
        }
        finally
        {
            _functionContextStack.Pop();
            _functionContextReturnsValue.Pop();
        }

        var endToken = Consume(TokenKind.EndGenerator, "Expected 'END GENERATOR'.");
        bodyStmts.Add(new EndProcedureStmt(endToken));

        // 'Name' deve avere il LEXEME del vero identificatore (nameToken, es. "Count"), non del
        // keyword 'GENERATOR' — PopulateProcedureAndFfiMaps (Compiler.cs) e altri consumatori
        // del Compiler indicizzano/chiamano le procedure per proc.Name.Lexeme, non per
        // OriginalName.Lexeme (usato invece dal Linker/SymbolTable per la registrazione del
        // simbolo). Con Name=keywordToken, una chiamata a un GENERATOR il cui nome coincide con
        // un builtin (es. `GENERATOR Count(...)`, che collide con TokenKind.Count →
        // BuiltInFunction.COUNT) veniva silenziosamente registrata sotto la chiave sbagliata
        // ("GENERATOR", non "Count"), mai trovata dal sito di chiamata, che quindi cadeva sempre
        // sul builtin — bug isolato con FunctionsYield.bas. Manteniamo comunque la Position di
        // keywordToken (motivo originale del commento precedente: FileId corretto in contesti
        // multi-file), fondendola col Lexeme/Kind/Literal corretti di nameToken.
        var correctedNameToken = nameToken with { Position = keywordToken.Position };
        var result = new GeneratorDefinition(-1, flags, correctedNameToken, parameters, yieldType, [..bodyStmts])
            { OriginalName = nameToken, DeclaredNamespace = _currentNamespace };
        Log(
            $"[ParseGeneratorDefinition] EXIT. Parsed generator '{nameToken.Lexeme}' with {bodyStmts.Count} statements. Next: {Peek()}");
        return result;
    }

    /// <summary>
    /// [FASE 1] Parsa una lista di parametri di tipo generici: &lt;T&gt;, &lt;T, U&gt;,
    /// &lt;T WHERE T IS IComparable, U IS IEquatable&gt;.
    /// </summary>
    private ImmutableArray<TypeParameter> ParseTypeParameterList()
    {
        Consume(TokenKind.LessThan, "Expected '<' to start type parameter list.");
        var parameters = ImmutableArray.CreateBuilder<TypeParameter>();
        do
        {
            var nameToken = Consume(TokenKind.Identifier, "Expected type parameter name.");
            ImmutableArray<DataType>? constraints = null;
            if (Match(TokenKind.Where))
            {
                // WHERE T IS TypeName [, TypeName2, ...]
                var constraintsBuilder = ImmutableArray.CreateBuilder<DataType>();
                do
                {
                    Match(TokenKind.Is); // 'IS' opzionale
                    var constraintType = ParseTypeName();
                    constraintsBuilder.Add(constraintType);
                } while (Match(TokenKind.Comma));
                constraints = constraintsBuilder.ToImmutable();
            }
            parameters.Add(new TypeParameter(nameToken, constraints));
        } while (Match(TokenKind.Comma));
        Consume(TokenKind.GreaterThan, "Expected '>' to close type parameter list.");
        return parameters.ToImmutable();
    }

    /// <summary>
    /// Sostituisce gli UnresolvedQualifiedType che corrispondono ai nomi dei type parameters
    /// con i TypeParameter veri nei parametri della funzione.
    /// </summary>
    private ImmutableArray<Parameter> ResolveTypeParametersInParameters(
        ImmutableArray<Parameter> parameters, ImmutableArray<TypeParameter> typeParameters, HashSet<string> typeParamNames)
    {
        if (typeParameters.Length == 0) return parameters;
        return parameters.Select(p =>
        {
            var newDataType = ResolveTypeParametersInType(p.DataType, typeParameters, typeParamNames);
            return newDataType == p.DataType ? p : p with { DataType = newDataType };
        }).ToImmutableArray();
    }

    /// <summary>
    /// Risolvi ricorsivamente i type parameters in un DataType.
    /// </summary>
    private DataType ResolveTypeParametersInType(
        DataType dataType, ImmutableArray<TypeParameter> typeParams, HashSet<string> typeParamNames)
    {
        if (typeParams.Length == 0) return dataType;
        return dataType switch
        {
            UnresolvedQualifiedType uqt when uqt.QualifiedName.Parts.Length == 1
                && typeParamNames.Contains(uqt.QualifiedName.Parts[0].Lexeme)
                => typeParams.First(tp => tp.Name.Lexeme.Equals(uqt.QualifiedName.Parts[0].Lexeme, StringComparison.OrdinalIgnoreCase)),
            ArrayType at => at with { ElementType = ResolveTypeParametersInType(at.ElementType, typeParams, typeParamNames) },
            ListType lt => lt with { ElementType = ResolveTypeParametersInType(lt.ElementType, typeParams, typeParamNames) },
            DictType dt => new DictType(
                ResolveTypeParametersInType(dt.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(dt.ValueType, typeParams, typeParamNames)),
            HashSetType hst => hst with { ElementType = ResolveTypeParametersInType(hst.ElementType, typeParams, typeParamNames) },
            QueueType qt => qt with { ElementType = ResolveTypeParametersInType(qt.ElementType, typeParams, typeParamNames) },
            StackType st => st with { ElementType = ResolveTypeParametersInType(st.ElementType, typeParams, typeParamNames) },
            SortedDictType sdt => new SortedDictType(
                ResolveTypeParametersInType(sdt.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(sdt.ValueType, typeParams, typeParamNames)),
            TreeType tt => new TreeType(
                ResolveTypeParametersInType(tt.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(tt.ValueType, typeParams, typeParamNames)),
            AVLTreeType avlt => new AVLTreeType(
                ResolveTypeParametersInType(avlt.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(avlt.ValueType, typeParams, typeParamNames)),
            RedBlackTreeType rbt => new RedBlackTreeType(
                ResolveTypeParametersInType(rbt.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(rbt.ValueType, typeParams, typeParamNames)),
            BinaryIndexedTreeType bitt => bitt with { ElementType = ResolveTypeParametersInType(bitt.ElementType, typeParams, typeParamNames) },
            LinkedListType llt => llt with { ElementType = ResolveTypeParametersInType(llt.ElementType, typeParams, typeParamNames) },
            DoublyLinkedListType dllt => dllt with { ElementType = ResolveTypeParametersInType(dllt.ElementType, typeParams, typeParamNames) },
            CircularLinkedListType cllt => cllt with { ElementType = ResolveTypeParametersInType(cllt.ElementType, typeParams, typeParamNames) },
            BinaryHeapType bht => new BinaryHeapType(
                ResolveTypeParametersInType(bht.ElementType, typeParams, typeParamNames), bht.IsMaxHeap),
            BagType bt => bt with { ElementType = ResolveTypeParametersInType(bt.ElementType, typeParams, typeParamNames) },
            CacheType ct => new CacheType(
                ResolveTypeParametersInType(ct.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(ct.ValueType, typeParams, typeParamNames)),
            GraphType gt => gt with { NodeType = ResolveTypeParametersInType(gt.NodeType, typeParams, typeParamNames) },
            TimelineType tlt => tlt with { ValueType = ResolveTypeParametersInType(tlt.ValueType, typeParams, typeParamNames) },
            HashRingType hrt => hrt with { NodeType = ResolveTypeParametersInType(hrt.NodeType, typeParams, typeParamNames) },
            RingBufferType rbt => rbt with { ElementType = ResolveTypeParametersInType(rbt.ElementType, typeParams, typeParamNames) },
            BPlusTreeType bpt => new BPlusTreeType(
                ResolveTypeParametersInType(bpt.KeyType, typeParams, typeParamNames),
                ResolveTypeParametersInType(bpt.ValueType, typeParams, typeParamNames)),
            NullableType nt => new NullableType(ResolveTypeParametersInType(nt.ElementType, typeParams, typeParamNames)),
            FunctionPointerType fpt => new FunctionPointerType(
                fpt.ParameterTypes.Select(p => ResolveTypeParametersInType(p, typeParams, typeParamNames)).ToImmutableArray(),
                ResolveTypeParametersInType(fpt.ReturnType, typeParams, typeParamNames)),
            UserDefinedType udt => udt with { Fields = udt.Fields.Select(f =>
                f with { DataType = ResolveTypeParametersInType(f.DataType, typeParams, typeParamNames) }
            ).ToImmutableArray() },
            _ => dataType
        };
    }

    /// <summary>
    /// [FASE 1] Parsa una definizione `DECLARE` per una funzione esterna (FFI) senza registrare il simbolo.
    /// </summary>
    private ExternalProcedureDefinition ParseDeclareStatement()
    {
        Log($"[ParseDeclareStatement] ENTER. Current: {Peek()}");

        var flags = SymbolFlags.None;
        if (Match(TokenKind.Public))
            flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
        else if (Match(TokenKind.Private))
            flags |= SymbolFlags.IsPrivate;
        else
            flags |= SymbolFlags.IsPrivate;

        // Salva il token 'DECLARE' che contiene la posizione corretta.
        var declareToken = Consume(TokenKind.Declare, "Internal parser error: Expected 'DECLARE' token.");
        // `DECLARE NATIVE FUNCTION|SUB`: modificatore contestuale (non e' una parola riservata).
        var isNativeAbi = false;
        if (Peek().Lexeme.Equals("NATIVE", StringComparison.OrdinalIgnoreCase) && At(1).Kind == TokenKind.Function)
        {
            isNativeAbi = true;
            Advance();
        }
        Consume(TokenKind.Function, "Expected FUNCTION or SUB after DECLARE.");
        var nameToken = Peek();
        if (!IsPermissiveNameToken(nameToken))
        {
            throw CreateError(nameToken, "Expected a function or subroutine name after DECLARE FUNCTION/SUB.");
        }

        Advance();
        Consume(TokenKind.Lib, "Expected 'LIB' keyword after routine name.");
        var libraryNameToken = Consume(TokenKind.StringLiteral, "Expected library name as a string.");
        Token? aliasNameToken = null;
        if (Match(TokenKind.Alias))
        {
            aliasNameToken = Consume(TokenKind.StringLiteral, "Expected exported routine name as a string.");
        }

        var parameters = ImmutableArray<Parameter>.Empty;
        DataType? returnType = null;
        var isFunction = false;
        var previousExternalSignature = _parsingExternalSignature;
        _parsingExternalSignature = true;
        try
        {
            parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;
            isFunction = Check(TokenKind.As);
            // 2026-09-24: come per FUNCTION/SUB — DECLARE ... LIB atterra nello stesso scope
            // del namespace (mai membro, mai nidificata qui: lo stack copre le locali).
            if (_functionContextStack.Count == 0)
                CheckBuiltinShadowing(nameToken, parameters);
            returnType = isFunction ? ParseTypeClause(nameToken) : null;
        }
        finally
        {
            _parsingExternalSignature = previousExternalSignature;
        }
        var callingConvention =
            Match(TokenKind.CDecl) ? FfiCallingConvention.CDecl : FfiCallingConvention.StdCall;
        if (callingConvention == FfiCallingConvention.StdCall)
            Match(TokenKind.CDecl); // Consuma CDECL se presente dopo STDCALL

        // Usa 'declareToken' per il costruttore di ProcedureDefinition
        // per garantire che la SourcePosition con il FileId corretto venga propagata.
        var result = new ExternalProcedureDefinition(-1, isFunction, declareToken, libraryNameToken, aliasNameToken,
            parameters,
            returnType, callingConvention)
        {
            Flags = flags | (isFunction ? SymbolFlags.IsFunction : SymbolFlags.None),
            OriginalName = nameToken,
            DeclaredNamespace = _currentNamespace,
            IsNativeAbi = isNativeAbi
        };

        Log($"[ParseDeclareStatement] EXIT. Parsed DECLARE for '{nameToken.Lexeme}'. Next: {Peek()}");
        return result;
    }

    /// <summary>
    /// [FASE 1] Parsa una definizione di enumerazione (`ENUM...END ENUM`).
    /// </summary>
    private EnumDefinition ParseEnumDefinition(SymbolFlags initialFlags = SymbolFlags.None)
    {
        var flags = initialFlags | SymbolFlags.IsEnum;

        // Gestione visibilità (PUBLIC/PRIVATE sono già stati consumati dal chiamante in ParseStatement
        // oppure sono gestiti qui se il chiamante è ParseTypeDefinition o simili.
        // Assumiamo che il chiamante in ParseStatement abbia visto PUBLIC/PRIVATE e poi chiamato questo metodo
        // oppure che questo metodo debba gestire i modificatori.
        // Guardando ParseClassDefinition, i modificatori vengono gestiti DENTRO se chiamati da ParseStatement.
        // Ma ParseStatement in Parser.Statements.cs gestisce PUBLIC/PRIVATE e poi fa lo switch.
        // Se arriviamo qui da ParseStatement -> case TokenKind.Public -> peek Enum -> chiama questo metodo?
        // No, ParseStatement chiama ParseDeclarationStatement o ParseTypeDefinition.
        // Dobbiamo aggiornare ParseStatement per gestire ENUM.

        // Per coerenza con ParseClassDefinition, controlliamo se c'è un modificatore precedente non consumato
        // o se dobbiamo consumarlo noi. 
        // Tuttavia, in ParseStatement, il token PUBLIC è già consumato e si fa peek sul successivo.
        // Quindi qui ci aspettiamo che il token corrente sia ENUM.

        // Se il chiamante ha già consumato PUBLIC/PRIVATE, dovrebbe passarci i flag.
        // Ma ParseStatement attuale non è strutturato per passare flag a metodi specifici come ParseClassDefinition.
        // ParseClassDefinition inizia con `var isPublic = Match(TokenKind.Public);`.
        // Quindi anche qui dobbiamo gestire i modificatori.

        // Se non sono stati passati flag di visibilità dall'esterno, proviamo a parsarli qui.
        // Questo supporta sia il caso "ENUM ..." (default) che "PRIVATE ENUM ..." se il chiamante non l'ha gestito.
        if ((initialFlags & (SymbolFlags.IsPublic | SymbolFlags.IsPrivate)) == 0)
        {
            if (Match(TokenKind.Public))
            {
                flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
            }
            else if (Match(TokenKind.Private))
            {
                flags |= SymbolFlags.IsPrivate;
            }
            else
            {
                // Default visibilità
                flags |= SymbolFlags.IsPublic;
            }
        }

        Consume(TokenKind.Enum, "Expected 'ENUM'.");
        var nameToken = Consume(TokenKind.Identifier, "Expected enum name.");

        DataType underlyingType = new IntegerType();
        if (Match(TokenKind.As))
        {
            underlyingType = ParseTypeName();
        }

        Consume(TokenKind.Newline, "Expected newline after ENUM definition.");

        var members = ImmutableArray.CreateBuilder<EnumMember>();

        while (!Check(TokenKind.EndEnum) && !IsAtEnd())
        {
            while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber))
            {
                /* Skip */
            }

            if (Check(TokenKind.EndEnum) || IsAtEnd()) break;

            var memberName = Consume(TokenKind.Identifier, "Expected enum member name.");
            Expr? valueExpr = null;

            if (Match(TokenKind.Equal))
            {
                valueExpr = ParseExpression();
            }

            members.Add(new EnumMember(-1, memberName, valueExpr));

            if (Check(TokenKind.Comma)) Advance();
        }

        Consume(TokenKind.EndEnum, "Expected 'END ENUM'.");

        return new EnumDefinition(-1, nameToken, underlyingType, flags, members.ToImmutable())
            { DeclaredNamespace = _currentNamespace };
    }

    /// <summary>
    /// [FASE 1] Parsa una definizione di tipo delegato (DELEGATE...).
    /// </summary>
    /// <param name="initialFlags">Flag di visibilità eventualmente già parsati dal chiamante.</param>
    /// <returns>Un nodo AST <see cref="DelegateDefinition"/>.</returns>
    private DelegateDefinition ParseDelegateDefinition(
        SymbolFlags initialFlags = SymbolFlags.None)
    {
        var flags = initialFlags;
        if ((initialFlags & (SymbolFlags.IsPublic | SymbolFlags.IsPrivate)) == 0)
        {
            if (Match(TokenKind.Public))
            {
                flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
            }
            else
            {
                flags |= SymbolFlags.IsPrivate;
            }
        }

        Consume(TokenKind.Delegate, "Expected 'DELEGATE'.");

        // Un delegato può definire una firma per una SUB o una FUNCTION.
        var isSub = Peek().Lexeme.Equals("SUB", StringComparison.OrdinalIgnoreCase);
        Consume(TokenKind.Function, "Expected 'FUNCTION' or 'SUB' after 'DELEGATE'.");

        var nameToken = ConsumeProcedureName();

        var parameters = Check(TokenKind.LeftParen) ? ParseParameters() : ImmutableArray<Parameter>.Empty;

        DataType returnType;
        if (Match(TokenKind.As))
        {
            if (isSub)
                throw CreateError(Previous(), "A DELEGATE SUB cannot have a return type specified with 'AS'.");
            returnType = ParseTypeName();
        }
        else
        {
            if (!isSub)
                throw CreateError(nameToken,
                    "A DELEGATE FUNCTION must have an explicit return type using the 'AS' clause.");
            returnType = new VoidType();
        }

        return new DelegateDefinition(-1, nameToken, parameters, returnType, flags)
            { OriginalName = nameToken, DeclaredNamespace = _currentNamespace };
    }

    /// <summary>
    /// [FASE 1] Parsa un'assegnazione al valore di ritorno di una funzione (`NomeFunzione =
    /// espr` / `OPERATOR = espr`, la forma "classica" BASIC senza `LET` né `RETURN`
    /// espliciti).
    /// </summary>
    /// <remarks>
    /// Bug corretto (2026-08-31, audit cross-backend closures/lambda/macro): questo metodo
    /// costruiva un <see cref="ReturnStmt"/> vero e proprio — semanticamente ERRATO, perché
    /// "NomeFunzione = espr" è SOLO un'assegnazione al return-slot (il valore che la funzione
    /// restituirà QUANDO uscirà, per fine corpo o EXIT FUNCTION/RETURN espliciti), MAI un
    /// early-exit implicito. Un <see cref="ReturnStmt"/> mid-body faceva sì che un pass a
    /// valle (dead-code-elimination sul corpo della procedura) scartasse SILENZIOSAMENTE
    /// ogni istruzione successiva nello stesso corpo (bug: `FUNCTION F(): F = 1: PRINT "x":
    /// END FUNCTION` non stampava mai "x" su IL/PCode-managed — la stampa non compariva
    /// affatto, senza errori). Il nativo VM/C (che usa lo stesso schema "local col nome della
    /// funzione", vedi NativeCompiler-Recursion-SelfCallShadowedByReturnSlot (solved).md) NON
    /// tratta questa forma come un return — continua l'esecuzione oltre l'assegnazione,
    /// preso qui come riferimento di comportamento corretto.
    /// Fix: costruisce una LetStmt/Assignment "bare" (LetToken null) su un target NON
    /// risolto (SymbolId -1) — il binder (RegisterImplicitLocalVariables, Linker.Binding.cs)
    /// risolve già correttamente un'assegnazione bare cercando il simbolo lungo l'intera
    /// catena di scope (FindSymbolInScope). Il token del target deve essere il NOME VERO
    /// della funzione/generatore corrente (da <see cref="_functionContextStack"/>, che
    /// FUNCTION/GENERATOR spingono col loro <c>nameToken</c> reale) — MAI il token che ha
    /// innescato il parsing di questo statement: per la forma `FUNCTION = espr` quel token è
    /// la keyword letterale "FUNCTION", il cui lessema non corrisponde a NESSUN simbolo reale
    /// (bug introdotto dal fix precedente: usare `returnInitiatorToken` come nome del target
    /// faceva fallire la risoluzione bare, con conseguente NullReferenceException a runtime —
    /// riprodotto con `FunctionsClosures.bas`, `FUNCTION = FN() AS LONG => ...`).
    /// `OPERATOR = espr` resta un caso a parte: <see cref="OperatorDefinition"/> non è una
    /// <see cref="ProcedureDefinition"/> e il suo simbolo viene registrato dal Linker sotto un
    /// nome MANGLED calcolato più avanti (mai noto qui in fase di parsing, e
    /// <c>_functionContextStack</c> non viene mai valorizzato per gli OPERATOR) — una
    /// risoluzione bare per nome non troverebbe mai il simbolo giusto. Dato che
    /// `OPERATOR = espr` è per costruzione SEMPRE l'ultima istruzione del blocco (nessun uso
    /// legittimo di codice successivo), il vecchio comportamento (ReturnStmt esplicito, che
    /// non necessita risoluzione per nome) resta corretto e viene mantenuto per questo caso.
    /// </remarks>
    private Stmt ParseFunctionReturnAssignmentStatement()
    {
        var returnInitiatorToken = Advance();
        Consume(TokenKind.Equal, $"Expected '=' after '{returnInitiatorToken.Lexeme}'.");
        var value = ParseExpression();
        if (_functionContextStack.Count == 0)
        {
            // OPERATOR = espr (mai annidato in un FUNCTION/GENERATOR che lo spinga): nessun
            // nome reale risolvibile qui, vedi remarks sopra — comportamento originale.
            return new ReturnStmt(value, returnInitiatorToken);
        }
        var nameToken = _functionContextStack.Peek();
        var target = new VariableTarget(-1, nameToken, nameToken);
        return new LetStmt([new Assignment(target, value)], null);
    }

    /// <summary>
    /// Parsa attributes nella sintassi &lt;AttributeName&gt;.
    /// Accetta sia Identifier che keyword (Serializable, Required) come nomi di attribute.
    /// </summary>
    private ImmutableArray<Token> ParseDeclarationAttributes()
    {
        var attributes = ImmutableArray.CreateBuilder<Token>();
        while (Check(TokenKind.LessThan))
        {
            Advance();
            var nameToken = Peek();
            if (nameToken.Kind == TokenKind.Identifier || nameToken.Kind == TokenKind.Serializable || nameToken.Kind == TokenKind.Required)
            {
                Advance();
                attributes.Add(nameToken);
            }
            else
            {
                throw CreateError(nameToken, $"Expected attribute name after '<', got '{nameToken.Lexeme}'.");
            }
            Consume(TokenKind.GreaterThan, "Expected '>' after attribute name.");
        }
        return attributes.ToImmutable();
    }
}
