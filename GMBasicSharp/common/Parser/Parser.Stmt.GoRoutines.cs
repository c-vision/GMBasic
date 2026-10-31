// FILE: Interpreter.Parser.Stmt.GoRoutines.cs

using System.Collections.Immutable;

namespace Basic;
/*
------------------------------------------------------------------------------------------------------------------------
     GUIDA DI RIFERIMENTO AI COMANDI DEL NAMESPACE 'CONCURRENCY'
------------------------------------------------------------------------------------------------------------------------
Questo file contiene i metodi di parsing per tutti i comandi relativi alla
programmazione concorrente (Go-routine, WaitGroup, Canali).

Questi comandi sono disponibili globalmente perché il namespace 'CONCURRENCY'
è importato di default. Utilizzano una sintassi tradizionale non qualificata.

------------------------------------------------------------------------------------------------------------------------
COMANDI DISPONIBILI:
------------------------------------------------------------------------------------------------------------------------

GO SUB / GO FUNCTION
Lancia una procedura (SUB o FUNCTION) in un nuovo task (goroutine).

- Sintassi SUB:      GO SUB MySub([arg1, arg2, ...]) [IN wg]
- Sintassi FUNCTION: GO FUNCTION MyFunc(arg1, arg2) [IN wg] INTO futureVariable

GO EXIT
Termina immediatamente l'esecuzione della goroutine (task) corrente.

- Sintassi:          GO EXIT

GO SELECT
Esegue il multiplexing su più operazioni di canale. Attende finché una delle clausole
CASE (RECEIVE o SEND) non diventa pronta per l'esecuzione.

- Sintassi:
   GO SELECT
       CASE RECEIVE variabile FROM canale: [istruzioni]
       CASE SEND valore TO canale: [istruzioni]
       [CASE DEFAULT: [istruzioni]]
   END SELECT

WAIT
Mette in pausa il task corrente finché il contatore di un WaitGroup non arriva a zero.

- Sintassi:          WAIT waitgroup_variable

DONE
Decrementa il contatore di un WaitGroup (segnala la fine di un task).

- Sintassi:          DONE waitgroup_variable

SEND
Invia un valore a un canale, bloccando il task se il canale è pieno.

- Sintassi:          SEND valore TO canale

RECEIVE
Riceve un valore da un canale, bloccando il task se il canale è vuoto.

- Sintassi:          RECEIVE variabile_destinazione FROM canale

SLEEP
Mette in pausa il task corrente per un numero specificato di millisecondi.

- Sintassi:          SLEEP millisecondi
------------------------------------------------------------------------------------------------------------------------
*/
/// <summary>
///     Contiene i metodi di parsing per le istruzioni di concorrenza.
/// </summary>
public sealed partial class Parser
{
    /// <summary>
    /// Parsa un'istruzione GO, gestendo le varianti `GO EXIT`, `GO SELECT` (multiplexing),
    /// `GO SUB/FUNCTION` e `GO FUNCTION ... INTO`.
    /// </summary>
    /// <param name="parser">L'istanza del parser su cui operare.</param>
    /// <param name="ns">Il namespace risolto (ignorato).</param>
    /// <param name="qualifier">Ignorato, deve essere null.</param>
    /// <returns>Un nodo AST <see cref="Stmt"/>.</returns>
    public static Stmt ParseGoStatement(Parser parser, string? ns)
    {
        var goToken = parser.Consume(TokenKind.Go, "Internal parser error: Expected 'GO' token.");

        // --- SINTASSI: GO EXIT ---
        if (parser.Match(TokenKind.Exit))
        {
            return new GoExitStmt(parser.Previous());
        }

        // --- SINTASSI: GO SELECT (Multiplexing su Canali) ---
        if (parser.Check(TokenKind.Select))
        {
            return parser.ParseGoSelectStatement();
        }

        // --- SINTASSI: LANCIO PROCEDURE (SUB/FUNCTION) ---
        if (!parser.Match(TokenKind.Function))
            throw CreateError(parser.Peek(), "Expected 'SUB', 'FUNCTION', or 'SELECT' after 'GO'.");
        
        var procName = parser.ConsumeProcedureName();
        var args = parser.ParseArgumentListInParentheses();

        var symbolIds = parser._program.Symbols.FindSymbolInScope(procName.Lexeme);
        var symbolId = symbolIds?.FirstOrDefault() ?? -1;

        Expr? waitGroup = null;
        if (parser.Match(TokenKind.In))
        {
            waitGroup = parser.ParseExpression();
        }

        if (parser.Match(TokenKind.Into))
        {
            // GO FUNCTION ... INTO result
            var intoTarget = parser.ParseTarget();
            var goFunctionExpr = new GoFunctionExpr(symbolId, procName, args, waitGroup, new InferredType());
            return new LetStmt([new Assignment(intoTarget, goFunctionExpr)], null);
        }
            
        // GO SUB ...
        var callExpr = new FunctionExpr(symbolId, procName, args, new VoidType());
        return new GoProcedureStmt(callExpr, waitGroup);
    }

