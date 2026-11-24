// FILE: IlBackendCodegen/Gpm/Ir/IlCoroutineEmitter.MethodEmitter.Infra.cs
// PARTIAL — dominio COROUTINE/INFRA: Ldarg/BuildSlotMap/CollectLocals/WalkStmt/WalkExpr.
// Migrato da IlCoroutineEmitter.MethodEmitter.cs (2026-08-30 — split).

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

internal sealed partial class MethodEmitter
{
        private void EmitLdarg(int index)
        {
            switch (index)
            {
                case 0: _il.Emit(OpCodes.Ldarg_0); break;
                case 1: _il.Emit(OpCodes.Ldarg_1); break;
                case 2: _il.Emit(OpCodes.Ldarg_2); break;
                case 3: _il.Emit(OpCodes.Ldarg_3); break;
                default: _il.Emit(OpCodes.Ldarg, index); break;
            }
        }

        private void BuildSlotMap()
        {
            var next = 0;
            if (_def.Parameters is { Length: > 0 } pars)
                for (var i = 0; i < pars.Length; i++)
                {
                    _slotOf[pars[i].SymbolId] = i;
                    _slotOfName[pars[i].OriginalName.Lexeme] = i;
                    next = i + 1;
                }
            // Scansione del corpo: variabili locali in ordine di prima apparizione.
            CollectLocals(_def.Body, next);
            _localCount = Math.Max(next, 16);
        }

        private void CollectLocals(ImmutableArray<Stmt> body, int next)
        {
            var n = next;
            void UseVar(int symId, string? name)
            {
                if (_slotOf.ContainsKey(symId)) return;
                // symId -1 (variabili FOR EACH, solo nome): se il nome ha già uno slot (es.
                // DIM v AS INTEGER prima del FOR EACH), NON sovrascriverlo con uno slot finto.
                if (symId < 0 && name is not null && _slotOfName.ContainsKey(name)) return;
                _slotOf[symId] = n;
                // GLOBAL: il valore vive nel bag condiviso di processo (IlRuntime.ProgramGlobals),
                // NON in frame.Locals (per-task) — due goroutine devono vedere lo stesso valore.
                if (symId >= 0 && _program is not null
                    && _program.Symbols.GetSymbol(symId).Flags.HasFlag(SymbolFlags.IsGlobal)
                    && !_program.Symbols.GetSymbol(symId).Flags.HasFlag(SymbolFlags.IsConst))
                {
                    _globalSlots.Add(n);
                    _symbolForSlot[n] = symId;
                }
                if (name is not null) _slotOfName[name] = n;
                n++;
            }
            foreach (var s in body) WalkStmt(s, UseVar);
            _localCount = n;
        }

