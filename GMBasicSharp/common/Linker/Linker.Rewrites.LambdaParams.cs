// FILE: Interpreter.Linker.Rewrites.LambdaParams.cs
// PARTIAL — inferenza dei tipi dei parametri di una LAMBDA dall'USO nel corpo.
// ReSharper disable RedundantArgumentDefaultValue

using System.Collections.Immutable;

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Inferisce il tipo dei parametri di una lambda ancora <see cref="InferredType"/> dall'USO
    /// nel corpo riscritto: se un parametro è passato a una chiamata (builtin o UDF) il cui
    /// parametro formale ha un tipo CONCRETO, quel tipo è l'evidenza del tipo reale.
    /// Caso misurato: <c>{ |s| RETURN UPPER(s) }</c> — senza inferenza <c>s</c> restava
    /// InferredType → sul backend IL la firma del delegate era <c>Func&lt;double,string&gt;</c>
    /// mentre il corpo chiamava <c>ToUpperInvariant</c> sull'argomento → InvalidProgramException
    /// (PCode/VM-C, dinamici, mascheravano il problema).
    /// L'inferenza è MONOTONA e conservativa: tocca solo parametri ancora InferredType e solo
    /// quando il tipo formale è concreto (mai Inferred/Any).
    /// </summary>
    private static void InferLambdaParameterTypesFromUsage(
        ImmutableArray<Parameter>.Builder parameters,
        IEnumerable<AstNode>? bodyNodes,
        Dictionary<string, DataType> effectiveLambdaScope,
        BasicProgram program)
    {
        if (bodyNodes is null) return;

        var paramBySymbol = new Dictionary<int, int>();
        for (var i = 0; i < parameters.Count; i++)
        {
            if (parameters[i].SymbolId >= 0 && parameters[i].DataType is InferredType)
                paramBySymbol[parameters[i].SymbolId] = i;
        }

        if (paramBySymbol.Count == 0) return;

        void Unify(int paramIndex, DataType declaredType)
        {
            if (declaredType is InferredType or AnyType or UnresolvedQualifiedType) return;
            var param = parameters[paramIndex];
            if (param.DataType is not InferredType) return;
            var updated = param with { DataType = declaredType };
            parameters[paramIndex] = updated;
            effectiveLambdaScope[updated.OriginalName.Lexeme] = declaredType;
            if (updated.SymbolId >= 0)
            {
                program.Symbols.UpdateSymbolType(updated.SymbolId, declaredType);
                program.Symbols.UpdateSymbolDeclaration(updated.SymbolId, updated);
            }
        }

        void UnifyArgs(ImmutableArray<Expr> args, ImmutableArray<Parameter> formalParams)
        {
            for (var i = 0; i < args.Length && i < formalParams.Length; i++)
            {
                if (args[i] is VariableExpr ve && ve.SymbolId >= 0 &&
                    paramBySymbol.TryGetValue(ve.SymbolId, out var pi))
                {
                    Unify(pi, formalParams[i].DataType);
                }
            }
        }

        var nodes = bodyNodes.ToList();

        // Uso INDICIZZATO dentro la lambda: `y(0)` → il parametro è un ARRAY (elemento Double
        // per default). Senza, una lambda ODE non annotata (`FN(t, y) => {-y(0)}`) restava
        // con y scalare e il backend IL generava una firma delegate incompatibile con
        // SolveIvp (Func<double,double[],double[]>) → InvalidProgramException. Il parametro
        // può essere già stato tipizzato Double dal target typing: l'uso indicizzato è
        // evidenza più forte e lo promuove ad array.
        void UnifyArray(int paramIndex)
        {
            var param = parameters[paramIndex];
            if (param.DataType is not (InferredType or DoubleType)) return;
            var updated = param with { DataType = new ArrayType(new DoubleType()) };
            parameters[paramIndex] = updated;
            effectiveLambdaScope[updated.OriginalName.Lexeme] = updated.DataType;
            if (updated.SymbolId >= 0)
            {
                program.Symbols.UpdateSymbolType(updated.SymbolId, updated.DataType);
                program.Symbols.UpdateSymbolDeclaration(updated.SymbolId, updated);
            }
        }

        foreach (var aae in FindAllNodesOfType<ArrayAccessExpr>(nodes))
        {
            int pi = -1;
            if (aae.SymbolId >= 0 && paramBySymbol.TryGetValue(aae.SymbolId, out var bySym))
                pi = bySym;
            else
            {
                for (var i = 0; i < parameters.Count; i++)
                {
                    if (string.Equals(parameters[i].OriginalName.Lexeme, aae.NameCleaned.Lexeme,
                            StringComparison.OrdinalIgnoreCase)
                        || string.Equals(parameters[i].OriginalName.Lexeme, aae.NameSuffixed.Lexeme,
                            StringComparison.OrdinalIgnoreCase))
                    {
                        pi = i;
                        break;
                    }
                }
            }
            if (pi >= 0) UnifyArray(pi);
        }

        // Builtin: la definizione nativa si risolve per namespace + BuiltinKind (stessa via
        // di InferGenericPolymorphicReturnType).
        foreach (var bce in FindAllNodesOfType<BuiltinFunctionCallExpr>(nodes))
        {
            var proc = program.GetProceduresForNamespace(bce.Namespace.Lexeme)
                .OfType<NativeProcedureDefinition>()
                .FirstOrDefault(p => p.BuiltinKind == bce.Name.Kind);
            if (proc is not null) UnifyArgs(bce.Args, proc.Parameters);
        }

        // UDF: parametri dalla definizione risolta (FunctionExpr).
        foreach (var fe in FindAllNodesOfType<FunctionExpr>(nodes))
        {
            if (fe.ResolvedDefinition is ProcedureDefinition pd) UnifyArgs(fe.Args, pd.Parameters);
        }

        // Concat `+` con un operando stringa: l'altro operando è usato come stringa.
        // Caso misurato: { |s| RETURN s + "!" } — senza, `s` restava InferredType e
        // IL (InvalidProgramException) (firma gmb_int_t col call-site
        // stringa) emettevano firme incompatibili; PCode/VM-C dinamici mascheravano.
        bool IsStringOperand(Expr e) => e switch
        {
            StringExpr => true,
            InterpolatedStringExpr => true,
            VariableExpr ve when ve.SymbolId >= 0 =>
                program.Symbols.GetSymbol(ve.SymbolId).DataType is StringType,
            BuiltinFunctionCallExpr bce =>
                program.GetProceduresForNamespace(bce.Namespace.Lexeme)
                    .OfType<NativeProcedureDefinition>()
                    .FirstOrDefault(p => p.BuiltinKind == bce.Name.Kind)?.ReturnType is StringType,
            FunctionExpr fe =>
                fe.ResolvedDefinition is FunctionDefinition fd && fd.ReturnType is StringType,
            _ => false
        };

        void UnifyOperandAsString(Expr e)
        {
            if (e is VariableExpr ve && ve.SymbolId >= 0 &&
                paramBySymbol.TryGetValue(ve.SymbolId, out var pi))
            {
                Unify(pi, new StringType());
            }
        }

        foreach (var be in FindAllNodesOfType<BinaryExpr>(nodes))
        {
            if (be.Op.Kind != TokenKind.Plus) continue;
            if (IsStringOperand(be.Left)) UnifyOperandAsString(be.Right);
            if (IsStringOperand(be.Right)) UnifyOperandAsString(be.Left);
        }
    }
}
