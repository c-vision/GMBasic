// FILE: Interpreter.Parser.Stmt.DataRead.cs
// ReSharper disable InvalidXmlDocComment

using System.Collections.Immutable;

namespace Basic;

/// <summary>
///     Contiene i metodi per il parsing delle singole istruzioni, definizioni e clausole del linguaggio BASIC.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parsa un'istruzione DATA.
    /// Ora consuma autonomamente il token 'DATA' e gestisce correttamente una virgola finale come una stringa vuota.
    /// </summary>
    /// <returns>Un nodo <see cref="DataStmt"/> dell'AST.</returns>
    private DataStmt ParseDataStatement()
    {
        var dataToken = Consume(TokenKind.Data, "Internal parser error: Expected 'DATA'.");
        Log($"[PARSER.ParseStatement]   Rilevato DATA alla riga {dataToken.Line}:{dataToken.Column}.");
        var valuesBuilder = ImmutableArray.CreateBuilder<Expr>();

        // Se la linea DATA è vuota (es. DATA seguito da Newline), non fare nulla.
        if (Check(TokenKind.Newline) || IsAtEnd() || Check(TokenKind.Colon))
        {
            return new DataStmt(valuesBuilder.ToImmutable(), dataToken);
        }

        while (true)
        {
            // Parsa l'espressione del valore
            valuesBuilder.Add(ParseExpression());

            // Controlla cosa segue
            if (Match(TokenKind.Comma))
            {
                // Abbiamo consumato una virgola. Se la riga finisce qui,
                // aggiungiamo una stringa vuota.
                if (!Check(TokenKind.Newline) && !IsAtEnd() && !Check(TokenKind.Colon)) continue;
                valuesBuilder.Add(new StringExpr("", Previous()));
                // Altrimenti, continua il loop per il prossimo valore.
            }

            // Se non c'è una virgola, la lista dei dati è finita.
            break;
        }

        Log($"[PARSER.ParseStatement]   Fine parsing DATA. Conteggio valori: {valuesBuilder.Count}.");
        return new DataStmt(valuesBuilder.ToImmutable(), dataToken);
    }

    /// <summary>
    /// Parsa un'istruzione READ.
    /// Ora consuma autonomamente il token 'READ'.
    /// </summary>
    /// <returns>Un nodo <see cref="ReadStmt"/>.</returns>
    private ReadStmt ParseReadStatement()
    {
        var readToken = Consume(TokenKind.Read, "Internal parser error: Expected 'READ'.");
        Log($"[PARSER.ParseStatement]   Rilevato READ alla riga {readToken.Line}:{readToken.Column}.");

        var variables = ParseTargetList();

        Log($"[PARSER.ParseStatement]   Fine parsing READ. Conteggio variabili: {variables.Length}.");
        return new ReadStmt(variables, readToken);
    }

    /// <summary>
    /// Parsa un'istruzione RESTORE [etichetta | numero_linea].
    /// QUESTA VERSIONE È CORRETTA E GESTISCE L'ARGOMENTO OPZIONALE.
    /// </summary>
    /// <returns>Un nodo <see cref="RestoreStmt"/>.</returns>
    private RestoreStmt ParseRestoreStatement()
    {
        // Consuma la keyword 'RESTORE'.
        var restoreToken = Consume(TokenKind.Restore, "Internal parser error: Expected 'RESTORE'.");

        // Controlla la presenza di un argomento opzionale.
        Token? target = null;
        if (IsAtEndOfStatement()) return new RestoreStmt(restoreToken, target);
        // Se non siamo alla fine dell'istruzione, ci aspettiamo un numero o un'etichetta.
        if (Check(TokenKind.Number) || Check(TokenKind.Identifier))
        {
            target = Advance();
        }
        else
        {
            // Se c'è qualcos'altro, è un errore di sintassi.
            throw CreateError(Peek(), "Expected a line number or a label after 'RESTORE'.");
        }

        // Crea e restituisce il nodo AST, includendo il target se presente.
        return new RestoreStmt(restoreToken, target);
    }
    
}