// FILE: Interpreter.Ast.GoRoutines.cs

namespace Basic;

/// <summary>
/// Represents a GO SUB or GO FUNCTION statement to launch a procedure concurrently.
/// </summary>
public sealed record GoProcedureStmt(Expr Call, Expr? WaitGroup) : Stmt(Call.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"GO {Call}{(WaitGroup is not null ? $" IN {WaitGroup}" : "")}";

    /// <summary>
    /// Crea una copia profonda di questo nodo GoProcedureStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GoProcedureStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedCall = (Expr)Call.DeepClone();
        var clonedWaitGroup = (Expr?)WaitGroup?.DeepClone();
        return new GoProcedureStmt(clonedCall, clonedWaitGroup);
    }
}

/// <summary>
/// Represents a WAIT statement that blocks execution until a WaitGroup's counter is zero.
/// </summary>
public sealed record WaitStmt(Expr WaitGroup, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => WaitGroup.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo WaitStmt.
    /// </summary>
    /// <returns>Una nuova istanza di WaitStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new WaitStmt((Expr)WaitGroup.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Represents a SEND statement to send a value to a channel.
/// </summary>
public sealed record SendStmt(Expr Value, Expr Channel, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"SEND {Value} TO {Channel}";

    /// <summary>
    /// Crea una copia profonda di questo nodo SendStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SendStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SendStmt((Expr)Value.DeepClone(), (Expr)Channel.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Represents a RECEIVE statement to receive a value from a channel.
/// </summary>
public sealed record ReceiveStmt(Target Destination, Expr Channel, Token Token, Target? OkFlag = null)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() =>
        OkFlag is null ? $"RECEIVE {Destination} FROM {Channel}" : $"RECEIVE {Destination} FROM {Channel} OK {OkFlag}";

    /// <summary>
    /// Crea una copia profonda di questo nodo ReceiveStmt.
    /// </summary>
    /// <returns>Una nuova istanza di ReceiveStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new ReceiveStmt((Target)Destination.DeepClone(), (Expr)Channel.DeepClone(), (Token)Token.DeepClone(),
            OkFlag is null ? null : (Target)OkFlag.DeepClone());
    }
}

