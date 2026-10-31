// FILE: Interpreter.Parser.Stmt.Arrays.cs

namespace Basic;

/// <summary>
/// Contiene le versioni dei metodi di parsing per i comandi del namespace ARRAYS,
/// specifiche per la Fase 1 (Parsing Puro). Questi metodi costruiscono l'AST
/// senza interagire con la SymbolTable e supportano la doppia sintassi (Legacy e Qualificata).
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Parsa un'istruzione AADD.
    /// Legacy: AADD array, value
    /// Moderna: ARRAYS.AADD array, value (senza parentesi)
    /// </summary>
    public static AAddStmt ParseAAddStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.AAdd, "Internal parser error.");

        // --- Sintassi unificata (legacy e qualificata fusa): array, value ---
        var array = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after array name.");
        var value = parser.ParseExpression();

        return new AAddStmt(array, value, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione AINS.
    /// Legacy: AINS array, position
    /// Moderna: ARRAYS.AINS array, position (senza parentesi)
    /// </summary>
    public static AInsStmt ParseAInsStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.AIns, "Internal parser error.");

        // --- Sintassi unificata (legacy e qualificata fusa): array, position ---
        var array = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after array name.");
        var position = parser.ParseExpression();

        return new AInsStmt(array, position, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione ADEL.
    /// Legacy: ADEL array, position
    /// Moderna: ARRAYS.ADEL array, position (senza parentesi)
    /// </summary>
    public static ADelStmt ParseADelStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.ADel, "Internal parser error.");

        // --- Sintassi unificata (legacy e qualificata fusa): array, position ---
        var array = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after array name.");
        var position = parser.ParseExpression();

        return new ADelStmt(array, position, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione ASHUFFLE.
    /// Legacy: ASHUFFLE array
    /// Moderna: ARRAYS.ASHUFFLE array (senza parentesi)
    /// </summary>
    public static AShuffleStmt ParseAShuffleStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.AShuffle, "Internal parser error.");
        var array = parser.ParseTarget();
        return new AShuffleStmt(array, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione ASORT.
    /// Legacy: ASORT array [, func] [, order]
    /// Moderna: ARRAYS.ASORT array [, func] [, order] (senza parentesi)
    /// </summary>
    public static ASortStmt ParseASortStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.ASort, "Internal parser error.");
        Target array;
        Expr? compareFunc = null;
        Expr? order = null;

        // --- Sintassi unificata (legacy e qualificata fusa): array [, func] [, order] ---
        array = parser.ParseTarget();
        if (parser.Match(TokenKind.Comma))
        {
            ParseOptionalASortArgs(parser, ref compareFunc, ref order);
        }

        return new ASortStmt(array, compareFunc, order, token);
    }

    /// <summary>
    /// Helper per parsare gli argomenti opzionali di ASORT, condiviso tra le due sintassi.
    /// </summary>
    private static void ParseOptionalASortArgs(Parser parser, ref Expr? compareFunc, ref Expr? order)
    {
        // Caso 1: Il primo argomento opzionale è omesso (es. ASORT(arr, , "D") o ASORT arr, , "D").
        if (parser.Check(TokenKind.Comma))
        {
            parser.Advance(); // Consuma la seconda virgola.
            order = parser.ParseExpression();
        }
        else // Almeno un argomento opzionale è presente.
        {
            var firstOptionalArg = parser.ParseExpression();

            // Controlla se segue un secondo argomento opzionale.
            if (parser.Match(TokenKind.Comma))
            {
                // Caso 2: Entrambi gli argomenti sono presenti (es. ASORT(arr, func, "D")).
                compareFunc = firstOptionalArg;
                order = parser.ParseExpression();
            }
            else
            {
                // Caso 3: È presente un solo argomento. Potrebbe essere la funzione o l'ordine.
                // Applichiamo un'euristica: se è una stringa "A" o "D", è l'ordine.
                if (firstOptionalArg is StringExpr strExpr)
                {
                    var val = strExpr.Value.Trim();
                    if (val.Equals("A", StringComparison.OrdinalIgnoreCase) ||
                        val.Equals("D", StringComparison.OrdinalIgnoreCase))
                    {
                        order = firstOptionalArg;
                    }
                    else
                    {
                        compareFunc = firstOptionalArg;
                    }
                }
                else
                {
                    // Qualsiasi altro tipo di espressione è considerato la funzione di confronto.
                    compareFunc = firstOptionalArg;
                }
            }
        }
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione AEVAL.
    /// Legacy: AEVAL array, func
    /// Moderna: ARRAYS.AEVAL array, func (senza parentesi)
    /// </summary>
    public static AEvalStmt ParseAEvalStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.AEval, "Internal parser error.");

        // --- Sintassi unificata (legacy e qualificata fusa): array, func ---
        var array = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after array name.");
        var actionFunc = parser.ParseExpression();

        return new AEvalStmt(array, actionFunc, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione AFILL.
    /// Legacy: AFILL array, value [, start] [, count]
    /// Moderna: ARRAYS.AFILL array, value [, start] [, count] (senza parentesi)
    /// </summary>
    public static AFillStmt ParseAFillStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.AFill, "Internal parser error.");

        // --- Sintassi unificata (legacy e qualificata fusa): array, value [, start] [, count] ---
        var array = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after array name.");
        var value = parser.ParseExpression();

        Expr? start = null;
        Expr? count = null;
        if (parser.Match(TokenKind.Comma)) start = parser.ParseOptionalExpression();
        if (parser.Match(TokenKind.Comma)) count = parser.ParseOptionalExpression();

        return new AFillStmt(array, value, start, count, token);
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione ACOPY.
    /// Legacy: ACOPY source, dest [, sStart] [, count] [, dStart]
    /// Moderna: ARRAYS.ACOPY source, dest [, sStart] [, count] [, dStart] (senza parentesi)
    /// </summary>
    public static ACopyStmt ParseACopyStatement(Parser parser, string? ns)
    {
        var token = parser.Consume(TokenKind.ACopy, "Internal parser error.");

        // --- Sintassi unificata (legacy e qualificata fusa): source, dest [, sStart] [, count] [, dStart] ---
        var source = parser.ParseTarget();
        parser.Consume(TokenKind.Comma, "Expected ',' after source array.");
        var destination = parser.ParseTarget();

        Expr? sourceStart = null;
        Expr? count = null;
        Expr? destStart = null;
        if (parser.Match(TokenKind.Comma)) sourceStart = parser.ParseOptionalExpression();
        if (parser.Match(TokenKind.Comma)) count = parser.ParseOptionalExpression();
        if (parser.Match(TokenKind.Comma)) destStart = parser.ParseOptionalExpression();

        return new ACopyStmt(source, destination, sourceStart, count, destStart, token);
    }
}