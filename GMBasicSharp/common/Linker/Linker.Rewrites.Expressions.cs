// FILE: Interpreter.Linker.Rewrites.Expressions.cs

using System.Collections.Immutable;
using System.Diagnostics.CodeAnalysis;

// ReSharper disable RedundantArgumentDefaultValue

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Riscrive un nodo <see cref="QualifiedNameExpr"/> (es. <c>MATH.PI</c>), risolvendo il nome qualificato
    /// in una <see cref="VariableExpr"/> o <see cref="BuiltinFunctionCallExpr"/> tramite lookup nella SymbolTable.
    /// Gestisce costanti (ConstDeclaration) e funzioni (ProcedureDefinition/NativeProcedureDefinition),
    /// mantenendo il TokenKind originale del simbolo (importante per il dispatcher della VM).
    /// </summary>
    private static Expr RewriteQualifiedNameExpr(QualifiedNameExpr qne, AstRewriteContext context, BasicProgram program)
    {
        // Per nomi qualificati con namespace (es. MATH.PI, CONSOLE.WRITELN), cerca il simbolo
        // nel namespace specificato (la prima parte del nome qualificato).
        if (qne.Parts.Length >= 2)
        {
            var nsName = qne.Parts[0].Lexeme;
            var memberName = qne.Parts[1].Lexeme;

            // Cerca simboli nel namespace e restituisci l'ID se trovato
            var nsSymbolIds = program.Symbols.GetSymbolIdsByNamespace(nsName);
            if (nsSymbolIds != null)
            {
                var foundSymbol = nsSymbolIds
                    .Select(id => program.Symbols.GetSymbol(id))
                    .FirstOrDefault(s => s.OriginalName.Equals(memberName, StringComparison.OrdinalIgnoreCase) && s.IsPublic);
                if (foundSymbol != null)
                {
                    if (foundSymbol.DeclarationNode is ConstDeclaration)
                    {
                        // Ridispatch attraverso RewriteExpression (mai un "return" diretto del
                        // VariableExpr): serve a raggiungere RewriteVariableExpr/PerformConstantFolding,
                        // che piega la costante nel suo LETTERALE (NumberExpr/StringExpr/BooleanExpr).
                        // Senza questo passaggio il nodo restava un riferimento a una variabile
                        // "COLOR.RED" mai esistita a runtime — vuoto/errato ovunque tranne che in un
                        // PRINT a top-level (dove qualche altro percorso mascherava il problema).
                        return RewriteExpression(
                            new VariableExpr(foundSymbol.Id,
                                new Token(TokenKind.Identifier, qne.Parts[1].Lexeme, null, qne.Parts[1].Position),
                                new Token(TokenKind.Identifier, memberName, null, qne.Parts[1].Position)),
                            context, program);
                    }
                    if (foundSymbol.DeclarationNode is ProcedureDefinition or NativeProcedureDefinition)
                    {
                        // Funzione built-in o UDF: usa il token ORIGINALE del simbolo (che ha TokenKind corretto)
                        var nsToken = new Token(TokenKind.Identifier, nsName, null, qne.Parts[0].Position);
                        // Cerca il token originale del simbolo (mantiene TokenKind.Pi non Identifier)
                        var originalToken = foundSymbol.OriginalToken ?? new Token(TokenKind.Identifier, memberName, null, qne.Parts[1].Position);
                        return new BuiltinFunctionCallExpr(nsToken, originalToken, [], foundSymbol.DataType);
                    }
                    return new VariableExpr(foundSymbol.Id,
                        new Token(TokenKind.Identifier, qne.Parts[1].Lexeme, null, qne.Parts[1].Position),
                        new Token(TokenKind.Identifier, memberName, null, qne.Parts[1].Position));
                }
            }
            else
            {
                // Non e' un namespace: potrebbe essere l'accesso a un membro STATIC di una CLASS
                // (es. Forma._nextId), l'unico modo per leggere/scrivere un campo STATIC dall'esterno
                // dei metodi della classe stessa (SELF._nextId funziona già tramite FieldAccessExpr).
                var classSymbol = program.Symbols.FindSymbolsByName(nsName)
                    .FirstOrDefault(s => s.DeclarationNode is ClassDefinition);
                if (classSymbol?.DeclarationNode is ClassDefinition classDef)
                {
                    var (foundMember, _) = FindFieldInHierarchy(classDef, memberName);
                    var staticSymbolId = foundMember switch
                    {
                        FieldMember { Declaration.Flags: var flags } fm when flags.HasFlag(SymbolFlags.IsStatic)
                            => fm.Declaration.SymbolId,
                        PropertyMember { Property.Flags: var flags } pm when flags.HasFlag(SymbolFlags.IsStatic)
                            => pm.Property.SymbolId,
                        _ => -1
                    };
                    if (staticSymbolId >= 0)
                    {
                        return new VariableExpr(staticSymbolId,
                            new Token(TokenKind.Identifier, memberName, null, qne.Parts[1].Position),
                            new Token(TokenKind.Identifier, memberName, null, qne.Parts[1].Position));
                    }
                }
            }
        }

        // Se non possiamo risolverlo, restituiamo il nodo originale (fallback)
        return qne;
    }

    /// <summary>
    /// Metodo helper che rimuove il suffisso di tipo da un nome di variabile e restituisce il tipo corrispondente.
    /// </summary>
    private static (string BaseName, DataType? SuffixType) SplitNameAndSuffix(string name)
    {
        if (name.Length <= 1) return (name, null);

        var lastChar = name[^1];
        DataType? type = lastChar switch
        {
            '$' => new StringType(),
            '%' => new IntegerType(),
            '&' => new LongType(),
            '!' => new SingleType(),
            '#' => new DoubleType(),
            '@' => new CurrencyType(),
            _ => null
        };

        return type != null ? (name[..^1], type) : (name, null);
    }

    /// <summary>
    /// Ricorsivamente riscrive un'espressione. Questo è il metodo centrale
    /// dove i nodi `FunctionExpr` generici vengono risolti e specializzati in fase di linking,
    /// e dove viene eseguita la validazione semantica per gli accessi ai membri.
    /// **CORREZIONE:** Assicura una propagazione coerente e completa del `context.ExpectedType`
    /// a tutte le sotto-espressioni durante la ricorsione, garantendo che l'informazione
    /// del tipo atteso sia sempre disponibile per l'inferenza dei letterali.
    /// </summary>
    /// <param name="expr">L'espressione AST originale da riscrivere.</param>
    /// <param name="context">Il contesto di riscrittura che contiene le mappe di risoluzione e il tipo atteso per l'espressione corrente.</param>
    /// <param name="program">L'istanza del programma che contiene tutti i simboli.</param>
    /// <param name="isMacroArgument">Spegne il late binding per le macro in stile Clipper</param>
    /// <returns>La nuova espressione AST, riscritta e ottimizzata, o l'originale se non sono necessarie modifiche.</returns>
    /// <summary>
    /// Chiave di letterale dict `{1: "uno"}`: il parser trasforma ogni token "nome" seguito da ':' in StringExpr, numeri compresi
    /// (`1` → "1"). Se il dict di destinazione ha chiave NUMERICA (DICT OF INTEGER/LONG/DOUBLE, ...) la chiave torna un NumberExpr,
    /// altrimenti `DICT OF INTEGER, STRING = {1: "uno"}` non compilava ("DICT OF STRING, STRING" non convertibile).
    /// I dict a chiave STRING (o non tipizzati) mantengono la chiave testuale, come sempre.
    /// </summary>
    private static Expr NumericDictKeyOrSelf(Expr key, DataType? expectedType)
    {
        if (key is StringExpr se && se.Token.Kind is TokenKind.Number or TokenKind.LineNumber
            && expectedType is DictType { KeyType: var kt } && IsNumericType(kt)
            && se.Token.Literal is double d)
            return new NumberExpr(d, se.Token);
        return key;
    }

    private static Expr RewriteExpression(Expr expr, AstRewriteContext context, BasicProgram program,
        bool isMacroArgument = false)
    {
        const bool log = false;
        Log(
                $"[REWRITE-EXPR] -> Inizio riscrittura per: {expr.GetType().Name} in Scope {expr.ScopeId}. " +
                $"ExpectedType: {context.ExpectedType?.ToString() ?? "N/A"}", log);

        // --- GESTIONE SPECIALE PER I LETTERALI (CON PROMOZIONE DI TIPO) ---
        if (expr is NumberExpr ne)
        {
            // Se c'è un tipo atteso e questo è un tipo numerico...
            if (context.ExpectedType != null && IsNumericType(context.ExpectedType))
            {
                // ...inferiamo il tipo di base del letterale (senza contesto).
                var baseInferredType = LinkerInferExpressionType(ne, context with { ExpectedType = null }, program);

                // Se il tipo atteso è diverso dal tipo base del letterale e la conversione è valida...
                if (!AreSameType(context.ExpectedType, baseInferredType, context, program) &&
                    CanCoerce(baseInferredType, context.ExpectedType, context, program))
                {
                    TokenKind conversionKind;
                    // Determina la funzione di conversione corretta.
                    if (context.ExpectedType is DoubleType) conversionKind = TokenKind.CDbl;
                    else if (context.ExpectedType is SingleType) conversionKind = TokenKind.CDbl; /* SINGLE = double (parità C) */
                    else if (context.ExpectedType is CurrencyType) conversionKind = TokenKind.CCur;
                    else if (context.ExpectedType is LongType) conversionKind = TokenKind.CLng;
                    else if (context.ExpectedType is IntegerType) conversionKind = TokenKind.CInt;
                    else if (context.ExpectedType is ByteType) conversionKind = TokenKind.CByte;
                    else return ne; // Se la promozione non è verso un tipo numerico noto, non facciamo nulla.

                    var funcToken = new Token(conversionKind, conversionKind.ToString().ToUpper(), null, ne.Position);
                    var nsToken = new Token(TokenKind.Identifier, "CONVERSIONS", null, ne.Position);

                    // "Timbra" la promozione nell'AST avvolgendo il letterale in una chiamata a funzione esplicita.
                    // Es: 3000 -> CDBL(3000)
                    return new BuiltinFunctionCallExpr(nsToken, funcToken, [ne], context.ExpectedType);
                }
            }

            // Se non c'è un tipo atteso o non è necessaria la promozione, restituisce il nodo originale.
            return ne;
        }

        if (expr is StringExpr stringExpr)
        {
            // Un letterale stringa e' SEMPRE testo: `&` dentro le virgolette non e' una macro ("R&D", "AT&T", "a&b"). La macro e' l'operatore
            // `&nome` / `&(expr)` FUORI dai letterali. (Rimossa la "text substitution" che espandeva `"Hello &cName"` per i globali: alterava
            // silenziosamente le stringhe dei programmi a livello di modulo.)
            return stringExpr;
        }

        if (expr is BooleanExpr or NilExpr)
        {
            // Per gli altri letterali, la logica di promozione non si applica, quindi li restituiamo direttamente.
            return expr;
        }

        var newContextForChildren = context;

        Expr rewrittenExpr = expr switch
        {
            MacroExpr me => RewriteMacroExpr(me, newContextForChildren, program),
            EvalExpr ee => RewriteEvalExpr(ee, newContextForChildren, program),
            UnresolvedCallExpr uce => RewriteUnresolvedCall(uce, newContextForChildren, program),
            ArrayAccessExpr aae => RewriteArrayAccess(aae, newContextForChildren, program),
            FieldAccessExpr fae => RewriteFieldAccess(fae, newContextForChildren, program),
            VariableExpr ve => RewriteVariableExpr(ve, newContextForChildren, program, isMacroArgument),
            FunctionReferenceExpr fre => RewriteFunctionReferenceExpr(fre, newContextForChildren, program),
            BuiltinFunctionCallExpr bce => RewriteBuiltinFunctionCall(bce, newContextForChildren, program),
            FunctionExpr fe => RewriteFunctionExpr(fe, newContextForChildren, program),
            MethodCallExpr mce => RewriteMethodCall(mce, newContextForChildren, program),
            GoFunctionExpr gfe => RewriteGoFunctionExpr(gfe, newContextForChildren, program),
            PropertyAccessExpr pae => pae with
            {
                Instance = RewriteExpression(pae.Instance, newContextForChildren, program)
            },
            QualifiedNameExpr qne => RewriteQualifiedNameExpr(qne, newContextForChildren, program),
            HttpFetchExpr hfe => new HttpFetchExpr(
                RewriteExpression(hfe.Url, newContextForChildren, program),
                hfe.Options != null ? RewriteExpression(hfe.Options, newContextForChildren, program) : null,
                hfe.CallbackName
            ) { ScopeId = hfe.ScopeId },
            ResolveExpr re => new ResolveExpr(
                RewriteExpression(re.FutureToResolve, newContextForChildren, program)
            ) { ScopeId = re.ScopeId },
            IndexedAccessExpr iae => iae with
            {
                Base = RewriteExpression(iae.Base, newContextForChildren, program),
                Indices = [..iae.Indices.Select(i => RewriteExpression(i, newContextForChildren, program))]
            },
            DirectCastExpr dce => dce with
            {
                Expression = RewriteExpression(dce.Expression, newContextForChildren, program)
            },
            SafeCastExpr sce => new SafeCastExpr(
                RewriteExpression(sce.Expression, newContextForChildren, program),
                ResolveDataType(sce.TargetType, program, context.ModulePath),
                sce.AsToken) { ScopeId = sce.ScopeId },
            NewExpr nw => RewriteNewExpr(nw, newContextForChildren, program),
            LambdaExpr le => RewriteLambdaExprInDbContext(le, newContextForChildren, program),
            ArraySliceExpr ase => ase with
            {
                Base = RewriteExpression(ase.Base, newContextForChildren, program),
                Start = ase.Start != null ? RewriteExpression(ase.Start, newContextForChildren, program) : null,
                End = ase.End != null ? RewriteExpression(ase.End, newContextForChildren, program) : null
            },
            PrefixIncrementExpr pie => pie with
            {
                Target = RewriteTarget(pie.Target, newContextForChildren, program)
            },
            PostfixIncrementExpr poe => poe with
            {
                Target = RewriteTarget(poe.Target, newContextForChildren, program)
            },
            TernaryExpr te => te with
            {
                Condition = RewriteExpression(te.Condition, newContextForChildren with { ExpectedType = new BitType() },
                    program),
                TruePart = RewriteExpression(te.TruePart, newContextForChildren, program),
                FalsePart = RewriteExpression(te.FalsePart, newContextForChildren, program)
            },
            SwitchExpr se => se with
            {
                Value = RewriteExpression(se.Value, newContextForChildren, program),
                Cases = [..se.Cases.Select(c => new SwitchCaseClause(
                    RewriteExpression(c.Condition, newContextForChildren, program),
                    RewriteExpression(c.Result, newContextForChildren, program)))],
                ElseCase = se.ElseCase != null ? RewriteExpression(se.ElseCase, newContextForChildren, program) : null
            },
            BinaryExpr be => RewriteBinaryExpr(be, newContextForChildren, program),
            UnaryExpr ue => ue with { Right = RewriteExpression(ue.Right, newContextForChildren, program) },
            GroupingExpr ge => ge with
            {
                Expression = RewriteExpression(ge.Expression, newContextForChildren, program)
            },
            ArrayLiteralExpr ale => RewriteArrayLiteralExpr(ale, newContextForChildren, program),
            ListLiteralExpr lle => new ListLiteralExpr(
                [..lle.Elements.Select(e => RewriteExpression(e, newContextForChildren, program))],
                lle.OpenBracket
            ) { ScopeId = lle.ScopeId },
            DictionaryLiteralExpr dle => new DictionaryLiteralExpr(
                [
                    ..dle.Entries.Select(e => new DictionaryEntry(
                        RewriteExpression(NumericDictKeyOrSelf(e.Key, context.ExpectedType), newContextForChildren, program),
                        RewriteExpression(e.Value, newContextForChildren, program)
                    ))
                ],
                dle.OpenBrace
            ) { ScopeId = dle.ScopeId },
            // Tuple literal: riscrive gli ELEMENTI (constant folding delle costanti %K —
            // prima mancava il case, quindi un `DIM t = (%K, %K + 1)` lasciava %K come
            // VariableExpr non foldato -> local non inizializzato = 0, ConstStructuralCanary).
            TupleLiteralExpr tle => new TupleLiteralExpr(
                [..tle.Elements.Select(e => new NamedTupleElement(RewriteExpression(e.Value, newContextForChildren, program), e.Name))],
                tle.OpenParen, tle.CloseParen
            ) { ScopeId = tle.ScopeId },
            ListComprehensionExpr lce => RewriteListComprehension(lce, newContextForChildren, program),
            DictComprehensionExpr dce => RewriteDictComprehension(dce, newContextForChildren, program),
            SetComprehensionExpr sce => RewriteSetComprehension(sce, newContextForChildren, program),
            GeneratorExpr ge => RewriteGeneratorExpr(ge, newContextForChildren, program),
            AssignmentExpr ae => new AssignmentExpr(
                RewriteTarget(ae.Target, newContextForChildren, program),
                RewriteExpression(ae.Value, newContextForChildren, program),
                ae.Operator
            ) { ScopeId = ae.ScopeId },
            JsonToObjectExpr jtoe => new JsonToObjectExpr(
                ResolveDataType(jtoe.TargetType, program, context.ModulePath),
                RewriteExpression(jtoe.JsonSource, newContextForChildren, program),
                jtoe.ToObjectToken
            ) { ScopeId = jtoe.ScopeId },
            NullConditionalAccessExpr nca => new NullConditionalAccessExpr(
                RewriteExpression(nca.Expression, newContextForChildren, program),
                nca.Kind,
                nca.MemberToken,
                nca.Arguments?.Select(a => RewriteExpression(a, newContextForChildren, program)).ToImmutableArray()
            ) { ScopeId = nca.ScopeId },
            InterpolatedStringExpr ise => ise with
            {
                Segments = [..ise.Segments.Select(seg => seg switch
                {
                    InterpolatedStringExprSegment es => (InterpolatedStringSegment)(es with
                    {
                        Expression = RewriteExpression(es.Expression, newContextForChildren with { ExpectedType = null }, program)
                    }),
                    _ => seg
                })]
            },
            AwaitExpr awe => new AwaitExpr(
                RewriteExpression(awe.Target, newContextForChildren, program),
                awe.AwaitToken
            ) { ScopeId = awe.ScopeId },
            LinqQueryExpr lqe => RewriteLinqQueryAsComprehension(lqe, newContextForChildren, program),
            // Fase D (audit rewrite non uniforme, item 7): questi 5 tipi cadevano nel default
            // `_ => expr` senza riscrivere i loro figli Expr — %COSTANTI/chiamate a funzione
            // annidate in un WITH .Metodo(x), in a?.Value/a?.HasValue, in un importo con
            // valuta estera, o in una cella TBROWSE, restavano non risolte.
            ForeignAmountExpr fae2 => fae2 with
            {
                Amount = RewriteExpression(fae2.Amount, newContextForChildren, program),
                ExchangeRate = fae2.ExchangeRate != null ? RewriteExpression(fae2.ExchangeRate, newContextForChildren, program) : null
            },
            NullableHasValueExpr nhve => nhve with { Expression = RewriteExpression(nhve.Expression, newContextForChildren, program) },
            NullableValueExpr nve => nve with { Expression = RewriteExpression(nve.Expression, newContextForChildren, program) },
            TBrowseCellExpr tbce => tbce with
            {
                Row = RewriteExpression(tbce.Row, newContextForChildren, program),
                Col = RewriteExpression(tbce.Col, newContextForChildren, program)
            },
            WithMemberAccessExpr wmae => wmae with
            {
                Arguments = wmae.Arguments?.Select(a => RewriteExpression(a, newContextForChildren, program)).ToImmutableArray()
            },
            _ => expr
        };
        Log(
            $"[REWRITE-EXPR] <- Fine riscrittura. Nodo finale: {rewrittenExpr.GetType().Name} ('{rewrittenExpr}'). " +
            $"Inferred Type: {LinkerInferExpressionType(rewrittenExpr, context, program)}. " +
            $"Final ExpectedType: {context.ExpectedType?.ToString() ?? "N/A"}", log);
        return rewrittenExpr;
    }

    /// <summary>
    /// Riscrive un nodo ListComprehensionExpr. Questo metodo è cruciale per l'inferenza di tipo
    /// all'interno della comprensione, poiché crea uno scope locale temporaneo per la variabile
    /// iteratore, permettendo al corpo dell'espressione di essere risolto correttamente.
    /// </summary>
    /// <param name="lce">La ListComprehensionExpr originale.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Una nuova istanza di ListComprehensionExpr con le sotto-espressioni riscritte.</returns>
    private static Expr RewriteListComprehension(ListComprehensionExpr lce, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();

        // Inizializza lo scope cumulativo con quello esistente (se presente)
        var cumulativeScope = context.LambdaScope != null
            ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        var currentContext = context;

        foreach (var clause in lce.Clauses)
        {
            // 1. Riscrivi la sorgente nel contesto corrente (che include gli iteratori precedenti)
            var rewrittenSource = RewriteExpression(clause.SourceExpression, currentContext, program);

            // 2. Inferisci il tipo degli elementi
            var elementType = InferElementType(rewrittenSource, currentContext, program) ?? new InferredType();

            // 3. Aggiungi l'iteratore corrente allo scope per le prossime clausole/condizioni
            cumulativeScope[clause.IteratorVariable.Lexeme] = elementType;

            // Aggiorna anche il tipo nella SymbolTable se il simbolo è stato registrato
            if (clause.IteratorSymbolId >= 0)
            {
                program.Symbols.UpdateSymbolType(clause.IteratorSymbolId, elementType);
                // Nota: non serve UpdateSymbolDeclaration qui perché VariableDeclaration non cambia, cambia solo il tipo nel Symbol
            }

            // Aggiorna il contesto con il nuovo scope
            currentContext = context with { LambdaScope = cumulativeScope };

            // 4. Riscrivi la condizione (se presente) nel nuovo contesto
            var rewrittenCondition = clause.ConditionExpression != null
                ? RewriteExpression(clause.ConditionExpression, currentContext, program)
                : null;

            rewrittenClauses.Add(new ComprehensionClause(
                clause.IteratorSymbolId,
                clause.IteratorVariable,
                rewrittenSource,
                rewrittenCondition
            ));
        }

        // 5. Riscrivi l'espressione di output nel contesto finale (con tutti gli iteratori visibili)
        var rewrittenOutput = RewriteExpression(lce.OutputExpression, currentContext, program);

        return new ListComprehensionExpr(
            rewrittenOutput,
            rewrittenClauses.ToImmutable(),
            lce.OpenBracket,
            lce.OrderByClauses
        ) { ScopeId = lce.ScopeId };
    }

    /// <summary>
    /// Converte un LinqQueryExpr (FROM...SELECT) in ListComprehensionExpr per il runtime.
    /// ExecuteLinqQueryStep e' uno stub; la conversione usa l'implementazione comprehension.
    /// </summary>
    private static Expr RewriteLinqQueryAsComprehension(LinqQueryExpr lqe, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenSource = RewriteExpression(lqe.Source, context, program);
        var rewrittenWhere = lqe.WhereClause != null
            ? RewriteExpression(lqe.WhereClause, context, program) : null;
        var outputExpr = lqe.SelectClause ??
            new VariableExpr(lqe.ItemSymbolId, lqe.ItemVar, lqe.ItemVar);
        var clause = new ComprehensionClause(
            lqe.ItemSymbolId, lqe.ItemVar, rewrittenSource, rewrittenWhere);
        return RewriteListComprehension(new ListComprehensionExpr(
            outputExpr, [clause], lqe.FromToken, lqe.OrderBy ?? []) { ScopeId = lqe.ScopeId },
            context, program);
    }

    /// <summary>
    /// Riscrive un nodo DictComprehensionExpr, gestendo l'inferenza di tipo degli iteratori
    /// e la risoluzione delle espressioni Key e Value.
    /// </summary>
    private static Expr RewriteDictComprehension(DictComprehensionExpr dce, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();
        var cumulativeScope = context.LambdaScope != null
            ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        var currentContext = context;

        foreach (var clause in dce.Clauses)
        {
            var rewrittenSource = RewriteExpression(clause.SourceExpression, currentContext, program);
            var elementType = InferElementType(rewrittenSource, currentContext, program) ?? new InferredType();

            cumulativeScope[clause.IteratorVariable.Lexeme] = elementType;
            if (clause.IteratorSymbolId >= 0) program.Symbols.UpdateSymbolType(clause.IteratorSymbolId, elementType);

            currentContext = context with { LambdaScope = cumulativeScope };

            var rewrittenCondition = clause.ConditionExpression != null
                ? RewriteExpression(clause.ConditionExpression, currentContext, program)
                : null;

            rewrittenClauses.Add(new ComprehensionClause(
                clause.IteratorSymbolId,
                clause.IteratorVariable,
                rewrittenSource,
                rewrittenCondition
            ));
        }

        var rewrittenKey = RewriteExpression(dce.KeyExpression, currentContext, program);
        var rewrittenValue = RewriteExpression(dce.ValueExpression, currentContext, program);

        return new DictComprehensionExpr(
            rewrittenKey,
            rewrittenValue,
            rewrittenClauses.ToImmutable(),
            dce.OpenBrace
        ) { ScopeId = dce.ScopeId };
    }

    /// <summary>
    /// Riscrive un nodo SetComprehensionExpr, gestendo l'inferenza di tipo degli iteratori
    /// e la risoluzione dell'espressione dell'elemento.
    /// </summary>
    private static Expr RewriteSetComprehension(SetComprehensionExpr sce, AstRewriteContext context,
        BasicProgram program)
    {
        var rewrittenClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();
        var cumulativeScope = context.LambdaScope != null
            ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        var currentContext = context;

        foreach (var clause in sce.Clauses)
        {
            var rewrittenSource = RewriteExpression(clause.SourceExpression, currentContext, program);
            var elementType = InferElementType(rewrittenSource, currentContext, program) ?? new InferredType();

            cumulativeScope[clause.IteratorVariable.Lexeme] = elementType;
            if (clause.IteratorSymbolId >= 0) program.Symbols.UpdateSymbolType(clause.IteratorSymbolId, elementType);

            currentContext = context with { LambdaScope = cumulativeScope };

            var rewrittenCondition = clause.ConditionExpression != null
                ? RewriteExpression(clause.ConditionExpression, currentContext, program)
                : null;

            rewrittenClauses.Add(new ComprehensionClause(
                clause.IteratorSymbolId,
                clause.IteratorVariable,
                rewrittenSource,
                rewrittenCondition
            ));
        }

        var rewrittenElement = RewriteExpression(sce.ElementExpression, currentContext, program);

        return new SetComprehensionExpr(
            rewrittenElement,
            rewrittenClauses.ToImmutable(),
            sce.OpenBrace
        ) { ScopeId = sce.ScopeId };
    }

    private static Expr RewriteGeneratorExpr(GeneratorExpr ge, AstRewriteContext context, BasicProgram program)
    {
        var rewrittenClauses = ImmutableArray.CreateBuilder<ComprehensionClause>();
        var cumulativeScope = context.LambdaScope != null
            ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        var currentContext = context;

        foreach (var clause in ge.Clauses)
        {
            var rewrittenSource = RewriteExpression(clause.SourceExpression, currentContext, program);
            var elementType = InferElementType(rewrittenSource, currentContext, program) ?? new InferredType();

            cumulativeScope[clause.IteratorVariable.Lexeme] = elementType;
            if (clause.IteratorSymbolId >= 0) program.Symbols.UpdateSymbolType(clause.IteratorSymbolId, elementType);

            currentContext = context with { LambdaScope = cumulativeScope };

            var rewrittenCondition = clause.ConditionExpression != null
                ? RewriteExpression(clause.ConditionExpression, currentContext, program)
                : null;

            rewrittenClauses.Add(new ComprehensionClause(
                clause.IteratorSymbolId,
                clause.IteratorVariable,
                rewrittenSource,
                rewrittenCondition
            ));
        }

        var rewrittenOutput = RewriteExpression(ge.OutputExpression, currentContext, program);

        return new GeneratorExpr(
            rewrittenOutput,
            rewrittenClauses.ToImmutable(),
            ge.OpenParen
        ) { ScopeId = ge.ScopeId };
    }

    /// <summary>
    /// Riscrive un nodo BinaryExpr, applicando la validazione dei tipi per OPTION STRICT
    /// prima di scendere ricorsivamente.
    /// </summary>
    private static Expr RewriteBinaryExpr(BinaryExpr be, AstRewriteContext context, BasicProgram program)
    {
        var rewrittenLeft = RewriteExpression(be.Left, context, program);
        var rewrittenRight = RewriteExpression(be.Right, context, program);

        if (be.Op.Kind == TokenKind.In)
        {
            // Trasforma l'operatore IN nella chiamata al metodo di appartenenza corretto
            // in base al tipo della collezione a destra.
            return RewriteInOperator(rewrittenLeft, rewrittenRight, be.Op, context, program);
        }
        // 

        // --- INIZIO BLOCCO VALIDAZIONE OPTION STRICT ---
        if (context.IsInStrictContext)
        {
            var leftType = LinkerInferExpressionType(rewrittenLeft, context, program);
            var rightType = LinkerInferExpressionType(rewrittenRight, context, program);

            if (leftType is not InferredType && rightType is not InferredType)
            {
                var isLeftNumeric = IsNumericType(leftType);
                var isRightNumeric = IsNumericType(rightType);
                var isLeftString = leftType is StringType;
                var isRightString = rightType is StringType;

                // CASO 1: Operatore '+' con tipi misti (stringa e numero)
                if (be.Op.Kind == TokenKind.Plus && (isLeftString || isRightString) &&
                    (isLeftNumeric || isRightNumeric))
                {
                    throw CreateError(be.Op,
                        $"OPTION STRICT ON: L'operatore '+' non può essere applicato a operandi di tipo '{leftType}' e '{rightType}'. Per la concatenazione, convertire esplicitamente il numero in stringa usando CSTR().",
                        ErrorCode.TypeMismatch);
                }

                // CASO 2: Altri operatori con tipi diversi che non sono entrambi numerici
                if (leftType.GetType() != rightType.GetType() && !(isLeftNumeric && isRightNumeric))
                {
                    throw CreateError(be.Op,
                        $"OPTION STRICT ON: L'operatore '{be.Op.Lexeme}' non può essere applicato a operandi di tipo '{leftType}' e '{rightType}'.",
                        ErrorCode.TypeMismatch);
                }
            }
        }

        return new BinaryExpr(rewrittenLeft, be.Op, rewrittenRight) { ScopeId = be.ScopeId };
    }

    /// <summary>
    /// [FASE 3] Riscrive l'operatore binario 'IN' trasformandolo in una chiamata semantica valida
    /// (metodo di istanza o funzione di libreria) in base al tipo di dato della collezione a destra.
    /// </summary>
    /// <param name="left">L'elemento da cercare (operando sinistro).</param>
    /// <param name="right">La collezione o stringa in cui cercare (operando destro).</param>
    /// <param name="op">Il token 'IN' originale, usato per la posizione.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma.</param>
    /// <returns>Un nuovo nodo AST (BinaryExpr o MethodCallExpr) che implementa la verifica di appartenenza.</returns>
    private static Expr RewriteInOperator(Expr left, Expr right, Token op, AstRewriteContext context,
        BasicProgram program)
    {
        var rightType = LinkerInferExpressionType(right, context, program);
        var scopeId = left.ScopeId;

        // 1. Gestione Stringhe: "s" IN "source" -> (STRINGS.INSTR("source", "s") > 0)
        if (rightType is StringType)
        {
            var nsToken = new Token(TokenKind.Identifier, "STRINGS", null, op.Position);
            var funcToken = new Token(TokenKind.Instr, "INSTR", null, op.Position);
            var instrCall = new BuiltinFunctionCallExpr(nsToken, funcToken, [right, left], new LongType())
                { ScopeId = scopeId };

            var zeroExpr = new NumberExpr(0.0, new Token(TokenKind.Number, "0", 0.0, op.Position))
                { ScopeId = scopeId };
            var gtOp = new Token(TokenKind.GreaterThan, ">", null, op.Position);

            return new BinaryExpr(instrCall, gtOp, zeroExpr) { ScopeId = scopeId };
        }

        // 2. Gestione Array: x IN array() -> (ARRAYS.ASCAN(array(), x) > -1)
        if (rightType is ArrayType)
        {
            var nsToken = new Token(TokenKind.Identifier, "ARRAYS", null, op.Position);
            var funcToken = new Token(TokenKind.AScan, "ASCAN", null, op.Position);
            var ascanCall = new BuiltinFunctionCallExpr(nsToken, funcToken, [right, left], new LongType())
                { ScopeId = scopeId };

            var minusOneExpr = new NumberExpr(-1.0, new Token(TokenKind.Number, "-1", -1.0, op.Position))
                { ScopeId = scopeId };
            var gtOp = new Token(TokenKind.GreaterThan, ">", null, op.Position);

            return new BinaryExpr(ascanCall, gtOp, minusOneExpr) { ScopeId = scopeId };
        }

        // 3. Gestione Collezioni Native (List, Dict, HashSet, Tree, etc.)
        // Determina il metodo di ricerca: 'ContainsKey' per mappe e dizionari, 'Contains' per gli altri.
        string methodName = rightType switch
        {
            DictType or SortedDictType or TreeType or AVLTreeType or RedBlackTreeType or
                BPlusTreeType or CacheType or JsonType => "ContainsKey",
            _ => "Contains"
        };

        var methodToken = new Token(TokenKind.Identifier, methodName, null, op.Position);
        var methodCall = new MethodCallExpr(right, methodToken, [left]) { ScopeId = scopeId };

        // Invochiamo RewriteMethodCall per garantire che la nuova chiamata sia propriamente risolta
        // e collegata alla definizione del metodo nel rispettivo schema di tipo.
        return RewriteMethodCall(methodCall, context, program);
    }

    /// <summary>
    /// [FASE 3] Tenta di mappare gli argomenti di una chiamata sulla firma di una procedura.
    /// Restituisce la lista di argomenti risolti, oppure null se la firma non è compatibile.
    /// VERSIONE OTTIMIZZATA: Delega la gestione del PARAMARRAY a RewriteExpression.
    /// </summary>
    private static ImmutableArray<Expr>? RewriteArgumentsWithContext(
        ImmutableArray<Expr> args,
        ImmutableArray<Parameter> parameters,
        ProcedureDefinition? procDef,
        AstRewriteContext context,
        BasicProgram program)
    {
        bool hasParamArray = parameters.Length > 0 &&
                             (parameters.Last().Flags & SymbolFlags.IsParamArray) != 0;
        int fixedParamCount = hasParamArray ? parameters.Length - 1 : parameters.Length;

        Expr?[] mappedArgs = new Expr?[fixedParamCount];
        List<Expr> variadicArgs = new List<Expr>();
        bool namedUsed = false;
        int nextSlot = 0;

        // Passata 1: Distribuzione argomenti (Identica a prima)
        for (int i = 0; i < args.Length; i++)
        {
            Expr arg = args[i];
            if (arg is NamedArgumentExpr nae || (arg is AssignmentExpr
                {
                    Operator.Kind: TokenKind.InlineAssign, Target: VariableTarget
                } ae))
            {
                namedUsed = true;
                string name = (arg is NamedArgumentExpr n)
                    ? n.Name.Lexeme
                    : ((VariableTarget)((AssignmentExpr)arg).Target).NameCleaned.Lexeme;
                Expr val = (arg is NamedArgumentExpr n2) ? n2.Value : ((AssignmentExpr)arg).Value;

                int idx = -1;
                for (int j = 0; j < fixedParamCount; j++)
                    if (parameters[j].Name.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase))
                    {
                        idx = j;
                        break;
                    }

                if (idx != -1)
                {
                    if (mappedArgs[idx] != null) return null; // Duplicato
                    mappedArgs[idx] = val;
                }
                else if (hasParamArray &&
                         parameters.Last().Name.Lexeme.Equals(name, StringComparison.OrdinalIgnoreCase))
                {
                    variadicArgs.Add(val);
                }
                else return null; // Parametro non trovato
            }
            else
            {
                if (namedUsed && !hasParamArray)
                    return null; // Argomento posizionale dopo uno nominale non è permesso (a meno di paramarray)
                while (nextSlot < fixedParamCount && mappedArgs[nextSlot] != null) nextSlot++;
                if (nextSlot < fixedParamCount) mappedArgs[nextSlot++] = arg;
                else if (hasParamArray) variadicArgs.Add(arg);
                else return null; // Troppi argomenti
            }
        }

        // Passata 2: Riempimento e Riscrittura parametri fissi
        ImmutableArray<Expr>.Builder finalBuilder = ImmutableArray.CreateBuilder<Expr>();
        int lastRequired = -1;
        for (int i = 0; i < fixedParamCount; i++)
            if (!parameters[i].Flags.HasFlag(SymbolFlags.IsOptional))
                lastRequired = i;
        int lastProvided = -1;
        for (int i = 0; i < fixedParamCount; i++)
            if (mappedArgs[i] != null)
                lastProvided = i;

        int effectiveCount = Math.Max(lastRequired + 1, lastProvided + 1);

        for (int i = 0; i < effectiveCount; i++)
        {
            Expr? argExpr = mappedArgs[i];
            if (argExpr == null)
            {
                if (parameters[i].Flags.HasFlag(SymbolFlags.IsOptional))
                    argExpr = parameters[i].DefaultValue ?? new NilExpr(new Token(TokenKind.Nil, "NIL", null,
                        procDef?.Position ?? SourcePosition.Unknown));
                else return null; // Obbligatorio mancante
            }

            finalBuilder.Add(RewriteSingleArgument(argExpr, parameters[i], context, program));
        }

        // Passata 3: Gestione Semplificata PARAMARRAY
        if (hasParamArray)
        {
            Parameter p = parameters.Last();
            // p.DataType è il tipo dell'array (es. ArrayType(Double)).
            
            // Caso speciale: Passaggio diretto di un singolo array.
            // - LETTERALE array (`F({})`, `F({1,2})`): è GIÀ l'array del parametro, non va
            //   incapsulato un'altra volta. Il caso `F()` veniva riscritto due volte (il
            //   secondo passaggio vedeva l'array vuoto sintetico come singolo argomento) e
            //   diventava `{{}}` = array di UN elemento → LEN 1, FOR EACH su un elemento
            //   spurio (valore garbage ~1e-314 su IL; Somma(vuota) in FunctionsParams).
            // - VARIABILE array con elemento dello stesso tipo, oppure `<inferred>`
            //   (`DIM a() = {}` non ha un tipo elemento dedotto): si passa direttamente e
            //   il contesto ExpectedType del parametro la tipizza.
            bool directArrayArg = false;
            if (variadicArgs.Count == 1)
            {
                var singleArg = variadicArgs[0];
                if (singleArg is ArrayLiteralExpr) directArrayArg = true;
                else if (LinkerInferExpressionType(singleArg, context, program) is ArrayType vat &&
                         (AreSameType(vat.ElementType, GetBaseType(p.DataType), context, program) ||
                          vat.ElementType is InferredType))
                    directArrayArg = true;
            }
            if (directArrayArg)
            {
                 // Se passiamo un array che matcha il tipo elemento, lo passiamo direttamente
                 finalBuilder.Add(RewriteSingleArgument(variadicArgs[0], p, context, program));
            }
            else
            {
                // Caso standard: Argomenti variadici sparsi.
                // 1. Creiamo un ArrayLiteralExpr sintetico con gli argomenti grezzi.
                var rawArrayLiteral = new ArrayLiteralExpr(
                    [..variadicArgs], // Argomenti non ancora riscritti
                    new Token(TokenKind.LeftBrace, "{", null, procDef?.Position ?? SourcePosition.Unknown));

                // 2. Creiamo un contesto che si aspetta il tipo del parametro PARAMARRAY (es. ARRAY OF DOUBLE).
                var paramContext = context with { ExpectedType = p.DataType };

                // 3. Deleghiamo a RewriteExpression. 
                // Questo invocherà RewriteArrayLiteralExpr, che farà:
                // - Inferenza del tipo degli elementi
                // - Verifica compatibilità con ExpectedType (p.DataType)
                // - Iniezione automatica di CDBL/CINT per coercizione numerica.
                var rewrittenArrayArg = RewriteExpression(rawArrayLiteral, paramContext, program);

                finalBuilder.Add(rewrittenArrayArg);
            }
        }

        return finalBuilder.ToImmutable();
    }

    /// <summary>
    /// Helper per la riscrittura atomica di un argomento con iniezione di contesto e risoluzione implicita.
    /// </summary>
    private static Expr RewriteSingleArgument(Expr arg, Parameter param, AstRewriteContext context,
        BasicProgram program)
    {
        var argContext = context with { ExpectedType = param.DataType };

        if ((param.Flags & SymbolFlags.IsByRef) != 0 && arg is VariableExpr { SymbolId: < 0 } ve)
        {
            var symbolIds = program.Symbols.FindSymbolInScope(ve.NameCleaned.Lexeme);
            if (symbolIds == null || !symbolIds.Any())
            {
                var newVarType = param.DataType is not InferredType ? param.DataType : new InferredType();
                var newId = program.Symbols.RegisterSymbol(ve.NameCleaned, SymbolFlags.IsLocal, newVarType,
                    ve);
                context.OnTypeInferred?.Invoke();
                return ve with { SymbolId = newId, ScopeId = context.CurrentScopeId ?? 0 };
            }
        }

        if (arg is LambdaExpr lambda && param.DataType is FunctionPointerType fpt)
        {
            var scope = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
            for (var k = 0; k < lambda.Parameters.Length && k < fpt.ParameterTypes.Length; k++)
                scope[lambda.Parameters[k].OriginalName.Lexeme] = fpt.ParameterTypes[k];
            return RewriteExpression(lambda, argContext with { LambdaScope = scope }, program);
        }

        return RewriteExpression(arg, argContext, program);
    }

    /// <summary>
    /// [NUOVO HELPER] Riscrive una lista di statement, propagando correttamente il contesto di riscrittura.
    /// Sostituisce le chiamate errate a un metodo non esistente.
    /// </summary>
    private static (ImmutableArray<Stmt> Rewritten, AstRewriteContext NewContext) RewriteStatementList(
        ImmutableArray<Stmt> statements, AstRewriteContext context, ProcedureDefinition? procedureContext,
        BasicProgram program)
    {
        var builder = ImmutableArray.CreateBuilder<Stmt>();
        var currentContext = context;
        foreach (var stmt in statements)
        {
            (var rewrittenStmt, currentContext) = RewriteStatement(stmt, currentContext, procedureContext, program);
            builder.Add(rewrittenStmt);
        }

        return (builder.ToImmutable(), currentContext);
    }

    /// <summary>
    /// [HELPER UNIFICATO E CORRETTO] Riscrive un nodo LambdaExpr.
    /// Applica i tipi inferiti dal contesto ai parametri, aggiorna la SymbolTable, 
    /// e inferisce il tipo di ritorno analizzando il corpo.
    /// </summary>
    private static LambdaExpr RewriteLambdaExprInDbContext(LambdaExpr le, AstRewriteContext context,
        BasicProgram program)
    {
        ImmutableArray<Parameter>.Builder rewrittenParamsBuilder = le.Parameters.ToBuilder();

        // Crea una copia dello scope per popolare i tipi dei parametri durante l'inferenza.
        Dictionary<string, DataType> effectiveLambdaScope = context.LambdaScope != null
            ? new Dictionary<string, DataType>(context.LambdaScope, StringComparer.OrdinalIgnoreCase)
            : new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);

        // I tipi ESPLICITI dei parametri (AS CURRENCY/AS Vendita) sovrascrivono gli Inferred
        // del LambdaScope del contesto: AREDUCE/AMAP dichiarano i parametri del callback
        // InferredType → lo scope della chiamata popola totale→Inferred, che mascherava il
        // tipo dichiarato → il corpo veniva inferito DoubleType invece di CurrencyType (il
        // delegate diventava Func<decimal,decimal,double> vs AReduce<decimal> → IL invalido).
        for (int i = 0; i < rewrittenParamsBuilder.Count; i++)
        {
            var p0 = rewrittenParamsBuilder[i];
            if (p0.DataType is not InferredType and not UnresolvedQualifiedType)
                effectiveLambdaScope[p0.OriginalName.Lexeme] = p0.DataType;
        }

        // --- Priorità 0: risoluzione dei tipi ESPLICITI dei parametri (es. `o AS Vendita`):
        // il parser produce UnresolvedQualifiedType (i tipi si risolvono nel Linker) e solo
        // DIM/parametri di procedure passano da ResolveDataType — i parametri espliciti delle
        // LAMBDA restavano UnresolvedQualifiedType → il backend IL li mappava a object
        // (delegate Func<object,...> → cast incompatibile + IL invalido su campi UDT).
        for (int i = 0; i < rewrittenParamsBuilder.Count; i++)
        {
            Parameter param = rewrittenParamsBuilder[i];
            if (param.DataType is not UnresolvedQualifiedType and not InferredType) continue;
            var resolvedParamType = ResolveDataType(param.DataType, program, context.ModulePath);
            if (resolvedParamType is not null and not UnresolvedQualifiedType and not InferredType)
            {
                Parameter updatedParam = param with { DataType = resolvedParamType };
                rewrittenParamsBuilder[i] = updatedParam;
                if (updatedParam.SymbolId >= 0)
                {
                    program.Symbols.UpdateSymbolType(updatedParam.SymbolId, resolvedParamType);
                    program.Symbols.UpdateSymbolDeclaration(updatedParam.SymbolId, updatedParam);
                }
            }
        }

        // --- INIZIO LOGICA DI INFERENZA A DUE LIVELLI ---

        // Priorità 1: LAMBDA SCOPE (da AEVAL, AFILTER, etc.)
        if (context.LambdaScope != null)
        {
            for (int i = 0; i < rewrittenParamsBuilder.Count; i++)
            {
                Parameter param = rewrittenParamsBuilder[i];
                if (param.DataType is InferredType &&
                    effectiveLambdaScope.TryGetValue(param.OriginalName.Lexeme, out DataType? inferredType) &&
                    inferredType is not InferredType)
                {
                    Parameter updatedParam = param with { DataType = inferredType };
                    rewrittenParamsBuilder[i] = updatedParam;

                    if (updatedParam.SymbolId >= 0)
                    {
                        program.Symbols.UpdateSymbolType(updatedParam.SymbolId, inferredType);
                        program.Symbols.UpdateSymbolDeclaration(updatedParam.SymbolId, updatedParam);
                    }

                    context.OnTypeInferred?.Invoke();
                }
            }
        }
        // Priorità 2: TARGET TYPING (da assegnazione a delegato)
        else if (context.ExpectedType is FunctionPointerType expectedFuncType)
        {
            for (int i = 0; i < rewrittenParamsBuilder.Count; i++)
            {
                if (i < expectedFuncType.ParameterTypes.Length)
                {
                    Parameter param = rewrittenParamsBuilder[i];
                    DataType expectedParamType = expectedFuncType.ParameterTypes[i];
                    if (param.DataType is InferredType && expectedParamType is not InferredType)
                    {
                        Parameter updatedParam = param with { DataType = expectedParamType };
                        rewrittenParamsBuilder[i] = updatedParam;

                        effectiveLambdaScope[updatedParam.OriginalName.Lexeme] = expectedParamType;

                        if (updatedParam.SymbolId >= 0)
                        {
                            program.Symbols.UpdateSymbolType(updatedParam.SymbolId, expectedParamType);
                            program.Symbols.UpdateSymbolDeclaration(updatedParam.SymbolId, updatedParam);
                        }

                        context.OnTypeInferred?.Invoke();
                    }
                }
            }
        }
        // --- FINE LOGICA DI INFERENZA A DUE LIVELLI ---

        ImmutableArray<Parameter> rewrittenParams = rewrittenParamsBuilder.ToImmutable();
        AstRewriteContext lambdaContext = context with { LambdaScope = effectiveLambdaScope, ExpectedType = null };

        // Riscrive il corpo
        Expr? rewrittenExprBody = le.ExpressionBody != null
            ? RewriteExpression(le.ExpressionBody, lambdaContext, program)
            : null;

        ImmutableArray<Stmt>? rewrittenBlockBody = null;
        if (le.BlockBody.HasValue)
        {
            (rewrittenBlockBody, _) =
                RewriteStatementList(le.BlockBody.Value, lambdaContext, context.CurrentProcedure, program);
        }

        // Inferenza dei parametri dall'USO nel corpo riscritto (es. UPPER(s) → s STRING):
        // senza, sul backend IL la firma del delegate restava double mentre il corpo trattava
        // l'argomento come stringa → InvalidProgramException (PCode/VM-C mascheravano).
        {
            IEnumerable<AstNode>? lambdaBodyNodes = rewrittenBlockBody.HasValue
                ? rewrittenBlockBody.Value
                : rewrittenExprBody != null
                    ? [rewrittenExprBody]
                    : null;
            InferLambdaParameterTypesFromUsage(rewrittenParamsBuilder, lambdaBodyNodes, effectiveLambdaScope,
                program);
            rewrittenParams = rewrittenParamsBuilder.ToImmutable();
        }

        // Inferisce il tipo di ritorno analizzando il corpo appena riscritto
        DataType returnType = le.ReturnType;
        if (returnType is InferredType)
        {
            DataType? newReturnType = null;
            if (rewrittenExprBody != null)
            {
                newReturnType = LinkerInferExpressionType(rewrittenExprBody, lambdaContext, program);
            }
            else if (rewrittenBlockBody.HasValue)
            {
                List<ReturnStmt> returns =
                    rewrittenBlockBody.Value.OfType<ReturnStmt>().Where(r => r.Value != null).ToList();
                newReturnType = returns.Count > 0
                    ? LinkerInferExpressionType(returns[0].Value!, lambdaContext, program)
                    : new VoidType();
            }

            if (newReturnType != null && newReturnType is not InferredType)
            {
                returnType = newReturnType;
                context.OnTypeInferred?.Invoke();
            }
        }

        // Adattamento SCALARE→ARRAY (bug Lambda-Param-Array-Indicizzato-Cross-Backend, AiWiki,
        // trovato 2026-09-16): un callback ODE-style (es. NUMERICAL.SolveIvp) dichiara sempre
        // ReturnType=ArrayType — anche per uno stato a UNA componente, il chiamante nativo si
        // aspetta SEMPRE un vettore derivate (common/IlBackendRuntime/IlRuntime.cs SolveIvp:
        // Func<double,double[],double[]>). Una λ codeblock senza annotazioni esplicite (es.
        // `{|t, y| RETURN -y(0)}`, a differenza di `FN(...) AS ARRAY OF DOUBLE => {-y(0)}`)
        // inferisce il tipo di ritorno dal CORPO (sopra): un'espressione scalare nuda produce
        // DoubleType, non ArrayType — la λ compilava con una firma delegate SBAGLIATA
        // (Func<double,double[],double> invece di Func<double,double[],double[]>), causando
        // comportamento indefinito a runtime (osservato: IndexOutOfRangeException/valori
        // garbage). Se il contesto chiamante si aspetta un ArrayType e il corpo produce uno
        // scalare, avvolgi il valore di ritorno in un letterale array a UN elemento — l'unica
        // interpretazione sensata per una λ senza annotazioni esplicite (un ODE a una sola
        // componente).
        if (context.ExpectedType is FunctionPointerType expectedFnRet
            && expectedFnRet.ReturnType is ArrayType
            && returnType is not ArrayType and not InferredType and not VoidType)
        {
            var openBrace = new Token(TokenKind.LeftBrace, "{", null, le.FuncToken.Position);
            if (rewrittenExprBody != null)
            {
                rewrittenExprBody = new ArrayLiteralExpr([rewrittenExprBody], openBrace);
            }
            else if (rewrittenBlockBody.HasValue)
            {
                rewrittenBlockBody = rewrittenBlockBody.Value.Select(st =>
                    st is ReturnStmt rs && rs.Value != null
                        ? (Stmt)(rs with { Value = new ArrayLiteralExpr([rs.Value], openBrace) })
                        : st
                ).ToImmutableArray();
            }
            returnType = expectedFnRet.ReturnType;
        }

        return le with
        {
            Parameters = rewrittenParams,
            ReturnType = returnType,
            ExpressionBody = rewrittenExprBody,
            BlockBody = rewrittenBlockBody
        };
    }

    /// <summary>
    /// Riscrive un nodo FunctionExpr, risolvendo la chiamata e gestendo l'inferenza contestuale
    /// per le lambda (Target Typing) e la creazione implicita di variabili per parametri BYREF.
    /// VERSIONE UNIFICATA: Utilizza l'helper RewriteArgumentsWithContext per tutte le procedure.
    /// </summary>
    [SuppressMessage("Performance", "CA1859:Usare tipi concreti quando possibile per migliorare le prestazioni")]
    private static Expr RewriteFunctionExpr(FunctionExpr fe, AstRewriteContext context, BasicProgram program)
    {
        int symbolId = fe.SymbolId;

        // --- CASO 0: Risoluzione ID (se mancante) ---
        if (symbolId < 0)
        {
            List<ProcedureDefinition> candidates =
                FindProcedureCandidatesInScope(fe.OriginalToken.Lexeme, context, program);
            List<ProcedureDefinition> compatible = candidates
                .Where(p => SymbolTable.IsProcedureSignatureCompatible(p, fe.Args.Length))
                .ToList();

            if (compatible.Count == 1)
            {
                symbolId = compatible[0].SymbolId;
            }
            else if (compatible.Count > 1)
            {
                throw CreateError(fe.OriginalToken, "Chiamata ambigua...", ErrorCode.AmbiguousCall);
            }
            else
            {
                throw CreateError(fe.OriginalToken, $"Funzione o subroutine non definita: '{fe.OriginalToken.Lexeme}'",
                    ErrorCode.UndefinedSubOrFunction);
            }
        }

        // --- CASO SPECIAL: Specializzazione generica (ID negativo) ---
        if (program.ResolvedSpecializations.TryGetValue(symbolId, out var specFd))
        {
            // Specializzazione già creata in un passaggio precedente — usa direttamente.
            // Fase A audit (2026-08-12): Args = fe.Args copiava gli argomenti GREZZI — ogni
            // chiamata successiva alla PRIMA a una funzione generica già specializzata saltava
            // interamente RewriteArgumentsWithContext (niente folding %NOME, niente coercizione
            // di ExpectedType, niente creazione implicita di variabile per BYREF).
            var mappingResultCached =
                RewriteArgumentsWithContext(fe.Args, specFd.Parameters, specFd, context, program);
            if (!mappingResultCached.HasValue)
            {
                throw CreateError(fe.OriginalToken,
                    $"Gli argomenti forniti non sono validi per la funzione '{specFd.OriginalName.Lexeme}'.");
            }
            return fe with
            {
                SymbolId = symbolId,
                Args = mappingResultCached.Value,
                ReturnType = specFd.ReturnType,
                ResolvedDefinition = specFd
            };
        }

        SymbolTable.Symbol symbol = program.Symbols.GetSymbol(symbolId);

        // --- CASO 1: Chiamata Indiretta (Function Pointer) ---
        if (symbol.DeclarationNode is not ProcedureDefinition && symbol.DataType is FunctionPointerType fpt1)
        {
            ImmutableArray<Parameter> dummyParams =
            [
                .. fpt1.ParameterTypes.Select((t, i) =>
                    new Parameter(-1, new Token(TokenKind.Identifier, $"p{i}", null, SourcePosition.Unknown), t,
                        SymbolFlags.None, null, false)
                )
            ];

            // Usa l'helper unificato anche qui, passando i parametri fittizi del delegato.
            ImmutableArray<Expr>? mappingResult =
                RewriteArgumentsWithContext(fe.Args, dummyParams, null, context, program);

            if (!mappingResult.HasValue)
            {
                throw CreateError(fe.OriginalToken, "Numero di argomenti non valido per la chiamata indiretta.",
                    ErrorCode.ArgumentCountMismatch);
            }

            return fe with
            {
                SymbolId = symbolId,
                Args = mappingResult.Value,
                ReturnType = fpt1.ReturnType,
                ResolvedDefinition = null
            };
        }

        if (symbol.DeclarationNode is not ProcedureDefinition resolvedProc)
        {
            throw CreateError(fe.OriginalToken, $"'{fe.OriginalToken.Lexeme}' non è una funzione...",
                ErrorCode.IllegalFunctionCall);
        }

        // --- GENERIC SPECIALIZATION ---
        // Specializza solo se i type parameters sono effettivamente usati nei parametri o nel return type.
        if (resolvedProc is FunctionDefinition genericFd && !genericFd.TypeParameters.IsDefaultOrEmpty && GenericUsesTypeParameters(genericFd))
        {
            var typeArgs = InferTypeArguments(genericFd, fe.Args, context, program);
            if (typeArgs != null)
            {
                var specialized = SpecializeGenericFunction(genericFd, typeArgs, program);
                var specId = program.Symbols.RegisterSymbol(
                    specialized.Name,
                    genericFd.Flags | SymbolFlags.IsFunction,
                    specialized.ReturnType,
                    specialized);
                specialized = specialized with { SymbolId = specId };
                // Vedi Linker.Rewrites.cs (stessa specializzazione): la DeclarationNode deve
                // contenere l'id, altrimenti il rewrite dei simboli di alto livello trova -1.
                program.Symbols.UpdateSymbolDeclaration(specId, specialized);
                program.ResolvedSpecializations[specId] = specialized;
                fe = fe with { SymbolId = specId, ReturnType = specialized.ReturnType };
                resolvedProc = specialized;
            }
        }

        // --- CASO 2 & 3 UNIFICATO: Chiamata a Procedura Diretta (Built-in o UDF) ---
        // Catturiamo il risultato nullable del mapping
        ImmutableArray<Expr>? mappingResultDirect =
            RewriteArgumentsWithContext(fe.Args, resolvedProc.Parameters, resolvedProc, context, program);

        if (!mappingResultDirect.HasValue)
        {
            throw CreateError(fe.OriginalToken,
                $"Gli argomenti forniti non sono validi per la funzione '{resolvedProc.OriginalName.Lexeme}'.");
        }

        // Estraiamo il valore non nullable
        ImmutableArray<Expr> rewrittenArgs = mappingResultDirect.Value;

        // Se la procedura è un built-in, la trasformiamo in un BuiltinFunctionCallExpr per coerenza.
        if (resolvedProc is NativeProcedureDefinition nativeProc)
        {
            SymbolTable.Scope? nsScope =
                program.Symbols.FindParentScopeOfType(symbol.ScopeId, SymbolTable.ScopeType.Namespace);
            string nsName = nsScope?.Name ?? "SYSTEM";
            Token nsToken = new Token(TokenKind.Identifier, nsName, null, fe.OriginalToken.Position);

            BuiltinFunctionCallExpr bce =
                new BuiltinFunctionCallExpr(nsToken, nativeProc.OriginalName, rewrittenArgs, nativeProc.ReturnType)
                {
                    ScopeId = fe.ScopeId,
                    SourceLineIndex = fe.SourceLineIndex
                };

            DataType finalBuiltinType = InferGenericPolymorphicReturnType(bce, context, program);
            return bce with { ReturnType = finalBuiltinType };
        }

        DataType finalReturnType = resolvedProc switch
        {
            FunctionDefinition fd => InferUdfReturnType(fd, context, program),
            GeneratorDefinition gd => new GeneratorType(gd.YieldType),
            _ => new VoidType()
        };

        if (resolvedProc is FunctionDefinition userFuncDef)
        {
            FunctionDefinition? currentDef = GetCurrentProcedureDefinition(program, userFuncDef);
            if (currentDef != null && !currentDef.ReturnType.Equals(finalReturnType))
            {
                UpdateProcedureReturnTypeInSymbolTable(program, userFuncDef, finalReturnType);
                resolvedProc = GetCurrentProcedureDefinition(program, userFuncDef) ?? resolvedProc;
            }
        }

        return fe with
        {
            SymbolId = symbolId,
            Args = rewrittenArgs,
            ReturnType = finalReturnType,
            ResolvedDefinition = resolvedProc
        };
    }

    /// <summary>
    /// Riscrive un nodo FunctionReferenceExpr, recuperando la procedura concreta
    /// tramite il suo SymbolId e popolando il nodo con il tipo corretto.
    /// QUESTA VERSIONE È CORRETTA: restituisce un FunctionReferenceExpr,
    /// preservando la semantica di puntatore e non trasformandolo in una chiamata.
    /// </summary>
    private static FunctionReferenceExpr RewriteFunctionReferenceExpr(FunctionReferenceExpr fre,
        AstRewriteContext context,
        BasicProgram program)
    {
        var symbolId = fre.SymbolId;

        // Se il Parser non ha trovato un simbolo univoco, l'ID sarà -1.
        if (symbolId < 0)
        {
            throw new BasicException(fre.OriginalToken,
                $"Riferimento alla funzione '{fre.OriginalToken.Lexeme}' non risolto o ambiguo.",
                ErrorCode.UndefinedSubOrFunction);
        }

        var symbol = program.Symbols.GetSymbol(symbolId);

        if (symbol.DeclarationNode is not ProcedureDefinition resolvedProc)
        {
            // L'ID punta a un simbolo che non è una procedura.
            throw new BasicException(fre.OriginalToken,
                $"Il simbolo '{fre.OriginalToken.Lexeme}' non è una funzione o subroutine e non può essere referenziato.",
                ErrorCode.TypeMismatch);
        }

        // Determina il tipo di dato (FunctionPointerType) della procedura referenziata.
        // Questo è fondamentale per il type-checking successivo.
        var paramTypes = resolvedProc.Parameters.Select(p => p.DataType).ToImmutableArray();

        var returnType = resolvedProc switch
        {
            FunctionDefinition fd => ResolveDataType(fd.ReturnType, program, context.ModulePath),
            NativeProcedureDefinition npd => ResolveDataType(npd.ReturnType, program, context.ModulePath),
            ExternalProcedureDefinition epd => epd.ReturnType != null
                ? ResolveDataType(epd.ReturnType, program, context.ModulePath)
                : new VoidType(),
            GeneratorDefinition gd => new GeneratorType(gd.YieldType),
            _ => new VoidType() // Per SUB e altre procedure
        };

        // Restituisce un FunctionReferenceExpr aggiornato, NON un FunctionExpr.
        return new FunctionReferenceExpr(symbolId, fre.OriginalToken);
    }

    /// <summary>
    /// Riscrive un nodo GoFunctionExpr, validando la chiamata alla funzione target e impostando il tipo di ritorno corretto.
    /// QUESTA VERSIONE È ARCHITETTURALMENTE CORRETTA.
    /// </summary>
    private static Expr RewriteGoFunctionExpr(GoFunctionExpr gfe, AstRewriteContext context, BasicProgram program)
    {
        int symbolId = gfe.SymbolId;

        // Se ID non è risolto, cerchiamo candidati
        if (symbolId < 0)
        {
            List<ProcedureDefinition> candidates =
                FindProcedureCandidatesInScope(gfe.OriginalToken.Lexeme, context, program);
            List<ProcedureDefinition> compatible = candidates
                .Where(p => SymbolTable.IsProcedureSignatureCompatible(p, gfe.Args.Length))
                .ToList();
            if (compatible.Count == 1)
            {
                symbolId = compatible[0].SymbolId;
            }
        }

        ImmutableArray<Expr> rewrittenArgs;
        if (symbolId >= 0)
        {
            SymbolTable.Symbol symbol = program.Symbols.GetSymbol(symbolId);
            if (symbol.DeclarationNode is ProcedureDefinition resolvedProc)
            {
                // Gestione del risultato nullable per il meccanismo di soft-fail degli overload
                ImmutableArray<Expr>? mappingResult = RewriteArgumentsWithContext(gfe.Args, resolvedProc.Parameters,
                    resolvedProc, context, program);

                if (!mappingResult.HasValue)
                {
                    throw CreateError(gfe.OriginalToken,
                        $"Gli argomenti forniti non sono validi per la funzione '{resolvedProc.OriginalName.Lexeme}' avviata in background.");
                }

                rewrittenArgs = mappingResult.Value;
            }
            else
            {
                // Fallback per simboli risolti che non sono direttamente definizioni di procedura (es. puntatori)
                rewrittenArgs = [.. gfe.Args.Select(arg => RewriteExpression(arg, context, program))];
            }
        }
        else
        {
            // Se non è stato possibile risolvere la funzione, solleviamo l'errore prima della creazione del nodo
            throw CreateError(gfe.OriginalToken, $"Funzione non definita per GO FUNCTION: '{gfe.OriginalToken.Lexeme}'",
                ErrorCode.UndefinedSubOrFunction);
        }

        Expr? rewrittenWaitGroup = gfe.WaitGroup != null ? RewriteExpression(gfe.WaitGroup, context, program) : null;

        return gfe with
        {
            Args = rewrittenArgs,
            WaitGroup = rewrittenWaitGroup,
            SymbolId = symbolId,
            ReturnType = new FutureType()
        };
    }

    /// <summary>
    /// [FASE 3 - HELPER] Riscrive un nodo ArrayLiteralExpr.
    /// Inferisce il tipo comune più ampio per gli elementi, forza questo tipo su ogni elemento
    /// (eventualmente wrappando in CDBL), e restituisce un nuovo AST internamente coerente.
    /// </summary>
    private static ArrayLiteralExpr RewriteArrayLiteralExpr(ArrayLiteralExpr ale, AstRewriteContext context,
        BasicProgram program)
    {
        // FASE 1: Inferisce il tipo finale e corretto dell'intero letterale array.
        // Questa chiamata ora funziona correttamente grazie alle correzioni precedenti.
        var finalArrayType = LinkerInferExpressionType(ale, context, program);

        // FASE 2: Estrae il tipo di elemento atteso dal tipo dell'array inferito.
        DataType? expectedElementType = (finalArrayType as ArrayType)?.ElementType;

        // Se non riusciamo a determinare un tipo elemento, non possiamo fare nulla di intelligente.
        if (expectedElementType == null || expectedElementType is InferredType)
        {
            // Fallback: riscrivi semplicemente gli elementi con il contesto originale.
            var rewrittenElementsSimple = ale.Elements
                .Select(e => RewriteExpression(e, context, program))
                .ToImmutableArray();
            return new ArrayLiteralExpr(rewrittenElementsSimple, ale.OpenBrace) { ScopeId = ale.ScopeId };
        }

        // FASE 3: Crea un nuovo contesto di riscrittura specifico per gli elementi.
        var elementContext = context with { ExpectedType = expectedElementType };

        // FASE 4: Riscrive ogni elemento, FORZANDO la coerenza del tipo.
        var rewrittenElements = ale.Elements.Select(e =>
        {
            var rewrittenElement = RewriteExpression(e, elementContext, program);
            var inferredElementType = LinkerInferExpressionType(rewrittenElement, elementContext, program);

            // Se il tipo inferito dell'elemento non è già quello atteso, e può essere promosso...
            if (!AreSameType(inferredElementType, expectedElementType, context, program) &&
                CanCoerce(inferredElementType, expectedElementType, context, program))
            {
                // ...e la promozione richiesta è verso un tipo a virgola mobile...
                if (expectedElementType is DoubleType || expectedElementType is SingleType ||
                    expectedElementType is CurrencyType)
                {
                    // ...lo avvolgiamo in una chiamata esplicita a CDBL() o equivalente.
                    // Questo "timbra" il tipo corretto nell'AST in modo inequivocabile.
                    var conversionTokenKind = expectedElementType is SingleType ? TokenKind.CDbl :
                        expectedElementType is CurrencyType ? TokenKind.CCur :
                        TokenKind.CDbl;

                    var funcToken = new Token(conversionTokenKind, conversionTokenKind.ToString().ToUpper(), null,
                        rewrittenElement.Position);
                    var nsToken = new Token(TokenKind.Identifier, "CONVERSIONS", null, rewrittenElement.Position);

                    return new BuiltinFunctionCallExpr(nsToken, funcToken, [rewrittenElement], expectedElementType);
                }
            }

            return rewrittenElement;
        }).ToImmutableArray();

        // FASE 5: Restituisce un NUOVO ArrayLiteralExpr, ora internamente coerente.
        return new ArrayLiteralExpr(rewrittenElements, ale.OpenBrace) { ScopeId = ale.ScopeId };
    }

    /// <summary>
    /// [FASE 3] Riscrive un nodo `NewExpr`, gestendo la risoluzione dei tipi, la validazione dei costruttori
    /// e impedendo l'istanziazione di classi astratte.
    /// </summary>
    /// <remarks>
    /// Questo metodo è un punto di controllo semantico cruciale per la creazione di oggetti. Le sue responsabilità includono:
    /// 1.  Risoluzione del Tipo: Trasforma il tipo di dato sintattico (es. `UnresolvedQualifiedType`) nel suo
    ///     tipo semantico concreto (`ClassType`, `UserDefinedType`, `ListType`, etc.).
    /// 2.  Validazione di Astrazione: Controlla se il tipo risolto è una classe `ABSTRACT`. Se lo è,
    ///     lancia un errore di compilazione, applicando una delle regole fondamentali dell'OOP.
    /// 3.  Risoluzione degli Overload dei Costruttori: Per Classi e UDT, verifica che esista un costruttore
    ///     compatibile con il numero di argomenti forniti in `NEW (...)`. Gestisce sia il caso di chiamata
    ///     con argomenti sia quello di chiamata senza argomenti (che richiede un costruttore di default).
    /// 4.  Riscrittura Ricorsiva: Scende ricorsivamente nelle espressioni fornite come argomenti del
    ///     costruttore o come valori di inizializzazione per le collezioni, assicurando che l'intero
    ///     sotto-albero AST sia semanticamente corretto.
    /// </remarks>
    /// <param name="ne">Il nodo `NewExpr` da riscrivere, proveniente dalla Fase 2.5.</param>
    /// <param name="context">Il contesto di riscrittura corrente.</param>
    /// <param name="program">L'istanza del programma per l'accesso alla SymbolTable.</param>
    /// <returns>Il nodo `NewExpr` riscritto e validato, pronto per le fasi successive.</returns>
    /// <exception cref="BasicException">
    /// Viene lanciata per diversi errori semantici:
    /// - `TypeMismatch`: Se il tipo specificato non è valido o non è un UDT/Classe quando richiesto.
    /// - `TypeNotInstantiable`: Se si tenta di creare un'istanza di una classe `ABSTRACT`.
    /// - `ArgumentCountMismatch`: Se non viene trovato un costruttore corrispondente agli argomenti forniti.
    /// </exception>
    /// <example>
    /// // Codice Valido
    /// DIM c AS Cerchio = NEW Cerchio("MioCerchio", 10.0) ' -> OK, il costruttore corrisponde
    /// DIM l AS LIST OF INTEGER = NEW LIST OF INTEGER     ' -> OK
    /// 
    ///
    /// // Codice NON Valido (che questo metodo intercetta)
    /// ABSTRACT CLASS Figura
    ///   ' ...
    /// END CLASS
    /// DIM f AS Figura = NEW Figura ' -> ERRORE: TypeNotInstantiable
    /// 
    /// CLASS Punto
    ///   PUBLIC SUB NEW(x AS INT)
    ///   END SUB
    /// END CLASS
    /// DIM p AS Punto = NEW Punto ' -> ERRORE: ArgumentCountMismatch (manca il costruttore di default)
    /// </example>
    private static Expr RewriteNewExpr(NewExpr ne, AstRewriteContext context, BasicProgram program)
    {
        // Risolve il tipo di dato (ad esempio, trasforma UnresolvedQualifiedType in UserDefinedType)
        // PRIMA di riscrivere gli argomenti del costruttore: serve a conoscere i Parameter del
        // costruttore selezionato, cosi' da propagare il loro DataType come ExpectedType per
        // ARGOMENTO (vedi sotto, RewriteSingleArgument) — esattamente come già avviene per una
        // chiamata a procedura regolare, ma PRIMA mancava qui: un letterale come 10 passato a un
        // parametro "AS INTEGER" veniva riscritto con il contesto "esterno" grezzo (mai
        // l'ExpectedType del parametro), quindi non veniva mai avvolto in un CINT implicito —
        // il backend IL riceveva un valore Double grezzo (Ldc_R8) per un parametro CLR Int16,
        // producendo IL non valido (InvalidProgramException a runtime) — scoperto testando un
        // costruttore SUB NEW(v AS INTEGER) su un campo array di CLASSE/UDT.
        var resolvedType = ResolveDataType(ne.ParsedType, program, context.ModulePath);

        // --- VALIDAZIONE INTERFACCE ---
        if (resolvedType is InterfaceType interfaceType)
        {
            throw CreateError(ne.NewToken,
                $"Impossibile creare un'istanza di un'interfaccia ('{interfaceType.Definition.Name.Lexeme}').",
                ErrorCode.TypeNotInstantiable);
        }

        // --- VALIDAZIONE CLASSI ASTRATTE ---
        if (resolvedType is ClassType abstractClassType &&
            abstractClassType.Definition.Flags.HasFlag(SymbolFlags.IsAbstract))
        {
            throw CreateError(ne.NewToken,
                $"Impossibile creare un'istanza della classe astratta '{abstractClassType.Definition.Name.Lexeme}'.",
                ErrorCode.TypeNotInstantiable);
        }

        // --- VALIDAZIONE DEI COSTRUTTORI PER UDT E CLASSI ---
        var argCount = ne.ConstructorArgs?.Length ?? 0;

        // Se il tipo risolto è una classe o un UDT, dobbiamo cercare e validare i costruttori.
        IEnumerable<ProcedureDefinition> constructorCandidates;
        string typeName;

        switch (resolvedType)
        {
            case UserDefinedType udt:
                // Per gli UDT, i costruttori sono definiti come ConstructorDefinition.
                constructorCandidates = udt.Constructors.Select(c =>
                    new FunctionDefinition(
                        -1,
                        new Token(TokenKind.New, "NEW", null, SourcePosition.Unknown),
                        c.Parameters,
                        new VoidType(),
                        ImmutableArray<Stmt>.Empty,
                        SymbolFlags.IsConstructor
                    )
                );
                typeName = udt.Name.Lexeme;
                break;
            case ClassType classType:
                // Per le Classi, i costruttori sono membri specifici.
                constructorCandidates = classType.Definition.Members.OfType<ConstructorMember>().Select(c =>
                    new FunctionDefinition(
                        -1,
                        new Token(TokenKind.New, "NEW", null, SourcePosition.Unknown),
                        c.Constructor.Parameters,
                        new VoidType(),
                        ImmutableArray<Stmt>.Empty,
                        SymbolFlags.IsConstructor
                    )
                );
                typeName = classType.Definition.Name.Lexeme;
                break;
            default:
                // Per tutti gli altri tipi (collezioni, tipi primitivi), la validazione del costruttore è più leggera.
                // Se ci sono argomenti per una collezione (es. NEW LIST(10)), assumiamo che sia corretto
                // e lasciamo che l'Executor gestisca il caso — nessun Parameter noto da cui derivare
                // un ExpectedType per argomento, quindi si riscrive con il contesto originale.
                return ne with
                {
                    ParsedType = resolvedType,
                    ConstructorArgs = ne.ConstructorArgs?.Select(arg => RewriteExpression(arg, context, program)).ToImmutableArray(),
                    InitialValues = ne.InitialValues?.Select(v => RewriteExpression(v, context, program)).ToImmutableArray()
                };
        }

        // --- Logica di Validazione di Sovraccarico del Costruttore ---

        // Materializza la lista dei candidati.
        var constructorList = constructorCandidates.ToList();
        ProcedureDefinition? matchedConstructor = null;

        if (ne.ConstructorArgs != null && constructorList.Count != 0)
        {
            // Cerca un costruttore compatibile in base al conteggio degli argomenti — stessa
            // identica risoluzione per conteggio già usata dal backend IL
            // (ResolveConstructorOverload), qui anticipata per conoscere i Parameter da
            // propagare come ExpectedType nella riscrittura degli argomenti sotto.
            matchedConstructor = constructorList.FirstOrDefault(ctor =>
                SymbolTable.IsProcedureSignatureCompatible(ctor, argCount));

            if (matchedConstructor is null)
            {
                throw CreateError(ne.NewToken,
                    $"Nessun costruttore accessibile per il tipo '{typeName}' accetta {argCount} argomenti.",
                    ErrorCode.ArgumentCountMismatch);
            }
        }
        else if (ne.ConstructorArgs == null) // Non sono stati forniti argomenti espliciti.
        {
            // Se non ci sono argomenti, dobbiamo assicurarci che esista un costruttore senza parametri (default).
            var hasDefaultConstructor = constructorList.Count == 0 || constructorList.Any(c => c.Parameters.IsEmpty);
            if (!hasDefaultConstructor)
            {
                throw CreateError(ne.NewToken,
                    $"Il tipo '{typeName}' non ha un costruttore senza parametri. È richiesta una chiamata esplicita a NEW(...).");
            }
        }

        // Riscrive ogni argomento con il DataType del parametro corrispondente del costruttore
        // selezionato come ExpectedType (RewriteSingleArgument — stesso meccanismo già usato per
        // le chiamate a procedura regolari), non più con il contesto "esterno" grezzo uguale per
        // tutti gli argomenti: necessario per la coercizione numerica implicita (CINT/CLNG/CDBL)
        // vedi commento in cima al metodo.
        var rewrittenConstructorArgs = ne.ConstructorArgs?.Select((arg, i) =>
            matchedConstructor is not null && i < matchedConstructor.Parameters.Length
                ? RewriteSingleArgument(arg, matchedConstructor.Parameters[i], context, program)
                : RewriteExpression(arg, context, program)).ToImmutableArray();
        var rewrittenInitializers =
            ne.InitialValues?.Select(v => RewriteExpression(v, context, program)).ToImmutableArray();

        // NOTA: La validazione del tipo di inizializzazione (InitialValues) è gestita in un pass successivo,
        // poiché richiede la piena risoluzione dei tipi.

        // Ritorna il nodo NewExpr riscritto con il tipo risolto.
        return ne with
        {
            ParsedType = resolvedType,
            ConstructorArgs = rewrittenConstructorArgs,
            InitialValues = rewrittenInitializers
        };
    }

    /// <summary>
    /// Verifica se almeno un type parameter è effettivamente usato nei parametri o nel return type.
    /// Evita specializzazioni inutili per funzioni come Identity<T>(x AS INTEGER) dove T non è usato.
    /// </summary>
    private static bool GenericUsesTypeParameters(FunctionDefinition fd)
    {
        foreach (var param in fd.Parameters)
        {
            if (DataTypeContainsTypeParameter(param.DataType, fd.TypeParameters))
                return true;
        }
        return DataTypeContainsTypeParameter(fd.ReturnType, fd.TypeParameters);
    }

    private static bool DataTypeContainsTypeParameter(DataType dataType, ImmutableArray<TypeParameter> typeParams)
    {
        return dataType switch
        {
            TypeParameter tp => typeParams.Contains(tp),
            ArrayType at => DataTypeContainsTypeParameter(at.ElementType, typeParams),
            ListType lt => DataTypeContainsTypeParameter(lt.ElementType, typeParams),
            DictType dt => DataTypeContainsTypeParameter(dt.KeyType, typeParams) || DataTypeContainsTypeParameter(dt.ValueType, typeParams),
            HashSetType hst => DataTypeContainsTypeParameter(hst.ElementType, typeParams),
            QueueType qt => DataTypeContainsTypeParameter(qt.ElementType, typeParams),
            StackType st => DataTypeContainsTypeParameter(st.ElementType, typeParams),
            NullableType nt => DataTypeContainsTypeParameter(nt.ElementType, typeParams),
            FunctionPointerType fpt => fpt.ParameterTypes.Any(p => DataTypeContainsTypeParameter(p, typeParams)) || DataTypeContainsTypeParameter(fpt.ReturnType, typeParams),
            UserDefinedType udt => udt.Fields.Any(f =>
                DataTypeContainsTypeParameter(f.DataType, typeParams)),
            _ => false
        };
    }
}