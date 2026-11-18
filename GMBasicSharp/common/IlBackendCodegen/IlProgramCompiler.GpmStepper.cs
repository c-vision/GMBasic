// FILE: Basic.IlBackend.IlProgramCompiler.GpmStepper.cs
// IL GPM — GENERAZIONE STEPPER (STEP 2b del piano, vedi 
// GMBasic-ILBackend-GPM-Chunk-State-Machine §13-2, §16, §26).
//
// Per ogni procedura may-park FLAT (caso semplice §13-2: nessuna chiamata may-park
// nidificata, punti di blocco solo al top del body) il compilatore emette un metodo
// statico:
//
//     public static IlGpmStepResult Step_<symbolId>(PCodeTask task)
//
// che a ogni chiamata esegue UN chunk e ritorna Continue/Blocked/Finished. Il resumeIp è
// task.Ip = indice del chunk. Il dispatch è un `switch` su task.Ip (§26.2). I locali CIL
// vengono salvati nel PCodeCallFrame.LocalVariables ai confini di chunk e ri-caricati al
// prologo (Scelta B del §11.3, INTERIM: corretta ma più lenta della Scelta A "tutto nel
// frame"; documentata per una futura ottimizzazione).
//
// STATO: gli stepper sono generati E, se Main è may-park FLAT, l'entry point
// avvia IlGpmDriver (ShouldRunMainAsGpm). I comandi grafici LOAD/SYNC sono
// block point: Main con SPRITE LOAD o SYNC gira come task GPM, non inline.

