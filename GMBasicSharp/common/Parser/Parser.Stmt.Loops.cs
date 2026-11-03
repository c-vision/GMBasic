// FILE: Interpreter.Parser.Stmt.Loops.cs

using System.Collections.Immutable;

// ReSharper disable UnusedParameter.Local

namespace Basic;

/// <summary>
/// Contiene le versioni dei metodi di parsing per i cicli, specifiche per la Fase 1 (Parsing Puro).
/// L'obiettivo è costruire la struttura dell'AST, inclusa la gestione degli scope,
/// senza registrare simboli.
/// </summary>
public sealed partial class Parser
{
    // DICHIARA nella classe Parser:
    private readonly Stack<Token> _loopStartLabels = new();

    // Record interno per tenere traccia delle etichette dei cicli nominati dall'utente.
    private record UserLoopLabel(Token LabelToken, Token ExitLabel, Token ContinueLabel, TokenKind LoopKind);

    // Stack per tenere traccia delle etichette di uscita (per EXIT).
    private readonly Stack<Token> _loopExitLabels = new();

    // Stack per tenere traccia delle etichette di continuazione (per CONTINUE).
    private readonly Stack<Token> _loopContinueLabels = new();

    // Stack per tracciare le variabili dei cicli FOR e validare le clausole NEXT.
    private readonly Stack<Token> _forLoopVariables = new();

    // Stack dei kind dei loop (DO / WHILE / FOR) per validare EXIT/CONTINUE <kind>.
    private readonly Stack<Token> _loopKindTokens = new();

    // NUOVO: Stack per le etichette dei cicli definite dall'utente.
    private readonly Stack<UserLoopLabel> _userLoopLabelStack = new();


    /// <summary>
    /// Genera un token etichetta univoco per i salti all'interno dei cicli.
    /// Usa la posizione del token del loop (es. FOR, DO, WHILE) invece di
    /// Peek().Position per evitare che i label sintetici vengano assegnati
    /// alla linea sbagliata (es. DO → body invece di DO stesso).
    /// </summary>
    private Token GenerateLoopLabel(string prefix, SourcePosition loopPosition)
    {
        var labelId = _program.GetNextLabelId();
        var labelName = $"___{prefix}_{labelId}";
        return new Token(TokenKind.Identifier, labelName, null, loopPosition);
    }

    /// <summary>
    /// [FASE 1] Dispatcher per FOR e FOR EACH.
    /// </summary>
    private List<Stmt> ParseForStatement(ProcedureDefinition? procedureContext)
    {
        //Console.WriteLine($"[ParseForStatement] ENTER. Current: {Peek()}");
        var isForEach = At(1).Kind == TokenKind.Each;
        var result = isForEach
            ? ParseForEachStatement(procedureContext)
            : ParseForNextSetup(procedureContext);
        //Console.WriteLine($"[ParseForStatement] EXIT. Success. Next: {Peek()}");
        return result;
    }

