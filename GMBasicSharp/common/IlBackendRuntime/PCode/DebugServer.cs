using Basic.Runtime.Gpm;
using Basic.Runtime;

namespace Basic.IlBackend.PCode;

internal class DebugServer
{
    private readonly ManualResetEventSlim _continueSignal = new(true);
    private volatile bool _paused;
    private PCodeTask? _pausedTask;
    private Dictionary<int, string>? _ipToName;
    private List<int>? _sourceLines;
    private readonly List<Breakpoint> _breakpoints = [];

    private enum StepMode { None, StepLine, StepOver, StepOut }
    private StepMode _stepMode;
    private int _stepDepth;
    private int _pausedLine;
    private int _observedLine = -1;

    public bool IsPaused => _paused;

    /// <summary>Ultima riga sorgente su cui il task si è fermato (0 = nessuno).
    /// Usato dall'IDE per evidenziare la riga corrente di esecuzione.</summary>
    public int CurrentLine => _pausedLine;

    public void SetProcedureMap(Dictionary<string, int> procAddresses)
    {
        _ipToName = new Dictionary<int, string>();
        foreach (var (name, ip) in procAddresses)
            _ipToName[ip] = name;
    }

    public void SetSourceLines(List<int> lines) => _sourceLines = lines;

    public List<StackFrame>? StackFrames => _pausedTask != null
        ? CaptureStackFrames(_pausedTask) : null;

    public List<DebugVariable>? GetVariables(int frameId)
    {
        if (_pausedTask == null) return null;
        var frame = GetFrame(_pausedTask, frameId);
        if (frame == null) return null;
        return CaptureLocals(frame);
    }

    public void SetBreakpoint(int line, string? condition = null)
    {
        _breakpoints.RemoveAll(b => b.Line == line);
        _breakpoints.Add(new Breakpoint { Line = line, Condition = condition });
    }

    public void SetFunctionBreakpoint(string name)
    {
        if (!_breakpoints.Any(b => b.FunctionName == name))
            _breakpoints.Add(new Breakpoint { FunctionName = name });
    }

    public void ClearBreakpoints() => _breakpoints.Clear();

    public string? Evaluate(string expression)
    {
        if (_pausedTask == null) return null;
        try
        {
            var trimmed = expression.Trim();
            object? val = EvaluateExpression(trimmed);
            return val switch
            {
                null => "NULL",
                double d => d.ToString("G"),
                int i => i.ToString(),
                string s => s,
                bool b => b ? "TRUE" : "FALSE",
                ByRefBox box => box.Value?.ToString() ?? "NULL",
                _ => val.ToString() ?? "?"
            };
        }
        catch (Exception ex) { return $"Error: {ex.Message}"; }
    }

    private object? EvaluateExpression(string expr)
    {
        expr = expr.Trim();
        if (string.IsNullOrEmpty(expr)) return null;

        if (double.TryParse(expr, System.Globalization.NumberStyles.Any,
                System.Globalization.CultureInfo.InvariantCulture, out var num))
            return num;

        if (expr.StartsWith('"') && expr.EndsWith('"'))
            return expr[1..^1];

        if (expr.Equals("TRUE", StringComparison.OrdinalIgnoreCase)) return true;
        if (expr.Equals("FALSE", StringComparison.OrdinalIgnoreCase)) return false;
        if (expr.Equals("NULL", StringComparison.OrdinalIgnoreCase)) return null;

        var val = EvaluateSymbol(expr);
        if (val != null) return val;

        var opIdx = FindLowestPrecedenceOp(expr);
        if (opIdx >= 0)
        {
            var left = EvaluateExpression(expr[..opIdx]);
            var right = EvaluateExpression(expr[(opIdx + 1)..]);
            var op = expr[opIdx];

            if (left is double ld && right is double rd)
            {
                return op switch
                {
                    '+' => ld + rd,
                    '-' => ld - rd,
                    '*' => ld * rd,
                    '/' => ld / rd,
                    '^' => Math.Pow(ld, rd),
                    '<' => ld < rd,
                    '>' => ld > rd,
                    _ => null
                };
            }
            if (op == '+' && left is string ls && right != null)
                return ls + right.ToString();
        }

        if (expr.StartsWith('-'))
        {
            var inner = EvaluateExpression(expr[1..]);
            if (inner is double d) return -d;
        }

        if (expr.EndsWith(')') && expr.Contains('('))
        {
            var open = expr.IndexOf('(');
            var func = expr[..open].Trim();
            var args = expr[(open + 1)..^1];
            return EvaluateBuiltin(func, args);
        }

        return null;
    }

