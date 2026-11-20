// FILE: Interpreter.Parser.Expressions.cs

using System.Collections.Immutable;

// ReSharper disable SwitchStatementMissingSomeEnumCasesNoDefault

// ReSharper disable RedundantArgumentDefaultValue
// ReSharper disable InvalidXmlDocComment
namespace Basic;

/*
------------------------------------------------------------------------------------------------------------------------
        GUIDA DI RIFERIMENTO A GM-BASIC (Rev. Definitiva)
        REGOLE SINTATTICHE (PARSER DELLE ESPRESSIONI)
------------------------------------------------------------------------------------------------------------------------

Questa parte del Parser è responsabile dell'analisi delle espressioni, rispettando la precedenza degli operatori
e costruendo i nodi `Expr` corrispondenti nell'AST.

1. MECCANISMO: PARSING A PRECEDENZA
====================================
- Implementazione: Il parser utilizza una tecnica classica chiamata "Pratt Parser" o "Top-Down Operator
  Precedence Parsing". Ogni livello di precedenza degli operatori è gestito da un metodo specifico.
- Flusso: L'analisi inizia dal livello di precedenza più basso (`ParseLogicalOr`) e risale ricorsivamente
  fino al più alto (`ParsePrimary`), che gestisce i letterali, le variabili e le parentesi.
- Associatività: L'associatività a destra per l'operatore di potenza (^) viene gestita tramite una
  chiamata ricorsiva diretta nel metodo `ParsePower`. Tutti gli altri operatori binari sono associativi a sinistra.

2. ORDINE DI PRECEDENZA DEGLI OPERATORI (dal più alto al più basso)
===================================================================
  1. `()` `[]` `{}`                  (Raggruppamento, Accesso ad array/indici, Letterali array)
  2. `.`                            (Accesso a campi di UDT/JSON)
  3. `^`                            (Esponenziazione)
  4. `-` `NOT`                      (Operatori Unari)
  5. `*` `/` `\` `MOD`              (Moltiplicazione, Divisione, Modulo)
  6. `+` `-`                        (Addizione, Sottrazione, Concatenazione stringhe)
  7. `=` `<>` `<` `>` `<=` `>=`      (Operatori di Comparazione)
  8. `AND`                          (Logico AND)
  9. `OR`                           (Logico OR)

3. GESTIONE DELLE ESPRESSIONI PRIMARIE (`ParsePrimary`)
=========================================================
Questo è il metodo fondamentale che riconosce gli "atomi" di un'espressione.

- Letterali: Riconosce numeri (inclusi 0x, 0o, 0b), stringhe ("..."), booleani (TRUE, FALSE) e
  array ({...}).
- Variabili: Un identificatore semplice viene parsato come `VariableExpr`.
- Raggruppamento: Un'espressione tra parentesi `()` viene parsata come `GroupingExpr`.
- Chiamate a Funzione e Array: Gestisce la sintassi ambigua `Nome(...)`:
  - Funzioni Built-in: Una lista statica di nomi di funzioni predefinite (`IsFunction`) viene usata per
    identificare e parsare immediatamente un `FunctionExpr`.
  - Funzioni Utente vs. Array: Per un identificatore non predefinito, consulta la tabella `_functionNames`
    (popolata dalla pre-scansione). Se il nome è presente, genera un `FunctionExpr`; altrimenti, genera
    un `ArrayAccessExpr`. Questo risolve l'ambiguità a livello di parsing.
- Accesso a Indici/Campi: Un ciclo `while` alla fine di `ParsePrimary` gestisce accessi a catena, come
  `MiaVar(i).Campo.AltroCampo(j)`, costruendo l'AST gerarchico corretto.
*/
/// <summary>
///     Contiene i metodi per il parsing di espressioni, gestendo la precedenza degli operatori.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// [FASE 1] Inizia il parsing di un'espressione, partendo dal livello di precedenza più basso (OR).
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/> che rappresenta l'intera espressione parsata.</returns>
    private Expr ParseExpression()
    {
        Log($"[ParseExpression] ENTER. Current: {Peek()}");
        // L'assegnazione inline (:=) ha la precedenza più bassa di tutte le espressioni.
        var expr = ParseAssignment();
        Log($"[ParseExpression] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// Metodo helper specializzato e sicuro per il parsing di un'espressione opzionale.
    /// Questo metodo è il cuore della correzione del bug di desincronizzazione.
    /// Prima di tentare di parsare un'espressione, controlla esplicitamente che il token
    /// corrente non sia un terminatore di istruzione (Newline, Colon) o la fine del file.
    /// Questo previene il "consumo eccessivo" di token che causava il fallimento del parser
    /// all'interno di blocchi come SUB...END SUB.
    /// </summary>
    /// <returns>Un nodo Expr se un'espressione valida viene trovata, altrimenti null.</returns>
    private Expr? ParseOptionalExpression()
    {
        // Controlla se siamo alla fine dell'istruzione logica.
        // Se sì, non c'è nessuna espressione opzionale da parsare.
        //if (Check(TokenKind.Newline) || Check(TokenKind.Colon) || IsAtEnd())
        // Controlla se siamo alla fine dell'istruzione logica o prima di una clausola ELSE.
        // Se sì, non c'è nessuna espressione opzionale da parsare. Restituisce null
        // SENZA consumare alcun token, risolvendo il bug di desincronizzazione.
        if (Check(TokenKind.Newline) || Check(TokenKind.Colon) || Check(TokenKind.Else) || IsAtEnd())
        {
            return null;
        }

        // Se non siamo alla fine, allora è sicuro chiamare ParseExpression().
        return ParseExpression();
    }

    /// <summary>
    /// [FASE 1] Micro-parser per una singola espressione. Utilizzato anche per il parsing SQL.
    /// Isola i token di un'espressione fermandosi ai delimitatori di alto livello
    /// e poi usa il parser di espressioni di Fase 1 su quel sottoinsieme.
    /// </summary>
    private Expr ParseExpressionFragment(params TokenKind[] stopTokens)
    {
        var expressionTokens = new List<Token>();
        var parenDepth = 0;

        while (!IsAtEnd())
        {
            var currentToken = Peek();
            // Aggiunge Newline e Colon come terminatori universali, a meno che non siamo dentro parentesi.
            if (parenDepth == 0 && currentToken.Kind is TokenKind.Newline or TokenKind.Colon)
            {
                break;
            }

            // A livello principale (non dentro parentesi), i token di stop specifici terminano l'espressione.
            if (parenDepth == 0 && stopTokens.Contains(currentToken.Kind))
            {
                break;
            }

            switch (currentToken.Kind)
            {
                case TokenKind.LeftParen:
                    parenDepth++;
                    break;
                case TokenKind.RightParen:
                    parenDepth--;
                    break;
            }

            expressionTokens.Add(currentToken);
            Advance();
        }

        if (expressionTokens.Count == 0)
        {
            throw CreateError(Peek(), "Expected an expression.");
        }

        var eofToken = new Token(TokenKind.Eof, "", null, Peek().Position);
        expressionTokens.Add(eofToken);

        var tempParser = new Parser(
            [..expressionTokens],
            _sourceMap,
            _program,
            _entryFilePath ?? "JIT_Fragment",
            _registry);

        // Chiama la versione di Fase 1 del parser di espressioni.
        return tempParser.ParseExpression();
    }

// FILE: Interpreter.Parser.Expressions.cs

    /// <summary>
    /// [FASE 1 - CORRETTO v5] Parsa una lista di espressioni separate da virgola fino a un token di chiusura.
    /// Gestisce correttamente i newline, la virgola finale opzionale e i modificatori di passaggio 
    /// parametri (BYREF/BYVAL) per le chiamate a funzione e metodo (TODO #41).
    /// </summary>
    /// <param name="endToken">Il token che termina la lista (es. RightParen, RightBrace).</param>
    /// <returns>Un array immutabile di <see cref="Expr"/>.</returns>
    private ImmutableArray<Expr> ParseExpressionList(TokenKind endToken)
    {
        var expressions = ImmutableArray.CreateBuilder<Expr>();

        // Salta eventuali newline iniziali
        while (Match(TokenKind.Newline))
        {
        }

        if (Check(endToken))
        {
            return expressions.ToImmutable();
        }

        do
        {
            // Salta newline prima di un elemento
            while (Match(TokenKind.Newline))
            {
            }

            // Se dopo aver saltato i newline e le virgole troviamo il token di chiusura,
            // significa che c'era una virgola finale.
            if (Check(endToken))
            {
                break;
            }

            // --- GESTIONE MODIFICATORI DI CHIAMATA ---
            // Consuma BYREF o BYVAL se presenti prima dell'espressione dell'argomento.
            // L'informazione semantica sul passaggio dei parametri è già contenuta nella 
            // firma della procedura nella SymbolTable; il parser qui si limita a permettere 
            // la sintassi senza lanciare errori di "espressione attesa".
            Match(TokenKind.ByRef, TokenKind.ByVal);

            expressions.Add(ParseExpression());

            // Salta newline dopo un elemento
            while (Match(TokenKind.Newline))
            {
            }
        } while (Match(TokenKind.Comma));

        return expressions.ToImmutable();
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni logiche con operatore OR (precedenza più bassa).
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParseLogicalOr()
    {
        Log($"[ParseLogicalOr] ENTER. Current: {Peek()}");
        var expr = ParseLogicalXor();
        while (Match(TokenKind.Or, TokenKind.BitOr))
        {
            var op = Previous();
            Log("[ParseLogicalOr]   -> Matched OR. Consuming right operand...");
            var right = ParseLogicalXor();
            expr = new BinaryExpr(expr, op, right);
        }

        Log($"[ParseLogicalOr] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// [XOR] Parsa espressioni bitwise con operatore XOR (precedenza: OR &lt; XOR &lt; AND).
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParseLogicalXor()
    {
        var expr = ParseLogicalAnd();
        while (Match(TokenKind.Xor, TokenKind.BitXor))
        {
            var op = Previous();
            var right = ParseLogicalAnd();
            expr = new BinaryExpr(expr, op, right);
        }
        return expr;
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni logiche con operatore AND.
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParseLogicalAnd()
    {
        Log($"[ParseLogicalAnd] ENTER. Current: {Peek()}");
        var expr = ParseComparison();
        while (Match(TokenKind.And, TokenKind.BitAnd))
        {
            var op = Previous();
            Log("[ParseLogicalAnd]   -> Matched AND. Consuming right operand...");
            var right = ParseComparison();
            expr = new BinaryExpr(expr, op, right);
        }

        Log($"[ParseLogicalAnd] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// Determina se un TokenKind può rappresentare l'inizio di un tipo di dato (primitivo o complesso).
    /// </summary>
    private static bool IsTypeStartToken(TokenKind kind)
    {
        return kind switch
        {
            TokenKind.StringType or TokenKind.Integer or TokenKind.Long or TokenKind.Double or
                TokenKind.Bit or TokenKind.Byte or TokenKind.SByte or TokenKind.Single or
                TokenKind.Currency or TokenKind.Quad or TokenKind.Date or TokenKind.Time or
                TokenKind.DateTime or TokenKind.Any or TokenKind.Json or
                TokenKind.Array or TokenKind.List or TokenKind.Dict or TokenKind.HashSet or
                TokenKind.Queue or TokenKind.Stack or TokenKind.Sorted or
                TokenKind.Identifier or TokenKind.Nil => true,
            _ => false
        };
    }

// FILE: Interpreter.Parser.Expressions.cs

    /// <summary>
    /// [FASE 1] Parsing delle espressioni di comparazione.
    /// VERSIONE AGGIORNATA: Gestisce l'operatore 'IS' sia per i controlli di identità (es. IS NIL)
    /// che per il controllo del tipo a runtime (es. IS STRING, IS NomeUdt) (TODO #84).
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/> rappresentante l'operazione binaria di confronto.</returns>
    private Expr ParseComparison(bool remapFatArrow = true)
    {
        Log($"[ParseComparison] ENTER. Current: {Peek()}, remapFatArrow={remapFatArrow}");

        var expr = ParseTerm();

        while (true)
        {
            Token? opToken = null;
            var k0 = Peek().Kind;
            var k1 = At(1).Kind;

            switch (k0)
            {
                // Gestione degli operatori composti da due token che il Lexer NON fonde
                case TokenKind.Equal when k1 == TokenKind.LessThan:
                {
                    var first = Advance();
                    Advance();
                    opToken = new Token(TokenKind.LessEqual, "=<", null, first.Position);
                    Log("[ParseComparison]   -> Fused operator '=<' to LessEqual.");
                    break;
                }
                case TokenKind.GreaterThan when k1 == TokenKind.LessThan:
                {
                    var first = Advance();
                    Advance();
                    opToken = new Token(TokenKind.NotEqual, "><", null, first.Position);
                    Log("[ParseComparison]   -> Fused operator '><' to NotEqual.");
                    break;
                }
            }

            // Se non è stato trovato un operatore multi-token, prova con quelli a singolo token
            if (opToken == null)
            {
                if (Match(TokenKind.Equal, TokenKind.NotEqual, TokenKind.LessThan,
                        TokenKind.LessEqual, TokenKind.GreaterThan, TokenKind.GreaterEqual,
                        TokenKind.ExactlyEqual, TokenKind.Is, TokenKind.In))
                {
                    opToken = Previous();
                    Log($"[ParseComparison]   -> Matched single operator '{opToken.Lexeme}'.");
                }
                else if (remapFatArrow && Match(TokenKind.FatArrow))
                {
                    var fatArrowToken = Previous();
                    opToken = new Token(TokenKind.GreaterEqual, fatArrowToken.Lexeme, fatArrowToken.Literal,
                        fatArrowToken.Position);
                    Log("[ParseComparison]   -> Remapped '=>' (FatArrow) to GreaterEqual for comparison.");
                }
            }

            if (opToken == null)
                break;

            Log("[ParseComparison]   -> Consuming right operand...");

            Expr right;
            // Se l'operatore è 'IS' e segue un tipo di dato, parsa come TypeExpr.
            // Se segue NIL o un'espressione, procede con il parsing standard dei termini.
            if (opToken.Kind == TokenKind.Is && IsTypeStartToken(Peek().Kind))
            {
                Log($"[ParseComparison]   -> Rilevato controllo di tipo con 'IS'. Invoco ParseTypeName.");
                var type = ParseTypeName();
                right = new TypeExpr(type);
            }
            else
            {
                right = ParseTerm();
            }

            // Applica lo Scope Stamping manuale per garantire il corretto shadowing.
            expr = new BinaryExpr(expr, opToken, right) { ScopeId = _program.Symbols.GetCurrentScope().Id };
        }

        Log($"[ParseComparison] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni di addizione, sottrazione e concatenazione di stringhe.
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParseTerm()
    {
        Log($"[ParseTerm] ENTER. Current: {Peek()}");
        var expr = ParseFactor();
        while (true)
        {
            if (Match(TokenKind.Plus, TokenKind.Minus))
            {
                var op = Previous();
                Log($"[ParseTerm]   -> Matched '{op.Lexeme}'. Consuming right operand...");
                var right = ParseFactor();
                expr = new BinaryExpr(expr, op, right);
            }
            // `&` binario = CONCATENAZIONE di stringhe (idioma BASIC comune). Riusa il path di
            // `+` (stringa) rimappando il token su Plus, così la semantica resta una sola su
            // tutti i backend. Prima veniva IGNORATO in silenzio (restava il solo operando
            // sinistro) → `PRINT "a" & x` stampava solo "a". Un `&` in testa (macro) resta
            // gestito da ParseUnary/ParseTarget.
            else if (Match(TokenKind.Ampersand))
            {
                var op = new Token(TokenKind.Plus, "+", null, Previous().Position);
                Log("[ParseTerm]   -> Matched '&' (concat). Consuming right operand...");
                var right = ParseFactor();
                expr = new BinaryExpr(expr, op, right);
            }
            else if (IsStringExpression(expr) && Check(TokenKind.StringLiteral))
            {
                var op = new Token(TokenKind.Plus, "+", null, Previous().Position);
                Log("[ParseTerm]   -> Implicit string concatenation. Consuming right operand...");
                var right = ParseFactor();
                expr = new BinaryExpr(expr, op, right);
            }
            else
            {
                break;
            }
        }

        Log($"[ParseTerm] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni di moltiplicazione, divisione (float e intera) e modulo.
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParseFactor()
    {
        Log($"[ParseFactor] ENTER. Current: {Peek()}");
        var expr = ParsePower();
        while (Match(TokenKind.Multiply, TokenKind.Divide, TokenKind.IntegerDivide, TokenKind.Mod))
        {
            var op = Previous();
            Log($"[ParseFactor]   -> Matched '{op.Lexeme}'. Consuming right operand...");
            var right = ParsePower();
            expr = new BinaryExpr(expr, op, right);
        }

        Log($"[ParseFactor] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni di esponenziazione (^), che è associativa a destra.
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParsePower()
    {
        Log($"[ParsePower] ENTER. Current: {Peek()}");
        var expr = ParseUnary();
        while (Match(TokenKind.Power))
        {
            var op = Previous();
            Log("[ParsePower]   -> Matched '^'. Consuming right operand recursively...");
            // Chiamata ricorsiva a se stesso per l'associatività a destra.
            var right = ParsePower();
            expr = new BinaryExpr(expr, op, right);
        }

        Log($"[ParsePower] EXIT. Parsed: {expr.GetType().Name}. Next: {Peek()}");
        return expr;
    }

    /// <summary>
    /// [FASE 1] Parsa espressioni unarie (NOT, -, &, +).
    /// Questa versione è stata corretta per gestire i letterali numerici negativi (es. -5)
    /// come un singolo `NumberExpr` invece di un `UnaryExpr` complesso e per risolvere
    /// ambiguità con nomi qualificati.
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/>.</returns>
    private Expr ParseUnary()
    {
        Log($"[ParseUnary] ENTER. Current: {Peek()}");

        if (Match(TokenKind.PlusPlus, TokenKind.MinusMinus))
        {
            var op = Previous();
            // Dopo un operatore prefisso, ci aspettiamo un target valido (L-value).
            var target = ParseTarget();
            var result = new PrefixIncrementExpr(op, target);
            Log($"[ParseUnary] EXIT. Parsed: PrefixIncrementExpr. Next: {Peek()}");
            return result;
        }

        if (Match(TokenKind.Minus))
        {
            var minusToken = Previous();
            if (Check(TokenKind.Number))
            {
                var numberToken = Advance();
                var negativeValue = -(double)numberToken.Literal!;
                var combinedToken = new Token(TokenKind.Number, $"-{numberToken.Lexeme}", negativeValue,
                    minusToken.Position);
                var result = new NumberExpr(negativeValue, combinedToken);
                Log($"[ParseUnary] EXIT. Parsed negative literal: {result.Value}. Next: {Peek()}");
                return result;
            }

            Log("[ParseUnary]   -> Matched '-'. Consuming right operand recursively...");
            // Chiamata ricorsiva a se stesso per gestire --x o - NOT x.
            var right = ParseUnary();
            var unaryResult = new UnaryExpr(minusToken, right);
            Log($"[ParseUnary] EXIT. Parsed: UnaryExpr. Next: {Peek()}");
            return unaryResult;
        }

        if (Match(TokenKind.Ampersand))
        {
            var macroToken = Previous();
            Log("[ParseUnary]   -> Matched '&'. Consuming macro expression...");
            var expressionToEvaluate = ParsePrimary();
            // Macro COSTANTE (&("...")): compilata a compile-time come Clipper (vedi
            // TryExpandConstantMacro) — i code block diventano lambda tipizzate.
            var expandedMacro = TryExpandConstantMacro(expressionToEvaluate, this);
            if (expandedMacro != null)
            {
                Log($"[ParseUnary] EXIT. Macro costante espansa: {expandedMacro.GetType().Name}. Next: {Peek()}");
                return expandedMacro;
            }
            var result = new MacroExpr(expressionToEvaluate, macroToken);
            Log($"[ParseUnary] EXIT. Parsed: MacroExpr. Next: {Peek()}");
            return result;
        }

        // EVAL(expr [, args...]): dispatch per TIPO del primo argomento (Clipper 5.3 + GMBasic).
        //   EVAL(<stringa>)          → macro-evaluator, come &expr (MacroExpr)
        //   EVAL(<codeblock>, args…) → invocazione del block con gli argomenti (Clipper)
        // Il parser non conosce i tipi: produce SEMPRE EvalExpr; il Linker risolve staticamente
        // i casi noti (stringa → MacroExpr, function pointer → FunctionExpr indiretto) e lascia
        // EvalExpr ai casi dinamici (target Any/Inferred), che i backend dispatchano a runtime.
        if (Match(TokenKind.Eval))
        {
            var evalToken = Previous();
            Consume(TokenKind.LeftParen, "Attesa '(' dopo EVAL.");
            var evalTarget = ParseExpression();
            var evalArgs = ImmutableArray.CreateBuilder<Expr>();
            while (Match(TokenKind.Comma))
                evalArgs.Add(ParseExpression());
            Consume(TokenKind.RightParen, "Attesa ')' dopo l'espressione di EVAL.");
            // Macro di stringa COSTANTE senza argomenti: compilata a compile-time (Clipper).
            if (evalArgs.Count == 0)
            {
                var expandedEval = TryExpandConstantMacro(evalTarget, this);
                if (expandedEval != null)
                {
                    Log($"[ParseUnary] EXIT. EVAL costante espanso: {expandedEval.GetType().Name}. Next: {Peek()}");
                    return expandedEval;
                }
            }
            return new EvalExpr(evalTarget, evalArgs.ToImmutable(), evalToken);
        }

        if (Match(TokenKind.Checked))
        {
            var checkedToken = Previous();
            if (!Check(TokenKind.StringLiteral))
            {
                throw CreateError(Peek(),
                    "CHECKED richiede un letterale stringa, es. CHECKED \"Saldo > 0\".");
            }

            var stringToken = Advance();
            var stringValue = (string)stringToken.Literal!;
            return new CheckedStringExpr(stringValue, checkedToken, stringToken);
        }

        if (Match(TokenKind.Not, TokenKind.BitNot, TokenKind.Plus))
        {
            var op = Previous();
            Log($"[ParseUnary]   -> Matched '{op.Lexeme}'. Consuming right operand recursively...");
            //La chiamata ricorsiva è corretta per gestire 'NOT NOT', 'NOT -5', etc.
            var right = ParseUnary();
            if (op.Kind == TokenKind.Plus)
            {
                Log($"[ParseUnary] EXIT (Unary Plus ignored). Parsed: {right.GetType().Name}. Next: {Peek()}");
                return right;
            }

            var result = new UnaryExpr(op, right);
            Log($"[ParseUnary] EXIT. Parsed: UnaryExpr. Next: {Peek()}");
            return result;
        }

        Log("[ParseUnary]   -> No unary operator. Delegating to ParsePower.");
        // Chiama il livello di precedenza successivo.
        return ParsePrimary();
    }

    /// <summary>
    /// [FASE 1] Parsa un "atomo" di espressione primario.
    /// Questa versione è stata resa robusta per gestire correttamente i delimitatori
    /// di letterali ({, [) anche in presenza di token malformati, e ripristina
    /// la corretta gerarchia di parsing.
    /// </summary>
    private Expr ParsePrimaryAtom()
    {
        const bool log = false;
        Log($"[ParsePrimaryAtom] ENTER. Current: {Peek()}", log);

        var currentToken = Peek();

        switch (currentToken.Kind)
        {
            // --- GESTIONE DEI DELIMITATORI DI BLOCCO (Priorità Massima) ---
            // Controlliamo esplicitamente il Kind prima di chiamare Match.
            case TokenKind.LeftParen:
            {
                var openParen = Advance(); // Consuma '('
                Log("[ParsePrimaryAtom]   -> Path: '(' detected. Parsing first expression...", log);

                // Controlla se è un tuple literal con primo campo nominato: (nome: expr, ...)
                // Prima di parsare l'espressione, guarda il pattern: Identifier ':' → campo nominato
                NamedTupleElement? firstNamedElement = null;
                if (Check(TokenKind.Identifier) && At(1).Kind == TokenKind.Colon && At(2).Kind != TokenKind.Colon)
                {
                    // Pattern (nome: ...) rilevato: è un tuple con campo nominato
                    var nameToken = Peek();
                    Advance(); // consuma nome
                    Advance(); // consuma ':'
                    var firstValue = ParseExpression();
                    firstNamedElement = new NamedTupleElement(firstValue, nameToken);
                }

                var firstExpr = firstNamedElement?.Value ?? ParseExpression();

                // Controlla se è un generatore lazy: (expr FOR var IN source [IF cond])
                if (firstNamedElement is null && Match(TokenKind.For))
                {
                    var clauses = ParseComprehensionClauses(isFirstForAlreadyConsumed: true);
                    Consume(TokenKind.RightParen, "Expected ')' after generator expression.");
                    var genExpr = new GeneratorExpr(firstExpr, clauses, openParen);
                    Log($"[ParsePrimaryAtom] EXIT. Parsed GeneratorExpr with {clauses.Length} clauses. Next: {Peek()}", log);
                    return genExpr;
                }

                // Controlla se è un tuple literal: (expr, expr, ...) o (nome: expr, ...)
                // Un tuple literal ha almeno 2 elementi separati da virgola
                // e NON è un tipo tuple (che avrebbe AS tipo dopo ogni elemento)
                if (Check(TokenKind.Comma) && !IsTupleTypeContext())
                {
                    Log("[ParsePrimaryAtom]   -> Path: Tuple literal detected.", log);
                    var elements = ImmutableArray.CreateBuilder<NamedTupleElement>();
                    elements.Add(firstNamedElement ?? new NamedTupleElement(firstExpr, null));
                    while (Match(TokenKind.Comma))
                    {
                        var elemExpr = ParseNamedTupleElementExpression();
                        elements.Add(elemExpr);
                    }
                    var closeParen = Consume(TokenKind.RightParen, "Expected ')' after tuple elements.");
                    var tupleExpr = new TupleLiteralExpr(elements.ToImmutable(), openParen, closeParen);
                    Log($"[ParsePrimaryAtom] EXIT. Parsed TupleLiteralExpr with {elements.Count} elements. Next: {Peek()}", log);
                    return tupleExpr;
                }

                // Altrimenti è un raggruppamento normale
                Consume(TokenKind.RightParen, "Expected ')' after expression.");
                Log($"[ParsePrimaryAtom] EXIT. Parsed GroupingExpr. Next: {Peek()}", log);
                return new GroupingExpr(firstExpr);
            }
            case TokenKind.LeftBrace:
            {
                Advance(); // Consuma '{'
                Log(
                    "[ParsePrimaryAtom]   -> Path: Brace-Enclosed literal '{}'. Chiamo ParseBraceEnclosedLiteralBody...",
                    log);
                var expr = ParseBraceEnclosedLiteralBody(Previous());
                Log($"[ParsePrimaryAtom] EXIT. Parsed {expr.GetType().Name}. Next: {Peek()}", log);
                return expr;
            }
            case TokenKind.LeftBracket:
            {
                Advance(); // Consuma '['
                Log(
                    "[ParsePrimaryAtom]   -> Path: Bracket-Enclosed literal '[]'. Chiamo ParseListOrComprehension...",
                    log);
                var expr = ParseListOrComprehension(Previous());
                Log($"[ParsePrimaryAtom] EXIT. Parsed {expr.GetType().Name}. Next: {Peek()}", log);
                return expr;
            }
        }

        // --- GESTIONE LEADING DOT (WITH block implicit member access) ---
        if (Check(TokenKind.Dot) && At(1).Kind == TokenKind.Identifier)
        {
            var dotToken = Advance(); // Consuma '.'
            var memberToken = Advance(); // Consuma l'identificatore

            // Controlla se e' una chiamata metodo: .Method(args)
            ImmutableArray<Expr>? args = null;
            if (Match(TokenKind.LeftParen))
            {
                var argList = ImmutableArray.CreateBuilder<Expr>();
                if (!Check(TokenKind.RightParen))
                {
                    do
                    {
                        argList.Add(ParseExpression());
                    } while (Match(TokenKind.Comma));
                }
                Consume(TokenKind.RightParen, "Expected ')' after method arguments.");
                args = argList.ToImmutable();
            }

            Log($"[ParsePrimaryAtom] EXIT. Parsed WithMemberAccessExpr '.{memberToken.Lexeme}'. Next: {Peek()}", log);
            return new WithMemberAccessExpr(memberToken, dotToken, args);
        }

        // --- GESTIONE AWAIT ---
        if (Check(TokenKind.Await))
        {
            var awaitToken = Advance(); // Consuma AWAIT
            var expression = ParseExpression();
            Log($"[ParsePrimaryAtom] EXIT. Parsed AwaitExpr. Next: {Peek()}", log);
            return new AwaitExpr(expression, awaitToken);
        }

        // --- GESTIONE DIRECTCAST ---
        if (Check(TokenKind.DirectCast))
        {
            var directCastToken = Advance(); // Consuma DIRECTCAST
            Consume(TokenKind.LeftParen, "Expected '(' after DIRECTCAST.");
            var expression = ParseExpression();
            Consume(TokenKind.Comma, "Expected ',' after expression in DIRECTCAST.");
            var targetType = ParseTypeName();
            Consume(TokenKind.RightParen, "Expected ')' after type in DIRECTCAST.");
            Log($"[ParsePrimaryAtom] EXIT. Parsed DirectCastExpr. Next: {Peek()}", log);
            return new DirectCastExpr(expression, targetType, directCastToken);
        }

        // --- GESTIONE LETTERALI SEMPLICI E KEYWORD ---
        if (ParseLiteralOrGrouping() is { } literalExpr)
        {
            Log($"[ParsePrimaryAtom] EXIT. Parsed Literal '{literalExpr}'. Next: {Peek()}", log);
            return literalExpr;
        }

        if (ParseKeywordExpression() is { } keywordExpr)
        {
            Log($"[ParsePrimaryAtom] EXIT. Parsed Keyword Expression '{keywordExpr}'. Next: {Peek()}", log);
            return keywordExpr;
        }


        // --- INKEY / INKEY$ come FUNZIONE (espressione): `k$ = INKEY`, `PRINT INKEY$` ---
        // Il comando (`INKEY k$`) è gestito dal dispatcher statement; qui è la forma
        // funzione Clipper (0 argomenti). INKEY$ è un TokenKind dedicato: il lexer
        // legge "INKEY$" come un unico token (il '$' fa parte di IsAlphaNumeric).
        if (currentToken.Kind is TokenKind.Inkey or TokenKind.InkeyDollar)
        {
            var fnToken = Advance();
            ImmutableArray<Expr> args = [];
            if (Check(TokenKind.LeftParen))
                args = ParseArgumentListInParentheses();
            var nsToken = new Token(TokenKind.Identifier, "CONSOLE", null, fnToken.Position);
            return new BuiltinFunctionCallExpr(nsToken, fnToken, args, new StringType());
        }

        // --- SCREEN come FUNZIONE (espressione): `IF SCREEN <> 1`, `x = SCREEN()` ---
        // Sintassi Clipper (senza parentesi) e con parentesi: restituisce la modalità
        // grafica corrente. Senza questo il parser trattava SCREEN come costante numerica
        // (194 = valore del TokenKind) e `IF SCREEN <> 1` terminava il campione.
        if (currentToken.Kind == TokenKind.Screen && At(1).Kind is not (TokenKind.Dot or TokenKind.Equal or TokenKind.LeftParen or TokenKind.Comma or TokenKind.As))
        {
            var fnToken = Advance();
            ImmutableArray<Expr> args = [];
            if (Check(TokenKind.LeftParen))
                args = ParseArgumentListInParentheses();
            var nsToken = new Token(TokenKind.Identifier, "GRAPHICS", null, fnToken.Position);
            return new BuiltinFunctionCallExpr(nsToken, fnToken, args, new DoubleType());
        }

        if (currentToken.Kind == TokenKind.Switch)
        {
            Advance(); // Consuma 'SWITCH'
            Log("[ParsePrimaryAtom]   -> Path: SWITCH expression. Chiamo ParseSwitchExpression...", log);
            var expr = ParseSwitchExpression(Previous());
            Log($"[ParsePrimaryAtom] EXIT. Parsed SwitchExpr. Next: {Peek()}", log);
            return expr;
        }

        // FUNCTION è lambda SOLO quando inizia davvero una firma/corpo: `FUNCTION(`, `=>`,
        // `AS`, `{`, `:`. Altrimenti è una VARIABILE omonima (parità col parser C): `PRINT
        // FUNCTION`, `x = FUNCTION + 1`. Senza questo guard, `FUNCTION` nudo in espressione
        // finiva nel parser lambda → "Expected ':', '{', or '=>' after lambda signature".
        if (currentToken.Kind == TokenKind.Function
            && At(1).Kind is TokenKind.LeftParen or TokenKind.FatArrow or TokenKind.As
                or TokenKind.LeftBrace or TokenKind.Colon)
        {
            Advance(); // Consuma 'FUNCTION'
            Log("[ParsePrimaryAtom]   -> Path: Lambda/Function. Chiamo ParseLambdaExpression...", log);
            var expr = ParseLambdaExpression(Previous());
            Log($"[ParsePrimaryAtom] EXIT. Parsed LambdaExpr. Next: {Peek()}", log);
            return expr;
        }

        // --- GESTIONE NOMI QUALIFICATI E IDENTIFICATORI ---
        // Permissivo: anche le keyword strutturali sono nomi di variabile in posizione di
        // operando (parità col parser C). Il ramo namespace qui sotto non scatta per una
        // keyword (nessun namespace ha quel lexeme) → VariableExpr.
        if (IsPermissiveNameToken(currentToken))
        {
            // Lookahead per namespace: se è un namespace conosciuto seguito da '.',
            // lo trattiamo come un nome qualificato completo.
            if (SymbolTable.BuiltinNamespaceNames.Contains(currentToken.Lexeme) && At(1).Kind == TokenKind.Dot)
            {
                Log(
                    $"[ParsePrimaryAtom]   -> Path: Namespace '{currentToken.Lexeme}' detected. Parsing as QualifiedNameExpr.",
                    log);
                var qualifiedNameExpr = ParseQualifiedNameExpression();
                Log($"[ParsePrimaryAtom] EXIT. Parsed QualifiedNameExpr '{qualifiedNameExpr}'. Next: {Peek()}",
                    log);
                return qualifiedNameExpr;
            }

            // Altrimenti, è un semplice identificatore di variabile.
            Advance(); // Consuma l'identificatore (Originale/Suffisso)

            // --- NORMALIZZAZIONE FASE 1 ---
            var originalToken = currentToken;
            var (baseName, _) = SplitNameAndSuffix(originalToken.Lexeme);
            var cleanToken = originalToken with { Lexeme = baseName };
            // ------------------------------

            // Crea il nodo passando ENTRAMBI i token.
            var expr = new VariableExpr(-1, originalToken, cleanToken);

            // Una keyword che è una FUNZIONE builtin a 0 ARGOMENTI usata SENZA parentesi
            // (`PRINT STRTIME`, `x = STRDATE`, `t = TIMER`) è una CHIAMATA, non una variabile:
            // il dispatch è per TokenKind (0 argomenti compatibili), esattamente come per la
            // forma con parentesi. Prima il nodo restava un VariableExpr e solo IL/VM-C lo
            // recuperavano a valle: su PCode `PRINT STRTIME` stampava 0 (bug 2026-11-01).
            // Esclusi i nomi con una VARIABILE dichiarata omonima (shadowing per scope) e il
            // nome della procedura corrente: `PRINT Sum` dentro `FUNCTION Sum()` legge lo
            // slot del valore di ritorno, non chiama il builtin SUM() a zero argomenti.
            var isCurrentProcName = _functionContextStack.Count > 0
                && _functionContextStack.Peek().Lexeme.Equals(originalToken.Lexeme, StringComparison.OrdinalIgnoreCase);
            if (originalToken.Kind != TokenKind.Identifier
                && Peek().Kind != TokenKind.LeftParen     // `STRTIME()` lo gestisce il postfix
                && !_declaredVariableNames.Contains(originalToken.Lexeme)
                // Un PARAMETRO omonimo shadowa la keyword a 0 argomenti come una variabile DIM:
                // `FUNCTION F(message AS STRING)` + `PRINT message` legge il parametro, non chiama
                // MESSAGE() (bug 2026-11-06: il parametro `message` risultava "" su IL/PCode/VM-C;
                // C99Native ha un parser proprio e non era affetto). Insieme separato da
                // _declaredVariableNames di proposito: quello devierebbe anche le chiamate con
                // parentesi (`LEN(x)` con un parametro `len`).
                && !_declaredParameterNames.Contains(originalToken.Lexeme)
                && !isCurrentProcName)
            {
                if (ResolveBuiltinFunctionCall(originalToken, ImmutableArray<Expr>.Empty) is { } zeroArgCall)
                {
                    Log($"[ParsePrimaryAtom] EXIT. Keyword a 0 argomenti risolta come chiamata: '{zeroArgCall}'.", log);
                    return zeroArgCall;
                }
            }

            Log($"[ParsePrimaryAtom] EXIT. Parsed Identifier as VariableExpr ('{expr}'). Next: {Peek()}", log);
            return expr;
        }

        throw CreateError(currentToken, "Expected an expression atom.");
    }

    /// <summary>
    /// Parsa una sequenza di identificatori o keyword separati da punti (es. Namespace.Member).
    /// Restituisce un QualifiedNameExpr che rappresenta il nome qualificato.
    /// Questa versione è flessibile e accetta qualsiasi token valido come parte del nome,
    /// lasciando la validazione semantica ai metodi chiamanti (es. DispatchRegisteredCommand).
    /// </summary>
    /// <returns>Un nodo AST <see cref="QualifiedNameExpr"/> che rappresenta il nome qualificato.</returns>
    private QualifiedNameExpr ParseQualifiedNameExpression()
    {
        Log($"[ParseQualifiedNameExpression] ENTER. Current Token: {Peek()}");
        var partsBuilder = ImmutableArray.CreateBuilder<Token>();

        var firstToken = Peek();

        // Un nome qualificato deve iniziare con qualcosa che assomigli a una parola.
        if (firstToken.Kind is TokenKind.LeftParen or TokenKind.RightParen or TokenKind.Comma or TokenKind.Equal)
        {
            throw CreateError(firstToken,
                "Expected an identifier or a namespace keyword at the start of a qualified name.");
        }

        Advance(); // Consuma il primo token.
        partsBuilder.Add(firstToken);

        while (Check(TokenKind.Dot))
        {
            Advance(); // Consuma il '.'
            var memberToken = Peek();

            // Un membro non può essere un delimitatore.
            if (memberToken.Kind is TokenKind.LeftParen or TokenKind.RightParen or TokenKind.Comma or TokenKind.Equal
                or TokenKind.Dot or TokenKind.Newline or TokenKind.Eof)
            {
                throw CreateError(Previous(), "Expected a member name after '.'.");
            }

            Advance(); // Consuma il token del membro.
            partsBuilder.Add(memberToken);
        }

        var qualifiedName = new QualifiedNameExpr(partsBuilder.ToImmutable());
        Log($"[ParseQualifiedNameExpression] EXIT. Parsed name '{qualifiedName.FullName}'. Next: {Peek()}");
        return qualifiedName;
    }

    /// <summary>
    // [FASE 1 - PARSER DEDICATO RIPRISTINATO] Parsa la sintassi speciale della funzione JSON.ToObject.
    // Viene chiamato DOPO che la parentesi aperta '(' è già stata consumata.
    /// </summary>
    private JsonToObjectExpr ParseJsonToObjectCall(Token toObjectToken)
    {
        // Argomento 1: Il Tipo di Dato
        var targetDataType = ParseTypeName();

        Consume(TokenKind.Comma, "Expected ',' after the type argument in JSON.ToObject.");

        // Argomento 2: L'Espressione Sorgente
        var sourceExpression = ParseExpression();

        // Il chiamante (ParsePrimary) consumerà la parentesi chiusa ')'.
        return new JsonToObjectExpr(targetDataType, sourceExpression, toObjectToken);
    }

    /// <summary>
    /// [FASE 1] Parsa le espressioni primarie, che sono gli "atomi" di qualsiasi espressione.
    /// Questo metodo è il cuore del parsing delle espressioni complesse e della disambiguazione sintattica
    /// permessa durante la Fase 1, nel rispetto rigoroso dell'architettura a passate separate.
    ///
    /// MEMORIA ARCHITETTURALE (NON DIMENTICARE):
    /// L'obiettivo di questo metodo in FASE 1 è produrre un AST sintatticamente corretto ma "ingenuo".
    /// È ASSOLUTAMENTE VIETATO tentare di risolvere simboli definiti dall'utente (variabili, UDF, array).
    /// L'unica risoluzione semantica permessa è quella basata sulla libreria standard (`StdLib`),
    /// che è statica e immutabile.
    ///
    /// Logica di Funzionamento a Due Fasi:
    /// 1. Parsing dell'Atomo Base: La chiamata iniziale a `ParsePrimaryAtom` analizza l'elemento
    ///    più a sinistra dell'espressione (un letterale, un nome, un'espressione tra parentesi, etc.).
    /// 2. Loop di Chaining: Un loop `while(true)` gestisce tutti gli operatori di accesso che seguono
    ///    l'atomo base (`.`, `(`, `[`). Ad ogni iterazione, "avvolge" l'AST corrente in un nodo più complesso,
    ///    costruendo espressioni a catena come `MioNamespace.MioOggetto.MioMembro(i)`.
    ///
    /// Regole di Disambiguazione (Cosa si Risolve in Fase 1):
    /// - Chiamate Qualificate a Funzioni Built-in (`NAMESPACE.FUNZIONE(...)`): Viene risolto immediatamente.
    ///   Se la base è un namespace noto, crea un `BuiltinFunctionCallExpr` (se non ambiguo) o un
    ///   `UnresolvedCallExpr` (se ambiguo tra più namespace). Esempio: `MATH.SIN(x)`.
    /// - Chiamate Non Qualificate a Funzioni Built-in (`FUNZIONE(...)`): Viene risolto tramite l'helper
    ///   `ResolveBuiltinFunctionCall`. Se il nome è in `StdLib`, crea un `BuiltinFunctionCallExpr`
    ///   o `UnresolvedCallExpr`. Esempio: `SIN(x)`.
    /// - Chiamate a Metodo (`oggetto.Metodo(...)`): Sintatticamente, viene creato un `MethodCallExpr`.
    ///   Il parser NON sa se `oggetto` ha un `Metodo`. La validazione è compito del Linker.
    ///
    /// Cosa Viene Deliberatamente Delegato al Linker (Fase 3):
    /// - Ambiguità `NOME(...)` per Funzioni Utente vs. Array: Se `NOME` non è una funzione built-in,
    ///   il parser crea **SEMPRE** un nodo "ponte" `ArrayAccessExpr`. Il parser di Fase 1 NON DEVE
    ///   interrogare la `SymbolTable` per UDF. Sarà il Linker a ispezionare questo `ArrayAccessExpr`
    ///   e, se necessario, a riscriverlo in un `FunctionExpr`. Esempio: `MiaFunzione(x)` -> `ArrayAccessExpr(...)`.
    /// - Validazione dei Tipi e dei Membri: Il parser non sa se `MioOggetto` ha un membro `MioMembro`.
    ///   Crea il `FieldAccessExpr` o `MethodCallExpr` basandosi solo sulla sintassi. La validazione è compito del Linker.
    /// </summary>
    private Expr ParseLinqQuery()
    {
        var fromToken = Advance(); // consuma FROM
        var itemVar = Consume(TokenKind.Identifier, "Expected item variable name after FROM.");
        if (!Match(TokenKind.In))
            throw CreateError(Peek(), "Expected 'IN' after item variable in FROM clause.");
        var source = ParseExpression();

        Expr? whereClause = null;
        ImmutableArray<LinqOrderByClause>? orderBy = null;
        Expr? selectClause = null;
        Expr? groupByClause = null;

        if (Match(TokenKind.Where)) whereClause = ParseExpression();
        if (Match(TokenKind.Order) && Check(TokenKind.By))
        {
            Advance(); // BY
            var orderBys = ImmutableArray.CreateBuilder<LinqOrderByClause>();
            do
            {
                var orderExpr = ParseExpression();
                bool descending = Match(TokenKind.Desc);
                if (!descending) Match(TokenKind.Asc);
                orderBys.Add(new LinqOrderByClause(orderExpr, descending));
            } while (Match(TokenKind.Comma));
            orderBy = orderBys.ToImmutable();
        }
        if (Match(TokenKind.Group) && Check(TokenKind.By)) { Advance(); groupByClause = ParseExpression(); }
        if (Match(TokenKind.Select)) selectClause = ParseExpression();

        return new LinqQueryExpr(fromToken, itemVar, -1, source, whereClause, orderBy, selectClause, groupByClause);
    }

    private Expr ParsePrimary()
    {
        const bool log = false;

        // --- LINQ QUERY ---
        if (Check(TokenKind.From))
            return ParseLinqQuery();
        Log($"[ParsePrimary] ENTER. Current: {Peek()}", log);

        var expr = TryParseForeignAmountSuffix(ParsePrimaryAtom());
        Log($"[ParsePrimary]   -> Atomo base parsato: {expr.GetType().Name} ('{expr}')", log);

        // --- POSTFIX: espr HAS VALUE (controllo nullable) ---
        // Riconosce il pattern: <espr> HAS VALUE
        if (Check(TokenKind.Has) && At(1).Kind == TokenKind.Value)
        {
            var hasToken = Advance(); // consuma HAS
            Advance(); // consuma VALUE
            expr = new NullableHasValueExpr(expr, hasToken);
            Log($"[ParsePrimary]   -> Rilevato postfix 'HAS VALUE'. Expr: '{expr}'", log);
        }

        // --- POSTFIX: espr AS? TypeName (safe cast) ---
        // Riconosce il pattern: <espr> AS? TypeName
        if (Check(TokenKind.As) && At(1).Kind == TokenKind.QuestionMark)
        {
            Advance(); // consuma AS
            Advance(); // consuma ?
            var asToken = Previous();
            var targetType = ParseTypeName();
            expr = new SafeCastExpr(expr, targetType, asToken);
            Log($"[ParsePrimary]   -> Rilevato postfix 'AS?'. Expr: '{expr}'", log);
        }

        while (true)
        {
            // --- NULL-CONDITIONAL: ?. ?( ?[ ---
            if (Check(TokenKind.QuestionMark))
            {
                var nextKind = At(1).Kind;
                if (nextKind == TokenKind.Dot)
                {
                    Advance(); // consuma ?
                    Advance(); // consuma .
                    var memberToken = Peek();
                    if (!IsPermissiveNameToken(memberToken))
                        throw CreateError(memberToken, "Expected member name after '?.'.");
                    Advance();
                    expr = new NullConditionalAccessExpr(expr, NullConditionalKind.Member, memberToken, default);
                    continue;
                }
                if (nextKind == TokenKind.LeftParen)
                {
                    Advance(); // consuma ?
                    Advance(); // consuma (
                    var args = ParseExpressionList(TokenKind.RightParen);
                    Consume(TokenKind.RightParen, "Expected ')' after null-conditional call.");
                    expr = new NullConditionalAccessExpr(expr, NullConditionalKind.Call, null, args);
                    continue;
                }
                if (nextKind == TokenKind.LeftBracket)
                {
                    Advance(); // consuma ?
                    Advance(); // consuma [
                    var indices = ParseExpressionList(TokenKind.RightBracket);
                    Consume(TokenKind.RightBracket, "Expected ']' after null-conditional index.");
                    expr = new NullConditionalAccessExpr(expr, NullConditionalKind.Index, null, indices);
                    continue;
                }
            }
            if (Match(TokenKind.Dot, TokenKind.Arrow))
            {
                var memberToken = Peek();
                if (!IsPermissiveNameToken(memberToken))
                {
                    throw CreateError(memberToken, "Expected a member name after '.' or '->'.");
                }

                Advance();

                // NOTA: '.VALUE' su un tipo nullable (es. `d.VALUE` con `d AS DOUBLE?`) va distinto
                // da un campo UDT/CLASS letteralmente chiamato "Value" (es. `TYPE Box: Value AS T`).
                // In Fase 1 il tipo della base non è ancora noto, quindi non possiamo decidere qui:
                // costruiamo sempre un FieldAccessExpr sintattico; è il Linker (RewriteFieldAccess) a
                // riscriverlo in NullableValueExpr, ma solo quando "Value" NON corrisponde a un vero
                // campo/proprietà UDT/CLASS della base (il campo reale ha sempre la precedenza).
                expr = new FieldAccessExpr(expr, memberToken);
                Log($"[ParsePrimary]     -> Chained with DOT. Current expr is now FieldAccessExpr: '{expr}'",
                    log);
            }
            else if (Match(TokenKind.LeftParen))
            {
                Log("[ParsePrimary]   -> Chaining with '()'", log);

                // --- GESTIONE SPECIALE PER JSON.TOOBJECT ---
                if (expr is QualifiedNameExpr qneForObject &&
                    qneForObject.FullName.Equals("JSON.TOOBJECT", StringComparison.OrdinalIgnoreCase))
                {
                    expr = ParseJsonToObjectCall(qneForObject.Parts.Last());
                    Consume(TokenKind.RightParen, "Expected ')' to close arguments for JSON.ToObject.");
                    continue;
                }

                var args = ParseExpressionList(TokenKind.RightParen);
                Consume(TokenKind.RightParen, "Expected ')' after arguments.");

                // if (expr is VariableExpr ve && args.IsEmpty &&
                //     !StdLib.Instance.AllBuiltinFunctionNames.Contains(ve.OriginalToken.Lexeme))
                // {
                //     Log(
                //         $"[ParsePrimary]     -> Detected empty '()' on a variable. Treating as array reference, " +
                //         $"not access. Expression remains '{expr}'.",
                //         log);
                // }
                // else
                // {
                switch (expr)
                {
                    // COMMENTO ESPLICATIVO:
                    // Questo case è FONDAMENTALE. Viene eseguito quando il parser incontra una chiamata a funzione
                    // qualificata, come `DBF.SELECT(...)`. In questo scenario:
                    // 1. `ParsePrimaryAtom` parsa `DBF.SELECT` come un `QualifiedNameExpr`.
                    // 2. Il flusso arriva qui dopo aver consumato `(`.
                    // 3. Questo blocco si occupa di risolvere il `QualifiedNameExpr` in una chiamata a funzione
                    //    built-in, se possibile. Se viene rimosso, il parser cade nel `default` e crea
                    //    erroneamente un `IndexedAccessExpr`.
                    case QualifiedNameExpr qne:
                    {
                        var namespaceToken = qne.Parts[0];
                        var memberToken = qne.Parts.Last();
                        // `WORLD` non è implicito: le letture qualificate (`WORLD.TIME()`,
                        // `WORLD.MOVING(1)`, …) richiedono IMPORT WORLD come i comandi. Gli altri
                        // namespace restano risolvibili dal registro senza IMPORT (TRADING.PORTFOLIO.*).
                        if (string.Equals(namespaceToken.Lexeme, "WORLD", StringComparison.OrdinalIgnoreCase)
                            && !IsNamespaceLoaded("WORLD"))
                        {
                            throw CreateError(namespaceToken,
                                "namespace non importato: WORLD (aggiungi IMPORT WORLD).");
                        }
                        var memberName = memberToken.Lexeme;

                        // Namespace COMPOSTI (es. TRADING.OPTIONS.ZOMMA): il namespace è il PREFISSO
                        // più lungo che è un namespace REGISTRATO — "TRADING" non contiene ZOMMA
                        // (è in "TRADING.OPTIONS"). Prima si usava solo Parts[0] -> UnresolvedCall.
                        var prefix = "";
                        string? resolvedNs = null;
                        List<NativeProcedureDefinition> candidates = [];
                        var lookupMember = memberName.EndsWith('$') ? memberName[..^1] : memberName;
                        for (int pi = 0; pi < qne.Parts.Length - 1; pi++)
                        {
                            prefix = prefix.Length == 0 ? qne.Parts[pi].Lexeme : prefix + "." + qne.Parts[pi].Lexeme;
                            bool materialized = _program.Namespaces.Contains(prefix);
                            List<NativeProcedureDefinition> cand = [];
                            if (materialized)
                            {
                                cand = _program.Symbols.GetProceduresForNamespace(prefix)
                                    .OfType<NativeProcedureDefinition>()
                                    .Where(p =>
                                        p.OriginalName.Lexeme.Equals(lookupMember, StringComparison.OrdinalIgnoreCase) &&
                                        SymbolTable.IsProcedureSignatureCompatible(p, args.Length))
                                    .ToList();
                            }
                            // Namespace COMPOSTO non materializzato (nessun IMPORT): il registro
                            // canonico lo conosce comunque. Prima si usava solo `Parts[0]` per
                            // il fallback → `TRADING.PORTFOLIO.ISLONGONLY(...)` senza IMPORT
                            // dava "ISLONGONLY non definita" (mentre il C99Native la risolve e
                            // con `IMPORT TRADING.PORTFOLIO` funzionava su tutte le gambe).
                            if (cand.Count == 0 && _registry.BuiltinProcedures.TryGetValue(prefix, out var regProcs))
                            {
                                cand = regProcs.OfType<NativeProcedureDefinition>()
                                    .Where(p =>
                                        p.OriginalName.Lexeme.Equals(lookupMember, StringComparison.OrdinalIgnoreCase) &&
                                        SymbolTable.IsProcedureSignatureCompatible(p, args.Length))
                                    .ToList();
                            }
                            if (cand.Count > 0) { candidates = cand; resolvedNs = prefix; }
                            else if (materialized && resolvedNs == null) resolvedNs = prefix;
                        }
                        // Se il namespace è registrato ma lo scope builtin non è materializzato
                        // (GetProceduresForNamespace può restituire [] per i namespace composti — in
                        // pratica: una chiamata QUALIFICATA tipo CONVERT.FORMATS() senza un IMPORT
                        // CONVERT precedente, che NON materializza il namespace nello scope), PRIMA
                        // di arrendersi al token grezzo sotto, prova _registry.BuiltinProcedures — lo
                        // STESSO registro "completo" (indipendente dall'IMPORT) usato dal fallback
                        // poco più sotto per un caso diverso (resolvedNs == null): trovare qui il
                        // NativeProcedureDefinition reale dà il Token con la TokenKind SPECIFICA
                        // (es. TokenKind.ConvFormats), la stessa che avrebbe con IMPORT. Senza
                        // questo tentativo, il ramo sotto costruiva la chiamata con `memberToken`
                        // grezzo (Kind sempre TokenKind.Identifier, mai risolto al parse time per un
                        // nome come "FORMATS" non richiesto altrove come keyword globale) — il
                        // Compiler (BuiltInFunctionMap.TryGetValue) non trova mai quella TokenKind,
                        // "found" resta false, la chiamata silenziosamente non fa nulla e ritorna
                        // vuoto/NIL (bug: CONVERT.*/AI.* qualificate senza IMPORT compilano ma non
                        // fanno nulla, mai un errore visibile).
                        if (resolvedNs != null && candidates.Count == 0 &&
                            _registry.BuiltinProcedures.TryGetValue(resolvedNs, out var earlyFallbackProcs))
                        {
                            var lookupName = memberName.EndsWith('$') ? memberName[..^1] : memberName;
                            candidates = earlyFallbackProcs
                                .OfType<NativeProcedureDefinition>()
                                .Where(p =>
                                    p.OriginalName.Lexeme.Equals(lookupName, StringComparison.OrdinalIgnoreCase) &&
                                    SymbolTable.IsProcedureSignatureCompatible(p, args.Length))
                                .ToList();
                        }
                        // Se il namespace è noto nel registro ma non è ancora materializzato nello scope
                        // (es. chiamata qualificata AI.FUNC() senza IMPORT AI), prova il registro completo.
                        // Questo gestisce chiamate qualificate a namespace non importati esplicitamente.
                        if (candidates.Count == 0 && resolvedNs == null &&
                            _registry.BuiltinProcedures.TryGetValue(namespaceToken.Lexeme, out var nsProcs))
                        {
                            var lookupName = memberName.EndsWith('$') ? memberName[..^1] : memberName;
                            candidates = nsProcs
                                .OfType<NativeProcedureDefinition>()
                                .Where(p =>
                                    p.OriginalName.Lexeme.Equals(lookupName, StringComparison.OrdinalIgnoreCase) &&
                                    SymbolTable.IsProcedureSignatureCompatible(p, args.Length))
                                .ToList();
                            if (candidates.Count > 0) resolvedNs = namespaceToken.Lexeme;
                        }
                        // Se ANCHE il registro completo non trova nulla (namespace composto
                        // genuinamente "vuoto" a questo prefisso), costruisci comunque la chiamata
                        // col token ORIGINALE (Kind = keyword, es. TokenKind.Zomma) — il compilatore
                        // dispatcha via BuiltInFunctionMap. Resta come ultima spiaggia per i
                        // namespace la cui TokenKind del membro è GIÀ quella giusta di suo (keyword
                        // globale, non un Identifier generico).
                        if (resolvedNs != null && candidates.Count == 0)
                        {
                            var tokenKind = memberToken.Kind;
                            // Il membro è una vera funzione registrata (non basta che il namespace
                            // esista: prima CONSOLE.SETWINDOWTITLE(...) — funzione inesistente —
                            // finiva QUI come BuiltinFunctionCallExpr con Kind Identifier, il
                            // Compiler non la trovava nella mappa e compilava OP_PUSH_NIL (no-op
                            // silenzioso, mai un errore). Con il check stretto su
                            // AllBuiltinFunctionNames la chiamata scende a UnresolvedCallExpr e il
                            // Compiler lancia "Funzione o subroutine non definita".
                            // Strip $ suffix for builtin function name lookup (e.g., CONSISTENCYDETAIL$ -> CONSISTENCYDETAIL)
                            var funcNameForLookup = memberName.EndsWith('$') ? memberName[..^1] : memberName;
                            var builtinMatch = _registry.AllBuiltinFunctionNames.Contains(funcNameForLookup);
                            // Fix 2026-08-11 (allineato GMBasicAI 64c976c): il check deve coprire
                            // anche il Kind del token — con Kind Identifier (nome NON keyword, es.
                            // `AI.STOPSERVER` dove "STOPSERVER" è una parola qualsiasi) NON si può
                            // dispatchare via BuiltInFunctionMap (keyed dal TokenKind lessicale, che
                            // per un Identifier è ambiguo): si scende a UnresolvedCallExpr per un
                            // errore esplicito, invece di un NIL silenzioso.
                            if (builtinMatch && tokenKind != TokenKind.Identifier)
                            {
                                // Strip $ suffix for the BuiltinFunctionCallExpr token (e.g., CONSISTENCYDETAIL$ -> CONSISTENCYDETAIL)
                                // The linker matches against OriginalName.Lexeme which doesn't have $.
                                // Kind REGISTRATO quando il namespace dichiara davvero quel nome
                                // (anche se non importato): il nodo di chiamata non deve mai portare
                                // il Kind GENERICO del lexer (es. `CONVERT.SEARCH` → DocSearch, non
                                // Search) — a valle il dispatch è per TokenKind.
                                NativeProcedureDefinition? registeredCall = null;
                                if (_registry.BuiltinProcedures.TryGetValue(resolvedNs, out var registeredProcs))
                                    registeredCall = registeredProcs.OfType<NativeProcedureDefinition>()
                                        .FirstOrDefault(p =>
                                            p.OriginalName.Lexeme.Equals(funcNameForLookup, StringComparison.OrdinalIgnoreCase) &&
                                            SymbolTable.IsProcedureSignatureCompatible(p, args.Length));
                                var tokenForCall = registeredCall is not null
                                    ? DedicatedBuiltinToken(registeredCall, memberToken)
                                    : new Token(memberToken.Kind, funcNameForLookup, funcNameForLookup, memberToken.Position);
                                // Il membro ha già una TokenKind SPECIFICA (keyword globale, es. MMAPPEEK
                                // chiamato come IO.MMAPPEEK): risolvi il tipo di ritorno canonicamente
                                // per TokenKind dal registro (la qualifica non deve perdere la firma
                                // registrata sotto un altro namespace). Richiede unanimità altrimenti
                                // resta InferredType (mai inventare un tipo).
                                DataType callReturnType = new InferredType();
                                var kindMatches = _registry.BuiltinProcedures.Values
                                    .SelectMany(pl => pl).OfType<NativeProcedureDefinition>()
                                    .Where(p => p.OriginalName.Kind == memberToken.Kind).ToList();
                                if (kindMatches.Count > 0 && kindMatches.All(p => p.ReturnType.Equals(kindMatches[0].ReturnType)))
                                    callReturnType = kindMatches[0].ReturnType;
                                expr = new BuiltinFunctionCallExpr(namespaceToken, tokenForCall, args, callReturnType);
                                Log($"[ParsePrimary] -> BuiltinFunctionCallExpr (namespace composto, scope assente): {expr}", log);
                                break;
                            }
                        }
                        var namespaceName = resolvedNs ?? namespaceToken.Lexeme;

                        // Fallback al registro completo SOLO se il namespace è stato IMPORTato
                        // (o è nei default): senza IMPORT il namespace non è visibile e la chiamata
                        // deve dare errore. Il fallback gestisce il caso di scope non materializzato
                        // per i namespace già caricati.
                        if (candidates.Count == 0 && _program.Namespaces.Contains(namespaceName) &&
                            _registry.BuiltinProcedures.TryGetValue(namespaceName, out var fallbackProcs))
                        {
                            candidates = fallbackProcs
                                .OfType<NativeProcedureDefinition>()
                                .Where(p =>
                                    p.OriginalName.Lexeme.Equals(memberName, StringComparison.OrdinalIgnoreCase) &&
                                    SymbolTable.IsProcedureSignatureCompatible(p, args.Length))
                                .ToList();
                        }

                        if (candidates.Count == 1)
                        {
                            var procDef = candidates[0];
                            expr = new BuiltinFunctionCallExpr(namespaceToken,
                                DedicatedBuiltinToken(procDef, procDef.OriginalName), args,
                                procDef.ReturnType);
                            Log(
                                $"[ParsePrimary]     -> Resolved QualifiedNameExpr as BuiltinFunctionCallExpr: '{expr}'",
                                log);
                        }
                        else if (candidates.Count > 1 && candidates.All(c => c is NativeProcedureDefinition npd && npd.BuiltinKind == ((NativeProcedureDefinition)candidates[0]).BuiltinKind))
                        {
                            // Overload multipli con lo stesso BuiltinKind (es. ISAM.INTEGRITY(LongType/StringType)):
                            // prendiamo il primo, il runtime disambigua in base al tipo dell'argomento.
                            var procDef = candidates[0];
                            expr = new BuiltinFunctionCallExpr(namespaceToken,
                                DedicatedBuiltinToken(procDef, procDef.OriginalName), args,
                                procDef.ReturnType);
                            Log(
                                $"[ParsePrimary]     -> Resolved ambiguous qualified call as BuiltinFunctionCallExpr (first overload): '{expr}'",
                                log);
                        }
                        else
                        {
                            expr = new UnresolvedCallExpr(memberToken, args, new InferredType(),
                                candidates.Count > 1
                                    ? UnresolvedReason.AmbiguousBuiltinFunctions
                                    : UnresolvedReason.FunctionNotFound,
                                [..candidates], namespaceName);
                            Log(
                                $"[ParsePrimary]     -> Created UnresolvedCallExpr for ambiguous/unfound qualified call: '{expr}'",
                                log);
                        }

                        break;
                    }

                    // Questo case gestisce le chiamate a metodo (`oggetto.membro(...)`) e le
                    // chiamate a funzioni qualificate (`NAMESPACE.Funzione(...)`).
                    case FieldAccessExpr fae:
                    {
                        Expr? resolvedExpr = null;

                        // Usiamo NameSuffixed (o NameCleaned) invece di OriginalToken.
                        // I namespace non dovrebbero avere suffissi, quindi NameCleaned andrebbe bene,
                        // ma NameSuffixed è il token originale, quindi usiamo quello per coerenza.
                        if (fae.Structure is VariableExpr namespaceVar &&
                            _program.Namespaces.Contains(namespaceVar.NameSuffixed.Lexeme))
                        {
                            resolvedExpr = ResolveBuiltinFunctionCall(fae.Field, args,
                                namespaceVar.NameCleaned.Lexeme);
                        }
                        // Namespace COMPOSTI (es. TRADING.OPTIONS.ZOMMA(...)): la struttura è
                        // FieldAccessExpr(TRADING, OPTIONS) e non una VariableExpr — senza questo
                        // ramo cadeva nel MethodCallExpr e il linker non risolveva mai le funzioni
                        // dei namespace con il punto (TRADING.OPTIONS/PORTFOLIO/METRICS/...).
                        else if (fae.Structure is FieldAccessExpr nestedFae &&
                                 TryGetQualifiedNamespace(nestedFae, out var compNs) &&
                                 SymbolTable.BuiltinNamespaceNames.Contains(compNs))
                        {
                            resolvedExpr = ResolveBuiltinFunctionCall(fae.Field, args, compNs);
                        }

                        if (resolvedExpr != null)
                        {
                            expr = resolvedExpr;
                            Log(
                                $"[ParsePrimary]     -> Resolved FieldAccessExpr as Qualified Built-in Call: '{expr.GetType().Name}'",
                                log);
                        }
                        else
                        {
                            // Se non è un built-in risolto, creiamo un MethodCallExpr.
                            // F1 Incongruenze: la grafia del membro viene risolta qui nel
                            // TokenKind dedicato Coll* (chiave canonica, AGENTS §12) — il
                            // lexeme resta invariato, cambia solo il Kind; i membri fuori
                            // tabella (HTTP/WS/SQL/metodi utente) restano immutati.
                            expr = new MethodCallExpr(fae.Structure, CollMembers.Remap(fae.Field), args);
                            Log($"[ParsePrimary]     -> Transformed FieldAccessExpr to MethodCallExpr: '{expr}'", log);
                        }

                        break;
                    }

                    // Questo case gestisce le chiamate a funzioni non qualificate (es. `SIN(...)`) e
                    // gli accessi ad array (es. `mioArray(...)`).
                    case VariableExpr varExpr:
                    {
                        // Un nome con suffisso $ DICHIARATO come variabile (es. DIM LIST$(5))
                        // ha la priorità sulla funzione builtin SENZA $ (conflitto col nome
                        // "LIST" di PROVIDER.LIST): altrimenti LIST$(I) verrebbe risolto come
                        // chiamata a ProviderList(I) — bug scoperto con StringsExamples.bas.
                        // Stessa regola per un ARRAY dichiarato omonimo di un builtin in forma
                        // nuda (es. DIM amax() vs AMAX, DIM argmax() vs MATRIX.ARGMAX):
                        // amax(0) è accesso all'elemento, non chiamata al builtin — senza,
                        // la variabile è illeggibile (bug MATRIX.ARGMAX/ARGMIN).
                        // 2026-11-01 — OGNI variabile dichiarata (anche scalare/variante, es.
                        // `DIM voci` riempita da STRINGS.SPLIT) shadowa una procedura omonima:
                        // `voci(0)` è accesso all'elemento, non una chiamata. Prima la regola
                        // valeva solo per i nomi con `$` e per gli array dichiarati; il nome
                        // senza suffisso cadeva sulla procedura importata (bug scoperto
                        // scrivendo l'app DigUp). È la regola di shadowing per SCOPE, decisa
                        // dal nome dichiarato, non dal Kind del token.
                        // Uno SCALARE omonimo non devia il builtin (DIM amax = 5 + AMAX(v));
                        // un array o un nome non tipizzato che può contenere una collezione
                        // (DIM voci + STRINGS.SPLIT) invece sì. L'ultima dichiarazione vince.
                        var declaredName = _declaredVariableNames.Contains(varExpr.NameSuffixed.Lexeme)
                            || _declaredArrayNames.Contains(varExpr.NameSuffixed.Lexeme);
                        var scalarShadow = _declaredVariableIsScalar.TryGetValue(
                            varExpr.NameSuffixed.Lexeme, out var isScalar) && isScalar;
                        if (declaredName && !scalarShadow)
                        {
                            expr = new ArrayAccessExpr(varExpr.SymbolId, varExpr.NameSuffixed, varExpr.NameCleaned, args);
                            if (Match(TokenKind.QuestionMark) && expr is ArrayAccessExpr aaeVar)
                                expr = aaeVar with { IsSafe = true };
                            break;
                        }
                        // Usiamo NameSuffixed. Importante per funzioni come LEFT$ o CHR$.
                        var resolvedBuiltin = ResolveBuiltinFunctionCall(varExpr.NameSuffixed, args);
                        if (resolvedBuiltin != null)
                        {
                            expr = resolvedBuiltin;
                            Log(
                                $"[ParsePrimary]     -> Resolved as Unqualified Built-in Call: '{expr.GetType().Name}'",
                                log);
                        }
                        else
                        {
                            expr = new ArrayAccessExpr(varExpr.SymbolId, varExpr.NameSuffixed, varExpr.NameCleaned,
                                args);
                            Log($"[ParsePrimary]     -> Fallback to ArrayAccessExpr for UDF/Array: '{expr}'",
                                log);
                            if (Match(TokenKind.QuestionMark) && expr is ArrayAccessExpr aae)
                                expr = aae with { IsSafe = true };
                        }

                        break;
                    }

                    // COMMENTO ESPLICATIVO:
                    // Questo è il caso di fallback. Gestisce le chiamate su espressioni complesse,
                    // come `GetArray()(1)`. In questo caso, `GetArray()` è `expr` e viene avvolto
                    // in un `IndexedAccessExpr`.
                    default:
                    {
                        expr = new IndexedAccessExpr(expr, args);
                        Log(
                            $"[ParsePrimary]     -> Transformed complex expression to IndexedAccessExpr: '{expr}'",
                            log);
                        if (Match(TokenKind.QuestionMark) && expr is IndexedAccessExpr iae1)
                            expr = iae1 with { IsSafe = true };
                        break;
                    }
                }
            }
            //}
            else if (Match(TokenKind.LeftBracket))
            {
                Log("[ParsePrimary]   -> Chaining with INDEX '[]'", log);

                // --- NUOVA LOGICA DI SLICING E INDICIZZAZIONE UNIFICATA ---

                // Controlla se è uno slice o un accesso a indice
                // Un lookahead è necessario per distinguere `[1,2]` da `[1:2]`
                var isSlice = false;
                var savedPos = _current;
                try
                {
                    // Parsa una potenziale prima espressione per vedere cosa la segue
                    if (!Check(TokenKind.Colon)) // Se non inizia con ':', parsa un'espressione
                    {
                        ParseExpression();
                    }

                    if (Check(TokenKind.Colon)) // Se dopo l'espressione (o all'inizio) c'è ':', è uno slice
                    {
                        isSlice = true;
                    }
                }
                finally
                {
                    _current = savedPos; // Ripristina sempre la posizione
                }

                if (isSlice)
                {
                    Expr? start = null;
                    if (!Check(TokenKind.Colon))
                    {
                        start = ParseExpression();
                    }

                    Consume(TokenKind.Colon, "Expected ':' for array slicing.");
                    Expr? end = null;
                    if (!Check(TokenKind.RightBracket))
                    {
                        end = ParseExpression();
                    }

                    Consume(TokenKind.RightBracket, "Expected ']' to end array slice.");
                    expr = new ArraySliceExpr(expr, start, end);
                    Log($"[ParsePrimary]     -> Transformed to ArraySliceExpr: '{expr}'", log);
                }
                else
                {
                    // È un accesso a indice normale
                    var indices = ParseExpressionList(TokenKind.RightBracket);
                    Consume(TokenKind.RightBracket, "Expected ']' after indices.");
                    expr = new IndexedAccessExpr(expr, indices);
                    Log($"[ParsePrimary]     -> Transformed to IndexedAccessExpr: '{expr}'", log);
                    if (Match(TokenKind.QuestionMark) && expr is IndexedAccessExpr iae2)
                        expr = iae2 with { IsSafe = true };
                }
            }
            else if (Match(TokenKind.PlusPlus, TokenKind.MinusMinus))
            {
                var op = Previous();
                // L'espressione parsata finora deve essere convertita in un target.
                var target = ParseTargetFromExpr(expr);
                expr = new PostfixIncrementExpr(target, op);
                Log(
                    $"[ParsePrimary]     -> Chained with Postfix '{op.Lexeme}'. Current expr is now PostfixIncrementExpr: '{expr}'",
                    log);
            }
            else
            {
                break;
            }
        }

        Log($"[ParsePrimary] EXIT. Final expression: {expr.GetType().Name} ('{expr}'). Next token: {Peek()}",
            log);
        return expr;
    }

    /// <summary>
    /// [FASE 1 - HELPER] Tenta di risolvere una chiamata a funzione non qualificata come funzione built-in.
    /// QUESTA VERSIONE È CORRETTA E GESTISCE I PARAMETRI OPZIONALI e la possibile assenza del TokenKind nella mappa.
    /// </summary>
    /// <returns>
    /// Un `BuiltinFunctionCallExpr` se la funzione è univoca,
    /// un `UnresolvedCallExpr` se è ambigua,
    /// o `null` se non è una funzione built-in nota.
    /// </returns>

    /// <summary>
    /// Ricostruisce il nome qualificato di un namespace COMPOSTO dalla struttura dell'espressione:
    /// FieldAccessExpr(VariableExpr("TRADING"), "OPTIONS") -> "TRADING.OPTIONS".
    /// </summary>
    private static bool TryGetQualifiedNamespace(Expr structure, out string qualified)
    {
        if (structure is VariableExpr ve) { qualified = ve.NameSuffixed.Lexeme; return true; }
        if (structure is FieldAccessExpr fae2 && TryGetQualifiedNamespace(fae2.Structure, out var prefix))
        {
            qualified = prefix + "." + fae2.Field.Lexeme;
            return true;
        }
        qualified = "";
        return false;
    }

    /// <summary>Token NOME di una call builtin risolta da una NativeProcedureDefinition:
    /// impone il TokenKind REGISTRATO (npd.BuiltinKind) invece di quello del lexer/lexeme. Regola
    /// d'oro §12.2: la chiave canonica è (namespace, TokenKind). Per i nomi che sono anche keyword
    /// (es. SEARCH → TokenKind.Search generico) il lexer darebbe un Kind condiviso; qui si forza il
    /// Kind DEDICATO (es. TokenKind.MemorySearch per MEMORY.SEARCH). Se il candidato non è una
    /// NativeProcedureDefinition (es. FunctionDefinition), si tiene il token originale.</summary>
    private static Token DedicatedBuiltinToken(ProcedureDefinition proc, Token fallback)
        => proc is NativeProcedureDefinition npd
            ? new Token(npd.BuiltinKind, npd.OriginalName.Lexeme, null, npd.OriginalName.Position)
            : fallback;

    /// <summary>Visibilità dei namespace built-in per una chiamata NON qualificata: il
    /// namespace deve essere caricato (IMPORT o default dei settings) perché la funzione sia
    /// richiamabile senza prefisso (parità con la risoluzione del Linker).</summary>
    private bool IsNamespaceLoaded(string namespaceName)
        => _program.Namespaces.Contains(namespaceName)
           || namespaceName.Equals("GUI", StringComparison.OrdinalIgnoreCase)
           || _program.Settings.DefaultNamespaces.Contains(namespaceName, StringComparer.OrdinalIgnoreCase);

    /// <summary>Nome usato per la risoluzione PER NOME dei builtin: serve SOLO quando il token
    /// è un Identifier (nessun Kind canonico) o per una chiamata qualificata. TIME$/TI$ e DATE$
    /// sono alias di nome DOCUMENTATI (il lexer li lascia identificatori di proposito, così una
    /// variabile utente `date$` continua a funzionare) — non sono un dispatch per lexeme.</summary>
    private static string BuiltinNameLookupKey(Token nameToken)
    {
        var funcName = nameToken.Lexeme;
        if (!funcName.EndsWith('$')) return funcName;
        return funcName.ToUpperInvariant() switch
        {
            "TIME$" or "TI$" => "STRTIME",
            "DATE$" => "STRDATE",
            _ => funcName[..^1]
        };
    }

    private Expr? ResolveBuiltinFunctionCall(Token nameToken, ImmutableArray<Expr> args,
        string? namespaceName = null)
    {
        /* SIZEOF ha un canone basato sulla FORMA dell'argomento (TypeLayout): identificatore
         * (anche fra parentesi) → tipo statico; letterale numerico → 8; QUALUNQUE altra
         * espressione → -1. Deciderlo qui, prima di ogni folding algebrico/costante, è l'unico
         * modo di non dipendere dall'ordine delle ottimizzazioni: con il fold attivo
         * `SIZEOF(i + 0)` e `SIZEOF(1 + 2)` davano 2/8 su IL/PCode/VM-C e -1 su C99Native
         * (divergenza misurata 2026-11-01). La sostituzione con la costante è sicura perché
         * SIZEOF NON valuta il proprio argomento (nessun effetto collaterale da perdere). */
        if (nameToken.Kind == TokenKind.SizeOf && args.Length == 1)
        {
            var form = args[0];
            while (form is GroupingExpr grp) form = grp.Expression;
            if (form is not (VariableExpr or NumberExpr))
                return new NumberExpr(TypeLayout.NoStaticSize, nameToken);
        }

        // Una PROCEDURA UTENTE con lo stesso nome di una keyword builtin vince quando la
        // chiamata non è qualificata: `FUNCTION Sum()` rende `Sum(x)` una chiamata utente e
        // non `MATH.Sum(x)`. Il Linker risolve poi il nome al simbolo della funzione definita
        // nel file (fallback ArrayAccessExpr → risoluzione UDF). Senza, il dispatch per
        // TokenKind del parser rubava la chiamata al builtin (bug types/class_array_member:
        // `Sum = ...` dentro la FUNCTION era riconosciuto dal parser, ma `Sum()` fuori no).
        if (namespaceName == null && _userProcedureNames.Contains(nameToken.Lexeme))
            return null;

        IEnumerable<ProcedureDefinition> candidates;

        // Namespace di PROVENIENZA di ogni candidato raccolto qui. Serve perché le definizioni
        // prese dal registro (`_registry.BuiltinProcedures`) NON sono materializzate nella
        // symbol table del programma: hanno `SymbolId = -1` e `GetNamespaceNameForSymbol(-1)`
        // non le risolve → il nodo di chiamata finiva con namespace VUOTO ("STRTIME()" su
        // namespace ""), che PCode/VM-C non sanno dispatchare (`STRTIME` senza parentesi
        // stampava 0 mentre IL/VM/C davano l'ora — bug scoperto il 2026-11-01).
        var candidateNamespaceOf = new List<(ProcedureDefinition Proc, string Ns)>();

        string ResolveNamespaceOf(ProcedureDefinition proc)
        {
            foreach (var (p, ns) in candidateNamespaceOf)
                if (ReferenceEquals(p, proc)) return ns;
            var fromSymbol = _program.Symbols.GetNamespaceNameForSymbol(proc.SymbolId);
            return string.IsNullOrEmpty(fromSymbol) ? "" : fromSymbol;
        }

        if (namespaceName != null)
        {
            var funcName = BuiltinNameLookupKey(nameToken);
            // --- CASO QUALIFICATO (Es. JSON.PARSE) ---
            // Ricerca diretta e non ambigua nel namespace specificato.
            candidates = _program.Symbols.GetProceduresForNamespace(namespaceName)
                .Where(p => p.OriginalName.Lexeme.Equals(funcName, StringComparison.OrdinalIgnoreCase));
        }
        else if (nameToken.Kind != TokenKind.Identifier)
        {
            // --- CASO NON QUALIFICATO: DISPATCH PER TOKENKIND (AGENTS §12, regola d'oro) ---
            // La chiave canonica è (namespace, TokenKind REGISTRATO): niente confronto per
            // lexeme. `BuiltinFunctionProvidersMap` dice quali namespace forniscono quel Kind;
            // ogni candidato deve avere `BuiltinKind == nameToken.Kind` (una NativeProcedure
            // può avere un Kind dedicato diverso dalla keyword scritta, es. AI.Search =
            // AiVectorSearch). Se il Kind non ha provider il nome NON è una chiamata builtin:
            // `LIST` (TokenKind.List) non è `AI.List` (AiList) → `list(0)` resta un accesso e
            // lo decide il linker (variabile/array o UDF).
            var kindNamespaces = _registry.BuiltinFunctionProvidersMap.TryGetValue(nameToken.Kind, out var kindNs)
                ? kindNs.Where(IsNamespaceLoaded).ToList()
                : [];

            if (kindNamespaces.Count == 0) return null;

            var kindCandidates = new List<ProcedureDefinition>();
            foreach (var nsName in kindNamespaces)
            {
                var nsProcs = _program.Symbols.GetProceduresForNamespace(nsName);
                if (!nsProcs.Any() && _registry.BuiltinProcedures.TryGetValue(nsName, out var regProcs))
                    nsProcs = regProcs;
                foreach (var p in nsProcs.Where(p =>
                             (p is NativeProcedureDefinition npd ? npd.BuiltinKind : p.OriginalName.Kind) == nameToken.Kind))
                {
                    kindCandidates.Add(p);
                    candidateNamespaceOf.Add((p, nsName));
                }
            }

            candidates = kindCandidates;
        }
        else
        {
            // --- CASO NON QUALIFICATO con NOME-IDENTIFICATORE: nessun Kind canonico ---
            // Un Identifier non porta identità (es. `FORMATS`, `EMAPT`): il nome è l'unica
            // chiave possibile — è il caso in cui il lexeme è strettamente necessario.
            var funcName = BuiltinNameLookupKey(nameToken);
            if (!_registry.AllBuiltinFunctionNames.Contains(funcName))
            {
                return null;
            }

            // Le funzioni builtin non sono intrinsecamente globali: appartengono a un namespace
            // e sono visibili senza prefisso SOLO se il loro namespace è caricato (SYSTEM/CONSOLE/
            // CONSTANTS sempre + default da settings + IMPORT). Le funzioni di namespace non
            // caricati richiedono il prefisso qualificato (es. MOUSE.X(), non X()).
            // NB: considera anche Settings.DefaultNamespaces (non solo program.Namespaces, che
            // resta vuoto nei programmi "nuovi" come i filtri DB/le chiavi di sort composte):
            // parità con la risoluzione del Linker (Linker.Rewrites.Variables.cs).
            var providerNamespaces = _registry.FunctionNameToNamespacesMap.TryGetValue(funcName, out var allNs)
                ? allNs.Where(IsNamespaceLoaded).ToList()
                : [];

            if (providerNamespaces.Count == 0)
            {
                // La funzione esiste ma il suo namespace non è caricato → non risolvibile
                // globalmente. NON trasformiamo questo in un unresolved "namespace non importato"
                // QUI: a parse-time non sappiamo se un'omonima FUNCTION/SUB utente (definita più
                // avanti nello stesso file) debba avere la priorità (shadowing) — torniamo null e
                // lasciamo risolvere al linker (Regressione evitata: sample SciPy con 'y' utente).
                return null;
            }

            var globalCandidates = new List<ProcedureDefinition>();
            foreach (var nsName in providerNamespaces)
            {
                var nsProcs = _program.Symbols.GetProceduresForNamespace(nsName);
                if (!nsProcs.Any() && _registry.BuiltinProcedures.TryGetValue(nsName, out var regProcs))
                    nsProcs = regProcs;
                foreach (var p in nsProcs.Where(p =>
                             p.OriginalName.Lexeme.Equals(funcName, StringComparison.OrdinalIgnoreCase)))
                {
                    globalCandidates.Add(p);
                    candidateNamespaceOf.Add((p, nsName));
                }
            }

            candidates = globalCandidates;
        }


        // ENV$ / ENV.HAS: arità ESATTAMENTE 1. Se il nome risolve a una di queste procedure
        // ma gli argomenti sono 0/2, l'errore è di PARAMETRI (compilazione) su tutte le gambe:
        // senza questo check il candidato non era "compatible" → null → il linker parlava di
        // "Simbolo non definito: ENV" (e VM/C usciva 134 prima del fix del runner).
        if (candidates.Any(p => p is NativeProcedureDefinition np
                && np.BuiltinKind is TokenKind.EnvGet or TokenKind.EnvHas)
            && args.Length != 1)
        {
            throw CreateError(nameToken,
                $"{nameToken.Lexeme} richiede 1 argomento (nome della variabile d'ambiente).");
        }

        var compatibleCandidates = candidates.Where(proc =>
        {
            var lastParam = proc.Parameters.LastOrDefault();
            if (lastParam != null && lastParam.Flags.HasFlag(SymbolFlags.IsParamArray))
            {
                var requiredParams = proc.Parameters.Count(p =>
                    !p.Flags.HasFlag(SymbolFlags.IsOptional) &&
                    !p.Flags.HasFlag(SymbolFlags.IsParamArray));
                return args.Length >= requiredParams;
            }

            var requiredParamsNormal = proc.Parameters.Count(p => !p.Flags.HasFlag(SymbolFlags.IsOptional));
            var totalParams = proc.Parameters.Length;
            return args.Length >= requiredParamsNormal && args.Length <= totalParams;
        }).ToList();

        switch (compatibleCandidates.Count)
        {
            case 0:
                return null;
            case 1:
            {
                var procDef = compatibleCandidates[0];
                var resolvedNsName = namespaceName ?? ResolveNamespaceOf(procDef);
                var nsToken = new Token(TokenKind.Identifier, resolvedNsName, null, nameToken.Position);

                var returnType = procDef switch
                {
                    FunctionDefinition fd => fd.ReturnType,
                    NativeProcedureDefinition npd => npd.ReturnType,
                    _ => new VoidType()
                };

                var nameForCall = DedicatedBuiltinToken(procDef, procDef is NativeProcedureDefinition nativeProc ? nativeProc.OriginalName : nameToken);
                return new BuiltinFunctionCallExpr(nsToken, nameForCall, args, returnType);
            }
        }

        if (compatibleCandidates[0] is NativeProcedureDefinition firstCandidate &&
            namespaceName == null) // Solo ambiguità non qualificata
        {
            var allEquivalent = true;
            for (var i = 1; i < compatibleCandidates.Count; i++)
            {
                if (compatibleCandidates[i] is NativeProcedureDefinition otherCandidate &&
                    otherCandidate.BuiltinKind == firstCandidate.BuiltinKind &&
                    otherCandidate.ReturnType.Equals(firstCandidate.ReturnType) &&
                    otherCandidate.Parameters.Length == firstCandidate.Parameters.Length) continue;
                allEquivalent = false;
                break;
            }

            if (allEquivalent)
            {
                var nsName = ResolveNamespaceOf(firstCandidate);
                var nsToken = new Token(TokenKind.Identifier, nsName, null, nameToken.Position);
                return new BuiltinFunctionCallExpr(nsToken,
                    DedicatedBuiltinToken(firstCandidate, firstCandidate.OriginalName), args,
                    firstCandidate.ReturnType);
            }
        }

        // Se qualificato e ambiguo, l'errore è grave (unresolved).
        if (namespaceName != null) return null;

        // Se non qualificato e ambiguo, restituiamo UnresolvedCallExpr per il Linker.
        return new UnresolvedCallExpr(nameToken, args, new InferredType(),
            UnresolvedReason.AmbiguousBuiltinFunctions,
            [..compatibleCandidates]);
    }

    /// <summary>
    /// Determina se un'espressione è di tipo stringa o può restituire una stringa,
    /// basandosi sul suffisso '$' del nome originale del simbolo.
    /// Usato per la logica di concatenazione implicita.
    /// </summary>
    /// <param name="expr">L'espressione da controllare.</param>
    /// <returns>True se l'espressione è o potrebbe essere una stringa.</returns>
    private static bool IsStringExpression(Expr expr)
    {
        return expr switch
        {
            StringExpr => true,
            VariableExpr ve => ve.NameSuffixed.Lexeme.EndsWith('$'),
            FunctionExpr fe => fe.OriginalToken.Lexeme.EndsWith('$'),
            BinaryExpr { Op.Kind: TokenKind.Plus } be => IsStringExpression(be.Left) ||
                                                         IsStringExpression(be.Right),
            _ => false
        };
    }

    /// <summary>
    /// [FASE 1] Parsa le espressioni di assegnazione inline (:=).
    /// Questo è il livello di precedenza più basso e ha associatività a destra
    /// per consentire assegnazioni concatenate come 'a := b := c'.
    /// </summary>
    /// <returns>Un nodo <see cref="Expr"/> che può essere un <see cref="AssignmentExpr"/> o un'espressione di precedenza superiore.</returns>
    private Expr ParseAssignment()
    {
        // Parsa il livello di precedenza successivo (Ternary).
        var expr = ParseTernary();

        // Se troviamo un operatore di assegnazione inline...
        if (Match(TokenKind.InlineAssign))
        {
            var op = Previous();
            var value = ParseAssignment(); // Chiamata ricorsiva per associatività a destra
            var target = ParseTargetFromExpr(expr);
            return new AssignmentExpr(target, value, op);
        }

        return expr;
    }

    /// <summary>
    /// [FASE 1 - NUOVO] Parsa l'operatore null-coalescing (??).
    /// Precedenza: Appena sopra il ternario, sotto OR logico. Associatività a destra.
    /// </summary>
    private Expr ParseCoalescing()
    {
        var expr = ParseLogicalOr();

        if (Match(TokenKind.QuestionMarkQuestion))
        {
            var op = Previous();
            var right = ParseCoalescing(); // Chiamata ricorsiva per associatività a destra
            return new BinaryExpr(expr, op, right);
        }

        return expr;
    }

    /// <summary>
    /// [FASE 1] Parsa l'operatore ternario (condizione ? vero : falso).
    /// Precedenza: Appena sopra l'assegnazione, sotto null-coalescing. Associatività a destra.
    /// </summary>
    private Expr ParseTernary()
    {
        // Chiama il nuovo livello di precedenza superiore (Coalescing) invece di LogicalOr
        var expr = ParseCoalescing();

        // Check: ? seguita da ., (, [ non è un ternario ma un null-conditional (gestito in ParsePrimary)
        if (Check(TokenKind.QuestionMark) && IsNullConditionalPrefix())
            return expr;

        if (Match(TokenKind.QuestionMark))
        {
            var truePart = ParseTernary(); // Chiamata ricorsiva per gestire nesting e associatività
            Consume(TokenKind.Colon, "Expected ':' in ternary operator.");
            var falsePart = ParseTernary(); // Associatività a destra
            return new TernaryExpr(expr, truePart, falsePart);
        }

        return expr;
    }

    /// <summary>
    /// Determina se il contesto attuale è un tipo tuple (es. (X AS DOUBLE, Y AS DOUBLE))
    /// invece di un valore tuple (es. (1, 2, 3)).
    /// Un tipo tuple ha la struttura: identificatore AS tipo, identificatore AS tipo, ...
    /// </summary>
    private bool IsTupleTypeContext()
    {
        // Guarda avanti: se dopo la prima espressione c'è AS, è un tipo tuple
        // Cerca il prossimo token AS o ) nella lista di token
        int depth = 0;
        for (int i = _current; i < _tokens.Length; i++)
        {
            var kind = _tokens[i].Kind;
            if (kind == TokenKind.LeftParen) depth++;
            else if (kind == TokenKind.RightParen)
            {
                if (depth == 0) return false; // Chiusura della nostra parentesi, nessun AS trovato
                depth--;
            }
            else if (kind == TokenKind.As && depth == 0)
            {
                return true; // Trovato AS al livello 0 → è un tipo tuple
            }
            else if (kind == TokenKind.Comma && depth == 0)
            {
                // Virgola al livello 0, continua a cercare
                continue;
            }
        }
        return false;
    }

    /// <summary>Ledger posting suffix: <c>100.00 USD @ 0.95</c>.</summary>
    private Expr TryParseForeignAmountSuffix(Expr amountExpr)
    {
        if (amountExpr is not (NumberExpr or UnaryExpr { Op.Kind: TokenKind.Minus, Right: NumberExpr }))
            return amountExpr;

        if (!Check(TokenKind.Identifier))
            return amountExpr;

        var lexeme = Peek().Lexeme;
        if (lexeme.Length != 3 || !lexeme.All(static c => char.IsAsciiLetter(c)))
            return amountExpr;

        var currencyToken = Advance();
        Expr? rateExpr = null;
        if (Match(TokenKind.AtSign))
            rateExpr = ParseUnary();

        return new ForeignAmountExpr(amountExpr, currencyToken, rateExpr);
    }

    /// <summary>
    /// Verifica se il token ? corrente è un prefisso null-conditional (?. ?( ?[).
    /// </summary>
    private bool IsNullConditionalPrefix()
    {
        if (!Check(TokenKind.QuestionMark)) return false;
        var next = At(1).Kind;
        return next == TokenKind.Dot || next == TokenKind.LeftParen || next == TokenKind.LeftBracket;
    }

    /// <summary>
    /// Parsa un singolo elemento di un literal tuple, con nome opzionale.
    /// Pattern: (nome: expr) oppure (expr)
    /// Il nome è riconosciuto come un Identifier seguito da ':' senza spazio.
    /// </summary>
    private NamedTupleElement ParseNamedTupleElementExpression()
    {
        if (Check(TokenKind.Identifier) && At(1).Kind == TokenKind.Colon)
        {
            var nameToken = Peek();
            Advance(); // consuma l'identificatore (nome)
            Advance(); // consuma ':'
            var value = ParseExpression();
            return new NamedTupleElement(value, nameToken);
        }
        else
        {
            var value = ParseExpression();
            return new NamedTupleElement(value, null);
        }
    }
}