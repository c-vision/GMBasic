using CommonLib.Screens;
using CommonLib.Consoles;
using CommonLib.Clipboard;
// FILE: Interpreter.Keyboard.Headless.cs

using System.Runtime.InteropServices;

namespace CommonLib.Keyboard;

/// <summary>
/// Implementazione "headless" di IKeyboardService per esecuzione automatica.
/// Le operazioni di lettura bloccanti completano immediatamente con valori
/// di default. Hook di sistema: SIGINT (CTRL-C) via System.Runtime.InteropServices.PosixSignalRegistration
/// (funziona su tutti i sistemi, incluso macOS dove Console.CancelKeyPress e' inaffidabile con kill -INT).
/// </summary>
public sealed class HeadlessKeyboardService : IKeyboardService, IDisposable
{
    private readonly Queue<string> _inputQueue = new();
    private readonly Queue<ConsoleKeyInfo> _keyQueue = new();
    private readonly object _lock = new();
    private readonly CancellationTokenSource? _breakCts;
    private readonly IDisposable? _sigIntReg;

    public HeadlessKeyboardService()
    {
        _breakCts = new CancellationTokenSource();
        try
        {
            // .NET 6+ API nativa per signal handling — funziona su macOS, Linux, Windows
            _sigIntReg = PosixSignalRegistration.Create(System.Runtime.InteropServices.PosixSignal.SIGINT, OnPosixSignal);
        }
        catch (Exception)
        {
            // Fallback per ambienti dove PosixSignalRegistration non e' disponibile
            Console.CancelKeyPress += OnCancelKeyPressLegacy;
        }
    }

    public IBlockingRequest? ActiveBlockingRequest => null;

    public event Action? BreakRequested;

    private void OnPosixSignal(System.Runtime.InteropServices.PosixSignalContext ctx)
    {
        ctx.Cancel = true;
        BreakRequested?.Invoke();
        _breakCts?.Cancel();
    }

    private void OnCancelKeyPressLegacy(object? sender, ConsoleCancelEventArgs e)
    {
        e.Cancel = true;
        BreakRequested?.Invoke();
        _breakCts?.Cancel();
    }

    public void QueueInput(string line)
    {
        lock (_lock) { _inputQueue.Enqueue(line); }
    }

    /// <summary>Alias per QueueInput (compatibilità BRT).</summary>
    public void QueueLine(string line) { lock (_lock) { _inputQueue.Enqueue(line); } }

    public void QueueInput(params string[] lines)
    {
        lock (_lock) { foreach (var l in lines) _inputQueue.Enqueue(l); }
    }

    /// <summary>
    /// Accoda un singolo tasto nel buffer per GET/INKEY.
    /// </summary>
    public void QueueKey(char c)
    {
        lock (_lock) { _keyQueue.Enqueue(new ConsoleKeyInfo(c, (ConsoleKey)c, false, false, false)); }
    }

    /// <summary>
    /// Accoda una stringa: ogni carattere diventa un tasto; \r termina la riga corrente.
    /// Usato da CONSOLE.SENDKEY.
    /// </summary>
    public void QueueInputString(string keys)
    {
        lock (_lock)
        {
            var currentLine = new System.Text.StringBuilder();
            foreach (var c in keys)
            {
                if (c == '\r' || c == '\n')
                {
                    if (currentLine.Length > 0)
                    {
                        _inputQueue.Enqueue(currentLine.ToString());
                        currentLine.Clear();
                    }
                }
                else
                {
                    _keyQueue.Enqueue(new ConsoleKeyInfo(c, (ConsoleKey)c, false, false, false));
                    currentLine.Append(c);
                }
            }
            if (currentLine.Length > 0)
                _inputQueue.Enqueue(currentLine.ToString());
        }
    }

    public void Start()
    {
    }

    public void Stop()
    {
        _sigIntReg?.Dispose();
        try { Console.CancelKeyPress -= OnCancelKeyPressLegacy; } catch { }
    }

