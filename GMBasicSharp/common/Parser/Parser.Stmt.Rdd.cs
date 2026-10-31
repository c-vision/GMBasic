// FILE: Parser.Stmt.Rdd.cs
// Sintassi legacy RDD (senza parentesi): comandi multi-parola sempre token, mai
// identificatori concatenati (SET FIELD, SET ORDER, SET REPROCESS, SET LOCK TIMEOUT,
// CREATE INDEX, CLOSE INDEX, ON EVENT, OFF EVENT, COPY TO, SORT TO, TOTAL ON).
// DELETE/RECALL/REPLACE usano le clausole di scope (ALL/NEXT/RECORD/REST/FOR/WHILE).

using System.Collections.Immutable;

namespace Basic;

public sealed partial class Parser
{
    public static Stmt ParseRddStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Rdd, "Expected 'RDD'.");
        var cmd = parser.Peek();
        
        return ParseRddSubcommand(parser, expectedNamespace);
    }

    /// <summary>RDD DELETE [scope] — marca i record come cancellati.</summary>
    public static Stmt ParseRddDeleteStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Delete, "Internal parser error.");
        var scope = ParseScopeClause(parser);
        return new RddDeleteStmt(scope, commandToken);
    }

    /// <summary>RDD RECALL [scope] — ripristina i record cancellati.</summary>
    public static Stmt ParseRddRecallStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Recall, "Internal parser error.");
        var scope = ParseScopeClause(parser);
        return new RddRecallStmt(scope, commandToken);
    }

    /// <summary>RDD REPLACE field WITH value [, ...] [scope] — sostituisce i valori dei campi.</summary>
    public static Stmt ParseRddReplaceStatement(Parser parser, string expectedNamespace)
    {
        var commandToken = parser.Consume(TokenKind.Replace, "Internal parser error.");
        var replacements = ImmutableArray.CreateBuilder<Assignment>();
        DbfScopeClause? scope = null;

        do
        {
            var target = parser.ParseTarget();
            parser.Consume(TokenKind.With, "Expected 'WITH' after field name.");
            // TokenKind.All mancava dallo stop-set (stesso bug/causa di DBF REPLACE, vedi
            // Parser.Stmt.Dbf.cs): "RDD REPLACE campo WITH valore ALL" assorbiva "ALL"
            // nell'espressione valore invece di fermarsi, lasciando ParseScopeClause senza
            // token da riconoscere — restava sempre sullo scope di default (Current).
            var value = parser.ParseExpressionFragment(TokenKind.Comma, TokenKind.All, TokenKind.For,
                TokenKind.While, TokenKind.Next, TokenKind.Record, TokenKind.Rest);
            replacements.Add(new Assignment(target, value));
        } while (parser.Match(TokenKind.Comma));

        scope = ParseScopeClause(parser);

        return new RddReplaceStmt(replacements.ToImmutable(), scope, commandToken);
    }
    internal static Stmt ParseRddSubcommand(Parser parser, string expectedNamespace)
    {
        var cmd = parser.Peek();
        return cmd.Kind switch
        {
            TokenKind.Go or TokenKind.Goto => ParseDbfGoStatement(parser, expectedNamespace),
            TokenKind.Skip => ParseDbfSkipStatement(parser, expectedNamespace),
            TokenKind.Seek => ParseDbfSeekStatement(parser, expectedNamespace),
            TokenKind.Select => ParseDbfSelectStatement(parser, expectedNamespace),
            TokenKind.Pack => ParseDbfPackStatement(parser, expectedNamespace),
            TokenKind.Zap => ParseDbfZapStatement(parser, expectedNamespace),
            TokenKind.Close => ParseRddCloseDispatcher(parser, expectedNamespace),
            TokenKind.Use => ParseRddUseStatement(parser, expectedNamespace),
            TokenKind.RLock => ParseRddRLockStatement(parser, expectedNamespace),
            TokenKind.FLock => ParseRddFLockStatement(parser, expectedNamespace),
            TokenKind.Unlock => ParseRddUnlockStatement(parser, expectedNamespace),
            TokenKind.Set => ParseRddSetSubCommand(parser),
            TokenKind.On => ParseRddOnEventStatement(parser, expectedNamespace),
            TokenKind.Off => ParseRddOffEventStatement(parser, expectedNamespace),
            TokenKind.Create => ParseRddCreateIndexStatement(parser, expectedNamespace),
            TokenKind.Copy => ParseRddCopyToStatement(parser, expectedNamespace),
            TokenKind.Sort => ParseRddSortToStatement(parser, expectedNamespace),
            TokenKind.Total => ParseRddTotalOnStatement(parser, expectedNamespace),
            TokenKind.Delete => ParseRddDeleteStatement(parser, expectedNamespace),
            TokenKind.Recall => ParseRddRecallStatement(parser, expectedNamespace),
            TokenKind.Replace => ParseRddReplaceStatement(parser, expectedNamespace),
            _ => throw CreateError(cmd, $"Comando '{cmd.Lexeme}' non riconosciuto per RDD. Usa sintassi qualificata: RDD.COMANDO(...)")
        };
    }


    

    /// <summary>Sub-dispatcher per RDD SET ...</summary>
    private static Stmt ParseRddSetSubCommand(Parser parser)
    {
        parser.Consume(TokenKind.Set, "Expected 'SET'.");
        var sub = parser.Peek();
        
        if (sub.Kind == TokenKind.Default)
        {
            parser.Consume(TokenKind.Default, "Expected 'DEFAULT'.");
            if (parser.Check(TokenKind.To)) parser.Advance(); // TO opzionale: RDD SET DEFAULT "ISAM"
            var expr = parser.ParseExpression();
            return new RddSetDefaultStmt(expr, parser.Previous());
        }
        
        if (sub.Kind == TokenKind.Deleted)
        {
            parser.Advance(); // consuma DELETED
            var onOff = parser.Advance();
            var on = onOff.Kind is TokenKind.On or TokenKind.True
                || onOff.Lexeme == "1";
            return new RddSetDeletedStmt(on, parser.Previous());
        }
        
        if (sub.Kind == TokenKind.Field)
        {
            // RDD SET FIELD area, campo, valore
            parser.Consume(TokenKind.Field, "Expected 'FIELD'.");
            var area = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after area.");
            var field = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after campo.");
            var value = parser.ParseExpression();
            return new RddSetFieldStmt(area, field, value, parser.Previous());
        }
        
        if (sub.Kind == TokenKind.Order)
        {
            // RDD SET ORDER TO area, ordine  |  RDD SET ORDER area, ordine
            parser.Consume(TokenKind.Order, "Expected 'ORDER'.");
            if (parser.Check(TokenKind.To)) parser.Advance();
            var area = parser.ParseExpression();
            Expr order;
            if (parser.Match(TokenKind.Comma))
                order = parser.ParseExpression();
            else
                order = new NilExpr(parser.Previous());
            return new RddSetOrderStmt(area, order, parser.Previous());
        }
        
        if (sub.Kind == TokenKind.Reprocess)
        {
            // RDD SET REPROCESS TO n
            parser.Consume(TokenKind.Reprocess, "Expected 'REPROCESS'.");
            if (parser.Check(TokenKind.To)) parser.Advance();
            var expr = parser.ParseExpression();
            return new RddSetReprocessStmt(expr, parser.Previous());
        }
        
        if (sub.Kind == TokenKind.Lock)
        {
            // RDD SET LOCK TIMEOUT TO n
            parser.Consume(TokenKind.Lock, "Expected 'LOCK'.");
            parser.Consume(TokenKind.Timeout, "Expected 'TIMEOUT' after 'LOCK'.");
            if (parser.Check(TokenKind.To)) parser.Advance();
            var expr = parser.ParseExpression();
            return new RddSetLockTimeoutStmt(expr, parser.Previous());
        }
        
        throw CreateError(sub, $"Comando SET '{sub.Lexeme}' non riconosciuto per RDD. Usa: SET DEFAULT TO, SET DELETED ON|OFF, SET FIELD, SET ORDER, SET REPROCESS, SET LOCK TIMEOUT");
    }
    
    /// <summary>RDD ON EVENT "evento" [, callback].</summary>
    public static Stmt ParseRddOnEventStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.On, "Expected 'ON'.");
        parser.Consume(TokenKind.Event, "Expected 'EVENT' after 'ON'.");
        var eventName = parser.ParseExpression();
        Expr? callback = null;
        if (parser.Match(TokenKind.Comma))
            callback = parser.ParseExpression();
        return new RddOnEventStmt(eventName, callback, parser.Previous());
    }
    
    /// <summary>RDD OFF EVENT "evento".</summary>
    public static Stmt ParseRddOffEventStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Off, "Expected 'OFF'.");
        parser.Consume(TokenKind.Event, "Expected 'EVENT' after 'OFF'.");
        var eventName = parser.ParseExpression();
        return new RddOffEventStmt(eventName, parser.Previous());
    }
    
    /// <summary>
    /// RDD CREATE INDEX area, "nome", "chiave"   (sintassi spazio; ON "chiave" accettato)
    /// oppure RDD CREATE INDEX "nome" ON "chiave" (area implicita).
    /// </summary>
    public static Stmt ParseRddCreateIndexStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Create, "Expected 'CREATE'.");
        parser.Consume(TokenKind.Index, "Expected 'INDEX' after 'CREATE'.");
        Expr area = new NilExpr(parser.Previous());
        Expr name, key;
        name = parser.ParseExpression();
        if (parser.Match(TokenKind.On))
        {
            // Forma: RDD CREATE INDEX "nome" ON "chiave" — area implicita
            key = parser.ParseExpression();
        }
        else if (parser.Match(TokenKind.Comma))
        {
            // Forma: RDD CREATE INDEX area, "nome", "chiave"
            area = name;
            name = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ','.");
            key = parser.ParseExpression();
        }
        else
        {
            throw CreateError(parser.Peek(), "Expected 'ON' or ',' after index name in CREATE INDEX.");
        }
        return new RddCreateIndexStmt(area, name, key, parser.Previous());
    }
    
    /// <summary>
    /// RDD CLOSE INDEX area, "nome"  (sintassi spazio) oppure RDD CLOSE INDEX "nome" (area implicita).
    /// </summary>
    /// <summary>RDD CLOSE [INDEX name] — senza INDEX chiude l'area corrente (cursore), con INDEX chiude l'indice.</summary>
    public static Stmt ParseRddCloseDispatcher(Parser parser, string expectedNamespace)
    {
        // Al momento della chiamata il cursore è ANCORA su 'CLOSE' (non consumato): Check
        // guarda il token CORRENTE, quindi Check(Index) confrontava sempre "CLOSE" con
        // "INDEX" (mai vero) — "RDD CLOSE INDEX ..." veniva sempre interpretato come un
        // semplice "RDD CLOSE" (chiude l'area), lasciando "INDEX area, nome" come
        // istruzione successiva non valida ("Token '1' inatteso all'inizio di
        // un'istruzione"). Serve guardare il token DOPO 'CLOSE' (At(1)), non quello corrente.
        if (parser.At(1).Kind == TokenKind.Index)
            return ParseRddCloseIndexStatement(parser, expectedNamespace);
        return ParseDbfCloseStatement(parser, expectedNamespace);
    }

    public static Stmt ParseRddCloseIndexStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");
        parser.Consume(TokenKind.Index, "Expected 'INDEX' after 'CLOSE'.");
        Expr area = new NilExpr(parser.Previous());
        Expr name;
        name = parser.ParseExpression();
        if (parser.Match(TokenKind.Comma))
        {
            // Forma: RDD CLOSE INDEX area, "nome"
            area = name;
            name = parser.ParseExpression();
        }
        return new RddCloseIndexStmt(area, name, parser.Previous());
    }
    
    /// <summary>RDD COPY TO file [VIA driver].</summary>
    public static Stmt ParseRddCopyToStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Copy, "Expected 'COPY'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'COPY'.");
        var file = parser.ParseExpression();
        Expr? outDriver = null;
        if (parser.Match(TokenKind.Via))
        {
            outDriver = parser.ParseExpression();
        }
        var area = new NilExpr(parser.Previous());
        return new RddCopyToStmt(area, file, outDriver, parser.Previous());
    }
    
    /// <summary>RDD SORT TO file [FIELDS ...].</summary>
    public static Stmt ParseRddSortToStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Sort, "Expected 'SORT'.");
        parser.Consume(TokenKind.To, "Expected 'TO' after 'SORT'.");
        var file = parser.ParseExpression();
        Expr? fields = null;
        if (parser.Match(TokenKind.Fields))
        {
            fields = parser.ParseExpression();
        }
        var area = new NilExpr(parser.Previous());
        return new RddSortToStmt(area, file, fields, parser.Previous());
    }
    
    /// <summary>RDD TOTAL ON chiave.</summary>
    public static Stmt ParseRddTotalOnStatement(Parser parser, string expectedNamespace)
    {
        parser.Consume(TokenKind.Total, "Expected 'TOTAL'.");
        parser.Consume(TokenKind.On, "Expected 'ON' after 'TOTAL'.");
        var key = parser.ParseExpression();
        var area = new NilExpr(parser.Previous());
        return new RddTotalOnStmt(area, key, parser.Previous());
    }

    // ========================================================================
    // Forme QUALIFICATE con punto (RDD.ONEVENT "x", RDD.CREATEINDEX "i" ON "k", ...).
    // La sintassi qualificata di un COMANDO non usa mai le parentesi da funzione:
    // consuma l'Identifier concatenato e gli argomenti in linea. La forma canonica
    // resta quella con SPAZIO (RDD ON EVENT ...) — questi parser coprono il punto.
    // ========================================================================

    /// <summary>RDD.ONEVENT "evento" [, callback] — forma qualificata.</summary>
    /// </summary>
    public static Stmt ParseRddUseStatement(Parser parser, string expectedNamespace)
    {
        var useToken = parser.Consume(TokenKind.Use, "Expected 'USE'.");
        var fileName = parser.ParseExpression();
        
        // VIA "driver" (opzionale — se assente, auto-detect da estensione)
        Expr driver;
        if (parser.Match(TokenKind.Via))
        {
            driver = parser.ParseExpression();
        }
        else
        {
            driver = new NilExpr(parser.Previous()); // NIL = auto-detect
        }
        
        // ALIAS x (opzionale)
        Token? aliasToken = null;
        if (parser.Match(TokenKind.Alias))
        {
            aliasToken = parser.Advance();
        }
        
        return new RddUseStmt(fileName, driver, aliasToken, useToken);
    }

    public static Stmt ParseRddRLockStatement(Parser parser, string ns)
    {
        var cmd = parser.Consume(TokenKind.RLock, "Expected 'RLOCK'.");
        Token? area = null;
        if (parser.Check(TokenKind.LeftParen))
        {
            parser.Advance();
            if (!parser.Check(TokenKind.RightParen))
                area = parser.Advance();
            parser.Consume(TokenKind.RightParen, "Expected ')'.");
        }
        else if (parser.Check(TokenKind.Number) || parser.Check(TokenKind.Identifier))
        {
            area = parser.Advance(); // area number/alias legacy senza parentesi
        }
        return new RddLockStmt(RddLockKind.RLock, area, cmd);
    }

    public static Stmt ParseRddFLockStatement(Parser parser, string ns)
    {
        var cmd = parser.Consume(TokenKind.FLock, "Expected 'FLOCK'.");
        Token? area = null;
        if (parser.Check(TokenKind.LeftParen))
        {
            parser.Advance();
            if (!parser.Check(TokenKind.RightParen))
                area = parser.Advance();
            parser.Consume(TokenKind.RightParen, "Expected ')'.");
        }
        else if (parser.Check(TokenKind.Number) || parser.Check(TokenKind.Identifier))
        {
            area = parser.Advance();
        }
        return new RddLockStmt(RddLockKind.FLock, area, cmd);
    }

    public static Stmt ParseRddUnlockStatement(Parser parser, string ns)
    {
        var cmd = parser.Consume(TokenKind.Unlock, "Expected 'UNLOCK'.");
        Token? area = null;
        if (parser.Check(TokenKind.LeftParen))
        {
            parser.Advance();
            if (!parser.Check(TokenKind.RightParen))
                area = parser.Advance();
            parser.Consume(TokenKind.RightParen, "Expected ')'.");
        }
        else if (parser.Check(TokenKind.Number) || parser.Check(TokenKind.Identifier))
        {
            area = parser.Advance();
        }
        return new RddLockStmt(RddLockKind.Unlock, area, cmd);
    }

}
