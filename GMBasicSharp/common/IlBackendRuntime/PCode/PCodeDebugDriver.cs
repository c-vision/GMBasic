// FILE: Basic.Executor.PCodeDebugDriver.cs
// Driver di debug per il backend P-Code/ILBackend: adatta il DebugServer della PCodeVM
// interna (Basic.IlBackend.PCode.DebugServer) al contratto IDebugDriver. Esegue il chunk
// con la PCodeVM in un task di background; il PCodeProcessor monta il DebugServer e blocca
// il thread a ogni breakpoint/step (MaybePauseAt → WaitForContinue). Continue/Step*
// rilasciano il task.

using System.Text;
using System.Threading;
using Compiler;
using Basic.Runtime;

namespace Basic.IlBackend.PCode;

/// <summary>Driver di debug per il backend P-Code/ILBackend (PCodeVM interna).</summary>
public sealed class PCodeDebugDriver : IDebugDriver
{
    private readonly object _sync = new();
    private Thread? _runThread;
    private CancellationTokenSource? _cts;
    private volatile bool _started;
    private readonly Basic.IlBackend.PCode.DebugServer _server = new();
    private readonly StringWriter _output = new();
    private HeadlessRuntimeHost? _host;

    public bool IsPaused => _server.IsPaused;
    public int CurrentLine => _server.CurrentLine;
    public bool IsRunning => _runThread != null && _runThread.IsAlive;

    public void Start(CompiledChunk chunk, IReadOnlyCollection<int> breakpoints)
    {
        lock (_sync)
        {
            if (_started) return;
            _started = true;
        }

        _server.SetSourceLines(chunk.Lines.Count == chunk.Code.Count ? [.. chunk.Lines] : []);
        _server.SetProcedureMap(chunk.ProcedureAddresses);
        _server.ClearBreakpoints();
        foreach (var bp in breakpoints) _server.SetBreakpoint(bp);

        _host = new HeadlessRuntimeHost(_output, null);
        _cts = new CancellationTokenSource();

        _runThread = new Thread(() =>
        {
            Basic.IlBackend.PCode.PCodeVm? vm = null;
            try
            {
                Basic.IlBackend.IlRuntime.SetCurrent(_host);
                // Monta il DebugServer sul processor: il PCodeProcessor lo consulta a ogni
                // istruzione (MaybePauseAt) e blocca su WaitForContinue ai breakpoint/step.
                PCodeProcessor.DebugServer = _server;
                vm = new PCodeVm(chunk, _host);
                vm.Run();
            }
            catch (Exception ex)
            {
                _output.Write($"[P-CODE DEBUG ERROR] {ex.Message}");
            }
            finally
            {
                PCodeProcessor.DebugServer = null;
                _server.Disconnect();
                // Dispose solo DOPO che Run() è terminato (i worker dello scheduler hanno
                // finito): chiamarla da Stop() mentre il task è in pausa = race su _queue.
                vm?.Dispose();
            }
        })
        { IsBackground = true, Name = "pcode-debug-run" };
        _runThread.Start();
    }

    public void Continue()
    {
        if (_started) _server.Continue();
    }

    public void StepIn()
    {
        if (_started) _server.StepIn();
    }

    public void StepOver()
    {
        if (_started) _server.StepOver();
    }

    public void StepOut()
    {
        if (_started) _server.StepOut();
    }

    public void SetBreakpoint(int line)
    {
        _server.SetBreakpoint(line);
    }

    public void ClearBreakpoints()
    {
        _server.ClearBreakpoints();
    }

    public string TakeOutput()
    {
        lock (_sync)
        {
            var s = _output.ToString();
            _output.GetStringBuilder().Clear();
            return s;
        }
    }

    public System.Collections.Generic.List<StackFrame>? StackFrames => _server.StackFrames;
    public System.Collections.Generic.List<DebugVariable>? GetVariables(int frameId) => _server.GetVariables(frameId);
    public string? Evaluate(string expression) => _server.Evaluate(expression);

    public void Stop()
    {
        try { _cts?.Cancel(); } catch { }
        try { _server.Disconnect(); } catch { }   // sblocca il task in pausa (se c'è)
        try { _runThread?.Join(2000); } catch { } // lascia finire il chunk (e disporre il VM nel thread)
        _started = false;
        _host = null;
    }

    public void Dispose() => Stop();
}