    /// <summary>
    /// [FASE 1] Analizza l'header di un FOR...NEXT, rilevando autonomamente l'etichetta.
    /// </summary>
    private List<Stmt> ParseForNextSetup(ProcedureDefinition? procedureContext)
    {
        //Console.WriteLine($"[ParseForNextSetup] ENTER. Current: {Peek()}");

        Token? userLabelToken = null;
        if (_previousToken?.Kind == TokenKind.Colon && _twoTokensBack?.Kind == TokenKind.Identifier)
        {
            userLabelToken = _twoTokensBack;
            //Console.WriteLine($"[ParseForNextSetup]   -> Detected label '{userLabelToken.Lexeme}'.");
        }

        var forToken = Consume(TokenKind.For, "Expected 'FOR'.");

        var variableToken = IsPermissiveNameToken(Peek())
            ? Advance()
            : throw CreateError(Peek(), "Expected a variable name after 'FOR'.");

        var (baseName, _) = SplitNameAndSuffix(variableToken.Lexeme);
        var cleanedToken = variableToken with { Lexeme = baseName };
        var variableTarget = new VariableTarget(-1, variableToken, cleanedToken);

        Consume(TokenKind.Equal, "Expected '=' after FOR variable.");
        var start = ParseExpression();
        Consume(TokenKind.To, "Expected 'TO' after FOR start value.");
        var end = ParseExpression();
        var step = Match(TokenKind.Step) ? ParseExpression() : null;

        var loopStartLabel = GenerateLoopLabel("FOR_START", forToken.Position);
        var loopContinueLabel = GenerateLoopLabel("FOR_CONTINUE", forToken.Position);
        var loopExitLabel = GenerateLoopLabel("FOR_EXIT", forToken.Position);

        _forLoopVariables.Push(variableToken);
        _loopStartLabels.Push(loopStartLabel);
        _loopContinueLabels.Push(loopContinueLabel);
        _loopExitLabels.Push(loopExitLabel);
        _loopKindTokens.Push(forToken);
        if (userLabelToken != null)
        {
            _userLoopLabelStack.Push(new UserLoopLabel(userLabelToken, loopExitLabel, loopContinueLabel,
                forToken.Kind));
        }

        //Console.WriteLine($"[ParseForNextSetup] EXIT. Success. Next: {Peek()}");
        return
        [
            new EnterLoopStmt(forToken, loopContinueLabel, loopExitLabel),
            new ForStmt(variableTarget, start, end, step),
            new LabelStmt(loopStartLabel),
            new ForCheckAndJumpStmt(variableToken, loopExitLabel)
        ];
    }

    /// <summary>
    /// [FASE 1] Parsa un'istruzione NEXT, gestendo la pulizia degli stack.
    /// </summary>
    private List<Stmt> ParseNextStatement()
    {
        //Console.WriteLine($"[ParseNextStatement] ENTER. Current: {Peek()}");
        var nextToken = Consume(TokenKind.Next, "Expected 'NEXT'.");
        var statements = new List<Stmt>();

        var varsToClose = new List<Token>();
        if (IsAtEndOfStatement())
        {
            if (_forLoopVariables.Count == 0)
                throw CreateError(nextToken, "NEXT without a matching FOR.");
            varsToClose.Add(_forLoopVariables.Peek());
        }
        else
        {
            do
            {
                varsToClose.Add(IsPermissiveNameToken(Peek())
                    ? Advance()
                    : throw CreateError(Peek(), "Expected a variable name after NEXT."));
            } while (Match(TokenKind.Comma));
        }

        foreach (var nextVar in varsToClose)
        {
            if (_forLoopVariables.Count == 0)
                throw CreateError(nextVar, $"NEXT variable '{nextVar.Lexeme}' does not have a matching FOR.");

            var forVar = _forLoopVariables.Peek();
            if (!forVar.Lexeme.Equals(nextVar.Lexeme, StringComparison.OrdinalIgnoreCase))
                throw CreateError(nextVar,
                    $"NEXT variable '{nextVar.Lexeme}' does not match the open FOR loop for '{forVar.Lexeme}'.");

            _forLoopVariables.Pop();
            _loopKindTokens.Pop();
            var loopStart = _loopStartLabels.Pop();
            var loopContinue = _loopContinueLabels.Pop();
            var loopExit = _loopExitLabels.Pop();

            if (_userLoopLabelStack.Count > 0 && _userLoopLabelStack.Peek().ExitLabel == loopExit)
            {
                _userLoopLabelStack.Pop();
            }

            statements.Add(new LabelStmt(loopContinue));
            statements.Add(new NextStmt(forVar));
            statements.Add(new JumpStmt(loopStart));
            statements.Add(new ExitLoopStmt(nextToken));
            statements.Add(new LabelStmt(loopExit));
        }

        //Console.WriteLine($"[ParseNextStatement] EXIT. Success. Next: {Peek()}");
        return statements;
    }

