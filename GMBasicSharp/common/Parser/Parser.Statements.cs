// FILE: Interpreter.Parser.Statements.cs

using System.Collections.Immutable;

// ReSharper disable SwitchStatementHandlesSomeKnownEnumValuesWithDefault
// ReSharper disable RedundantArgumentDefaultValue
// ReSharper disable SwitchExpressionHandlesSomeKnownEnumValuesWithExceptionInDefault
// ReSharper disable SwitchStatementMissingSomeEnumCasesNoDefault

// ReSharper disable InvalidXmlDocComment

namespace Basic;

/// <summary>
/// Contiene i metodi del Parser per la Fase 1: Costruzione dell'AST Puro.
/// Questi metodi sono responsabili esclusivamente dell'analisi sintattica e della
/// creazione di un AST "spurio", senza alcuna interazione di scrittura con la SymbolTable.
/// Questa classe duplica e adatta solo i metodi il cui comportamento è specifico per la Fase 1,
/// riutilizzando gli helper di parsing puramente sintattici dalla classe Parser principale.
/// </summary>
public sealed partial class Parser
{
    // ========================================================================
    // --- Entry Point e Metodi di Parsing Principali (Fase 1) ---
    // ========================================================================

    /// <summary>
    /// [FASE 1] Parser di alto livello per l'intero “modulo” sorgente BASIC.
    /// Converte la sequenza di token (già prodotta dal <c>Lexer</c> e dalle sue
    /// pre–passate di pre–processing) nel nodo radice dell’AST del modulo:
    /// tipicamente un contenitore di:
    ///  - Dichiarazioni (FUNCTION / SUB / PROPERTY / TYPE / CLASS …)
    ///  - Statement top–level (PRINT, LET, IF, GOTO, LABEL, ecc.)
    ///  - Label globali e numeri di riga (salti / indirizzi logici)
    ///  - Commenti e righe vuote (REM impliciti, opzionali)
    ///  - Direttive già risolte (non più presenti come token attivi)
    /// </summary>
    /// <remarks>
    /// RESPONSABILITÀ PRINCIPALI:
    /// 1. Inizializza lo stato di parsing per il modulo (tabella simboli provvisoria,
    ///    tracking di line numbers / label, raccolte temporanee).
    /// 2. Esegue un loop fino a <c>Eof</c>, delegando il parsing di:
    ///    - ProcedureDefinition (SUB / FUNCTION) → accumula body fino a END SUB / END FUNCTION.
    ///    - Blocchi strutturati (IF multi-riga, SELECT CASE, WHILE, FOR…) demandati
    ///      ai relativi metodi ParseXXX (non implementati qui).
    ///    - Statement singoli e riga logica tramite <see cref="ParseLogicalLine"/>.
    /// 3. Normalizza “riga logica” vs “riga fisica”: il Lexer ha già unito le continuazioni
    ///    (underscore finali, colon iniziali di continuazione), quindi questo metodo non
    ///    deve gestire merging testuale, ma solo token semantics.
    /// 4. Gestisce label e numeri di riga top–level:
    ///    - Numeri di riga (TokenKind.LineNumber) all’inizio di una riga logica
    ///      possono essere registrati come punti di salto (LineAnchor) per fasi successive.
    ///    - Identificatori seguiti da ‘:’ → LabelStmt (etichetta globale).
    /// 5. Distingue tra dichiarazioni e statement:
    ///    - Le dichiarazioni vengono raccolte in una lista separata (<c>ModuleDeclarations</c>)
    ///      per build della symbol table nelle fasi successive (FASE 2).
    ///    - Gli statement top-level restano nel corpo principale (Main statements).
    ///
    /// INTEGRAZIONE CON ALTRE FASI:
    /// - FASE 0 / Lexer: Ha già filtrato commenti, direttive, raw string e continuity.
    /// - FASE 1 (questo metodo): Costruisce struttura gerarchica grezza (syntax tree).
    /// - FASE 2 (semantica): Risoluzione simboli (binding), tipi impliciti, validazione referenze.
    /// - FASE 3 (emissione / interpretazione): Generazione bytecode o direttamente esecuzione.
    ///
    /// ERRORE HANDLING:
    /// - Solleva <c>BasicException</c> su:
    ///   * Fine inattesa di blocco (END SUB mancante, END IF assente, ecc.)
    ///   * Token inatteso all’inizio di riga top-level (es: ‘ELSE’ fuori da IF).
    ///   * Duplicato di label / line number (se la semantica lo proibisce).
    /// - Tenta di contestualizzare il messaggio con la riga originale (<c>SourcePosition</c>).
    ///
    /// PERFORMANCE:
    /// - Singolo pass lineare; ogni riga logica è consumata una sola volta.
    /// - Usa lookahead minimo (At(1), At(2)) per distinguere pattern (es. identificatore + ‘:’).
    ///
    /// STATO INTERNO TIPICO (schematico):
    /// <code>
    /// while (!IsAtEnd())
    /// {
    ///     if (Peek().Kind == TokenKind.LineNumber) RegistraLineAnchor();
    ///     else if (IsLabelDefinition()) ParseLabel();
    ///     else if (IsProcedureStart()) ParseProcedure();
    ///     else ParseLogicalLine(...);
    /// }
    /// </code>
    ///
    /// DISTINZIONE PROCEDURE:
    /// - FUNCTION vs SUB (token multiparola già prodotti dal Lexer come EndFunction / EndSub).
    /// - Ogni procedura ha:
    ///   * Signature (nome + parametri + eventuale tipo di ritorno implicito da suffisso).
    ///   * Corpo: read fino a END SUB / END FUNCTION (incluso).
    /// - Nessun annidamento: se il dialetto non supporta SUB dentro SUB, solleva errore se rileva definizione nested.
    ///
    /// LABEL / NUMERO DI RIGA:
    /// - Caso tipico:
    ///   <code>
    ///   100 PRINT "Inizio"      // 100 registrato come line anchor
    ///   StartLoop: PRINT A      // Label “StartLoop”
    ///   IF A > 0 THEN 200       // Salto implicito a line 200 (gestito più in basso)
    ///   </code>
    /// - Le destinazioni (line number / label) vengono annotate ma la risoluzione (binding) avverrà dopo.
    ///
    /// CASI SUPPORTATI (ESEMPI):
    ///   Intermix dichiarazioni e codice:
    /// 
    ///   SUB Init
    ///      PRINT "Init"
    ///   END SUB
    ///   A = 1
    ///   PRINT A
    /// 
    ///   Etichette e GOTO:
    /// 
    ///   LoopStart:
    ///   PRINT "Step"
    ///   GOTO LoopStart
    /// 
    ///   Numeri di riga:
    /// 
    ///   10 PRINT "Riga 10"
    ///   20 IF X THEN 10
    /// 
    ///   Multi-riga IF in main:
    /// 
    ///   IF A < B THEN
    ///      PRINT "A<B"
    ///   ELSE
    ///      PRINT "A>=B"
    ///   END IF
    /// 
    ///   Mix colon e statement:
    /// 
    ///   PRINT "A" : PRINT "B" : C = 3
    ///
    /// LIMITI / NON GESTITI QUI:
    /// - Directives (#IFDEF ...) già filtrate nel preprocessing.
    /// - Inclusioni di file multipli (gestite a livello di Program se presenti).
    /// - Ottimizzazioni (constant folding) rinviate a fasi successive.
    /// - Analisi semantica di tipi impliciti (suffissi $, %, #, ecc.) rinviata.
    ///
    /// ESTENSIONI FUTURE:
    /// - Supporto moduli parziali / namespace.
    /// - Aggregazione metadata (documentazione inline, attributi).
    /// - Pre-registrazione rapida di simboli esportati (per cross-module binding).
    ///
    /// ESEMPI DI USO:
    /// <example>
    /// <code>
    /// var parser = new Parser(tokens);
    /// var moduleAst = parser.ParseModuleSource();
    /// // moduleAst.Declarations → funzioni e sub
    /// // moduleAst.Statements → corpo principale eseguibile
    /// </code>
    /// </example>
    /// </remarks>
    /// <returns>
    /// AST del modulo (contenente dichiarazioni e corpo principale).
    /// </returns>
    /// <exception cref="BasicException">
    /// Sollevata su strutture sintattiche non valide (END mancante, token inatteso).
    /// </exception>
    public BasicModule ParseModuleSource(List<ImportStmt> imports)
    {
        _current = 0;
        var statementsByLine = new Dictionary<int, List<Stmt>>();
        var executionOrder = new List<int>();
        var declarations = new List<AstNode>();
        _lastParsedLineNumber = null;
        _currentNamespace = null;
        _libraryName = null;

        // --- Rilevamento e CONSUMO di OPTION STRICT ---
        bool isStrict = false;
        // Salta eventuali newline/commenti iniziali
        while (Check(TokenKind.Newline) || Check(TokenKind.Rem))
        {
            Advance();
        }

        if (Check(TokenKind.Option))
        {
            // Esegui un lookahead per confermare che sia 'OPTION STRICT'
            if (At(1).Kind == TokenKind.Strict)
            {
                Advance(); // Consuma 'OPTION'
                Advance(); // Consuma 'STRICT'
                if (Match(TokenKind.On))
                {
                    isStrict = true;
                    Log($"[OPTION STRICT] Rilevato 'OPTION STRICT ON' per il modulo: {Path.GetFileName(_entryFilePath)}");
                }
                else if (Match(TokenKind.Off))
                {
                    isStrict = false; // Già default, ma esplicito per chiarezza
                    Log($"[OPTION STRICT] Rilevato 'OPTION STRICT OFF' per il modulo: {Path.GetFileName(_entryFilePath)}");
                }
                else
                {
                    // Se non c'è né ON né OFF, è un errore di sintassi.
                    throw CreateError(Peek(), "Expected 'ON' or 'OFF' after 'OPTION STRICT'.");
                }
            }
        }
        // A questo punto, se la direttiva era presente, è stata consumata.
        // Il parsing prosegue normalmente dal token successivo.

        const int SyntheticLineBase = 1_000_000;
        int syntheticLineCounter = 0;

        while (!IsAtEnd())
        {
            while (Match(TokenKind.Newline))
            {
            }

            if (IsAtEnd()) break;

            SynchronizeSourceLine();

            var token = Peek();

            // --- ATTRIBUTES a livello di modulo: <Serializable>, <Required>, etc. ---
            if (token.Kind == TokenKind.LessThan)
            {
                _pendingAttributes = ParseStatementAttributes();
                continue; // Torna all'inizio del loop per parsare il statement successivo
            }

            switch (token.Kind)
            {
                case TokenKind.Namespace:
                    Advance();
                    var nsToken = Consume(TokenKind.Identifier, "Expected a namespace name after 'NAMESPACE'.");
                    _currentNamespace = nsToken.Lexeme;
                    Log($"[PARSER-NS-TRACE] Namespace attivo impostato a: '{_currentNamespace}'");
                    continue;

                case TokenKind.Library:
                {
                    if (_currentNamespace != null)
                        throw CreateError(Peek(),
                            $"LIBRARY not allowed inside NAMESPACE '{_currentNamespace}'.",
                            ErrorCode.SyntaxError);
                    if (_libraryName != null)
                        throw CreateError(Peek(),
                            "LIBRARY già dichiarata in questo modulo.", ErrorCode.SyntaxError);
                    Advance();
                    // Nome: stringa letterale ("mylib") oppure identificatore nudo (mylib).
                    var libTok = Peek();
                    if (libTok.Kind == TokenKind.StringLiteral)
                    {
                        _libraryName = libTok.Literal as string ?? libTok.Lexeme.Trim('"');
                        Advance();
                    }
                    else if (libTok.Kind == TokenKind.Identifier)
                    {
                        _libraryName = libTok.Lexeme;
                        Advance();
                    }
                    else
                    {
                        throw CreateError(libTok,
                            "Expected a library name (string or identifier) after 'LIBRARY'.",
                            ErrorCode.SyntaxError);
                    }
                    continue;
                }

                case TokenKind.LineNumber:
                {
                    if (_currentNamespace != null)
                        throw CreateError(Peek(),
                            $"Line numbers not allowed inside NAMESPACE '{_currentNamespace}'.",
                            ErrorCode.SyntaxError);
                    var lineNumber = ParseLineNumberedStatement(statementsByLine, declarations);
                    executionOrder.Add(lineNumber);
                    break;
                }

                default:
                {
                    var lineStmts = new List<Stmt>();
                    var tempDeclarations = new List<AstNode>();
                    var topPosBefore = _current;
                    ParseLogicalLine(lineStmts, tempDeclarations, null);
                    if (_current == topPosBefore) ThrowIfStrayBlockTerminator();   // END IF/ELSE/CASE orfano a livello modulo: errore, non HANG

                    foreach (var declNode in tempDeclarations)
                    {
                        var finalNodeToAdd = declNode;
                        if (_currentNamespace != null)
                        {
                            if (finalNodeToAdd is Decl d)
                            {
                                finalNodeToAdd = d with { DeclaredNamespace = _currentNamespace };
                            }
                            else if (finalNodeToAdd is DeclarationStmt ds)
                            {
                                var ns = _currentNamespace;
                                var scopedDecls = ds.Declarations
                                    .Select(vd => vd with { DeclaredNamespace = ns })
                                    .ToImmutableArray();
                                finalNodeToAdd = ds with { Declarations = scopedDecls };
                            }
                            else if (finalNodeToAdd is ConstStmt cs)
                            {
                                var ns = _currentNamespace;
                                var scopedConsts = cs.Declarations
                                    .Select(cd => cd with { DeclaredNamespace = ns })
                                    .ToImmutableArray();
                                finalNodeToAdd = cs with { Declarations = scopedConsts };
                            }
                        }

                        declarations.Add(finalNodeToAdd);
                    }

                    if (lineStmts.Count > 0)
                    {
                        int synthKey = SyntheticLineBase + syntheticLineCounter++;
                        statementsByLine[synthKey] = lineStmts;
                        executionOrder.Add(synthKey);
                    }

                    break;
                }
            }
        }

        var module = new BasicModule(
            _entryFilePath!,
            isStrict,
            _sourceMap,
            statementsByLine,
            imports,
            declarations,
            _libraryName
        );
        module.ExecutionOrder = [..executionOrder];
        return module;
    }