using System.Collections.Immutable;
using System.Reflection;
using System.Reflection.Emit;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public sealed partial class IlProgramCompiler
{
    // Scaffolding GPM v2 stepper (work in progress): campi LETTI nelle parti del compilatore
    // (ControlFlow/Operators/Members/StatementsMore) ma non ancora scritti — il flag verrà
    // assegnato quando il wiring dello stepper sarà completato. Warning CS0649 silenziato.
#pragma warning disable 0649
    private bool _gpmStepperMode;
    private Dictionary<int, int>? _gpmFrameSlots;   // symbolId → slot in LocalVariables
#pragma warning restore 0649
    private readonly List<(int SymbolId, LocalBuilder Local, Type ClrType)> _gpmTrackedLocals = new();
    // In stepper mode i temporanei del FOR (end/step, double) NON possono vivere in LocalBuilder
    // CIL: a ogni tick il metodo riparte e i locali si azzerano (§15 — bug GoRoutinesMultiplexing:
    // step=0 → il contatore non avanza → loop infinito). Vivono in slot dedicati del frame.
    private readonly Dictionary<string, (int CounterSymbolId, int EndSlot, int StepSlot)> _gpmForState = new();
    private int _gpmNextForSlot;
    // Variabili che ricevono un GO FUNCTION ... INTO x: il loro local CIL deve essere OBJECT
    // (contengono un PCodeFutureValue, non il valore tipizzato — che arriva alla risoluzione).
    private readonly HashSet<int> _gpmFutureTargets = new();
    // Etichetta → indice del chunk in cui cade (per ON ERROR GOTO <label> / RESUME <label>).
    private Dictionary<string, int> _gpmLabelToChunk = new();

    /// <summary>True se Main deve girare come task GPM: è may-park FLAT (ha uno stepper
    /// `Step_&lt;id&gt;` generato). Coerente col gate usato in EmitGpmSteppers.</summary>
    public static bool ShouldRunMainAsGpmV2(
        FunctionDefinition mainProcDef,
        Dictionary<int, FunctionDefinition> procedureDefinitions,
        Func<int, Basic.DataType?>? symbolType = null,
        Func<int, bool>? isGlobalSymbol = null)
    {
        if (!MayParkAnalysis.IsLocalMayPark(mainProcDef, _ => false, symbolType)) return false;
        var procedures = procedureDefinitions.Values.ToImmutableArray();
        var mayPark = MayParkAnalysis.ComputeMayPark(procedures, id => procedureDefinitions.TryGetValue(id, out var d) ? d : null, symbolType);
        if (!mayPark.Contains(mainProcDef.SymbolId)) return false;
        var nameToId = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var p in procedures) { nameToId[p.OriginalName.Lexeme] = p.SymbolId; nameToId[p.Name.Lexeme] = p.SymbolId; }
        return IlCoroutineEmitter.IsV2Subset(mainProcDef, mayPark, nameToId, symbolType, isGlobalSymbol);
    }

    /// <summary>Entry point GPM v2 (G7): chiama IlGpmRuntime.RunV2EntryConsole(mainSymbolId) —
    /// il driver v2 (IlGpmV2Driver) esegue Main come task stackless. Il wrapper avvia il feeder
    /// console→QueueInkey (solo console reale; no-op headless/test) così WAITKEY/INKEY del v2
    /// vedono i tasti da tastiera reale. Molto più compatto del bootstrap inline del ponytail.</summary>
    private void EmitEntryPointGpmV2(FunctionDefinition mainProcDef)
    {
        EmitProcedureRegistrations();
        _il.Emit(OpCodes.Ldc_I4, mainProcDef.SymbolId);
        _il.Emit(OpCodes.Call, IlGpmRuntimeRunV2EntryConsoleMethod);
        // Il driver v2 è terminato (RunV2EntryConsole è bloccante fino a fine Main):
        // finalizzazione deterministica dei globali CLASS/UDT con DESTRUCTOR (parità PCode/VMC).
        EmitFinalizeGlobalObjects();
        _il.Emit(OpCodes.Ret);
    }

    private static readonly MethodInfo IlGpmRuntimeRunV2EntryConsoleMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.RunV2EntryConsole))!;

    /// <summary>Entry point GPM (STEP 1b, §11.6/§16): crea lo PCodeScheduler, il task Main
    /// con OwnerProcedure = Main.SymbolId (il dispatcher lo mappa su Step_&lt;MainId&gt;),
    /// lo accoda e avvia IlGpmDriver. Non chiama mai Proc_&lt;Main&gt; inline: l'intera
    /// Main gira come task tramite il suo stepper. Il dispatcher (CreateDispatcher)
    /// riflette sull'ASSEMBLY di <paramref name="programType"/> per trovare i metodi
    /// Step_* — da cui la necessità di passare il TypeBuilder della classe generata.</summary>
    /// (LOAD TEXTURE lancia). Il trampolino avvia lo stepper già emesso da EmitGpmSteppers.
    /// G7: il trampolino chiama <see cref="IlGpmRuntime.RunProcedureAsGpm"/>, che esegue la
    /// procedura tramite il suo <c>StepV2_&lt;id&gt;</c>. Se la procedura NON è nel sottoinsieme
    /// v2 lo stepper NON esiste: RunProcedureAsGpm ricade su
    /// <c>RunFlatGoroutine(Proc_&lt;id&gt;)</c> — che è il trampolino stesso → ricorsione infinita
    /// (ogni livello apre un RunV2Inline con thread NUOVI: il guard ThreadLocal di
    /// RunFlatGoroutine non scatta). Quindi il trampolino si emette SOLO quando esiste lo
    /// stepper; altrimenti il corpo flat è l'unico percorso (bug reale:
    /// GPM-Graphics-Park-SCREEN-MOUSE-Headless-Hang-CPU-Spin — Main con MOUSE ON fuori
    /// subset → CPU al 1500% e hang).
    /// Oltre ai park grafici, il trampolino copre anche le procedure con READ (@GET … READ):
    /// una SUB con READ non deve restare lineare (bloccante su stdin) ma girare come task GPM
    /// (park per-tasto, parità col modello C gmb_read_run).</summary>
    private static bool IsGraphicsGpmTrampoline(FunctionDefinition definition, ImmutableHashSet<int> mayPark,
        Dictionary<string, int> nameToId, Func<int, Basic.DataType?> symbolType, Func<int, bool>? isGlobalSymbol = null)
        => definition.ReturnType is VoidType
           && mayPark.Contains(definition.SymbolId)

           && (MayParkAnalysis.HasGraphicsPark(definition.Body)
               || MayParkAnalysis.HasReadPark(definition.Body))

           && IlCoroutineEmitter.IsV2Subset(definition, mayPark, nameToId, symbolType, isGlobalSymbol);

    /// <summary>Proc_* → <see cref="IlGpmRuntime.RunProcedureAsGpm"/> (stesso SymbolId dello
    /// stepper Step_*). I parametri vanno nei primi slot del frame, come SpawnGoSub.</summary>
    private void EmitGpmGraphicsTrampoline(FunctionDefinition definition)
    {
        _il.Emit(OpCodes.Ldc_I4, definition.SymbolId);
        var n = definition.Parameters.Length;
        _il.Emit(OpCodes.Ldc_I4, n);
        _il.Emit(OpCodes.Newarr, typeof(object));
        for (var i = 0; i < n; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i);
            _il.Emit(OpCodes.Ldarg, i);
            var clr = DataTypeMapper.Map(NormalizeInferredType(definition.Parameters[i].DataType), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, IlGpmRuntimeRunProcedureAsGpmMethod);
        _il.Emit(OpCodes.Ret);
    }

    private static readonly MethodInfo IlGpmRuntimeRunProcedureAsGpmMethod =
        typeof(IlGpmRuntime).GetMethod(nameof(IlGpmRuntime.RunProcedureAsGpm))!;

    /// <summary>Vero solo per il "caso semplice" §13-2: procedura may-park i cui punti di
    /// blocco stanno al TOP del body (niente blocchi dentro IF/loop/chiamate). Le chiamate
    /// a procedure may-park sono esplicitamente non supportate (regola conservativa §11.5).</summary>
}
