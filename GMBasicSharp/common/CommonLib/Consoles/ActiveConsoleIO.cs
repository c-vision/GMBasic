using CommonLib.Keyboard;

namespace CommonLib.Consoles;

/// <summary>
/// Console ATTIVA globale: l'UNICO punto di I/O del sistema. La console non è mai fissa
/// (SDL / Standard / System / Headless), quindi nessun componente (REPL, HelpForm,
/// ClipperInput, ConsoleRenderer, IDE, menu) deve usare <c>System.Console</c> in diretta:
/// passa da qui. <c>System.Console</c> resta SOLO dentro <see cref="StandardConsole"/> e
/// <see cref="SystemConsole"/>. Se la console attiva è null → fallback System.Console
/// (avvio prima del boot della console).
/// </summary>
public static class ActiveConsoleIO
{
    /// <summary>Console attiva (IConsole: Sdl/Standard/System). Null → System.Console.</summary>
    public static IConsole? Console { get; set; }

    /// <summary>Keyboard service della console attiva (SDL/Standard). Null → Console.ReadKey.</summary>
    public static IKeyboardService? Keyboard { get; set; }

    /// <summary>Pompa l'event-loop esterno (SDL: PollEvents) + Present. Settata dalla REPL.</summary>
    public static Action? Pump { get; set; }

    /// <summary>Chiusura richiesta (es. click sul pulsante di chiusura SDL). I loop la controllano.</summary>
    public static volatile bool QuitRequested;

    public static bool HasConsole => Console != null;

    public static TextWriter Out => Console?.Out ?? System.Console.Out;
    public static TextWriter Error => Console?.Error ?? System.Console.Error;

    public static int WindowWidth { get { try { return Console?.WindowWidth ?? System.Console.WindowWidth; } catch { return 80; } } }
    public static int WindowHeight { get { try { return Console?.WindowHeight ?? System.Console.WindowHeight; } catch { return 25; } } }

    public static bool CursorVisible
    {
        get { try { return Console != null ? Console.CursorVisible : System.Console.CursorVisible; } catch { return true; } }
        set { try { if (Console != null) Console.CursorVisible = value; else System.Console.CursorVisible = value; } catch { } }
    }

    public static void SetCursorPosition(int left, int top)
    {
        try { if (Console != null) Console.SetCursorPosition(left, top); else System.Console.SetCursorPosition(left, top); } catch { }
    }

    public static void Clear()
    {
        try { if (Console != null) Console.Clear(); else System.Console.Clear(); } catch { }
    }

    public static void Present() { try { Console?.Present(); } catch { } }

    /// <summary>
    /// Legge un tasto. <paramref name="timeoutMs"/> &lt; 0 = attende indefinitamente
    /// (pompando l'event-loop esterno); 0 = non-bloccante; &gt; 0 = fino a timeout.
    /// </summary>
    public static ConsoleKeyInfo? ReadKey(int timeoutMs)
    {
        long deadline = timeoutMs < 0 ? long.MaxValue : Environment.TickCount64 + timeoutMs;
        while (true)
        {
            if (Keyboard != null)
            {
                var k = Keyboard.TryReadKey(false);
                if (k.HasValue) return k;
            }
            else
            {
                try { if (System.Console.KeyAvailable) return System.Console.ReadKey(true); } catch { }
                if (timeoutMs == 0) return null;
            }
            Pump?.Invoke();
            if (Environment.TickCount64 >= deadline) return null;
            if (Keyboard != null) Thread.Sleep(2);
        }
    }
}
