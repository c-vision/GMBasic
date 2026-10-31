// FILE: Basic.IlBackend.Gpm.Ir.IlCoroutineIr.cs
// G2 — IR stackless minima (piano canonico §8). Pipeline:
//   AST → MaySuspendAnalysis → StacklessLowering → IlCoroutineIr → verifier → CIL StepV2_<id>
// L'IR è UN SOLO basic block per ogni value-Pc: istruzioni straight-line + terminator.
// Ogni sottoespressione viva a un punto di sospensione è già spillata (G2.2); qui i tipi.

using System;
using System.Collections.Immutable;

namespace Basic.IlBackend.Gpm.Ir;

/// <summary>Park SQL.EXEC forma FUNZIONE: il blocco chiama TrySqlExecFnPark col builtin e,
/// al ritorno, scrive il valore (changes) nello slot TargetSlot (-1 = scartato).</summary>
internal sealed record SqlExecFnPark(Basic.BuiltinFunctionCallExpr Call, int TargetSlot);

/// <summary>Regione di protezione (TRY/CATCH/FINALLY/USING) a cui un blocco appartiene.
/// RegionPath di un blocco = lista delle regioni aperte, da esterna a interna.</summary>
public enum IlIrRegionKind { Try, Catch, Finally, Using }

public readonly record struct IlIrRegion(IlIrRegionKind Kind, int HandlerPc, int EndPc, int FinallyPc = -1)
{
    public override string ToString() => $"{Kind}@{HandlerPc}..{EndPc}";
}

/// <summary>Istruzioni straight-line di un blocco. Operano su uno stack virtuale di valore:
/// ogni istruzione PUSH (LoadConst/LoadLocal/LoadSpill/CallSync) o POP (StoreLocal/
/// StoreSpill) un valore; il terminator consuma la cima. Le chiamate suspendable sono il
/// TERMINATOR Call (gli argomenti stanno sullo stack prima di esso). Await è un marker
/// opzionale subito prima di un terminator Park: identifica il punto di sospensione.</summary>
public enum IlIrInstKind
{
    LoadConst,      // push costante
    LoadLocal,      // push Locals[Slot]
    StoreLocal,     // pop → Locals[Slot]
    LoadSpill,      // push Spills[Slot]
    StoreSpill,     // pop → Spills[Slot]
    CallSync,       // pop N argomenti, chiamata sincrona a procedura flat, push risultato
    Await           // marker di sospensione (segue terminator Park)
}

/// <summary>Singola istruzione. Slot = local/spill (o -1), Value = costante per LoadConst,
/// TargetProc = procedura per CallSync, ArgCount = numero di argomenti da pop per CallSync.</summary>
public readonly record struct IlIrInst(
    IlIrInstKind Kind,
    int Slot = -1,
    object? Value = null,
    int TargetProc = -1,
    int ArgCount = 0)
{
    public override string ToString() =>
        Kind switch
        {
            IlIrInstKind.LoadConst => $"LOADC {Value}",
            IlIrInstKind.LoadLocal => $"LOADL {Slot}",
            IlIrInstKind.StoreLocal => $"STOREL {Slot}",
            IlIrInstKind.LoadSpill => $"LOADS {Slot}",
            IlIrInstKind.StoreSpill => $"STORES {Slot}",
            IlIrInstKind.CallSync => $"CALLS {TargetProc} ({ArgCount})",
            IlIrInstKind.Await => "AWAIT",
            _ => Kind.ToString()
        };
}

/// <summary>Terminator di blocco (piano §8): branch/switch/call/return/throw/park.
/// Un call suspendable è il TERMINATOR Call: il driver crea il frame del callee e il
/// callee al Return riprende ResumePc del caller. ArgSlots = slot Spills del frame dove
/// vivono gli argomenti della call (per una sub void sono dentro l'AstNode del blocco).</summary>
public enum IlIrTermKind
{
    Branch,         // TargetPc
    Switch,         // Targets[], test sul risultato del blocco (IF/SELECT/loop check)
    Call,           // CallSuspendable: TargetProc, ResumePc, ReturnDest, ArgSlots
    Return,         // ReturnDest + Value/ValueSlot
    Throw,          // eccezione dal risultato del blocco
    Park,           // sospende al value-Pc del blocco (Await prima)
    CompleteTask,   // solo root
    Gosub,          // pusha continuazione (ResumePc) sul GosubStack e salta a TargetProc
    ReturnGosub,    // popa la continuazione GOSUB e riprende (o Return normale se vuoto)
    ContinueUnwind, // dopo FINALLY/CATCH: riprende l'unwind (o continua normalmente)
    GoExit,         // termina il task attraversando i FINALLY (unwind reason GoExit)
    OnError,        // installa l'handler ON ERROR (TargetProc = pc handler, ResumePc = successore)
    Resume,         // RESUME [NEXT] dopo un handler ON ERROR (ResumePc = 1 per NEXT)
    Select          // GO SELECT (Targets = pc dei body dei case + default/successore)
    ,
    DeferPush,      // DEFER <body>: registra il blocco deferito (TargetPc = bodyPc) e continua
                    //   (ResumePc = successore). Il body gira LIFO all'uscita della procedura.
    DeferDone       // fine del corpo deferito: il driver popa il successivo o completa l'uscita
}