    /// <summary>
    /// [FASE 1] Parsa una riga di codice che inizia con un numero, smistando i nodi risultanti.
    /// Restituisce il numero di riga parsato.
    /// </summary>
    private int ParseLineNumberedStatement(Dictionary<int, List<Stmt>> statementsByLine,
        List<AstNode> declarations)
    {
        _currentLineNumberToken = Consume(TokenKind.LineNumber, "Internal parser error: Expected a line number.");
        if (_currentLineNumberToken.Literal is not double lineNumberDouble)
            throw CreateError(_currentLineNumberToken, "Line number must be a valid numeric value.");

        var lineNumber = (int)lineNumberDouble;
        _lastParsedLineNumber = lineNumber;

        if (!statementsByLine.TryGetValue(lineNumber, out var currentLineStatements))
        {
            currentLineStatements = [];
            statementsByLine[lineNumber] = currentLineStatements;
        }

        if (Check(TokenKind.Newline) || IsAtEnd())
        {
            var remToken = new Token(TokenKind.Rem, "REM", "Implicit REM for empty line",
                _currentLineNumberToken.Position);
            currentLineStatements.Add(new RemStmt(remToken));
            return lineNumber;
        }

        // FIX REGRESSIONE: Usiamo una lista temporanea per catturare le dichiarazioni della riga.
        // Dobbiamo filtrare:
        // 1. DIM/CONST: Vanno scartate da 'declarations' (sono già in 'currentLineStatements' come eseguibili
        //    e nel codice numerato non devono essere registrate come globali hoisterate per evitare conflitti).
        // 2. TYPE/CLASS/FUNCTION: Vanno preservate e aggiunte a 'declarations' perché sono definizioni strutturali
        //    necessarie alla SymbolTable (es. per 'AS Cliente').
        var tempDeclarations = new List<AstNode>();
        var lnPosBefore = _current;
        ParseLogicalLine(currentLineStatements, tempDeclarations, null);
        if (_current == lnPosBefore) ThrowIfStrayBlockTerminator();

        foreach (var decl in tempDeclarations)
        {
            // Aggiungi alla lista principale del modulo solo se NON è una dichiarazione di variabile/costante.
            if (decl is not (DeclarationStmt or ConstStmt))
            {
                declarations.Add(decl);
            }
        }

        return lineNumber;
    }

