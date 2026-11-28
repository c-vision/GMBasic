// FILE: Basic.IlBackend.Gpm.MayParkAnalysis.cs
// MAY-PARK ANALYSIS — STEP 2a del piano IL-GPM (vedi 
// GMBasic-ILBackend-GPM-Chunk-State-Machine §9.2, §13, §16, §26).
//
// Predicato *puro* (nessuna emissione IL, nessun runtime): decide a compile-time quali
// procedure devono essere compilate come STEPPER a chunk + resumeIp invece che come
// metodo CIL lineare. Una procedura è may-park se il suo corpo (ricorsivo nei
// contenitori IF/SELECT/loop) contiene un punto di blocco cooperativo — SEND, RECEIVE,
// WAIT, SLEEP, YIELD, GO SELECT, GO EXIT, GO SUB/FUNCTION (spawn: il chiamante
// partecipante alla GPM) — oppure se CHIAMA (transitivamente) una procedura may-park.
//
// Il predicato è stabile a compile-time e non dipende da euristica runtime: è la stessa
// decisione che il design §10.8 assegna a "IslandEligibility rovesciata", resa riusabile
// qui con propagazione lungo il call graph.

using System.Collections.Immutable;

namespace Basic.IlBackend.Gpm;

/// <summary>
/// Analisi may-park per il backend IL. Funzione pura di confine: NON esegue IL, NON
/// tocca il task, non ha stato — tutti i metodi sono statici.
/// </summary>
public static class MayParkAnalysis
{
    /// <summary>Uno statement "punto di blocco" (§9.2, §11.4): sospende il task corrente o
    /// cede il turno, e perciò apre un nuovo chunk. SOLO gli statement che possono FAR
    /// PARCHEGGIARE il task (SEND/RECEIVE/GO SELECT/WAIT/SLEEP) o cederlo (YIELD, confine
    /// sintetico §9.2) sono punti di blocco. GO SUB/GO FUNCTION sono un SPAWN non-bloccante
    /// (torneranno Continue — §11.4: "no blocco") e stanno DENTRO il chunk corrente
    /// (vedi §16.3 chunk 0 di Main); GO EXIT termina il task (Finished) senza sospenderlo.</summary>
    public static bool IsBlockPoint(Stmt stmt) => stmt switch
    {
        // Parcheggi reali (retry sullo stesso chunk al wake, §11.4).
        // END TICK è l'attesa a scadenza del frame (IlGpmRuntime.TickStep): dentro un task GPM
        // deve PARCHEGGIARE, non dormire il worker (§0ter) — prima era una call flat, quindi
        // Thread.Sleep sul worker GPM.
        SendStmt or ReceiveStmt or GoSelectStmt or WaitStmt or SleepStmt or TickEndStmt => true,
        // Mutex: LOCK parcheggia se il mutex è detenuto (parità Go mu.Lock()).
        MutexLockStmt => true,
        // RWMUTEX RLOCK: parcheggia se uno scrittore ha il lock (parità Go rw.RLock()).
        MutexRLockStmt => true,
        // ONCE RUN: parcheggia se un altro chiamante sta eseguendo la procedura (Go sync.Once).
        OnceStmt => true,
        // DEFER <body>: i corpi deferiti girano LIFO all'uscita e possono parkare; in ogni
        // caso il flat path non ha DEFER → la procedura va in v2 (stackless).
        DeferStmt => true,
        // COND WAIT: rilascia il mutex e parcheggia sul cond finché un SIGNAL/BROADCAST
        // (parità Go sync.Cond.Wait); al wake riacquisisce il mutex (può parkare di nuovo).
        CondWaitStmt => true,
        // Confine sintetico: cede il turno (Gosched), non parka (§9.2, §11.4).
        YieldStmt => true,
        // I/O su reti/file/console: blocco di sospensione (conservativo, prima release).
        TcpConnectStmt or TcpAcceptStmt or TcpRecvStmt or TcpSendStmt
            or UdpRecvStmt or UdpSendStmt or HttpFetchStmt
            or SslOpenStmt or SslConnectStmt or WebSocketConnectStmt
            or InputStmt or LineInputStmt
            => true,
        // SQL EXEC: scrittura pesante (riscrive file via vtab csv/parquet) → park coarse.
        SqlExecStmt => true,
        // Grafica (parità gmo SysCall): LOAD/GETPIXELS/SYNC/SCREEN/POINT attendono il main
        // thread SDL. POSITION/ON/OFF/TRANSFORM restano fire-and-forget.
        SyncStmt or LoadTextureStmt or ScreenStmt or PointStmt => true,
        SpriteCommandStmt { Command.Kind: TokenKind.SpriteLoad or TokenKind.SpriteGetPixels } => true,
        // WAITKEY (§bug ILBackend-Gpm-Stepper-For-FrameChain-Hang): attende un tasto dalla
        // coda INKEY, alimentata in modo asincrono (SDL/--key) -- un vero punto di blocco
        // cooperativo (IlGpmRuntime.TryWaitKeyPark), non uno statement "flat" qualunque.
        // Prima di questo fix non era riconosciuto qui: veniva compilato come chiamata
        // diretta e BLOCCANTE (IlRuntime.ReadKey, spin-wait sul thread) anche dentro uno
        // stepper, congelando l'intero driver GPM invece di parcheggiare solo il task.
        WaitKeyStmt => true,
        // READ (@GET … READ): legge i campi a tasti, parcheggiando per-tasto (ReadFieldStep).
        // Senza questo, un READ in una procedura la farebbe compilare flat (bloccante su stdin):
        // il campo va invece reso e atteso in modo cooperativo come WAITKEY.
        ClipperReadStmt => true,
        // INKEY var (SENZA timeout, es. "INKEY K$"): stesso identico problema di WAITKEY --
        // EmitInkeyStatement passa timeout=0 a IlRuntime.Inkey(int), che con 0 richiama
        // Inkey() (bloccante, spin-wait identico a ReadKey). Verificato dal vivo: il sample
        // reale del bug (GraphicsPutsVsTexturesOld.bas) usa esattamente "INKEY K$" senza
        // timeout dentro i DO dei benchmark -- il fix del solo WAITKEY non bastava.
        // La forma CON timeout esplicito (INKEY K$, ms) resta fuori scope qui (bloccante ma
        // LIMITATA nel tempo, non indefinita -- un problema minore, non quello di questo bug).
        InkeyStmt { TimeoutExpr: null } => true,
        // FETCH espresso asincrono (§E.5): x = FETCH(...) salva un PCodeFutureValue in x
        // (object) e la lettura successiva fa l'implicit-await (park). Lo statement DEVE aprire
        // un chunk: al resume il task riparte dal chunk della LETTURA, mai riesegue il FETCH
        // (che creerebbe un nuovo future → loop infinito).
        LetStmt { Assignments: { Length: 1 } lsa1 } when lsa1[0].Value is HttpFetchExpr => true,
        DeclarationStmt ds2 when ds2.Declarations.Any(d => d.InitialValue is HttpFetchExpr) => true,
        // AI.BLACKBOARDWAIT/AI.AGENTSWARM (Strada C, F1): la call è async (gira sul blocking
        // pool, il task GPM parcheggia sul future) — apre un chunk come HttpFetchExpr, così la
        // procedura è compilata come stepper v2 e non come call flat bloccante (che scatenerebbe
        // GpmWorkerGuard su un worker GPM).
        LetStmt { Assignments: { Length: 1 } lsaT } when IsTeamParkBuiltin(lsaT[0].Value) => true,
        DeclarationStmt dsT when dsT.Declarations.Any(d => IsTeamParkBuiltin(d.InitialValue)) => true,
        ExpressionStmt { Expression: BuiltinFunctionCallExpr bfT } when IsTeamParkBuiltin(bfT) => true,
        // SQL.EXEC forma FUNZIONE: come la forma comando, e' un punto may-park (park coarse).
        LetStmt { Assignments: { Length: 1 } lsaS } when IsSqlExecFn(lsaS[0].Value) => true,
        DeclarationStmt dsS when dsS.Declarations.Any(d => IsSqlExecFn(d.InitialValue)) => true,
        ExpressionStmt { Expression: BuiltinFunctionCallExpr bfS } when IsSqlExecFn(bfS) => true,
        // AWAIT CONCURRENCY.SLEEPASYNC(ms): park (IlTimerAwait) — la forma FUNZIONE deve aprire
        // un chunk come lo statement SLEEP, altrimenti la procedura resta FLAT e la call gira sul
        // worker con Task.Delay bloccante (§0ter violato: due ondate di sleeper, misurato
        // 2026-11-23 con 32×600 ms → 1.17 s invece di 0.6 s).
        ExpressionStmt esSleep when IsAwaitSleep(esSleep.Expression) => true,
        LetStmt { Assignments: { Length: 1 } lsaSleep } when IsAwaitSleep(lsaSleep[0].Value) => true,
        DeclarationStmt dsSleep when dsSleep.Declarations.Any(d => IsAwaitSleep(d.InitialValue)) => true,
        _ => false,
    };

