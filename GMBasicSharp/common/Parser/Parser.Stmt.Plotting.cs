// FILE: Interpreter.Parser.Stmt.Plotting.cs
// ReSharper disable SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
namespace Basic;
/// <summary>
/// Contiene i metodi di parsing per le istruzioni grafiche.
/// </summary>
public sealed partial class Parser
{
    // ========================================================================
    // --- NUOVI PARSER PER IL SOTTOSISTEMA DI PLOTTING ---
    // ========================================================================

    /// <summary>
    /// Agisce come router per la sintassi legacy `CHART sottocomando`.
    /// Viene invocato solo quando `CHART` è all'inizio di un'istruzione.
    /// </summary>
    public static Stmt DispatchChartStatement(Parser parser, string expectedNamespace)
    {
        // Disambiguazione: `CHART` seguito da un sotto-verbo è il COMANDO (CHART SET/GET/DRAW/…);
        // `CHART` seguito da argomenti (coordinate) è il CONTROLLO a celle `CONTROLS CHART x,y,w,h`.
        var nx = parser.At(1);
        bool isSub = nx.Kind is TokenKind.Plot or TokenKind.Financial or TokenKind.Clear
            or TokenKind.Close or TokenKind.Save or TokenKind.Show or TokenKind.Set
            or TokenKind.Get or TokenKind.Draw
            || (nx.Kind == TokenKind.Identifier && string.Equals(nx.Lexeme, "GETS", StringComparison.OrdinalIgnoreCase));
        if (!isSub)
            return ParsePixelControlStandalone(parser, "CONTROLS");

        parser.Consume(TokenKind.Chart, "Internal parser error.");
        return ParseChartSubcommand(parser, expectedNamespace);
    }

    /// <summary>
    /// Parsa il comando CHART.PLOT handle, x_data(), y_data(), type$ [, legend$]
    /// </summary>
    public static Stmt ParseChartPlotStatement(Parser parser, string ns)
    {
        var plotToken = parser.Consume(TokenKind.Plot, "Expected 'PLOT'.");
        var args = parser.ParseExpressionList(TokenKind.Newline);
        return args.Length is < 4 or > 5 
            ? throw CreateError(plotToken, "CHART.PLOT richiede da 4 a 5 argomenti.") 
            : new ChartPlotStmt(args, plotToken);
    }

    /// <summary>
    /// Parsa il comando CHART.FINANCIAL handle, ohlc_data(), type$ [, legend$]
    /// </summary>
    public static Stmt ParseChartFinancialStatement(Parser parser, string ns)
    {
        var financialToken = parser.Consume(TokenKind.Financial, "Expected 'FINANCIAL'.");
        var args = parser.ParseExpressionList(TokenKind.Newline);
        return args.Length is < 3 or > 4 
            ? throw CreateError(financialToken, "CHART.FINANCIAL richiede 3 o 4 argomenti.") 
            : new ChartFinancialStmt(args, financialToken);
    }
    internal static Stmt ParseChartSubcommand(Parser parser, string expectedNamespace)
    {
        var subCommandToken = parser.Peek();
        // GETS non è un TokenKind: si riconosce dal lessema (come CONTROL GETS).
        if (subCommandToken.Kind == TokenKind.Identifier
            && string.Equals(subCommandToken.Lexeme, "GETS", StringComparison.OrdinalIgnoreCase))
            return ParseChartConfig(parser, expectedNamespace, "GETS");
        return subCommandToken.Kind switch
        {
            TokenKind.Plot => ParseChartPlotStatement(parser, expectedNamespace),
            TokenKind.Financial => ParseChartFinancialStatement(parser, expectedNamespace),
            TokenKind.Clear => ParseChartClearStatement(parser, expectedNamespace),
            TokenKind.Close => ParseChartCloseStatement(parser, expectedNamespace),
            TokenKind.Save => ParseChartSaveStatement(parser, expectedNamespace),
            TokenKind.Show => ParseChartShowStatement(parser, expectedNamespace),
            TokenKind.Set => ParseChartConfig(parser, expectedNamespace, "SET"),
            TokenKind.Get => ParseChartConfig(parser, expectedNamespace, "GET"),
            TokenKind.Draw => ParseChartDrawStatement(parser, expectedNamespace),
            _ => throw CreateError(subCommandToken,
                $"Sottocomando non valido per CHART: '{subCommandToken.Lexeme}'.")
        };
    }