    /// <summary>
    /// FASE 1 – Parsing di una “riga logica” del programma BASIC.
    /// Una riga logica è la sequenza di uno o più segmenti di codice sullo stesso flusso,
    /// separati (eventualmente) da due punti ':' e terminati da NEWLINE o EOF.
    /// Questo metodo:
    /// 1. Consuma e normalizza i separatori (colon multipli, spazi).
    /// 2. Raccoglie in ordine i nodi AST (statement e dichiarazioni) prodotti dai parser
    ///    specifici (PRINT, LET, IF, GOSUB, ecc.).
    /// 3. Gestisce etichette e salti impliciti (label / line number isolati) SOLO se la
    ///    semantica del chiamante lo consente (dipende dall’uso del metodo).
    /// 4. Integra il lookahead di assegnazione implicita tramite <see cref="IsImplicitAssignment"/>.
    /// 5. Termina al primo NEWLINE / EOF senza “mangiare” token oltre la riga.
    /// 6. Non produce nodi per segmenti vuoti (es. ‘: :’).
    ///
    /// OUTPUT:
    /// - Popola due liste esterne: una di statement (List&lt;Stmt&gt;) e una di dichiarazioni / altri nodi (List&lt;AstNode&gt;).
    /// - Non restituisce valore diretto: modifica le liste passate per riferimento.
    ///
    /// PERCHÉ SEPARATO:
    /// - Consente riuso da costrutti che hanno corpo inline (IF single-line, THEN multi-istruzione, ELSE, ecc.).
    /// - Uniforma la gestione dei colon evitando duplicazione di logica in più parser di alto livello.
    ///
    /// FLUSSO GENERALE:
    /// 1. Loop:
    ///    a. Consuma colon iniziali (mentre c’è TokenKind.Colon).
    ///    b. Se il prossimo token è terminatore (Newline, Eof) → esci.
    ///    c. Se il prossimo token è una label nuda:
    ///         - Registra LabelStmt (o nodo equivalente) e continua.
    ///    d. Se <see cref="IsImplicitAssignment"/> ritorna true:
    ///         - Parsa target + '=' + espressione → LetStmt.
    ///    e. Altrimenti dispatch generico: <see cref="ParseStatement"/>:
    ///         - Può produrre più nodi (es. dichiarazione + corpo).
    ///    f. Gestisce eventuali trailing colon: se c’è un colon, riparte il loop (nuovo segmento).
    /// 2. Termina su NEWLINE / EOF senza consumare il terminatore (a meno che il design preveda la sua rimozione).
    ///
    /// CASI SPECIALI GESTITI (esempi):
    /// - Riga vuota: produce RemStmt implicito (opzionale) oppure nessun nodo.
    /// - PRINT "A" : PRINT "B" : PRINT "C"
    /// - LET A = 1 : B = 2 : C = 3
    /// - Etichetta all’inizio: StartLoop: PRINT "Inizio"
    /// - Assegnazione implicita riconosciuta: A = 10 : PRINT A
    /// - Colon multipli: PRINT "X" : : PRINT "Y" (secondo colon ignorato)
    ///
    /// NON GESTISCE QUI:
    /// - Strutture multi-riga (IF ... END IF) → demandate a parser dedicati.
    /// - Pre-processore (#IFDEF, ecc.) → già risolto dal Lexer.
    /// - Continuazioni con underscore (_) → già unificate dal pre-processore.
    ///
    /// REGOLE PER LABEL NUDA:
    /// - IDENTIFIER seguito da (Colon | Newline | Else | Eof) a seconda del contesto.
    /// - Se la label è seguita da colon immediato, la label è registrata e il colon trattato come separatore.
    ///
    /// INTERAZIONE CON SALTI IMPLICITI:
    /// - Se il chiamante vuole trasformare un IDENTIFIER o LineNumber isolato in GotoStmt,
    ///   DEVE farlo prima o dopo la chiamata (ParseLogicalLine è neutrale / riutilizzabile).
    ///
    /// ERRORE EVITATI:
    /// - “Token ':' inatteso…”: colon viene scartato prima del dispatch.
    /// - “Token ‘<lineNumber>’ inatteso…”: numeri di riga isolati non passati come statement se non previsti.
    ///
    /// ESEMPI DI USO (pseudo):
    /// <code>
    /// var stmts = new List&lt;Stmt&gt;();
    /// var decls = new List&lt;AstNode&gt;();
    /// ParseLogicalLine(stmts, decls, currentProcedure);
    /// // Ora stmts contiene la sequenza di statement sulla riga logica.
    /// </code>
    ///
    /// PERFORMANCE:
    /// - Singolo pass lineare sui token della riga.
    /// - Effettua lookahead minimo (At(1)) per label nuda / assegnazione implicita.
    /// - Non fa rewind multipli (solo quello interno di IsImplicitAssignment).
    ///
    /// ESTENSIONI FUTURE:
    /// - Supporto destructuring: (A, B) = Funzione()
    /// - Supporto assignment multiplo separato da virgole: A = 1, B = 2 (richiede grammatica dedicata).
    /// - Emissione di Warning per colon consecutivi (lint).
    ///
    /// CONTRATTI:
    /// - Non deve avanzare oltre il terminatore di riga (Newline / Eof) salvo design esplicito.
    /// - Non solleva eccezioni su segmenti vuoti (li ignora).
    /// - Solleva eccezioni solo su token veramente non validi all’inizio di un segmento (post colon).
    /// </summary>
    /// <summary>
    /// Terminatori/separatori di blocco che <see cref="ParseLogicalLine"/> lascia volutamente NON consumati (li gestisce il parser
    /// del blocco IF / SELECT CASE che li ha aperti). Se compaiono dove nessun IF/SELECT li attende sono ORFANI: prima il chiamante
    /// non avanzava mai (HANG del front-end, es. `IF c THEN a ELSE` + blocco + `END IF`, che lascia un `END IF` senza `IF`) o li
    /// inghiottiva in silenzio (ParseBlockBodyUntil). Ora e' un errore di parsing esplicito.
    /// </summary>
    private void ThrowIfStrayBlockTerminator()
    {
        var t = Peek();
        var msg = t.Kind switch
        {
            TokenKind.EndIf => "END IF without a matching IF.",
            TokenKind.Else => "ELSE without a matching IF.",
            TokenKind.ElseIf => "ELSEIF without a matching IF.",
            TokenKind.EndSelect => "END SELECT without a matching SELECT CASE.",
            TokenKind.Case => "CASE without a matching SELECT CASE.",
            _ => null
        };
        if (msg is null) return;
        // Il caso tipico e' la forma MISTA `IF c THEN <stmt> ELSE` seguita da un blocco: e' invalida (single-line IF non ha END IF).
        if (t.Kind is TokenKind.EndIf or TokenKind.Else)
            msg += " (a single-line 'IF c THEN stmt [ELSE stmt]' has no END IF; if you mixed it with a multi-line block, use the block form 'IF c THEN' / 'ELSE' / 'END IF')";
        throw CreateError(t, msg);
    }

    private void ParseLogicalLine(List<Stmt> statements, List<AstNode> declarations,
        ProcedureDefinition? procedureContext, bool allowGotoLabel = false)
    {
        SynchronizeSourceLine();
        var isFirstLogical = true;

        do
        {
            if (Check(TokenKind.Newline) || IsAtEnd())
                break;

            if (Check(TokenKind.Identifier) && At(1).Kind == TokenKind.Colon)
            {                var labelToken = Advance();
                Advance();
                statements.Add(new LabelStmt(labelToken));
                if (Check(TokenKind.Newline) || IsAtEnd()) continue;
            }

            if (Check(TokenKind.ElseIf) || Check(TokenKind.Else) || Check(TokenKind.EndIf) ||
                Check(TokenKind.EndSelect) || Check(TokenKind.Case))
            {
                break;
            }

            if (allowGotoLabel && isFirstLogical && (
                    Check(TokenKind.LineNumber) ||
                    (Check(TokenKind.Identifier) && !IsStructuralKeyword(Peek().Kind) &&
                     At(1).Kind is TokenKind.Newline or TokenKind.Colon or TokenKind.Else or TokenKind.Eof)))
            {
                var targetToken = Advance();
                statements.Add(new GotoStmt(targetToken, targetToken));
                isFirstLogical = false;
                continue;
            }

            isFirstLogical = false;

            if (Check(TokenKind.Newline) || IsAtEnd() || Check(TokenKind.Colon)) continue;
            var parsedNodes = ParseStatement(procedureContext);
            foreach (var node in parsedNodes)
            {
                switch (node)
                {
                    // FIX IBRIDO: DeclarationStmt e ConstStmt devono essere SIA Dichiarazioni che Statement.
                    // - Come Dichiarazioni: Servono al Binder per popolare la SymbolTable (specialmente i Globali).
                    // - Come Statement: Servono all'Executor per l'inizializzazione e al Binder per lo scope locale.
                    case DeclarationStmt or ConstStmt:
                        var stmt = (Stmt)node;
                        statements.Add(stmt with { SourceLineIndex = _currentSourceLineIndex });
                        declarations.Add(node with { SourceLineIndex = _currentSourceLineIndex });
                        break;

                    case Stmt stmtNode:
                        // Altri statement (LET, PRINT, IF...) solo eseguibili.
                        statements.Add(stmtNode with { SourceLineIndex = _currentSourceLineIndex });
                        break;
                    default:
                        // Definizioni pure (FUNCTION, CLASS...) solo dichiarative.
                        declarations.Add(node with { SourceLineIndex = _currentSourceLineIndex });
                        break;
                }
            }
        } while (Match(TokenKind.Colon));
    }