    /// <summary>CONCURRENCY.SLEEPASYNC(ms) — la SLEEP come FUNZIONE (timer await). Il wrapper
    /// <c>AWAIT</c> è già stato scartato prima di qui (lo statement arriva come
    /// BuiltinFunctionCallExpr nudo): si accettano entrambe le forme. La forma funzione deve
    /// aprire un chunk come lo statement SLEEP — altrimenti la procedura resta FLAT e la call
    /// gira sul worker con Task.Delay bloccante (§0ter violato: 32 sleeper × 600 ms in due
    /// ondate invece di una, misurato 2026-11-23).</summary>
    private static bool IsAwaitSleep(Expr? e)
    {
        if (e is AwaitExpr ae) e = ae.Target;
        return e is BuiltinFunctionCallExpr b && b.Name.Kind == TokenKind.Sleep;
    }

    /// <summary>True se lo statement è un park grafico (non SEND/SLEEP/…). Usato per
    /// ammettere stepper con LOAD/SYNC nidificati in IF: il retry è sul chunk del
    /// contenitore, con GpmIoReady che evita il doppio LOAD.</summary>
    public static bool IsGraphicsPark(Stmt stmt) => stmt switch
    {
        SyncStmt or LoadTextureStmt or ScreenStmt or PointStmt => true,
        SpriteCommandStmt { Command.Kind: TokenKind.SpriteLoad or TokenKind.SpriteGetPixels } => true,
        _ => false,
    };

