// FILE: Interpreter.Parser.JustInTime.cs

using System.Collections.Immutable;

// ReSharper disable InvalidXmlDocComment

namespace Basic;

/// <summary>
/// Fornisce metodi statici di utilità per il parsing "just-in-time" (JIT) di frammenti di codice
/// a runtime. Questa classe parziale è il cuore del supporto per l'esecuzione dinamica di codice
/// e per l'implementazione delle macro in stile Clipper.
/// </summary>
/// <remarks>
/// A differenza dell'istanza principale del <see cref="Parser"/>, che viene creata all'inizio
/// per compilare un intero programma, i metodi in questa classe sono `static` e operano in modo
/// isolato e thread-safe. Essi accettano un <see cref="BasicProgram"/> esistente, garantendo
/// che il codice compilato dinamicamente venga analizzato nello stesso "universo" del programma
/// principale, con accesso agli stessi namespace, funzioni definite dall'utente, tipi e alias.
/// 
/// L'uso principale di questi metodi è da parte dell'Executor, quando deve interpretare una stringa
/// come codice eseguibile.
/// </remarks>
/// <example>
/// <b>Esempio 1: Valutazione di un'espressione per un indice DBF</b>
/// <code>
/// // All'interno dell'Executor, durante un comando INDEX ON.
/// // L'Executor ha accesso al contesto di compilazione del programma in esecuzione.
/// var programContext = _program.CompilationContext;
/// var keyExpressionString = "UPPER(TRIM(Cognome))";
/// 
/// // Il parser JIT viene usato per trasformare la stringa in un AST eseguibile.
/// Expr keyAst = Parser.ParseSingleExpression(keyExpressionString, programContext);
/// 
/// // L'Executor può ora valutare l'AST 'keyAst' per ogni record.
/// </code>
/// </example>
/// <example>
/// <b>Esempio 2: Implementazione di una macro di espressione (stile Clipper)</b>
/// <code>
/// // Codice BASIC dell'utente:
/// cVarName = "MyVar"
/// MyVar = 100
/// PRINT &cVarName  // L'operatore '&' indica una macro
/// 
/// // Logica interna dell'Executor per l'operatore '&amp;':
/// // 1. Valuta l'espressione a destra dell'&amp; (cVarName) -> ottiene la stringa "MyVar".
/// string codeToCompile = "MyVar";
/// 
/// // 2. Usa il parser JIT per compilare la stringa "MyVar" in un nuovo AST.
/// Expr macroAst = Parser.ParseSingleExpression(codeToCompile, _program.CompilationContext);
/// 
/// // 3. Esegue il nuovo AST 'macroAst' nello scope corrente per ottenere il valore (100).
/// </code>
/// </example>
/// <example>
/// <b>Esempio 3: Implementazione di una macro di comando (stile Clipper)</b>
/// <code>
/// // Codice BASIC dell'utente:
/// cCommand = "REPLACE Prezzo WITH Prezzo * 1.1 FOR Scontato = FALSE"
/// &cCommand
/// 
/// // Logica interna dell'Executor per la macro di comando:
/// // 1. Valuta l'espressione cCommand -> ottiene la stringa del comando REPLACE.
/// string statementToCompile = "REPLACE Prezzo WITH Prezzo * 1.1 FOR Scontato = FALSE";
/// 
/// // 2. Usa il parser JIT per compilare la stringa in un nodo Stmt.
/// Stmt macroStmt = Parser.ParseSingleStatement(statementToCompile, _program.CompilationContext);
/// 
/// // 3. Inserisce il nuovo nodo 'macroStmt' nel flusso di esecuzione della VM.
/// </code>
/// </example>
public sealed partial class Parser
{
    /// <summary>
    /// Helper per il parsing "Just-In-Time" di espressioni fornite come stringhe
    /// negli argomenti dei comandi qualificati.
    /// </summary>
    private static Expr JitParseStringOrKeepExpr(Expr expr, Parser parser)
    {
        if (expr is StringExpr strExpr && !string.IsNullOrEmpty(strExpr.Value))
        {
            // Parser JIT temporanei (ParseSingleExpression) hanno una sourceMap di UNA riga:
            // _currentSourceLineIndex può puntare fuori (macri annidate/EVAL nel testo macro).
            var originalSourceLine =
                parser._currentSourceLineIndex >= 0 && parser._currentSourceLineIndex < parser._sourceMap.Length
                    ? parser._sourceMap[parser._currentSourceLineIndex]
                    : parser._sourceMap.Length > 0
                        ? parser._sourceMap[0]
                        : new SourceLine(-1, "", 0, strExpr.Value);
            var baseColumn = strExpr.Token.Column + 1;
            return ParseSingleExpression(strExpr.Value, parser._program, originalSourceLine, baseColumn,
                parser._registry);
        }
        return expr;
    }