/// <summary>Terminator di un basic block.</summary>
public readonly record struct IlIrTerminator(
    IlIrTermKind Kind,
    int TargetPc = -1,
    ImmutableArray<int> Targets = default,
    int TargetProc = -1,
    int ResumePc = -1,
    IlReturnDestination ReturnDest = IlReturnDestination.Discard,
    int ValueSlot = -1,
    ImmutableArray<int> ArgSlots = default,
    int ReturnSlot = -1)
{
    public override string ToString() =>
        Kind switch
        {
            IlIrTermKind.Branch => $"BR {TargetPc}",
            IlIrTermKind.Switch => $"SW [{string.Join(',', Targets)}]",
            IlIrTermKind.Call => $"CALL {TargetProc} resume@{ResumePc} → {ReturnDest}",
            IlIrTermKind.Return => $"RET {ReturnDest}",
            IlIrTermKind.Throw => "THROW",
            IlIrTermKind.Park => "PARK",
            IlIrTermKind.CompleteTask => "COMPLETE",
            IlIrTermKind.DeferPush => $"DEFER {TargetPc} →{ResumePc}",
            IlIrTermKind.DeferDone => "DEFER-DONE",
            _ => Kind.ToString()
        };
}

/// <summary>Procedura may-suspend lowering in IR stackless: basic block per value-Pc,
/// tabella locali/spill, regione metadata e path. Entry = 0.</summary>
public sealed class IlCoroutineIr
{
    public int ProcedureId;
    public int LocalCount;
    public int SpillCount;
    public int[] ParamSlots = Array.Empty<int>();

    private readonly List<IlCoroutineBlock> _blocks = new();
    public IReadOnlyList<IlCoroutineBlock> Blocks => _blocks;

    public readonly Dictionary<int, IlCoroutineBlock> ByPc = new();

    public IlCoroutineBlock CreateBlock(int pc, IlIrRegionPath path)
    {
        if (ByPc.ContainsKey(pc)) return ByPc[pc];
        var b = new IlCoroutineBlock(pc, path);
        _blocks.Add(b);
        ByPc[pc] = b;
        return b;
    }

    public void AddBlock(IlCoroutineBlock b)
    {
        _blocks.Add(b);
        ByPc[b.Pc] = b;
    }

    /// <summary>G8: rimuove un blocco (fusione flat) — dal post-pass FuseFlatBlocks.</summary>
    public void RemoveBlock(IlCoroutineBlock b)
    {
        _blocks.Remove(b);
        ByPc.Remove(b.Pc);
    }

    public string Dump()
    {
        var sb = new System.Text.StringBuilder();
        sb.AppendLine($"proc {ProcedureId} locals={LocalCount} spills={SpillCount}");
        foreach (var b in _blocks)
        {
            sb.AppendLine($"  B{b.Pc} ({b.RegionPath}):");
            foreach (var i in b.Instructions) sb.AppendLine($"    {i}");
            sb.AppendLine($"    {b.Terminator}");
        }
        return sb.ToString();
    }
}

/// <summary>Basic block: istruzioni straight-line + terminator + region path + il nodo AST
/// (Stmt o Expr) che l'emitter valuta inline per le istruzioni non-safepoint.</summary>
public sealed class IlCoroutineBlock
{
    public int Pc;
    public IlIrRegionPath RegionPath;
    public readonly List<IlIrInst> Instructions = new();
    public IlIrTerminator Terminator;
    public object? AstNode;
    /// <summary>Nodo ausiliario associato dal lowering (es. il ForStmt per il block del check/
    /// next del FOR unit): il nodo principale resta AstNode, questo porta il contesto.</summary>
    public object? Aux;
    /// <summary>Slot di un FUTURE (GO FUNCTION INTO / FETCH) letto da questo blocco: il blocco
    /// fa l'implicit-await PARCHEGGIANTE (TryReadFuturePark) prima di eseguire lo statement —
    /// parità Go (v, ok := <-future). Null = nessun read future.</summary>
    public int? FutureSlot;
    /// <summary>G8: statement FLAT fusi nel blocco dal post-pass (il blocco esegue AstNode +
    /// ExtraStatements in sequenza, poi UN SOLO Branch — riduce i blocchi/dispatch/CIL try).</summary>
    public List<Stmt>? ExtraStatements;
    /// <summary>READ (@GET … READ): i campi @GET raccolti PRIMA di questa istruzione READ, con
    /// row/col/width/PICTURE e i predicati WHEN/VALID. Il blocco PARK del READ li cicla, rendendo
    /// ogni campo ad ogni tasto e parcheggiando (IlGpmRuntime.ReadFieldStep). Null se non è un READ.</summary>
    public List<AtGetStmt>? ReadFields;

    public IlCoroutineBlock(int pc, IlIrRegionPath path)
    {
        Pc = pc;
        RegionPath = path;
    }

    public override string ToString() => $"B{Pc}";
}

/// <summary>Path delle regioni aperte (da esterna a interna) al valore-Pc del blocco.
/// Immutable: i push/pop creano nuovi path (niente mutazione condivisa).</summary>
public readonly struct IlIrRegionPath
{
    private readonly ImmutableArray<IlIrRegion> _regions;

    private IlIrRegionPath(ImmutableArray<IlIrRegion> regions) => _regions = regions;

    public static readonly IlIrRegionPath Empty = new(ImmutableArray<IlIrRegion>.Empty);

    public bool IsEmpty => _regions.IsEmpty;

    public int Depth => _regions.Length;

    public IlIrRegion Inner => _regions.IsEmpty ? default : _regions[_regions.Length - 1];

    public IEnumerable<IlIrRegion> OuterToInner => _regions;

    public IlIrRegionPath Push(IlIrRegion r) => new(_regions.Add(r));

    public IlIrRegionPath Pop() => _regions.IsEmpty ? this : new(_regions.RemoveAt(_regions.Length - 1));

    public override string ToString() =>
        _regions.IsEmpty ? "[]" : "[" + string.Join(",", _regions) + "]";
}
