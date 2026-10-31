// FILE: Interpreter.Linker.Optimization.Lowering.cs

namespace Basic;

public static partial class Linker
{
    /// <summary>
    /// Pass di ottimizzazione AST: IR Lowering e Normalizzazione del Flusso di Controllo.
    /// Semplifica e canonizza i costrutti complessi dell'AST prima della compilazione.
    /// </summary>
    public static class AstLowerer
    {
        public static Stmt LowerStatement(Stmt statement)
        {
            switch (statement)
            {
                case ForStmt forStmt when forStmt.Step == null:
                    // Normalizzazione cicli FOR senza STEP esplicito -> STEP 1
                    var stepTok = new Token(TokenKind.Number, "1", 1.0, forStmt.Position);
                    var defaultStep = new NumberExpr(1.0, stepTok);
                    return forStmt with { Step = defaultStep };

                case IfStmt ifStmt:
                    // Normalizzazione rami IF
                    var loweredClauses = new List<IfClause>();
                    foreach (var clause in ifStmt.IfClauses)
                    {
                        var loweredStmt = clause.Statements.Select(LowerStatement).ToList();
                        loweredClauses.Add(clause with { Statements = [.. loweredStmt] });
                    }
                    ElseClause? loweredElse = ifStmt.ElseClause != null 
                        ? ifStmt.ElseClause with { Statements = [.. ifStmt.ElseClause.Statements.Select(LowerStatement)] } 
                        : null;

                    return ifStmt with { IfClauses = [.. loweredClauses], ElseClause = loweredElse };

                default:
                    return statement;
            }
        }
    }
}
