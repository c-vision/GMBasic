// FILE: Basic.IlBackend.PCode.PCodeProcessor.Hot.cs
// P-CODE VM — percorso CALDO del dispatch (2026-11-06).
//
// PERCHE': `DispatchOpcode` era un unico `switch` da ~70 KB di IL (oltre 200 opcode). RyuJIT compila in
// MinOpts (nessuna allocazione di registri, nessun inlining, nessun Tier-1) i metodi sopra ~60.000 byte di IL
// ("Tier-0 switched MinOpts" nel log del JIT): ogni istruzione dell'interprete girava non ottimizzata
// (~0,4 µs per istruzione non fusa). Qui stanno gli opcode CALDI — stack, aritmetica, confronti, salti,
// variabili locali/globali, chiamate/ritorno, FOR, elementi di array, builtin — SPOSTATI (non duplicati)
// dal metodo gigante, che resta come percorso freddo (`DispatchOpcodeCold`). Questo metodo deve restare
// PICCOLO: se cresce oltre le soglie del JIT il guadagno sparisce (verificare con
// `DOTNET_JitDisasmSummary=1`: deve comparire "Tier1", non "switched MinOpts").
//
// Contratto: `handled=false` => l'opcode non e' caldo, il chiamante usa `DispatchOpcodeCold`. I corpi sono
// IDENTICI a quelli che c'erano nel metodo originale (stessi `return true/false`).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Basic.Ledger;
using Compiler;
using System.Globalization;
using System.Linq;
using System.Net;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>Dispatch degli opcode caldi; vedi l'intestazione del file.</summary>
    private bool DispatchHot(PCodeTask task, OpCode opcode, out bool handled)
    {
        handled = true;
        switch (opcode)
        {
            // ---- righe originali 1070-1333 ----
            case OpCode.OP_PUSH_CONST:
            {
                var idx = ReadU16(task);
                task.PushRef(idx >= 0 && idx < _constants.Count ? _constants[idx] : null);
                return true;
            }
            case OpCode.OP_PUSH_NIL:
                task.PushRef(null);
                return true;
            case OpCode.OP_PUSH_TRUE:
                task.PushBool(true);
                return true;
            case OpCode.OP_PUSH_FALSE:
                task.PushBool(false);
                return true;
            case OpCode.OP_POP:
                if (task.Sp > 0) task.Sp--;
                return true;
            case OpCode.OP_DUP:
                if (task.Sp > 0) { var v = task.PopRef(); task.PushRef(v); task.PushRef(v); }
                return true;

            // === Arithmetic ===
            case OpCode.OP_ADD:
            {
                var b = task.PopRef(); var a = task.PopRef();
                // DateValue + number = date arithmetic, DateValue + Time = DateTime
                if (a is DateValue dva && b is DateValue dvb) {
                    // Both are dates: add time to date → DateTime (OADate)
                    task.PushRef(new DateValue(dva.OADate + dvb.OADate));
                } else if (a is DateValue dva2 && b is double db2) {
                    // Date + number (days) = new date
                    task.PushRef(new DateValue(dva2.OADate + db2));
                } else if (a is double da2 && b is DateValue dvb2) {
                    // number + date = new date
                    task.PushRef(new DateValue(da2 + dvb2.OADate));
                } else if (a is string || b is string)
                    // Formattazione coerente con IL e VM/C: G15 in cultura INVARIANTE per i
                    // numeri (ToString() userebbe la cultura corrente → virgola) e "[...]" per
                    // le collezioni (es. tupla annidata → "[1, 2]", non "System.Object[]").
                    task.PushRef(ConcatFormat(a) + ConcatFormat(b));
                else
                {
                    // Coercizione UNICA (NumOf/PopNum): la copia inline usata qui perdeva il
                    // BIT — `TRUE + 1` dava 1 (TryParse("True") → 0) mentre `TRUE * 3` dava
                    // -3 (PopNum), quindi `TRUE + 1` divergeva da IL/VM-C/C99Native (=0).
                    task.PushNum(NumOf(a) + NumOf(b));
                }
                return true;
            }
            case OpCode.OP_SUBTRACT:
            {
                var b = PopNum(task); var a = PopNum(task);
                task.PushNum(a - b);
                return true;
            }
            case OpCode.OP_MULTIPLY:
            {
                var b = PopNum(task); var a = PopNum(task);
                task.PushNum(a * b);
                return true;
            }
            case OpCode.OP_XOR:
            {
                var b = PopNum(task); var a = PopNum(task);
                task.PushNum((double)((long)a ^ (long)b));
                return true;
            }
            case OpCode.OP_BITAND:
            {
                var b = PopNum(task); var a = PopNum(task);
                task.PushNum((double)((long)a & (long)b));
                return true;
            }
            case OpCode.OP_BITOR:
            {
                var b = PopNum(task); var a = PopNum(task);
                task.PushNum((double)((long)a | (long)b));
                return true;
            }
            case OpCode.OP_DIVIDE:
            {
                var b = PopNum(task); var a = PopNum(task);
                // Divisione per zero → ERR 11 "Division by zero" (canone BASIC, identico alle
                // 4 gambe): schema a 3 livelli di RaiseBasicError (TRY/CATCH → ON ERROR →
                // errore fatale). Prima: `a / b` nudo su double dava +Infinity senza errore,
                // mentre IL/C99/VM-C sollevavano ERR 11 → quattro esiti diversi per la stessa
                // espressione (Bugs/(solved) - Divisione-Per-Zero-Divergente-4-Gambe).
                if (b == 0) { if (RaiseBasicError(task, 11.0, "Division by zero")) task.PushNum(0); return true; }
                task.PushNum(a / b);
                return true;
            }
            case OpCode.OP_NEGATE:
            {
                var a = PopNum(task);
                task.PushNum(-a);
                return true;
            }
            case OpCode.OP_MOD:
            {
                var b = PopNum(task); var a = PopNum(task);
                if (b == 0) { if (RaiseBasicError(task, 11.0, "Division by zero")) task.PushNum(0); return true; }
                task.PushNum(a % b);
                return true;
            }
            case OpCode.OP_INT_DIVIDE:
            {
                var b = PopNum(task); var a = PopNum(task);
                // Divisione INTERA (`\`): stesso ERR 11 su divisore 0 (prima Truncate(Inf) = Inf
                // e/o overflow in conversione, senza errore coerente fra le gambe).
                if (b == 0) { if (RaiseBasicError(task, 11.0, "Division by zero")) task.PushNum(0); return true; }
                task.PushNum(Math.Truncate(a / b));
                return true;
            }
            case OpCode.OP_POWER:
            {
                var b = PopNum(task); var a = PopNum(task);
                task.PushNum(Math.Pow(a, b));
                return true;
            }
            // === Logical ===
            case OpCode.OP_NOT:
            {
                var a = PopNum(task);
                task.PushNum((double)(~(long)a));
                return true;
            }

            // === Comparisons ===
            case OpCode.OP_CMP_EQ:
            {
                var b = task.Sp > 0 ? task.PopRef() : null;
                var a = task.Sp > 0 ? task.PopRef() : null;
                if (a is string sa && b is string sb)
                    task.PushNum(sa == sb ? -1.0 : 0.0);
                else if (a is System.Collections.IDictionary || b is System.Collections.IDictionary)
                    task.PushNum(UdtValueEquals(a, b) ? -1.0 : 0.0);
                // Handle GPM vs 0/NIL: riferimento non null → NON uguale a 0.
                else if (IsGpmHandleForCompare(a) && IsNumericZeroForCompare(b)
                         || IsGpmHandleForCompare(b) && IsNumericZeroForCompare(a))
                    task.PushNum(0.0);
                // NULL/Nothing va confrontato per identita' (null=null vero, null=qualunque altra
                // cosa falso), MAI lasciato cadere nel fallback numerico sotto: quel fallback
                // converte un operando non numerico/non parsabile a 0 (TryParse fallito = 0), quindi
                // "token$ = ""password_segreta""" con token$ REALMENTE null (non stringa vuota — es.
                // un header HTTP assente, dove Headers.Get(...) restituisce null, non "") risultava
                // SEMPRE vero: null->0 e "password_segreta" (non numerica)->0 anch'essa, 0=0. Bug
                // reale, di sicurezza: un middleware di autenticazione con `IF token$ = "secret"
                // THEN ... ELSE Nega accesso` autorizzava SEMPRE, indipendentemente dal token,
                // quando il token era assente (il caso comune di un client che non lo manda affatto).
                // Porting da GMBasicAI (commit 7736047).
                //
                // ECCEZIONE aggiunta (bug trovato dal vivo su BSP, SESSION("chiave-mai-scritta")):
                // null confronta uguale anche a "" (stringa vuota), non solo a null. Convenzione
                // pervasiva in questo dialetto: un campo SESSION/JSON/dict mai impostato si legge
                // come null (OP_LOAD_ELEMENT su chiave assente), ma il codice lo confronta sempre
                // con "" per testare "non impostato" (lo stesso BspCompiler genera
                // `PAGE("Title") = ""` come default nel preambolo). Con la sola regola null==null,
                // `IF SESSION("hits") = "" THEN` era SEMPRE falso al primo hit (mai inizializzato),
                // saltando ogni logica "if unset" nelle pagine BSP. La sicurezza del fix originale
                // resta intatta: null NON combacia con un valore stringa NON vuoto (es.
                // "password_segreta") — solo con "" specificamente, un'aggiunta molto più stretta.
                else if (a is null || b is null)
                    task.PushNum(NullEqualsForCompare(a, b) ? -1.0 : 0.0);
                else
                {
                    // Coercizione UNICA (NumOf, la stessa di PopNum/aritmetica): gestisce
                    // long/int/float/decimal/date oltre a double/bool/stringa-numerica — con una
                    // copia separata che li perdeva, due LONG (come i campi record ISAM)
                    // confrontavano sempre uguali (entrambi -> 0).
                    var da = NumOf(a);
                    var db = NumOf(b);
                    task.PushNum(da == db ? -1.0 : 0.0);
                }
                return true;
            }
            case OpCode.OP_CMP_NE:
            {
                var b = task.Sp > 0 ? task.PopRef() : null;
                var a = task.Sp > 0 ? task.PopRef() : null;
                if (a is string sa && b is string sb)
                    task.PushNum(sa != sb ? -1.0 : 0.0);
                else if (a is System.Collections.IDictionary || b is System.Collections.IDictionary)
                    task.PushNum(!UdtValueEquals(a, b) ? -1.0 : 0.0);
                // Handle GPM vs 0/NIL: riferimento non null → diverso da 0.
                else if (IsGpmHandleForCompare(a) && IsNumericZeroForCompare(b)
                         || IsGpmHandleForCompare(b) && IsNumericZeroForCompare(a))
                    task.PushNum(-1.0);
                // Vedi il commento in OP_CMP_EQ: stessa correzione, negata.
                else if (a is null || b is null)
                    task.PushNum(NullEqualsForCompare(a, b) ? 0.0 : -1.0);
                else
                {
                    // Vedi commento in OP_CMP_EQ: coercizione unica (NumOf).
                    var da = NumOf(a);
                    var db = NumOf(b);
                    task.PushNum(da != db ? -1.0 : 0.0);
                }
                return true;
            }
            // GT/GTE/LT/LTE: come OP_CMP_EQ/NE, due STRINGHE si confrontano per ordinale, non
            // via PopNum — PopNum su una stringa non numerica ("Gamma") ritorna 0.0, quindi
            // PRIMA la ricerca ordinale confrontava sempre 0 con 0 (sempre uguali/false) per
            // qualunque coppia di stringhe non numeriche, es. un SET FILTER `Name > 'B'` non
            // filtrava mai nulla. Stesso fallback numerico di OP_CMP_EQ/NE: NumOf, coercizione
            // unica (long/int/float/decimal/date/bool), non solo double.
            case OpCode.OP_CMP_GT:
            {
                var b = task.Sp > 0 ? task.PopRef() : null; var a = task.Sp > 0 ? task.PopRef() : null;
                if (a is string sa && b is string sb) task.PushNum(string.CompareOrdinal(sa, sb) > 0 ? -1.0 : 0.0);
                else task.PushNum(NumOf(a) > NumOf(b) ? -1.0 : 0.0);
                return true;
            }
            case OpCode.OP_CMP_GTE:
            {
                var b = task.Sp > 0 ? task.PopRef() : null; var a = task.Sp > 0 ? task.PopRef() : null;
                if (a is string sa && b is string sb) task.PushNum(string.CompareOrdinal(sa, sb) >= 0 ? -1.0 : 0.0);
                else task.PushNum(NumOf(a) >= NumOf(b) ? -1.0 : 0.0);
                return true;
            }
            case OpCode.OP_CMP_LT:
            {
                var b = task.Sp > 0 ? task.PopRef() : null; var a = task.Sp > 0 ? task.PopRef() : null;
                if (a is string sa && b is string sb) task.PushNum(string.CompareOrdinal(sa, sb) < 0 ? -1.0 : 0.0);
                else task.PushNum(NumOf(a) < NumOf(b) ? -1.0 : 0.0);
                return true;
            }
            case OpCode.OP_CMP_LTE:
            {
                var b = task.Sp > 0 ? task.PopRef() : null; var a = task.Sp > 0 ? task.PopRef() : null;
                if (a is string sa && b is string sb) task.PushNum(string.CompareOrdinal(sa, sb) <= 0 ? -1.0 : 0.0);
                else task.PushNum(NumOf(a) <= NumOf(b) ? -1.0 : 0.0);
                return true;
            }

            // === Flow Control ===
            case OpCode.OP_JUMP_FORWARD:
            {
                var offset = ReadU16(task);
                task.Ip += offset;
                return true;
            }
            case OpCode.OP_JUMP_BACKWARD:
            {
                var offset = ReadU16(task);
                task.Ip -= offset;
                return true;
            }
            case OpCode.OP_JUMP_IF_FALSE:
            {
                var offset = ReadU16(task);
                var rawCond = (task.Sp > 0) ? task.Stack[task.Sp - 1] : (StackEntry?)null;
                var cond = PopNum(task);
                if (!PCodeTruthy(rawCond, cond)) task.Ip += offset;
                return true;
            }
            case OpCode.OP_JUMP_IF_TRUE:
            {
                var offset = ReadU16(task);
                var rawCond = (task.Sp > 0) ? task.Stack[task.Sp - 1] : (StackEntry?)null;
                var cond = PopNum(task);
                if (PCodeTruthy(rawCond, cond)) task.Ip += offset;
                return true;
            }
            // ---- righe originali 1334-1427 ----
            case OpCode.OP_CLEAR_RETURN:
            {
                if (task.Frame != null)
                    task.Frame.ReturnValue = null;
                return true;
            }
            case OpCode.OP_RETURN:
            {
                if (task.GosubContinuations.TryPeek(out var gosub) &&
                    ReferenceEquals(gosub.Frame, task.Frame))
                {
                    if (BeginGosubReturnUnwind(task, gosub))
                        return true;
                    return !task.IsFinished;
                }
                if (BeginReturnUnwind(task, task.CurrentInstructionIp))
                    return true;
                // ON ERROR GOSUB usa IpStack per la continuazione, mentre i normali GOSUB
                // usano PCodeCallFrame. Il RETURN dell'handler deve quindi consumare prima
                // questa continuazione senza smontare la procedura che ha installato l'handler.
                if (task.ErrorHandlerIsGosub && task.IpStack is { Count: > 0 })
                {
                    task.Ip = task.IpStack.Pop();
                }
                else if (task.Frame != null)
                {
                    // Prefer stack value (from RETURN expr) over stale ReturnValue.
                    object? rv = null;
                    if (task.Frame.FrameSp > 0) rv = task.Frame.PopFrameValue();
                    else if (!task.Frame.IsEventDispatch && ((task.Frame.ConstructorInstance != null || task.Frame.IsDirectCall)
                        ? task.Sp > task.Frame.SavedStackPointer
                        : task.Sp > 0))
                    {
                        // Il ramo "ConstructorInstance != null" e' esclusivo delle chiamate a
                        // costruttore (OP_NEW_CLASS_INSTANCE/OP_NEW_UDT_INSTANCE — l'unico punto
                        // che valorizza questo campo). Solo li' il controllo deve essere RELATIVO
                        // a SavedStackPointer, non assoluto: quel sito calcola SavedStackPointer
                        // DOPO aver gia' fatto pop di numArgs/args, quindi rappresenta lo stack del
                        // chiamante PRIMA della chiamata — se lo stack condiviso ha contenuto
                        // residuo preesistente li' sotto (osservato: task.Sp == 1 fin dalla
                        // primissima istruzione eseguita nel programma), un controllo assoluto
                        // POP-a quel residuo invece di usare il fallback ConstructorInstance,
                        // restituendo garbage/null al posto dell'istanza appena costruita — bug
                        // scoperto con `NEW Box<DOUBLE>(...)` chiamato a livello top-level (BUG
                        // 8-10, CLASS generiche). Il controllo ASSOLUTO resta invariato per tutti
                        // gli altri chiamanti (es. CallClosureSync, che cattura il proprio
                        // SavedStackPointer PRIMA di pop-are gli argomenti nei locals — li'
                        // "niente di nuovo pushato" puo' comunque lasciare legittimamente
                        // task.Sp > SavedStackPointer, quindi un confronto relativo romperebbe
                        // il valore di ritorno genuino della closure).
                        rv = task.PopRef();
                    }
                    else rv = task.Frame.ReturnValue ?? task.Frame.ConstructorInstance;
                    // If the return value is a ClosureData whose first upvalue is a Dict (context),
                    // return the Dict instead (so CALL_INDIRECT receives the context as fnPtr).
                    if (rv is ClosureData rvCd && rvCd.GetUpvalue(0) is System.Collections.IDictionary)
                        rv = rvCd.GetUpvalue(0);
                    var savedSp = task.Frame.SavedStackPointer;
                    var returningFrame = task.Frame;
                    task.Ip = task.Frame.ReturnIp;
                    task.Frame = task.Frame.Previous;
                    task.Sp = savedSp;
                    RestoreCallerErrorScope(task, returningFrame);
                    while (task.TryStack.Count > 0 && ReferenceEquals(task.TryStack.Peek().Frame, returningFrame))
                        task.TryStack.Pop();
                    while (task.CleanupStack.Count > 0 && ReferenceEquals(task.CleanupStack.Peek().Frame, returningFrame))
                        task.CleanupStack.Pop();
                    task.PushRef(rv);
                    // RAISE EVENT con più handler: il RETURN di un handler passa al successivo
                    // (chiamata annidata invece di tornare al main) finché la coda non finisce.
                    if (_pendingEventHandlers != null && ++_pendingEventIndex < _pendingEventHandlers.Length)
                    {
                        DispatchEventHandler(task, _pendingEventHandlers[_pendingEventIndex].Handler,
                            _pendingEventHandlers[_pendingEventIndex].Args);
                        return true;
                    }
                    _pendingEventHandlers = null;
                }
                else if (task.IpStack != null && task.IpStack.Count > 0)
                {
                    task.Ip = task.IpStack.Pop();
                }
                else
                {
                    // Parità Go defer: prima di completare la procedura, esegui i body DEFER
                    // registrati (LIFO). Se ce ne sono, il task entra in modalità defer e
                    // torna qui solo dopo l'ultimo OP_DEFER_RET.
                    if (BeginDeferredExecution(task))
                        return true;
                    task.State = PCodeTaskState.Finished;
                    return false;
                }
                return true;
            }
            // ---- righe originali 1526-1551 ----
            case OpCode.OP_CALL:
            {
                var callIp = task.Ip;
                var addr = ReadU16(task);
                var island = DispatchIsland(task, addr, isCallN: false, task.Sp);
                if (island != null) { if (!island.Value) task.Ip = callIp - 1; return island.Value; }
                var newFrame = new PCodeCallFrame { SavedStackPointer = task.Sp,  ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = CreateLocals(addr) };
                EnterProcedureErrorScope(task, newFrame);
                task.Frame = newFrame;
                task.Ip = addr;
                return true;
            }
            case OpCode.OP_CALL_N:
            {
                var callIp = task.Ip;
                var addr = ReadU16(task);
                var argCount = ReadU8(task);
                var savedSp = task.Sp - argCount;
                var island = DispatchIsland(task, addr, isCallN: true, savedSp);
                if (island != null) { if (!island.Value) task.Ip = callIp - 1; return island.Value; }
                var newFrame = new PCodeCallFrame { SavedStackPointer = savedSp, ReturnIp = task.Ip, Previous = task.Frame, LocalVariables = CreateLocals(addr), ReturnValue = null };
                EnterProcedureErrorScope(task, newFrame);
                task.Frame = newFrame;
                task.Ip = addr;
                return true;
            }
            // ---- righe originali 1748-1811 ----
            case OpCode.OP_LOAD_GLOBAL:
            {
                var idx = ReadU16(task);
                var gv = task.GetGlobal(idx);
                var resolvedGv = gv is ByRefBox box ? box.Value : gv;
                // Leggere una variabile che contiene ancora un GO FUNCTION future non risolto
                // blocca chi legge (implicit await, come Gpm.Expressions.cs lato AST) — non
                // serve un opcode/statement AWAIT dedicato, la lettura stessa sospende.
                if (resolvedGv is PCodeFutureValue futG)
                {
                    // RegisterWaiter è l'UNICA fonte di verità: fa "se non risolto, registrati"
                    // in un solo lock hold (vedi PCodeFutureValue), impostando anche
                    // task.State=Blocked atomicamente. Il rewind di Ip va fatto PRIMA di
                    // chiamarlo (non dopo): appena il task è registrato diventa visibile a un
                    // eventuale Resolve/Fail concorrente, che potrebbe farlo ripartire (via
                    // WakeTask) PRIMA che questo stesso thread abbia finito di sistemare Ip — un
                    // retry partirebbe dal punto sbagliato (bug reale, causa di un hang
                    // intermittente osservato sotto vero parallelismo in GoRoutinesWorkers.bas).
                    task.Ip -= 3; // opcode(1) + idx(2): ri-esegue OP_LOAD_GLOBAL al risveglio
                    if (futG.RegisterWaiter(task))
                    {
                        return false;
                    }
                    task.Ip += 3; // già risolto: annulla il rewind
                    if (futG.FailureException != null) { PropagateFutureFailure(task, futG); return true; }
                    task.PushRef(futG.Result);
                    return true;
                }
                task.PushRef(resolvedGv);
                return true;
            }
            case OpCode.OP_STORE_GLOBAL:
            {
                var idx = ReadU16(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                task.EnsureGlobals(idx + 1);
                if (task.Globals![idx] is ByRefBox box) box.Value = val;
                else task.Globals[idx] = val;
                return true;
            }

            // === VARIABLES (locals) ===
            case OpCode.OP_LOAD_LOCAL:
            {
                var slot = ReadU16(task);
                var locals = (task.Frame?.LocalVariables is { Length: > 0 } fLocals) ? fLocals : task.GetLocals();
                var lv = slot < locals.Length ? locals[slot] : null;
                var resolvedLv = lv is ByRefBox box ? box.Value : lv;
                if (resolvedLv is PCodeFutureValue futL)
                {
                    // Stesso motivo di OP_LOAD_GLOBAL: rewind PRIMA di RegisterWaiter, non dopo.
                    task.Ip -= 3; // opcode(1) + slot(2): ri-esegue OP_LOAD_LOCAL al risveglio (slot u16 da 671f06c3)
                    if (futL.RegisterWaiter(task))
                    {
                        return false;
                    }
                    task.Ip += 3; // già risolto: annulla il rewind
                    if (futL.FailureException != null) { PropagateFutureFailure(task, futL); return true; }
                    task.PushRef(futL.Result);
                    return true;
                }
                task.PushRef(resolvedLv);
                return true;
            }
            // ---- righe originali 1861-1889 ----
            case OpCode.OP_STORE_LOCAL:
            {
                var slot = ReadU16(task);
                var val = task.Sp > 0 ? task.PopRef() : null;
                var locals = (task.Frame?.LocalVariables is { Length: > 0 } fLocals) ? fLocals : task.GetLocals();
                if (slot < locals.Length)
                {
                    if (locals[slot] is ByRefBox box) box.Value = val;
                    else if (locals[slot] is RefToVar rvSL) rvSL.Array[rvSL.Index] = val;
                    else locals[slot] = val;
                    if (task.Frame != null) task.Frame.ReturnValue = val;
                }
                return true;
            }
            case OpCode.OP_NATIVE_BUILTIN_ASSIGN:
            {
                // Fusion nativo: `x = f(y)` — replica la semantica della VM/C
                // (gmb_vm_core.c, case OP_NATIVE_BUILTIN_ASSIGN). Formato:
                //   [op] [src_slot u8] [dst_slot u8] [f_code u16]
                var srcSlot = ReadU16(task);
                var dstSlot = ReadU16(task);
                var fCode = ReadU16(task);
                var locals = task.Frame?.LocalVariables;
                if (locals == null || locals.Length == 0) locals = task.GetLocals();
                double src = srcSlot < locals.Length && locals[srcSlot] is double srcVal ? srcVal : 0;
                if (dstSlot < locals.Length) locals[dstSlot] = NativeLoopMath(fCode, src);
                return true;
            }

            // ---- righe originali 1909-2031 ----
            case OpCode.OP_FOR_PREPARE:
            {
                var slot = ReadU16(task);
                var step = PopNum(task);
                var end = PopNum(task);
                var start = PopNum(task);
                ForStateOf(task)[slot] = (start, end, step);
                var locals = task.Frame?.LocalVariables;
                if (locals == null || locals.Length == 0) locals = task.GetLocals();
                if (slot < locals.Length) locals[slot] = start;
                // Spinge 1.0 se il corpo deve eseguire (start <= end per step positivo)
                var shouldExecute = (step >= 0) ? start <= end : start >= end;
                task.PushNum(shouldExecute ? 1.0 : 0.0);
                return true;
            }
            case OpCode.OP_FOR_LOOP:
            {
                var slot = ReadU16(task);
                var jumpOffset = ReadU16(task);
                if (ForStateOf(task).TryGetValue(slot, out var state))
                {
                    // Read actual counter from local (may have been modified in loop body)
                    var locals = task.Frame?.LocalVariables;
                    if (locals == null || locals.Length == 0) locals = task.GetLocals();
                    var actualCounter = slot < locals.Length && locals[slot] is double cd ? cd : state.Counter;
                    var newCounter = actualCounter + state.Step;
                    var done = (state.Step >= 0) ? newCounter > state.End : newCounter < state.End;
                    // Store the new counter in locals BEFORE check, so the exit value is available
                    ForStateOf(task)[slot] = (newCounter, state.End, state.Step);
                    if (slot < locals.Length) locals[slot] = newCounter;
                    if (!done)
                    {
                        task.Ip -= jumpOffset; // Salta indietro
                    }
                    else
                    {
                        ForStateOf(task).Remove(slot);
                    }
                }
                return true;
            }
            case OpCode.OP_NATIVE_LOOP_ACC:
            {
                // Fusion nativo: accumulatore puro in loop — replica la semantica della VM/C
                // (gmb_vm_core.c, case OP_NATIVE_LOOP_ACC): esegue il corpo del FOR interamente
                // qui, niente dispatch per iterazione. Formato (11 byte):
                //   [op] [acc_slot u8] [ctr_slot u8] [acc_op u8] [term_mode u8]
                //   [f_code u16] [f_code2 u16] [m_idx u16]
                // acc_op: 0=+=, 1=-=, 2=*=, 3=/=. term_mode: 0=f(I)*m, 1=f(I), 2=I*I, 3=I*m,
                // 4=I, 5=f(I)+g(I). Range [start,end,step] già in ForState (OP_FOR_PREPARE).
                const int nativeLoopSliceIterations = 1_024;
                var resumeIp = task.Ip - 1;
                var accSlot = ReadU16(task);
                var ctrSlot = ReadU16(task);
                var accOp = ReadU8(task);
                var termMode = ReadU8(task);
                var fCode = ReadU16(task);
                var fCode2 = ReadU16(task);
                var mIdx = ReadU16(task);
                if (ForStateOf(task).TryGetValue(ctrSlot, out var nativeState))
                {
                    var locals = task.Frame?.LocalVariables;
                    if (locals == null || locals.Length == 0) locals = task.GetLocals();
                    double acc = accSlot < locals.Length && locals[accSlot] is double accVal ? accVal : 0;
                    var step = nativeState.Step;
                    var end = nativeState.End;
                    var i = ctrSlot < locals.Length && locals[ctrSlot] is double ctrVal ? ctrVal : nativeState.Counter;
                    double m = 1.0;
                    if (mIdx < _constants.Count)
                    {
                        if (_constants[mIdx] is double md) m = md;
                        else if (_constants[mIdx] is int mi) m = mi;
                        else if (_constants[mIdx] is long ml) m = ml;
                    }
                    var iterations = 0;
                    for (; ; )
                    {
                        double term;
                        switch (termMode)
                        {
                            case 0:
                            case 1:
                            {
                                var fv = NativeLoopMath(fCode, i);
                                term = termMode == 0 ? fv * m : fv;
                                break;
                            }
                            case 2: term = i * i; break;
                            case 3: term = i * m; break;
                            case 5: term = NativeLoopMath(fCode, i) + NativeLoopMath(fCode2, i); break;
                            default: term = i; break;
                        }
                        switch (accOp)
                        {
                            case 0: acc += term; break;
                            case 1: acc -= term; break;
                            case 2: acc *= term; break;
                            case 3: acc = term != 0.0 ? acc / term : 0.0; break;
                        }
                        var nxt = i + step;
                        var done = step >= 0 ? nxt > end : nxt < end;
                        i = nxt;
                        iterations++;
                        if (done) break;
                        if (iterations >= nativeLoopSliceIterations)
                        {
                            // Un opcode fuso resta un singolo dispatch, quindi il quantum esterno
                            // non può preemptarlo. Salviamo lo stato nei normali slot/FOR state e
                            // rieseguiamo lo stesso opcode nella slice successiva.
                            ForStateOf(task)[ctrSlot] = (i, end, step);
                            if (accSlot < locals.Length) locals[accSlot] = acc;
                            if (ctrSlot < locals.Length) locals[ctrSlot] = i;
                            task.Ip = resumeIp;
                            task.YieldRequested = true;
                            return true;
                        }
                    }
                    ForStateOf(task).Remove(ctrSlot);
                    if (accSlot < locals.Length) locals[accSlot] = acc;
                    if (ctrSlot < locals.Length) locals[ctrSlot] = i;
                }
                return true;
            }
            // ---- righe originali 2124-2218 ----
            case OpCode.OP_LOAD_ELEMENT:
            // OP_LOAD_ELEMENT_REF (AssignField, Arr(i).Campo = valore): un Dictionary<string,
            // object?> (istanza CLASS/UDT) non viene mai copiato da un semplice indicizzatore
            // di array in .NET — identico a OP_LOAD_ELEMENT qui. La distinzione esiste solo
            // per il backend VM in C (native/gmb_vm_c), che senza un opcode separato
            // copierebbe l'elemento invece di restituirne il riferimento — vedi OpCode.cs.
            case OpCode.OP_LOAD_ELEMENT_REF:
            {
                var numIndices = ReadU8(task);
                // PERCORSO RAPIDO (2026-11-06): un solo indice numerico su un array 1D noto, in limiti: niente
                // array di chiavi, niente LINQ, niente GetValue(int[]). Fuori limiti/altri tipi: percorso generale.
                if (numIndices == 1 && task.Sp >= 2 && TryFastElementLoad(task)) return true;
                // implicit-await inline: la base è sotto gli indici (es. FETCH(url).Headers["x"])
                if (AwaitInlineFutureAt(task, 2, task.Sp - numIndices)) return false;
                var keys = new object?[numIndices];
                for (var i = numIndices - 1; i >= 0; i--)
                    keys[i] = task.Sp > 0 ? task.PopRef() : null;
                var arr = task.Sp > 0 ? task.PopRef() : null;
                if (arr is RefToVar rvL) arr = rvL.Array[rvL.Index];
                // Stage C: valore matrice = descrittore CommonLib.MatrixValue (risultati
                // MATRIX.*). Due indici → elemento (Get, 0 fuori range come il runtime IL);
                // un indice su una matrice → RIGA come double[] (FOR EACH).
                if (arr is CommonLib.MatrixValue mvL)
                {
                    if (numIndices >= 2)
                        task.PushRef(mvL.Get(Convert.ToInt32(keys[0] ?? 0), Convert.ToInt32(keys[1] ?? 0)));
                    else if (numIndices == 1)
                    {
                        var rowIdxL = Convert.ToInt32(keys[0] ?? 0);
                        var rowL = new double[mvL.Cols];
                        for (var cL = 0; cL < mvL.Cols; cL++) rowL[cL] = mvL.Get(rowIdxL, cL);
                        task.PushRef(rowL);
                    }
                    else task.PushRef(null);
                    return true;
                }
                if (arr is Array array && numIndices > 0)
                {
                    var idx = keys.Select(k => Convert.ToInt32(k ?? 0)).ToArray();
                    try {
                        var elem = array.GetValue(idx);
                        task.PushRef(elem);
                    }
                    catch (Exception ex) when (ex is IndexOutOfRangeException or ArgumentOutOfRangeException)
                    {
                        // Indice fuori dai limiti di un array dimensionato: ERR 9, non un
                        // silenzioso null (parità IL; VM/C e C99Native fanno lo stesso).
                        if (RaiseBasicError(task, 9.0, string.Empty)) task.PushNum(0);
                    }
                    catch { task.PushRef(null); }
                }
                else if (arr is System.Collections.IDictionary dict && numIndices == 1)
                {
                    // Sempre chiave REALE (mai indice posizionale) — simmetrico a OP_STORE_ELEMENT,
                    // che ha sempre trattato la chiave come reale. Prima di questo fix, una chiave
                    // numerica veniva SEMPRE interpretata come indice di iterazione (per supportare
                    // FOR EACH), rendendo un vero DICT<LONG,V> illeggibile via indicizzatore:
                    // `d[5]=x` (OP_STORE_ELEMENT, chiave reale) seguito da `d[5]` (OP_LOAD_ELEMENT,
                    // ex indice posizionale) restituiva null a meno che 5 non fosse anche, per
                    // coincidenza, un indice di iterazione valido. L'accesso posizionale per FOR
                    // EACH/comprehension ora passa da OP_LOAD_ELEMENT_POSITIONAL (vedi sotto),
                    // emesso SOLO da quei due call site — mai da un vero indicizzatore `[...]`.
                    // Porting da GMBasicAI (commit 701da5e).
                    var key = keys[0]!;
                    try { task.PushRef(dict[key]); }
                    catch { task.PushRef(null); }
                    // Coercizione chiave stringa→chiave reale del DICT (parità IL/VM-C:
                    // `d("4")` su DICT OF DOUBLE vede la chiave 4.0). Prima una chiave
                    // stringa non matching restituiva null — gap trovato da
                    // minitests/constructs/expr_dict_comprehension.bas sulla gamba PCode
                    // (a= vuoto invece di 16).
                    if ((task.PeekTop() is null) && key is string ks2 && ks2.Length > 0
                        && double.TryParse(ks2, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out var kd2)
                        && dict.Contains(kd2))
                    {
                        task.PopRef();
                        task.PushRef(dict[kd2]);
                    }
                }
                else if (arr is System.Collections.IList list && numIndices > 0)
                {
                    try { task.PushRef(list[Convert.ToInt32(keys[0] ?? 0)]); }
                    catch { task.PushRef(null); }
                }
                else if (arr is string str && numIndices > 0)
                {
                    var idx = Convert.ToInt32(keys[0] ?? 0);
                    try { task.PushRef(idx >= 0 && idx < str.Length ? str[idx].ToString() : null); }
                    catch { task.PushRef(null); }
                }
                /* Collezioni enumerabili NON IList/IDictionary (CommonLib TRIE e affini):
                 * elemento POSIZIONALE nell'ordine di enumerazione. Senza, il FOR EACH
                 * (UBOUND + LOAD_ELEMENT_POSITIONAL) iterava correttamente ma stampava
                 * vuoto (CollectionsModern: `FOR EACH w IN trie` → righe "  - "). */
                else if (arr is System.Collections.IEnumerable genEnum && numIndices == 1)
                {
                    var gidx = Convert.ToInt32(keys[0] ?? 0);
                    object? gitem = null;
                    var gk = 0;
                    foreach (var it in genEnum) { if (gk++ == gidx) { gitem = it; break; } }
                    task.PushRef(gitem);
                }
                // Collection chiave→valore (RedBlackTree, Cache, BPlusTree): indexer via
                // l'interfaccia non-generica (niente reflection — illink segue il pattern).
                else if (arr is CommonLib.Collections.IKeyValueIndexable kvi && numIndices == 1)
                {
                    if (kvi.TryGetValue(keys[0], out var v))
                        task.PushRef(v);
                    else
                        task.PushRef(null);
                }
                else
                    task.PushRef(null);
                return true;
            }
            // ---- righe originali 2357-2411 ----
            case OpCode.OP_ARRAY_LEN:
            {
                var arrObj = task.Sp > 0 ? task.PopRef() : null;
                if (arrObj is Array arr)
                    task.PushNum(arr.Length);
                else if (arrObj is System.Collections.IList list)
                    task.PushNum(list.Count);
                else if (arrObj is string str)
                    task.PushNum(str.Length);
                else if (arrObj is System.Collections.IDictionary dict)
                    task.PushNum(dict.Count);
                else
                    task.PushNum(0);
                return true;
            }
            case OpCode.OP_STORE_ELEMENT:
            {
                var numIndices = ReadU8(task);
                // PERCORSO RAPIDO (2026-11-06): vedi OP_LOAD_ELEMENT; il valore deve essere già del tipo dell'array.
                if (numIndices == 1 && task.Sp >= 3 && TryFastElementStore(task)) return true;
                var keys = new object?[numIndices];
                for (var i = numIndices - 1; i >= 0; i--)
                    keys[i] = task.Sp > 0 ? task.PopRef() : null;
                var arr = task.Sp > 0 ? task.PopRef() : null;
                var val = task.Sp > 0 ? task.PopRef() : null;
                if (arr is RefToVar rvS) arr = rvS.Array[rvS.Index];
                // Stage C: matrice a descrittore — scrittura diretta dell'elemento (Set no-op
                // fuori range, canone del descrittore IL).
                if (arr is CommonLib.MatrixValue mvS && numIndices >= 2)
                {
                    mvS.Set(Convert.ToInt32(keys[0] ?? 0), Convert.ToInt32(keys[1] ?? 0), SafeDouble(val));
                    return true;
                }
                if (arr is Array array && numIndices > 0)
                {
                    var idx = keys.Select(k => Convert.ToInt32(k ?? 0)).ToArray();
                    try { array.SetValue(val, idx); }
                    catch (Exception ex) when (ex is IndexOutOfRangeException or ArgumentOutOfRangeException)
                    {
                        // Scrittura oltre UBOUND di un array dimensionato: ERR 9 (prima veniva
                        // scartata in silenzio; VM/C e C99Native estendevano l'array).
                        RaiseBasicError(task, 9.0, string.Empty);
                    }
                    catch { }
                }
                else if (arr is System.Collections.IDictionary dict && numIndices == 1)
                {
                    dict[keys[0]!] = val;
                }
                else if (arr is System.Collections.IList list && numIndices == 1)
                {
                    var idx = Convert.ToInt32(keys[0] ?? 0);
                    if (idx >= list.Count) list.Add(val);
                    else list[idx] = val;
                }
                // Collection chiave→valore (RedBlackTree, Cache, BPlusTree): indexer via
                // l'interfaccia non-generica (niente reflection).
                else if (arr is CommonLib.Collections.IKeyValueIndexable kvi && numIndices == 1)
                {
                    try { kvi.Set(keys[0], val); } catch { }
                }
                return true;
            }

            // ---- righe originali 2477-2522 ----
            case OpCode.OP_CALL_BUILTIN:
            {
                var funcCode = ReadU16(task);
                var argCount = ReadU8(task);
                var args = new object?[argCount];
                for (var i = argCount - 1; i >= 0; i--)
                    args[i] = task.Sp > 0 ? task.PopRef() : null;
                // Ordine output (2026-09-23, Parita-5-Backend-Residui §2 — ModelQuantize):
                // i PRINT sono bufferati per-tick (PendingPrints, flush a fine tick) mentre
                // un builtin può scrivere DIRETTAMENTE su Current.Console (es. AiModelInspect
                // via il provider embedded): senza flush, il suo output precede i PRINT
                // precedenti (tabella prima dell'header). Stesso schema del SAY
                // (Syscalls.cs, FlushPrintBuffer prima della scrittura diretta).
                FlushPrintBuffer(task);
                var bResult = ExecuteBuiltInGuarded(task, funcCode, args);
                if (ReferenceEquals(bResult, ParkedBuiltinResult))
                    return true;
                // NEXT() su un generatore esaurito segnala StopIteration (ERR.Number=144) invece
                // di restituire un valore normale — stesso meccanismo bytecode di THROW_ERROR
                // (nessuna eccezione C#, TRY/CATCH in P-CODE è puro redirect di task.Ip via
                // task.TryStack). Va gestito QUI, prima del PushRef incondizionato sotto: se
                // saltassimo l'IP dentro ExecuteBuiltIn, il PushRef(bResult) successivo finirebbe
                // comunque nel nuovo contesto (il blocco CATCH), corrompendone lo stack.
                if (bResult is StopIterationSignal)
                {
                    SetErrGlobal(task, new System.Collections.Generic.Dictionary<string, object?>
                    {
                        ["Number"] = 144.0, // ErrorCode.StopIteration
                        ["Description"] = "Stop iteration",
                        ["LineNumber"] = (double)SourceLineForIp(task.CurrentInstructionIp),
                        ["SourceLineText"] = string.Empty,
                        ["StackTrace"] = BuildBasicTrace(task)
                    });
                    var errInfo = new System.Collections.Generic.Dictionary<string, object?>
                    {
                        ["Number"] = 144.0, // ErrorCode.StopIteration
                        ["Description"] = "Stop iteration"
                    };
                    if (TryEnterExceptionHandler(task, errInfo))
                        return true;
                    throw new InvalidOperationException("BASIC Runtime Error: Stop iteration (nessun blocco TRY attivo).");
                }
                task.PushRef(bResult);
                return true;
            }

            default:
                handled = false;
                return false;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Percorso rapido per gli elementi di array (indice singolo). NON cambia la semantica: se una
    // condizione non e' soddisfatta (tipo dell'array/dell'indice/valore, indice fuori limiti) restituisce
    // false SENZA toccare lo stack e il chiamante esegue il percorso generale (che solleva ERR 9, ecc.).
    // ------------------------------------------------------------------------------------------------

    /// <summary>Indice intero di una entry di stack: stesso arrotondamento di Convert.ToInt32(object).</summary>
    private static bool TryFastIndex(in StackEntry e, out int idx)
    {
        if (e.Type == 1) { idx = Convert.ToInt32(e.Num); return true; }
        if (e.Type == 2)
        {
            if (e.Ref is double d) { idx = Convert.ToInt32(d); return true; }
            if (e.Ref is int i) { idx = i; return true; }
        }
        idx = 0;
        return false;
    }

    /// <summary>Stack: [..., array, indice]. Array 1D con indice in [0, Length): push dell'elemento.</summary>
    private static bool TryFastElementLoad(PCodeTask task)
    {
        var st = task.Stack;
        var sp = task.Sp;
        if (st[sp - 2].Type != 2 || !TryFastIndex(in st[sp - 1], out var idx)) return false;
        var arrObj = st[sp - 2].Ref;
        switch (arrObj)
        {
            case double[] da:
                if ((uint)idx >= (uint)da.Length) return false;
                st[sp - 1] = default; st[sp - 2] = default; task.Sp = sp - 2;
                task.PushNum(da[idx]);
                return true;
            case long[] la:
                if ((uint)idx >= (uint)la.Length) return false;
                st[sp - 1] = default; st[sp - 2] = default; task.Sp = sp - 2;
                task.PushRef(la[idx]);
                return true;
            case int[] ia:
                if ((uint)idx >= (uint)ia.Length) return false;
                st[sp - 1] = default; st[sp - 2] = default; task.Sp = sp - 2;
                task.PushRef(ia[idx]);
                return true;
            case object?[] oa:   // include string[] e array di riferimenti (covarianza)
                if ((uint)idx >= (uint)oa.Length) return false;
                st[sp - 1] = default; st[sp - 2] = default; task.Sp = sp - 2;
                task.PushRef(oa[idx]);
                return true;
            default:
                return false;
        }
    }

    /// <summary>Stack: [..., valore, array, indice]. Solo double[] con valore numerico e object[] esatto
    /// (nessuna conversione implicita: gli altri casi restano al percorso generale).</summary>
    private static bool TryFastElementStore(PCodeTask task)
    {
        var st = task.Stack;
        var sp = task.Sp;
        if (st[sp - 2].Type != 2 || !TryFastIndex(in st[sp - 1], out var idx)) return false;
        var arrObj = st[sp - 2].Ref;
        if (arrObj is double[] da)
        {
            if ((uint)idx >= (uint)da.Length) return false;
            double v;
            if (st[sp - 3].Type == 1) v = st[sp - 3].Num;
            else if (st[sp - 3].Type == 2 && st[sp - 3].Ref is double dv) v = dv;
            else return false;
            da[idx] = v;
        }
        else if (arrObj != null && arrObj.GetType() == typeof(object[]))
        {
            var oa = (object?[])arrObj;
            if ((uint)idx >= (uint)oa.Length) return false;
            oa[idx] = st[sp - 3].Type == 1 ? (object)st[sp - 3].Num : st[sp - 3].Ref;
        }
        else return false;
        st[sp - 1] = default; st[sp - 2] = default; st[sp - 3] = default;
        task.Sp = sp - 3;
        return true;
    }
}
