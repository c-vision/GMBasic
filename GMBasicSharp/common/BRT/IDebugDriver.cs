// FILE: Basic.Executor.IDebugDriver.cs
// Contratto comune per il debug dei 3 backend (P-Code/ILBackend, VM/C, C99).
// L'IDE (DebugServer unificato) parla SOLO questa interfaccia: ogni backend
// implementa il proprio driver che adatta il debugger nativo del backend.
// Driver previsti:
//   - PCodeDebugDriver  → DebugServer della PCodeVM interna (funzionante)
//   - VmCDebugDriver    → VM/C (stub, richiede ponte in-process)
//   - C99DebugDriver    → transpiler C99 (stub, richiede line-map BASIC→C)

using Compiler;

namespace Basic.Runtime;

/// <summary>Contratto che ogni backend espone per il debug di riga.</summary>
public interface IDebugDriver : IDisposable
{
    /// <summary>True se il task è in pausa su un breakpoint/step.</summary>
    bool IsPaused { get; }

    /// <summary>True se il task è ancora in esecuzione (non terminato).</summary>
    bool IsRunning { get; }

    /// <summary>Riga sorgente corrente (1-based, 0 = nessuna).</summary>
    int CurrentLine { get; }

    /// <summary>Avvia l'esecuzione del chunk. Imposta breakpoint e line-map.</summary>
    void Start(CompiledChunk chunk, IReadOnlyCollection<int> breakpoints);

    /// <summary>Riprende l'esecuzione fino al prossimo breakpoint/fine.</summary>
    void Continue();

    /// <summary>Step singolo (entra nelle subroutine).</summary>
    void StepIn();

    /// <summary>Step fino alla riga successiva (senza entrare nelle subroutine).</summary>
    void StepOver();

    /// <summary>Step fino all'uscita dalla subroutine corrente.</summary>
    void StepOut();

    /// <summary>Imposta un breakpoint per riga sorgente (1-based).</summary>
    void SetBreakpoint(int line);

    /// <summary>Rimuove tutti i breakpoint.</summary>
    void ClearBreakpoints();

    /// <summary>Output prodotto dall'esecuzione (catturato fin dall'ultima lettura).</summary>
    string TakeOutput();

    /// <summary>Frame dello stack di chiamata quando in pausa (null se non disponibile).</summary>
    System.Collections.Generic.List<StackFrame>? StackFrames { get; }

    /// <summary>Variabili locali di un frame (id da StackFrames).</summary>
    System.Collections.Generic.List<DebugVariable>? GetVariables(int frameId);

    /// <summary>Valuta un'espressione nel contesto del frame in pausa (null se non possibile).</summary>
    string? Evaluate(string expression);

    /// <summary>Interrompe l'esecuzione (cancel + cleanup).</summary>
    void Stop();
}
