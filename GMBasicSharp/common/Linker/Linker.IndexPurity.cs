// FILE: Interpreter.Linker.IndexPurity.cs

namespace Basic;

public static partial class Linker
{
    private static readonly HashSet<string> SafeIndexEngineFunctions =
        new(StringComparer.OrdinalIgnoreCase) { "UPPER", "JSON_VALUE", "COMPOSITE", "STR", "CSTR" };

    private static readonly HashSet<TokenKind> SafeIndexBuiltinKinds =
    [
        TokenKind.Upper,
        TokenKind.Str,
        TokenKind.CStr
    ];

    /// <summary>
    /// Compile-time check: expression-index key AST must not call impure UDFs unless <paramref name="isUnsafe"/>.
    /// Mirrors engine-side safe evaluators (field refs, UPPER, JSON_VALUE, COMPOSITE) and GPM UNSAFE path.
    /// </summary>
    public static void ValidateIndexKeyExpressionPurity(Expr expr, bool isUnsafe, BasicProgram program, Token errorToken)
    {
        if (isUnsafe)
        {
            return;
        }

        ValidateIndexKeyExpressionPurityCore(expr, program, errorToken);
    }

    private static void ValidateIndexKeyExpressionPurityCore(Expr expr, BasicProgram program, Token errorToken)
    {
        switch (expr)
        {
            case StringExpr or NumberExpr or BooleanExpr or NilExpr:
                return;

            case VariableExpr or FieldAccessExpr or BaseExpr:
                return;

            case UnaryExpr u:
                ValidateIndexKeyExpressionPurityCore(u.Right, program, errorToken);
                return;

            case BinaryExpr b:
                ValidateIndexKeyExpressionPurityCore(b.Left, program, errorToken);
                ValidateIndexKeyExpressionPurityCore(b.Right, program, errorToken);
                return;

            case GroupingExpr g:
                ValidateIndexKeyExpressionPurityCore(g.Expression, program, errorToken);
                return;

            case MacroExpr m:
                ValidateIndexKeyExpressionPurityCore(m.Expression, program, errorToken);
                return;

            case EvalExpr ee:
                ValidateIndexKeyExpressionPurityCore(ee.Target, program, errorToken);
                foreach (var evalArg in ee.Args)
                    ValidateIndexKeyExpressionPurityCore(evalArg, program, errorToken);
                return;

            case BuiltinFunctionCallExpr bce when IsSafeIndexBuiltin(bce.Name):
                foreach (var arg in bce.Args)
                {
                    ValidateIndexKeyExpressionPurityCore(arg, program, errorToken);
                }

                return;

            case FunctionExpr fe:
                if (IsSafeIndexEngineFunction(fe.OriginalToken.Lexeme))
                {
                    foreach (var arg in fe.Args)
                    {
                        ValidateIndexKeyExpressionPurityCore(arg, program, errorToken);
                    }

                    return;
                }

                if (fe.SymbolId < 0)
                {
                    throw CreateIndexKeyPurityError(errorToken, fe.OriginalToken.Lexeme);
                }

                var symbol = program.Symbols.GetSymbol(fe.SymbolId);
                if (symbol.Flags.HasFlag(SymbolFlags.IsPure) ||
                    symbol.DeclarationNode is ProcedureDefinition { Flags: var pf } && pf.HasFlag(SymbolFlags.IsPure))
                {
                    foreach (var arg in fe.Args)
                    {
                        ValidateIndexKeyExpressionPurityCore(arg, program, errorToken);
                    }

                    return;
                }

                throw CreateIndexKeyPurityError(errorToken, fe.OriginalToken.Lexeme);

            case UnresolvedCallExpr uce:
                if (IsSafeIndexEngineFunction(uce.Name.Lexeme))
                {
                    foreach (var arg in uce.Arguments)
                    {
                        ValidateIndexKeyExpressionPurityCore(arg, program, errorToken);
                    }

                    return;
                }

                throw CreateIndexKeyPurityError(errorToken, uce.Name.Lexeme);

            default:
                throw CreateError(errorToken,
                    "Espressione non consentita in una chiave indice in modalità safe. " +
                    "Usare UNSAFE sulla chiave, aprire la work area in UNSAFE, o una funzione <Pure>.",
                    ErrorCode.InvalidParameter);
        }
    }

    private static bool IsSafeIndexBuiltin(Token name) =>
        SafeIndexBuiltinKinds.Contains(name.Kind) ||
        SafeIndexEngineFunctions.Contains(name.Lexeme);

    private static bool IsSafeIndexEngineFunction(string name) =>
        SafeIndexEngineFunctions.Contains(name);

    private static BasicException CreateIndexKeyPurityError(Token errorToken, string functionName) =>
        CreateError(errorToken,
            $"La funzione '{functionName}' non è pura e non può essere usata in una chiave indice senza UNSAFE. " +
            "Marcare la funzione con <Pure> o aggiungere UNSAFE alla chiave / work area.",
            ErrorCode.InvalidParameter);

    private static void ValidateRewrittenIndexKey(
        Expr rewritten,
        bool fieldIsUnsafe,
        AstRewriteContext context,
        BasicProgram program,
        Token errorToken)
    {
        var effectiveUnsafe = fieldIsUnsafe || context.DbfState.CurrentIsUnsafe;
        ValidateIndexKeyExpressionPurity(rewritten, effectiveUnsafe, program, errorToken);
    }

    private static Token IndexKeyErrorToken(Expr expr) => expr switch
    {
        VariableExpr v => v.NameCleaned,
        FunctionExpr f => f.OriginalToken,
        BuiltinFunctionCallExpr b => b.Name,
        FieldAccessExpr fa => fa.Field,
        UnresolvedCallExpr u => u.Name,
        BinaryExpr b => IndexKeyErrorToken(b.Left),
        UnaryExpr u => IndexKeyErrorToken(u.Right),
        GroupingExpr g => IndexKeyErrorToken(g.Expression),
        _ => new Token(TokenKind.Identifier, "index key", null, expr.Position)
    };
}