    /// <summary>
    /// Determina, in fase di parsing (FASE 1), se la sequenza di token corrente
    /// rappresenta l’inizio di una assegnazione IMPLICITA di forma:
    ///    IDENTIFICATORE '=' <espressione>
    /// senza aver già deciso di consumare il target come statement completo.
    /// Restituisce <c>true</c> solo come LOOKAHEAD: non consuma definitivamente
    /// i token (la posizione del parser viene sempre ripristinata), così
    /// che il chiamante possa poi eseguire il parsing vero e proprio dell’assegnazione.
    /// </summary>
    /// <remarks>
    /// SCOPO:
    /// - Evitare di invocare il parser di un "target" su token che NON possono mai costituire
    ///   un L-value (es: Colon, Newline, parole chiave di blocco, numeri di riga).
    /// - Distinguere fra:
    ///     IF A = 10 THEN ...            (comparazione: '=' fa parte di un BinaryExpr)
    ///     A = 10                        (assegnazione implicita sulla riga)
    ///     LABEL:                        (etichetta nuda; NON deve essere scambiata per target assegnabile)
    ///
    /// LOGICA PRINCIPALE:
    /// 1. Early rejection: Se il token iniziale è fra quelli di separazione o controllo
    ///    (Colon, Newline, Else, ElseIf, EndIf, EndSelect, Case, Eof, LineNumber) → ritorna false.
    /// 2. Label nuda: IDENTIFIER seguito da terminatore (Newline, Colon, Else, etc.) → è un’etichetta, ritorna false.
    /// 3. Unico candidato base accettato: IDENTIFIER (eventualmente si possono aggiungere altri inizi validi:
    ///    AtSign, DollarIdent, parentesi per dereferenziazioni complesse, ecc.)
    /// 4. Esegue ParseTarget() in modalità di prova:
    ///    - Se il parsing fallisce (eccezione) → non è target assegnabile → false.
    ///    - Se riesce e il token successivo è '=' → true (assegnazione implicita).
    /// 5. In TUTTI i casi ripristina _current (posizione dei token) nel blocco finally (lookahead non consumante).
    ///
    /// PERCHÉ È NECESSARIO:
    /// - Senza questa funzione il parser potrebbe tentare di interpretare ":" o una label come inizio di un’assegnazione,
    ///   producendo errori (“Token ':' inatteso...”).
    /// - Mantiene separata la semantica di IF single-line (dove un IDENTIFIER isolato può essere salto implicito)
    ///   dalla semantica di statement di assegnazione.
    ///
    /// DIFFERENZE RISPETTO A UN PARSER LVALUE STANDARD:
    /// - Non costruisce alcun nodo AST: solo verifica la forma.
    /// - Non gestisce tipi complessi o destructuring; se servono, estendere ParseTarget.
    ///
    /// LIMITAZIONI:
    /// - Attualmente riconosce come potenziale start solo IDENTIFIER.
    /// - Numeri di riga (LineNumber) sono esclusi di proposito: "100 =" non è valido come assegnazione.
    /// - Non distingue ancora casi ambigui come: IDENTIFIER seguito da '=' usato dentro un’espressione di confronto
    ///   in un contesto dove l’assegnazione non è sintatticamente permessa. La responsabilità di scegliere
    ///   l’interpretazione corretta ricade sul chiamante (tipicamente ParseStatement).
    ///
    /// ESEMPI (true):
    /// - A = 10
    /// - Nome$ = "Test"
    /// - Contatore% = Contatore% + 1
    ///
    /// ESEMPI (false):
    /// - : A = 10          (colon iniziale → separatore)
    /// - 100 A = 10        (numero di riga → non L-value) */ // (solo se la riga logica inizia con 100)
    /// - A:                (etichetta nuda) */
    /// - ELSE A = 10       (keyword di blocco)
    /// - A < 10            (comparazione, manca '=' dopo target)
    ///
    /// ESTENSIONI FUTURE:
    /// - Supporto a TARGET complessi (es: arr(i).Campo = ...)
    /// - Supporto a destructuring (es: (A, B) = Funzione())
    ///
    /// ROBUSTEZZA:
    /// - Non solleva eccezioni in caso di forma non valida: cattura BasicException e ritorna false.
    /// - Garantisce sempre il ripristino della posizione del parser.
    /// </remarks>
    /// <returns>
    /// <c>true</c> se la sequenza corrente corrisponde ad un’assegnazione implicita (IDENTIFIER ... '='),
    /// altrimenti <c>false</c>.
    /// </returns>
    /// <example>
    /// // Assegnazione implicita valida:
    /// A = 10
    /// // Label + statement (assegnazione NON scambiata, ritorna false nel lookahead):
    /// StartLoop:
    /// // IF con comparazione: il lookahead sugli statement non deve segnare assegnazione:
    /// IF A = 10 THEN PRINT "Ok"
    /// // Separatore singolo-linea: il colon non genera assegnazione:
    /// : A = 10
    /// // GOTO implicito: IDENTIFIER seguito da terminatore → non è assegnazione:
    /// IF Flag THEN JumpLabel
    /// </example>
    private bool IsImplicitAssignment()
    {
        var saved = _current;
        var token = Peek();
        var k = token.Kind;

        if (k is TokenKind.Colon or TokenKind.Newline or TokenKind.Else or TokenKind.ElseIf
            or TokenKind.EndIf or TokenKind.EndSelect or TokenKind.Case or TokenKind.Eof
            or TokenKind.LineNumber)
            return false;

        var la1 = At(1).Kind;
        if (k == TokenKind.Identifier &&
            la1 is TokenKind.Newline or TokenKind.Colon or TokenKind.Eof
                or TokenKind.Else or TokenKind.ElseIf or TokenKind.EndIf)
            return false;

        // Usa IsValidNameToken per accettare qualsiasi token che il parser
        // considera un nome valido (es. 'KEYS', 'PRINT', 'INPUT' usati come variabili),
        // non solo Identifier, Self o Base.
        // Permissivo anche sulle keyword STRUTTURALI (parità col parser C): `step = 1`,
        // `for = 2`, … l'ambiguità con i costrutti è risolta dal lookahead su '='.
        // Accetta anche Dot per il leading-dot dei blocchi WITH...END WITH (.Member = value).
        if (!IsPermissiveNameToken(token) && token.Kind != TokenKind.Dot)
            return false;

        try
        {
            ParseTarget();
            return Check(TokenKind.Equal);
        }
        catch (BasicException)
        {
            return false;
        }
        finally
        {
            _current = saved;
        }
    }