    /// <summary>
    /// Helper per determinare se un ciclo è single-line.
    /// </summary>
    private bool IsSingleLineLoop(TokenKind endKind)
    {
        var i = _current;
        while (i < _tokens.Length)
        {
            var tk = _tokens[i];
            if (tk.Kind is TokenKind.Newline or TokenKind.Eof) return false;
            if (tk.Kind == endKind) return true;
            if (tk.Kind == TokenKind.Rem)
            {
                while (i < _tokens.Length && _tokens[i].Kind != TokenKind.Newline && _tokens[i].Kind != TokenKind.Eof)
                {
                    i++;
                }

                continue;
            }

            i++;
        }

        return false;
    }

    /// <summary>
    /// [FASE 1 - RIPRISTINATO E CORRETTO] Analizza un blocco FOR EACH, rilevando l'etichetta
    /// e gestendo correttamente sia la sintassi single-line che multi-line.
    /// </summary>
    private List<Stmt> ParseForEachStatement(ProcedureDefinition? procedureContext)
    {
        //Console.WriteLine($"[ParseForEachStatement] ENTER. Current: {Peek()}");

        Token? userLabelToken = null;
        if (_previousToken?.Kind == TokenKind.Colon && _twoTokensBack?.Kind == TokenKind.Identifier)
        {
            userLabelToken = _twoTokensBack;
            //Console.WriteLine($"[ParseForEachStatement]   -> Detected label '{userLabelToken.Lexeme}'.");
        }

        var forToken = Consume(TokenKind.For, "Expected 'FOR'.");
        Consume(TokenKind.Each, "Expected 'EACH'.");

        // FOR EACH KEY k IN dict / FOR EACH VALUE v IN dict: zucchero sintattico riscritto
        // internamente nella forma a 2 variabili già supportata da
        // VisitForEachAdvanceAndAssignStatement (estrazione Key/Value su OP_LOAD_FIELD) usando
        // una variabile sintetica, mai referenziata, per lo slot non richiesto — zero nuovo
        // codice AST/Compiler/VM. Il lookahead su At(1) evita di rompere il caso (già valido
        // oggi) in cui "KEY"/"VALUE" siano usati come nome di variabile bare seguito
        // direttamente da IN/virgola/AS.
        var isKeyOnly = Check(TokenKind.Key) && At(1).Kind is not (TokenKind.In or TokenKind.Comma or TokenKind.As);
        var isValueOnly = !isKeyOnly && Check(TokenKind.Value) &&
                           At(1).Kind is not (TokenKind.In or TokenKind.Comma or TokenKind.As);

        var iteratorVariables = ImmutableArray.CreateBuilder<Token>();
        if (isKeyOnly || isValueOnly)
        {
            Advance(); // consuma KEY/VALUE
            var userVar = ConsumeProcedureName();
            var syntheticVar = new Token(TokenKind.Identifier, $"___fe_kv_{_program.GetNextLabelId()}", null, userVar.Position);
            if (isKeyOnly) { iteratorVariables.Add(userVar); iteratorVariables.Add(syntheticVar); }
            else { iteratorVariables.Add(syntheticVar); iteratorVariables.Add(userVar); }
        }
        else
        {
            do
            {
                iteratorVariables.Add(ConsumeProcedureName());
            } while (Match(TokenKind.Comma) && !Check(TokenKind.In) && !Check(TokenKind.As));
        }

        DataType? declaredType = null;
        if (Match(TokenKind.As))
            declaredType = ParseTypeName();

        Consume(TokenKind.In, "Expected 'IN' after iterator variable(s).");
        var collectionExpr = ParseExpression();

        var isSingleLine = IsSingleLineLoop(TokenKind.Next);

        if (!isSingleLine && !IsAtEnd())
        {
            Consume(TokenKind.Newline, "Expected newline after FOR EACH statement.");
        }

        var loopStartLabel = GenerateLoopLabel("FOREACH_START", forToken.Position);
        var loopContinueLabel = GenerateLoopLabel("FOREACH_CONTINUE", forToken.Position);
        var loopExitLabel = GenerateLoopLabel("FOREACH_EXIT", forToken.Position);

        _loopKindTokens.Push(forToken);
        _loopStartLabels.Push(loopStartLabel);
        _loopContinueLabels.Push(loopContinueLabel);
        _loopExitLabels.Push(loopExitLabel);
        if (userLabelToken != null)
        {
            _userLoopLabelStack.Push(new UserLoopLabel(userLabelToken, loopExitLabel, loopContinueLabel,
                forToken.Kind));
        }

        var statements = new List<Stmt>
        {
            new EnterLoopStmt(forToken, loopContinueLabel, loopExitLabel),
            new ForEachInitStmt(forToken, iteratorVariables.ToImmutable(), declaredType, collectionExpr),
            new LabelStmt(loopStartLabel),
            new ForEachAdvanceAndAssignStmt(iteratorVariables.ToImmutable(), loopExitLabel)
        };

        // --- RIPRISTINO LOGICA DI PARSING DEL CORPO BIFORCATA ---
        if (isSingleLine)
        {
            while (!Check(TokenKind.Next) && !IsAtEnd())
            {
                if (Match(TokenKind.Colon)) continue;
                statements.AddRange(ParseStatement(procedureContext).OfType<Stmt>());
            }
        }
        else
        {
            var body = ParseBlockBodyUntil(procedureContext, TokenKind.Next);
            statements.AddRange(body);
        }
        // --- FINE RIPRISTINO ---

        var nextToken = Consume(TokenKind.Next, "Expected 'NEXT' to close FOR EACH block.");

        // NEXT può nominare le variabili del ciclo (`FOR EACH x IN ...` / `NEXT x`), come
        // il FOR classico e come il parser C. Prima il nome dopo NEXT restava un token
        // orfano ("Token 'x' inatteso all'inizio di un'istruzione") e il file non compilava
        // sulle gambe gestite (IL/PCode/VM-C), mentre C99Native lo accettava.
        if (!IsAtEndOfStatement() && IsPermissiveNameToken(Peek()))
        {
            do
            {
                Advance();
            } while (Match(TokenKind.Comma) && IsPermissiveNameToken(Peek()));
        }

        statements.Add(new LabelStmt(loopContinueLabel));
        statements.Add(new JumpStmt(loopStartLabel));
        statements.Add(new LabelStmt(loopExitLabel));
        statements.Add(new ExitLoopStmt(nextToken));

        _loopExitLabels.Pop();
        _loopContinueLabels.Pop();
        _loopStartLabels.Pop();
        _loopKindTokens.Pop();

        if (userLabelToken != null && _userLoopLabelStack.Count > 0 &&
            _userLoopLabelStack.Peek().LabelToken == userLabelToken)
        {
            _userLoopLabelStack.Pop();
        }

        //Console.WriteLine($"[ParseForEachStatement] EXIT. Success. Next: {Peek()}");
        return statements;
    }