        private static void WalkStmt(Stmt s, Action<int, string?> use)
        {
            switch (s)
            {
                case LetStmt let:
                    foreach (var a in let.Assignments)
                    {
                        switch (a.Target)
                        {
                            case VariableTarget vt:
                                use(vt.SymbolId, vt.NameCleaned?.Lexeme ?? vt.NameSuffixed?.Lexeme);
                                break;
                            // x(i) = v / base(i) = v: l'array/collezione è un simbolo come gli altri
                            // (se è GLOBALE finisce nello slot del bag globale, vedi _globalSlots) e
                            // gli indici possono riferire variabili che senza registrazione non
                            // avrebbero slot → "StepV2: variabile senza slot" a metà emissione.
                            case ArrayElementTarget aet:
                                use(aet.SymbolId, aet.NameCleaned?.Lexeme ?? aet.NameSuffixed?.Lexeme);
                                foreach (var ix in aet.Indices) WalkExpr(ix, use);
                                break;
                            case IndexedTarget { Base: VariableExpr itv } idxTarget:
                                use(itv.SymbolId, itv.NameCleaned?.Lexeme ?? itv.NameSuffixed?.Lexeme);
                                foreach (var ix in idxTarget.Indices) WalkExpr(ix, use);
                                break;
                        }
                        WalkExpr(a.Value, use);
                    }
                    break;
                case DeclarationStmt d:
                    foreach (var decl in d.Declarations)
                    {
                        use(decl.SymbolId, decl.NameCleaned.Lexeme);
                        // L'inizializzatore (es. DIM res = FETCH(url$)) può referenziare
                        // variabili/parametri che vanno registrati come slot (il url$ globale
                        // in FETCH(url$) non avrebbe slot → 'variabile senza slot').
                        if (decl.InitialValue is { } declInit) WalkExpr(declInit, use);
                    }
                    break;
                case PrintStmt p:
                    foreach (var a in p.Arguments) if (a.Expression is { } ex) WalkExpr(ex, use);
                    break;
                case CPrintStmt cp:
                    foreach (var a in cp.Arguments) if (a.Expression is { } ex) WalkExpr(ex, use);
                    break;
                case ForStmt f:
                    use(f.Variable.SymbolId, f.Variable.NameCleaned?.Lexeme ?? f.Variable.NameSuffixed?.Lexeme);
                    WalkExpr(f.Start, use); WalkExpr(f.End, use);
                    if (f.Step is { } st) WalkExpr(st, use);
                    break;
                case SendStmt send: WalkExpr(send.Value, use); WalkExpr(send.Channel, use); break;
                case ReceiveStmt recv:
                    if (recv.Destination is VariableTarget rt) use(rt.SymbolId, rt.NameCleaned?.Lexeme ?? rt.NameSuffixed?.Lexeme);
                    WalkExpr(recv.Channel, use);
                    break;
                case SleepStmt sl: WalkExpr(sl.Duration, use); break;
                case WaitStmt w: WalkExpr(w.WaitGroup, use); break;
                case DoneStmt d2: WalkExpr(d2.WaitGroup, use); break;
                case CondWaitStmt cw: WalkExpr(cw.Cond, use); WalkExpr(cw.Mutex, use); break;
                case CondSignalStmt cs2: WalkExpr(cs2.Cond, use); break;
                case CondBroadcastStmt cb2: WalkExpr(cb2.Cond, use); break;
                case GoSelectStmt gs3:
                    // Registra le variabili delle comunicazioni dei case GO SELECT (destinazioni
                    // RECEIVE anche IMPLICITE — senza DIM, come V in GoSelectParkWake — e i canali),
                    // altrimenti SlotOf(V) fallisce a metà emissione → metodo StepV2 parziale.
                    foreach (var c in gs3.Cases)
                    {
                        switch (c.CommunicationStmt)
                        {
                            case ReceiveStmt rc3:
                                if (rc3.Destination is VariableTarget rvt) use(rvt.SymbolId, rvt.NameCleaned?.Lexeme ?? rvt.NameSuffixed?.Lexeme);
                                if (rc3.OkFlag is VariableTarget okvt) use(okvt.SymbolId, okvt.NameCleaned?.Lexeme ?? okvt.NameSuffixed?.Lexeme);
                                WalkExpr(rc3.Channel, use);
                                break;
                            case SendStmt snd3:
                                WalkExpr(snd3.Channel, use);
                                WalkExpr(snd3.Value, use);
                                break;
                            case TimeoutStmt ts3:
                                WalkExpr(ts3.Duration, use);
                                break;
                        }
                        foreach (var bs in c.Body) WalkStmt(bs, use);
                    }
                    if (gs3.DefaultCase is { } dc3) foreach (var bs in dc3) WalkStmt(bs, use);
                    break;
                case ForEachInitStmt fei2:
                    // FOR EACH su canale: la variabile del loop e il canale (CollectionExpr).
                    // I Token delle variabili non hanno SymbolId → nome sufficiente per _slotOfName.
                    foreach (var v in fei2.Variables) use(-1, v.Lexeme);
                    WalkExpr(fei2.CollectionExpr, use);
                    break;
                case ForEachAdvanceAndAssignStmt fea2:
                    foreach (var v in fea2.Variables) use(-1, v.Lexeme);
                    break;
                case TryCatchStmt tc:
                    foreach (var b in tc.TryBody) WalkStmt(b, use);
                    foreach (var c in tc.CatchClauses)
                    {
                        // La variabile CATCH (ex AS ERR / ex AS ClassType) è un local del frame:
                        // il blocco di entry del catch la bind dall'eccezione (BindCatchVariable).
                        if (c.Variable is { } ccv)
                            use(ccv.SymbolId, ccv.NameCleaned.Lexeme);
                        foreach (var b in c.Body) WalkStmt(b, use);
                    }
                    if (tc.Finally is { } fin) foreach (var b in fin.Body) WalkStmt(b, use);
                    break;
                case IfStmt ifs:
                    foreach (var c in ifs.IfClauses) { WalkExpr(c.Condition, use); foreach (var b in c.Statements) WalkStmt(b, use); }
                    if (ifs.ElseClause is { } el) foreach (var b in el.Statements) WalkStmt(b, use);
                    break;
                case SelectStmt sel:
                    WalkExpr(sel.Expression, use);
                    foreach (var c in sel.CaseClauses)
                    {
                        foreach (var t in c.Tests) if (t is CaseValue cv) WalkExpr(cv.Value, use);
                        foreach (var b in c.Statements) WalkStmt(b, use);
                    }
                    if (sel.ElseClause is { } se) foreach (var b in se.Statements) WalkStmt(b, use);
                    break;
                case ExpressionStmt es:
                    if (es.Expression is FunctionExpr fe) foreach (var a in fe.Args) WalkExpr(a, use);
                    break;
                case GoProcedureStmt gp:
                    if (gp.Call is FunctionExpr gfe) foreach (var a in gfe.Args) WalkExpr(a, use);
                    break;
                case ConditionalJumpStmt cj: WalkExpr(cj.Condition, use); break;
                // SQL: il path/la query e i parametri possono riferire simboli (anche GLOBALI,
                // che non hanno un DIM nel corpo) — senza questi case il builder degli slot non
                // li vede e `SlotOf` lancia "variabile senza slot" a metà emissione, lasciando
                // il metodo con etichette non marcate (l'errore esplodeva al Save: "Label N has
                // not been marked"). L'alias NON va raccolto: è emesso come NOME (Ldstr).
                case SqlOpenStmt so0: WalkExpr(so0.FilePath, use); break;
                case SqlExecStmt se0:
                    WalkExpr(se0.Sql, use);
                    foreach (var seArg in se0.Parameters) WalkExpr(seArg, use);
                    break;
                case SqlQueryStmt sq0:
                    WalkExpr(sq0.Sql, use);
                    foreach (var sqArg in sq0.Parameters) WalkExpr(sqArg, use);
                    break;
            }
        }