/// <summary>
/// Represents a CHANNEL CLOSE statement (parità Go close(ch)).
/// </summary>
public sealed record CloseChannelStmt(Expr Channel, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"CHANNEL CLOSE {Channel}";

    /// <summary>
    /// Crea una copia profonda di questo nodo CloseChannelStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new CloseChannelStmt((Expr)Channel.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un case TIMEOUT dentro GO SELECT (parità Go case &lt;-time.After(d)):
/// se nessun canale è pronto entro Duration ms, viene scelto questo case.
/// </summary>
public sealed record TimeoutStmt(Expr Duration, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"TIMEOUT {Duration}";

    /// <summary>
    /// Crea una copia profonda di questo nodo TimeoutStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new TimeoutStmt((Expr)Duration.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Represents a DONE statement that signals a WaitGroup that a task has finished.
/// </summary>
public sealed record DoneStmt(Expr WaitGroup, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => WaitGroup.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo DoneStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DoneStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DoneStmt((Expr)WaitGroup.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione `GOEXIT` che termina immediatamente la goroutine corrente.
/// </summary>
/// <param name="Token">Il token `GOEXIT` per la posizione nel codice sorgente.</param>
public sealed record GoExitStmt(Token Token) : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => "GOEXIT";

    /// <summary>
    /// Crea una copia profonda di questo nodo GoExitStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GoExitStmt con una copia del suo token.</returns>
    public override AstNode DeepClone()
    {
        return new GoExitStmt((Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta una clausola CASE all'interno di un'istruzione GO SELECT.
/// Ogni caso lega un'operazione di canale (SEND o RECEIVE) a un blocco di codice.
/// </summary>
/// <param name="CommunicationStmt">L'istruzione di comunicazione, che deve essere un SendStmt o un ReceiveStmt.</param>
/// <param name="Body">La lista immutabile di istruzioni da eseguire se questo caso viene scelto.</param>
public sealed record GoSelectCase(Stmt CommunicationStmt, ImmutableArray<Stmt> Body)
{
    /// <summary>
    /// Crea una copia profonda di questa clausola GoSelectCase.
    /// </summary>
    /// <returns>Una nuova istanza di GoSelectCase con copie di tutti i suoi figli.</returns>
    public GoSelectCase DeepClone()
    {
        return new GoSelectCase((Stmt)CommunicationStmt.DeepClone(), [..Body.Select(s => (Stmt)s.DeepClone())]);
    }
}

/// <summary>
/// Rappresenta un'istruzione `GO SELECT`, che attende che una tra diverse
/// operazioni di comunicazione su canali diventi possibile e poi esegue
/// il blocco di codice associato.
/// </summary>
/// <param name="SelectToken">Il token `SELECT` per la posizione nel codice sorgente.</param>
/// <param name="Cases">La lista immutabile delle clausole `CASE`.</param>
/// <param name="DefaultCase">Il blocco di codice opzionale per la clausola `DEFAULT`.</param>
public sealed record GoSelectStmt(Token SelectToken, ImmutableArray<GoSelectCase> Cases, 
    ImmutableArray<Stmt>? DefaultCase) : Stmt(SelectToken.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"GO SELECT ({Cases.Length} cases{(DefaultCase is not null ? " with DEFAULT" : "")})";

    /// <summary>
    /// Crea una copia profonda di questo nodo GoSelectStmt.
    /// </summary>
    /// <returns>Una nuova istanza di GoSelectStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        var clonedCases = Cases.Select(c => c.DeepClone()).ToImmutableArray();
        var clonedDefaultCase = DefaultCase?.Select(s => (Stmt)s.DeepClone()).ToImmutableArray();
        return new GoSelectStmt((Token)SelectToken.DeepClone(), clonedCases, clonedDefaultCase);
    }
}

/// <summary>
/// Represents a SLEEP statement that pauses the current task for a specified duration.
/// </summary>
public sealed record SleepStmt(Expr Duration, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => Duration.ToString();

    /// <summary>
    /// Crea una copia profonda di questo nodo SleepStmt.
    /// </summary>
    /// <returns>Una nuova istanza di SleepStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new SleepStmt((Expr)Duration.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione DEFER stmt (parità Go defer): registra l'istruzione da eseguire
/// alla fine della procedura corrente, nell'ordine inverso (LIFO). Utile per il release di
/// risorse (UNLOCK mu, CHANNEL CLOSE ch, ...) che deve avvenire a ogni uscita della procedura
/// (RETURN, GOSUB-RETURN, eccezione, fine task).
/// </summary>
public sealed record DeferStmt(Stmt Body, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"DEFER {Body}";

    /// <summary>
    /// Crea una copia profonda di questo nodo DeferStmt.
    /// </summary>
    /// <returns>Una nuova istanza di DeferStmt con copie di tutti i suoi figli.</returns>
    public override AstNode DeepClone()
    {
        return new DeferStmt((Stmt)Body.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione LOCK mu (parità Go mu.Lock()): acquisisce il mutex,
/// bloccando il task se già detenuto da un altro.
/// </summary>
public sealed record MutexLockStmt(Expr Mutex, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"LOCK {Mutex}";

    /// <summary>
    /// Crea una copia profonda di questo nodo LockStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new MutexLockStmt((Expr)Mutex.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione UNLOCK mu (parità Go mu.Unlock()): rilascia il mutex.
/// </summary>
public sealed record MutexUnlockStmt(Expr Mutex, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"UNLOCK {Mutex}";

    /// <summary>
    /// Crea una copia profonda di questo nodo UnlockStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new MutexUnlockStmt((Expr)Mutex.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione RLOCK mu (parità Go mu.RLock() su sync.RWMutex): acquisisce il
/// lock in lettura (condiviso tra lettori), bloccando se un scrittore detiene il lock.
/// </summary>
public sealed record MutexRLockStmt(Expr Mutex, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"RLOCK {Mutex}";

    /// <summary>
    /// Crea una copia profonda di questo nodo RLockStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new MutexRLockStmt((Expr)Mutex.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione RUNLOCK mu (parità Go mu.RUnlock() su sync.RWMutex): rilascia il
/// lock in lettura.
/// </summary>
public sealed record MutexRUnlockStmt(Expr Mutex, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"RUNLOCK {Mutex}";

    /// <summary>
    /// Crea una copia profonda di questo nodo RUnlockStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new MutexRUnlockStmt((Expr)Mutex.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>
/// Rappresenta un'istruzione ONCE RUN o, proc (parità Go sync.Once.Do): esegue la procedura
/// `proc` (per nome, senza argomenti, arity 0) una sola volta, anche se chiamata da più
/// goroutine concorrenti; i chiamanti successivi (o concorrenti durante l'esecuzione)
/// attendono il completamento e poi proseguono senza ri-eseguirla.
/// </summary>
public sealed record OnceStmt(Expr Once, Token ProcedureName, Token Token)
    : Stmt(Token.Position)
{
    /// <summary>
    /// Restituisce una rappresentazione testuale compatta del nodo.
    /// </summary>
    public override string ToString() => $"ONCE RUN {Once}, {ProcedureName.Lexeme}";

    /// <summary>
    /// Crea una copia profonda di questo nodo OnceStmt.
    /// </summary>
    public override AstNode DeepClone()
    {
        return new OnceStmt((Expr)Once.DeepClone(), (Token)ProcedureName.DeepClone(), (Token)Token.DeepClone());
    }
}

/// <summary>COND WAIT c, mu (parità Go c.Wait()): rilascia il mutex, parcheggia finché un segnale.</summary>
public sealed record CondWaitStmt(Expr Cond, Expr Mutex, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"COND WAIT {Cond}";
    public override AstNode DeepClone() => new CondWaitStmt((Expr)Cond.DeepClone(), (Expr)Mutex.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>COND SIGNAL c (parità Go c.Signal()): sveglia un waiter.</summary>
public sealed record CondSignalStmt(Expr Cond, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"COND SIGNAL {Cond}";
    public override AstNode DeepClone() => new CondSignalStmt((Expr)Cond.DeepClone(), (Token)Token.DeepClone());
}

/// <summary>COND BROADCAST c (parità Go c.Broadcast()): sveglia tutti i waiter.</summary>
public sealed record CondBroadcastStmt(Expr Cond, Token Token) : Stmt(Token.Position)
{
    public override string ToString() => $"COND BROADCAST {Cond}";
    public override AstNode DeepClone() => new CondBroadcastStmt((Expr)Cond.DeepClone(), (Token)Token.DeepClone());
}