    /// <summary>
    /// [FASE 1] Parsa l'header di un ciclo DO, rilevando l'etichetta.
    /// </summary>
    private List<Stmt> ParseDoHeader(ProcedureDefinition? procedureContext)
    {
        //Console.WriteLine($"[ParseDoHeader] ENTER. Current: {Peek()}");

        Token? userLabelToken = null;
        if (_previousToken?.Kind == TokenKind.Colon && _twoTokensBack?.Kind == TokenKind.Identifier)
        {
            userLabelToken = _twoTokensBack;
            //Console.WriteLine($"[ParseDoHeader]   -> Detected label '{userLabelToken.Lexeme}'.");
        }

        var doToken = Consume(TokenKind.Do, "Expected 'DO'.");

        var loopStartLabel = GenerateLoopLabel("DO_START", doToken.Position);
        var loopContinueLabel = GenerateLoopLabel("DO_CONTINUE", doToken.Position);
        var loopExitLabel = GenerateLoopLabel("DO_EXIT", doToken.Position);

        var isPreTest = Match(TokenKind.While, TokenKind.Until);
        var preIsUntil = isPreTest && Previous().Kind == TokenKind.Until;
        var preCondition = isPreTest ? ParseExpression() : null;

        // NOTA: loopContinueLabel NON viene impostato = loopStartLabel per i pre-test.
        // Se fossero uguali, il LabelStmt(loopContinueLabel) in ParseLoopStatement
        // sovrascriverebbe loopStartLabel nella SymbolTable con la linea della LOOP,
        // causando loop infinito nei jump-back.

        _loopKindTokens.Push(doToken);
        _loopStartLabels.Push(loopStartLabel);
        _loopContinueLabels.Push(loopContinueLabel);
        _loopExitLabels.Push(loopExitLabel);
        if (userLabelToken != null)
        {
            _userLoopLabelStack.Push(new UserLoopLabel(userLabelToken, loopExitLabel, loopContinueLabel, doToken.Kind));
        }

        var statements = new List<Stmt>
        {
            new EnterLoopStmt(doToken, loopContinueLabel, loopExitLabel),
            new LabelStmt(loopStartLabel)
        };

        if (isPreTest && preCondition != null)
        {
            statements.Add(new ConditionalJumpStmt(preCondition, loopExitLabel, preIsUntil));
        }

        //Console.WriteLine($"[ParseDoHeader] EXIT. Success. Next: {Peek()}");
        return statements;
    }