    private object? EvaluateBuiltin(string name, string args)
    {
        var parts = args.Split(',');
        var evaluated = parts.Select(p => EvaluateExpression(p.Trim())).ToArray();
        try
        {
            return name.ToUpperInvariant() switch
            {
                "ABS" when evaluated[0] is double d => Math.Abs(d),
                "SQR" when evaluated[0] is double d => Math.Sqrt(d),
                "INT" when evaluated[0] is double d => (int)d,
                "LEN" when evaluated[0] is string s => (double)s.Length,
                "CSTR" => evaluated[0]?.ToString(),
                "VAL" when evaluated[0] is string s && double.TryParse(s, out var v) => v,
                _ => null
            };
        }
        catch { return null; }
    }

    private static int FindLowestPrecedenceOp(string expr)
    {
        int depth = 0;
        int bestIdx = -1;
        int bestPrec = int.MaxValue;
        for (int i = expr.Length - 1; i >= 0; i--)
        {
            var ch = expr[i];
            if (ch == ')') depth++;
            else if (ch == '(') depth--;
            else if (depth == 0 && IsOp(ch))
            {
                var prec = Precedence(ch);
                if (prec < bestPrec) { bestPrec = prec; bestIdx = i; }
            }
        }
        return bestIdx;
    }

    private static bool IsOp(char c) => c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '<' || c == '>' || c == '=';
    private static int Precedence(char op) => op switch { '=' or '<' or '>' => 1, '+' or '-' => 2, '*' or '/' => 3, '^' => 4, _ => 99 };

    public void SignalPause(PCodeTask task)
    {
        var lineMode = _stepMode == StepMode.StepLine;
        if (lineMode)
        {
            _stepMode = StepMode.None;
        }
        else if (_stepMode == StepMode.StepOver)
        {
            var depth = GetCallDepth(task);
            if (depth > _stepDepth) return;
            _stepMode = StepMode.None;
        }
        else if (_stepMode == StepMode.StepOut)
        {
            var depth = GetCallDepth(task);
            if (depth >= _stepDepth) return;
            _stepMode = StepMode.None;
        }

        if (!lineMode)
        {
            if (!ShouldBreak(task))
            {
                if (_stepMode == StepMode.None) return;
            }
        }

        var pauseLine = task.Ip > 0 && _sourceLines != null && task.Ip - 1 < _sourceLines.Count
            ? _sourceLines[task.Ip - 1] + 1 : 0;
        _pausedLine = pauseLine;
        _paused = true;
        _pausedTask = task;
        _continueSignal.Reset();
    }

    /// <summary>Hook chiamato dal PCodeProcessor a ogni istruzione quando questo DebugServer è
    /// montato su PCodeProcessor.DebugServer. Evita l'iniezione di OP_BREAKPOINT nel bytecode
    /// (che sposterebbe i target U16 assoluti di OP_GOTO/OP_GOSUB). Ferma il task solo all'
    /// ingresso di una nuova riga sorgente: scatta per breakpoint per-riga e per step modes.
    /// Blocca il thread fino a Continue/Step. Ritorna true se ha fermato il task.</summary>
    public bool MaybePauseAt(int ip, PCodeTask task)
    {
        var line = _sourceLines != null && ip >= 0 && ip < _sourceLines.Count ? _sourceLines[ip] + 1 : 0;
        var lineChanged = line != _observedLine;
        _observedLine = line;

        if (lineChanged && line > 0)
        {
            foreach (var bp in _breakpoints)
            {
                if (bp.Line > 0 && bp.Line == line && (bp.Condition == null || EvaluateCondition(bp.Condition)))
                {
                    PauseAt(line, task);
                    return true;
                }
            }
        }

        if (_stepMode != StepMode.None)
        {
            if (_stepMode == StepMode.StepLine)
            {
                if (lineChanged)
                {
                    _stepMode = StepMode.None;
                    PauseAt(line, task);
                    return true;
                }
                return false;
            }
            if (_stepMode == StepMode.StepOver)
            {
                var depth = GetCallDepth(task);
                if (depth > _stepDepth || !lineChanged) return false;
                _stepMode = StepMode.None;
                PauseAt(line, task);
                return true;
            }
            if (_stepMode == StepMode.StepOut)
            {
                var depth = GetCallDepth(task);
                if (depth >= _stepDepth) return false;
                _stepMode = StepMode.None;
                PauseAt(line, task);
                return true;
            }
        }
        return false;
    }

    private void PauseAt(int line, PCodeTask task)
    {
        _pausedLine = line;
        _paused = true;
        _pausedTask = task;
        _continueSignal.Reset();
        WaitForContinue();
    }

    private bool ShouldBreak(PCodeTask task)
    {
        if (_breakpoints.Count == 0) return false;
        foreach (var bp in _breakpoints)
        {
            if (bp.FunctionName != null)
            {
                var frameName = ResolveName(task.Ip);
                if (frameName.Contains(bp.FunctionName, StringComparison.OrdinalIgnoreCase))
                {
                    if (bp.Condition == null || EvaluateCondition(bp.Condition))
                        return true;
                }
            }
        }
        return false;
    }

