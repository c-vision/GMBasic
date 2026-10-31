// FILE: Basic.Executor.DebugServer.cs
// DebugServer UNIFICATO per l'IDE: incapsula un IDebugDriver (scelto in base al backend)
// ed espone l'API usata da GMBasicIde (IsPaused/CurrentLine/Continue/Step/breakpoint/
// Watch/Stack/Output). L'IDE non sa quale backend esegue: parla solo con questo wrapper.

using Compiler;
using Basic.Runtime;
using Basic.IlBackend.PCode;

namespace Basic.Executor;

/// <summary>Backend di esecuzione per cui creare il driver di debug.</summary>
public enum DebugBackend { PCode, VmC }

/// <summary>Debugger unificato: delega a un IDebugDriver per il backend attivo.</summary>
public sealed class DebugServer
{
    private readonly IDebugDriver _driver;
    private bool _running;

    public bool IsPaused => _driver.IsPaused;
    public bool IsRunning => _driver.IsRunning;
    public int CurrentLine => _driver.CurrentLine;

    public DebugServer(IDebugDriver driver) => _driver = driver;

    /// <summary>Crea il driver per il backend richiesto.</summary>
    public static DebugServer Create(DebugBackend backend) => backend switch
    {
        DebugBackend.PCode => new DebugServer(new PCodeDebugDriver()),
        DebugBackend.VmC => new DebugServer(new VmCDebugDriver()),
        _ => new DebugServer(new PCodeDebugDriver()),
    };

    public void Start(CompiledChunk chunk, IReadOnlyCollection<int> breakpoints)
    {
        _driver.Start(chunk, breakpoints);
        _running = true;
    }

    public void Continue() { if (_running) _driver.Continue(); }
    public void StepIn() { if (_running) _driver.StepIn(); }
    public void StepOver() { if (_running) _driver.StepOver(); }
    public void StepOut() { if (_running) _driver.StepOut(); }
    public void SetBreakpoint(int line) => _driver.SetBreakpoint(line);
    public void ClearBreakpoints() => _driver.ClearBreakpoints();
    public string TakeOutput() => _driver.TakeOutput();
    public System.Collections.Generic.List<StackFrame>? StackFrames => _driver.StackFrames;
    public System.Collections.Generic.List<DebugVariable>? GetVariables(int frameId) => _driver.GetVariables(frameId);
    public string? Evaluate(string expression) => _driver.Evaluate(expression);

    public void Stop()
    {
        _running = false;
        _driver.Stop();
    }
}