    /// <summary>
    /// [FASE 1] Parsa una singola istruzione BASIC.
    /// Ripristina una gerarchia di dispatching pulita:
    /// 1. Riconosce i comandi legacy (DBF USE) e li invia al loro dispatcher.
    /// 2. Gestisce le keyword strutturali (IF, FOR, etc.).
    /// 3. Esegue il fallback al parsing di espressioni per chiamate a SUB e assegnazioni.
    /// </summary>
    internal IReadOnlyList<AstNode> ParseStatement(ProcedureDefinition? procedureContext)
    {
        var token = Peek();

        // Gli attributes a livello di modulo sono gestiti nel loop principale di ParseModuleLevelStatements.

        // --- Gestione CALL (zucchero sintattico opzionale) ---
        // CALL SubName è equivalente a SubName. Consuma CALL e procedi.
        // `CALL = 5` NON è una chiamata: è un'assegnazione alla variabile "CALL" (parità
        // col parser C) → lascia passare al check di assegnazione implicita.
        if (token.Kind == TokenKind.Call && At(1).Kind != TokenKind.Equal)
        {
            Advance(); // Consuma CALL
            var callExpr = ParseExpression();
            return callExpr is UnresolvedCallExpr or MethodCallExpr or FunctionExpr or BuiltinFunctionCallExpr
                or ArrayAccessExpr or GoFunctionExpr or VariableExpr or QualifiedNameExpr or IndexedAccessExpr
                or AssignmentExpr or PrefixIncrementExpr or PostfixIncrementExpr or WithMemberAccessExpr
                or LinqQueryExpr
                ? [new ExpressionStmt(callExpr)]
                : throw CreateError(Previous(), "Expected a subroutine call after CALL.", ErrorCode.SyntaxError);
        }

        // --- Gestione dell'assegnazione al valore di ritorno di un OPERATOR ---
        // Riconosce la sintassi 'OPERATOR = valore' all'interno di un blocco OPERATOR.
        if (token.Kind == TokenKind.Operator && At(1).Kind == TokenKind.Equal)
        {
            // La semantica è identica a un'assegnazione al ritorno di una funzione,
            // quindi possiamo riutilizzare lo stesso parser.
            return [ParseFunctionReturnAssignmentStatement()];
        }

        // --- @...SAY / @...GET (Clipper-style) ---
        if (token.Kind == TokenKind.AtSign)
            return [ParseAtStatement(this)];

        // CONNECTOR è sempre disponibile (F0) — non richiede IMPORT, come AI.
        // `CONNECTOR = 5` è un'assegnazione alla variabile omonima (parità col parser C).
        if (token.Kind == TokenKind.Connector && At(1).Kind != TokenKind.Dot && At(1).Kind != TokenKind.Equal)
            return [ParseConnectorStatement(this, "CONNECTOR")];
        // Se è una keyword di namespace MA non è seguita da un punto, è un COMANDO LEGACY.
        // `AI = 5`, `DBF = 5`, … sono assegnazioni alla variabile omonima → passano al check
        // di assegnazione implicita sotto (parità col parser C).
        if (IsNamespaceCommandToken(token.Kind) && At(1).Kind != TokenKind.Dot && At(1).Kind != TokenKind.Equal)
        {
            if (_program.Namespaces.Contains(GetNamespaceForToken(token.Kind)))
            {
                switch (token.Kind)
                {
                    case TokenKind.Dbf: return [ParseDbfStatement(this, "DBF")];
                    case TokenKind.Isam: return [ParseIsamStatement(this, "ISAM")];
                    case TokenKind.Rdd: return [ParseRddStatement(this, "RDD")];
                    case TokenKind.Speed: return [ParseSpeedStatement(this, "SPEED")];
                    case TokenKind.Ledger: return [ParseLedgerStatement(this, "LEDGER")];
                    case TokenKind.Sql: return [ParseSqlStatement(this, "SQL")];
                    case TokenKind.Http: return [ParseHttpStatement(this, "HTTP")];
                    case TokenKind.Tcp: return [ParseTcpStatement(this, "NET")];
                    case TokenKind.Udp: return [ParseUdpStatement(this, "NET")];
                    case TokenKind.Ssl: return [ParseSslStatement(this, "NET")];

                    case TokenKind.Chart: return [DispatchChartStatement(this, "CHART")];
                    case TokenKind.Matrix: return [DispatchMatrixStatement(this, "MATRIX")];
                    case TokenKind.Vector: return [DispatchVectorStatement(this, "VECTOR")];
                    case TokenKind.VectorDb: return [ParseVectorDbStatement(this, "VECTORDB")];
                    case TokenKind.Ai: return [ParseAiStatement(this, "AI")];
                    case TokenKind.Llm: return [ParseLlmStatement(this, "LLM")];
                    case TokenKind.Gmb: return [ParseGmbStatement(this, "GMB")];
                    case TokenKind.Train: return [ParseTrainStatement(this, "TRAIN")];
                    case TokenKind.Torch: return [ParseTorchStatement(this, "TORCH")];
                    case TokenKind.Graph: return [ParseGraphStatement(this, "GRAPH")];
                    case TokenKind.Connector: return [ParseConnectorStatement(this, "CONNECTOR")];
                }
            }
        }

        // --- WEBSOCKET COMMAND (non è un namespace, è un comando standalone) ---
        if (token.Kind == TokenKind.WebSocket && At(1).Kind == TokenKind.Connect)
        {
            return [ParseWebSocketStatement(this, "NET")];
        }
        // --- FINE NUOVA LOGICA ---

        if (token.Kind == TokenKind.Ampersand)
        {
            return [ParseMacroStatement()];
        }

        // EVAL("cmd") come STATEMENT: comando macro (come `&("cmd")`). EVAL(bBlock, args)
        // resta un'espressione-statement (invocazione del block, valore scartato).
        if (token.Kind == TokenKind.Eval)
        {
            var evalStatementExpr = ParseExpression();
            if (evalStatementExpr is EvalExpr { Args.IsEmpty: true } evalStmt)
                return [new MacroStmt(evalStmt.Target, evalStmt.Token)];
            return [new ExpressionStmt(evalStatementExpr)];
        }

        if (_functionContextStack.Count > 0)
        {
            var currentFn = _functionContextStack.Peek();
            // `FUNCTION = expr` è assegnazione al valore di ritorno SOLO dentro una procedura
            // che restituisce un valore (FUNCTION/GENERATOR): in una SUB, FUNCTION è una
            // VARIABILE (parità col parser C — Bugs/(open) - Parser-STEP-...).
            var isKeywordAssign = token.Kind == TokenKind.Function && At(1).Kind == TokenKind.Equal
                && _functionContextReturnsValue.Count > 0 && _functionContextReturnsValue.Peek();
            var isNameAssign = token.Kind == TokenKind.Identifier &&
                               token.Lexeme.Equals(currentFn.Lexeme, StringComparison.OrdinalIgnoreCase) &&
                               At(1).Kind == TokenKind.Equal;
            if (isKeywordAssign || isNameAssign)
            {
                return [ParseFunctionReturnAssignmentStatement()];
            }
        }

        if (procedureContext is not null && token.Kind == TokenKind.Function && At(1).Kind != TokenKind.Equal)
        {
            return [new LocalProcedureStmt(ParseFunctionDefinition())];
        }

        // Assegnazione implicita PRIMA del dispatch: `step = 1`, `for = 2`, `while = 3`,
        // `next = 6`, `PRINT = 5`, `AI = 5`, `FUNCTION = 5` (fuori da una FUNCTION), … sono
        // assegnazioni a variabili omonime (parità col parser C, che accetta ogni token come
        // nome — Bugs/(open) - Parser-STEP-...). Il lookahead su '=' è discriminante: nessun
        // costrutto ha la forma `KEYWORD =` (FOR i = 1 ha l'Identifier dopo FOR; CASE ELSE,
        // END SUB, DO WHILE, SELECT CASE, USING f = OPEN(…) non hanno '=' come secondo token).
        // OPERATOR = … è già gestito sopra; %NOME = … è una COSTANTE PowerBASIC (esclusa qui).
        if (At(1).Kind == TokenKind.Equal
            && IsPermissiveNameToken(token)
            && !(token.Kind == TokenKind.Identifier && token.Lexeme.StartsWith('%')))
        {
            return ParseLetStatement();
        }

        switch (token.Kind)
        {
            case TokenKind.Public or TokenKind.Private:
            {
                var visibilityToken = Peek();

                var flags = SymbolFlags.None;
                if (visibilityToken.Kind == TokenKind.Public)
                    flags |= SymbolFlags.IsPublic | SymbolFlags.IsExported;
                else
                    flags |= SymbolFlags.IsPrivate;

                var nextToken = At(1).Kind;

                // Gestisce la sequenza 'PUBLIC/PRIVATE ABSTRACT ...'
                if (nextToken == TokenKind.Abstract)
                {
                    var thirdTokenKind = At(2).Kind;
                    return thirdTokenKind switch
                    {
                        TokenKind.Class => [ParseClassDefinition()],
                        TokenKind.Function => [ParseFunctionDefinition()],
                        _ => throw CreateError(At(2), "Expected 'CLASS', 'FUNCTION', or 'SUB' after 'ABSTRACT'.")
                    };
                }
                
                if (nextToken == TokenKind.Type || nextToken == TokenKind.Packed)
                {
                    // Logica specifica per PUBLIC/PRIVATE TYPE
                    if (visibilityToken.Kind == TokenKind.Public && _functionContextStack.Count > 0)
                    {
                        throw CreateError(visibilityToken,
                            "A PUBLIC TYPE definition is not allowed inside a SUB or FUNCTION.");
                    }

                    // PRIVATE TYPE (o TYPE senza qualificatore) dentro una funzione diventa LocalTypeStmt
                    if (_functionContextStack.Count > 0)
                    {
                        return [new LocalTypeStmt(ParseTypeDefinition())];
                    }

                    // Altrimenti, è una dichiarazione a livello di modulo
                    return [ParseTypeDefinition()];
                }

                if (nextToken == TokenKind.Enum)
                {
                    Advance(); // Consuma Public/Private
                    return [ParseEnumDefinition(flags)];
                }

                if (nextToken == TokenKind.Interface)
                {
                    // Non consumare 'PUBLIC' qui, perché ParseInterfaceDefinition lo gestirà
                    return [ParseInterfaceDefinition()];
                }
                
                if (nextToken == TokenKind.Delegate)
                {
                    Advance(); // Consuma PUBLIC/PRIVATE
                    return [ParseDelegateDefinition(flags)];
                }

                // Handle PUBLIC/PRIVATE STATIC OPERATOR
                if (nextToken == TokenKind.Static && At(2).Kind == TokenKind.Operator)
                {
                    Advance(); // Consume PUBLIC/PRIVATE
                    Advance(); // Consume STATIC
                    return [ParseOperatorDefinition(flags.HasFlag(SymbolFlags.IsPublic))];
                }

                // Logica esistente per le altre dichiarazioni
                return nextToken switch
                {
                    TokenKind.Function => [ParseFunctionDefinition()],
                    TokenKind.Generator => [ParseGeneratorDefinition()],
                    TokenKind.Declare => [ParseDeclareStatement()],
                    TokenKind.Class => [ParseClassDefinition()],
                    TokenKind.Interface => [ParseInterfaceDefinition(flags)],
                    TokenKind.Dim or TokenKind.Global or TokenKind.Static or TokenKind.Var
                        or TokenKind.Const => [ParseDeclarationStatement()],
                    _ => throw CreateError(At(1),
                        $"Expected a declaration (FUNCTION, TYPE, CLASS, etc.) after '{visibilityToken.Lexeme}'.")
                };
            }
            case TokenKind.Abstract:
            {
                // 'ABSTRACT' deve essere seguito da 'CLASS' o 'FUNCTION'/'SUB'.
                var nextToken = At(1).Kind;
                if (nextToken == TokenKind.Class)
                {
                    // È 'ABSTRACT CLASS ...'
                    return [ParseClassDefinition()];
                }
                if (nextToken == TokenKind.Function)
                {
                    // È 'ABSTRACT FUNCTION/SUB ...'
                    return [ParseFunctionDefinition()];
                }

                throw CreateError(At(1), "La keyword 'ABSTRACT' può essere usata solo per modificare 'CLASS', 'FUNCTION' o 'SUB'.");
            }
            case TokenKind.Async:
            {
                // 'ASYNC' deve essere seguito da 'FUNCTION' o 'SUB'.
                Advance(); // Consuma ASYNC
                var nextToken = Peek().Kind;
                if (nextToken == TokenKind.Function)
                {
                    // È 'ASYNC FUNCTION ...'
                    return [ParseFunctionDefinition(isAsync: true)];
                }
                // SUB asincrona — gestita dal caso TokenKind.Sub sottostante
                // Qui non facciamo nulla, il parser normale gestirà SUB
                throw CreateError(Peek(), "ASYNC SUB non è ancora completamente supportato. Usare ASYNC FUNCTION.");
            }
            case TokenKind.Await:
            {
                // AWAIT come statement: AWAIT <espressione>
                Advance(); // Consuma AWAIT
                var awaitExpr = ParseExpression();
                return [new ExpressionStmt(awaitExpr)];
            }
            case TokenKind.Process: return [ParseProcessStatement()];
            case TokenKind.Import:
            case TokenKind.Require:
                return [ParseDirectiveAsComment()];
            case TokenKind.Let: return ParseLetStatement();
            case TokenKind.Dim or TokenKind.Global or TokenKind.Local or TokenKind.Static or TokenKind.Var
                or TokenKind.Const or TokenKind.Dynamic:
                return [ParseDeclarationStatement()];
            case TokenKind.Delegate: return [ParseDelegateDefinition()];
            case TokenKind.Operator: return [ParseOperatorDefinition(true)];
            case TokenKind.Function: return [ParseFunctionDefinition()];
            case TokenKind.Generator: return [ParseGeneratorDefinition()];
            case TokenKind.Enum: return [ParseEnumDefinition()];
            case TokenKind.Type or TokenKind.Packed:
            {
                // Se siamo all'interno di una procedura (SUB, FUNCTION, etc.),
                // una dichiarazione TYPE senza modificatore di visibilità è locale.
                if (_functionContextStack.Count > 0)
                {
                    return [new LocalTypeStmt(ParseTypeDefinition())];
                }

                // Altrimenti, è una dichiarazione di tipo a livello di modulo.
                return [ParseTypeDefinition()];
            }
            case TokenKind.Class: return [ParseClassDefinition()];
            case TokenKind.Unload: return [ParseUnloadStatement(this)];
            case TokenKind.Interface: return [ParseInterfaceDefinition()];
            case TokenKind.Declare: return [ParseDeclareStatement()];
            case TokenKind.If: return [ParseIfStatement(procedureContext)];
            case TokenKind.For: return ParseForStatement(procedureContext);
            case TokenKind.Do: return ParseDoHeader(procedureContext);
            case TokenKind.Draw: return [ParseDrawCommandStatement(this, "GRAPHICS")];
            case TokenKind.While: return ParseWhileHeader(procedureContext);
            case TokenKind.Next: return ParseNextStatement();
            case TokenKind.Loop: return ParseLoopStatement();
            case TokenKind.Wend: return ParseWendStatement();
            case TokenKind.Exit: return [ParseExitStatement()];
            case TokenKind.Continue: return [ParseContinueStatement()];
            case TokenKind.Select:
                var selectStmts = DispatchSelectStatement(this, procedureContext);
                if (selectStmts != null) return selectStmts;
                break;
            case TokenKind.Match:
                return [ParseMatchStatement(procedureContext)];
            case TokenKind.With:
                return [ParseWithStatement(procedureContext)];
            case TokenKind.Using:
                return [ParseUsingStatement(procedureContext)];
            case TokenKind.Tick:
                return ParseTickStatement(this, procedureContext);
            case TokenKind.Use:
                var useStmts = DispatchUseStatement(this);
                if (useStmts != null) return useStmts;
                break;
            case TokenKind.Try: return [ParseTryCatchStatement(procedureContext)];
            case TokenKind.Redim: return [ParseRedimStatement()];
            case TokenKind.Go:
                return [ParseGoStatement(this, "CONCURRENCY")];
            case TokenKind.Goto: return [ParseGotoStatement()];
            case TokenKind.Gosub: return [ParseGosubStatement()];
            case TokenKind.Return: return [ParseReturnStatement()];
            case TokenKind.On: return ParseOnStatement();
            case TokenKind.Error: return [ParseErrorStatement()];
            case TokenKind.Resume: return [ParseResumeStatement()];
            case TokenKind.Data:
                // Disambigua DATA statement vs uso di "data" come NOME variabile/array.
                // IsValidNameToken ammette keyword non-strutturali come identificatori
                // (es. `DIM data(2) AS INTEGER` compila), ma il dispatcher statement
                // trattava OGNI riga iniziante con `data` come DATA statement: così
                // `data(0) = 42` veniva parsato come `DATA (0)` e l'assegnazione spariva
                // in silenzio (array sempre vuoti, min=/max= vuoti in TypesTuple.bas).
                // DATA classico è solo `DATA <valori>`; se dopo data c'è `(` (array/call)
                // o `=` (assegnazione) è un uso da identificatore → break (cade sul
                // parsing di assegnazione implicita sotto lo switch).
                if (At(1).Kind is TokenKind.LeftParen or TokenKind.Equal)
                    break;
                return [ParseDataStatement()];
            case TokenKind.Read:
            {
                // Se READ è seguito da target variables → DATA/READ
                // Se READ è standalone (senza variabili) → Clipper READ (loop interattivo)
                var afterRead = At(1);
                if (afterRead.Kind == TokenKind.Eof || afterRead.Kind == TokenKind.Colon ||
                    afterRead.Kind == TokenKind.Newline || afterRead.Kind == TokenKind.Else || afterRead.Kind == TokenKind.EndIf)
                    return [ParseClipperReadStatement(this)];
                return [ParseReadStatement()];
            }
            case TokenKind.Restore: return [ParseRestoreStatement()];
            case TokenKind.Randomize: return [ParseRandomizeStatement(this)];
            case TokenKind.Swap: return [ParseSwapStatement()];
            case TokenKind.Stop: return [ParseStopStatement()];
            case TokenKind.Run: return [ParseRunStatement()];
            case TokenKind.End: return [ParseEndStatement()];
            case TokenKind.Sound:
            case TokenKind.Play:
                return DispatchAudioStatement(this);
            case TokenKind.Filter:
                return [ParseFilterStatement(this)];
            case TokenKind.Box:
                return [ParseBoxStatement(this)];
            case TokenKind.Scroll:
                return [ParseScrollStatement(this)];
            case TokenKind.Print or TokenKind.QuestionMark:
                return new List<AstNode> { DispatchPrintStatement(this, "CONSOLE") };
            case TokenKind.Yield: return [ParseYieldStatement()];
            case TokenKind.Raise:
                return [ParseRaiseEventStatement(this, null)];
            case TokenKind.Throw: return [ParseThrowStatement()];
            case TokenKind.Add:
                // Lookahead per distinguere ADD HANDLER da altri usi futuri di ADD
                if (At(1).Kind == TokenKind.Handler)
                    return [ParseAddHandlerStatement(this, null)];
                break;
            case TokenKind.View:
                // VIEW PRINT [topRow TO bottomRow] — scrolling region.
                // Se omesso, resetta a schermo intero.
                if (At(1).Kind == TokenKind.Print || At(1).Kind == TokenKind.ViewPrint)
                {
                    Advance(); // consuma VIEW
                    Advance(); // consuma PRINT
                    var viewPrintToken = Previous();
                    if (!IsAtEndOfStatement() && !Check(TokenKind.Colon))
                    {
                        var topExpr = ParseExpression();
                        Consume(TokenKind.To, "Expected TO after top row.");
                        var bottomExpr = ParseExpression();
                        return [new ViewPrintStmt(topExpr, bottomExpr, viewPrintToken)];
                    }
                    return [new ViewPrintStmt(null, null, viewPrintToken)];
                }
                break;
            case TokenKind.Remove:
                // Lookahead per distinguere REMOVE HANDLER
                if (At(1).Kind == TokenKind.Handler)
                    return [ParseRemoveHandlerStatement(this, null)];
                break;
        }

        // --- COSTANTE STILE POWERBASIC: %NOME = valore ---
        // Un identificatore il cui lessema inizia con '%' seguito da '=' è una DICHIARAZIONE di
        // costante (equivalente a CONST NOME AS INTEGER = valore), non un'assegnazione — il '%'
        // iniziale ha lo stesso significato del suffisso finale "nome%" (INTEGER) già esistente,
        // applicato qui come prefisso in stile PowerBASIC. Va intercettato PRIMA di
        // IsImplicitAssignment, che altrimenti tratterebbe "%NOME = 5" come un'assegnazione
        // implicita a una variabile chiamata letteralmente "%NOME".
        if (token.Kind == TokenKind.Identifier && token.Lexeme.StartsWith('%') && At(1).Kind == TokenKind.Equal)
        {
            return [ParsePercentConstantDeclaration()];
        }

        // Questo permette di usare keyword di comandi (es. DONE, PRINT) come nomi di variabili
        // se seguite da '=', risolvendo l'ambiguità.
        if (IsImplicitAssignment())
        {
            return ParseLetStatement();
        }

        var parsedStmt = DispatchRegisteredCommand();
        if (parsedStmt != null)
        {
            return [parsedStmt];
        }

        // Fallback "senza prefisso": prova i subcomandi dei namespace attivi.
        var bareStmt = TryParseBareNamespaceCommand();
        if (bareStmt != null)
        {
            return [bareStmt];
        }

        // Controlla se è un'assegnazione di tupla: (x, y, z) = expr
        if (token.Kind == TokenKind.LeftParen && IsTupleAssignment())
        {
            return [ParseTupleAssignmentStatement()];
        }

        // Fallback finale: se non è un comando noto, prova a parsarlo come un'espressione
        // (che potrebbe essere una chiamata a SUB o a una funzione qualificata).
        // TokenKind.Dot è permesso per il leading-dot dei blocchi WITH...END WITH.
        // Permissivo sulle keyword strutturali: `step(0)` è una chiamata/indicizzazione e
        // ParsePrimaryAtom le accetta come VariableExpr; una keyword NUDA resta rifiutata
        // dal controllo sui tipi di espressione qui sotto (VariableExpr non è statement).
        if (!IsPermissiveNameToken(token) && !IsNamespaceCommandToken(token.Kind) && token.Kind != TokenKind.Dot)
            throw CreateError(token, $"Token '{token.Lexeme}' inatteso all'inizio di un'istruzione.");
        var expression = ParseExpression();
        // NOTA: una variabile o un nome qualificato da soli NON sono statement validi
        // (chiamare una SUB senza parentesi non è legittimo: serve Nome() o CALL Nome).
        // Accettarli produceva ExpressionStmt no-op silenziosi e mascherava sequenze
        // senza operatore (es. 'X Y Z' -> tre statement da variabili nude).
        return expression is UnresolvedCallExpr or MethodCallExpr or FunctionExpr or BuiltinFunctionCallExpr
            or ArrayAccessExpr or GoFunctionExpr or IndexedAccessExpr
            or AssignmentExpr or PrefixIncrementExpr or PostfixIncrementExpr or WithMemberAccessExpr
            or LinqQueryExpr
            ? ([new ExpressionStmt(expression)])
            : throw CreateError(token, $"Token '{token.Lexeme}' inatteso all'inizio di un'istruzione.");
    }