    public ConsoleKeyInfo? TryReadKey(bool convertToUppercase)
    {
        lock (_lock)
        {
            if (_keyQueue.Count > 0)
                return _keyQueue.Dequeue();
        }
        return null;
    }

    public async ValueTask<ConsoleKeyInfo> ReadKeyAsync(CancellationToken cancellationToken = default,
        bool convertToUppercase = false)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            var key = TryReadKey(convertToUppercase);
            if (key.HasValue)
                return key.Value;
            await Task.Delay(10, cancellationToken);
        }
        return new ConsoleKeyInfo(' ', ConsoleKey.Spacebar, false, false, false);
    }

    public Task<string?> ReadLineAsync(IConsole console, string prompt,
        Func<char, string, (bool process, char convertedChar)>? charConversionRule,
        CancellationToken cancellationToken = default)
    {
        lock (_lock)
        {
            if (_inputQueue.Count > 0)
                return Task.FromResult<string?>(_inputQueue.Dequeue());
        }
        return Task.FromResult<string?>(string.Empty);
    }

    public void RegisterHook(KeyCombination keyCombination, Action action)
    {
    }

    public void UnregisterHook(KeyCombination keyCombination)
    {
    }

    public void UnregisterAllHooks()
    {
    }

    public bool IsHookRegistered(KeyCombination keyCombination) => false;

    public ValueTask ClearBufferAsync() => ValueTask.CompletedTask;

    public void Pause()
    {
    }

    public void Resume()
    {
    }

    public Task ResetAsync() => Task.CompletedTask;

    public void Dispose()
    {
        Stop();
        _breakCts?.Dispose();
    }

    public TerminalKeyInfo ReadKey()
    {
        // Legge dalla coda se c'è un tasto iniettato (test headless con QueueKey/QueueInputString);
        // altrimenti default non-bloccante (tasto nullo). La VM sincrona usa questo metodo.
        var k = TryReadKey(convertToUppercase: false);
        return k.HasValue
            ? new TerminalKeyInfo(k.Value.KeyChar, (int)k.Value.Key, k.Value.Modifiers.HasFlag(ConsoleModifiers.Shift),
                k.Value.Modifiers.HasFlag(ConsoleModifiers.Alt), k.Value.Modifiers.HasFlag(ConsoleModifiers.Control))
            : new TerminalKeyInfo('\0', 0, false, false, false);
    }
    public TerminalKeyInfo? ReadKey(int timeoutMs)
    {
        var k = TryReadKey(convertToUppercase: false);
        return k.HasValue
            ? new TerminalKeyInfo(k.Value.KeyChar, (int)k.Value.Key, k.Value.Modifiers.HasFlag(ConsoleModifiers.Shift),
                k.Value.Modifiers.HasFlag(ConsoleModifiers.Alt), k.Value.Modifiers.HasFlag(ConsoleModifiers.Control))
            : null;
    }
    public string? ReadLine(string? prompt)
    {
        // Legge dalla coda se c'è una riga iniettata (test headless con QueueInput/QueueLine);
        // altrimenti default (null) — la VM sincrona usa questo metodo.
        lock (_lock)
        {
            if (_inputQueue.Count > 0)
                return _inputQueue.Dequeue();
        }
        return null;
    }
    public TerminalKeyInfo? TryReadKey()
    {
        var k = TryReadKey(convertToUppercase: false);
        return k.HasValue
            ? new TerminalKeyInfo(k.Value.KeyChar, (int)k.Value.Key, k.Value.Modifiers.HasFlag(ConsoleModifiers.Shift),
                k.Value.Modifiers.HasFlag(ConsoleModifiers.Alt), k.Value.Modifiers.HasFlag(ConsoleModifiers.Control))
            : null;
    }
    /// <summary>SendKey da BASIC (CONSOLE.SENDKEY): accoda come QueueKey — prima no-op
    /// (hang: GET/INKEY/WAITKEY in attesa per sempre su gamba PCode/headless).</summary>
    public void SendKey(char key) { QueueKey(key); }
}