// FILE: Linker.ParameterModes.cs
// PARTIAL — validazione dei contratti IN/OUT/INOUT dei parametri matrice
// (ADR-MATRIX-Valore-Descrittore-INOUT.md §3).
//
// IN (default): il callee NON può modificare il parametro. OUT/INOUT: può.
// Il passaggio è per descrittore condiviso (nessuna copia): il modo è un
// contratto statico. Questo pass copre le scritture DIRETTE (assegnazione,
// REDIM, AADD/AINS/ADEL/ASORT/AFILL/AEVAL) e MATRIX.SETVALUE, e la PROPAGAZIONE
// attraverso le chiamate: un parametro IN non può essere passato a un parametro
// OUT/INOUT di un'altra procedura. Il divieto di lettura degli OUT resta a F3-ter.

using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>Valida i contratti IN/OUT/INOUT dei parametri matrice su tutte le
    /// procedure del programma linkato. Chiamata dopo la riscrittura finale.</summary>
    public static void ValidateParameterModes(BasicProgram program)
    {
        foreach (var procedure in program.GetAllProcedures())
        {
            if (procedure.Parameters.IsDefaultOrEmpty) continue;
            var inMatrices = procedure.Parameters
                .Where(p => p.Mode == ParameterMode.In && IsMatrixParamType(p.DataType))
                .Select(p => p.SymbolId)
                .ToHashSet();
            if (inMatrices.Count > 0)
                WalkStatements(procedure.Body, statement => CheckStatement(statement, inMatrices, procedure));

            // F3-ter: un parametro OUT non deve essere LETTO (solo scritture). Qui si
            // vieta la lettura di un ELEMENTO (`x = m(i,j)`); i metadati
            // (ROWCOUNT/COLUMNCOUNT/ISCONTIGUOUS) e le scritture restano leciti.
            var outMatrices = procedure.Parameters
                .Where(p => p.Mode == ParameterMode.Out && IsMatrixParamType(p.DataType))
                .Select(p => p.SymbolId)
                .ToHashSet();
            if (outMatrices.Count > 0)
                WalkStatements(procedure.Body, statement => CheckOutStatement(statement, outMatrices, procedure));
        }
    }

    /// <summary>Vero per un tipo di parametro che rappresenta una matrice: il valore
    /// descrittore (`MATRIX OF (I1, I2, DOUBLE)` → <see cref="MatrixValueType"/>) o
    /// l'array rank≥2 legacy (FFI/DIM).</summary>
    private static bool IsMatrixParamType(DataType? type)
        => type is MatrixValueType || type is ArrayType { Rank: >= 2 };

    private static void CheckOutStatement(Stmt statement, HashSet<int> outMatrices, ProcedureDefinition procedure)
    {
        switch (statement)
        {
            case LetStmt let:
                foreach (var assignment in let.Assignments)
                    CheckOutRead(assignment.Value, outMatrices, procedure);
                break;
            case ExpressionStmt expression:
                CheckOutRead(expression.Expression, outMatrices, procedure);
                break;
        }
    }

    /// <summary>Segnala la lettura di un elemento di un parametro OUT in un'espressione
    /// (contesto di lettura: RHS/espressione). Gli argomenti passati a un parametro OUT
    /// del callee sono scritture e non vengono visitati qui (CheckCallArguments).</summary>
    private static void CheckOutRead(Expr? expression, HashSet<int> outMatrices, ProcedureDefinition procedure)
    {
        if (expression is null) return;
        switch (expression)
        {
            case ArrayAccessExpr access when outMatrices.Contains(access.SymbolId):
                ThrowOutReadViolation(procedure, access.SymbolId, access.Position);
                return;
            case VariableExpr variable when outMatrices.Contains(variable.SymbolId):
                ThrowOutReadViolation(procedure, variable.SymbolId, variable.Position);
                return;
            case BinaryExpr binary:
                CheckOutRead(binary.Left, outMatrices, procedure);
                CheckOutRead(binary.Right, outMatrices, procedure);
                break;
            case UnaryExpr unary:
                CheckOutRead(unary.Right, outMatrices, procedure);
                break;
            case GroupingExpr grouping:
                CheckOutRead(grouping.Expression, outMatrices, procedure);
                break;
            case NamedArgumentExpr named:
                CheckOutRead(named.Value, outMatrices, procedure);
                break;
            case FunctionExpr function:
                for (var i = 0; i < function.Args.Length; i++)
                {
                    if (IsOutArgument(function.Args[i], function.ResolvedDefinition, i)) continue;
                    CheckOutRead(function.Args[i], outMatrices, procedure);
                }
                break;
            case MethodCallExpr method:
                for (var i = 0; i < method.Args.Length; i++)
                {
                    if (IsOutArgument(method.Args[i], method.ResolvedDefinition, i)) continue;
                    CheckOutRead(method.Args[i], outMatrices, procedure);
                }
                break;
            case BuiltinFunctionCallExpr builtin:
                // Metadati: MATRIX.ROWCOUNT/COLUMNCOUNT/ISCONTIGUOUS leggono la FORMA, non i valori.
                if (builtin.Name.Kind is TokenKind.MatrixRowCount or TokenKind.MatrixColumnCount
                    or TokenKind.MatrixIsContiguous)
                    return;
                for (var i = 0; i < builtin.Args.Length; i++)
                {
                    // MATRIX.SETVALUE(m, ...): il primo argomento è il TARGET (scrittura).
                    if (builtin.Name.Kind == TokenKind.MatrixSetValue && i == 0) continue;
                    CheckOutRead(builtin.Args[i], outMatrices, procedure);
                }
                break;
            case ArrayAccessExpr access:
                if (!access.Indices.IsDefault)
                    foreach (var index in access.Indices) CheckOutRead(index, outMatrices, procedure);
                break;
        }
    }

    /// <summary>True se l'argomento è passato a un parametro OUT del callee (scrittura).</summary>
    private static bool IsOutArgument(Expr argument, ProcedureDefinition? callee, int index)
    {
        if (callee is null || callee.Parameters.IsDefaultOrEmpty) return false;
        var parameterIndex = index;
        if (argument is NamedArgumentExpr n)
        {
            parameterIndex = -1;
            for (var k = 0; k < callee.Parameters.Length; k++)
                if (callee.Parameters[k].OriginalName.Lexeme.Equals(n.Name.Lexeme, StringComparison.OrdinalIgnoreCase))
                {
                    parameterIndex = k;
                    break;
                }
        }
        if (parameterIndex < 0 || parameterIndex >= callee.Parameters.Length) return false;
        return callee.Parameters[parameterIndex].Mode == ParameterMode.Out;
    }

    private static void ThrowOutReadViolation(ProcedureDefinition procedure, int symbolId, SourcePosition position)
    {
        var name = procedure.Parameters.FirstOrDefault(p => p.SymbolId == symbolId)?.OriginalName.Lexeme
                   ?? procedure.Parameters.FirstOrDefault(p => p.SymbolId == symbolId)?.Name.Lexeme
                   ?? "?";
        var token = new Token(TokenKind.Identifier, name, name, position);
        throw CreateError(token,
            $"Il parametro matrice '{name}' è dichiarato OUT (solo scrittura): non può essere letto. " +
            "Dichiaralo INOUT se devi leggere e scrivere.");
    }

    private static void CheckStatement(Stmt statement, HashSet<int> inMatrices, ProcedureDefinition procedure)
    {
        switch (statement)
        {
            case LetStmt let:
                foreach (var assignment in let.Assignments)
                    CheckTarget(assignment.Target, inMatrices, procedure);
                CheckWriterCall(let.Assignments, inMatrices, procedure);
                break;
            case RedimStmt redim:
                CheckTarget(redim.ArrayTarget, inMatrices, procedure);
                break;
            case AAddStmt add:
                CheckTarget(add.Array, inMatrices, procedure);
                break;
            case AInsStmt insert:
                CheckTarget(insert.Array, inMatrices, procedure);
                break;
            case ADelStmt delete:
                CheckTarget(delete.Array, inMatrices, procedure);
                break;
            case ASortStmt sort:
                CheckTarget(sort.Array, inMatrices, procedure);
                break;
            case AFillStmt fill:
                CheckTarget(fill.Array, inMatrices, procedure);
                break;
            case AEvalStmt eval:
                CheckTarget(eval.Array, inMatrices, procedure);
                break;
            case SwapStmt swap:
                CheckTarget(swap.Left, inMatrices, procedure);
                CheckTarget(swap.Right, inMatrices, procedure);
                break;
            case ExpressionStmt expression:
                CheckExpression(expression.Expression, inMatrices, procedure);
                break;
        }
    }

    private static void CheckWriterCall(ImmutableArray<Assignment> assignments,
        HashSet<int> inMatrices, ProcedureDefinition procedure)
    {
        foreach (var assignment in assignments)
            CheckExpression(assignment.Value, inMatrices, procedure);
    }

    /// <summary>Cerca le scritture tramite builtin (MATRIX.SETVALUE) verso un parametro IN.</summary>
    private static void CheckExpression(Expr? expression, HashSet<int> inMatrices, ProcedureDefinition procedure)
    {
        if (expression is null) return;
        switch (expression)
        {
            case BuiltinFunctionCallExpr call when call.Name.Kind == TokenKind.MatrixSetValue
                                                    && call.Args.Length > 0:
                CheckTargetOfExpression(call.Args[0], inMatrices, procedure);
                break;
            case BinaryExpr binary:
                CheckExpression(binary.Left, inMatrices, procedure);
                CheckExpression(binary.Right, inMatrices, procedure);
                break;
            case UnaryExpr unary:
                CheckExpression(unary.Right, inMatrices, procedure);
                break;
            case GroupingExpr grouping:
                CheckExpression(grouping.Expression, inMatrices, procedure);
                break;
            case FunctionExpr function:
                CheckCallArguments(function.Args, function.ResolvedDefinition, inMatrices, procedure);
                foreach (var argument in function.Args) CheckExpression(argument, inMatrices, procedure);
                break;
            case MethodCallExpr method:
                CheckCallArguments(method.Args, method.ResolvedDefinition, inMatrices, procedure);
                foreach (var argument in method.Args) CheckExpression(argument, inMatrices, procedure);
                break;
            case NamedArgumentExpr named:
                CheckExpression(named.Value, inMatrices, procedure);
                break;
            case BuiltinFunctionCallExpr builtin:
                foreach (var argument in builtin.Args) CheckExpression(argument, inMatrices, procedure);
                break;
            case ArrayAccessExpr access:
                if (!access.Indices.IsDefault)
                    foreach (var index in access.Indices) CheckExpression(index, inMatrices, procedure);
                break;
        }
    }

    /// <summary>Propagazione del contratto: un parametro matrice IN del chiamante non
    /// può essere passato a un parametro OUT/INOUT (che il callee può scrivere) di
    /// un'altra procedura. Gli argomenti nominali sono risolti per nome.</summary>
    private static void CheckCallArguments(ImmutableArray<Expr> args, ProcedureDefinition? callee,
        HashSet<int> inMatrices, ProcedureDefinition procedure)
    {
        if (callee is null || args.IsDefaultOrEmpty) return;
        var parameters = callee.Parameters;
        if (parameters.IsDefaultOrEmpty) return;
        for (var i = 0; i < args.Length; i++)
        {
            var argument = args[i];
            var parameterIndex = i;
            if (argument is NamedArgumentExpr named)
            {
                parameterIndex = -1;
                for (var k = 0; k < parameters.Length; k++)
                {
                    if (parameters[k].OriginalName.Lexeme.Equals(named.Name.Lexeme, StringComparison.OrdinalIgnoreCase)
                        || parameters[k].Name.Lexeme.Equals(named.Name.Lexeme, StringComparison.OrdinalIgnoreCase))
                    {
                        parameterIndex = k;
                        break;
                    }
                }
                argument = named.Value;
            }
            if (parameterIndex < 0 || parameterIndex >= parameters.Length) continue;
            var target = parameters[parameterIndex];
            if (target.Mode == ParameterMode.In) continue;
            if (!IsMatrixParamType(target.DataType)) continue;
            var symbolId = argument switch
            {
                VariableExpr variable => variable.SymbolId,
                ArrayAccessExpr access => access.SymbolId,
                _ => -1
            };
            if (symbolId >= 0 && inMatrices.Contains(symbolId))
                ThrowPropagationViolation(procedure, symbolId, target, callee, argument.Position);
        }
    }

    private static void ThrowPropagationViolation(ProcedureDefinition caller, int symbolId,
        Parameter target, ProcedureDefinition callee, SourcePosition position)
    {
        var name = caller.Parameters.FirstOrDefault(p => p.SymbolId == symbolId)?.OriginalName.Lexeme
                   ?? caller.Parameters.FirstOrDefault(p => p.SymbolId == symbolId)?.Name.Lexeme
                   ?? "?";
        var mode = target.Mode == ParameterMode.Out ? "OUT" : "INOUT";
        var token = new Token(TokenKind.Identifier, name, name, position);
        throw CreateError(token,
            $"Il parametro matrice '{name}' è dichiarato IN (sola lettura) ma è passato al parametro " +
            $"'{target.OriginalName.Lexeme}' {mode} di '{callee.OriginalName.Lexeme}', che può modificarlo. " +
            "Dichiaralo INOUT (o OUT) oppure passa una copia (MATRIX.CLONEMATRIX).");
    }

    private static void CheckTargetOfExpression(Expr expression, HashSet<int> inMatrices,
        ProcedureDefinition procedure)
    {
        var symbolId = expression switch
        {
            VariableExpr variable => variable.SymbolId,
            ArrayAccessExpr access => access.SymbolId,
            _ => -1
        };
        if (symbolId >= 0 && inMatrices.Contains(symbolId))
            ThrowInViolation(procedure, symbolId, expression.Position);
    }

    private static void CheckTarget(Target target, HashSet<int> inMatrices, ProcedureDefinition procedure)
    {
        switch (target)
        {
            case VariableTarget variable when inMatrices.Contains(variable.SymbolId):
                ThrowInViolation(procedure, variable.SymbolId, variable.Position);
                break;
            case ArrayElementTarget element when inMatrices.Contains(element.SymbolId):
                ThrowInViolation(procedure, element.SymbolId, element.Position);
                break;
            case FieldTarget field:
                if (field.Structure is not null) CheckTarget(field.Structure, inMatrices, procedure);
                break;
            case IndexedTarget indexed:
                if (indexed.Base is VariableExpr baseVariable && inMatrices.Contains(baseVariable.SymbolId))
                    ThrowInViolation(procedure, baseVariable.SymbolId, indexed.Position);
                else
                    CheckExpression(indexed.Base, inMatrices, procedure);
                break;
        }
    }

    private static void ThrowInViolation(ProcedureDefinition procedure, int symbolId, SourcePosition position)
    {
        var name = symbolId >= 0
            ? procedure.Parameters.FirstOrDefault(p => p.SymbolId == symbolId)?.OriginalName.Lexeme
              ?? procedure.Parameters.FirstOrDefault(p => p.SymbolId == symbolId)?.Name.Lexeme
            : null;
        var token = new Token(TokenKind.Identifier, name ?? "parametro",
            name, position);
        throw CreateError(token,
            $"Il parametro matrice '{(name ?? "?")}' è dichiarato IN (sola lettura) e non può essere modificato. " +
            "Dichiaralo INOUT (o OUT) per consentire la scrittura.");
    }

    /// <summary>Visita ricorsivamente i blocchi annidati (IF/SELECT/WITH/USING/TRY).</summary>
    private static void WalkStatements(IEnumerable<Stmt> statements, Action<Stmt> visit)
    {
        foreach (var statement in statements)
        {
            visit(statement);
            switch (statement)
            {
                case IfStmt ifStatement:
                    foreach (var clause in ifStatement.IfClauses)
                        WalkStatements(clause.Statements, visit);
                    if (ifStatement.ElseClause is not null)
                        WalkStatements(ifStatement.ElseClause.Statements, visit);
                    break;
                case SelectStmt select:
                    foreach (var clause in select.CaseClauses)
                        WalkStatements(clause.Statements, visit);
                    if (select.ElseClause is not null)
                        WalkStatements(select.ElseClause.Statements, visit);
                    break;
                case WithStmt withStatement:
                    WalkStatements(withStatement.Body, visit);
                    break;
                case UsingStmt usingStatement:
                    WalkStatements(usingStatement.Body, visit);
                    break;
                case TryCatchStmt tryStatement:
                    WalkStatements(tryStatement.TryBody, visit);
                    foreach (var clause in tryStatement.CatchClauses)
                        WalkStatements(clause.Body, visit);
                    if (tryStatement.Finally is not null)
                        WalkStatements(tryStatement.Finally.Body, visit);
                    break;
            }
        }
    }
}