    /// <summary>
    /// Tenta di risolvere e parsare un'istruzione basata su keyword.
    /// Dispatcher centrale per tutti i comandi (solo sintassi legacy/non qualificata).
    /// </summary>
    private Stmt? DispatchRegisteredCommand()
    {
        const bool log = false;
        Log($"[DispatchRegisteredCommand] ENTER. Current: {Peek()}", log);

        // --- GESTIONE SINTASSI NON QUALIFICATA (Legacy) ---
        var unqualifiedCommandToken = Peek();
        // Se il token di comando è seguito da un punto (es. `MUTEX.TRYLOCK(mu)`), NON è un
        // comando legacy ma una chiamata qualificata: il fallback del chiamante la parsa come
        // espressione (QualifiedNameExpr → funzione builtin del namespace). Senza questo check,
        // `MUTEX.TRYLOCK` finiva in ParseMutexStatement (che si aspetta LOCK/UNLOCK dopo MUTEX).
        if (At(1).Kind == TokenKind.Dot)
            return null;
        var lookupKind = unqualifiedCommandToken.Kind;

        // Comandi registrati per NOME/LESSEMA (non-keyword): es. i controlli pixel e
        // CONTROL (senza TokenKind dedicato). Solo namespace attivi (regola ferrea).
        if (unqualifiedCommandToken.Lexeme != null &&
            ParserRegistries.NamedCommandRegistry.TryGetValue(unqualifiedCommandToken.Lexeme, out var namedRegs) &&
            namedRegs.Count > 0)
        {
            // Controlli pixel + CONTROL + FORM_* (per nome, default-active). Ma se lo stesso
            // lexeme ha una registrazione TokenKind ATTIVA (es. TUI importato: TOGGLE/GRID/...),
            // cede a quella (semantica TUI). Senza IMPORT TUI, vince il pixel (GUI).
            bool tokenKindActive = ParserRegistries.UnifiedCommandRegistry.TryGetValue(unqualifiedCommandToken.Kind, out var tkRegs)
                && tkRegs.Any(r => _program.Namespaces.Contains(r.ProvidingNamespace));
            if (!tokenKindActive) return namedRegs[0].ParseFunction(this, namedRegs[0].ProvidingNamespace);
        }

        if (unqualifiedCommandToken.Kind == TokenKind.Identifier &&
            _registry.CommandNameToTokenKindMap.TryGetValue(unqualifiedCommandToken.Lexeme, out var mappedKind))
        {
            lookupKind = mappedKind;
        }

        if (ParserRegistries.UnifiedCommandRegistry.TryGetValue(lookupKind, out var allRegistrations) &&
            allRegistrations.Count > 0)
        {
            // REGOLA FERREA (stessa di TryParseBareNamespaceCommand): nessun comando di un
            // namespace funziona senza che il namespace sia ATTIVO (importato esplicitamente
            // o di default/settings). Un comando legacy (es. `MUTEX LOCK`, `GO SUB`) è nel
            // namespace che lo registra (es. CONCURRENCY): se quel namespace non è importato,
            // il comando NON esiste qui. Prima del filtro, il dispatch ignorava la visibilità
            // e un comando di un namespace non importato girava comunque (bug di parità).
            CommandRegistration? active = null;
            foreach (var reg in allRegistrations)
            {
                if (!_program.Namespaces.Contains(reg.ProvidingNamespace))
                    continue;
                active = reg;
                break;
            }
            if (active == null)
                return null; // comando di namespace non attivo: lascia il flusso al fallback

            if (allRegistrations.Count == 1)
            {
                // Il parser di Fase 1 non deve preoccuparsi della visibilità (legacy vs. import).
                // Deve solo trovare e invocare il parser registrato per la keyword.
                return active.ParseFunction(this, active.ProvidingNamespace);
            }

            // Comando polimorfico
            // Se ci sono più parser (comando polimorfico come CLOSE, GET, etc.),
            // invochiamo il primo. Sarà responsabilità del parser specifico (il dispatcher
            // polimorfico) eseguire il lookahead per decidere quale azione intraprendere.
            var dispatcher = active.ParseFunction;
            return dispatcher(this, string.Empty);
        }

        Log($"[DispatchRegisteredCommand] EXIT. No command match found. Next token: {Peek()}", log);
        return null;
    }