    /// <summary>
    /// Espansione a COMPILE-TIME di una macro con target COSTANTE (letterale stringa) — la
    /// semantica Clipper 5.3 compila le macro costanti una sola volta. Usata da <c>&amp;("...")</c>
    /// e <c>EVAL("...")</c>: il testo viene parsato ORA e il nodo risultante sostituito all'AST,
    /// così un <c>&amp;("{ |x| ... }")</c> diventa una <see cref="LambdaExpr"/> TIPIZZATA (quindi
    /// invocabile con parità su tutte e 5 le gambe), senza dipendere dal motore macro a runtime
    /// (che sui transpiler C non sa compilare code block).
    /// Ritorna <c>null</c> quando la macro deve restare a RUNTIME: target non costante, testo
    /// non parsabile come espressione (comandi: PRINT/LET/...), o uguaglianza top-level
    /// (<c>"X = 99"</c> in posizione espressione è un confronto valido, ma l'utente intende
    /// l'assegnazione — la re-interpreta il percorso runtime).
    /// </summary>
    private static Expr? TryExpandConstantMacro(Expr target, Parser parser)
    {
        // &("...")/EVAL("..."): il target tra parentesi è un GroupingExpr attorno al letterale.
        var inner = target is GroupingExpr grouping ? grouping.Expression : target;
        if (inner is not StringExpr strExpr || string.IsNullOrWhiteSpace(strExpr.Value)) return null;
        try
        {
            var expanded = JitParseStringOrKeepExpr(strExpr, parser);
            // Uguaglianza top-level: "X = 99" è un confronto valido ma l'utente intende
            // l'assegnazione — la re-interpreta il percorso runtime.
            if (expanded is BinaryExpr { Op.Kind: TokenKind.Equal }) return null;
            // Nome NUDO ("ghostVar"/"X"): NON espandere — il runtime lo risolve nello scope
            // macro (globale/locale/transiente); l'espansione creerebbe un globale implicito
            // nel chiamante (leak di EVAL("ghostVar") — ClipperEvalScope/MacroLocalVar).
            if (expanded is VariableExpr) return null;
            return expanded;
        }
        catch (BasicException)
        {
            // Non è un'espressione valida (es. un comando): resta macro a runtime.
            return null;
        }
    }
    
    /// <summary>
    /// Parsa "Just-In-Time" (JIT) una stringa di codice sorgente che rappresenta una singola espressione,
    /// trasformandola in un nodo AST <see cref="Expr"/>.
    /// </summary>
    /// <param name="expressionString">La stringa di codice sorgente dell'espressione da parsare.</param>
    /// <param name="program">L'istanza di <see cref="BasicProgram"/> che fornisce il contesto di compilazione globale.</param>
    /// <param name="originalSourceLine">L'oggetto <see cref="SourceLine"/> originale da cui è stata estratta l'espressione.</param>
    /// <param name="baseColumn">La colonna (1-based) all'interno della `originalSourceLine` da cui inizia l' `expressionString`.</param>
    /// <param name="registry">Registry delle funzioni built-in.</param>
    /// <param name="verboseLexer">Se true, abilita il logging verbose del lexer.</param>
    /// <returns>Il nodo AST <see cref="Expr"/> che rappresenta l'espressione parsata.</returns>
    /// <exception cref="BasicException">Lanciata se la stringa contiene un errore di sintassi o token superflui.</exception>
    public static Expr ParseSingleExpression(string expressionString, BasicProgram program,
        SourceLine originalSourceLine, int baseColumn, IParserRegistry? registry = null,
        bool verboseLexer = false)
    {
        var expressionSourceLine = originalSourceLine with { Text = expressionString };
        var sourceMap = ImmutableArray.Create(expressionSourceLine);

        var lexer = new Lexer(sourceMap, program, verboseLexer, disableLineNumbers: true, expressionMode: true);
        var tokens = lexer.ScanTokens().Select(t =>
        {
            var newPosition = new SourcePosition(
                t.Position.FileId,
                originalSourceLine.OriginalLineNumber,
                t.Position.Column + baseColumn - 1);
            return new Token(t.Kind, t.Lexeme, t.Literal, newPosition);
        }).ToImmutableArray();

        var tempParser = new Parser(tokens, sourceMap, program, originalSourceLine.FilePath, registry);

        try
        {
            var expression = tempParser.ParseExpression();

            // Verifica che tutti i token (tranne Newline/EOF) siano stati consumati.
            while (tempParser.Match(TokenKind.Newline))
            {
            }

            if (tempParser.IsAtEnd() || tempParser.Peek().Kind == TokenKind.Eof) return expression;

            var unexpectedToken = tempParser.Peek();
            throw CreateError(unexpectedToken,
                $"Caratteri inattesi dopo la fine dell'espressione: '{unexpectedToken.Lexeme}'.");
        }
        catch (BasicException ex)
        {
            throw new BasicException(ex.Code, ex.Position,
                $"Errore di sintassi nell'espressione '{expressionString}': {ex.Message}", ex.SourceLocation, ex);
        }
    }

