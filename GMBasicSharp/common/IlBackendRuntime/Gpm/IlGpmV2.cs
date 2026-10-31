// FILE: Basic.IlBackend.Gpm.IlGpmV2.cs
// G1 — Runtime additivo v2 (piano canonico ILBackend-GPM-Stackless-Coroutines):
// tipi del modello a coroutine stackless dietro il flag GMBASIC_IL_GPM_V2=1.
// Nessuna semantica nuova: solo i tipi che materializzano la continuazione completa
// (PC per attivazione, locali, spill, destinazione, call chain, GOSUB, regioni,
// cleanup, unwind, ON ERROR, await) e le transizioni tipizzate del driver.
// Il flag è letto dal driver v2; qui serve solo per l'esposizione.

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Threading;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Gpm;

/// <summary>Vero se la GPM v2 è abilitata (GMBASIC_IL_GPM_V2=1). Il vecchio percorso
/// flat/ponytail resta il default durante G1–G7 (piano §16).</summary>
public static class IlGpmV2
{
    public static bool Enabled =>
        Environment.GetEnvironmentVariable("GMBASIC_IL_GPM_V2") == "1";

    /// <summary>Costruisce il dispatcher v2 che mappa ProcedureId → stepper StepV2_&lt;id&gt;,
    /// riflettendo sull'assembly generato i metodi `StepV2_&lt;symbolId&gt;` (e `StepV2_Main`
    /// per il top-level, ProcedureId = -1). Le procedure locali con CATTURA (closure,
    /// G4) hanno lo StepV2 come metodo D'ISTANZA della classe __ClosureCtx: il delegato
    /// viene creato al volo in Dispatch(procedureId, context) legato al context. Mirror
    /// di IlGpmRuntime.CreateDispatcher per il path flat.</summary>
    public static IlGpmV2Dispatcher CreateV2Dispatcher(Assembly assembly)
    {
        var steps = new Dictionary<int, IlGpmStepV2>();
        var instanceSteps = new Dictionary<int, MethodInfo>();
        foreach (var type in assembly.GetTypes())
        {
            foreach (var m in type.GetMethods(BindingFlags.Public | BindingFlags.Static | BindingFlags.Instance))
            {
                if (!m.Name.StartsWith("StepV2_", StringComparison.Ordinal)) continue;
                if (m.Name == "StepV2_Main")
                {
                    if (m.IsStatic)
                        steps[-1] = m.CreateDelegate(typeof(IlGpmStepV2)) is IlGpmStepV2 sMain ? sMain : null!;
                    continue;
                }
                var idPart = m.Name["StepV2_".Length..];
                if (!int.TryParse(idPart, out var id)) continue;
                if (m.IsStatic)
                    steps[id] = m.CreateDelegate(typeof(IlGpmStepV2)) is IlGpmStepV2 s
                        ? s
                        : throw new InvalidOperationException($"IlGpmV2: firma stepper '{m.Name}' non valida.");
                else
                    instanceSteps[id] = m;
            }
        }
        return new TableDispatcherV2(steps, instanceSteps) { SourceAssembly = assembly };
    }

    private sealed class TableDispatcherV2 : IlGpmV2Dispatcher
    {
        private readonly Dictionary<int, IlGpmStepV2> _steps;
        private readonly Dictionary<int, MethodInfo> _instanceSteps;
        public TableDispatcherV2(Dictionary<int, IlGpmStepV2> steps, Dictionary<int, MethodInfo> instanceSteps)
        {
            _steps = steps;
            _instanceSteps = instanceSteps;
        }

        public override IlGpmStepV2? Dispatch(int procedureId)
            => _steps.TryGetValue(procedureId, out var step) ? step : null;

        /// <summary>Dispatch di una procedura di CLOSURE: crea il delegato d'istanza legato
        /// al context (l'istanza __ClosureCtx promossa dal Linker). Il delegato statico resta
        /// il caso default. Nessuna cache mutabile condivisa: il dispatcher e' usato da tutti
        /// i worker v2 e una coppia id/delegate non puo' essere pubblicata atomicamente.</summary>
        public override IlGpmStepV2? Dispatch(int procedureId, object? context)
        {
            if (context is not null && _instanceSteps.TryGetValue(procedureId, out var m))
                return (IlGpmStepV2)Delegate.CreateDelegate(typeof(IlGpmStepV2), context, m);
            return Dispatch(procedureId);
        }
    }
}

/// <summary>Stepper v2 di una procedura: esegue UN passo (block) e ritorna la transizione.
/// Riceve il task (per gli helper runtime di park/wake) e il frame corrente.</summary>
public delegate IlStepTransition IlGpmStepV2(PCodeTask task, IlActivationFrame frame);

/// <summary>Dispatcher v2: mappa frame.ProcedureId → stepper StepV2_&lt;id&gt; (e
/// StepV2_Main per il top-level, ProcedureId = -1).</summary>
public abstract class IlGpmV2Dispatcher
{
    /// <summary>Assembly dell'assembly corrente (quello che ha generato gli stepper v2):
    /// il fallback FLAT (RunFlatGoroutine/CallFlatSub) deve risolvere Proc_&lt;id&gt; SOLO qui
    /// — mai in tutte le assembly del processo (in un harness a processo condiviso i proc id
    /// collidono tra sample → 'Invalid cast String→ChannelInfo').</summary>
    public Assembly? SourceAssembly { get; set; }

    public abstract IlGpmStepV2? Dispatch(int procedureId);

    /// <summary>Dispatch con il context della closure (istanza __ClosureCtx) per gli
    /// stepper d'istanza delle procedure locali con cattura; default: delegato statico.</summary>
    public virtual IlGpmStepV2? Dispatch(int procedureId, object? context)
        => Dispatch(procedureId);
}
