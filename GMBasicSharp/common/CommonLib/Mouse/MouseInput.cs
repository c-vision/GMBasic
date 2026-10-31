// FILE: CommonLib.Mouse.MouseInput.cs

using System.Collections.Concurrent;

namespace CommonLib;

public static class MouseInput
{
    public static int LastX { get; private set; }
    public static int LastY { get; private set; }
    public static int LastButtons { get; private set; }
    public static int LastScrollDelta { get; private set; }
    public static bool Enabled { get; private set; }
    public static bool HasMoved { get; private set; }

    public static void Enable()
    {
        if (Enabled) return;
        Console.Write("\x1b[?1000h\x1b[?1003h\x1b[?1006h");
        Console.Out.Flush();
        Enabled = true;
        StartBackgroundReader();
    }

    public static void Disable()
    {
        if (!Enabled) return;
        Console.Write("\x1b[?1000l\x1b[?1003l\x1b[?1006l");
        Console.Out.Flush();
        Enabled = false;
        StopBackgroundReader();
    }

    private static Thread? _readerThread;
    private static CancellationTokenSource? _cts;
    private static readonly ConcurrentQueue<(int x, int y, int buttons, int scroll)> _eventQueue = new();

    private static void StartBackgroundReader()
    {
        _cts = new CancellationTokenSource();
        _readerThread = new Thread(() => ReaderLoop(_cts.Token))
        {
            IsBackground = true,
            Name = "MouseInputReader"
        };
        _readerThread.Start();
    }

    private static void StopBackgroundReader()
    {
        _cts?.Cancel();
        _readerThread?.Join(500);
        _cts?.Dispose();
        _cts = null;
        _readerThread = null;
    }

    private static void ReaderLoop(CancellationToken ct)
    {
        var stdin = Console.OpenStandardInput();
        var buf = new byte[64];
        while (!ct.IsCancellationRequested)
        {
            try
            {
                int read = stdin.Read(buf, 0, buf.Length);
                if (read > 0)
                    ProcessInput(buf, read);
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch
            {
                Thread.Sleep(10);
            }
        }
    }

    private static void ProcessInput(byte[] buffer, int count)
    {
        for (int i = 0; i < count; i++)
        {
            if (buffer[i] == 0x1b && i + 3 < count && buffer[i + 1] == '[' && buffer[i + 2] == '<')
            {
                int j = i + 3;
                var parts = new List<int>();
                int current = 0;
                while (j < count && buffer[j] != 'M' && buffer[j] != 'm')
                {
                    if (buffer[j] == ';')
                    {
                        parts.Add(current);
                        current = 0;
                    }
                    else if (buffer[j] >= '0' && buffer[j] <= '9')
                    {
                        current = current * 10 + (buffer[j] - '0');
                    }
                    else break;
                    j++;
                }
                if (j < count && (buffer[j] == 'M' || buffer[j] == 'm') && parts.Count >= 3)
                {
                    int cb = parts[0];
                    int cx = parts[1];
                    int cy = parts[2];
                    bool release = buffer[j] == 'm';

                    int buttons = 0;
                    int scroll = 0;
                    if ((cb & 0x40) != 0)
                    {
                        scroll = (cb & 1) == 0 ? 1 : -1;
                    }
                    else if (release)
                    {
                        buttons = 0;
                    }
                    else
                    {
                        if ((cb & 1) != 0) buttons |= 1;
                        if ((cb & 2) != 0) buttons |= 2;
                        if ((cb & 4) != 0) buttons |= 4;
                    }

                    _eventQueue.Enqueue((cx, cy, buttons, scroll));
                    i = j;
                }
            }
        }
    }

    public static void PumpEvents()
    {
        while (_eventQueue.TryDequeue(out var evt))
        {
            LastX = evt.x;
            LastY = evt.y;
            HasMoved = true;
            if (evt.buttons != 0 || evt.scroll != 0)
                LastButtons = evt.buttons;
            if (evt.scroll != 0)
                LastScrollDelta = evt.scroll;
        }
    }

    // --- Polimorfici (funzionano anche in TUI, senza SDL) ---

    /// <summary>Limite del movimento del mouse (null = illimitato). Applicato a SetPosition.</summary>
    private static (int x1, int y1, int x2, int y2)? _limit;

    public static void Show() { try { Console.Write("\x1b[?25h"); Console.Out.Flush(); } catch { } }
    public static void Hide() { try { Console.Write("\x1b[?25l"); Console.Out.Flush(); } catch { } }

    public static void SetPosition(int x, int y)
    {
        if (_limit is { } lim)
        {
            x = Math.Clamp(x, lim.x1, lim.x2);
            y = Math.Clamp(y, lim.y1, lim.y2);
        }
        LastX = x; LastY = y;
    }

    public static void Limit(int x1, int y1, int x2, int y2)
    {
        _limit = (Math.Min(x1, x2), Math.Min(y1, y2), Math.Max(x1, x2), Math.Max(y1, y2));
    }

    public static void Unlimit() => _limit = null;

    public static void SetCursor(int cursorId)
    {
        // Forma del cursore: 0 = default/blocco, 1 = linea, 2 = barra (ANSI DECSCUSR)
        var code = cursorId switch { 1 => "\x1b[4 q", 2 => "\x1b[6 q", _ => "\x1b[0 q" };
        try { Console.Write(code); Console.Out.Flush(); } catch { }
    }
}