    /// <summary>
    /// Fallback "senza prefisso": prova a parsare il comando corrente come subcomando dei
    /// namespace ATTIVI (importati esplicitamente o di default/settings — regola ferrea:
    /// nessun comando funziona senza il suo namespace attivo). Per ogni namespace:
    /// - tenta il suo dispatcher di subcomandi;
    /// - se il parser consuma l'ISTRUZIONE PER INTERO → è quello giusto (niente conteggio
    ///   di duplicati);
    /// - altrimenti ripristina il puntatore (backtracking) e prova il namespace successivo.
    /// </summary>
    private Stmt? TryParseBareNamespaceCommand()
    {
        var token = Peek();
        // Un identificatore puro non è un subcomando: lascia il flusso alle variabili/espressioni.
        if (token.Kind == TokenKind.Identifier)
            return null;

        foreach (var (ns, dispatcher) in ParserRegistries.NamespaceSubcommandDispatchers)
        {
            // Regola ferrea: solo namespace attivi (importati o di default/settings).
            if (!_program.Namespaces.Contains(ns))
                continue;

            var saved = _current;
            try
            {
                var stmt = dispatcher(this, ns);
                if (IsAtEndOfStatement())
                    return stmt;
            }
            catch
            {
                // fallito: ripristina il puntatore e prova il namespace successivo
            }
            _current = saved;
        }
        return null;
    }

    // ========================================================================
    // --- Metodi di Parsing Specializzati per la Fase 1 ---
    // ========================================================================