        private static void WalkExpr(Expr e, Action<int, string?> use)
        {
            switch (e)
            {
                case VariableExpr v: use(v.SymbolId, v.NameCleaned?.Lexeme ?? v.NameSuffixed?.Lexeme); break;
                case BinaryExpr b: WalkExpr(b.Left, use); WalkExpr(b.Right, use); break;
                case UnaryExpr u: WalkExpr(u.Right, use); break;
                case GroupingExpr g: WalkExpr(g.Expression, use); break;
                case HttpFetchExpr hf:
                    WalkExpr(hf.Url, use);
                    if (hf.Options is { } opts) WalkExpr(opts, use);
                    break;
                case GoFunctionExpr gf:
                    foreach (var a in gf.Args) WalkExpr(a, use);
                    break;
                case FunctionExpr fne:
                    foreach (var a in fne.Args) WalkExpr(a, use);
                    break;
                case BuiltinFunctionCallExpr bfc:
                    foreach (var a in bfc.Args) WalkExpr(a, use);
                    break;
                // Lettura di elemento: il simbolo dell'array/collezione va registrato (LoadSlotObject
                // in EmitExprObject/EmitExprDouble usa SlotOf, che senza slot lancia a metà emissione).
                case ArrayAccessExpr aa:
                    use(aa.SymbolId, null);
                    foreach (var ix in aa.Indices) WalkExpr(ix, use);
                    break;
                case IndexedAccessExpr ia:
                    WalkExpr(ia.Base, use);
                    foreach (var ix in ia.Indices) WalkExpr(ix, use);
                    break;
            }
        }

        // ─── Tracciamento delle etichette (emissione fallita ≠ assembly rotta) ──────────────
        // Un'eccezione a metà emissione lascia il metodo GIÀ registrato nel TypeBuilder: ogni
        // etichetta definita e mai marcata a cui si è saltati farebbe fallire CreateType/Save con
        // "Label N has not been marked" — e con essa l'INTERA assembly. Tenendo traccia di tutte le
        // etichette create, il catch di Emit() può marcare le residue alla posizione corrente e
        // chiudere con un THROW esplicito: l'assembly si salva e, se qualcuno invoca il metodo, il
        // fallimento è rumoroso invece di eseguire un corpo monco.
        private readonly List<Label> _labelsDefined = new();
        private readonly HashSet<Label> _labelsMarked = new();

        private Label DefineLabel()
        {
            var l = _il.DefineLabel();
            _labelsDefined.Add(l);
            return l;
        }

        /// <summary>MarkLabel idempotente (una seconda marcatura della stessa etichetta, che
        /// ILGenerator rifiuta, è un no-op).</summary>
        private void MarkLabel(Label l)
        {
            if (_labelsMarked.Add(l)) _il.MarkLabel(l);
        }

        /// <summary>Marca alla posizione corrente tutte le etichette definite e non ancora
        /// marcate (percorso di recupero dopo un'eccezione di emissione).</summary>
        private void MarkRemainingLabels()
        {
            foreach (var l in _labelsDefined) MarkLabel(l);
        }

