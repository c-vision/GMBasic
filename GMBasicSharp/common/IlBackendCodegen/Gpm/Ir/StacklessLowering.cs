// FILE: Basic.IlBackend.Gpm.Ir.StacklessLowering.cs
// G2 — StacklessLowering: AST → IlCoroutineIr per il sottoinsieme G2 (piano §8, §12 G2).
//
// Design deliberato (coerente con l'IR minima §8): l'IR tiene il controllo (basic block +
// terminator) e i SAFEPOINT; l'EMITTER valuta le espressioni straight-line INLINE in CIL a
// partire da AstNode del blocco. Le istruzioni IR servono SOLO dove il valore deve
// sopravvivere a un rientro (retry dopo park) o a una call suspendable: gli argomenti delle
// call may-park vivono in Spills del frame (terminator Call.ArgSlots).
//
// Sottoinsieme G2: sequenze, LET, PRINT, IF, SELECT, FOR/WHILE, SLEEP, SEND, RECEIVE,
// chiamate (CallSync flat vs Call suspendable). TRY/FINALLY/ON ERROR = G3.

using System;
using System.Collections.Generic;
using System.Collections.Immutable;

namespace Basic.IlBackend.Gpm.Ir;

/// <summary>Lowering AST → IR per una procedura may-suspend (corpo piatto già linkato).
/// Le procedure flat NON passano da qui (restano Proc_*).</summary>
public static class StacklessLowering
{
    public const int EntryPc = 0;
    /// <summary>Valore-Pc sentinel per "fine procedura": l'emitter emette Return (o
    /// CompleteTask per la entry).</summary>
    public const int EndPc = -2;

    public static IlCoroutineIr Lower(
        FunctionDefinition definition,
        ImmutableHashSet<int> mayPark,
        Func<int, FunctionDefinition?> procedureForId)
        => Lower(definition, mayPark, procedureForId, null);

    public static IlCoroutineIr Lower(
        FunctionDefinition definition,
        ImmutableHashSet<int> mayPark,
        Func<int, FunctionDefinition?> procedureForId,
        Dictionary<string, int>? nameToId,
        Func<int, Basic.DataType?>? symbolType = null,
        ImmutableArray<Stmt> moduleTopLevel = default)
    {
        var ir = new IlCoroutineIr { ProcedureId = definition.SymbolId };
        ir.LocalCount = CountLocals(definition);
        var ctx = new LowerCtx(ir, mayPark, procedureForId, IsEntry(definition), nameToId, symbolType);
        ctx.IndexLabels(definition.Body);
        ctx.IndexModuleLabels(moduleTopLevel);
        ctx.LowerList(EntryPc, definition.Body, 0, IlIrRegionPath.Empty, EndPc, setEntry: true);
        ctx.InlineModuleHandlers();
        ctx.ResolveLabels();
        FuseFlatBlocks(ir);
        ir.SpillCount = ctx.SpillCount;
        return ir;
    }

    /// <summary>G8: fusione dei block FLAT consecutivi (Branch → Branch) in UN SOLO blocco:
    /// il blocco esegue AstNode + ExtraStatements in sequenza e poi UN SOLO Branch. Riduce il
    /// numero di blocchi (dispatch Switch, CIL try/catch per block, transizioni Continue).
    /// Conservativa: fonde SOLO se il successore è raggiunto esclusivamente da questo blocco
    /// (nessun altro blocco/label/entry lo referenzia) e NON è un NextStmt/ForStmt/EndStmt
    /// (che richiedono il proprio Aux). La fusione avviene DOPO ResolveLabels (le label sono
    /// già nei terminator).</summary>
    private static void FuseFlatBlocks(IlCoroutineIr ir)
    {
        if (ir.Blocks.Count < 2) return;
        var byPc = ir.Blocks.ToDictionary(b => b.Pc);
        bool changed;
        do
        {
            changed = false;
            for (var i = 0; i < ir.Blocks.Count; i++)
            {
                var b = ir.Blocks[i];
                if (b.Terminator.Kind != IlIrTermKind.Branch) continue;
                // Non estendere un blocco che termina con un THROW (ErrorStmt/ThrowStmt/
                // RethrowStmt: gli extra diventerebbero dead code dopo il lancio).
                if (b.AstNode is ErrorStmt or ThrowStmt or RethrowStmt) continue;
                var succPc = b.Terminator.TargetPc;
                if (succPc < 0 || succPc == EntryPc || !byPc.TryGetValue(succPc, out var succ)) continue;
                if (succ.Terminator.Kind != IlIrTermKind.Branch) continue;
                if (succ.AstNode is NextStmt or ForStmt or EndStmt or ExitStmt or LabelStmt or EnterLoopStmt or ExitLoopStmt or TryCatchStmt
                    or OnErrorStmt or OnErrorResumeStmt or ResumeStmt or GoExitStmt) continue;
                // Niente fusioni tra regioni diverse (TRY/FINALLY/CATCH: il blocco esegue il
                // push/pop della regione nel path — fondere blocchi di regioni diverse rompe
                // l'unwind delle eccezioni).
                if (!b.RegionPath.Equals(succ.RegionPath)) continue;
                var succSucc = succ.Terminator.TargetPc;
                if (succSucc != EndPc && !byPc.ContainsKey(succSucc)) continue;
                // Il successore deve essere raggiunto SOLO da b (niente altri Branch/Resume/target).
                var refs = 0;
                foreach (var x in ir.Blocks)
                {
                    if (x == b) continue;
                    if (x.Terminator.TargetPc == succPc) refs++;
                    if (x.Terminator.ResumePc == succPc) refs++;
                    if (!x.Terminator.Targets.IsDefaultOrEmpty && x.Terminator.Targets.Contains(succPc)) refs++;
                }
                if (refs > 0) continue;
                (b.ExtraStatements ??= new()).Add((Stmt)succ.AstNode!);
                b.Terminator = succ.Terminator;
                ir.RemoveBlock(succ);
                byPc.Remove(succPc);
                changed = true;
                break;
            }
        } while (changed);
    }

