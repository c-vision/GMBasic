// FILE: Interpreter.Parser.Stmt.Matrix.cs
/*
------------------------------------------------------------------------------------------------------------------------
       GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI MATRIX (LEGACY PREFISSATA E QUALIFICATA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file supportano due stili di sintassi per le operazioni sulle matrici di trasformazione,
garantendo la retrocompatibilità con il prefisso 'MATRIX' e fornendo un'API moderna e non ambigua.

La sintassi "Legacy Prefissata" richiede che ogni comando sia preceduto dalla keyword `MATRIX`.
La sintassi "Moderna" o "Qualificata fusa" usa `MATRIX.VERBOFUSO` SENZA parentesi (direttiva
AGENTS.md §2.2): il verbo è fuso in un unico identificatore e gli argomenti sono gli STESSI
della forma legacy multi-parola.

--- Esempi di Doppia Sintassi ---

1. OPERAZIONI DI BASE E TRASFORMAZIONE
   Legacy:  MATRIX IDENTITY hMat
   Moderna: MATRIX.IDENTITY hMat

   Legacy:  MATRIX TRANSLATE hMat, x, y, z
   Moderna: MATRIX.TRANSLATE hMat, x, y, z

   Legacy:  MATRIX ROTATE hMat, ax, ay, az
   Moderna: MATRIX.ROTATE hMat, ax, ay, az

   Legacy:  MATRIX SCALE hMat, sx, sy, sz
   Moderna: MATRIX.SCALE hMat, sx, sy, sz

2. OPERAZIONI AVANZATE E STACK
   Legacy:  MATRIX MULTIPLY hM1, hM2, hRes
   Moderna: MATRIX.MULTIPLY hM1, hM2, hRes

   Legacy:  MATRIX PUSH hMat
   Moderna: MATRIX.PUSH hMat

   Legacy:  MATRIX POP hMat
   Moderna: MATRIX.POP hMat

3. IMPOSTAZIONI DEL MOTORE DI RENDERING (PIPELINE)
   Legacy:  MATRIX WORLD hMat
   Moderna: MATRIX.WORLD hMat

   Legacy:  MATRIX VIEW hMat
   Moderna: MATRIX.VIEW hMat

   Legacy:  MATRIX PROJECTION hMat
   Moderna: MATRIX.PROJECTION hMat

NOTA: Tutti i comandi del namespace MATRIX supportano entrambe le sintassi.
------------------------------------------------------------------------------------------------------------------------
*/

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi del sottosistema MATRIX che iniziano con la keyword 'MATRIX' (Sintassi Legacy).
    /// Legge il sottocomando e delega al parser specifico.
    /// </summary>
    public static Stmt DispatchMatrixStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Matrix, "Internal parser error: Expected 'MATRIX' token.");
        var subCommandToken = parser.Peek();

        return subCommandToken.Kind switch
        {
            TokenKind.Identity => ParseMatrixIdentityStatement(parser, expectedNamespace),
            TokenKind.Translate => ParseMatrixTranslateStatement(parser, expectedNamespace),
            TokenKind.Rotate => ParseMatrixRotateStatement(parser, expectedNamespace),
            TokenKind.Scale => ParseMatrixScaleStatement(parser, expectedNamespace),
            TokenKind.Multiply => ParseMatrixMultiplyStatement(parser, expectedNamespace),
            TokenKind.Push => ParseMatrixPushStatement(parser, expectedNamespace),
            TokenKind.Pop => ParseMatrixPopStatement(parser, expectedNamespace),
            TokenKind.World => ParseMatrixSetWorldStatement(parser, expectedNamespace),
            TokenKind.View => ParseMatrixSetViewStatement(parser, expectedNamespace),
            TokenKind.Projection => ParseMatrixSetProjectionStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando 'MATRIX {subCommandToken.Lexeme}' non riconosciuto.")
        };
    }

    /// <summary>
    /// Parsa il comando IDENTITY per resettare una matrice.
    /// </summary>
    public static Stmt ParseMatrixIdentityStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma IDENTITY

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId ---
        var matrixId = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixIdentityCommand, Lexeme = "IDENTITY" },
            [matrixId], opToken);
    }

    /// <summary>
    /// Parsa il comando TRANSLATE per applicare una traslazione.
    /// </summary>
    public static Stmt ParseMatrixTranslateStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma TRANSLATE

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId, x, y, z ---
        var matrixId = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var x = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var y = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var z = parser.ParseExpression();

        return new MatrixCommandStmt(
            opToken with { Kind = TokenKind.LinAlgMatrixTranslateCommand, Lexeme = "TRANSLATE" }, [matrixId, x, y, z],
            opToken);
    }

    /// <summary>
    /// Parsa il comando ROTATE per applicare una rotazione.
    /// </summary>
    public static Stmt ParseMatrixRotateStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma ROTATE

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId, ax, ay, az ---
        var matrixId = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var ax = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var ay = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var az = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixRotateCommand, Lexeme = "ROTATE" },
            [matrixId, ax, ay, az], opToken);
    }

    /// <summary>
    /// Parsa il comando SCALE per applicare una scalatura.
    /// </summary>
    public static Stmt ParseMatrixScaleStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma SCALE

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId, sx, sy, sz ---
        var matrixId = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var sx = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var sy = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var sz = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixScaleCommand, Lexeme = "SCALE" },
            [matrixId, sx, sy, sz], opToken);
    }

    /// <summary>
    /// Parsa il comando MULTIPLY per moltiplicare due matrici in-place.
    /// </summary>
    public static Stmt ParseMatrixMultiplyStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma MULTIPLY

        // --- Sintassi unificata (legacy e qualificata fusa): m1, m2, res ---
        var m1 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var m2 = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var res = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixMultiplyCommand, Lexeme = "MULTIPLY" },
            [m1, m2, res], opToken);
    }

    /// <summary>
    /// Parsa il comando PUSH per salvare una matrice sullo stack.
    /// </summary>
    public static Stmt ParseMatrixPushStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma PUSH

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId ---
        var matrixId = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixPushCommand, Lexeme = "PUSH" },
            [matrixId], opToken);
    }

    /// <summary>
    /// Parsa il comando POP per ripristinare una matrice dallo stack.
    /// </summary>
    public static Stmt ParseMatrixPopStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma POP

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId ---
        var matrixId = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixPopCommand, Lexeme = "POP" },
            [matrixId], opToken);
    }

    /// <summary>
    /// Parsa il comando WORLD per impostare la matrice World.
    /// </summary>
    public static Stmt ParseMatrixSetWorldStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma WORLD

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId ---
        var matrixId = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixSetWorldCommand, Lexeme = "WORLD" },
            [matrixId], opToken);
    }

    /// <summary>
    /// Parsa il comando VIEW per impostare la matrice View.
    /// </summary>
    public static Stmt ParseMatrixSetViewStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma VIEW

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId ---
        var matrixId = parser.ParseExpression();

        return new MatrixCommandStmt(opToken with { Kind = TokenKind.LinAlgMatrixSetViewCommand, Lexeme = "VIEW" },
            [matrixId], opToken);
    }

    /// <summary>
    /// Parsa il comando PROJECTION per impostare la matrice Projection.
    /// </summary>
    public static Stmt ParseMatrixSetProjectionStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma PROJECTION

        // --- Sintassi unificata (legacy e qualificata fusa): matrixId ---
        var matrixId = parser.ParseExpression();

        return new MatrixCommandStmt(
            opToken with { Kind = TokenKind.LinAlgMatrixSetProjectionCommand, Lexeme = "PROJECTION" }, [matrixId],
            opToken);
    }
}