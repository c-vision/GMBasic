using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

/// <summary>
/// <see cref="IInputSource"/> che unifica il canale input legacy: rispetta per
/// primo <see cref="ClipperState.KeySource"/> e <see cref="ClipperState.Typeahead"/>
/// (canale headless/test), poi la console reale tramite <see cref="TerminalReader"/>
/// (che già restituisce <see cref="UiEvent"/> e aggiorna MouseX/MouseY).
/// </summary>
public sealed class ClipperInputSource : IInputSource
{
    private static ClipperState S() => ClipperGlobal.State;

    public UiEvent Read(int timeoutMs)
    {
        var st = S();

        if (st.PendingKey.HasValue)
        {
            var pk = st.PendingKey.Value;
            st.PendingKey = null;
            st.LastKey = pk;
            st.ReadKey = pk;
            return new UiKey(pk, KeyModifiers.None);
        }

        if (st.KeySource != null)
        {
            var c = st.KeySource();
            st.LastKey = c;
            st.ReadKey = c;
            return WithMouseState(ToEvent(c));
        }

        if (st.Typeahead.Count > 0)
        {
            var b = st.Typeahead.Dequeue();
            var c = ByteToInkey(b);
            st.LastKey = c;
            st.ReadKey = c;
            return new UiKey(c, KeyModifiers.None);
        }

        if (Clipper.ClipperKeyInput.Current.InputRedirected)
            return new UiNone();

        MouseCapture.Enable();

        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (timeoutMs < 0 || sw.ElapsedMilliseconds < timeoutMs)
        {
            if (Clipper.ClipperKeyInput.Current.KeyAvailable)
                return WithMouseState(TerminalReader.ReadEventCore());
            System.Threading.Thread.Sleep(10);
        }
        return new UiNone();
    }

    public void Dispose() { }

    /// <summary>Aggiorna lo stato mouse quando l'evento è un <see cref="UiMouse"/>.</summary>
    private static UiEvent WithMouseState(UiEvent ev)
    {
        if (ev is UiMouse um)
            ClipperMouse.UpdateFromEvent(um.Action);
        return ev;
    }

    /// <summary>Converte un codice INKEY legacy in <see cref="UiEvent"/> (mouse ⇄ UiMouse).</summary>
    private static UiEvent ToEvent(int key)
    {
        if (key <= KeyConstants.K_MOUSE_BASE && MouseAction.FromInkeyCode(key) is MouseAction ma)
            return new UiMouse(ma);
        return new UiKey(key, KeyModifiers.None);
    }

    private static int ByteToInkey(byte b) => b switch
    {
        (byte)'\n' or (byte)'\r' => KeyConstants.K_ENTER,
        27 => KeyConstants.K_ESC,
        9 => KeyConstants.K_TAB,
        127 => KeyConstants.K_BS,
        _ => b
    };
}
