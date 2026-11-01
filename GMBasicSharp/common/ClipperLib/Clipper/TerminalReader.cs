using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Text;

namespace ClipperLib.Clipper;

public static class TerminalReader
{
    /// <summary>Sorgente tasti iniettabile (default console). L'host può sostituirla quando
    /// esiste un <c>IKeyboardService</c> (vedi TODO TerminalReader-Via-KeyboardService).</summary>
    private static IClipperKeySource K => ClipperKeyInput.Current;

    public static bool IsEventAvailable() => K.KeyAvailable;

    public static bool IsEventAvailable(int timeoutMs)
    {
        if (timeoutMs <= 0) return K.KeyAvailable;
        if (K.KeyAvailable) return true;
        Thread.Sleep(10);
        return K.KeyAvailable;
    }

    public static UiEvent ReadEventCore()
    {
        if (K.InputRedirected) return new UiNone();

        EnableMouseCaptureIfNeeded();

        var ki = K.ReadKey(true);
        var mods = MapModifiers(ki.Modifiers);

        // ESC — could be standalone key or start of CSI/mouse sequence.
        // Wait up to 60ms for the rest of the sequence to arrive (mouse
        // escape sequences can be delayed on slow terminals / SSH).
        if (ki.Key == ConsoleKey.Escape)
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            while (sw.ElapsedMilliseconds < 60 && !K.KeyAvailable)
                Thread.Sleep(2);
            if (!K.KeyAvailable)
                return new UiKey(KeyConstants.K_ESC, mods);

            var seq = CollectEscSequence();
            if (MouseDecoder.DecodeAndProcess(seq) is MouseAction ma)
            {
                var st = ClipperGlobal.State;
                st.MouseX = ma.X;
                st.MouseY = ma.Y;
                st.MouseDoubleClicked = ma.Kind == MouseActionKind.DoubleClick;
                for (var i = 0; i < 3; i++)
                {
                    st.MouseShift[i] = (ma.Modifiers & KeyModifiers.Shift) != 0;
                    st.MouseCtrl[i] = (ma.Modifiers & KeyModifiers.Control) != 0;
                }
                // Dispatch al gestore eventi unificato (ON MOUSE): il mouse TUI alimenta
                // gli handler registrati via RegisterMouse.
                try
                {
                    var mk = ma.Kind switch
                    {
                        MouseActionKind.Click or MouseActionKind.RightClick or MouseActionKind.MiddleClick => 0, // Click
                        MouseActionKind.DoubleClick => 1, // DblClick
                        MouseActionKind.Release => 3,     // Up
                        MouseActionKind.Drag => 4,        // Move
                        _ => 5                            // Scroll
                    };
                    CommonLib.App.EventDispatcher.Instance.DispatchMouse(mk, ma.X, ma.Y);
                }
                catch { }
                return new UiMouse(ma);
            }

            if (seq.StartsWith("\x1b[200~"))
            {
                // Bracketed paste (CSI 200~ ... CSI 201~): il testo incollato arriva come
                // tasti normali; segnala l'evento Paste all'EventDispatcher (ON WINDOW PASTE).
                try { CommonLib.App.EventDispatcher.Instance.DispatchWindow(8); } catch { }
                return new UiKey(KeyConstants.K_ESC, mods);
            }

            if (seq.Length >= 2 && seq[1] == '[')
            {
                var key = DecodeCsiKey(seq);
                if (key > 0) return new UiKey(key, mods);
            }

            return new UiKey(KeyConstants.K_ESC, mods);
        }