        public void Emit(string methodName)
        {
            _labelsDefined.Clear();
            _labelsMarked.Clear();
            _storeTemp = null; // ogni metodo ha i propri locali (l'alias StepV2_Main riemette)
            _feValueLocal = null;
            _feOkLocal = null;
            MethodBuilder mb;
            if (_closureType is not null)
            {
                // Procedura locale con cattura: StepV2 come metodo D'ISTANZA della classe
                // __ClosureCtx — this = __context (il driver lo lega nel delegato, G4).
                mb = _closureType.DefineMethod(
                    methodName,
                    MethodAttributes.Public,
                    typeof(IlStepTransition),
                    new[] { typeof(PCodeTask), typeof(IlActivationFrame) });
            }
            else
            {
                mb = _programType.DefineMethod(
                    methodName,
                    MethodAttributes.Public | MethodAttributes.Static,
                    typeof(IlStepTransition),
                    new[] { typeof(PCodeTask), typeof(IlActivationFrame) });
            }
            _il = mb.GetILGenerator();

            // La transizione di ritorno vive in un local: ogni block la memorizza e fa leave
            // all'uscita comune — necessario per avvolgere i blocchi in regione protetta (TRY)
            // in un CIL try/catch (un `ret` dentro una protected region è IL invalido).
            _transitionLocal = _il.DeclareLocal(typeof(IlStepTransition));
            _exitLabel = DefineLabel();

            // Assicura i locali del frame (i frame root da SpawnGoSub possono essere piccoli).
            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldc_I4, _localCount);
            _il.Emit(OpCodes.Call, EnsureFrameLocalsMethod);

            // switch su frame.Pc → label dei block. ATTENZIONE: Switch indicizza per VALORE
            // del pc (frame.Pc) sull'array — quindi labels[pc] = label del block con quel pc.
            // I pc IR possono NON essere contigui (gap tra costrutti): l'array è dimensionato
            // a maxPc+1 con i buchi → invalid (quei pc non capitano mai).
            var invalid = DefineLabel();
            var blocks = _ir.Blocks;
            var maxPc = 0;
            foreach (var b in blocks) if (b.Pc > maxPc) maxPc = b.Pc;
            var labels = new Label[maxPc + 1];
            Array.Fill(labels, invalid);
            foreach (var b in blocks) labels[b.Pc] = DefineLabel();

            EmitLdarg(_frameArg);
            _il.Emit(OpCodes.Ldfld, FramePcField);
            _il.Emit(OpCodes.Switch, labels);
            _il.Emit(OpCodes.Br, invalid);

            try
            {
                foreach (var b in blocks)
                {
                    MarkLabel(labels[b.Pc]);
                    EmitBlock(b);
                }
            }
            catch
            {
                // Un fallimento a metà emissione lascia il metodo GIÀ registrato nel TypeBuilder:
                // le etichette dei block non emessi resterebbero non marcate e romperebbero il
                // Save() dell'intera assembly (l'eccezione arriva FUORI dal try/catch del
                // chiamante). Si marcano le etichette residue e si chiude con un THROW esplicito
                // (mai un corpo silenziosamente "completato"): se invocato, il percorso fallisce
                // forte invece di restituire un risultato sbagliato.
                // Le etichette rimaste non marcate NON sono solo quelle dei block non emessi:
                // `invalid` (default dello switch), `_exitLabel` e ogni etichetta interna del blocco
                // che stava fallendo. Il tracciamento DefineLabel/MarkLabel le copre tutte.
                MarkRemainingLabels();
                _il.Emit(OpCodes.Ldstr, "StepV2 non emesso (errore di codegen): percorso GPM v2 non disponibile");
                _il.Emit(OpCodes.Newobj,
                    typeof(InvalidOperationException).GetConstructor([typeof(string)])!);
                _il.Emit(OpCodes.Throw);
                throw;
            }

            MarkLabel(invalid);
            EmitTransition(IlStepKind.CompleteTask, resumePc: 0, targetProc: 0, args: null, dest: IlReturnDestination.Discard, value: null);
            EmitReturnTransition();

            // Uscita comune del metodo: la transizione (già memorizzata) viene ritornata.
            MarkLabel(_exitLabel);
            _il.Emit(OpCodes.Ldloc, _transitionLocal);
            _il.Emit(OpCodes.Ret);
        }

        /// <summary>Memorizza la transizione in cima allo stack nel local e fa leave all'uscita
        /// comune. `Leave` è valido sia fuori che dentro una protected region (TRY).</summary>
        private void EmitReturnTransition()
        {
            _il.Emit(OpCodes.Stloc, _transitionLocal!);
            _il.Emit(OpCodes.Leave, _exitLabel);
        }

        // ─── Emitter dei blocchi ───────────────────────────────────────────────────────────
}
