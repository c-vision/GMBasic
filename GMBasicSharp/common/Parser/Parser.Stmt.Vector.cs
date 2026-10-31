// FILE: Interpreter.Parser.Stmt.Vector.cs
/*
------------------------------------------------------------------------------------------------------------------------
       GUIDA DI RIFERIMENTO ALLA DOPPIA SINTASSI VECTOR (LEGACY PREFISSATA E QUALIFICATA)
------------------------------------------------------------------------------------------------------------------------
I metodi di parsing in questo file supportano due stili di sintassi per le operazioni sui vettori,
garantendo la retrocompatibilità con il prefisso 'VECTOR' e fornendo un'API moderna e non ambigua.

La sintassi "Legacy Prefissata" richiede che ogni comando sia preceduto dalla keyword `VECTOR`.
La sintassi "Moderna" o "Qualificata fusa" usa `VECTOR.VERBOFUSO` SENZA parentesi (direttiva
AGENTS.md §2.2): il verbo è fuso in un unico identificatore e gli argomenti sono gli STESSI
della forma legacy multi-parola.

--- Esempi di Doppia Sintassi ---

1. OPERAZIONI ARITMETICHE
   Legacy:  VECTOR ADD v1(), v2()
   Moderna: VECTOR.ADD v1, v2

   Legacy:  VECTOR SUB v1(), v2()
   Moderna: VECTOR.SUB v1, v2

   Legacy:  VECTOR MUL v1(), scalar
   Moderna: VECTOR.MUL v1, scalar

   Legacy:  VECTOR DIV v1(), scalar
   Moderna: VECTOR.DIV v1, scalar

2. ASSEGNAZIONE E INIZIALIZZAZIONE
   Legacy:  VECTOR SET v1(), scalarValue
   Moderna: VECTOR.SET v1, scalarValue

   Legacy:  VECTOR FILL RANDOM v1(), -10, 10
   Moderna: VECTOR.FILLRANDOM v1, -10, 10

3. GESTIONE LIMITI (BOUNDS)
   Legacy:  VECTOR WRAP v1(), vMin(), vMax()
   Moderna: VECTOR.WRAP v1, vMin, vMax

   Legacy:  VECTOR ADD WRAP v1(), v2(), vMin(), vMax()
   Moderna: VECTOR.ADDWRAP v1, v2, vMin, vMax

4. FISICA E RENDERING BATCH
   Legacy:  VECTOR BOUNCE2D pos(), vel(), width, height
   Moderna: VECTOR.BOUNCE2D pos, vel, width, height

NOTA: Tutti i comandi del namespace VECTOR supportano entrambe le sintassi.
------------------------------------------------------------------------------------------------------------------------
*/

namespace Basic;