    /// <summary>
    /// Parsa il blocco GO SELECT...END SELECT per il multiplexing dei canali.
    /// </summary>
    private GoSelectStmt ParseGoSelectStatement()
    {
        var selectToken = Consume(TokenKind.Select, "Expected 'SELECT' after 'GO'.");
        Consume(TokenKind.Newline, "Expected newline after 'GO SELECT'.");

        var cases = ImmutableArray.CreateBuilder<GoSelectCase>();
        ImmutableArray<Stmt>? defaultCase = null;

        while (!Check(TokenKind.EndSelect) && !IsAtEnd())
        {
            SkipNewlines(this);
            if (Check(TokenKind.EndSelect)) break;

            var caseToken = Consume(TokenKind.Case, "Expected 'CASE' or 'END SELECT'.");

            if (Match(TokenKind.Default))
            {
                if (defaultCase != null)
                    throw CreateError(Previous(), "Only one 'DEFAULT' case is allowed in 'GO SELECT'.");

                Consume(TokenKind.Colon, "Expected ':' after 'CASE DEFAULT'.");
                defaultCase = ParseBlockBodyUntil(null, TokenKind.Case, TokenKind.EndSelect);
            }
            else
            {
                cases.Add(ParseGoSelectCase());
            }
        }

        Consume(TokenKind.EndSelect, "Expected 'END SELECT' to close 'GO SELECT' block.");

        return new GoSelectStmt(selectToken, cases.ToImmutable(), defaultCase);
    }

    /// <summary>
    /// Parsa una singola clausola CASE all'interno di un GO SELECT.
    /// Supporta 'CASE RECEIVE', 'CASE SEND' e 'CASE TIMEOUT'.
    /// </summary>
    private GoSelectCase ParseGoSelectCase()
    {
        Stmt commStmt;

        if (Check(TokenKind.Receive))
        {
            commStmt = ParseReceiveStatement(this, null);
        }
        else if (Check(TokenKind.Send))
        {
            commStmt = ParseSendStatement(this, null);
        }
        else if (Check(TokenKind.Timeout))
        {
            commStmt = ParseTimeoutStatement(this, null);
        }
        else
        {
            throw CreateError(Peek(), "Expected 'RECEIVE', 'SEND' or 'TIMEOUT' after 'CASE' in 'GO SELECT'.");
        }

        Consume(TokenKind.Colon, "Expected ':' after channel operation.");
        var body = ParseBlockBodyUntil(null, TokenKind.Case, TokenKind.EndSelect);

        return new GoSelectCase(commStmt, body);
    }

    /// <summary>
    /// Parsa un'istruzione TIMEOUT durata (parità Go time.After), usata come case di GO SELECT.
    /// </summary>
    public static Stmt ParseTimeoutStatement(Parser parser, string? ns)
    {
        var timeoutToken = parser.Consume(TokenKind.Timeout, "Internal parser error: Expected 'TIMEOUT' token.");
        var durationExpr = parser.ParseExpression();
        return new TimeoutStmt(durationExpr, timeoutToken);
    }
    