    private static bool IsEntry(FunctionDefinition definition)
        => definition.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase);

    private static int CountLocals(FunctionDefinition definition)
    {
        var max = 16;
        if (definition.Parameters is { Length: > 0 } pars)
            foreach (var p in pars) max = Math.Max(max, p.SymbolId % 4096 + 1);
        return max;
    }

    private sealed class LowerCtx
    {
        private readonly IlCoroutineIr _ir;
        private readonly ImmutableHashSet<int> _mayPark;
        private readonly Func<int, FunctionDefinition?> _procedureForId;
        private readonly bool _isEntry;
        private readonly Dictionary<string, int>? _nameToId;
        private readonly Func<int, Basic.DataType?>? _symbolType;
        private bool _channelForeachInit;
        private int _nextPc = 1;
        private int _spill;

        private readonly Dictionary<string, int> _labelToIndex = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<int, int> _stmtEntryPc = new();
        private readonly List<(IlCoroutineBlock From, string Label, int TargetIndex)> _pendingLinks = new();
        /// <summary>Simboli che ricevono un FUTURE (GO FUNCTION INTO / FETCH espressione): i
        /// blocchi che LI LEGGONO fanno l'implicit-await parcheggiante (parità Go).</summary>
        private readonly HashSet<int> _futureSlots = new();
        /// <summary>Campi @GET accumulati nell'ordine di lowering, consumati dall'istruzione READ
        /// (parità col path flat IlProgramCompiler._pendingReadFields).</summary>
        private readonly List<AtGetStmt> _pendingReadFields = new();
        /// <summary>Statement TOP-LEVEL del MODULO (label/handler a livello modulo, es. un
        /// handler ON ERROR GOTO da una SUB verso un label del modulo). Il lowering li INLINA
        /// nella SUB che li referenzia (cross-proc, parità PCodeVM).</summary>
        private ImmutableArray<Stmt> _moduleTopLevel = ImmutableArray<Stmt>.Empty;
        private readonly Dictionary<string, int> _moduleLabelToIndex = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, int> _moduleLabelToPc = new(StringComparer.OrdinalIgnoreCase);
        private readonly List<(IlCoroutineBlock, string, IlIrRegionPath)> _pendingModuleHandlers = new();

        public int SpillCount => _spill;

        public LowerCtx(IlCoroutineIr ir, ImmutableHashSet<int> mayPark,
            Func<int, FunctionDefinition?> procedureForId, bool isEntry, Dictionary<string, int>? nameToId = null)
            : this(ir, mayPark, procedureForId, isEntry, nameToId, null)
        {
        }

        public LowerCtx(IlCoroutineIr ir, ImmutableHashSet<int> mayPark,
            Func<int, FunctionDefinition?> procedureForId, bool isEntry, Dictionary<string, int>? nameToId,
            Func<int, Basic.DataType?>? symbolType)
        {
            _ir = ir;
            _mayPark = mayPark;
            _procedureForId = procedureForId;
            _isEntry = isEntry;
            _nameToId = nameToId;
            _symbolType = symbolType;
        }

        private int NewPc() => _nextPc++;

        /// <summary>Ritorna il SymbolId di un future letto dallo statement (GO FUNCTION INTO /
        /// FETCH) — il primo simbolo futuro referenziato, se c'è. Walk ricorsiva sugli
        /// statement/espressioni.</summary>
        private int? ReadsFuture(Stmt stmt)
        {
            int? found = null;
            void WalkExpr(Expr e)
            {
                if (found != null) return;
                if (e is VariableExpr ve && _futureSlots.Contains(ve.SymbolId)) { found = ve.SymbolId; return; }
                switch (e)
                {
                    case GroupingExpr g: WalkExpr(g.Expression); break;
                    case UnaryExpr u: WalkExpr(u.Right); break;
                    case BinaryExpr b: WalkExpr(b.Left); WalkExpr(b.Right); break;
                    case IndexedAccessExpr ia: WalkExpr(ia.Base); foreach (var i in ia.Indices) WalkExpr(i); break;
                    case ArrayAccessExpr aa: foreach (var i in aa.Indices) WalkExpr(i); break;
                    case FieldAccessExpr fa: if (fa.Structure is { } s) WalkExpr(s); break;
                    case BuiltinFunctionCallExpr bfc: foreach (var a in bfc.Args) WalkExpr(a); break;
                    case FunctionExpr fn: foreach (var a in fn.Args) WalkExpr(a); break;
                }
            }
            switch (stmt)
            {
                case LetStmt lt: foreach (var a in lt.Assignments) { if (a.Value is { } v) WalkExpr(v); } break;
                case PrintStmt p: foreach (var a in p.Arguments) if (a.Expression is { } ex) WalkExpr(ex); break;
                case CPrintStmt cp: foreach (var a in cp.Arguments) if (a.Expression is { } ex) WalkExpr(ex); break;
                case DeclarationStmt d: foreach (var de in d.Declarations) if (de.InitialValue is { } iv) WalkExpr(iv); break;
                case IfStmt ifs:
                    foreach (var c in ifs.IfClauses) { WalkExpr(c.Condition); foreach (var s in c.Statements) if (ReadsFuture(s) is { } rf) found = rf; }
                    if (ifs.ElseClause is { } el) foreach (var s in el.Statements) if (ReadsFuture(s) is { } rf) found = rf;
                    break;
                case ExpressionStmt es: WalkExpr(es.Expression); break;
                case SendStmt sn: WalkExpr(sn.Value); break;
            }
            return found;
        }

        private IlCoroutineBlock Block(int pc, object? astNode, IlIrRegionPath path)
        {
            var b = _ir.CreateBlock(pc, path);
            b.AstNode ??= astNode;
            return b;
        }

        /// <summary>Indice le etichette della lista piatta → indice statement (per i jump).</summary>
        public void IndexLabels(ImmutableArray<Stmt> body)
        {
            for (var i = 0; i < body.Length; i++)
                if (body[i] is LabelStmt lbl)
                    _labelToIndex[lbl.Label.Lexeme] = i;
        }

        /// <summary>Indicizza i label TOP-LEVEL del modulo (handler ON ERROR cross-proc).</summary>
        public void IndexModuleLabels(ImmutableArray<Stmt> moduleTopLevel)
        {
            if (moduleTopLevel.IsDefaultOrEmpty) return;
            _moduleTopLevel = moduleTopLevel;
            for (var i = 0; i < moduleTopLevel.Length; i++)
                if (moduleTopLevel[i] is LabelStmt lbl)
                    _moduleLabelToIndex[lbl.Label.Lexeme] = i;
        }

        /// <summary>Lowera la lista da <paramref name="startIndex"/>. Ogni statement riceve un
        /// pc (il primo è <paramref name="entryPc"/>); il successore è riservato PRIMA di
        /// lowerare lo statement corrente (i costrutti ramificano verso successori noti).
        /// L'ultimo statement della lista termina a <paramref name="listEndPc"/>. La ricorsione
        /// si ferma a <paramref name="stopIndex"/> (esclusivo) per i corpi dei FOR unit.</summary>
        public void LowerList(int entryPc, ImmutableArray<Stmt> body, int startIndex, IlIrRegionPath path, int listEndPc, int stopIndex = int.MaxValue, bool setEntry = false)
        {
            if (startIndex >= body.Length || startIndex >= stopIndex) return;

            // Solo la lista TOP-LEVEL registra gli entry-pc per la risoluzione delle etichette:
            // le liste annidate (corpi di IF/SELECT/FOR) usano indici propri che SOVRASCRIVEREBBERO
            // quelli del body (stesso spazio di indice) → il Jump/label risolverebbe al pc sbagliato.
            if (setEntry) _stmtEntryPc[startIndex] = entryPc;

            var stmt = body[startIndex];
            var isLast = startIndex + 1 >= body.Length || startIndex + 1 >= stopIndex;
            var succ = isLast ? listEndPc : NewPc();

            if (stmt is ForStmt)
            {
                LowerForUnit(entryPc, body, startIndex, path, listEndPc, stopIndex);
                return;
            }

            LowerStmt(entryPc, stmt, path, succ);
            LowerList(succ, body, startIndex + 1, path, listEndPc, stopIndex, setEntry);
        }

        private void LowerStmt(int pc, Stmt stmt, IlIrRegionPath path, int succ)
        {
            // Implicit-await PARCHEGGIANTE: se lo statement LEGGE un simbolo future (GO
            // FUNCTION INTO / FETCH), il blocco attende che il future risolva (FutureSlot).
            if (_futureSlots.Count > 0 && ReadsFuture(stmt) is { } fs)
                Block(pc, stmt, path).FutureSlot = fs;
            // @GET: accumula il campo per l'istruzione READ che lo consuma (parità flat).
            if (stmt is AtGetStmt agRead && agRead.Variable is VariableTarget)
                _pendingReadFields.Add(agRead);
            switch (stmt)
            {
                case IfStmt ifs: LowerIf(pc, ifs, path, succ); break;
                case SelectStmt sel: LowerSelect(pc, sel, path, succ); break;
                case SleepStmt or SendStmt or ReceiveStmt or WaitKeyStmt or WaitStmt or MutexLockStmt
                    or MutexRLockStmt or CondWaitStmt or OnceStmt or TickEndStmt
                    or TcpConnectStmt or SslOpenStmt or SslConnectStmt or WebSocketConnectStmt
                    or TcpSendStmt or TcpRecvStmt or TcpAcceptStmt
                    or UdpRecvStmt
                    or SyncStmt or ScreenStmt or PointStmt or LoadTextureStmt
                    or SpriteCommandStmt { Command.Kind: TokenKind.SpriteLoad or TokenKind.SpriteGetPixels }
                    or SqlExecStmt or ClipperReadStmt: LowerPark(pc, stmt, path, succ); break;
                case DeferStmt defr:
                {
                    // DEFER <body> (parità Go defer): il body NON gira inline — viene abbassato
                    // come blocco nascosto (bodyPc) registrato a run-time (frame.DeferredBodies)
                    // dal sito DEFER ed eseguito LIFO all'uscita della procedura. Il blocco
                    // deferito termina con DeferDone, che il driver usa per popare il successivo
                    // o ricompletare il Return/GoExit interrotto.
                    var bodyPc = NewPc();
                    var deferDonePc = NewPc();
                    LowerList(bodyPc, ImmutableArray.Create(defr.Body), 0, path, deferDonePc);
                    Block(deferDonePc, defr, path).Terminator =
                        new IlIrTerminator(IlIrTermKind.DeferDone);
                    Block(pc, defr, path).Terminator =
                        new IlIrTerminator(IlIrTermKind.DeferPush, TargetPc: bodyPc, ResumePc: succ);
                    break;
                }
                case ForEachInitStmt fei:
                    // FOR EACH su canale (parità Go for v := range ch): l'Init salva il canale
                    // (flat); l'Advance seguente fa la RECEIVE con flag OK → park. La canalità è
                    // nel tipo del CollectionExpr.
                    _channelForeachInit = _symbolType != null
                        && fei.CollectionExpr is VariableExpr cve
                        && _symbolType(cve.SymbolId) is ChannelType;
                    LowerFlat(pc, stmt, path, succ);
                    break;
                case ForEachAdvanceAndAssignStmt fea when _channelForeachInit:
                    _channelForeachInit = false;
                    LowerForEachChannelPark(pc, fea, path, succ);
                    break;
                case ExpressionStmt { Expression: FunctionExpr fe }: LowerCall(pc, stmt, fe, path, succ, IlReturnDestination.Discard, -1); break;
                case ExpressionStmt { Expression: BuiltinFunctionCallExpr bfa } when bfa.Name.Kind == TokenKind.Sleep:
                    LowerSleepFnPark(pc, stmt, bfa, path, succ, -1);
                    break;
                case ExpressionStmt { Expression: AwaitExpr { Target: BuiltinFunctionCallExpr bfaw } } when bfaw.Name.Kind == TokenKind.Sleep:
                    LowerSleepFnPark(pc, stmt, bfaw, path, succ, -1);
                    break;
            case ExpressionStmt { Expression: BuiltinFunctionCallExpr bfe }: LowerCall(pc, stmt, bfe, path, succ, IlReturnDestination.Discard, -1); break;
                case GotoStmt g: LowerGoto(pc, g.Target.Lexeme, g, path, succ); break;
                case JumpStmt j: LowerGoto(pc, j.TargetLabel.Lexeme, j, path, succ); break;
                case ExitStmt { TargetLabel: { } target } exit:
                    LowerGoto(pc, target.Lexeme, exit, path, succ);
                    break;
                case GosubStmt gs: LowerGosub(pc, gs, path, succ); break;
                case ReturnStmt rs when rs.Value is null: LowerReturnGosub(pc, rs, path); break;
                case ReturnStmt rs when rs.Value is not null: LowerReturnValue(pc, rs, path); break;
                case LetStmt lt when lt.Assignments is { Length: 1 } a0 && a0[0].Target is VariableTarget vtr:
                    LowerCall(pc, lt, a0[0].Value, path, succ, IlReturnDestination.Local, vtr.SymbolId);
                    break;
                case DeclarationStmt ds when ds.Declarations is { Length: 1 } dd && dd[0].InitialValue is not null:
                    // DIM x AS T = <call may-park> (es. DIM rv AS LONG = CaptureFinally(...)):
                    // il call-value può essere sospendibile → LowerCall con dest Local.
                    LowerCall(pc, ds, dd[0].InitialValue, path, succ, IlReturnDestination.Local, dd[0].SymbolId);
                    break;
                case GoExitStmt: LowerGoExit(pc, stmt, path); break;
                case OnErrorStmt oe: LowerOnError(pc, oe, path, succ); break;
                case OnErrorResumeStmt or ResumeStmt: LowerResume(pc, stmt, path); break;
                case GoSelectStmt gs: LowerGoSelect(pc, gs, path, succ); break;
                case TryCatchStmt tc: LowerTry(pc, tc, path, succ); break;
                case ConditionalJumpStmt cj: LowerCondJump(pc, cj, path, succ); break;
                case LabelStmt or EnterLoopStmt: LowerFlat(pc, stmt, path, succ); break;
                default: LowerFlat(pc, stmt, path, succ); break;
            }
        }

        /// <summary>TRY/CATCH/FINALLY (§7): il corpo del TRY è una regione protetta (l'entry
        /// block pusha la regione sul frame via EnterTryRegion); al completamento normale va al
        /// FINALLY (o al successore); CATCH e FINALLY terminano con ContinueUnwind (l'unwind
        /// resta nel frame attraverso i park dentro FINALLY).</summary>
        private void LowerTry(int pc, TryCatchStmt tc, IlIrRegionPath path, int succ)
        {
            var tryBodyPc = NewPc();
            var tryEndPc = NewPc();
            var catchPcs = new int[tc.CatchClauses.Length];
            var catchEndPcs = new int[tc.CatchClauses.Length];
            for (var i = 0; i < tc.CatchClauses.Length; i++)
            {
                catchPcs[i] = NewPc();
                catchEndPcs[i] = NewPc();
            }
            var finallyPc = tc.Finally is not null ? NewPc() : -1;
            var finallyEndPc = tc.Finally is not null ? NewPc() : -1;

            // Entry block: pusha la regione (EnterTryRegion) e salta al corpo del TRY.
            var tryRegion = new IlIrRegion(
                IlIrRegionKind.Try,
                HandlerPc: catchPcs.Length > 0 ? catchPcs[0] : -1,
                EndPc: succ,
                FinallyPc: finallyPc);
            var entryBlock = Block(pc, tc, path.Push(tryRegion));
            entryBlock.Terminator = new IlIrTerminator(IlIrTermKind.Branch, TargetPc: tryBodyPc);

            // Corpo del TRY (regione protetta): al termine normale → FINALLY (o successore).
            LowerList(tryBodyPc, tc.TryBody, 0, path.Push(tryRegion), tryEndPc, setEntry: false);
            var tryEndBlock = Block(tryEndPc, tc, path);
            tryEndBlock.Terminator = new IlIrTerminator(
                IlIrTermKind.Branch, TargetPc: finallyPc >= 0 ? finallyPc : succ);

            // Catch bodies: un blocco di ENTRY (AstNode = CatchClause) bind la variabile del
            // catch (BindCatchVariable) e salta al body; il body termina con ContinueUnwind
            // (pulisce il cleanup FINALLY).
            for (var i = 0; i < tc.CatchClauses.Length; i++)
            {
                var catchBodyPc = NewPc();
                LowerList(catchBodyPc, tc.CatchClauses[i].Body, 0, path, catchEndPcs[i], setEntry: false);
                var catchEntryBlock = Block(catchPcs[i], tc.CatchClauses[i], path);
                catchEntryBlock.Terminator = new IlIrTerminator(IlIrTermKind.Branch, TargetPc: catchBodyPc);
                var catchEndBlock = Block(catchEndPcs[i], tc.CatchClauses[i], path);
                catchEndBlock.Terminator = new IlIrTerminator(IlIrTermKind.ContinueUnwind, ResumePc: succ);
            }

            // FINALLY body: al termine → ContinueUnwind (normale: continua; unwind: riprende).
            if (finallyPc >= 0)
            {
                LowerList(finallyPc, tc.Finally!.Body, 0, path, finallyEndPc, setEntry: false);
                var finallyEndBlock = Block(finallyEndPc, tc.Finally, path);
                finallyEndBlock.Terminator = new IlIrTerminator(IlIrTermKind.ContinueUnwind, ResumePc: succ);
            }
        }

        /// <summary>Scava le conversioni numeriche (CLNG/CINT/CDBL/CSNG/VAL) fino all'espressione
        /// interna: il linker le avvolge attorno alle call tipizzate.</summary>
        private static Expr UnwrapConversions(Expr e)
        {
            while (e is BuiltinFunctionCallExpr { Args: { Length: 1 } } bf
                   && (bf.Name.Lexeme is "CLNG" or "CINT" or "CDBL" or "CSNG" or "VAL"))
                e = bf.Args[0];
            return e;
        }

        /// <summary>GOSUB: pusha la continuazione (ResumePc = succ) sul GosubStack del frame e
        /// salta al label target (risolto a fine lowering in TargetProcedure).</summary>
        private void LowerGosub(int pc, GosubStmt gs, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, gs, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.Gosub, ResumePc: succ, TargetProc: -1);
            _pendingLinks.Add((b, gs.Target.Lexeme, -1));
        }

        /// <summary>GO SELECT (parità Go select): il select block registra atomico i case
        /// (RECEIVE + TIMEOUT) e parcheggia; i body dei case e il DEFAULT escono a succ.
        /// Targets = [body0..bodyN, defaultPc|succ].</summary>
        private void LowerGoSelect(int pc, GoSelectStmt gs, IlIrRegionPath path, int succ)
        {
            var bodyPcs = new int[gs.Cases.Length];
            for (var i = 0; i < gs.Cases.Length; i++)
            {
                bodyPcs[i] = NewPc();
                LowerList(bodyPcs[i], gs.Cases[i].Body, 0, path, succ, setEntry: false);
            }
            var defaultPc = gs.DefaultCase is { Length: > 0 } ? NewPc() : -1;
            if (defaultPc >= 0 && gs.DefaultCase is { } dc)
                LowerList(defaultPc, dc, 0, path, succ, setEntry: false);

            var targets = new List<int>(bodyPcs);
            targets.Add(defaultPc >= 0 ? defaultPc : succ);
            var b = Block(pc, gs, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.Select, Targets: targets.ToImmutableArray());
        }

        /// <summary>ON ERROR GOTO/GOSUB <label> | GOTO 0: installa/disinstalla l'handler.
        /// Il target è un'etichetta risolta a fine lowering in TargetProc; GOTO 0 → disable.</summary>
        private void LowerOnError(int pc, OnErrorStmt oe, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, oe, path);
            var isDisable = oe.Target is { Kind: TokenKind.Number, Literal: double n } && n == 0;
            if (isDisable)
            {
                b.Terminator = new IlIrTerminator(IlIrTermKind.OnError, TargetProc: IlGpmRuntime.GpmOnErrorDisable, ResumePc: succ);
            }
            else if (oe.Target is { } label)
            {
                // Label a livello MODULO (handler ON ERROR cross-proc, es. `FutureHandler:` dopo
                // END SUB): l'handler è INLINATO IN RITARDO (dopo il body della sub, via
                // InlineModuleHandlers) così i pc del body restano sequenziali e il RESUME
                // (ErrorLinePc+1) non finisce dentro l'handler.
                if (!_labelToIndex.ContainsKey(label.Lexeme) && _moduleLabelToIndex.ContainsKey(label.Lexeme))
                {
                    _pendingModuleHandlers.Add((b, label.Lexeme, path));
                    b.Terminator = new IlIrTerminator(IlIrTermKind.OnError, TargetProc: -1, ResumePc: succ);
                }
                else
                {
                    b.Terminator = new IlIrTerminator(IlIrTermKind.OnError, TargetProc: -1, ResumePc: succ);
                    _pendingLinks.Add((b, label.Lexeme, -1));
                }
            }
            else
            {
                throw new InvalidOperationException("ON ERROR senza target.");
            }
        }

        /// <summary>RESUME [NEXT]: riprende dopo l'handler ON ERROR (ResumePc = 1 per NEXT).</summary>
        private void LowerResume(int pc, Stmt stmt, IlIrRegionPath path)
        {
            var b = Block(pc, stmt, path);
            var isNext = stmt is OnErrorResumeStmt || (stmt as ResumeStmt)?.Target is not null;
            b.Terminator = new IlIrTerminator(IlIrTermKind.Resume, ResumePc: isNext ? 1 : 0);
        }

        /// <summary>GO EXIT: termina il task attraversando i FINALLY (unwind reason GoExit).</summary>
        private void LowerGoExit(int pc, Stmt stmt, IlIrRegionPath path)
        {
            var b = Block(pc, stmt, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.GoExit);
        }

        /// <summary>RETURN con valore (FUNCTION): terminator Return — l'emitter valuta il valore
        /// inline e lo passa nella transizione; il driver lo scrive nella destinazione decisa
        /// alla Call (ReturnDest + ReturnSlot del frame del callee).</summary>
        private void LowerReturnValue(int pc, ReturnStmt rs, IlIrRegionPath path)
        {
            var b = Block(pc, rs, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.Return);
        }

        /// <summary>RETURN nudo (SUB): dentro un GOSUB riprende la continuazione; fuori è il
        /// Return della procedura (il driver distingue dal GosubStack).</summary>
        private void LowerReturnGosub(int pc, ReturnStmt rs, IlIrRegionPath path)
        {
            var b = Block(pc, rs, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.ReturnGosub);
        }

        /// <summary>GOTO/JUMP: branch non condizionato verso l'etichetta (risolta a fine lowering).</summary>
        private void LowerGoto(int pc, string label, Stmt stmt, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, stmt, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.Branch, TargetPc: -1);
            _pendingLinks.Add((b, label, -1));
        }

        /// <summary>ConditionalJumpStmt(cond, label, jumpWhenTrue): l'emitter valuta cond e
        /// salta all'etichetta se (cond == jumpWhenTrue), altrimenti al successore.</summary>
        private void LowerCondJump(int pc, ConditionalJumpStmt cj, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, cj, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.Switch, Targets: ImmutableArray.Create(-1, succ));
            _pendingLinks.Add((b, cj.InitialTargetLabel.Lexeme, -1));
        }

        /// <summary>Dopo il body della sub: inlina gli handler ON ERROR a livello MODULO
        /// (cross-proc) come blocchi nascosti ALLA FINE dell'IR, così i pc del body restano
        /// sequenziali (il RESUME ErrorLinePc+1 del driver non finisce dentro l'handler) e il
        /// target dell'ON ERROR punta all'ingresso dell'handler inlinalo.</summary>
        public void InlineModuleHandlers()
        {
            if (_pendingModuleHandlers.Count == 0) return;
            foreach (var (b, label, path) in _pendingModuleHandlers)
            {
                if (!_moduleLabelToPc.TryGetValue(label, out var handlerPc))
                {
                    var modIdx = _moduleLabelToIndex[label];
                    handlerPc = InlineModuleHandler(modIdx, path);
                    _moduleLabelToPc[label] = handlerPc;
                }
                var t = b.Terminator;
                b.Terminator = new IlIrTerminator(IlIrTermKind.OnError, TargetProc: handlerPc, ResumePc: t.ResumePc);
            }
            _pendingModuleHandlers.Clear();
        }

        /// <summary>Inlina un handler a livello MODULO nella sub corrente (cross-proc, parità
        /// PCodeVM): le statement del modulo dal label fino alla fine (o al label successivo)
        /// sono lowerate come blocchi nascosti della sub; il pc di ingresso diventa il target
        /// dell'ON ERROR. Il RESUME NEXT dell'handler riprende al pc successivo all'errore
        /// nella sub (ErrorLinePc+1, come lo standard).</summary>
        private int InlineModuleHandler(int modIdx, IlIrRegionPath path)
        {
            var handlerPc = NewPc();
            var end = _moduleTopLevel.Length;
            for (var i = modIdx + 1; i < _moduleTopLevel.Length; i++)
                if (_moduleTopLevel[i] is LabelStmt) { end = i; break; }
            var handlerEndPc = NewPc();
            LowerList(handlerPc, _moduleTopLevel, modIdx + 1, path, handlerEndPc, stopIndex: end, setEntry: false);
            var endBlock = Block(handlerEndPc, null, path);
            endBlock.Terminator = new IlIrTerminator(IlIrTermKind.ReturnGosub);
            return handlerPc;
        }

        /// <summary>Chiusura del lowering: risolve le etichette nei pcs delle statement di
        /// destinazione. I target non risolti restano -1 (edge non valido → il verifier
        /// rifiuta).</summary>
        public void ResolveLabels()
        {
            foreach (var (from, label, _) in _pendingLinks)
            {
                if (!_labelToIndex.TryGetValue(label, out var idx)) continue;
                if (!_stmtEntryPc.TryGetValue(idx, out var targetPc)) continue;
                var t = from.Terminator;
                from.Terminator = t.Kind switch
                {
                    IlIrTermKind.Switch => t with { Targets = t.Targets.IsDefaultOrEmpty
                        ? ImmutableArray.Create(targetPc, -1)
                        : t.Targets.SetItem(0, targetPc) },
                    // Park con Targets (FOR EACH canale): Targets[0] = etichetta di EXIT.
                    IlIrTermKind.Park when !t.Targets.IsDefaultOrEmpty && t.Targets.Length > 0 =>
                        t with { Targets = t.Targets.SetItem(0, targetPc) },
                    IlIrTermKind.Gosub or IlIrTermKind.OnError => t with { TargetProc = targetPc },
                    _ => t with { TargetPc = targetPc }
                };
            }
        }

        /// <summary>Blocco flat: l'emitter valuta lo statement inline; Branch(succ).</summary>
        private void LowerFlat(int pc, Stmt stmt, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, stmt, path);
            b.Terminator = new IlIrTerminator(IlIrTermKind.Branch, TargetPc: succ);
        }

        /// <summary>IF: blocco condizione con terminator Switch — l'emitter valuta le
        /// condizioni dei rami in ordine e salta al primo ramo vero (target[i]), o al ramo
        /// else/successore. I corpi dei rami e dell'else escono a succ.</summary>
        private void LowerIf(int pc, IfStmt ifs, IlIrRegionPath path, int succ)
        {
            var condBlock = Block(pc, ifs, path);
            var targets = new List<int>();
            foreach (var clause in ifs.IfClauses)
            {
                var bodyPc = NewPc();
                targets.Add(bodyPc);
                LowerList(bodyPc, clause.Statements, 0, path, succ, setEntry: false);
            }
            if (ifs.ElseClause is { } el)
            {
                var elsePc = NewPc();
                targets.Add(elsePc);
                LowerList(elsePc, el.Statements, 0, path, succ, setEntry: false);
            }
            else
            {
                targets.Add(succ);
            }
            condBlock.Terminator = new IlIrTerminator(IlIrTermKind.Switch, Targets: targets.ToImmutableArray());
        }

        private void LowerSelect(int pc, SelectStmt sel, IlIrRegionPath path, int succ)
        {
            var testBlock = Block(pc, sel, path);
            var targets = new List<int>();
            foreach (var c in sel.CaseClauses)
            {
                var bodyPc = NewPc();
                targets.Add(bodyPc);
                LowerList(bodyPc, c.Statements, 0, path, succ, setEntry: false);
            }
            if (sel.ElseClause is { } els)
            {
                var elsePc = NewPc();
                targets.Add(elsePc);
                LowerList(elsePc, els.Statements, 0, path, succ, setEntry: false);
            }
            else
            {
                targets.Add(succ);
            }
            testBlock.Terminator = new IlIrTerminator(IlIrTermKind.Switch, Targets: targets.ToImmutableArray());
        }

        /// <summary>FOR unit: il linker appiattisce il loop in ForStmt (init) + LabelStmt +
        /// ForCheckAndJumpStmt (check) + corpo + NextStmt [+ JumpStmt]. Scansione come unità
        /// (il check NON è necessariamente a forIndex+1: può esserci un LabelStmt di loop-start)
        /// e cablaggio init→check, check→(corpo/uscita), corpo→next, next→check.</summary>
        private void LowerForUnit(int entryPc, ImmutableArray<Stmt> body, int forIndex,
            IlIrRegionPath path, int listEndPc, int stopIndex)
        {
            var forStmt = (ForStmt)body[forIndex];
            var initPc = entryPc;
            var checkPc = NewPc();
            var unitEnd = Math.Min(body.Length, stopIndex);

            // Trova il check (ForCheckAndJumpStmt) subito dopo l'init; i LabelStmt di
            // loop-start intermedi vengono mappati al blocco del check (GOTO → check).
            var checkIndex = -1;
            for (var i = forIndex + 1; i < unitEnd; i++)
            {
                if (body[i] is ForCheckAndJumpStmt) { checkIndex = i; break; }
                if (body[i] is LabelStmt lbl) _stmtEntryPc[i] = checkPc;
                else if (body[i] is not EnterLoopStmt) break; // struttura inattesa: stop
            }
            if (checkIndex < 0)
                throw new InvalidOperationException($"FOR senza ForCheckAndJumpStmt (indice {forIndex}).");

            // Trova il NextStmt di chiusura (rispettando i FOR annidati).
            var depth = 0;
            var nextIndex = -1;
            for (var i = checkIndex + 1; i < unitEnd; i++)
            {
                if (body[i] is ForStmt) { depth++; continue; }
                if (body[i] is NextStmt)
                {
                    if (depth == 0) { nextIndex = i; break; }
                    depth--;
                }
            }
            if (nextIndex < 0)
                throw new InvalidOperationException($"FOR senza NextStmt (indice {forIndex}).");

            // L'uscita del loop NON è il statement subito dopo NextStmt: dopo NEXT il linker
            // emette i marker di chiusura (JumpStmt di loop-back, ExitLoopStmt, LabelStmt).
            // L'uscita del check deve puntare DOPO ExitLoopStmt, al primo statement del
            // codice che segue il loop — altrimenti l'uscita cade sul JumpStmt (loop-back)
            // → check → exit → jump → ... loop infinito.
            var exitIndex = nextIndex + 1;
            while (exitIndex < unitEnd && body[exitIndex] is JumpStmt or LabelStmt or ExitLoopStmt)
                exitIndex++;

            var bodyPc = NewPc();
            var nextPc = NewPc();
            var loopExit = exitIndex < unitEnd ? NewPc() : listEndPc;

            var initBlock = Block(initPc, forStmt, path);
            initBlock.Terminator = new IlIrTerminator(IlIrTermKind.Branch, TargetPc: checkPc);

            var checkBlock = Block(checkPc, body[checkIndex], path);
            checkBlock.Aux = forStmt;
            checkBlock.Terminator = new IlIrTerminator(
                IlIrTermKind.Switch, Targets: ImmutableArray.Create(bodyPc, loopExit));

            // Corpo: da checkIndex+1 a nextIndex (escluso).
            if (checkIndex + 1 < nextIndex)
                LowerList(bodyPc, body, checkIndex + 1, path, nextPc, stopIndex: nextIndex, setEntry: false);

            var nextBlock = Block(nextPc, body[nextIndex], path);
            nextBlock.Aux = forStmt;
            nextBlock.Terminator = new IlIrTerminator(IlIrTermKind.Branch, TargetPc: checkPc);

            if (exitIndex < unitEnd)
                LowerList(loopExit, body, exitIndex, path, listEndPc,
                    stopIndex: unitEnd, setEntry: true);
        }

        /// <summary>Park (SLEEP/SEND/RECEIVE/WAITKEY): Await + terminator Park con il pc di
        /// ripresa (dopo il consume, controllo al successore). Al retry si ri-esegue dal pc.</summary>
        private void LowerPark(int pc, Stmt stmt, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, stmt, path);
            b.Instructions.Add(new IlIrInst(IlIrInstKind.Await));
            b.Terminator = new IlIrTerminator(IlIrTermKind.Park, ResumePc: succ);
            // READ: allega i campi @GET accumulati (il blocco PARK li cicla con park per-tasto).
            if (stmt is ClipperReadStmt && _pendingReadFields.Count > 0)
            {
                b.ReadFields = new List<AtGetStmt>(_pendingReadFields);
                _pendingReadFields.Clear();
            }
        }

        /// <summary>FOR EACH su canale: park che fa la RECEIVE con flag OK. ResumePc = corpo del
        /// loop (ok=true); Targets[0] = etichetta di EXIT (ok=false, canale chiuso e svuotato).</summary>
        private void LowerForEachChannelPark(int pc, ForEachAdvanceAndAssignStmt stmt, IlIrRegionPath path, int succ)
        {
            var b = Block(pc, stmt, path);
            b.Instructions.Add(new IlIrInst(IlIrInstKind.Await));
            b.Terminator = new IlIrTerminator(IlIrTermKind.Park, ResumePc: succ, Targets: ImmutableArray.Create(-1));
            _pendingLinks.Add((b, stmt.ExitLabel.Lexeme, -1));
        }

        /// <summary>Park della forma FUNZIONE CONCURRENCY.SLEEPASYNC(ms): blocco con terminator
        /// Park e Aux SleepFnPark (l'emitter chiama IlGpmRuntime.SleepStep e scrive "" nello slot).</summary>
        private void LowerSleepFnPark(int pc, Stmt stmt, BuiltinFunctionCallExpr call, IlIrRegionPath path, int succ, int targetSlot)
        {
            var b = Block(pc, stmt, path);
            b.Instructions.Add(new IlIrInst(IlIrInstKind.Await));
            b.Terminator = new IlIrTerminator(IlIrTermKind.Park, ResumePc: succ);
            b.Aux = new SleepFnPark(call.Args.Length > 0 ? call.Args[0] : null, targetSlot);
        }

        /// <summary>Chiamata: il callValue può essere un FunctionExpr (SymbolId diretto) o un
        /// BuiltinFunctionCallExpr col nome della procedura utente (risolto via nameToId — il
        /// linker rappresenta così le call in espressione). Callee may-park → terminator Call
        /// (suspendable) con gli argomenti materializzati in Spills; flat → LowerFlat.</summary>
        private void LowerCall(int pc, Stmt stmt, Expr callValue, IlIrRegionPath path, int succ,
            IlReturnDestination dest = IlReturnDestination.Discard, int returnSlot = -1)
        {
            var unwrapped = UnwrapConversions(callValue);
            // AWAIT CONCURRENCY.SLEEPASYNC(ms) (forma funzione del timer await) → park, non call
            // flat bloccante: stessa struttura di SQL.EXEC forma funzione, ma con SleepStep.
            var sleepCall = unwrapped is AwaitExpr { Target: BuiltinFunctionCallExpr slpW } ? slpW
                : unwrapped as BuiltinFunctionCallExpr;
            if (sleepCall is not null && sleepCall.Name.Kind == TokenKind.Sleep)
            {
                LowerSleepFnPark(pc, stmt, sleepCall, path, succ, dest == IlReturnDestination.Local ? returnSlot : -1);
                return;
            }
            // SQL.EXEC forma FUNZIONE → park coarse (come la forma comando), col valore a slot.
            if (unwrapped is BuiltinFunctionCallExpr sbf && sbf.Name.Kind == TokenKind.SqlExec)
            {
                var sb = Block(pc, stmt, path);
                sb.Instructions.Add(new IlIrInst(IlIrInstKind.Await));
                sb.Terminator = new IlIrTerminator(IlIrTermKind.Park, ResumePc: succ);
                sb.Aux = new SqlExecFnPark(sbf, returnSlot);
                return;
            }
            // GO FUNCTION ... INTO x / FETCH espressione → il target riceve un FUTURE:
            // registralo per l'implicit-await parcheggiante dei blocchi che lo leggono.
            // Stessa cosa per AI.BLACKBOARDWAIT/AI.AGENTSWARM (Strada C): la call è async e
            // ritorna un PCodeFutureValue (il task GPM parcheggia sul read, mai sul worker).
            if (dest == IlReturnDestination.Local && returnSlot >= 0
                && (unwrapped is GoFunctionExpr or HttpFetchExpr
                    || MayParkAnalysis.IsTeamParkBuiltin(unwrapped)))
                _futureSlots.Add(returnSlot);
            int targetId;
            ImmutableArray<Expr> args;
            switch (unwrapped)
            {
                case FunctionExpr fe:
                    if (!_mayPark.Contains(fe.SymbolId)) { LowerFlat(pc, stmt, path, succ); return; }
                    targetId = fe.SymbolId;
                    args = fe.Args;
                    break;
                case BuiltinFunctionCallExpr bfc when _nameToId is not null
                    && _nameToId.TryGetValue(bfc.Name.Lexeme, out var uid):
                    if (!_mayPark.Contains(uid)) { LowerFlat(pc, stmt, path, succ); return; }
                    targetId = uid;
                    args = bfc.Args;
                    break;
                default:
                    LowerFlat(pc, stmt, path, succ);
                    return;
            }

            var b = Block(pc, stmt, path);
            ImmutableArray<int> argSlots = ImmutableArray<int>.Empty;
            if (args.Length > 0)
            {
                var slots = new int[args.Length];
                for (var i = 0; i < args.Length; i++)
                    slots[i] = _spill++;
                argSlots = slots.ToImmutableArray();
            }
            b.Terminator = new IlIrTerminator(
                IlIrTermKind.Call, TargetProc: targetId, ResumePc: succ,
                ReturnDest: dest, ArgSlots: argSlots, ReturnSlot: returnSlot);
        }
    }
}