    /// <summary>
    /// [FASE 1] Parsa l'istruzione LOOP, gestendo la pulizia degli stack.
    /// </summary>
    private List<Stmt> ParseLoopStatement()
    {
        //Console.WriteLine($"[ParseLoopStatement] ENTER. Current: {Peek()}");

        var loopToken = Consume(TokenKind.Loop, "Expected 'LOOP'.");

        if (_loopKindTokens.Count == 0 || _loopKindTokens.Peek().Kind != TokenKind.Do)
            throw CreateError(loopToken, "LOOP without a matching DO.");

        _loopKindTokens.Pop();
        var loopStartLabel = _loopStartLabels.Pop();
        var loopContinueLabel = _loopContinueLabels.Pop();
        var loopExitLabel = _loopExitLabels.Pop();

        if (_userLoopLabelStack.Count > 0 && _userLoopLabelStack.Peek().ExitLabel == loopExitLabel)
        {
            _userLoopLabelStack.Pop();
        }

        var statements = new List<Stmt> { new LabelStmt(loopContinueLabel) };

        var hasPostTest = Match(TokenKind.While, TokenKind.Until);
        if (hasPostTest)
        {
            var isUntil = Previous().Kind == TokenKind.Until;
            var postCondition = ParseExpression();
            statements.Add(new ConditionalJumpStmt(postCondition, loopStartLabel, !isUntil));
        }
        else
        {
            statements.Add(new JumpStmt(loopStartLabel));
        }

        statements.Add(new ExitLoopStmt(loopToken));
        statements.Add(new LabelStmt(loopExitLabel));
        //Console.WriteLine($"[ParseLoopStatement] EXIT. Success. Next: {Peek()}");
        return statements;
    }