    /// <summary>
    /// Parsa un'istruzione WAIT wg o CONCURRENCY.WAIT wg.
    /// </summary>
    public static Stmt ParseWaitStatement(Parser parser, string? ns)
    {
        var waitToken = parser.Consume(TokenKind.Wait, "Internal parser error: Expected 'WAIT' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): wg ---
        var waitGroupExpr = parser.ParseExpression();

        return new WaitStmt(waitGroupExpr, waitToken);
    }

    /// <summary>
    /// Parsa un'istruzione SEND value TO channel o CONCURRENCY.SEND value TO channel.
    /// </summary>
    public static Stmt ParseSendStatement(Parser parser, string? ns)
    {
        var sendToken = parser.Consume(TokenKind.Send, "Internal parser error: Expected 'SEND' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): value TO channel ---
        var valueExpr = parser.ParseExpression();
        parser.Consume(TokenKind.To, "Expected 'TO' after value in SEND statement.");
        var channelExpr = parser.ParseExpression();

        return new SendStmt(valueExpr, channelExpr, sendToken);
    }

    /// <summary>
    /// Parsa un'istruzione RECEIVE dest FROM channel [OK flag] o CONCURRENCY.RECEIVE dest FROM channel [OK flag].
    /// La clausola opzionale OK flag (parità Go v, ok := <-ch) scrive nel flag 1 se il valore
    /// è reale, 0 se il canale è chiuso e svuotato.
    /// </summary>
    public static Stmt ParseReceiveStatement(Parser parser, string? ns)
    {
        var receiveToken = parser.Consume(TokenKind.Receive, "Internal parser error: Expected 'RECEIVE' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): dest FROM channel [OK flag] ---
        var destination = parser.ParseTarget();
        parser.Consume(TokenKind.From, "Expected 'FROM' after destination in RECEIVE statement.");
        var channelExpr = parser.ParseExpression();

        Target? okFlag = null;
        if (parser.Match(TokenKind.Ok))
            okFlag = parser.ParseTarget();

        return new ReceiveStmt(destination, channelExpr, receiveToken, okFlag);
    }

    /// <summary>
    /// Dispatcher per i comandi `CHANNEL` (legacy e qualificata fusa). Consuma la keyword
    /// CHANNEL, poi disambigua il sottocomando (CLOSE).
    /// </summary>
    public static Stmt ParseChannelStatement(Parser parser, string? ns)
    {
        var channelToken = parser.Consume(TokenKind.Channel, "Internal parser error: Expected 'CHANNEL' keyword.");
        var subCommand = parser.Peek();
        return subCommand.Kind switch
        {
            TokenKind.Close => ParseCloseChannelStatement(parser, ns),
            _ => throw CreateError(subCommand,
                $"Sottocomando '{subCommand.Lexeme}' non riconosciuto per CHANNEL.")
        };
    }

    /// <summary>
    /// Parsa un'istruzione CHANNEL CLOSE ch (parità Go close(ch)).
    /// </summary>
    public static Stmt ParseCloseChannelStatement(Parser parser, string? ns)
    {
        var closeToken = parser.Consume(TokenKind.Close, "Expected 'CLOSE' after CHANNEL.");
        var channelExpr = parser.ParseExpression();
        return new CloseChannelStmt(channelExpr, closeToken);
    }

    /// <summary>
    /// Parsa un'istruzione DONE wg o CONCURRENCY.DONE wg.
    /// </summary>
    public static Stmt ParseDoneStatement(Parser parser, string? ns)
    {
        var doneToken = parser.Consume(TokenKind.Done, "Internal parser error: Expected 'DONE' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): wg ---
        var waitGroupExpr = parser.ParseExpression();

        return new DoneStmt(waitGroupExpr, doneToken);
    }

    /// <summary>
    /// Parsa un'istruzione SLEEP duration o CONCURRENCY.SLEEP duration.
    /// </summary>
    public static Stmt ParseSleepStatement(Parser parser, string? ns)
    {
        var sleepToken = parser.Consume(TokenKind.Sleep, "Internal parser error: Expected 'SLEEP' token.");

        // --- Sintassi unificata (legacy e qualificata fusa): duration ---
        var durationExpr = parser.ParseExpression();

        return new SleepStmt(durationExpr, sleepToken);
    }

    /// <summary>
    /// Parsa un'istruzione DEFER stmt (parità Go defer): registra il body da eseguire
    /// a fine procedura, in ordine LIFO.
    /// </summary>
    public static Stmt ParseDeferStatement(Parser parser, string? ns)
    {
        var deferToken = parser.Consume(TokenKind.Defer, "Internal parser error: Expected 'DEFER' token.");
        var bodyNodes = parser.ParseStatement(null);
        if (bodyNodes.Count != 1 || bodyNodes[0] is not Stmt body)
            throw Parser.CreateError(parser.Peek(), "Expected a single statement after DEFER.");
        return new DeferStmt(body, deferToken);
    }

    public static Stmt ParseMutexStatement(Parser parser, string? ns)
    {
        var mutexToken = parser.Consume(TokenKind.Mutex, "Internal parser error: Expected 'MUTEX' keyword.");
        var subCommand = parser.Peek();
        return subCommand.Kind switch
        {
            TokenKind.Lock => ParseMutexLockStatement(parser, ns),
            TokenKind.Unlock => ParseMutexUnlockStatement(parser, ns),
            TokenKind.RLock => ParseMutexRLockStatement(parser, ns),
            TokenKind.Runlock => ParseMutexRUnlockStatement(parser, ns),
            _ => throw Parser.CreateError(subCommand,
                $"Sottocomando '{subCommand.Lexeme}' non riconosciuto per MUTEX.")
        };
    }

    /// <summary>
    /// Parsa un'istruzione MUTEX LOCK mu (parità Go mu.Lock()).
    /// </summary>
    public static Stmt ParseMutexLockStatement(Parser parser, string? ns)
    {
        parser.Consume(TokenKind.Lock, "Internal parser error: Expected 'LOCK' after MUTEX.");
        var mutexExpr = parser.ParseExpression();
        return new MutexLockStmt(mutexExpr, new Token(TokenKind.Mutex, "MUTEX", null, mutexExpr.Position));
    }

    /// <summary>
    /// Parsa un'istruzione MUTEX UNLOCK mu (parità Go mu.Unlock()).
    /// </summary>
    public static Stmt ParseMutexUnlockStatement(Parser parser, string? ns)
    {
        parser.Consume(TokenKind.Unlock, "Internal parser error: Expected 'UNLOCK' after MUTEX.");
        var mutexExpr = parser.ParseExpression();
        return new MutexUnlockStmt(mutexExpr, new Token(TokenKind.Mutex, "MUTEX", null, mutexExpr.Position));
    }

    /// <summary>
    /// Parsa un'istruzione COND (parità Go sync.Cond): COND WAIT c, mu / COND SIGNAL c /
    /// COND BROADCAST c. Il subcomando è riconosciuto dal lexeme (SIGNAL/BROADCAST non sono
    /// keyword globali per non collidere con i sample che li usano come identificatori).
    /// </summary>
    public static Stmt ParseCondStatement(Parser parser, string? ns)
    {
        var condToken = parser.Consume(TokenKind.Cond, "Internal parser error: Expected 'COND' keyword.");
        var sub = parser.Peek();
        if (sub.Kind == TokenKind.Wait)
        {
            parser.Advance();
            var condExpr = parser.ParseExpression();
            parser.Consume(TokenKind.Comma, "Expected ',' after the COND in 'COND WAIT cond, mutex'.");
            var mutexExpr = parser.ParseExpression();
            return new CondWaitStmt(condExpr, mutexExpr, new Token(TokenKind.Cond, "COND", null, condToken.Position));
        }
        if (sub.Lexeme.Equals("SIGNAL", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var condExpr = parser.ParseExpression();
            return new CondSignalStmt(condExpr, new Token(TokenKind.Cond, "COND", null, condToken.Position));
        }
        if (sub.Lexeme.Equals("BROADCAST", StringComparison.OrdinalIgnoreCase))
        {
            parser.Advance();
            var condExpr = parser.ParseExpression();
            return new CondBroadcastStmt(condExpr, new Token(TokenKind.Cond, "COND", null, condToken.Position));
        }
        throw Parser.CreateError(sub,
            $"Sottocomando '{sub.Lexeme}' non riconosciuto per COND (attesi WAIT, SIGNAL o BROADCAST).");
    }

    /// <summary>
    /// Parsa un'istruzione MUTEX RLOCK rw (parità Go mu.RLock() su sync.RWMutex).
    /// </summary>
    public static Stmt ParseMutexRLockStatement(Parser parser, string? ns)
    {
        parser.Consume(TokenKind.RLock, "Internal parser error: Expected 'RLOCK' after MUTEX.");
        var mutexExpr = parser.ParseExpression();
        return new MutexRLockStmt(mutexExpr, new Token(TokenKind.Mutex, "MUTEX", null, mutexExpr.Position));
    }

    /// <summary>
    /// Parsa un'istruzione MUTEX RUNLOCK rw (parità Go mu.RUnlock() su sync.RWMutex).
    /// </summary>
    public static Stmt ParseMutexRUnlockStatement(Parser parser, string? ns)
    {
        parser.Consume(TokenKind.Runlock, "Internal parser error: Expected 'RUNLOCK' after MUTEX.");
        var mutexExpr = parser.ParseExpression();
        return new MutexRUnlockStmt(mutexExpr, new Token(TokenKind.Mutex, "MUTEX", null, mutexExpr.Position));
    }

    /// <summary>
    /// Parsa un'istruzione ONCE RUN o, proc (parità Go sync.Once.Do): esegue la procedura
    /// `proc` (per nome, senza argomenti) una sola volta anche con chiamate concorrenti.
    /// </summary>
    public static Stmt ParseOnceStatement(Parser parser, string? ns)
    {
        var onceToken = parser.Consume(TokenKind.Once, "Internal parser error: Expected 'ONCE' keyword.");
        parser.Consume(TokenKind.Run, "Internal parser error: Expected 'RUN' after ONCE.");
        var onceExpr = parser.ParseExpression();
        parser.Consume(TokenKind.Comma, "Expected ',' between the ONCE value and the procedure in ONCE RUN.");
        var procName = parser.ConsumeProcedureName();
        return new OnceStmt(onceExpr, procName, new Token(TokenKind.Once, "ONCE", null, onceToken.Position));
    }
}