public sealed partial class Parser
{
    /// <summary>
    /// Dispatcher per i comandi del sottosistema VECTOR che iniziano con la keyword 'VECTOR' (Sintassi Legacy).
    /// Legge il sottocomando e delega al parser specifico.
    /// </summary>
    public static Stmt DispatchVectorStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Vector, "Internal parser error: Expected 'VECTOR' token.");
        var subCommandToken = parser.Peek();

        return subCommandToken.Kind switch
        {
            TokenKind.Add when parser.At(1).Kind == TokenKind.Wrap =>
                ParseVectorAddWrapStatement(parser, expectedNamespace),
            TokenKind.Add => ParseVectorAddStatement(parser, expectedNamespace),
            TokenKind.Function => ParseVectorSubStatement(parser, expectedNamespace),
            TokenKind.Mul => ParseVectorMulStatement(parser, expectedNamespace),
            TokenKind.Div => ParseVectorDivStatement(parser, expectedNamespace),
            TokenKind.Set => ParseVectorSetStatement(parser, expectedNamespace),
            TokenKind.Wrap => ParseVectorWrapStatement(parser, expectedNamespace),
            TokenKind.Fill when parser.At(1).Kind == TokenKind.Random =>
                ParseVectorFillRandomStatement(parser, expectedNamespace),
            TokenKind.Bounce2d => ParseVectorBounce2dStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando 'VECTOR {subCommandToken.Lexeme}' non riconosciuto.")
        };
    }

    /// <summary>
    /// Parsa il comando ADD per i vettori.
    /// </summary>
    public static Stmt ParseVectorAddStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Consume(TokenKind.Add, "Expected 'ADD'.");

        // --- Sintassi unificata (legacy e qualificata fusa): dest, source ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ',' after destination vector.");
        var source = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorAddCommand, Lexeme = "ADD" },
            [dest, source]);
    }

    /// <summary>
    /// Parsa il comando SUB per i vettori.
    /// </summary>
    public static Stmt ParseVectorSubStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma SUB (tramite lexeme nel dispatcher)

        // --- Sintassi unificata (legacy e qualificata fusa): dest, source ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ',' after destination vector.");
        var source = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorSubCommand, Lexeme = "SUB" },
            [dest, source]);
    }

    /// <summary>
    /// Parsa il comando MUL per i vettori.
    /// </summary>
    public static Stmt ParseVectorMulStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma MUL

        // --- Sintassi unificata (legacy e qualificata fusa): dest, source ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ',' after destination vector.");
        var source = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorMulCommand, Lexeme = "MUL" },
            [dest, source]);
    }

    /// <summary>
    /// Parsa il comando DIV per i vettori.
    /// </summary>
    public static Stmt ParseVectorDivStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Advance(); // Consuma DIV

        // --- Sintassi unificata (legacy e qualificata fusa): dest, source ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ',' after destination vector.");
        var source = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorDivCommand, Lexeme = "DIV" },
            [dest, source]);
    }

    /// <summary>
    /// Parsa il comando SET per i vettori.
    /// </summary>
    public static Stmt ParseVectorSetStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Consume(TokenKind.Set, "Expected 'SET'.");

        // --- Sintassi unificata (legacy e qualificata fusa): dest, value ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ',' after destination vector.");
        var value = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorSetCommand, Lexeme = "SET" },
            [dest, value]);
    }

    /// <summary>
    /// Parsa il comando WRAP per i vettori.
    /// </summary>
    public static Stmt ParseVectorWrapStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Consume(TokenKind.Wrap, "Expected 'WRAP'.");

        // --- Sintassi unificata (legacy e qualificata fusa): dest, min, max ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ',' after destination.");
        var min = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' after min.");
        var max = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorWrapCommand, Lexeme = "WRAP" },
            [dest, min, max]);
    }

    /// <summary>
    /// Parsa il comando ADD WRAP per i vettori.
    /// </summary>
    public static Stmt ParseVectorAddWrapStatement(Parser parser, string expectedNamespace)
    {
        // Forma legacy: ADD WRAP come parole separate.
        var opToken = parser.Consume(TokenKind.Add, "Expected 'ADD'.");
        parser.Consume(TokenKind.Wrap, "Expected 'WRAP' after 'ADD'.");

        // --- Argomenti unificati (senza parentesi): dest, source, min, max ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var source = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var min = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var max = parser.ParseExpression();

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorAddWrapCommand, Lexeme = "ADD WRAP" },
            [dest, source, min, max]);
    }

    /// <summary>
    /// Parsa il comando FILL RANDOM per i vettori.
    /// </summary>
    public static Stmt ParseVectorFillRandomStatement(Parser parser, string expectedNamespace)
    {
        // Forma legacy: FILL RANDOM come parole separate.
        var opToken = parser.Advance(); // Consuma FILL
        parser.Consume(TokenKind.Random, "Expected 'RANDOM' after 'FILL'.");

        // --- Argomenti unificati (senza parentesi): dest, min, max ---
        var dest = ConvertTargetToExpr(parser.ParseTarget());
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var min = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ','.");
        var max = parser.ParseExpression();

        return new VectorCommandStmt(
            opToken with { Kind = TokenKind.LinAlgVectorFillRandomCommand, Lexeme = "FILL RANDOM" }, [dest, min, max]);
    }

    /// <summary>
    /// Parsa il comando BOUNCE2D per i vettori.
    /// </summary>
    public static Stmt ParseVectorBounce2dStatement(Parser parser, string expectedNamespace)
    {
        var opToken = parser.Consume(TokenKind.Bounce2d, "Expected 'BOUNCE2D'.");
        var arguments = new List<Expr>();

        // --- Sintassi unificata (legacy e qualificata fusa), senza parentesi ---
        // La sintassi è ambigua. Tentiamo di parsare la versione a 8 argomenti per prima,
        // e se fallisce, facciamo il fallback a quella da 4.
        var savedPos = parser._current;
        try
        {
            // Tenta di parsare la versione a 8 argomenti (4 Target + 4 Expr)
            arguments.Add(ConvertTargetToExpr(parser.ParseTarget())); // posX
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(ConvertTargetToExpr(parser.ParseTarget())); // posY
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(ConvertTargetToExpr(parser.ParseTarget())); // velX
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(ConvertTargetToExpr(parser.ParseTarget())); // velY
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(parser.ParseExpression()); // minX
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(parser.ParseExpression()); // minY
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(parser.ParseExpression()); // maxX
            parser.Consume(TokenKind.Comma, "Expected ','.");
            arguments.Add(parser.ParseExpression()); // maxY
        }
        catch (BasicException)
        {
            // Il parsing a 8 argomenti è fallito. Riavvolgiamo e proviamo con 4.
            parser._current = savedPos;
            arguments.Clear();

            // Tenta di parsare la versione a 4 argomenti (2 Target + 2 Expr)
            arguments.Add(ConvertTargetToExpr(parser.ParseTarget())); // pos
            parser.Consume(TokenKind.Comma, "Expected ',' after position vector.");
            arguments.Add(ConvertTargetToExpr(parser.ParseTarget())); // vel
            parser.Consume(TokenKind.Comma, "Expected ',' after velocity vector.");
            arguments.Add(parser.ParseExpression()); // width
            parser.Consume(TokenKind.Comma, "Expected ',' after width.");
            arguments.Add(parser.ParseExpression()); // height
        }
        
        if (arguments.Count != 4 && arguments.Count != 8)
        {
             throw CreateError(opToken, $"Il comando BOUNCE2D si aspetta 4 o 8 argomenti, ma ne sono stati forniti {arguments.Count}.");
        }

        return new VectorCommandStmt(opToken with { Kind = TokenKind.LinAlgVectorBounce2dCommand, Lexeme = "BOUNCE2D" },
            [..arguments]);
    }

}