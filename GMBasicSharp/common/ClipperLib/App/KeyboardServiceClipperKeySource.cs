using CommonLib.Keyboard;
using ClipperLib.Clipper;

namespace ClipperLib.App;

/// <summary>
/// Bridge che alimenta <see cref="TerminalReader"/> (via <see cref="ClipperKeyInput.Current"/>)
/// dall'<see cref="IKeyboardService"/> dell'host, invece di leggere <c>System.Console</c>.
///
/// PROBLEMA risolto: <c>IKeyboardService.TryReadKey</c> è <b>consumante</b> (rimuove dal canale),
/// mentre l'API di TerminalReader separa "disponibilità" (<see cref="IClipperKeySource.KeyAvailable"/>)
/// da "lettura" (<see cref="IClipperKeySource.ReadKey"/>). Se KeyAvailable consumasse il tasto, il
/// successivo ReadKey lo perderebbe. Questo bridge fa <b>staging</b> del prossimo tasto: KeyAvailable
/// mette in attesa un tasto dal canale (senza consumarlo due volte) e ReadKey restituisce quello in
/// staging (o fa polling se non ancora disponibile). Così le sequenze ESC/CSI/mouse di TerminalReader
/// funzionano leggendo tasto-per-tasto dal canale dell'host.
///
/// ATTIVAZIONE (TODO TerminalReader-Via-KeyboardService, item #2): l'host/repl/runtime imposta
/// <c>ClipperKeyInput.Current = new KeyboardServiceClipperKeySource(hostKeyboard)</c> quando avvia
/// una sessione TUI e ripristina <c>ClipperKeyInput.UseConsole()</c> a fine sessione.
/// </summary>
public sealed class KeyboardServiceClipperKeySource : IClipperKeySource
{
    private readonly IKeyboardService _keyboard;
    private ConsoleKeyInfo? _staged;

    public KeyboardServiceClipperKeySource(IKeyboardService keyboard)
    {
        _keyboard = keyboard ?? throw new ArgumentNullException(nameof(keyboard));
    }

    /// <summary>
    /// Quando l'input arriva da un <c>IKeyboardService</c>, la redirezione di System.Console è
    /// IRRILEVANTE: i tasti vengono dal canale/coda del servizio (headless: coda iniettata;
    /// console: listener che drena Console). Quindi il bridge NON deve early-return su
    /// <c>Console.IsInputRedirected</c> — altrimenti in stdin reindirizzato TerminalReader
    /// restituirebbe UiNone e ignorerebbe i tasti del servizio. Il chiamante deve assicurare
    /// che il servizio sia attivo (funzionante) prima di installare il bridge.
    /// </summary>
    public bool InputRedirected => false;

    public bool KeyAvailable
    {
        get
        {
            if (_staged.HasValue) return true;
            _staged = _keyboard.TryReadKey(false);
            return _staged.HasValue;
        }
    }

    public ConsoleKeyInfo ReadKey(bool intercept)
    {
        // Restituisce prima l'eventuale tasto messo in staging da KeyAvailable.
        var staged = _staged;
        if (staged.HasValue)
        {
            _staged = null;
            return staged.Value;
        }
        // Polling sul canale finché non arriva un tasto.
        while (true)
        {
            var k = _keyboard.TryReadKey(false);
            if (k.HasValue) return k.Value;
            Thread.Sleep(4);
        }
    }
}
