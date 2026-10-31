// FILE: Interpreter.Parser.Stmt.Branches.cs

using System.Collections.Immutable;

// ReSharper disable InvalidXmlDocComment

namespace Basic;

/// <summary>
/// Contiene i metodi per il parsing dei costrutti di controllo di flusso
/// decisionali come IF e SELECT CASE.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parser della FASE 1 per l'istruzione IF in GM-BASIC.
    /// Gestisce tutte le varianti single-line e multi-line, inclusi:
    /// - Salti impliciti (numeri di riga / etichette) dopo THEN o ELSE.
    /// - Segmenti multipli separati da due punti (:).
    /// - THEN ed ELSE vuoti.
    /// - Forme senza THEN (dialetto permissivo: IF <cond> <stmt>).
    /// - Forme con GOTO/GOSUB espliciti (IF <cond> GOTO/GOSUB <target>).
    /// - Blocchi multi-riga con ELSEIF / ELSE / END IF.
    /// Riconosce automaticamente il formato (single-line vs multi-line) in base al token immediatamente successivo a THEN:
    /// - Multi-line se THEN è seguito da NEWLINE o EOF.
    /// - Single-line in tutti gli altri casi.
    /// </summary>
    /// <remarks>
    /// LOGICA DI RICONOSCIMENTO:
    /// 1. Lookahead: IF <expr> THEN (newline|EOF) ⇒ blocco multi-riga; altrimenti single-line.
    /// 2. Single-line:
    ///    - Consuma eventuali ':' consecutivi subito dopo THEN (THEN vuoto).
    ///    - Ogni segmento separato da ':' può essere:
    ///       a) Salto implicito (numero di riga / etichetta isolata).
    ///       b) Istruzione normale (parsed via ParseStatement).
    ///       c) Vuoto (colon multipli o colon prima di ELSE).
    ///    - Caso THEN vuoto: IF cond THEN : ELSE ... / IF cond THEN : <newline>
    ///    - Il ramo ELSE single-line segue regole simmetriche (colon iniziali ⇒ ELSE vuoto).
    /// 3. Salto implicito:
    ///    - Un token Identifier / Number / LineNumber seguito da uno dei terminatori
    ///      (Colon, Else, Newline, Eof) viene convertito in GotoStmt.
    ///    - Gestisce anche varianti: THEN label : ELSE label2 / THEN 220 : ELSE 330.
    /// 4. Espliciti GOTO/GOSUB:
    ///    - IF <cond> GOTO <target>
    ///    - IF <cond> GOSUB <target>
    ///    - Nel ramo single-line vengono trattati come THEN contenente un singolo jump.
    /// 5. Multi-riga:
    ///    - Struttura: IF <cond> THEN <NL> <corpo> [ELSEIF <cond> THEN <NL> <corpo>]* [ELSE <NL> <corpo>] END IF.
    ///    - Ogni corpo è una sequenza di istruzioni fino al prossimo marcatore (ELSEIF / ELSE / END IF).
    ///
    /// GESTIONE DEI CASI VUOTI:
    /// - THEN vuoto single-line: IF cond THEN <terminatore>
    /// - THEN vuoto con colon:  IF cond THEN : ELSE stmt
    /// - ELSE vuoto con colon multipli: IF cond THEN stmt ELSE : : <newline>
    /// - Multi-riga non supporta “vuoto esplicito” con colon (lo stile è sempre a blocco).
    ///
    /// OPERATORI ALTERNATIVI NELLE CONDIZIONI:
    /// - Supporta =<, =>, >< oltre a <=, >=, <> e != (fusi a livello parser in LessEqual, GreaterEqual, NotEqual).
    ///
    /// DIFFERENZE TOKEN:
    /// - LineNumber: solo se il numero è il primo token non-whitespace della riga logica (salti impliciti).
    /// - Number: letterale numerico in espressioni (non usato come target di salto implicito se non isolato).
    /// - Identifier: variabile, etichetta potenziale di salto implicito se isolata.
    ///
    /// COLON (:) COME SEPARATORE:
    /// - Multipli consecutivi vengono ignorati (slot vuoti).
    /// - Non generano nodi AST.
    /// - Non attivano lookahead di assegnazione (filtrati prima del parse di target).
    ///
    /// ERRORI EVITATI:
    /// - “Token ':' inatteso…”: il colon non viene più passato come inizio statement.
    /// - “Expected END IF…”: corretta distinzione single-line vs multi-riga.
    /// - “Token '220' inatteso…”: numeri di riga dopo THEN/ELSE riconosciuti come salti.
    /// </remarks>
    /// <example>
    /// Single-line base:
    /// IF A > B THEN PRINT "Maggiore"
    /// </example>
    /// <example>
    /// Single-line con ELSE:
    /// IF A > B THEN PRINT "A" ELSE PRINT "B"
    /// </example>
    /// <example>
    /// THEN multi-istruzione (segmenti con colon):
    /// IF A < B THEN PRINT "Primo" : PRINT "Secondo" : PRINT "Terzo"
    /// </example>
    /// <example>
    /// THEN vuoto:
    /// IF A > B THEN
    /// (equivalente a ramo THEN vuoto, se newline immediato)
    /// </example>
    /// <example>
    /// THEN vuoto con colon e ELSE:
    /// IF A > B THEN : ELSE PRINT "Else eseguito"
    /// </example>
    /// <example>
    /// Colon multipli ignorati:
    /// IF A > B THEN : : PRINT "Solo questa istruzione conta"
    /// </example>
    /// <example>
    /// Salto implicito a numero di riga:
    /// IF FLAG = 0 THEN 220
    /// </example>
    /// <example>
    /// Salto implicito a etichetta:
    /// IF FLAG = 0 THEN StartLoop
    /// </example>
    /// <example>
    /// Salti impliciti in THEN ed ELSE:
    /// IF A < B THEN 500 ELSE 600
    /// </example>
    /// <example>
    /// GOTO esplicito:
    /// IF A < B GOTO ExitPoint
    /// </example>
    /// <example>
    /// GOSUB esplicito:
    /// IF Ready GOSUB InitSub
    /// </example>
    /// <example>
    /// THEN con salto implicito seguito da altre istruzioni (non valido: il salto chiude il segmento):
    /// IF A < B THEN 200 : PRINT "Questa non verrà eseguita se il salto è preso"
    /// (La PRINT rimane nello stesso ramo ma l'esecuzione runtime salta prima.)
    /// </example>
    /// <example>
    /// ELSE con salto implicito:
    /// IF A < B THEN PRINT "Ok" ELSE 900
    /// </example>
    /// <example>
    /// ELSE vuoto:
    /// IF A < B THEN PRINT "Solo THEN"
    /// (Se il testo sorgente fosse: IF A < B THEN PRINT "Solo THEN" ELSE :
    /// il ramo ELSE è vuoto.)
    /// </example>
    /// <example>
    /// Multi-riga completo:
    /// IF A < B THEN
    ///   PRINT "THEN 1"
    ///   PRINT "THEN 2"
    /// ELSEIF A = B THEN
    ///   PRINT "ELSEIF"
    /// ELSE
    ///   PRINT "ELSE"
    /// END IF
    /// </example>
    /// <example>
    /// Operatori alternativi:
    /// IF A =< B THEN PRINT "=< OK"
    /// IF A => B THEN PRINT "=> OK"
    /// IF A >< B THEN PRINT ">< OK"
    /// IF A != B THEN PRINT "!= OK"
    /// </example>
    /// <example>
    /// Dialetto permissivo senza THEN:
    /// IF A < B PRINT "Forma senza THEN"
    /// </example>
    /// <example>
    /// THEN vuoto + colon + statement:
    /// IF A = 10 THEN : PRINT "Statement dopo THEN vuoto"
    /// </example>
    /// <example>
    /// Sequenza con colon chiave:
    /// IF A = 10 THEN : : PRINT "Statement dopo colon multipli"
    /// </example>
    /// <example>
    /// GOSUB + ritorno:
    /// IF A > 0 THEN GOSUB SubRoutine : PRINT "Dopo il ritorno"
    /// </example>
    /// <example>
    /// Combinazione avanzata:
    /// IF A < B THEN 220 : ELSE : PRINT "ELSE eseguito"
    /// </example>
    private IfStmt ParseIfStatement(ProcedureDefinition? procedureContext)
    {
        var ifToken = Consume(TokenKind.If, "Internal parser error: Expected 'IF'.");
        var condition = ParseExpression();

        // Lookahead multi-riga: THEN seguito da NEWLINE/EOF
        var saved = _current;
        var isMultiLine = Match(TokenKind.Then) && (Check(TokenKind.Newline) || IsAtEnd());
        _current = saved;

        if (isMultiLine)
        {
            Consume(TokenKind.Then, "Expected 'THEN' in multi-line IF.");
            Consume(TokenKind.Newline, "Expected newline after IF...THEN.");
            var clauses = ImmutableArray.CreateBuilder<IfClause>();
            var thenStmtsBlock =
                ParseBlockBodyUntil(procedureContext, TokenKind.ElseIf, TokenKind.Else, TokenKind.EndIf);
            clauses.Add(new IfClause(condition, thenStmtsBlock, ifToken));
            ElseClause? elseBlock = null;
            while (Match(TokenKind.ElseIf))
            {
                var elifTok = Previous();
                var elifCond = ParseExpression();
                Consume(TokenKind.Then, "Expected THEN after ELSEIF.");
                Consume(TokenKind.Newline, "Expected newline after ELSEIF...THEN.");
                var elifStmts = ParseBlockBodyUntil(procedureContext, TokenKind.ElseIf, TokenKind.Else,
                    TokenKind.EndIf);
                clauses.Add(new IfClause(elifCond, elifStmts, elifTok));
            }

            if (Match(TokenKind.Else))
            {
                var elseTok = Previous();
                Consume(TokenKind.Newline, "Expected newline after ELSE.");
                var elseStmts = ParseBlockBodyUntil(procedureContext, TokenKind.EndIf);
                elseBlock = new ElseClause(elseStmts, elseTok);
            }

            Consume(TokenKind.EndIf, "Expected 'END IF' to close the multi-line block.");
            return new IfStmt(clauses.ToImmutable(), elseBlock, ifToken);
        }

        // -------- SINGLE-LINE --------
        var thenBody = new List<Stmt>();

        if (Match(TokenKind.Then))
        {
            var thenToken = Previous();

            // 1. Consuma colon iniziali: THEN : : :  (THEN vuoto)
            while (Match(TokenKind.Colon))
            {
                /* THEN vuoto */
            }

            // 2. Se subito dopo c'è ELSE / NEWLINE / EOF → THEN vuoto, non aggiungere nulla
            if (!(Check(TokenKind.Else) || Check(TokenKind.Newline) || IsAtEnd()))
            {
                // 3. Parsa segmenti THEN (incluso caso THEN : PRINT "X")
                while (true)
                {
                    // Salto implicito (label / numero isolato)
                    var la1 = At(1).Kind;
                    if (Peek().Kind is TokenKind.Identifier or TokenKind.Number or TokenKind.LineNumber &&
                        la1 is TokenKind.Colon or TokenKind.Else or TokenKind.Newline or TokenKind.Eof)
                    {
                        thenBody.Add(new GotoStmt(Advance(), thenToken));
                    }
                    else
                    {
                        thenBody.AddRange(ParseStatement(procedureContext).OfType<Stmt>());
                    }

                    // Consuma colon multipli tra istruzioni
                    if (!Match(TokenKind.Colon))
                        break;
                    while (Match(TokenKind.Colon))
                    {
                        /* separatori vuoti consecutivi */
                    }

                    if (Check(TokenKind.Else) || Check(TokenKind.Newline) || IsAtEnd())
                        break;
                }
            }
        }
        else if (Match(TokenKind.Goto, TokenKind.Gosub))
        {
            var jumpKeyword = Previous();
            Token target;
            if (Match(TokenKind.Identifier) || Match(TokenKind.LineNumber) || Match(TokenKind.Number))
                target = Previous();
            else
                throw CreateError(Peek(), "Expected target after GOTO/GOSUB.");
            thenBody.Add(jumpKeyword.Kind == TokenKind.Goto
                ? new GotoStmt(target, jumpKeyword)
                : new GosubStmt(target, jumpKeyword));
        }
        else
        {
            // IF <cond> <stmt> (dialetto senza THEN)
            thenBody.AddRange(ParseStatement(procedureContext).OfType<Stmt>());
        }

        var ifClause = new IfClause(condition, [.. thenBody], ifToken);

        // Forma mista: THEN inline seguito da ELSE/ELSEIF/END IF su una riga separata
        // (es. "IF c THEN PRINT "A"" poi "ELSE PRINT "B"" poi "END IF"). Senza questo controllo
        // ELSE/END IF restano token non consumati e il ciclo di parsing del blocco chiamante
        // non avanza mai (loop infinito). Rilevabile guardando oltre i NEWLINE che seguono il
        // corpo THEN inline: se il primo token non-NEWLINE è ELSE/ELSEIF/END IF, trattiamo
        // l'intero costrutto come la forma multilinea, riusando il corpo THEN già parsato.
        var mixedLookahead = 0;
        while (At(mixedLookahead).Kind == TokenKind.Newline)
            mixedLookahead++;
        // La forma mista è valida SOLO a top-level di un blocco. Se questo IF inline è DENTRO
        // il corpo di un blocco multilinea (_insideMultiLineBlock), l'END IF/ELSE che segue
        // appartiene al blocco ESTERNO — non consumarlo (rompeva la struttura → "END SUB inatteso").
        if (!_insideMultiLineBlock &&
            mixedLookahead > 0 &&
            At(mixedLookahead).Kind is TokenKind.Else or TokenKind.ElseIf or TokenKind.EndIf)
        {
            SkipNewlines(this);

            var mixedClauses = ImmutableArray.CreateBuilder<IfClause>();
            mixedClauses.Add(ifClause);
            ElseClause? mixedElseBlock = null;

            while (Match(TokenKind.ElseIf))
            {
                var elifTok = Previous();
                var elifCond = ParseExpression();
                Consume(TokenKind.Then, "Expected THEN after ELSEIF.");
                Consume(TokenKind.Newline, "Expected newline after ELSEIF...THEN.");
                var elifStmts = ParseBlockBodyUntil(procedureContext, TokenKind.ElseIf, TokenKind.Else,
                    TokenKind.EndIf);
                mixedClauses.Add(new IfClause(elifCond, elifStmts, elifTok));
            }

            if (Match(TokenKind.Else))
            {
                var elseTok = Previous();
                if (Check(TokenKind.Newline))
                {
                    // ELSE multilinea "puro" (nessuno statement inline sulla stessa riga di ELSE).
                    Consume(TokenKind.Newline, "Expected newline after ELSE.");
                    var elseStmts = ParseBlockBodyUntil(procedureContext, TokenKind.EndIf);
                    mixedElseBlock = new ElseClause(elseStmts, elseTok);
                }
                else
                {
                    // ELSE con statement inline sulla stessa riga (es. "ELSE PRINT "NO"").
                    var mixedElseBody = new List<Stmt>();
                    while (Match(TokenKind.Colon))
                    {
                        /* ELSE vuoto iniziale */
                    }

                    if (!(Check(TokenKind.Newline) || IsAtEnd()))
                    {
                        while (true)
                        {
                            mixedElseBody.AddRange(ParseStatement(procedureContext).OfType<Stmt>());
                            if (!Match(TokenKind.Colon))
                                break;
                            while (Match(TokenKind.Colon))
                            {
                            }

                            if (Check(TokenKind.Newline) || IsAtEnd())
                                break;
                        }
                    }

                    mixedElseBlock = new ElseClause([.. mixedElseBody], elseTok);

                    // Se dopo l'ELSE inline segue END IF su una riga separata, saltiamo il newline.
                    if (Check(TokenKind.Newline) && At(1).Kind == TokenKind.EndIf)
                        SkipNewlines(this);
                }
            }

            Consume(TokenKind.EndIf, "Expected 'END IF' to close the mixed-form IF block.");
            return new IfStmt(mixedClauses.ToImmutable(), mixedElseBlock, ifToken);
        }

        // Ramo ELSE single-line (colon iniziali = ELSE vuoto)
        ElseClause? elseClauseSingle = null;
        // ReSharper disable once InvertIf
        if (Match(TokenKind.Else))
        {
            var elseTok = Previous();
            var elseBody = new List<Stmt>();

            // Consuma eventuali colon vuoti dopo ELSE
            while (Match(TokenKind.Colon))
            {
                /* ELSE vuoto iniziale */
            }

            if (!(Check(TokenKind.Newline) || IsAtEnd()))
            {
                while (true)
                {
                    var la1 = At(1).Kind;
                    if (Peek().Kind is TokenKind.Identifier or TokenKind.Number or TokenKind.LineNumber &&
                        la1 is TokenKind.Newline or TokenKind.Colon or TokenKind.Eof)
                    {
                        elseBody.Add(new GotoStmt(Advance(), elseTok));
                        break;
                    }

                    elseBody.AddRange(ParseStatement(procedureContext).OfType<Stmt>());

                    if (!Match(TokenKind.Colon))
                        break;
                    while (Match(TokenKind.Colon))
                    {
                    }

                    if (Check(TokenKind.Newline) || IsAtEnd())
                        break;
                }
            }

            elseClauseSingle = new ElseClause([.. elseBody], elseTok);
        }

        // ENDIF di chiusura sulla STESSA riga logica del THEN/ELSE inline (es.
        // "IF c THEN PRINT "a" ENDIF" o "IF c THEN PRINT "a" ELSE PRINT "b" ENDIF"): il ramo
        // "forma mista" sopra gestisce solo un ENDIF/ELSE/ELSEIF raggiungibile DOPO uno o più
        // NEWLINE (mixedLookahead > 0); qui il token segue direttamente, senza newline in
        // mezzo, quindi quel ramo non scatta mai e l'ENDIF restava non consumato — il ciclo
        // di parsing del blocco chiamante rivedeva lo stesso token EndIf a ogni iterazione
        // senza mai avanzare (loop infinito, hang). 
        // ATTENZIONE al confronto di RIGA: alcuni statement del corpo inline (es. SPRITE OFF,
        // la cui lista argomenti ParseExpressionList consuma i NEWLINE finali) possono lasciare
        // il parser OLTRE il newline di fine riga. In quel caso Check(EndIf) vedrebbe un ENDIF
        // della RIGA SUCCESSIVA — che chiude il blocco multilinea ESTERNO — e lo consumerebbe
        // ("END SUB inatteso"). Si consuma l'ENDIF SOLO se è sulla stessa riga dell'ultimo
        // token del corpo (lo stesso criterio di "segue direttamente" del fix).
        if (Check(TokenKind.EndIf) && Peek().Position.Line == Previous().Position.Line)
            Advance();

        return new IfStmt([ifClause], elseClauseSingle, ifToken);
    }

    /*/// <summary>
    /// Analizza un blocco SELECT CASE...END SELECT e le clausole interne CASE.
    /// Questo metodo fa parte della prima fase di parsing (se applicabile) o del parsing
    /// standard del blocco.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Gestire l'intera struttura di salto condizionale multi-ramo.
    /// Il metodo analizza l'espressione di controllo del SELECT e poi consuma iterativamente
    /// le clausole CASE, interpretando le varie sintassi di test:
    /// - 'CASE Valore'
    /// - 'CASE Valore1 TO Valore2' (range)
    /// - 'CASE IS > Valore' (comparazione)
    ///
    /// Logica del Blocco: Ogni clausola CASE viene parsata in un blocco di istruzioni separato
    /// fino a quando non viene incontrato il token 'CASE' successivo o 'END SELECT'.
    /// La clausola 'CASE ELSE' (se presente) deve essere l'ultima clausola del blocco.
    ///
    /// Gestione Flusso:
    /// Il metodo utilizza l'helper 'ParseBlockBodyUntil' per delegare il parsing
    /// del corpo di ogni clausola, garantendo che i confini del blocco siano rispettati.
    /// </remarks>
    private SelectStmt ParseSelectCaseStatement(ProcedureDefinition? procedureContext)
    {
        var selectToken = Consume(TokenKind.Select, "Internal parser error: Expected 'SELECT'.");
        Consume(TokenKind.Case, "Expected 'CASE' after 'SELECT'.");
        var expression = ParseExpression();
        Consume(TokenKind.Newline, "Expected newline after SELECT CASE expression.");

        var caseClauses = ImmutableArray.CreateBuilder<CaseClause>();
        CaseElseClause? elseClause = null;

        while (!Check(TokenKind.EndSelect) && !IsAtEnd())
        {
            // Salta qualsiasi riga vuota, commento o numero di riga tra le clausole.
            bool consumed;
            do
            {
                consumed = Match(TokenKind.Newline) || Match(TokenKind.LineNumber);
            } while (consumed);

            if (Check(TokenKind.EndSelect) || IsAtEnd()) break;

            if (Check(TokenKind.Case))
            {
                var caseToken = Consume(TokenKind.Case, "Internal parser error.");

                if (Match(TokenKind.Else))
                {
                    Consume(TokenKind.Newline, "Expected newline after 'CASE ELSE'.");
                    // Chiama direttamente il parser di blocchi di Fase 1.
                    var elseStatements = ParseBlockBodyUntil(procedureContext, TokenKind.EndSelect);
                    elseClause = new CaseElseClause(elseStatements, caseToken);
                    break; // Dopo CASE ELSE, il blocco SELECT termina.
                }

                var tests = ImmutableArray.CreateBuilder<CaseTest>();
                do
                {
                    if (Match(TokenKind.Is))
                    {
                        var opToken = Peek();
                        if (!Match(TokenKind.Equal, TokenKind.NotEqual, TokenKind.LessThan, TokenKind.LessEqual,
                                TokenKind.GreaterThan, TokenKind.GreaterEqual))
                            throw CreateError(opToken, "Expected a comparison operator (e.g., =, <>, >) after 'IS'.");
                        var value = ParseExpression();
                        tests.Add(new CaseComparison(opToken, value));
                    }
                    else
                    {
                        var startValue = ParseExpression();
                        if (Match(TokenKind.To))
                        {
                            var endValue = ParseExpression();
                            tests.Add(new CaseRange(startValue, endValue));
                        }
                        else
                        {
                            tests.Add(new CaseValue(startValue));
                        }
                    }
                } while (Match(TokenKind.Comma));

                Consume(TokenKind.Newline, "Expected newline after CASE conditions.");
                // Chiama direttamente il parser di blocchi di Fase 1.
                var statements = ParseBlockBodyUntil(procedureContext, TokenKind.Case, TokenKind.EndSelect);
                caseClauses.Add(new CaseClause(tests.ToImmutable(), statements, caseToken));
            }
            else
            {
                throw CreateError(Peek(), "Expected 'CASE' or 'END SELECT'.");
            }
        }

        // Aggiungi un loop di pulizia finale prima di cercare END SELECT.
        while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber))
        {
        }

        Consume(TokenKind.EndSelect, "Expected 'END SELECT' to close the block.");
        return new SelectStmt(expression, caseClauses.ToImmutable(), elseClause, selectToken);
    }*/
    
    /// <summary>
    /// [FASE 1 - NUOVO] Parsa un singolo test di CASE, gestendo tutti i pattern avanzati e tradizionali.
    /// Agisce come dispatcher per i diversi tipi di test/pattern: valore/range, comparazione (IS >), 
    /// pattern tipo (IS Tipo), e pattern valore con acquisizione (Valore AS v).
    /// </summary>
    /// <param name="procedureContext">Il contesto procedurale corrente (per la risoluzione dei tipi in ParseExpression).</param>
    /// <returns>Un nodo AST derivato da <see cref="CaseTest"/> (e.g., <see cref="CaseValue"/>, <see cref="CaseRange"/>, <see cref="CaseComparison"/>, <see cref="CaseTypePattern"/>, <see cref="CaseValuePattern"/>).</returns>
    /// <exception cref="BasicException">Lanciata se viene rilevata una sintassi non valida per un test di caso.</exception>
    private CaseTest ParseCaseTest()
    {
        // 1. CASE IS ... (Comparazione o Pattern di Tipo)
        if (Match(TokenKind.Is))
        {
            // A. Comparazione: CASE IS > 10
            if (Check(TokenKind.Equal) || Check(TokenKind.NotEqual) || 
                Check(TokenKind.LessThan) || Check(TokenKind.LessEqual) || 
                Check(TokenKind.GreaterThan) || Check(TokenKind.GreaterEqual))
            {
                var op = Advance();
                var value = ParseExpression();
                return new CaseComparison(op, value);
            }
            
            // B. Pattern di Tipo: CASE IS INTEGER [x] [WHEN ...]
            // Nota: ParseTypeName consuma token, quindi dobbiamo essere sicuri che sia un tipo.
            // Se non è un operatore di confronto, assumiamo sia un tipo.
            var targetType = ParseTypeName();
            Token? variableName = null;
            
            // Supporto sintassi: CASE IS STRING s  oppure CASE IS STRING AS s
            if (Match(TokenKind.As))
            {
                variableName = Consume(TokenKind.Identifier, "Expected variable name after 'AS'.");
            }
            else if (Check(TokenKind.Identifier) && !IsStructuralKeyword(Peek().Kind) && !Check(TokenKind.When))
            {
                // Cattura variabile opzionale se non è una keyword (es. WHEN)
                variableName = Advance();
            }

            Expr? whenCondition = null;
            if (Match(TokenKind.When))
            {
                whenCondition = ParseExpression();
            }

            return new CaseTypePattern(targetType, variableName, whenCondition);
        }

        // 2. Parsing espressione iniziale (Valore o Inizio Range)
        var startExpr = ParseExpression();

        // 3. CASE 1 TO 10 (Range)
        if (Match(TokenKind.To))
        {
            var endExpr = ParseExpression();
            return new CaseRange(startExpr, endExpr);
        }

        // 4. Pattern di Valore Esteso: CASE 10 AS x [WHEN ...]
        Token? alias = null;
        if (Match(TokenKind.As))
        {
            alias = Consume(TokenKind.Identifier, "Expected variable name after 'AS'.");
        }

        // 5. Filtro WHEN (valido sia per valore semplice che per pattern con alias)
        // CASE 10 WHEN ...  oppure CASE 10 AS x WHEN ...
        Expr? whenFilter = null;
        if (Match(TokenKind.When))
        {
            whenFilter = ParseExpression();
        }

        // Se abbiamo alias o filtro, è un Pattern su Valore
        if (alias != null || whenFilter != null)
        {
            return new CaseValuePattern(startExpr, alias, whenFilter);
        }

        // 6. Caso Base: Valore Semplice
        return new CaseValue(startExpr);
    }
    
    /// <summary>
    /// Analizza un blocco SELECT CASE...END SELECT e le clausole interne CASE.
    /// Questo metodo fa parte della prima fase di parsing (se applicabile) o del parsing
    /// standard del blocco.
    /// </summary>
    /// <remarks>
    /// Scopo Architetturale: Gestire l'intera struttura di salto condizionale multi-ramo.
    /// Il metodo analizza l'espressione di controllo del SELECT e poi consuma iterativamente
    /// le clausole CASE, interpretando le varie sintassi di test.
    /// 
    /// Sintassi Tradizionali:
    /// - 'CASE Valore' (Es. CASE 10)
    /// - 'CASE Valore1 TO Valore2' (Es. CASE 0 TO 17)
    /// - 'CASE IS <Op> Valore' (Es. CASE IS > 0 o CASE IS < 0)
    ///
    /// Nuove Sintassi di Pattern Matching (Avanzato):
    /// - 'CASE IS Tipo [AS Var]' (Es. CASE IS STRING s o CASE IS Punto p)
    /// - 'CASE Valore AS Var' (Es. CASE 10 AS v - pattern su valore con acquisizione)
    /// - 'CASE ... WHEN Condizione' (Es. CASE IS Punto p WHEN p.X > 0 - filtro aggiuntivo)
    ///
    /// Logica del Blocco: Ogni clausola CASE viene parsata in un blocco di istruzioni separato
    /// fino a quando non viene incontrato il token 'CASE' successivo o 'END SELECT'.
    /// La clausola 'CASE ELSE' (se presente) deve essere l'ultima clausola del blocco.
    ///
    /// Gestione Flusso:
    /// Il metodo utilizza l'helper 'ParseBlockBodyUntil' per delegare il parsing
    /// del corpo di ogni clausola, garantendo che i confini del blocco siano rispettati.
    /// </remarks>
    private SelectStmt ParseSelectCaseStatement(ProcedureDefinition? procedureContext)
    {
        var selectToken = Consume(TokenKind.Select, "Expected 'SELECT'.");
        Consume(TokenKind.Case, "Expected 'CASE' after 'SELECT'.");

        var expression = ParseExpression();
        Consume(TokenKind.Newline, "Expected newline after 'SELECT CASE <expression>'.");

        var caseClauses = ImmutableArray.CreateBuilder<CaseClause>();
        CaseElseClause? elseClause = null;

        while (!Check(TokenKind.EndSelect) && !IsAtEnd())
        {
            // Pulisce i metadati (newline, numeri di riga) che possono precedere una clausola CASE.
            while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber) || Match(TokenKind.Rem))
            {
                if (Previous().Kind == TokenKind.Rem)
                {
                    while (!Check(TokenKind.Newline) && !IsAtEnd()) Advance();
                }
            }

            if (Check(TokenKind.EndSelect) || IsAtEnd()) break;

            var caseToken = Consume(TokenKind.Case, "Expected 'CASE' or 'END SELECT'.");

            if (Match(TokenKind.Else))
            {
                Consume(TokenKind.Newline, "Expected newline after 'CASE ELSE'.");
                var elseStatements = ParseBlockBodyUntil(procedureContext, TokenKind.EndSelect);
                elseClause = new CaseElseClause(elseStatements, caseToken);
                break;
            }

            var tests = ImmutableArray.CreateBuilder<CaseTest>();
            do
            {
                while (Match(TokenKind.Newline)) { }
                tests.Add(ParseCaseTest());
            } while (Match(TokenKind.Comma));

            Consume(TokenKind.Newline, "Expected newline after CASE conditions.");
            
            var statements = new List<Stmt>();
            var declarations = new List<AstNode>();
            
            while (!Check(TokenKind.Case) && !Check(TokenKind.EndSelect) && !IsAtEnd())
            {
                var positionBefore = _current;

                // *** CORREZIONE DEFINITIVA ***
                // Pulisce i metadati (numeri di riga, commenti, righe vuote) all'inizio
                // del corpo del blocco, prima di tentare di parsare uno statement.
                while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber) || Match(TokenKind.Rem))
                {
                    if (Previous().Kind == TokenKind.Rem)
                    {
                        while (!Check(TokenKind.Newline) && !IsAtEnd()) Advance();
                    }
                }
                
                // Dopo la pulizia, controlla di nuovo se siamo arrivati alla fine del blocco.
                if (Check(TokenKind.Case) || Check(TokenKind.EndSelect) || IsAtEnd()) break;

                // Ora è sicuro chiamare ParseLogicalLine, perché il token corrente
                // è garantito essere l'inizio di uno statement valido.
                ParseLogicalLine(statements, declarations, procedureContext);
                
                if (_current == positionBefore)
                {
                    // Meccanismo di sicurezza per evitare loop infiniti se ParseLogicalLine si blocca.
                    break;
                }
            }
            
            caseClauses.Add(new CaseClause(tests.ToImmutable(), [.. statements], caseToken));
        }

        Consume(TokenKind.EndSelect, "Expected 'END SELECT'.");

        return new SelectStmt(expression, caseClauses.ToImmutable(), elseClause, selectToken);
    }

    /// <summary>
    /// [FASE 1] Parsa un blocco MATCH...END MATCH (pattern matching stile Rust).
    /// Sintassi:
    /// MATCH expr
    ///     CASE IS NOTHING => body
    ///     CASE IS TYPE var WHEN cond => body
    ///     CASE value => body
    ///     CASE val1, val2 => body
    ///     CASE val1 TO val2 => body
    ///     CASE IS > val => body
    ///     CASE _ THEN body  (wildcard)
    /// END MATCH
    /// </summary>
    private MatchStmt ParseMatchStatement(ProcedureDefinition? procedureContext)
    {
        var matchToken = Consume(TokenKind.Match, "Expected 'MATCH'.");
        var expression = ParseExpression();
        Consume(TokenKind.Newline, "Expected newline after 'MATCH <expression>'.");

        var cases = ImmutableArray.CreateBuilder<MatchCaseClause>();

        while (!Check(TokenKind.EndMatch) && !IsAtEnd())
        {
            while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber) || Match(TokenKind.Rem))
            {
                if (Previous().Kind == TokenKind.Rem)
                    while (!Check(TokenKind.Newline) && !IsAtEnd()) Advance();
            }

            if (Check(TokenKind.EndMatch) || IsAtEnd()) break;

            var caseToken = Consume(TokenKind.Case, "Expected 'CASE' or 'END MATCH'.");

            // Parse the pattern
            Expr? nullPattern = null;
            CaseTypePattern? typePattern = null;
            CaseValuePattern? valuePattern = null;
            CaseValue? valueTest = null;
            CaseRange? rangeTest = null;
            CaseComparison? comparisonTest = null;
            Token? wildcardToken = null;

            // CASE _ (wildcard)
            if (Peek().Kind == TokenKind.Identifier && Peek().Lexeme == "_")
            {
                wildcardToken = Advance();
            }
            // CASE IS NOTHING
            else if (Check(TokenKind.Is))
            {
                Advance(); // Consuma IS

                if (Check(TokenKind.Nil))
                {
                    // CASE IS NOTHING
                    nullPattern = new NilExpr(Advance());
                }
                else if (Check(TokenKind.GreaterThan) || Check(TokenKind.LessThan))
                {
                    // CASE IS > val / CASE IS < val (comparison pattern)
                    var comparisonToken = Advance(); // Consuma > o <
                    var comparisonValue = ParseMatchValueExpr();
                    comparisonTest = new CaseComparison(comparisonToken, comparisonValue);
                }
                else
                {
                    // CASE IS TYPE var WHEN cond
                    var targetType = ParseTypeName();
                    Token? variableName = null;

                    if (Match(TokenKind.As))
                        variableName = Consume(TokenKind.Identifier, "Expected variable name after 'AS'.");
                    else if (Check(TokenKind.Identifier) && !IsStructuralKeyword(Peek().Kind))
                        variableName = Advance();

                    Expr? whenCondition = null;
                    if (Match(TokenKind.When))
                        whenCondition = ParseMatchValueExpr();

                    typePattern = new CaseTypePattern(targetType, variableName, whenCondition);
                }
            }
            else
            {
                // CASE value oppure CASE value AS var WHEN cond
                // Usa ParseMatchValueExpr per evitare che venga consumato => (FatArrow)
                var startExpr = ParseMatchValueExpr();

                // CASE val1 TO val2 (range)
                if (Match(TokenKind.To))
                {
                    var endExpr = ParseMatchValueExpr();
                    rangeTest = new CaseRange(startExpr, endExpr);
                }
                else
                {
                    // CASE value AS var WHEN cond
                    Token? alias = null;
                    if (Match(TokenKind.As))
                    {
                        alias = Consume(TokenKind.Identifier, "Expected variable name after 'AS'.");
                    }

                    Expr? whenFilter = null;
                    if (Match(TokenKind.When))
                        whenFilter = ParseMatchValueExpr();

                    if (alias != null || whenFilter != null)
                        valuePattern = new CaseValuePattern(startExpr, alias, whenFilter);
                    else
                        valueTest = new CaseValue(startExpr);
                }
            }

            // Consuma => se presente, poi richiedi il newline
            if (Check(TokenKind.FatArrow))
                Advance();
            Consume(TokenKind.Newline, "Expected newline after CASE pattern.");

            // Parse body fino al prossimo CASE o END MATCH (stesso pattern di SELECT CASE)
            var statements = new List<Stmt>();
            var declarations = new List<AstNode>();

            while (!Check(TokenKind.Case) && !Check(TokenKind.EndMatch) && !IsAtEnd())
            {
                // Pulisce i metadati (numeri di riga, commenti, righe vuote) all'inizio del corpo.
                while (Match(TokenKind.Newline) || Match(TokenKind.LineNumber) || Match(TokenKind.Rem))
                {
                    if (Previous().Kind == TokenKind.Rem)
                    {
                        while (!Check(TokenKind.Newline) && !IsAtEnd()) Advance();
                    }
                }

                if (Check(TokenKind.Case) || Check(TokenKind.EndMatch) || IsAtEnd()) break;

                var positionBefore = _current;
                ParseLogicalLine(statements, declarations, procedureContext);

                if (_current == positionBefore)
                {
                    break;
                }
            }

            cases.Add(new MatchCaseClause(
                nullPattern, typePattern, valuePattern,
                valueTest, rangeTest, comparisonTest,
                wildcardToken!,
                statements.ToImmutableArray(),
                caseToken));
        }

        Consume(TokenKind.EndMatch, "Expected 'END MATCH' to close the block.");

        return new MatchStmt(expression, cases.ToImmutable(), matchToken);
    }

    /// <summary>
    /// Parsa un'espressione per il valore di un CASE nel MATCH.
    /// Non remappa => (FatArrow) a >= perché nel MATCH => è un separatore.
    /// </summary>
    private Expr ParseMatchValueExpr()
    {
        return ParseComparison(remapFatArrow: false);
    }

    /// <summary>
    /// Parsa un blocco WITH...END WITH.
    /// WITH expression
    ///   .Member = value
    /// END WITH
    /// </summary>
    private WithStmt ParseWithStatement(ProcedureDefinition? procedureContext)
    {
        var withToken = Consume(TokenKind.With, "Expected 'WITH'.");
        var expression = ParseExpression();
        Consume(TokenKind.Newline, "Expected newline after 'WITH <expression>'.");

        var body = ParseBlockBodyUntil(procedureContext, TokenKind.EndWith);

        Consume(TokenKind.EndWith, "Expected 'END WITH' to close the block.");
        return new WithStmt(expression, body, withToken);
    }

    private UsingStmt ParseUsingStatement(ProcedureDefinition? procedureContext)
    {
        var usingToken = Consume(TokenKind.Using, "Expected 'USING'.");
        var target = ParseTarget();
        Consume(TokenKind.Equal, "Expected '=' after resource variable in USING.");
        var init = ParseExpression();
        Consume(TokenKind.Newline, "Expected newline after 'USING <var> = <expr>'.");

        var body = ParseBlockBodyUntil(procedureContext, TokenKind.EndUsing);

        Consume(TokenKind.EndUsing, "Expected 'END USING' to close the block.");
        return new UsingStmt(target, init, body, usingToken);
    }
}