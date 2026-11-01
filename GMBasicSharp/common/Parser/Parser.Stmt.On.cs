// FILE: Interpreter.Parser.Stmt.On.cs

// ReSharper disable InvalidXmlDocComment

using System.Collections.Immutable;

namespace Basic;

/// <summary>
///     Contiene i metodi per il parsing delle singole istruzioni, definizioni e clausole del linguaggio BASIC.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parsa un'istruzione ON, distinguendo tra le varianti di salto calcolato, i gestori di eventi asincroni
    /// e la gestione degli errori (legacy e moderna).
    /// <para>Sintassi ammesse:</para>
    /// <code>
    /// ' 1. Salto calcolato (Computed Jump)
    /// ON espressione GOTO etichetta1, etichetta2, ...
    /// ON espressione GOSUB etichetta1, etichetta2, ...
    /// 
    /// ' 2. Gestione Errori Legacy (Jump-based)
    /// ON ERROR GOTO etichetta
    /// ON ERROR GOSUB numero_linea
    /// ON ERROR RESUME NEXT
    /// 
    /// ' 3. Gestione Errori Moderna (Procedure-based - TODO #72)
    /// ON ERROR NomeProcedura()  ' Invocazione di un handler indipendente
    /// 
    /// ' 4. Gestione Eventi Input/Sistema (Async Handlers)
    /// ON KEY(codice) GOSUB etichetta
    /// ON MOUSE evento azione (es. ON MOUSE CLICK GOSUB ...)
    /// ON WINDOW evento azione (es. ON WINDOW RESIZE GOTO ...)
    /// </code>
    /// </summary>
    /// <returns>Una lista contenente il nodo AST (singolo) che rappresenta l'istruzione ON specifica.</returns>
    private List<Stmt> ParseOnStatement()
    {
        var onToken = Consume(TokenKind.On, "Expected 'ON'.");

        // 1. Gestione Eventi: ON KEY
        if (Match(TokenKind.Key))
        {
            return [ParseOnKeyStatement(onToken)];
        }

        // 2. Gestione Eventi: ON MOUSE
        if (Match(TokenKind.Mouse))
        {
            return [ParseOnMouseStatement(onToken)];
        }

        // 2a. Gestione Eventi: ON MESSAGE (namespace MESSAGE)
        if (Match(TokenKind.Message))
        {
            return [ParseOnMessageStatement(onToken)];
        }

        // 2a-bis. Gestione Eventi: ON SIGNAL (namespace SIGNAL — comando per nome, non keyword)
        if (At(0).Lexeme != null && At(0).Lexeme.Equals("SIGNAL", StringComparison.OrdinalIgnoreCase))
        {
            Advance(); // SIGNAL
            return [ParseOnSignalStatement(onToken)];
        }

        // 2b. Gestione Eventi: ON SPRITE COLLISION / ON SPRITE COLLISIONS
        if (Match(TokenKind.Sprite))
        {
            return [ParseOnSpriteCollisionStatement(onToken)];
        }

        // 2c. Eventi controllo raggruppati: ON <EVENTO> CONTROL <id> <azione>
        //     (es. ON CLICK CONTROL h1 CALL OnOk()). Richiede l'evento (CLICK/CHANGE/...) + CONTROL.
        if (At(0).Lexeme != null && PixelEvents.Contains(At(0).Lexeme)
            && string.Equals(At(1).Lexeme, "CONTROL", StringComparison.OrdinalIgnoreCase))
        {
            return [ParseOnControlEventStatement(onToken)];
        }

        // 3. Gestione Eventi: ON WINDOW
        if (Match(TokenKind.Window))
        {
            return [ParseOnWindowStatement(onToken)];
        }

        // 4. Gestione Errori: ON ERROR ...
        if (Match(TokenKind.Error))
        {
            // 4a. Varianti Legacy GOTO / GOSUB
            if (Check(TokenKind.Goto) || Check(TokenKind.Gosub))
            {
                var keyword = Advance();
                var target = Consume(TokenKind.Number, TokenKind.Identifier);

                // Registra l'handler di salto (ProcedureCall = null).
                return [new OnErrorStmt(keyword, target, null, onToken)];
            }

            // 4b. Variante In-line: ON ERROR RESUME NEXT
            if (Match(TokenKind.Resume))
            {
                Consume(TokenKind.Next, "Expected 'NEXT' after 'ON ERROR RESUME'.");
                return [new OnErrorResumeStmt(onToken)];
            }

            // 4c. Variante Procedurale
            // Se non è un comando di salto noto, tentiamo di parsare una chiamata a funzione/sub.
            // Il Linker si occuperà di validare che l'espressione sia effettivamente una chiamata valida.
            var procedureCall = ParseExpression();
            return [new OnErrorStmt(null, null, procedureCall, onToken)];
        }

        // 5. Salto Calcolato: ON <expr> GOTO/GOSUB
        var expression = ParseExpression();

        if (!Match(TokenKind.Goto, TokenKind.Gosub))
            throw CreateError(Peek(), "Expected GOTO or GOSUB after expression in ON statement.");

        var actionKeyword = Previous();
        var jumpType = actionKeyword.Kind == TokenKind.Gosub ? JumpType.Gosub : JumpType.Goto;

        var targets = ImmutableArray.CreateBuilder<Token>();
        do
        {
            // I target del salto calcolato possono essere numeri di riga o etichette testuali.
            targets.Add(Consume(TokenKind.Number, TokenKind.Identifier));
        } while (Match(TokenKind.Comma));

        var onJumpStmt = new OnJumpStmt(expression, jumpType, targets.ToImmutable(), onToken);

        return [onJumpStmt];
    }

    /// <summary>
    /// ON SPRITE COLLISION <handler> / ON SPRITE COLLISIONS <handler>.
    /// </summary>
    private OnSpriteCollisionStmt ParseOnSpriteCollisionStatement(Token onToken)
    {
        Token kind;
        if (Check(TokenKind.SpriteCollisionEvent))
            kind = Advance();
        else if (Check(TokenKind.SpriteCollisionsEvent))
            kind = Advance();
        else
            throw CreateError(Peek(), "Expected COLLISION or COLLISIONS after 'ON SPRITE'.");
        var handler = Consume(TokenKind.Identifier, "Expected handler name after ON SPRITE COLLISION(S).");
        return new OnSpriteCollisionStmt(handler.Lexeme, kind.Kind == TokenKind.SpriteCollisionsEvent, onToken);
    }

    /// <summary>
    /// Parsa un'istruzione ON KEY.
    /// <para>Sintassi ammesse:</para>
    /// <code>
    /// ON KEY(espressione_tasto) GOTO etichetta
    /// ON KEY(espressione_tasto) GOSUB etichetta
    /// ON KEY(espressione_tasto) nome_sub [arg1, arg2, ...]
    /// ON KEY(espressione_tasto) STOP
    /// ON KEY STOP
    /// </code>
    /// </summary>
    /// <param name="onToken">Il token 'ON' iniziale, per riferimento a linea/colonna.</param>
    /// <returns>Un nodo <see cref="OnKeyStmt"/> che rappresenta l'istruzione.</returns>
    private OnKeyStmt ParseOnKeyStatement(Token onToken)
    {
        Expr? keyExpr = null;
        OnKeyAction action;

        // Sintassi: ON KEY(expr) ...
        if (Match(TokenKind.LeftParen))
        {
            keyExpr = ParseExpression();
            Consume(TokenKind.RightParen, "Expected ')' after key expression.");

            // Utilizza l'helper condiviso con ON MOUSE / ON WINDOW
            // per parsare GOTO, GOSUB, STOP o una chiamata a procedura.
            action = ParseEventAction();
        }
        // Sintassi Legacy: ON KEY STOP (disabilita tutti gli eventi tastiera)
        else if (Match(TokenKind.Stop))
        {
            action = new StopAction();
        }
        else
        {
            throw CreateError(Peek(), "Expected '(' or 'STOP' after ON KEY.");
        }

        return new OnKeyStmt(keyExpr, action, onToken);
    }

    /// <summary>
    /// Parsa un'istruzione ON MOUSE per la registrazione di gestori di eventi del mouse.
    /// Configura un interrupt asincrono che si attiva quando si verifica l'evento specificato.
    /// </summary>
    /// <remarks>
    /// Sintassi: ON MOUSE Evento Azione
    /// 
    /// Eventi supportati:
    /// - CLICK: Click sinistro completo (pressione e rilascio).
    /// - DBLCLICK: Doppio click sinistro.
    /// - DOWN: Pressione di un qualsiasi tasto del mouse.
    /// - UP: Rilascio di un tasto del mouse.
    /// - MOVE: Movimento del cursore.
    /// - SCROLL: Movimento della rotella di scorrimento.
    /// 
    /// Azioni supportate:
    /// - GOTO label: Salta incondizionatamente a un'etichetta.
    /// - GOSUB label: Esegue una subroutine locale e ritorna al punto di interruzione.
    /// - CALL NomeSub (o solo NomeSub): Invoca una procedura specificata.
    /// - STOP: Disabilita la gestione dell'evento specificato.
    /// </remarks>
    /// <example>
    /// Esempi di utilizzo:
    /// ON MOUSE CLICK GOSUB GestoreClick
    /// ON MOUSE MOVE CALL AggiornaCursore
    /// ON MOUSE SCROLL GestoreScroll
    /// ON MOUSE DBLCLICK STOP
    /// </example>
    /// <param name="onToken">Il token 'ON' iniziale, per riferimento alla posizione nel codice.</param>
    /// <returns>Un nodo OnMouseStmt configurato.</returns>
    private OnMouseStmt ParseOnMouseStatement(Token onToken)
    {
        MouseEventKind eventKind;

        if (Match(TokenKind.Click)) eventKind = MouseEventKind.Click;
        else if (Match(TokenKind.DblClick)) eventKind = MouseEventKind.DblClick;
        else if (Match(TokenKind.Move)) eventKind = MouseEventKind.Move;
        else if (Match(TokenKind.Scroll)) eventKind = MouseEventKind.Scroll;
        else if (Match(TokenKind.Down)) eventKind = MouseEventKind.Down;
        else if (Match(TokenKind.Up)) eventKind = MouseEventKind.Up;
        else throw CreateError(Peek(), "Expected mouse event type (CLICK, DBLCLICK, MOVE, SCROLL, DOWN, UP).");

        var action = ParseEventAction();
        return new OnMouseStmt(eventKind, action, onToken);
    }

    /// <summary>
    /// Parsa un'istruzione ON WINDOW per la registrazione di gestori di eventi della finestra.
    /// Configura un interrupt asincrono che si attiva su cambiamenti di stato della finestra grafica.
    /// </summary>
    /// <remarks>
    /// Sintassi: ON WINDOW Evento Azione
    /// 
    /// Eventi supportati:
    /// - RESIZE: La finestra è stata ridimensionata dall'utente o dal sistema.
    /// - QUIT: L'utente ha richiesto la chiusura della finestra (es. clic sulla X).
    /// 
    /// Azioni supportate:
    /// - GOTO label: Salta incondizionatamente a un'etichetta.
    /// - GOSUB label: Esegue una subroutine locale e ritorna al punto di interruzione.
    /// - CALL NomeSub (o solo NomeSub): Invoca una procedura specificata.
    /// - STOP: Disabilita la gestione dell'evento specificato.
    /// </remarks>
    /// <example>
    /// Esempi di utilizzo:
    /// ON WINDOW QUIT GOSUB GestoreUscita
    /// ON WINDOW RESIZE CALL RicalcolaLayout
    /// ON WINDOW QUIT STOP
    /// </example>
    /// <param name="onToken">Il token 'ON' iniziale, per riferimento alla posizione nel codice.</param>
    /// <returns>Un nodo OnWindowStmt configurato.</returns>
    private OnWindowStmt ParseOnWindowStatement(Token onToken)
    {
        WindowEventKind eventKind;

        if (Match(TokenKind.Close)) eventKind = WindowEventKind.Close;
        else if (Match(TokenKind.Resize)) eventKind = WindowEventKind.Resize;
        else if (Match(TokenKind.Quit)) eventKind = WindowEventKind.Quit;
        else if (Match(TokenKind.Move)) eventKind = WindowEventKind.Move;
        else if (Match(TokenKind.Focus)) eventKind = WindowEventKind.Focus;
        else if (Match(TokenKind.Minimize)) eventKind = WindowEventKind.Minimize;
        else if (Match(TokenKind.Restore)) eventKind = WindowEventKind.Restore;
        else if (Match(TokenKind.Open)) eventKind = WindowEventKind.Open;
        else if (Match(TokenKind.Paste)) eventKind = WindowEventKind.Paste;
        else throw CreateError(Peek(), "Expected window event type (CLOSE, RESIZE, QUIT, MOVE, FOCUS, MINIMIZE, RESTORE, OPEN, PASTE).");

        Expr? ofWin = null;
        if (Match(TokenKind.Of)) ofWin = ParseExpression();
        var action = ParseEventAction();
        return new OnWindowStmt(eventKind, action, onToken, ofWin);
    }

    /// <summary>
    /// Parsa l'azione di un evento (GOTO, GOSUB, CALL, STOP).
    /// Helper condiviso per ON KEY, ON MOUSE, ON WINDOW.
    /// </summary>
    private OnKeyAction ParseEventAction()
    {
        if (Match(TokenKind.Goto))
        {
            var target = Consume(TokenKind.Identifier, TokenKind.Number);
            return new GotoAction(target);
        }

        if (Match(TokenKind.Gosub))
        {
            var target = Consume(TokenKind.Identifier, TokenKind.Number);
            return new GosubAction(target);
        }

        if (Match(TokenKind.Stop))
        {
            return new StopAction();
        }

        // Consuma opzionalmente il token CALL se presente
        Match(TokenKind.Call);

        // Parsa l'espressione che deve rappresentare la chiamata
        var callExpr = ParseExpression();

        // Validazione: deve essere sintatticamente una chiamata (es. con parentesi).
        // In Fase 1, una chiamata a UDF appare spesso come ArrayAccessExpr.
        if (callExpr is FunctionExpr or BuiltinFunctionCallExpr or MethodCallExpr or ArrayAccessExpr)
        {
            return new CallAction(callExpr);
        }

        // Nome nudo senza parentesi (`ON MOUSE Click CALL MiaSub`): è una chiamata a
        // procedura a ZERO argomenti. La leg C la accetta e i campioni la usano; la si
        // normalizza in una FunctionExpr senza argomenti, che il linker risolve come una
        // normale chiamata (candidati per nome + compatibilità di firma).
        if (callExpr is VariableExpr ve)
        {
            return new CallAction(new FunctionExpr(-1, ve.NameCleaned, ImmutableArray<Expr>.Empty, null));
        }

        throw CreateError(Peek(), "Attesa un'istruzione di salto (GOTO, GOSUB), STOP o una chiamata a procedura.");
    }

    /// <summary>`ON &lt;EVENTO&gt; CONTROL &lt;id&gt; &lt;azione&gt;` — gestore evento su un controllo
    /// per handle (sovrascrive l'inline). Azione: CALL/GOSUB/GOTO/STOP.</summary>
    private Stmt ParseOnControlEventStatement(Token onToken)
    {
        var ev = (Peek().Lexeme ?? "").ToUpperInvariant(); Advance();
        if (!string.Equals(Peek().Lexeme, "CONTROL", StringComparison.OrdinalIgnoreCase))
            throw CreateError(Peek(), "Atteso CONTROL dopo l'evento.");
        Advance();
        var id = ParseExpression();
        var cl = ParseControlEventClause(ev);
        var target = cl.ActionKind == "CALL" ? cl.SubName : cl.LabelName;
        return new ControlEventStmt(ev, id, cl.ActionKind, target, onToken, _lastEvTargetToken);
    }
}