    /// <summary>True se il corpo (ricorsivo nei contenitori) contiene un park grafico.
    /// Serve al trampolino Proc_* → stepper: una SUB con LOAD TEXTURE non può restare
    /// IL lineare (SdlGraphicsService.LoadTexture lancia).</summary>
    public static bool HasGraphicsPark(ImmutableArray<Stmt> body)
    {
        foreach (var stmt in body)
            if (StatementHasGraphicsPark(stmt)) return true;
        return false;
    }

    private static bool StatementHasGraphicsPark(Stmt stmt) => stmt switch
    {
        _ when IsGraphicsPark(stmt) => true,
        IfStmt ifStmt => ifStmt.IfClauses.Any(c => HasGraphicsPark(c.Statements)) ||
                         (ifStmt.ElseClause is not null && HasGraphicsPark(ifStmt.ElseClause.Statements)),
        SelectStmt selectStmt => selectStmt.CaseClauses.Any(c => HasGraphicsPark(c.Statements)) ||
                                 (selectStmt.ElseClause is not null && HasGraphicsPark(selectStmt.ElseClause.Statements)),
        MatchStmt matchStmt => matchStmt.Cases.Any(c => HasGraphicsPark(c.Statements)),
        WithStmt withStmt => HasGraphicsPark(withStmt.Body),
        UsingStmt usingStmt => HasGraphicsPark(usingStmt.Body),
        TryCatchStmt tryCatch => HasGraphicsPark(tryCatch.TryBody) ||
                                 tryCatch.CatchClauses.Any(c => HasGraphicsPark(c.Body)) ||
                                 (tryCatch.Finally is not null && HasGraphicsPark(tryCatch.Finally.Body)),
        ForEachStmt forEach => HasGraphicsPark(forEach.Body),
        LocalProcedureStmt local => HasGraphicsPark(local.Procedure.Body),
        _ => false,
    };

