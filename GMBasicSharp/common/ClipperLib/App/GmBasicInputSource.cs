using CommonLib.Consoles.Tui;
using CommonLib.Keyboard;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

/// <summary>
/// Adattatore che fa da ponte tra <see cref="IKeyboardService"/> (GMBasic)
/// e <see cref="IInputSource"/> (ClipperLib). Un singolo IKeyboardService
/// alimenta sia il loop eventi TUI che i comandi BASIC (INKEY$, ON KEY).
/// </summary>
public sealed class GmBasicInputSource : IInputSource
{
    private readonly IKeyboardService _keyboard;

    public GmBasicInputSource(IKeyboardService keyboard)
    {
        _keyboard = keyboard ?? throw new ArgumentNullException(nameof(keyboard));
    }

    public UiEvent Read(int timeoutMs)
    {
        var cki = _keyboard.TryReadKey(false);
        if (cki == null)
        {
            // In headless mode, give the keyboard queue time to fill.
            // Without this, EventLoop spins consuming 100% CPU.
            if (timeoutMs > 0)
                System.Threading.Thread.Sleep(Math.Min(timeoutMs, 50));
            else
                System.Threading.Thread.Sleep(1);
            return new UiNone();
        }

        var mods = cki.Value.Modifiers;
        var km = KeyModifiers.None;
        if ((mods & ConsoleModifiers.Shift) != 0) km |= KeyModifiers.Shift;
        if ((mods & ConsoleModifiers.Alt) != 0) km |= KeyModifiers.Alt;
        if ((mods & ConsoleModifiers.Control) != 0) km |= KeyModifiers.Control;

        // ConsoleKey → codici interni KeyConstants (K_UP=5, K_DOWN=24, ...): il loop READ
        // di ClipperRead confronta i tasti con K_* (codici del TerminalReader), NON con i
        // codici ConsoleKey — senza questa mappa le frecce (38/40) cadevano nel case
        // "carattere" (32..126) scrivendo '&'/'(' nel campo invece di navigare/steppare.
        return new UiKey(ToInternalKey(cki.Value), km);
    }

    /// <summary>Mappa i tasti di navigazione ConsoleKey ai codici interni KeyConstants usati
    /// dal loop READ di ClipperRead (frecce/TAB/INS/DEL/HOME/END/PGUP/PGDN).</summary>
    public static int ToInternalKey(ConsoleKeyInfo cki)
    {
        return cki.Key switch
        {
            ConsoleKey.UpArrow => KeyConstants.K_UP,
            ConsoleKey.DownArrow => KeyConstants.K_DOWN,
            ConsoleKey.LeftArrow => KeyConstants.K_LEFT,
            ConsoleKey.RightArrow => KeyConstants.K_RIGHT,
            ConsoleKey.Tab => KeyConstants.K_TAB,
            ConsoleKey.Enter => KeyConstants.K_ENTER,
            ConsoleKey.Escape => KeyConstants.K_ESC,
            ConsoleKey.Backspace => KeyConstants.K_BS,
            ConsoleKey.Delete => KeyConstants.K_DEL,
            ConsoleKey.Insert => KeyConstants.K_INS,
            ConsoleKey.Home => KeyConstants.K_HOME,
            ConsoleKey.End => KeyConstants.K_END,
            ConsoleKey.PageUp => KeyConstants.K_PGUP,
            ConsoleKey.PageDown => KeyConstants.K_PGDN,
            _ => (int)cki.Key
        };
    }

    public void Dispose() { }
}