    /// <summary>CHART SET h, "prop", num [, "str"] | CHART GET h, "prop" INTO v |
    /// CHART GETS h, "prop" INTO v$ — configurazione grafico da BASIC.</summary>
    public static Stmt ParseChartConfig(Parser parser, string ns, string sub)
    {
        var tok = parser.Peek();
        parser.Advance();   // consuma SET/GET/GETS
        var handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' dopo l'handle del grafico.");
        var prop = parser.ParseExpression();
        Expr? num = null, str = null;
        Target? into = null;
        if (sub == "SET")
        {
            if (parser.Match(TokenKind.Comma))
            {
                num = parser.ParseExpression();
                if (parser.Match(TokenKind.Comma)) str = parser.ParseExpression();
            }
        }
        else
        {
            if (parser.Match(TokenKind.Into)) into = parser.ParseTarget();
        }
        return new ChartConfigStmt(handle, sub, prop, num, str, into, tok);
    }

    /// <summary>CHART DRAW h, "op"[, args...] [INTO id] — oggetti di disegno
    /// (line/hline/vline/rect/fib) + move/remove/clear/count/hit.</summary>
    public static Stmt ParseChartDrawStatement(Parser parser, string ns)
    {
        var tok = parser.Consume(TokenKind.Draw, "Expected 'DRAW'.");
        var handle = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' dopo l'handle del grafico.");
        var op = parser.ParseExpression();
        var args = new List<Expr>();
        while (parser.Match(TokenKind.Comma)) args.Add(parser.ParseExpression());
        Target? into = null;
        if (parser.Match(TokenKind.Into)) into = parser.ParseTarget();
        return new ChartDrawStmt(handle, op, [.. args], into, tok);
    }



    /// <summary>
    /// Parsa il comando CHART.CLEAR handle
    /// </summary>
    public static Stmt ParseChartClearStatement(Parser parser, string ns)
    {
        var clearToken = parser.Consume(TokenKind.Clear, "Expected 'CLEAR'.");
        var handle = parser.ParseExpression();
        return new ChartClearStmt(handle, clearToken);
    }

    /// <summary>
    /// Parsa il comando CHART.CLOSE handle
    /// </summary>
    public static Stmt ParseChartCloseStatement(Parser parser, string ns)
    {
        var closeToken = parser.Consume(TokenKind.Close, "Expected 'CLOSE'.");
        var handle = parser.ParseExpression();
        return new ChartCloseStmt(handle, closeToken);
    }

    /// <summary>
    /// Parsa il comando CHART.SAVE handle, percorso$, [larghezza], [altezza].
    /// </summary>
    public static Stmt ParseChartSaveStatement(Parser parser, string ns)
    {
        var saveToken = parser.Consume(TokenKind.Save, "Internal parser error.");
        var args = parser.ParseExpressionList(TokenKind.Newline);
        if (args.Length is < 2 or > 4)
            throw CreateError(saveToken,
                "CHART.SAVE richiede da 2 a 4 argomenti: Handle, FilePath$, [Larghezza], [Altezza].");
        return new ChartRenderStmt(true, args, saveToken);
    }

    /// <summary>
    /// Parsa il comando CHART.SHOW handle [, x, y] [, w, h]
    /// </summary>
    public static Stmt ParseChartShowStatement(Parser parser, string ns)
    {
        var showToken = parser.Consume(TokenKind.Show, "Expected 'SHOW'.");
        var args = parser.ParseExpressionList(TokenKind.Newline);
        return args.Length is < 1 or > 5 
            ? throw CreateError(showToken, "CHART.SHOW richiede da 1 a 5 argomenti.") 
            : new ChartRenderStmt(false, args, showToken);
    }
}