    /// <summary>True se il corpo (ricorsivo nei contenitori) contiene un READ (@GET … READ).
    /// Serve al trampolino Proc_* → stepper: una SUB con READ non deve restare IL lineare
    /// (bloccante su stdin) ma girare come task GPM (park per-tasto, parità coi backend C).</summary>
    public static bool HasReadPark(ImmutableArray<Stmt> body)
    {
        foreach (var stmt in body)
            if (StatementHasReadPark(stmt)) return true;
        return false;
    }

    private static bool StatementHasReadPark(Stmt stmt) => stmt switch
    {
        ClipperReadStmt => true,
        IfStmt ifStmt => ifStmt.IfClauses.Any(c => HasReadPark(c.Statements)) ||
                         (ifStmt.ElseClause is not null && HasReadPark(ifStmt.ElseClause.Statements)),
        SelectStmt selectStmt => selectStmt.CaseClauses.Any(c => HasReadPark(c.Statements)) ||
                                 (selectStmt.ElseClause is not null && HasReadPark(selectStmt.ElseClause.Statements)),
        MatchStmt matchStmt => matchStmt.Cases.Any(c => HasReadPark(c.Statements)),
        WithStmt withStmt => HasReadPark(withStmt.Body),
        UsingStmt usingStmt => HasReadPark(usingStmt.Body),
        TryCatchStmt tryCatch => HasReadPark(tryCatch.TryBody) ||
                                 tryCatch.CatchClauses.Any(c => HasReadPark(c.Body)) ||
                                 (tryCatch.Finally is not null && HasReadPark(tryCatch.Finally.Body)),
        ForEachStmt forEach => HasReadPark(forEach.Body),
        LocalProcedureStmt local => HasReadPark(local.Procedure.Body),
        _ => false,
    };

    /// <summary>True se la funzione <paramref name="definition"/> è may-park *per emissione
    /// diretta* (senza propagazione lungo il call graph): contiene un punto di blocco.
    /// <paramref name="calleeMayPark"/> risolve lo stato di una procedura chiamata
    /// (bool già calcolato): se una chiamata punta a una procedura may-park, lo statement
    /// che la contiene è a sua volta un punto di "blocco indiretto".</summary>
    public static bool IsLocalMayPark(FunctionDefinition definition, Func<int, bool> calleeMayPark)
        => HasMayPark(definition.Body, calleeMayPark);

    public static bool IsLocalMayPark(FunctionDefinition definition, Func<int, bool> calleeMayPark, Func<int, Basic.DataType?>? symbolType)
        => HasMayPark(definition.Body, calleeMayPark, null, symbolType);

    /// <summary>True per una lista di statement, scendendo nei contenitori. I punti di
    /// blocco indiretto (chiamata a procedura may-park) contano come punti di blocco.</summary>
    public static bool HasMayPark(ImmutableArray<Stmt> body, Func<int, bool> calleeMayPark)
        => HasMayPark(body, calleeMayPark, null, null);

    public static bool HasMayPark(ImmutableArray<Stmt> body, Func<int, bool> calleeMayPark, Dictionary<string, int>? nameToId)
        => HasMayPark(body, calleeMayPark, nameToId, null);

    public static bool HasMayPark(ImmutableArray<Stmt> body, Func<int, bool> calleeMayPark, Dictionary<string, int>? nameToId, Func<int, Basic.DataType?>? symbolType)
    {
        foreach (var stmt in body)
            if (StatementMayPark(stmt, calleeMayPark, nameToId, symbolType)) return true;
        return false;
    }

    /// <summary>Devia ogni statement: true se lo statement può parcheggiare (direttamente o
    /// perché chiama una procedura may-park), scendendo nei corpi annidati.</summary>
    private static bool StatementMayPark(Stmt stmt, Func<int, bool> calleeMayPark)
        => StatementMayPark(stmt, calleeMayPark, null, null);

    /// <summary>True se l'espressione collezione di un FOR EACH è un CANALE (parità Go
    /// for v := range ch): il CollectionExpr è una VariableExpr il cui simbolo è ChannelType.</summary>
    private static bool IsChannelCollection(Expr collection, Func<int, Basic.DataType?>? symbolType)
        => symbolType != null
           && collection is VariableExpr ve
           && symbolType(ve.SymbolId) is ChannelType;

