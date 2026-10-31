// FILE: CommonGraphics.Graphics.IlSdlInput
using System.Text;
using CommonGraphics.Consoles;
using SDL2;

namespace CommonGraphics.Graphics;

/// <summary>
/// Ponte SDL → programma IL: tasti verso INKEY, X della finestra e Ctrl+C verso abort.
/// Non tocca il rendering (nessun RaiseWindow, nessuno scale).
/// </summary>
public static class IlSdlInput
{
    private static bool _bound;

    /// <summary>Collega eventi SDL una sola volta per processo.</summary>
    /// <param name="queueInkey">Accoda un tasto per INKEY/WAITKEY.</param>
    /// <param name="requestAbort">Interrompe il programma (X o Ctrl+C non agganciato).</param>
    /// <param name="ctrlCIsIntercepted">True se il programma ha un hook su Ctrl+C.</param>
    public static void Bind(Action<string> queueInkey, Action requestAbort, Func<bool>? ctrlCIsIntercepted = null)
    {
        ArgumentNullException.ThrowIfNull(queueInkey);
        ArgumentNullException.ThrowIfNull(requestAbort);
        if (_bound) return;
        _bound = true;

        void abort()
        {
            queueInkey("\x1b");
            requestAbort();
            CloseNow();
        }

        GraphicsManager.Instance.WindowCloseRequested += abort;
        SdlManager.Instance.QuitRequested += _ => abort();
        SdlManager.Instance.WindowEvent += e =>
        {
            if (e.window.windowEvent == SDL.SDL_WindowEventID.SDL_WINDOWEVENT_CLOSE)
                abort();
        };
        SdlManager.Instance.KeyboardEvent += e => OnKey(e, queueInkey, abort, ctrlCIsIntercepted);
        SdlManager.Instance.TextInputEvent += e => OnText(e, queueInkey);
        try { SDL.SDL_StartTextInput(); } catch { /* dummy driver */ }
    }

    /// <summary>
    /// True se il loop Present deve uscire e chiudere SDL subito.
    /// Su macOS Ctrl+C arriva come SIGINT (<c>AbortRequested</c>) e spesso non alza
    /// <c>Console.CancelKeyPress</c>: non basta <paramref name="windowQuitRequested"/>.
    /// </summary>
    /// <param name="windowQuitRequested">X della finestra / SDL_QUIT.</param>
    /// <param name="abortRequested">Valore di <c>IlRuntime.AbortRequested</c>.</param>
    /// <returns>True → <see cref="CloseNow"/> e return, niente flush screenshot.</returns>
    public static bool ShouldStopUi(bool windowQuitRequested, bool abortRequested)
        => windowQuitRequested || abortRequested;

    /// <summary>Chiude SDL sul thread del loop eventi. Idempotente.</summary>
    public static void CloseNow()
    {
        try { SdlManager.Instance.Quit(); } catch { /* già chiuso */ }
    }

    private static void OnKey(SDL.SDL_Event e, Action<string> queueInkey, Action abort, Func<bool>? ctrlCIsIntercepted)
    {
        if (e.type != SDL.SDL_EventType.SDL_KEYDOWN || e.key.repeat != 0) return;
        var sym = e.key.keysym.sym;
        var mod = e.key.keysym.mod;
        var ctrl = (mod & (SDL.SDL_Keymod.KMOD_LCTRL | SDL.SDL_Keymod.KMOD_RCTRL | SDL.SDL_Keymod.KMOD_GUI)) != 0;
        if (ctrl && sym == SDL.SDL_Keycode.SDLK_c)
        {
            if (ctrlCIsIntercepted?.Invoke() == true)
                queueInkey("\x03");
            else
                abort();
            return;
        }
        switch (sym)
        {
            case SDL.SDL_Keycode.SDLK_ESCAPE:
                queueInkey("\x1b");
                return;
            case SDL.SDL_Keycode.SDLK_RETURN:
            case SDL.SDL_Keycode.SDLK_KP_ENTER:
                queueInkey("\r");
                return;
            case SDL.SDL_Keycode.SDLK_BACKSPACE:
                queueInkey("\b");
                return;
            case SDL.SDL_Keycode.SDLK_TAB:
                queueInkey("\t");
                return;
            case SDL.SDL_Keycode.SDLK_SPACE:
                queueInkey(" ");
                return;
        }

        if (sym >= SDL.SDL_Keycode.SDLK_0 && sym <= SDL.SDL_Keycode.SDLK_9)
        {
            queueInkey(((char)('0' + (sym - SDL.SDL_Keycode.SDLK_0))).ToString());
            return;
        }
        if (sym >= SDL.SDL_Keycode.SDLK_a && sym <= SDL.SDL_Keycode.SDLK_z)
        {
            queueInkey(((char)('a' + (sym - SDL.SDL_Keycode.SDLK_a))).ToString());
        }
    }

    private static void OnText(SDL.SDL_Event e, Action<string> queueInkey)
    {
        if (e.type != SDL.SDL_EventType.SDL_TEXTINPUT) return;
        string text;
        unsafe
        {
            text = Encoding.UTF8.GetString(e.text.text, 32).TrimEnd('\0');
        }
        if (text.Length > 0 && text[0] != '\x03')
            queueInkey(text);
    }
}