    /// <summary>
    /// Parsa "Just-In-Time" (JIT) una stringa di codice sorgente che rappresenta una singola istruzione,
    /// trasformandola in un nodo AST <see cref="Stmt"/>. Questo metodo è fondamentale per l'implementazione
    /// delle macro di comando (es. `&comando$`), dove una stringa viene compilata ed eseguita a runtime.
    /// </summary>
    /// <remarks>
    /// Il metodo orchestra un mini-ciclo di compilazione isolato, simile a `ParseSingleExpression`:
    /// 1.  Lexing Dinamico: La stringa `source` viene analizzata da un'istanza temporanea del <see cref="Lexer"/>.
    /// 2.  Correzione della Posizione: I token vengono ri-mappati per riflettere la loro posizione originale
    ///     nel file sorgente (`originalSourceLine` e `baseColumn`), garantendo un reporting degli errori accurato.
    /// 3.  Parsing: Viene creata un'istanza "usa e getta" del <see cref="Parser"/> per invocare
    ///     il dispatcher principale `ParseStatement`.
    /// 4.  Validazione di Unicità: Al termine, il metodo verifica che sia stata generata esattamente una
    ///     singola istruzione e che non ci siano token residui, garantendo che la macro contenga
    ///     un solo comando.
    /// </remarks>
    /// <param name="source">La stringa contenente la singola istruzione da parsare.</param>
    /// <param name="program">L'istanza di <see cref="BasicProgram"/> che fornisce il contesto di compilazione.</param>
    /// <param name="originalSourceLine">L'oggetto <see cref="SourceLine"/> originale da cui proviene la macro.</param>
    /// <param name="baseColumn">La colonna (1-based) all'interno della `originalSourceLine` da cui inizia la stringa `source`.</param>
    /// <returns>Il nodo AST <see cref="Stmt"/> corrispondente all'istruzione parsata.</returns>
    /// <param name="registry">Registry delle funzioni built-in.</param>
    /// <param name="verboseLexer">Se true, abilita il logging verbose del lexer.</param>
    /// <exception cref="BasicException">Lanciata se la stringa non contiene esattamente una istruzione o se c'è un errore di sintassi.</exception>
    public static Stmt ParseSingleStatement(string source, BasicProgram program, SourceLine originalSourceLine,
        int baseColumn, IParserRegistry? registry = null, bool verboseLexer = false)
    {
        var statementSourceLine = originalSourceLine with { Text = source };
        var sourceMap = ImmutableArray.Create(statementSourceLine);

        var lexer = new Lexer(sourceMap, program, verboseLexer, disableLineNumbers: true);
        var tokens = lexer.ScanTokens().Select(t =>
        {
            var newPosition =
                new SourcePosition(t.Position.FileId, originalSourceLine.OriginalLineNumber,
                    t.Position.Column + baseColumn - 1);
            return new Token(t.Kind, t.Lexeme, t.Literal, newPosition);
        }).ToImmutableArray();

        var tempParser = new Parser(tokens, sourceMap, program, originalSourceLine.FilePath, registry);

        try
        {
            var statements = tempParser.ParseStatement(null);

            if (statements.Count == 0 && tempParser.IsAtEnd())
            {
                // Caso di una stringa vuota o solo commenti
                return new RemStmt(new Token(TokenKind.Rem, "REM", "Empty macro", SourcePosition.Unknown));
            }

            if (statements.Count != 1)
            {
                throw CreateError(tokens.First(),
                    "La stringa di una macro di comando deve contenere esattamente una singola istruzione.");
            }

            var resultStmt = (Stmt)statements[0];

            while (tempParser.Match(TokenKind.Newline))
            {
            }

            if (tempParser.IsAtEnd() || tempParser.Peek().Kind == TokenKind.Eof) return resultStmt;

            var unexpectedToken = tempParser.Peek();
            throw CreateError(unexpectedToken,
                $"Caratteri inattesi dopo la fine dell'istruzione: '{unexpectedToken.Lexeme}'.");
        }
        catch (BasicException ex)
        {
            throw new BasicException(ErrorCode.SyntaxError, ex.Position,
                $"Errore di sintassi nell'istruzione macro: {ex.Message}", ex.SourceLocation, ex);
        }
    }
}