    private static bool StatementMayPark(Stmt stmt, Func<int, bool> calleeMayPark, Dictionary<string, int>? nameToId)
        => StatementMayPark(stmt, calleeMayPark, nameToId, null);

    private static bool StatementMayPark(Stmt stmt, Func<int, bool> calleeMayPark, Dictionary<string, int>? nameToId, Func<int, Basic.DataType?>? symbolType) => stmt switch
    {
        // --- Punti di blocco espliciti. ---
        _ when IsBlockPoint(stmt) => true,

        // --- FOR EACH su canale (parità Go for v := range ch): l'advance fa una RECEIVE con
        // flag OK → parcheggia se il canale è vuoto ma non chiuso. La canalità è nel
        // CollectionExpr del ForEachInitStmt (tipo del simbolo). ---
        ForEachInitStmt fei when IsChannelCollection(fei.CollectionExpr, symbolType) => true,

        // --- Chiamata a procedura utente may-park (blocco indiretto). ---
        ExpressionStmt { Expression: FunctionExpr fn } => calleeMayPark(fn.SymbolId) || ExpressionContainsPoint(fn),
        ExpressionStmt { Expression: BuiltinFunctionCallExpr bf } when nameToId != null && nameToId.TryGetValue(bf.Name.Lexeme, out var uId) && calleeMayPark(uId) => true,
        ExpressionStmt { Expression: MethodCallExpr mc } when IsPotentialParkCall(mc) => true,
        ExpressionStmt { Expression: BuiltinFunctionCallExpr bf } when IsIOParkBuiltin(bf) => true,
        // GO SUB (spawn goroutine): la presenza di goroutine richiede lo SCHEDULER GPM
        // (il percorso lineare lo emetterebbe come chiamata sincrona, niente concorrenza).
        GoProcedureStmt => true,
        LetStmt let => let.Assignments.Any(a => ExpressionContainsPoint(a.Value)
            || (UnwrapConversions(a.Value) is FunctionExpr lfe && calleeMayPark(lfe.SymbolId))
            || (nameToId != null && UnwrapConversions(a.Value) is BuiltinFunctionCallExpr bfc
                && nameToId.TryGetValue(bfc.Name.Lexeme, out var ucId) && calleeMayPark(ucId))),
        PrintStmt print => print.Arguments.Any(a => ExpressionContainsPoint(a.Expression)),

        // --- Contenitori di statements: scendi ricorsivamente. ---
        IfStmt ifStmt => ifStmt.IfClauses.Any(c => ExpressionContainsPoint(c.Condition)) ||
                         HasMayPark(ifStmt.IfClauses.SelectMany(c => c.Statements).ToImmutableArray(), calleeMayPark, nameToId) ||
                         (ifStmt.ElseClause is not null && HasMayPark(ifStmt.ElseClause.Statements, calleeMayPark, nameToId)),
        SelectStmt selectStmt => HasMayPark(selectStmt.CaseClauses.SelectMany(c => c.Statements).ToImmutableArray(), calleeMayPark, nameToId) ||
                                 (selectStmt.ElseClause is not null && HasMayPark(selectStmt.ElseClause.Statements, calleeMayPark, nameToId)),
        MatchStmt matchStmt => HasMayPark(matchStmt.Cases.SelectMany(c => c.Statements).ToImmutableArray(), calleeMayPark, nameToId),
        WithStmt withStmt => HasMayPark(withStmt.Body, calleeMayPark, nameToId),
        UsingStmt usingStmt => HasMayPark(usingStmt.Body, calleeMayPark, nameToId),
        TryCatchStmt tryCatch => HasMayPark(tryCatch.TryBody, calleeMayPark, nameToId) ||
                                 HasMayPark(tryCatch.CatchClauses.SelectMany(c => c.Body).ToImmutableArray(), calleeMayPark, nameToId) ||
                                 (tryCatch.Finally is not null && HasMayPark(tryCatch.Finally.Body, calleeMayPark, nameToId)),
        ForEachStmt forEach => HasMayPark(forEach.Body, calleeMayPark, nameToId),
        LocalProcedureStmt local => HasMayPark(local.Procedure.Body, calleeMayPark, nameToId),

        // --- Tutto il resto (statement "flat"): non blocca. ---
        _ => false,
    };

