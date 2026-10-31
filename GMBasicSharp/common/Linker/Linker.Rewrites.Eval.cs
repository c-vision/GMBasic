// FILE: Interpreter.Linker.Rewrites.Eval.cs
// PARTIAL — dominio EVAL: dispatch per tipo del nodo EvalExpr (Clipper 5.3 + GMBasic).
// ReSharper disable RedundantArgumentDefaultValue

using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>Contatore per FuncToken unici delle lambda JIT delle macro (scope unico).</summary>
    private static int _macroLambdaUid;
    /// <summary>
    /// Riscrive un nodo <see cref="EvalExpr"/> (chiamata <c>EVAL(target [, args...])</c>) con
    /// dispatch per TIPO STATICO del target — semantica CA-Clipper 5.3 + estensione GMBasic:
    /// <list type="bullet">
    ///   <item><c>EVAL(&lt;stringa&gt;)</c> → <see cref="MacroExpr"/> (macro-evaluator GMBasic,
    ///   compila+esegue la stringa a runtime);</item>
    ///   <item><c>EVAL(&lt;codeblock&gt; [, args...])</c> → <see cref="FunctionExpr"/> indiretto
    ///   (<c>IsIndirect</c>), stesso percorso della chiamata diretta <c>b(args)</c> — già
    ///   supportato da tutte e 5 le gambe;</item>
    ///   <item>target Any/Inferred (block prodotti da macro o parametri impliciti) → resta
    ///   <see cref="EvalExpr"/>: i backend fanno il dispatch a runtime (stringa vs callable).</item>
    /// </list>
    /// </summary>
    private static Expr RewriteEvalExpr(EvalExpr ee, AstRewriteContext context, BasicProgram program)
    {
        var rewrittenTarget = RewriteExpression(ee.Target, context with { ExpectedType = null }, program,
            isMacroArgument: true);
        var rewrittenArgs = ee.Args.Select(a => RewriteExpression(a, context, program)).ToImmutableArray();

        // Lambda INLINE come target: inferisci i tipi dei parametri dagli ARGOMENTI del
        // call-site (EVAL({ |s| PRINT s }, "y") → s STRING). Senza, il parametro restava
        // Inferred → firma numerica con argomento stringa (IL InvalidProgramException,
        // C99Native errore C).
        if (rewrittenTarget is LambdaExpr leInline && !rewrittenArgs.IsEmpty)
        {
            var argTypes = rewrittenArgs
                .Select(a => LinkerInferExpressionType(a, context, program) ?? new InferredType())
                .ToImmutableArray();
            if (argTypes.Any(t => t is not InferredType))
            {
                var expected = new FunctionPointerType(argTypes, leInline.ReturnType);
                rewrittenTarget = RewriteExpression(ee.Target, context with { ExpectedType = expected },
                    program, isMacroArgument: true);
            }
        }

        var targetType = ResolveEvalTargetType(rewrittenTarget, context, program);

        // Code block con tipo noto: invocazione indiretta (stesso percorso di `b(args)`).
        if (targetType is FunctionPointerType fpt)
        {
            if (fpt.ParameterTypes.Length != rewrittenArgs.Length)
            {
                throw CreateError(ee.Token,
                    $"EVAL(code block): numero di argomenti non valido. Attesi {fpt.ParameterTypes.Length}, " +
                    $"forniti {rewrittenArgs.Length}.",
                    ErrorCode.ArgumentCountMismatch);
            }

            // Tipi degli argomenti vs parametri del block: parametro senza evidenza = default
            // statico DoubleType (come i backend). Un argomento STRINGA per un parametro
            // numerico (o viceversa) è AMBIGUO → errore ESPLICITO UNIFORME su tutti i backend
            // (prima: IL InvalidProgram, PCode/VM-C concatenavano, C99* errore C — bug
            // Lambda-Concat-Due-Param-Senza-Evidenza-Tipo).
            for (var i = 0; i < rewrittenArgs.Length; i++)
            {
                var paramType = fpt.ParameterTypes[i] is InferredType or AnyType
                    ? new DoubleType()
                    : fpt.ParameterTypes[i];
                var argType = LinkerInferExpressionType(rewrittenArgs[i], context, program);
                if (argType is null or InferredType or AnyType) continue;
                if (paramType is StringType != argType is StringType)
                {
                    throw CreateError(ee.Token,
                        "EVAL(code block): argomento di tipo incompatibile col parametro del block: " +
                        "per la concatenazione tra parametri annota AS STRING.",
                        ErrorCode.TypeMismatch);
                }
            }

            if (rewrittenTarget is VariableExpr ve && ve.SymbolId >= 0)
            {
                return new FunctionExpr(ve.SymbolId, ve.NameCleaned, rewrittenArgs, fpt.ReturnType)
                {
                    ResolvedDefinition = null,
                    IsIndirect = true,
                    ScopeId = ee.ScopeId
                };
            }
            // Target non-variabile (es. funzione che ritorna un block): dispatch a runtime.
            return new EvalExpr(rewrittenTarget, rewrittenArgs, ee.Token) { ScopeId = ee.ScopeId };
        }

        // Stringa: macro-evaluator GMBasic (la forma storica EVAL("...")/EVAL(cVarStringa)).
        if (targetType is StringType)
        {
            if (!rewrittenArgs.IsEmpty)
            {
                throw CreateError(ee.Token,
                    "EVAL(<stringa>) non accetta argomenti aggiuntivi: gli argomenti sono dell'invocazione " +
                    "di un code block (EVAL(<block>, args...)).",
                    ErrorCode.ArgumentCountMismatch);
            }
            return new MacroExpr(rewrittenTarget, ee.Token) { ScopeId = ee.ScopeId };
        }

        // Target DINAMICO con argomenti (es. `EVAL(<var macro dinamica>, args)`): nessun
        // dispatch uniforme possibile (i transpiler C non hanno un compilatore BASIC a
        // runtime; PCode/VM-C hanno la closure del chunk macro cross-chunk; IL non ha un
        // EvalExpr dinamico). ERRORE ESPLICITO UNIFORME su tutti i backend — le forme
        // costanti sono espanse PRIMA a compile-time (letterali/concatenazioni di costanti).
        if (!rewrittenArgs.IsEmpty)
        {
            throw CreateError(ee.Token,
                "EVAL su un target dinamico non supportato: usa un code block costante " +
                "(letterale o costruito da parti costanti) o una variabile di tipo funzione.",
                ErrorCode.TypeMismatch);
        }

        // Dinamico (Any/Inferred) senza argomenti: resta EvalExpr (macro-eval di espressione).
        return new EvalExpr(rewrittenTarget, rewrittenArgs, ee.Token) { ScopeId = ee.ScopeId };
    }

    /// <summary>
    /// Riscrive un <see cref="MacroExpr"/>: riscrive l'espressione interna e, se questa è una
    /// VARIABILE il cui valore è una stringa COSTANTE (DIM/literal mai riassegnata altrove),
    /// espande la macro a COMPILE-TIME — così `DIM b = &(cBlockDef)` con cBlockDef costante
    /// diventa una lambda tipizzata invocabile su TUTTE le gambe, senza compilazione runtime
    /// (le gambe C non hanno un compilatore BASIC a runtime). Bug Clipper-Macro-Variabile-
    /// Block-Runtime-5-Backend.
    /// </summary>
    private static Expr RewriteMacroExpr(MacroExpr me, AstRewriteContext context, BasicProgram program)
    {
        var rewritten = RewriteExpression(me.Expression, context with { ExpectedType = null }, program,
            isMacroArgument: true);

        // `&(cVar)` tra parentesi → GroupingExpr attorno alla variabile: scava.
        var macroTarget = rewritten is GroupingExpr grouping ? grouping.Expression : rewritten;
        if (macroTarget is VariableExpr ve && ve.SymbolId >= 0
            && TryGetConstantStringSymbol(ve.SymbolId, program, out var literal))
        {
            var expanded = TryExpandConstantStringAtLinkTime(literal, context, program);
            if (expanded != null) return expanded;
        }

        return me with { Expression = rewritten };
    }

    /// <summary>Espande a link-time il testo di una macro costante: parse JIT + riscrittura
    /// nello scope corrente. Ritorna null se il testo non è un'espressione espandibile
    /// (comando, uguaglianza top-level, nome nudo — stesse regole del parser).</summary>
    private static Expr? TryExpandConstantStringAtLinkTime(string literal, AstRewriteContext context,
        BasicProgram program)
    {
        if (string.IsNullOrWhiteSpace(literal)) return null;
        try
        {
            var sourceLine = new SourceLine(-1, "", 0, literal);
            var parsed = Parser.ParseSingleExpression(literal, program, sourceLine, 1, StdLib.Instance);
            if (parsed is BinaryExpr { Op.Kind: TokenKind.Equal } or VariableExpr) return null;
            // I nodi JIT non sono passati dal Binding: la lambda va legata QUI (parametri con
            // SymbolId, body con ScopeId) prima della riscrittura, altrimenti `p` resta
            // irrisolto e il backend lo legge come globale 0 (bug macro-variabile-block: IL 0).
            // La LAMBDA riceve un FuncToken UNICO: `EnterScope` del Binder riusa uno scope
            // figlio omonimo (`Lambda_<line>_<col>`) → senza, la SECONDA espansione della
            // stessa lambda riuserebbe lo scope della prima e ri-dichiarerebbe `x`
            // ("simbolo già dichiarato in questo scope") → seconda macro non espansa.
            if (parsed is LambdaExpr jitLam)
            {
                var uid = System.Threading.Interlocked.Increment(ref _macroLambdaUid);
                parsed = jitLam with
                {
                    FuncToken = jitLam.FuncToken with
                    {
                        Position = new SourcePosition(jitLam.FuncToken.Position.FileId, 1_000_000 + uid, 0)
                    }
                };
                parsed = BindExpression(parsed, program, false);
            }
            else
            {
                parsed = parsed with { ScopeId = context.CurrentScopeId ?? 0 };
                parsed = BindExpression(parsed, program, false);
            }
            return RewriteExpression(parsed, context with { ExpectedType = null }, program);
        }
        catch (BasicException ex)
        {
            return null;
        }
    }

    /// <summary>True se il simbolo è una variabile con inizializzatore letterale stringa e
    /// NESSUNA altra assegnazione in tutto il programma (globale o locale): il suo valore è
    /// quindi una costante di compilazione. Conservativo: qualunque assegnazione (LET/
    /// AssignmentExpr/DIM successivo/BYREF) invalida.</summary>
    private static bool TryGetConstantStringSymbol(int symbolId, BasicProgram program, out string value)
        => TryGetConstantStringSymbol(symbolId, program, out value, new HashSet<int>());

    private static bool TryGetConstantStringSymbol(int symbolId, BasicProgram program, out string value,
        HashSet<int> visiting)
    {
        value = "";
        var symbol = program.Symbols.GetSymbol(symbolId);
        if (symbol.DeclarationNode is not VariableDeclaration decl)
            return false;
        if (!visiting.Add(symbolId)) return false;   // ciclo (a = b; b = a)

        // Valore costante dall'inizializzatore (letterale o concatenazione di costanti).
        string? constVal = null;
        if (decl.InitialValue is not null &&
            TryEvaluateConstantString(decl.InitialValue, program, out var iv, visiting) &&
            !string.IsNullOrEmpty(iv))
            constVal = iv;

        // `DIM s AS STRING` (senza init, o init vuota) + UNA SOLA assegnazione costante:
        // il valore è noto (bug macro dinamiche: copre la riassegnazione singola).
        if (constVal is null)
        {
            constVal = TryGetUniqueConstantAssignmentValue(symbolId, program);
            if (constVal is null) return false;
            value = constVal;
            return true;
        }
        value = constVal;

        foreach (var proc in program.GetAllProcedures())
            if (ContainsAssignmentToSymbol(proc.Body, symbolId, decl)) return false;

        foreach (var module in program.LoadedModules.Values)
        {
            foreach (var stmts in module.Statements.Values)
                if (ContainsAssignmentToSymbol(stmts, symbolId, decl)) return false;
        }
        return true;
    }

    /// <summary>Se il simbolo ha UNA SOLA assegnazione (LET/AssignmentExpr) in tutto il
    /// programma e il suo valore è una stringa costante, la ritorna; altrimenti null.
    /// (Per `DIM s AS STRING` senza inizializzatore + `s = "letterale"`.)</summary>
    private static string? TryGetUniqueConstantAssignmentValue(int symbolId, BasicProgram program)
    {
        string? found = null;
        var count = 0;
        void Consider(IEnumerable<AstNode> nodes)
        {
            foreach (var let in FindAllNodesOfType<LetStmt>(nodes))
                foreach (var assignment in let.Assignments)
                    if (assignment.Target is VariableTarget vt && vt.SymbolId == symbolId)
                    {
                        count++;
                        if (TryEvaluateConstantString(assignment.Value, program, out var v, new HashSet<int>()))
                            found = v;
                    }
            foreach (var ae in FindAllNodesOfType<AssignmentExpr>(nodes))
                if (ae.Target is VariableTarget vt && vt.SymbolId == symbolId)
                {
                    count++;
                    if (TryEvaluateConstantString(ae.Value, program, out var v, new HashSet<int>()))
                        found = v;
                }
        }
        foreach (var proc in program.GetAllProcedures()) Consider(proc.Body);
        foreach (var module in program.LoadedModules.Values)
            foreach (var stmts in module.Statements.Values) Consider(stmts);
        return count == 1 ? found : null;
    }

    /// <summary>Valuta un'espressione come stringa COSTANTE: letterale, variabile con
    /// inizializzatore costante (una sola assegnazione), concatenazione `+` di costanti.
    /// Conservativo: qualunque altro caso → false (nessuna espansione).</summary>
    private static bool TryEvaluateConstantString(Expr expr, BasicProgram program, out string value,
        HashSet<int> visiting)
    {
        switch (expr)
        {
            case StringExpr se:
                value = se.Value;
                return true;
            case BinaryExpr { Op.Kind: TokenKind.Plus } be:
                if (TryEvaluateConstantString(be.Left, program, out var l, visiting) &&
                    TryEvaluateConstantString(be.Right, program, out var r, visiting))
                {
                    value = l + r;
                    return true;
                }
                break;
            case VariableExpr ve when ve.SymbolId >= 0:
                return TryGetConstantStringSymbol(ve.SymbolId, program, out value, visiting);
        }
        value = "";
        return false;
    }

    private static bool ContainsAssignmentToSymbol(IEnumerable<AstNode> nodes, int symbolId,
        VariableDeclaration ownDeclaration)
    {
        foreach (var let in FindAllNodesOfType<LetStmt>(nodes))
            foreach (var assignment in let.Assignments)
                if (assignment.Target is VariableTarget vt && vt.SymbolId == symbolId) return true;

        foreach (var assignExpr in FindAllNodesOfType<AssignmentExpr>(nodes))
            if (assignExpr.Target is VariableTarget vt && vt.SymbolId == symbolId) return true;

        foreach (var byRef in FindAllNodesOfType<MakeByRefExpr>(nodes))
            if (byRef.SymbolId == symbolId) return true;

        return false;

        foreach (var byRef in FindAllNodesOfType<MakeByRefExpr>(nodes))
            if (byRef.SymbolId == symbolId) return true;

        return false;
    }

    /// <summary>
    /// Tipo statico del target di un <see cref="EvalExpr"/>: simbolo risolto (variabile o
    /// parametro), tipo di ritorno di una chiamata, stringa letterale, altrimenti inferenza.
    /// </summary>
    private static DataType ResolveEvalTargetType(Expr target, AstRewriteContext context, BasicProgram program)
    {
        switch (target)
        {
            case VariableExpr ve when ve.SymbolId >= 0:
                return program.Symbols.GetSymbol(ve.SymbolId).DataType;
            case FunctionExpr fe:
                return fe.ReturnType;
            case StringExpr:
                return new StringType();
            case MacroExpr:
                return new AnyType();
            default:
                return LinkerInferExpressionType(target, context with { ExpectedType = null }, program)
                    ?? new InferredType();
        }
    }
}
