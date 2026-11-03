namespace ILOptim;

public class ConsoleLogger : ICompilationLogger
{
    public LogLevel MinLevel { get; set; } = LogLevel.Info;

    readonly ArrayStack<(LoggerScopeInfo Info, string? Message, int SyncId, LogLevel MinLevel)> _scopeStack = new();
    int _scopeSyncId;
    int _lastScopeSyncId;

    public void Log(LogLevel level, ReadOnlySpan<char> msg, Exception? exception)
    {
        // Diagnostica ILOptim (Warn/Error/Fatal) su STDERR: non deve sporcare l'output del
        // programma (stdout), altrimenti la gamba IL diverge dalle altre. Vale anche per i
        // MESSAGGI DI SCOPE ("Apply passes to …"): prima andavano sempre su stdout e un
        // errore di un pass stampava quella riga nello stdout del programma (divergenza
        // parity4 su lambda_nested_capture).
        var sink = level >= LogLevel.Warn ? Console.Error : Console.Out;
        SyncScopes(sink);

        Console.ForegroundColor = level switch {
            LogLevel.Trace => ConsoleColor.DarkGray,
            LogLevel.Debug => ConsoleColor.Cyan,
            LogLevel.Info  => ConsoleColor.White,
            LogLevel.Warn  => ConsoleColor.Yellow,
            LogLevel.Error => ConsoleColor.Red,
            LogLevel.Fatal => ConsoleColor.DarkRed,
            _ => ConsoleColor.White,
        };
        if (exception != null) {
            msg = msg.ToString() + "\n\n" + exception.ToString();
        }
        // (sink calcolato sopra: Info/Trace → stdout, Warn/Error/Fatal → stderr.)
        foreach (var line in msg.EnumerateLines()) {
            if (!line.IsEmpty) Indent(_scopeStack.Count, sink);
            sink.WriteLine(line);
        }
        Console.ResetColor();
    }

    private void SyncScopes(TextWriter sink)
    {
        int minDepth = _scopeStack.Count;
        while (minDepth > 0 && _lastScopeSyncId < _scopeStack[minDepth - 1].SyncId) {
            minDepth--;
        }
        for (int i = minDepth; i < _scopeStack.Count; i++) {
            ref var scope = ref _scopeStack[i];

            Indent(i, sink);
            if (MinLevel == LogLevel.Trace) {
                Console.ForegroundColor = ConsoleColor.DarkGreen;
                sink.Write($"[{scope.Info.Name}] ");
            }
            if (scope.Message != null) {
                Console.ForegroundColor = ConsoleColor.Gray;
                sink.WriteLine(scope.Message);
            }
        }
        _lastScopeSyncId = _scopeSyncId;
    }

    private void Indent(int depth) => Indent(depth, Console.Out);
    private void Indent(int depth, TextWriter sink)
    {
        for (int i = 0; i < depth; i++) {
            sink.Write("  ");
        }
    }

    public bool IsEnabled(LogLevel level)
    {
        return level >= MinLevel && (_scopeStack.IsEmpty || level >= _scopeStack.Top.MinLevel);
    }

    public LoggerScopeHandle Push(in LoggerScopeInfo info, string? msg = null)
    {
        Ensure.That(_scopeStack.Count < 128, "Too many log scopes, forgot to call Pop()?");

        _scopeSyncId++;
        _scopeStack.Push((info, msg, _scopeSyncId, LogLevel.Trace));
        return new LoggerScopeHandle(this, _scopeSyncId);
    }
    public void Pop(in LoggerScopeHandle handle)
    {
        var scope = _scopeStack.Pop();
        Ensure.That(scope.SyncId == handle.SyncId, "Unsynchronized scope stack");
    }
}

public class VoidLogger : ICompilationLogger
{
    public LogLevel MinLevel => LogLevel.Fatal;

    public void Log(LogLevel level, ReadOnlySpan<char> msg, Exception? exception) { }
    public bool IsEnabled(LogLevel level) => false;

    public LoggerScopeHandle Push(in LoggerScopeInfo info, string? msg = null) => default;
    public void Pop(in LoggerScopeHandle scope) { }
}