    public void WaitForContinue() => _continueSignal.Wait();

    public void Continue()
    {
        _stepMode = StepMode.None;
        _paused = false;
        _pausedTask = null;
        _continueSignal.Set();
    }

    public void StepOver()
    {
        _stepMode = StepMode.StepOver;
        _stepDepth = _pausedTask != null ? GetCallDepth(_pausedTask) : 0;
        _paused = false;
        _pausedTask = null;
        _continueSignal.Set();
    }

    public void StepOut()
    {
        _stepMode = StepMode.StepOut;
        _stepDepth = _pausedTask != null ? GetCallDepth(_pausedTask) : 0;
        _paused = false;
        _pausedTask = null;
        _continueSignal.Set();
    }

    public void StepIn()
    {
        _stepMode = StepMode.StepLine;
        _paused = false;
        _pausedTask = null;
        _continueSignal.Set();
    }

    public void Disconnect()
    {
        _stepMode = StepMode.None;
        _paused = false;
        _pausedTask = null;
        _continueSignal.Set();
    }

    private bool EvaluateCondition(string condition)
    {
        try
        {
            var result = Evaluate(condition);
            return result != null && result.Equals("TRUE", StringComparison.OrdinalIgnoreCase);
        }
        catch { return false; }
    }

    private object? EvaluateSymbol(string name)
    {
        var locals = _pausedTask?.Frame?.LocalVariables;
        if (locals != null)
        {
            for (int i = 0; i < locals.Length; i++)
            {
                if (locals[i] is string s && s.Equals(name, StringComparison.OrdinalIgnoreCase))
                    return locals[i + 1 < locals.Length ? i + 1 : i];
            }
        }
        var globals = _pausedTask?.Globals;
        if (globals != null)
        {
            for (int i = 0; i < globals.Length; i++)
            {
                if (globals[i] != null)
                {
                    var g = globals[i];
                    if (g is ByRefBox box) g = box.Value;
                    var gname = ResolveGlobalName(i);
                    if (gname != null && name.Equals(gname, StringComparison.OrdinalIgnoreCase))
                        return g;
                }
            }
        }
        return null;
    }

    private static int GetCallDepth(PCodeTask task)
    {
        int depth = 0;
        var frame = task.Frame;
        while (frame != null) { depth++; frame = frame.Previous; }
        return depth;
    }

    private int ResolveLine(int ip)
    {
        if (_sourceLines == null || ip < 0 || ip >= _sourceLines.Count) return 0;
        return _sourceLines[ip] + 1;
    }

    private List<StackFrame> CaptureStackFrames(PCodeTask task)
    {
        var frames = new List<StackFrame>();
        var frame = task.Frame;
        int id = 0;
        while (frame != null)
        {
            var name = ResolveName(frame.ReturnIp);
            frames.Add(new StackFrame
            {
                Id = id++,
                Name = name,
                Ip = frame.ReturnIp,
                Line = ResolveLine(frame.ReturnIp),
                Locals = CaptureLocals(frame)
            });
            frame = frame.Previous;
        }
        return frames;
    }

    private string ResolveName(int ip)
    {
        if (_ipToName == null) return $"Frame (ip={ip})";
        int bestIp = -1;
        string? bestName = null;
        foreach (var (addr, name) in _ipToName)
        {
            if (addr <= ip && addr > bestIp) { bestIp = addr; bestName = name; }
        }
        return bestName ?? $"Frame (ip={ip})";
    }

    private string? ResolveGlobalName(int index)
    {
        if (_ipToName == null) return null;
        foreach (var (ip, name) in _ipToName)
        {
            if (ip == index) return name;
        }
        return $"global_{index}";
    }

    private static PCodeCallFrame? GetFrame(PCodeTask task, int frameId)
    {
        var frame = task.Frame;
        int id = 0;
        while (frame != null)
        {
            if (id == frameId) return frame;
            frame = frame.Previous;
            id++;
        }
        return null;
    }

    private static List<DebugVariable> CaptureLocals(PCodeCallFrame frame)
    {
        var vars = new List<DebugVariable>();
        if (frame.LocalVariables == null) return vars;
        for (int i = 0; i < frame.LocalVariables.Length; i++)
        {
            var val = frame.LocalVariables[i];
            if (val != null)
                vars.Add(new DebugVariable { Name = $"local_{i}", Value = val.ToString() ?? "null", Type = val.GetType().Name });
        }
        return vars;
    }

    private class Breakpoint
    {
        public int Line { get; set; }
        public string? Condition { get; set; }
        public string? FunctionName { get; set; }
    }
}