    /// <summary>
    /// [FASE 1] Parsa l'header di un ciclo WHILE, rilevando l'etichetta.
    /// </summary>
    private List<Stmt> ParseWhileHeader(ProcedureDefinition? procedureContext)
    {
        //Console.WriteLine($"[ParseWhileHeader] ENTER. Current: {Peek()}");

        Token? userLabelToken = null;
        if (_previousToken?.Kind == TokenKind.Colon && _twoTokensBack?.Kind == TokenKind.Identifier)
        {
            userLabelToken = _twoTokensBack;
            //Console.WriteLine($"[ParseWhileHeader]   -> Detected label '{userLabelToken.Lexeme}'.");
        }

        var whileToken = Consume(TokenKind.While, "Expected 'WHILE'.");
        var condition = ParseExpression();

        var loopStartLabel = GenerateLoopLabel("WHILE_START", whileToken.Position);
        var loopExitLabel = GenerateLoopLabel("WHILE_EXIT", whileToken.Position);

        _loopKindTokens.Push(whileToken);
        _loopStartLabels.Push(loopStartLabel);
        _loopContinueLabels.Push(loopStartLabel);
        _loopExitLabels.Push(loopExitLabel);
        if (userLabelToken != null)
        {
            _userLoopLabelStack.Push(new UserLoopLabel(userLabelToken, loopExitLabel, loopStartLabel,
                whileToken.Kind));
        }

        //Console.WriteLine($"[ParseWhileHeader] EXIT. Success. Next: {Peek()}");
        return
        [
            new EnterLoopStmt(whileToken, loopStartLabel, loopExitLabel),
            new LabelStmt(loopStartLabel),
            new ConditionalJumpStmt(condition, loopExitLabel, false)
        ];
    }

    /// <summary>
    /// [FASE 1] Parsa l'istruzione WEND, gestendo la pulizia degli stack.
    /// </summary>
    private List<Stmt> ParseWendStatement()
    {
        //Console.WriteLine($"[ParseWendStatement] ENTER. Current: {Peek()}");
        var wendToken = Consume(TokenKind.Wend, "Expected 'WEND'.");

        if (_loopKindTokens.Count == 0 || _loopKindTokens.Peek().Kind != TokenKind.While)
            throw CreateError(wendToken, "WEND without a matching WHILE.");

        _loopKindTokens.Pop();
        var loopStartLabel = _loopStartLabels.Pop();
        _loopContinueLabels.Pop();
        var loopExitLabel = _loopExitLabels.Pop();

        if (_userLoopLabelStack.Count > 0 && _userLoopLabelStack.Peek().ExitLabel == loopExitLabel)
        {
            _userLoopLabelStack.Pop();
        }

        //Console.WriteLine($"[ParseWendStatement] EXIT. Success. Next: {Peek()}");
        return
        [
            new JumpStmt(loopStartLabel),
            new ExitLoopStmt(wendToken),
            new LabelStmt(loopExitLabel)
        ];
    }

    /// <summary>
    /// [FASE 1] Parsa CONTINUE [etichetta | TIPO_LOOP].
    /// </summary>
    private ContinueStmt ParseContinueStatement()
    {
        //Console.WriteLine($"[ParseContinueStatement] ENTER. Current: {Peek()}");
        var continueToken = Consume(TokenKind.Continue, "Expected 'CONTINUE'.");

        if (IsAtEndOfStatement())
        {
            return _loopContinueLabels.Count == 0 ? throw CreateError(continueToken, "CONTINUE can only be used inside a loop.") :
                //Console.WriteLine($"[ParseContinueStatement] EXIT (simple). Success. Next: {Peek()}");
                new ContinueStmt(continueToken, _loopContinueLabels.Peek());
        }

        var targetToken = Advance();

        switch (targetToken.Kind)
        {
            case TokenKind.Identifier:
            {
                var loopContext = _userLoopLabelStack.FirstOrDefault(l =>
                    l.LabelToken.Lexeme.Equals(targetToken.Lexeme, StringComparison.OrdinalIgnoreCase));
                if (loopContext != null)
                {
                    //Console.WriteLine($"[ParseContinueStatement] EXIT (labeled). Success. Next: {Peek()}");
                    return new ContinueStmt(continueToken, loopContext.ContinueLabel);
                }

                break;
            }
            case TokenKind.For or TokenKind.Do or TokenKind.While when _loopKindTokens.Count == 0 || _loopKindTokens.Peek().Kind != targetToken.Kind:
                throw CreateError(continueToken,
                    $"CONTINUE {targetToken.Lexeme.ToUpper()} does not match the innermost loop type.");
            //Console.WriteLine($"[ParseContinueStatement] EXIT (typed). Success. Next: {Peek()}");
            case TokenKind.For or TokenKind.Do or TokenKind.While:
                return new ContinueStmt(continueToken, _loopContinueLabels.Peek());
        }

        throw CreateError(targetToken, "Expected a loop type (FOR, DO, WHILE) or a loop label after CONTINUE.");
    }