    /// <summary>Una call MIABILE su un void leggibile come "comunicazione bloccante"
    /// (REPL/SEND su oggetti): conservative per la prima release — una chiamata il cui
    /// nome (case-insensitive) contiene un verbo di blocco viene trattata come may-park.</summary>
    private static bool IsPotentialParkCall(MethodCallExpr mc)
    {
        var name = mc.MethodName.Lexeme.ToUpperInvariant();
        return name.Contains("RECEIVE") || name.Contains("SEND")
            || name.Contains("WAIT") || name.Contains("SLEEP")
            || name.Equals("DONE", StringComparison.Ordinal)
            || name.Contains("RECV") || name.Equals("GET", StringComparison.Ordinal);
    }

    /// <summary>Il linker avvolge le conversioni numeriche (CLNG/CINT/CDBL/CSNG/VAL) attorno a
    /// espressioni tipizzate: scava fino all'espressione interna (per trovare una FunctionExpr
    /// di una procedura may-park chiamata in un LET).</summary>
    private static Expr UnwrapConversions(Expr e)
    {
        while (e is BuiltinFunctionCallExpr { Args: { Length: 1 } } bf
               && (bf.Name.Lexeme is "CLNG" or "CINT" or "CDBL" or "CSNG" or "VAL"))
            e = bf.Args[0];
        return e;
    }

    private static bool IsIOParkBuiltin(BuiltinFunctionCallExpr bf)
    {
        var name = bf.Name.Lexeme.ToUpperInvariant();
        return name is "SLEEP" or "WAIT" or "DONE" or "YIELD" or "POINT";
    }

    /// <summary>True se l'espressione è una call a AI.BLACKBOARDWAIT / AI.AGENTSWARM (Strada C:
    /// call async park-based in goroutine — il task GPM parcheggia sul future, mai un blocco
    /// sincrono sul worker). Riconosciuto per TokenKind (chiave canonica, mai il lexeme).</summary>
    /// <summary>SQL.EXEC come FUNZIONE (espressione) — stesso park della forma comando.</summary>
    public static bool IsSqlExecFn(Expr? e)
        => e is BuiltinFunctionCallExpr bf && bf.Name.Kind == TokenKind.SqlExec;

    public static bool IsTeamParkBuiltin(Expr? e)
        => e is BuiltinFunctionCallExpr bf
           && bf.Name.Kind is TokenKind.BlackboardWait or TokenKind.AgentSwarm
              or TokenKind.AiHookFire or TokenKind.AiGoalTick;

    /// <summary>POINT(x,y) è una funzione che in gmo parcheggia (SysCall.POINT). Rileva
    /// la builtin anche dentro espressioni (LET/PRINT/condizioni IF).</summary>
    public static bool ExpressionContainsPoint(Expr? e) => e switch
    {
        null => false,
        BuiltinFunctionCallExpr bf when bf.Name.Kind == TokenKind.Point
            || bf.Name.Lexeme.Equals("POINT", StringComparison.OrdinalIgnoreCase) => true,
        BuiltinFunctionCallExpr bf => bf.Args.Any(ExpressionContainsPoint),
        FunctionExpr fn => fn.Args.Any(ExpressionContainsPoint),
        GoFunctionExpr => true, // GO FUNCTION ... INTO x → spawn con future (legge → park)
        MethodCallExpr mc => ExpressionContainsPoint(mc.Instance) || mc.Args.Any(ExpressionContainsPoint),
        BinaryExpr b => ExpressionContainsPoint(b.Left) || ExpressionContainsPoint(b.Right),
        UnaryExpr u => ExpressionContainsPoint(u.Right),
        GroupingExpr g => ExpressionContainsPoint(g.Expression),
        ArrayAccessExpr a => a.Indices.Any(ExpressionContainsPoint),
        IndexedAccessExpr i => ExpressionContainsPoint(i.Base) || i.Indices.Any(ExpressionContainsPoint),
        _ => false,
    };

