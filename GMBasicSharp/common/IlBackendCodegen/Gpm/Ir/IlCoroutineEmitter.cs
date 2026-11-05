// FILE: Basic.IlBackend.Gpm.Ir.IlCoroutineEmitter.cs
// G2 — Emitter v2 separato (piano §8, §12 G2): IR → metodi CIL `StepV2_<symbolId>`.
// Ogni StepV2_<id>(PCodeTask task, IlActivationFrame frame) esegue UN block (value-Pc)
// e ritorna una IlStepTransition: il driver v2 la applica. L'emitter valuta le espressioni
// straight-line INLINE a partire da AstNode del blocco; le istruzioni IR materializzano i
// safepoint (Await) e i terminator il controllo. Sottoinsieme G2: sequenze, LET, PRINT,
// IF, SELECT, FOR/WHILE, SLEEP, SEND/RECEIVE, WAIT/DONE, GO SUB, call may-park.
//
// Nessun cambiamento al percorso flat: i metodi StepV2_* sono additivi e girano solo
// con il driver v2 (GMBASIC_IL_GPM_V2=1 / harness di test).

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Reflection;
using System.Reflection.Emit;
using Basic.IlBackend.Gpm;
using Basic.IlBackend.Gpm.Ir;
using Basic.Runtime;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public static class IlCoroutineEmitter
{
    /// <summary>Emesso subito dopo EmitGpmSteppers: genera StepV2_&lt;id&gt; per le procedure
    /// may-park nel sottoinsieme G2. Le procedure con costrutti fuori subset (TRY, ON ERROR,
    /// RETURN con valore, SELECT con pattern...) NON ricevono lo StepV2 (gireranno flat).
    /// Include (G4) le procedure locali con CATTURA: classi __ClosureCtx scoperte nella
    /// SymbolTable, i cui metodi may-park ricevono StepV2_&lt;id&gt; come metodo D'ISTANZA
    /// della classe closure (this = __context).</summary>
    internal static void EmitV2Steppers(
        Dictionary<int, FunctionDefinition> procedureDefinitions,
        TypeBuilder programType,
        ImmutableHashSet<int> mayPark,
        Dictionary<int, TypeBuilder>? classTypes = null,
        Dictionary<int, FieldBuilder>? classFields = null,
        Dictionary<int, List<IlProgramCompiler.ClassConstructorInfo>>? classConstructors = null,
        BasicProgram? program = null)
    {
        // Procedure locali con cattura: promosse dal Linker a metodi d'istanza delle classi
        // sintetiche __ClosureCtx_<FUNCTION>_<id> (registrate nella SymbolTable, MAI nelle
        // Declarations dei moduli — stesso pattern di IlProgramCompiler.Host).
        var closureProcs = new Dictionary<int, FunctionDefinition>();
        var closureClassOf = new Dictionary<int, int>();
        if (program is not null && classTypes is not null)
        {
            var clsCount = program.Symbols.Count;
            for (var ci = 0; ci < clsCount; ci++)
            {
                var sym = program.Symbols.GetSymbol(ci);
                if (sym.DeclarationNode is ClassDefinition synthClass
                    && sym.OriginalName.StartsWith("__ClosureCtx", StringComparison.Ordinal)
                    && classTypes.ContainsKey(synthClass.SymbolId))
                {
                    foreach (var m in synthClass.Members)
                    {
                        if (m is MethodMember mm && mm.Procedure is FunctionDefinition f)
                        {
                            closureProcs[f.SymbolId] = f;
                            closureClassOf[f.SymbolId] = synthClass.SymbolId;
                        }
                    }
                }
            }
        }

        // Il mayPark arriva dal path flat (senza le closure): Main chiama una locale con
        // cattura via BuiltinFunctionCallExpr → la propagazione va ricalcolata includendola.
        if (closureProcs.Count > 0)
        {
            var allProcs = procedureDefinitions.Values.Concat(closureProcs.Values).ToImmutableArray();
            mayPark = MayParkAnalysis.ComputeMayPark(allProcs, id => allProcs.FirstOrDefault(p => p.SymbolId == id),
                program is null ? null : symId => program.Symbols.GetSymbol(symId).DataType);
        }

        if (mayPark.Count == 0) return;
        var byId = (int id) => procedureDefinitions.TryGetValue(id, out var d)
            ? d
            : closureProcs.TryGetValue(id, out var cd) ? cd : null;
        var nameToId = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var p in procedureDefinitions.Values.Concat(closureProcs.Values))
        {
            nameToId[p.OriginalName.Lexeme] = p.SymbolId;
            nameToId[p.Name.Lexeme] = p.SymbolId;
        }
        foreach (var def in procedureDefinitions.Values.Concat(closureProcs.Values))
        {
            if (!mayPark.Contains(def.SymbolId)) continue;
            var isGlobalSym = program is null
                ? (Func<int, bool>?)null
                : sym => { var s = program.Symbols.GetSymbol(sym); return s.Flags.HasFlag(SymbolFlags.IsGlobal) || s.Flags.HasFlag(SymbolFlags.IsStaticStorage); };
            if (!IsV2Subset(def, mayPark, nameToId, program is null ? null : sym => program.Symbols.GetSymbol(sym).DataType, isGlobalSym)) continue;
            try
            {
                // Top-level del modulo (label/statement a livello modulo, es. un handler
                // ON ERROR GOTO da una SUB verso un label del modulo): il lowering li inlina
                // nella SUB che li referenzia (cross-proc, parità PCodeVM).
                var moduleTopLevel = program?.MainModule is { } mm
                    ? mm.ExecutionOrder.SelectMany(l => mm.Statements.TryGetValue(l, out var sl) ? sl : []).ToImmutableArray()
                    : ImmutableArray<Stmt>.Empty;
                var ir = StacklessLowering.Lower(def, mayPark, byId, nameToId,
                    program is null ? null : sym => program.Symbols.GetSymbol(sym).DataType,
                    moduleTopLevel);
                IlCoroutineVerifier.AssertValid(ir, def.OriginalName.Lexeme);
                var isEntry = def.OriginalName.Lexeme.Equals("Main", StringComparison.OrdinalIgnoreCase);
                var isClosure = closureClassOf.TryGetValue(def.SymbolId, out var closureClassId);
                // classConstructors/classFields/closureClassOf vanno a OGNI emitter: anche il
                // CHIAMANTE (es. Main) istanzia la closure (DIM __context = NEW ...) e deve
                // trovare lo slot __context alla call. Solo _closureType è specifico della
                // procedura di closure (StepV2 d'istanza).
                var emitter = new MethodEmitter(def, ir, programType, byId, nameToId,
                    isClosure ? classTypes![closureClassId] : null,
                    classConstructors,
                    classFields,
                    closureClassOf,
                    program,
                    mayPark);
                emitter.Emit("StepV2_" + def.SymbolId);
                if (isEntry) emitter.Emit("StepV2_Main"); // alias entry → ProcedureId -1 (convenzione flat)
            }
            catch (Exception ex)
            {
                // L'emissione v2 è ADDITIVA e opzionale (G2 in corso): qualunque procedura
                // che non si lascia emettere in v2 NON deve MAI rompere la compilazione del
                // percorso flat. Si salta: la procedura girerà sul path flat, senza StepV2_*.
                // Il gate v2 è separato e copre solo il sottoinsieme emettibile.
                if (Environment.GetEnvironmentVariable("GMB_DEBUG_V2") == "1")
                    throw new InvalidOperationException(
                        $"StepV2 per '{def.OriginalName.Lexeme}' fallito: {ex.Message} --- {ex.StackTrace}", ex);
            }
        }
    }

    /// <summary>True se la procedura è interamente nel sottoinsieme G2 (corpo piatto linkato).
    /// Il predicato è SUFFICIENTE per l'emettibilità: verifica le espressioni di OGNI statement
    /// (un metodo emesso a metà lascia label non marcati → "Label not marked" al Save).</summary>
    public static bool IsV2Subset(FunctionDefinition def, ImmutableHashSet<int> mayPark, Dictionary<string, int>? nameToId = null,
        Func<int, Basic.DataType?>? symbolType = null, Func<int, bool>? isGlobalSymbol = null)
    {
        // Il v2 stepper NON sa emettere l'accesso ai campi di una variabile UDT (value type):
        // lo emette in silenzio a vuoto (variabili locali) o lascia label non marcati al Save
        // (variabili globali). Se il corpo accede a un campo di una variabile UDT, la
        // procedura NON è nel subset v2 → fallback al percorso flat (corretto).
        if (symbolType is not null && BodyAccessesUdtField(def, symbolType)) return false;
        // Il v2 stepper NON sa emettere lo store di un campo/proprietà su un oggetto runtime
        // (ws.OnMessage, res.StatusCode, cfg.Port): l'emissione parziale lascia label non
        // marcati → "Label N has not been marked" al Save dell'assembly. Fallback al flat.
        if (symbolType is not null && BodyAssignsRuntimeField(def, symbolType)) return false;
        // Il v2 stepper NON sa emettere l'accesso a elementi di un array GLOBALE/STATIC
        // (il global-init piatto lo crea short[]/double[], la lettura v2 userebbe ldelem del
        // tipo sbagliato; l'emissione parziale lascia label non marcati — bug 2026-10-01:
        // GO SUB/WAIT con store su array globale → "Label not marked"). Rifiutate la
        // procedura → gira flat (corretto). Gli array LOCALI restano v2.
        if (isGlobalSymbol is not null && BodyAccessesGlobalArray(def, isGlobalSymbol)) return false;
        foreach (var s in def.Body)
            if (!StatementIsV2(s, mayPark, nameToId, symbolType)) return false;
        return true;
    }

    /// <summary>Vero se il corpo SCRIVE un campo/proprietà di un oggetto runtime (non UDT/closure):
    /// `ws.OnMessage = "..."`, `res.StatusCode = 404`, `cfg.Port = 8090`. L'emitter v2 sa fare solo
    /// store su variabili e su campi di una CLASSE (closure); un FieldTarget su un oggetto runtime
    /// lo faceva fallire a metà emissione — e un metodo già registrato nel TypeBuilder con label
    /// non marcati rompe il Save dell'INTERA assembly ("Label N has not been marked"). Questo
    /// predicato è quindi obbligatorio: la procedura gira sul percorso flat, che li emette.
    /// Bug reale: `webapp/app.bas` (handler WebSocket) non compilava su IL.</summary>
    private static bool BodyAssignsRuntimeField(FunctionDefinition def, Func<int, Basic.DataType?> symbolType)
    {
        foreach (var stmt in def.Body)
            foreach (var node in WalkNodes(stmt))
            {
                if (node is FieldTarget ft && ft.Structure is VariableTarget fvt
                    && symbolType(fvt.SymbolId) is not ClassType)
                    return true;
            }
        return false;
    }

    /// <summary>Vero se il corpo legge/scrive un elemento di un array GLOBALE/STATIC
    /// (pattern non emettibile dallo stepper v2, vedi <see cref="IsV2Subset"/>).</summary>
    private static bool BodyAccessesGlobalArray(FunctionDefinition def, Func<int, bool> isGlobalSymbol)
    {
        foreach (var stmt in def.Body)
            foreach (var node in WalkNodes(stmt))
            {
                switch (node)
                {
                    case ArrayAccessExpr aa when isGlobalSymbol(aa.SymbolId):
                    case IndexedAccessExpr ia when ia.Base is VariableExpr iv && isGlobalSymbol(iv.SymbolId):
                        return true;
                }
            }
        return false;
    }

    /// <summary>Vero se il corpo accede a un campo/proprietà di una variabile di tipo UDT
    /// (value type) — pattern che l'emitter v2 non supporta (vedi <see cref="IsV2Subset"/>).</summary>
    private static bool BodyAccessesUdtField(FunctionDefinition def, Func<int, Basic.DataType?> symbolType)
    {
        foreach (var stmt in def.Body)
            foreach (var node in WalkNodes(stmt))
            {
                switch (node)
                {
                    case FieldAccessExpr fa when fa.Structure is VariableExpr fve
                        && symbolType(fve.SymbolId) is UserDefinedType:
                    case PropertyAccessExpr pa when pa.Instance is VariableExpr pve
                        && symbolType(pve.SymbolId) is UserDefinedType:
                    case FieldTarget ft when ft.Structure is VariableTarget fvt
                        && symbolType(fvt.SymbolId) is UserDefinedType:
                        return true;
                }
            }
        return false;
    }

    /// <summary>Visita riflessiva dell'albero AST. A differenza di IlProgramCompiler.WalkAst
    /// attraversa anche i contenitori NON-AstNode (es. <c>Assignment</c>, che non deriva da
    /// AstNode) e protegge dai cicli con un visited-set — serve a raggiungere i <c>Target</c>
    /// delle assegnazioni (es. <c>FieldTarget</c> di <c>g.Id = 77</c>).</summary>
    private static IEnumerable<Basic.AstNode> WalkNodes(object? node)
    {
        var visited = new HashSet<object>(ReferenceEqualityComparer.Instance);
        foreach (var n in WalkNodesCore(node, visited)) yield return n;
    }

    private static IEnumerable<Basic.AstNode> WalkNodesCore(object? node, HashSet<object> visited)
    {
        if (node is null) yield break;
        var type = node.GetType();
        if (type.IsPrimitive || type.IsEnum || node is string || node is Basic.Token) yield break;
        if (!visited.Add(node)) yield break;
        if (node is Basic.AstNode) yield return (Basic.AstNode)node;
        foreach (var prop in type.GetProperties(BindingFlags.Public | BindingFlags.Instance))
        {
            if (prop.Name == "Parent") continue;
            object? value;
            try { value = prop.GetValue(node); } catch { continue; }
            if (value is null || value is string || value is Basic.DataType) continue;
            if (value is System.Collections.IEnumerable en)
            {
                if (value.GetType().GetProperty("IsDefault")?.GetValue(value) is true) continue;
                foreach (var item in en)
                    foreach (var n in WalkNodesCore(item, visited)) yield return n;
            }
            else
            {
                foreach (var n in WalkNodesCore(value, visited)) yield return n;
            }
        }
    }

    public static bool StatementIsV2(Stmt s, ImmutableHashSet<int> mayPark, Dictionary<string, int>? nameToId = null,
        Func<int, Basic.DataType?>? symbolType = null)
    {
        return StatementIsV2Core(s, mayPark, nameToId, symbolType);
    }

    private static bool StatementIsV2Core(Stmt s, ImmutableHashSet<int> mayPark, Dictionary<string, int>? nameToId = null,
        Func<int, Basic.DataType?>? symbolType = null)
    {
        switch (s)
        {
            case ForEachInitStmt fei:
                // FOR EACH su canale (parità Go for v := range ch): v2 SOLO se la collezione è
                // un canale (il FOR EACH su array non è nel subset v2).
                return symbolType != null
                    && fei.CollectionExpr is VariableExpr fce
                    && symbolType(fce.SymbolId) is ChannelType;
            case ForEachAdvanceAndAssignStmt:
                // Avanzamento del FOR EACH canale: riceve con flag OK (park). Se il ForEachInit
                // era un canale il subset è già stato accettato; se era un array, la procedura
                // non è v2 (rigettata al ForEachInitStmt).
                return true;
            case LetStmt let:
                return let.Assignments.All(a =>
                    (a.Target is VariableTarget || a.Target is FieldTarget) && (
                        a.Value is GoFunctionExpr gfe2 && gfe2.Args.All(ExprIsV2) && (gfe2.WaitGroup is null || ExprIsV2(gfe2.WaitGroup)) ||
                        ExprIsV2(a.Value) ||
                        (UnwrapConversions(a.Value) is FunctionExpr lfe &&
                         mayPark.Contains(lfe.SymbolId) && lfe.Args.All(ExprIsV2)) ||
                        (nameToId != null && UnwrapConversions(a.Value) is BuiltinFunctionCallExpr ub
                         && nameToId.TryGetValue(ub.Name.Lexeme, out var uid)
                         && mayPark.Contains(uid) && ub.Args.All(ExprIsV2))));
            case PrintStmt p:
                return p.Arguments.All(a => a.Expression is null || ExprIsV2(a.Expression));
            case CPrintStmt cp:
                // CPRINT (console grafico): nel v2 emesso come PRINT verso l'output host —
                // nel harness headless (NullGraphicsService) il marker deve comunque apparire.
                return cp.Arguments.All(a => a.Expression is null || ExprIsV2(a.Expression));
            case FontStmt fnt: return ExprIsV2(fnt.FontPath) && ExprIsV2(fnt.Size);
            case ColorStmt clr: return (clr.Foreground is null || ExprIsV2(clr.Foreground)) && (clr.Background is null || ExprIsV2(clr.Background));
            case LocateStmt loc: return ExprIsV2(loc.ColumnExpr) && ExprIsV2(loc.LineExpr);
            case LineStmt ln: return (ln.X1 is null || ExprIsV2(ln.X1)) && (ln.Y1 is null || ExprIsV2(ln.Y1))
                && ExprIsV2(ln.X2) && ExprIsV2(ln.Y2) && (ln.Color is null || ExprIsV2(ln.Color));
            case CircleStmt ci: return ExprIsV2(ci.X) && ExprIsV2(ci.Y) && ExprIsV2(ci.Radius) && (ci.Color is null || ExprIsV2(ci.Color));
            case PSetStmt ps: return ExprIsV2(ps.X) && ExprIsV2(ps.Y) && (ps.Color is null || ExprIsV2(ps.Color));
            case PresetStmt pr: return ExprIsV2(pr.X) && ExprIsV2(pr.Y);
            case DrawStmt dr: return ExprIsV2(dr.CommandString);
            case SaveScreenStmt ss: return ss.Name is null || ExprIsV2(ss.Name);
            case RestoreScreenStmt rs: return rs.Name is null || ExprIsV2(rs.Name);
            case DeclarationStmt d:
                // Array fuori subset, TRANNE il fisso 1D numerico (DIM px(10) AS INTEGER —
                // pattern grafica in goroutine): newarr + IndexedAccessExpr + UBOUND.
                return d.Declarations.All(decl =>
                    (decl.Dimensions.Length == 0 && (decl.InitialValue is null || decl.InitialValue is FunctionExpr || ExprIsV2(decl.InitialValue)))
                    || (decl.Dimensions.Length == 1
                        && (decl.DataType is ArrayType arrT ? arrT.ElementType : decl.DataType) is IntegerType or LongType or DoubleType
                        && decl.Dimensions[0] is NumberExpr));
            case ForStmt f:
                return ExprIsV2(f.Start) && ExprIsV2(f.End) && (f.Step is null || ExprIsV2(f.Step));
            case SendStmt send:
                return ExprIsV2(send.Value) && ExprIsV2(send.Channel);
            case ReceiveStmt recv:
                if (recv.Destination is not VariableTarget) return false;
                if (recv.OkFlag is { } okf && okf is not VariableTarget) return false;
                return ExprIsV2(recv.Channel);
            case CloseChannelStmt cc:
                return ExprIsV2(cc.Channel);
            case MutexLockStmt ml:
                return ExprIsV2(ml.Mutex);
            case MutexUnlockStmt mu:
                return ExprIsV2(mu.Mutex);
            case MutexRLockStmt mrl:
                return ExprIsV2(mrl.Mutex);
            case MutexRUnlockStmt mru:
                return ExprIsV2(mru.Mutex);
            case OnceStmt once:
                // ONCE RUN o, DoInit: park se il Once è in Running (parità Go sync.Once.Do).
                return ExprIsV2(once.Once);
            case DeferStmt defer:
                // DEFER <body>: il body è un singolo statement abbassato come blocco nascosto
                // (EmitDeferPush/DeferDone); il body stesso può essere un park → statement v2.
                return StatementIsV2(defer.Body, mayPark, nameToId, symbolType);
            case CondWaitStmt cw:
                return ExprIsV2(cw.Cond) && ExprIsV2(cw.Mutex);
            case CondSignalStmt cs:
                return ExprIsV2(cs.Cond);
            case CondBroadcastStmt cb:
                return ExprIsV2(cb.Cond);
            case SleepStmt sl:
                return ExprIsV2(sl.Duration);
            case WaitKeyStmt:
                return true;
            case GetKeyboardStmt:
                return true;
            case AtGetStmt ag:
                // Campo @GET (READ): reso e atteso per-tasto dal blocco PARK del READ.
                return (ag.Row is null || ExprIsV2(ag.Row)) && (ag.Col is null || ExprIsV2(ag.Col))
                    && (ag.ControlWidth is null || ExprIsV2(ag.ControlWidth))
                    && (ag.When is null || ExprIsV2(ag.When))
                    && (ag.Valid is null || ExprIsV2(ag.Valid));
            case ClipperReadStmt:
                return true;
            case InkeyStmt inkey:
                return inkey.TimeoutExpr is null; // INKEY senza timeout = poll non bloccante
            case WaitStmt w:
                return ExprIsV2(w.WaitGroup);
            case DoneStmt dn:
                return ExprIsV2(dn.WaitGroup);
            case TcpConnectStmt tcp:
                return ExprIsV2(tcp.Host) && ExprIsV2(tcp.Port)
                    && (tcp.Engine is null || ExprIsV2(tcp.Engine))
                    && (tcp.Timeout is null || ExprIsV2(tcp.Timeout));
            case SslOpenStmt sslOpen:
                return ExprIsV2(sslOpen.Port) && ExprIsV2(sslOpen.CertificatePath)
                    && (sslOpen.CertificatePassword is null || ExprIsV2(sslOpen.CertificatePassword));
            case SslConnectStmt sslC:
                return ExprIsV2(sslC.Host) && (sslC.Port is null || ExprIsV2(sslC.Port))
                    && (sslC.Timeout is null || ExprIsV2(sslC.Timeout));
            case WebSocketConnectStmt ws:
                return ExprIsV2(ws.Host) && ExprIsV2(ws.Port)
                    && (ws.Path is null || ExprIsV2(ws.Path))
                    && (ws.Timeout is null || ExprIsV2(ws.Timeout));
            case TcpSendStmt tcpS:
                return ExprIsV2(tcpS.ClientHandle) && ExprIsV2(tcpS.Data);
            case TcpRecvStmt tcpR:
                return ExprIsV2(tcpR.ClientHandle) && (tcpR.Timeout is null || ExprIsV2(tcpR.Timeout));
            case TcpAcceptStmt tcpA:
                return ExprIsV2(tcpA.ServerHandle) && (tcpA.Timeout is null || ExprIsV2(tcpA.Timeout));
            case UdpRecvStmt udpR:
                return ExprIsV2(udpR.Handle) && (udpR.Timeout is null || ExprIsV2(udpR.Timeout));
            case UdpOpenStmt udpO:
                return udpO.Port is null || ExprIsV2(udpO.Port);
            case TcpOpenStmt tcpO:
                return ExprIsV2(tcpO.Port)
                    && (tcpO.Engine is null || ExprIsV2(tcpO.Engine));
            case TcpCloseStmt tcpC:
                return ExprIsV2(tcpC.Handle);
            case UdpSendStmt udpS:
                return ExprIsV2(udpS.Handle) && ExprIsV2(udpS.Host) && ExprIsV2(udpS.Port) && ExprIsV2(udpS.Data);
            case UdpCloseStmt udpC:
                return ExprIsV2(udpC.Handle);
            case SetOptionStmt so:
                // SETOPTION handle, "OPT", valore — veloce (non park, INetworkService.
                // TcpSetOption; emesso inline come TcpCloseStmt).
                return ExprIsV2(so.Handle) && ExprIsV2(so.Option) && ExprIsV2(so.Value);
            case ShutdownStmt shut:
                // SHUTDOWN handle, modo — veloce (non park, INetworkService.TcpShutdown;
                // emesso inline come il flat EmitShutdownStatement).
                return ExprIsV2(shut.Handle) && ExprIsV2(shut.Mode);
            case SyncStmt:
                return true;
            case ScreenStmt scr:
                return ExprIsV2(scr.Mode)
                    && (scr.ActivePage is null || ExprIsV2(scr.ActivePage))
                    && (scr.VisiblePage is null || ExprIsV2(scr.VisiblePage));
            case PointStmt pt:
                return ExprIsV2(pt.X) && ExprIsV2(pt.Y) && (pt.Color is null || ExprIsV2(pt.Color));
            case LoadTextureStmt lt:
                return ExprIsV2(lt.FileName)
                    && (lt.CellWidth is null || ExprIsV2(lt.CellWidth))
                    && (lt.CellHeight is null || ExprIsV2(lt.CellHeight));
            case SpriteCommandStmt sp when sp.Command.Kind is TokenKind.SpriteLoad or TokenKind.SpriteGetPixels:
                return sp.Arguments.All(ExprIsV2);
            case ClsStmt:
                return true;
            case ErrorStmt er:
                return ExprIsV2(er.ErrorCode) && (er.Description is null || ExprIsV2(er.Description));
            case ThrowStmt ts:
                return ts.Value is null || ExprIsV2(ts.Value);
            case GoProcedureStmt gp:
                if (gp.Call is not FunctionExpr gfe) return false;
                if (!gfe.Args.All(ExprIsV2)) return false;
                return gp.WaitGroup is null || ExprIsV2(gp.WaitGroup);
            case SqlExecStmt se:
                // SQL EXEC con coarse-park (emesso da EmitSqlExecPark → TrySqlExecPark).
                // Senza, qualunque procedura con SQL EXEC restava flat e le riscritture
                // csv/parquet bloccavano il worker IL (bug 2026-10-01, todo Coarse-Park).
                return ExprIsV2(se.DbAlias) && ExprIsV2(se.Sql) && se.Parameters.All(ExprIsV2);
            case SqlOpenStmt soStep:
                // SQL OPEN: veloce, non park (solo registrazione alias; emesso inline).
                return ExprIsV2(soStep.FilePath);
            case SqlCloseStmt scStep:
                // SQL CLOSE: veloce, non park (emesso inline).
                return ExprIsV2(scStep.DbAlias);
            case RemStmt or EndStmt or EndProcedureStmt
                or LabelStmt or GotoStmt or JumpStmt or GosubStmt
                or EnterLoopStmt or ExitLoopStmt
                or ForCheckAndJumpStmt or NextStmt:
                return true;
            case ExitStmt exitStmt:
                // EXIT DO/FOR/WHILE viene risolto dal parser verso il LabelStmt di uscita
                // del ciclo e abbassato a Branch dall'IR stackless. EXIT FUNCTION non ha
                // un target label e richiede invece la semantica del valore di ritorno.
                return exitStmt.TargetLabel is not null;
            case ReturnStmt rs:
                return rs.Value is null || ExprIsV2(rs.Value);
            case GoExitStmt:
                return true;
            case OnErrorStmt oe:
                // target etichetta della stessa procedura (o GOTO 0); RESUME NEXT è un altro stmt.
                return oe.Target is null || oe.Target.Kind is TokenKind.Number or TokenKind.Identifier;
            case OnErrorResumeStmt or ResumeStmt:
                return true;
            case ConditionalJumpStmt cj:
                return ExprIsV2(cj.Condition);
            case IfStmt ifs:
                if (ifs.IfClauses.Any(c => !ExprIsV2(c.Condition))) return false;
                if (ifs.IfClauses.Any(c => c.Statements.Any(x => !StatementIsV2(x, mayPark, nameToId, symbolType)))) return false;
                return ifs.ElseClause is null || ifs.ElseClause.Statements.All(x => StatementIsV2(x, mayPark, nameToId, symbolType));
            case SelectStmt sel:
                if (!ExprIsV2(sel.Expression)) return false;
                foreach (var c in sel.CaseClauses)
                {
                    if (c.Tests.Any(t => t is not CaseValue and not CaseRange)) return false;
                    if (c.Tests.OfType<CaseValue>().Any(cv => !ExprIsV2(cv.Value))) return false;
                    if (c.Tests.OfType<CaseRange>().Any(cr => !ExprIsV2(cr.Start) || !ExprIsV2(cr.End))) return false;
                    if (c.Statements.Any(x => !StatementIsV2(x, mayPark, nameToId, symbolType))) return false;
                }
                return sel.ElseClause is null || sel.ElseClause.Statements.All(x => StatementIsV2(x, mayPark, nameToId, symbolType));
            case TryCatchStmt tc:
                if (!tc.TryBody.All(x => StatementIsV2(x, mayPark, nameToId, symbolType))) return false;
                foreach (var c in tc.CatchClauses)
                {
                    if (c.WhenCondition is { } wc && !ExprIsV2(wc)) return false;
                    if (!c.Body.All(x => StatementIsV2(x, mayPark, nameToId, symbolType))) return false;
                }
                return tc.Finally is null || tc.Finally.Body.All(x => StatementIsV2(x, mayPark, nameToId, symbolType));
            case LocalProcedureStmt:
                return true; // la dichiarazione è no-op; la procedura locale è compilata a parte
            case GoSelectStmt gs:
                foreach (var c in gs.Cases)
                {
                    switch (c.CommunicationStmt)
                    {
                        case ReceiveStmt rc:
                            if (rc.Destination is not VariableTarget) return false;
                            if (rc.OkFlag is { } okf2 && okf2 is not VariableTarget) return false;
                            if (!ExprIsV2(rc.Channel)) return false;
                            break;
                        case TimeoutStmt ts:
                            if (!ExprIsV2(ts.Duration)) return false;
                            break;
                        case SendStmt ss:
                            if (!ExprIsV2(ss.Channel) || !ExprIsV2(ss.Value)) return false;
                            break;
                        default:
                            return false;
                    }
                    if (!c.Body.All(x => StatementIsV2(x, mayPark, nameToId, symbolType))) return false;
                }
                return gs.DefaultCase is not { } gdc || gdc.All(x => StatementIsV2(x, mayPark, nameToId, symbolType));
            case ExpressionStmt es:
                return es.Expression switch
                {
                    // Call a procedura come statement: may-park → Call suspendable (v2), non
                    // may-park → CallFlatSub (chiamata flat reale, non più un placeholder).
                    // Basta che gli argomenti siano nel subset.
                    FunctionExpr fe => fe.Args.All(ExprIsV2),
                    // I builtin esplicitamente ammessi da ExprIsV2 sono operazioni flat e
                    // l'emitter object sa già valutarli/scartarne il risultato. Escluderli
                    // qui faceva ricadere l'intera Main nel percorso lineare anche quando
                    // conteneva GO/SEND/RECEIVE (es. SYSTEM.GOMAXPROCS nello stress GPM).
                    BuiltinFunctionCallExpr bfc => ExprIsV2(bfc),
                    _ => false
                };
            default:
                return false;
        }
    }

    /// <summary>Espressioni supportate dal subset G2: letterali, variabili, operatori binari/
    /// unari numerici, raggruppamento. Chiamate/flussi/array/oggetti fuori subset.</summary>
    public static bool ExprIsV2(Expr e) => e switch
    {
        NumberExpr or StringExpr or BooleanExpr or VariableExpr => true,
        // Indice di array fisso 1D numerico (DIM px(10) AS INTEGER — grafica in goroutine).
        IndexedAccessExpr ia => ExprIsV2(ia.Base) && ia.Indices.All(ExprIsV2),
        ArrayAccessExpr aa => aa.Indices.All(ExprIsV2),
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.UBound
            && bfc.Args.Length == 1 && ExprIsV2(bfc.Args[0]) => true,
        // Literal LIST (es. DIM x AS LIST<INTEGER> = [1,2,3] — pattern di CATCH in goroutine).
        ListLiteralExpr ll => ll.Elements.All(ExprIsV2),
        GroupingExpr g => ExprIsV2(g.Expression),
        UnaryExpr u => u.Op.Kind is TokenKind.Minus or TokenKind.Plus or TokenKind.Not && ExprIsV2(u.Right),
        BinaryExpr b when (b.Op.Kind is TokenKind.Plus or TokenKind.Minus or TokenKind.Multiply or TokenKind.Divide
            or TokenKind.LessThan or TokenKind.LessEqual or TokenKind.GreaterThan or TokenKind.GreaterEqual
            or TokenKind.ExactlyEqual or TokenKind.NotEqual or TokenKind.Equal or TokenKind.And or TokenKind.Or or TokenKind.Mod)
            && ExprIsV2(b.Left) && ExprIsV2(b.Right) => true,
        BuiltinFunctionCallExpr bfc when IsNumericConversion(bfc.Name.Lexeme) && bfc.Args.All(ExprIsV2) => true,
        // G8-bis: builtin flat POOL/SYNCMAP/ATOMIC/SYSTEM.NUMGOROUTINE/NUMCPU (parità PCodeVM).
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind is
            TokenKind.PoolGet or TokenKind.PoolPut
            or TokenKind.SyncMapStore or TokenKind.SyncMapLoad or TokenKind.SyncMapDelete or TokenKind.SyncMapLoadOrStore
            or TokenKind.AtomicIncrement or TokenKind.AtomicFetchValue or TokenKind.AtomicStore or TokenKind.AtomicCompareSwap or TokenKind.AtomicSwap
            or TokenKind.NumGoroutine or TokenKind.NumCpu
            or TokenKind.Str
            && bfc.Args.All(ExprIsV2) => true,
        // G8-bis: handle di rete LOCALPORT/REMOTEPORT/REMOTEIP (lettura porta/ip di un socket UDP/TCP —
        // veloci, non park).
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind is
            TokenKind.LocalPort or TokenKind.RemotePort or TokenKind.RemoteIp
            && bfc.Args.All(ExprIsV2) => true,
        // SYSTEM.GOMAXPROCS(n) (parità Go runtime.GOMAXPROCS): lettura/configurazione P — flat.
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.GoMaxProcs && bfc.Args.All(ExprIsV2) => true,
        // G8-bis: NET.LOCALHOST() (TokenKind.Host, 0 arg) / NET.GETHOSTIP$ / LEN / ABS — veloci,
        // non park. Senza queste braccia la Main di NetTcpAdvanced (e il repro ABS del bug IL
        // 2026-10-06) ricadeva nel flat sincrono: GO SUB inline + TCP ACCEPT senza timeout → hang.
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind is
            TokenKind.Host or TokenKind.GetHostIp or TokenKind.Len or TokenKind.Abs
            && bfc.Args.All(ExprIsV2) => true,
        // G8-bis: NET.CONN(h) / RECV(h) (byte disponibili, non bloccante) — lettura veloce,
        // non park (parità flat Builtins.cs :1296/:2204).
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind is TokenKind.Conn or TokenKind.Recv
            && bfc.Args.All(ExprIsV2) => true,
        // G8-bis: NET.PEEK(handle, numBytes) — 2 arg = peek di socket non consumante (TcpPeek,
        // veloce). La forma a 1 arg (PEEK di LIST) resta fuori subset.
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind == TokenKind.Peek && bfc.Args.Length == 2
            && bfc.Args.All(ExprIsV2) => true,
        // AI.BLACKBOARDWAIT/AI.AGENTSWARM (Strada C): call async che ritorna un future (park
        // del task GPM sul read), come HttpFetchExpr.
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind is TokenKind.BlackboardWait or TokenKind.AgentSwarm
            or TokenKind.AiHookFire or TokenKind.AiGoalTick
            && bfc.Args.All(ExprIsV2) => true,
        // CONTEXT namespace (parità Go context): BACKGROUND/WITHCANCEL/CANCEL/DONE/CANCELLED — flat.
        BuiltinFunctionCallExpr bfc when bfc.Name.Kind is
            TokenKind.ContextBackground or TokenKind.ContextWithCancel or TokenKind.ContextCancel
            or TokenKind.ContextDone or TokenKind.ContextCancelled
            && bfc.Args.All(ExprIsV2) => true,
        // G5: HTTP FETCH espressione — ritorna un future (async, Task.Run dietro le quinte).
        HttpFetchExpr hf => ExprIsV2(hf.Url) && (hf.Options is null || ExprIsV2(hf.Options)),
        // G4 closure: accesso a un campo della closure (__context.cattura), lettura campo.
        FieldAccessExpr fa when fa.Structure is VariableExpr => true,
        // G4 closure: istanziazione della classe __ClosureCtx (DIM __context = NEW ...).
        NewExpr ne when ne.ParsedType is ClassType && ne.ConstructorArgs is not { Length: > 0 } && ne.InitialValues is not { Length: > 0 } => true,
        _ => false
    };

    /// <summary>Conversioni numeriche che il linker inietta per i letterali tipizzati
    /// (es. `N = N + 1` con N LONG diventa `N + CLNG(1)`): l'emitter le valuta come double.</summary>
    private static bool IsNumericConversion(string name)
        => name.ToUpperInvariant() is "CLNG" or "CINT" or "CDBL" or "CSNG" or "VAL";

    /// <summary>Scava le conversioni numeriche fino all'espressione interna (per trovare una
    /// FunctionExpr may-park avvolta in CLNG ecc.).</summary>
    private static Expr UnwrapConversions(Expr e)
    {
        while (e is BuiltinFunctionCallExpr { Args: { Length: 1 } } bf && IsNumericConversion(bf.Name.Lexeme))
            e = bf.Args[0];
        return e;
    }

    /// <summary>Emette lo StepV2 per una procedura: dispatch su frame.Pc + un label per block.</summary>
}
