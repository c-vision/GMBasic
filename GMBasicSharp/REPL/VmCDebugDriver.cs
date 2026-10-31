// FILE: Basic.Executor.VmCDebugDriver.cs
// Driver di debug per la VM/C (backend `vm`, esecuzione con la VM in C).
// STUB: oggi l'esecuzione P-Code usa la PCodeVM C# (PCodeDebugDriver), quindi questo
// driver non è necessario per il percorso attuale. Quando il backend VM/C tornerà
// attivo, andrà collegato al debugger nativo della VM/C (ponte in-process o gmb_vm_host).
// Lancia errore chiaro invece di un no-op silenzioso.

using Compiler;
using Basic.Runtime;

namespace Basic.Executor;

/// <summary>Driver di debug per la VM/C — STUB (vedi header del file).</summary>
public sealed class VmCDebugDriver : IDebugDriver
{
    public bool IsPaused => false;
    public bool IsRunning => false;
    public int CurrentLine => 0;

    public void Start(CompiledChunk chunk, IReadOnlyCollection<int> breakpoints)
        => throw new NotSupportedException("Debug VM/C non implementato: il percorso P-Code usa la PCodeVM C# (PCodeDebugDriver).");

    public void Continue() { }
    public void StepIn() { }
    public void StepOver() { }
    public void StepOut() { }
    public void SetBreakpoint(int line) { }
    public void ClearBreakpoints() { }
    public string TakeOutput() => "";
    public System.Collections.Generic.List<StackFrame>? StackFrames => null;
    public System.Collections.Generic.List<DebugVariable>? GetVariables(int frameId) => null;
    public string? Evaluate(string expression) => null;
    public void Stop() { }
    public void Dispose() { }
}
