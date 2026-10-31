// FILE: Interpreter.Parser.Stmt.Exceptions.cs

using System.Collections.Immutable;

namespace Basic;

/// <summary>
/// Contiene i metodi per il parsing dei costrutti di gestione delle eccezioni.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Parsa un blocco TRY...CATCH...FINALLY...END TRY.
    /// Questa versione pura per la Fase 1 mantiene la gestione degli scope per creare
    /// la corretta struttura dell'albero degli scope, ma non registra simboli.
    /// </summary>
    private TryCatchStmt ParseTryCatchStatement(ProcedureDefinition? procedureContext)
    {
        var tryToken = Consume(TokenKind.Try, "Internal parser error: Expected 'TRY'.");
        Consume(TokenKind.Newline, "Expected newline after 'TRY'.");

        var catchClauses = ImmutableArray.CreateBuilder<CatchClause>();
        FinallyClause? finallyClause = null;

        // Parsa il corpo del TRY senza creare uno scope.
        var tryBody =
            ParseBlockBodyUntil(procedureContext, TokenKind.Catch, TokenKind.Finally, TokenKind.EndTry);

        // Loop per parsare una o più clausole CATCH
        while (true)
        {
            // Loop di pulizia per saltare righe vuote e numeri di riga
            while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber))
            {
            }

            if (!Match(TokenKind.Catch)) break; // Se non è CATCH, esci dal loop

            // Chiama la versione di Fase 1, che non gestirà più il proprio scope internamente.
            catchClauses.Add(ParseCatchClause(procedureContext));
        }

        // Loop di pulizia prima di cercare FINALLY
        while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber))
        {
        }

        // Parsa la clausola FINALLY opzionale, senza creare uno scope.
        if (Match(TokenKind.Finally))
        {
            var finallyToken = Previous();
            Consume(TokenKind.Newline, "Expected newline after 'FINALLY'.");
            var finallyBody = ParseBlockBodyUntil(procedureContext, TokenKind.EndTry);
            finallyClause = new FinallyClause(finallyBody, finallyToken);
        }

        // Loop di pulizia finale prima di cercare END TRY
        while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber))
        {
        }

        Consume(TokenKind.EndTry, "Expected 'END TRY' to close the TRY block.");

        return new TryCatchStmt(tryBody, catchClauses.ToImmutable(), finallyClause, tryToken);
    }

    /// <summary>
    /// [FASE 1] Parsa una singola clausola CATCH [ex AS TIPO] [WHEN condizione].
    /// Questa versione è pura, non registra simboli e chiama solo metodi di Fase 1.
    /// La gestione dello scope viene mantenuta per creare la corretta struttura dell'albero degli scope.
    /// </summary>
    /// <summary>
    /// [FASE 1] Parsa una singola clausola CATCH [ex AS TIPO] [WHEN condizione].
    /// Questa versione è pura, non registra simboli, non gestisce scope e chiama solo metodi di Fase 1.
    /// </summary>
    private CatchClause ParseCatchClause(ProcedureDefinition? procedureContext)
    {
        var catchToken = Previous();
        VariableDeclaration? variable = null;
        Expr? whenCondition = null;

        // Controlla la presenza di una variabile (es. CATCH ex [AS ERRORINFO])
        // Se il prossimo token è un identificatore, assumiamo che sia la variabile dell'eccezione.
        if (Check(TokenKind.Identifier))
        {
            var name = Consume(TokenKind.Identifier, "Expected variable name for exception.");
            DataType type;
            var hasAsClause = false;

            if (Match(TokenKind.As))
            {
                type = ParseTypeName();
                hasAsClause = true;
            }
            else
            {
                // Se AS è omesso, usiamo InferredType. 
                // Il Binder o il Linker lo risolveranno (tipicamente default a ERR).
                type = new InferredType();
            }

            // Crea un VariableDeclaration con SymbolId fittizio (-1).
            variable = new VariableDeclaration(
                -1,
                name, // NameSuffixed
                name, // NameCleaned
                ImmutableArray<Expr>.Empty,
                type,
                null,
                false,
                hasAsClause);
        }

        // Controlla la presenza di un filtro WHEN
        if (Match(TokenKind.When))
        {
            whenCondition = ParseExpression();
        }

        Consume(TokenKind.Newline, "Expected newline after CATCH clause.");
        // Chiama il parser di blocchi di Fase 1.
        var catchBody = ParseBlockBodyUntil(procedureContext, TokenKind.Catch, TokenKind.Finally, TokenKind.EndTry);

        return new CatchClause(variable, whenCondition, catchBody, catchToken);
    }

    /// <summary>
    /// [FASE 1] Parsa l'istruzione ERROR numero_errore [, descrizione].
    /// </summary>
    private ErrorStmt ParseErrorStatement()
    {
        var errorToken = Consume(TokenKind.Error, "Internal parser error: Expected 'ERROR'.");
        var errorCode = ParseExpression();
        Expr? description = null;

        if (Match(TokenKind.Comma))
        {
            description = ParseExpression();
        }

        return new ErrorStmt(errorCode, description, errorToken);
    }
    
    /// <summary>
    /// Parsa l'istruzione THROW
    /// Supporta:
    ///   THROW                    — rethrow (senza espressione)
    ///   THROW expr               — throw valutando un'espressione (es. THROW 999)
    ///   THROW NEW Class(args)    — throw con nuova istanza di classe
    /// </summary>
    private Stmt ParseThrowStatement()
    {
        var token = Consume(TokenKind.Throw, "Expected 'THROW'.");
        if (IsAtEndOfStatement())
            return new RethrowStmt(token);
        var value = ParseExpression();
        return new ThrowStmt(value, token);
    }
}