    /// <summary>
    /// [FASE 1] Parsa EXIT [etichetta | TIPO_LOOP | FUNCTION].
    /// </summary>
    private ExitStmt ParseExitStatement()
    {
        //Console.WriteLine($"[ParseExitStatement] ENTER. Current: {Peek()}");
        var exitToken = Consume(TokenKind.Exit, "Expected 'EXIT'.");

        if (IsAtEndOfStatement())
        {
            if (_loopKindTokens.Count > 0)
            {
                //Console.WriteLine($"[ParseExitStatement] EXIT (simple loop). Success. Next: {Peek()}");
                return new ExitStmt(exitToken, _loopKindTokens.Peek(), _loopExitLabels.Peek());
            }

            if (_functionContextStack.Count > 0)
            {
                //Console.WriteLine($"[ParseExitStatement] EXIT (simple function). Success. Next: {Peek()}");
                return new ExitStmt(exitToken, new Token(TokenKind.Function, "FUNCTION", null, exitToken.Position),
                    null);
            }

            throw CreateError(exitToken, "EXIT can only be used inside a loop or a procedure.");
        }

        var targetToken = Advance();

        switch (targetToken.Kind)
        {
            case TokenKind.Identifier:
            {
                var loopContext = _userLoopLabelStack.FirstOrDefault(l =>
                    l.LabelToken.Lexeme.Equals(targetToken.Lexeme, StringComparison.OrdinalIgnoreCase));
                if (loopContext != null)
                {
                    var loopKindToken = new Token(loopContext.LoopKind, loopContext.LoopKind.ToString().ToUpper(), null,
                        targetToken.Position);
                    //Console.WriteLine($"[ParseExitStatement] EXIT (labeled). Success. Next: {Peek()}");
                    return new ExitStmt(exitToken, loopKindToken, loopContext.ExitLabel);
                }

                break;
            }
            case TokenKind.For or TokenKind.Do or TokenKind.While or TokenKind.Tick or TokenKind.Function:
            {
                Token? exitLabel = null;
                /* EXIT TICK: come FOR/DO/WHILE, ha un'etichetta di uscita (TICK_EXIT).
                 * Prima restituiva TargetLabel=null → il compilatore emetteva un RETURN e
                 * il loop TICK continuava per sempre (hang P-Code su "EXIT TICK"). */
                if (targetToken.Kind == TokenKind.Tick)
                {
                    if (_loopKindTokens.Count > 0 && _loopKindTokens.Peek().Kind == TokenKind.Tick)
                        exitLabel = _loopExitLabels.Peek();
                    return new ExitStmt(exitToken, targetToken, exitLabel);
                }
                if (targetToken.Kind is not (TokenKind.For or TokenKind.Do or TokenKind.While))
                    return new ExitStmt(exitToken, targetToken, exitLabel);
                if (_loopKindTokens.Count == 0 || _loopKindTokens.Peek().Kind != targetToken.Kind)
                    throw CreateError(targetToken,
                        $"EXIT {targetToken.Lexeme.ToUpper()} does not match innermost loop.");
                exitLabel = _loopExitLabels.Peek();

                //Console.WriteLine($"[ParseExitStatement] EXIT (typed). Success. Next: {Peek()}");
                return new ExitStmt(exitToken, targetToken, exitLabel);
            }
        }

        throw CreateError(targetToken, "Expected a loop type (FOR, DO, WHILE), FUNCTION, or a loop label after EXIT.");
    }
}