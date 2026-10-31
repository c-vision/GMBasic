using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.App;
using static ClipperLib.Clipper.KeyConstants;

namespace ClipperLib.Clipper;

public static class ClipperInput
{
    private static ClipperState S() => ClipperGlobal.State;

    private static readonly ClipperInputSource Source = new();

    public static void DisableMouseCapture() => MouseCapture.Disable();

    /// <summary>
    /// Abilita il tracciamento completo del mouse (pressione + rilascio + movimento),
    /// utile per interfacce che rispondono all'hover come un menu. Richiede che il
    /// programma gestisca un flusso fitto di eventi di movimento.
    /// </summary>
    public static void EnableMouseMotion() => MouseCapture.EnableMotion();

    public static int InKey()
    {
        var evt = Source.Read(-1);
        return KeyFromEvent(evt, 0);
    }

    public static int InKeyTimeout(int ms)
    {
        var evt = Source.Read(ms);
        return KeyFromEvent(evt, -1);
    }

    public static (int key, KeyModifiers mods) InKeyTimeoutEx(int ms)
    {
        var evt = Source.Read(ms);
        switch (evt)
        {
            case UiKey uk: return (uk.Key, uk.Modifiers);
            case UiMouse um: return (um.Action.InkeyCode, KeyModifiers.None);
            default: return (-1, KeyModifiers.None);
        }
    }

    private static int KeyFromEvent(UiEvent evt, int fallback)
    {
        switch (evt)
        {
            case UiKey uk: return uk.Key;
            case UiMouse um: return um.Action.InkeyCode;
            default: return fallback;
        }
    }

    public static int LastKey() => S().LastKey;
    public static int ReadKey() => S().ReadKey;

    /// <summary>
    /// NEXTKEY() — prossimo tasto che INKEY() restituirebbe, senza consumarlo.
    /// Non blocca: legge da KeySource (mettendolo in coda) o dal typeahead, altrimenti 0.
    /// </summary>
    public static int NextKey()
    {
        var st = S();
        if (st.PendingKey.HasValue) return st.PendingKey.Value;
        if (st.KeySource != null)
        {
            var c = st.KeySource();
            st.PendingKey = c;
            return c;
        }
        if (st.Typeahead.Count > 0)
        {
            var b = st.Typeahead.Peek();
            return b switch
            {
                (byte)'\n' or (byte)'\r' => KeyConstants.K_ENTER,
                27 => KeyConstants.K_ESC,
                9 => KeyConstants.K_TAB,
                127 => KeyConstants.K_BS,
                _ => b,
            };
        }
        return 0;
    }

    public static void Keyboard(string s)
    {
        var st = S();
        foreach (var b in System.Text.Encoding.ASCII.GetBytes(s))
            st.Typeahead.Enqueue(b);
    }

    public static void SetKey(int n, Action proc)
    {
        S().SetKeys[n] = proc;
    }

    public static void SetKeySource(Func<int> src)
    {
        S().KeySource = src;
    }

    /// <summary>Azzera la sorgente di tasti sintetica (headless/test). Senza questo,
    /// una KeySource impostata da un demo headless resta attiva e blocca mouse/tasti
    /// reali per tutti i controlli successivi.</summary>
    public static void ClearKeySource()
    {
        S().KeySource = null;
    }

    public static void SetRenderHook(Action<ScreenBuffer> hook)
    {
        S().RenderHook = hook;
    }

    public static bool Accept(ref string buf)
    {
        buf = "";
        var startRow = S().Row;
        var startCol = S().Col;

        while (true)
        {
            var st = S();
            var c = st.Col;
            var r = st.Row;
            var w = st.Buffer.Width;
            var fg = st.Fg;
            var bg = st.Bg;

            for (var x = c; x < w; x++)
                st.Buffer.SetChar(x, r, ' ', fg, bg);
            st.Buffer.SetStringN(c, r, buf, int.MaxValue, fg, bg);
            st.InvokeRenderHook();

            var k = InKey();
            if (k == KeyConstants.K_ENTER) return true;
            if (k == KeyConstants.K_ESC) return false;
            if (k == KeyConstants.K_BS) { if (buf.Length > 0) buf = buf[..^1]; }
            else if (k is >= 32 and <= 126) buf += (char)k;

            st.Row = startRow;
            st.Col = startCol;
        }
    }

    public static int Wait(double? secs) => InKey();
}