    /// <summary>
    /// [FASE 1] Analizza un blocco di istruzioni fino a incontrare un token di terminazione specificato.
    /// Questo metodo è il motore di parsing principale per i corpi di tutti i costrutti a blocco
    /// (IF/THEN/ELSE, FOR/NEXT, DO/LOOP, WHILE/WEND, SELECT/CASE, TRY/CATCH/FINALLY).
    /// È stato reso robusto per prevenire loop infiniti in scenari con blocchi annidati.
    /// </summary>
    /// <remarks>
    /// Logica di Funzionamento:
    /// Il metodo opera in un loop che continua finché non viene raggiunto un token di stop o la fine del file.
    /// 1. Pulizia Iniziale: Consuma tutte le righe vuote (`Newline`) e i numeri di riga.
    /// 2. Condizione di Uscita: Se il token corrente corrisponde a uno dei `stopKinds` forniti, il loop termina.
    /// 3. Delega e Controllo Anti-Loop:
    ///    - La posizione dei token viene salvata prima di delegare il parsing a `ParseLogicalLine`.
    ///    - Dopo la chiamata, la posizione viene ricontrollata. Se non è avanzata, significa che
    ///      `ParseLogicalLine` ha incontrato un token di fine blocco (es. `END IF`) che non sa gestire
    ///      e si è fermato. Per evitare un loop infinito, `ParseBlockBodyUntil` interrompe
    ///      il proprio ciclo, lasciando il token di blocco sul flusso per essere gestito dal parser corretto.
    /// </remarks>
    /// <param name="procedureContext">Il contesto della procedura corrente, usato per validare l'assenza di numeri di riga.</param>
    /// <param name="stopAtIfKeywords">Se true, la chiamata interna a `ParseLogicalLine` si fermerà prima di consumare keyword come `ELSE` o `END IF`.</param>
    /// <param name="stopKinds">Un array di `TokenKind` che, se incontrati, causano l'immediata terminazione del parsing del blocco.</param>
    /// <returns>Un array immutabile di nodi `Stmt` che costituiscono il corpo del blocco parsato.</returns>
    private ImmutableArray<Stmt> ParseBlockBodyUntil(ProcedureDefinition? procedureContext,
        params TokenKind[] stopKinds)
    {
        const bool log = false; // Abilita i log per questo metodo
        Log(
            $"[DEBUG] >>> ENTER ParseBlockBodyUntil. stopKinds=[{string.Join(", ", stopKinds)}]. Current: {Peek()}",
            log);

        var statementsBuilder = new List<Stmt>();
        var declarationsBuilder = new List<AstNode>();

        var savedBlockFlag = _insideMultiLineBlock;
        _insideMultiLineBlock = true;
        try
        {
        while (!IsAtEnd())
        {
            Log($"[DEBUG]     Loop iteration. Current: {Peek()}", log);

            // FASE 1: Ciclo di pulizia.
            var consumedSomethingInClean = false;
            while (true)
            {
                if (Match(TokenKind.Newline))
                {
                    Log("[DEBUG]       Consumed Newline.", log);
                    consumedSomethingInClean = true;
                    continue;
                }

                if (Check(TokenKind.LineNumber))
                {
                    if (procedureContext != null)
                    {
                        throw CreateError(Peek(), "Line numbers are not allowed inside a FUNCTION or SUB block.");
                    }

                    Advance();
                    Log("[DEBUG]       Consumed LineNumber.", log);
                    consumedSomethingInClean = true;
                    continue;
                }

                break;
            }

            if (consumedSomethingInClean) Log($"[DEBUG]     After clean loop. Current: {Peek()}", log);

            // FASE 2: Condizione di uscita.
            if (stopKinds.Any(Check) || IsAtEnd())
            {
                Log($"[DEBUG]     Stop condition met. Current: {Peek()}. Breaking loop.", log);
                break;
            }

            // --- Logica Anti-Loop Infinito ---
            var positionBeforeParse = _current;

            Log("[DEBUG]     Calling ParseLogicalLine...", log);
            ParseLogicalLine(statementsBuilder, declarationsBuilder, procedureContext);
            Log($"[DEBUG]     Returned from ParseLogicalLine. Current: {Peek()}", log);

            if (_current == positionBeforeParse)
            {
                // ParseLogicalLine si e' fermato su un delimitatore che NESSUN blocco attende qui (gli stopKinds sono gia' stati
                // controllati sopra): ELSE/ELSEIF/END IF/END SELECT/CASE orfani sono un errore, non da ignorare in silenzio.
                ThrowIfStrayBlockTerminator();
                // Altro token non avanzato: lo consumiamo per evitare un loop infinito.
                Advance();
            }
        }

        Log($"[DEBUG] <<< EXIT ParseBlockBodyUntil. Parsed {statementsBuilder.Count} statements.", log);
        return [.. statementsBuilder];
        }
        finally
        {
            _insideMultiLineBlock = savedBlockFlag;
        }
    }

    /// <summary>
    /// Verifica se il token corrente è l'inizio di un'assegnazione di tupla: (id, id, ...) = expr
    /// </summary>
    private bool IsTupleAssignment()
    {
        // Lookahead: (identifier, identifier, ...) = expr
        int pos = _current;
        if (pos >= _tokens.Length || _tokens[pos].Kind != TokenKind.LeftParen)
            return false;

        pos++; // Salta (
        int identCount = 0;

        // Conta gli identificatori separati da virgole
        while (pos < _tokens.Length)
        {
            if (_tokens[pos].Kind == TokenKind.Identifier || _tokens[pos].Kind == TokenKind.Nil)
            {
                identCount++;
                pos++;

                // Dopo un identificatore, controlla se c'è una virgola o una )
                if (pos < _tokens.Length && _tokens[pos].Kind == TokenKind.Comma)
                {
                    pos++; // Salta ,
                    continue;
                }

                if (pos < _tokens.Length && _tokens[pos].Kind == TokenKind.RightParen)
                {
                    pos++; // Salta )
                    // Dopo ) deve esserci =
                    if (pos < _tokens.Length && _tokens[pos].Kind == TokenKind.Equal && identCount >= 2)
                        return true;
                }
            }

            return false;
        }

        return false;
    }

    /// <summary>
    /// Parsa un'assegnazione di tupla: (x, y, z) = expr
    /// </summary>
    private Stmt ParseTupleAssignmentStatement()
    {
        var openParen = Consume(TokenKind.LeftParen, "Expected '(' for tuple assignment.");
        var variables = ImmutableArray.CreateBuilder<Token>();

        do
        {
            variables.Add(Consume(TokenKind.Identifier, "Expected variable name in tuple assignment."));
        } while (Match(TokenKind.Comma));

        Consume(TokenKind.RightParen, "Expected ')' after variable list.");
        Consume(TokenKind.Equal, "Expected '=' after variable list.");
        var expression = ParseExpression();

        return new TupleAssignmentStmt(variables.ToImmutable(), expression, openParen);
    }

    /// <summary>
    /// Parsa una lista di attributes nella sintassi &lt;AttributeName&gt; a livello di statement.
    /// Accetta sia Identifier che keyword (es. Serializable, Required) come nomi di attribute.
    /// Non consuma &lt;SingleIdentifier&gt; seguito da , o WHERE (che sono type parameters).
    /// </summary>
    private ImmutableArray<Token> ParseStatementAttributes()
    {
        var attributes = ImmutableArray.CreateBuilder<Token>();
        while (Check(TokenKind.LessThan))
        {
            // Guarda oltre: se dopo < c'è un identificatore seguito da >, , o WHERE,
            // potrebbe essere un type parameter <T> o <T, U> — non un attribute.
            // Gli attributes sono semplici: <Name> senza virgole o clausole.
            if (At(1).Kind == TokenKind.Identifier && At(2).Kind == TokenKind.GreaterThan)
            {
                // Potrebbe essere <T> type parameter — non consumare come attribute.
                // Lascia che il chiamatore gestisca i type parameters.
                break;
            }

            Advance();
            var nameToken = Peek();
            if (nameToken.Kind == TokenKind.Identifier || nameToken.Kind == TokenKind.Serializable || nameToken.Kind == TokenKind.Required)
            {
                Advance();
                attributes.Add(nameToken);
            }
            else
            {
                throw CreateError(nameToken, $"Expected attribute name after '<', got '{nameToken.Lexeme}'.");
            }
            Consume(TokenKind.GreaterThan, "Expected '>' after attribute name.");
        }
        return attributes.ToImmutable();
    }

    /// <summary>PROCESS &lt;file&gt; PROMPT &lt;prompt&gt; (E0): invia un documento all'agente con un
    /// prompt di elaborazione — dispatch per estensione (pdf/xlsx/csv/html/md/json/testo) → testo,
    /// poi l'agente esegue le istruzioni (riassunto, RAG, SQL, report). Compilato come builtin call
    /// (BuiltinFunctionCallExpr) così tutti i backend (IL/PCode/C99/VM/C) la trattano via il dispatch
    /// builtin condiviso. `PROMPT` può essere seguito da un `:` opzionale.</summary>
    private Stmt ParseProcessStatement()
    {
        var procToken = Consume(TokenKind.Process, "Expected 'PROCESS'.");
        var fileExpr = ParseExpression();
        Expr? promptExpr = null;
        if (Check(TokenKind.Prompt))
        {
            Advance(); // PROMPT
            if (Check(TokenKind.Colon)) Advance(); // ':' opzionale dopo PROMPT
            promptExpr = ParseExpression();
        }
        var call = new BuiltinFunctionCallExpr(
            new Token(TokenKind.Identifier, "", null, procToken.Position),
            new Token(TokenKind.Process, "PROCESS", null, procToken.Position),
            promptExpr != null ? [fileExpr, promptExpr] : [fileExpr],
            new VoidType());
        return new ExpressionStmt(call);
    }
}