        // All other keys — use ki.Key and ki.Modifiers for correct detection (Shift+Tab etc.)
        var clipperKey = MapClipperKey(ki);
        return new UiKey(clipperKey, mods);
    }

    private static string CollectEscSequence()
    {
        var sb = new StringBuilder("\x1b");
        for (var retry = 0; retry < 10 && sb.Length < 64; retry++)
        {
            if (!K.KeyAvailable)
            {
                var s = sb.ToString();
                if (s.Length > 1)
                {
                    if (s.StartsWith("\x1b[<"))
                    {
                        var end = s.IndexOfAny(['M', 'm']);
                        if (end >= 0) return s[..(end + 1)];
                    }
                    if (s.StartsWith("\x1b[M") && s.Length >= 6)
                        return s[..6];
                    var lc = s[^1];
                    if (lc is >= 'a' and <= 'z' or >= 'A' and <= 'Z')
                        return s;
                }
                Thread.Sleep(4);
                continue;
            }
            while (K.KeyAvailable)
            {
                var next = K.ReadKey(true);
                sb.Append(next.KeyChar);
                retry = 0;

                var s = sb.ToString();
                if (s.StartsWith("\x1b[<"))
                {
                    var end = s.IndexOfAny(['M', 'm']);
                    if (end >= 0) return s[..(end + 1)];
                }
                if (s.StartsWith("\x1b[M") && s.Length >= 6)
                    return s[..6];
                var lc = s[^1];
                if (lc is >= 'a' and <= 'z' or >= 'A' and <= 'Z')
                    return s;
            }
        }
        return sb.ToString();
    }

    private static int MapClipperKey(ConsoleKeyInfo ki)
    {
        switch (ki.Key)
        {
            case ConsoleKey.Enter: return KeyConstants.K_ENTER;
            case ConsoleKey.Escape: return KeyConstants.K_ESC;
            case ConsoleKey.Tab:
                return (ki.Modifiers & ConsoleModifiers.Shift) != 0
                    ? KeyConstants.K_BTAB : KeyConstants.K_TAB;
            case ConsoleKey.Backspace: return KeyConstants.K_BS;
            case ConsoleKey.Delete: return KeyConstants.K_DEL;
            case ConsoleKey.LeftArrow: return KeyConstants.K_LEFT;
            case ConsoleKey.RightArrow: return KeyConstants.K_RIGHT;
            case ConsoleKey.UpArrow: return KeyConstants.K_UP;
            case ConsoleKey.DownArrow: return KeyConstants.K_DOWN;
            case ConsoleKey.Home: return KeyConstants.K_HOME;
            case ConsoleKey.End: return KeyConstants.K_END;
            case ConsoleKey.PageUp: return KeyConstants.K_PGUP;
            case ConsoleKey.PageDown: return KeyConstants.K_PGDN;
            case ConsoleKey.Insert: return KeyConstants.K_INS;
            case ConsoleKey.F1: return KeyConstants.K_F1;
            case ConsoleKey.F2: return KeyConstants.K_F2;
            case ConsoleKey.F3: return KeyConstants.K_F3;
            case ConsoleKey.F4: return KeyConstants.K_F4;
            case ConsoleKey.F5: return KeyConstants.K_F5;
            case ConsoleKey.F6: return KeyConstants.K_F6;
            case ConsoleKey.F7: return KeyConstants.K_F7;
            case ConsoleKey.F8: return KeyConstants.K_F8;
            case ConsoleKey.F9: return KeyConstants.K_F9;
            case ConsoleKey.F10: return KeyConstants.K_F10;
            case ConsoleKey.F11: return KeyConstants.K_F11;
            case ConsoleKey.F12: return KeyConstants.K_F12;
            default:
                var ch = ki.KeyChar;
                if (ch >= 32 && ch <= 126) return ch;
                // Ctrl/Alt+lettera: ConsoleKey espone la lettera, ma KeyChar è il codice
                // di controllo (1..26). Ritorna la lettera maiuscola in modo che gli
                // acceleratori (es. Ctrl+N) raggiungano l'app; i Modifiers distinguono
                // dal tasto nudo. Es.: Ctrl+N → 'N' (78) con Modifiers.Control.
                if (ki.Key is >= ConsoleKey.A and <= ConsoleKey.Z)
                {
                    var letter = 'A' + (ki.Key - ConsoleKey.A);
                    return letter;
                }
                return 0;
        }
    }

    private static KeyModifiers MapModifiers(ConsoleModifiers m)
    {
        var mods = KeyModifiers.None;
        if ((m & ConsoleModifiers.Shift) != 0) mods |= KeyModifiers.Shift;
        if ((m & ConsoleModifiers.Alt) != 0) mods |= KeyModifiers.Alt;
        if ((m & ConsoleModifiers.Control) != 0) mods |= KeyModifiers.Control;
        return mods;
    }

    private static int DecodeCsiKey(string seq)
    {
        if (seq.Length < 2) return 0;
        return seq[^1] switch
        {
            'A' => KeyConstants.K_UP,
            'B' => KeyConstants.K_DOWN,
            'C' => KeyConstants.K_RIGHT,
            'D' => KeyConstants.K_LEFT,
            'H' => KeyConstants.K_HOME,
            'F' => KeyConstants.K_END,
            'Z' => KeyConstants.K_BTAB,
            _ => 0,
        };
    }

    private static void EnableMouseCaptureIfNeeded() => MouseCapture.Enable();
}