    /// <summary>True se l'albero dello statement contiene solo park grafici (LOAD/SYNC/
    /// POINT/SCREEN), nessun SEND/SLEEP/call may-park. Permette uno stepper per IF con
    /// SPRITE LOAD nidificato (parità gmo: l'IP della VM parcheggia ovunque).</summary>
    public static bool NestedParksAreGraphicsOnly(Stmt stmt, Func<int, bool> calleeMayPark)
    {
        var ok = true;
        Walk(stmt);
        return ok;

        void Walk(Stmt s)
        {
            if (!ok) return;
            switch (s)
            {
                case SendStmt or ReceiveStmt or GoSelectStmt or WaitStmt or SleepStmt
                    or CondWaitStmt
                    or YieldStmt or TcpConnectStmt or TcpAcceptStmt or TcpRecvStmt or TcpSendStmt
                    or UdpRecvStmt or UdpSendStmt or HttpFetchStmt or InputStmt or LineInputStmt:
                    ok = false; return;
                case ExpressionStmt { Expression: FunctionExpr fn } when calleeMayPark(fn.SymbolId):
                    ok = false; return;
                case IfStmt ifs:
                    foreach (var c in ifs.IfClauses) { foreach (var b in c.Statements) Walk(b); }
                    if (ifs.ElseClause is { } el) foreach (var b in el.Statements) Walk(b);
                    return;
                case SelectStmt sel:
                    foreach (var c in sel.CaseClauses) foreach (var b in c.Statements) Walk(b);
                    if (sel.ElseClause is { } se) foreach (var b in se.Statements) Walk(b);
                    return;
                case ForEachStmt fe:
                    foreach (var b in fe.Body) Walk(b);
                    return;
                case TryCatchStmt tc:
                    foreach (var b in tc.TryBody) Walk(b);
                    foreach (var c in tc.CatchClauses) foreach (var b in c.Body) Walk(b);
                    if (tc.Finally is { } f) foreach (var b in f.Body) Walk(b);
                    return;
                case WithStmt w: foreach (var b in w.Body) Walk(b); return;
                case UsingStmt u: foreach (var b in u.Body) Walk(b); return;
            }
        }
    }

    /// <summary>Risolve il may-park di TUTTE le procedure con fixed-point lungo il call
    /// graph (ricorsione diretta o mutua inclusa). Ritorna l'insieme dei SymbolId
    /// may-park. Il callee resolver <paramref name="procedureForId"/> mappa SymbolId →
    /// FunctionDefinition (o null se non è una procedura utente).</summary>
    public static ImmutableHashSet<int> ComputeMayPark(
        ImmutableArray<FunctionDefinition> procedures,
        Func<int, FunctionDefinition?> procedureForId)
        => ComputeMayPark(procedures, procedureForId, null);

    /// <summary>Come <see cref="ComputeMayPark(ImmutableArray{FunctionDefinition}, Func{int, FunctionDefinition?})"/>,
    /// ma con un resolver del tipo dichiarato (symbolId → DataType) per riconoscere il FOR EACH
    /// su canale (parità Go for v := range ch) come punto di blocco. Il tipo del canale non è
    /// nell'AST del ForEachInitStmt/AdvanceAndAssignStmt (la canalità è nel CollectionExpr).</summary>
    public static ImmutableHashSet<int> ComputeMayPark(
        ImmutableArray<FunctionDefinition> procedures,
        Func<int, FunctionDefinition?> procedureForId,
        Func<int, Basic.DataType?>? symbolType)
    {
        var nameToId = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var p in procedures)
        {
            nameToId[p.OriginalName.Lexeme] = p.SymbolId;
            nameToId[p.Name.Lexeme] = p.SymbolId;
        }
        var mayPark = new HashSet<int>();
        bool changed;
        do
        {
            changed = false;
            foreach (var def in procedures)
            {
                if (mayPark.Contains(def.SymbolId)) continue;
                bool callee(int symId) => mayPark.Contains(symId);
                if (HasMayPark(def.Body, callee, nameToId, symbolType))
                {
                    mayPark.Add(def.SymbolId);
                    changed = true;
                }
            }
        } while (changed);
        return mayPark.ToImmutableHashSet();
    }

    private static Stmt? ChunkBlocker(Stmt firstStatement, Func<int, bool> calleeMayPark)
        => StatementMayPark(firstStatement, calleeMayPark) ? firstStatement : null;

}