using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.App;

using ClipperLib.Clipper;

/// <summary>
/// Host di eventi condiviso per il nesting di sessioni modali: un <see cref="IApp"/>
/// eseguito da codice legacy, o una generica funzione modale. Tutte le sessioni
/// condividono lo stesso <see cref="EventLoop"/> e <see cref="BlinkClock"/> (LIFO per
/// call-stack: una sessione blocca la chiamante finché non ritorna).
/// </summary>
public sealed class ModalHost : IDisposable
{
    private static readonly AsyncLocal<ModalHost?> _current = new();
    private readonly IInputSource _input;
    private bool _disposed;

    public ModalHost(IInputSource input)
    {
        _input = input;
        Loop = new EventLoop(input);
    }

    /// <summary>Host attivo nel contesto corrente (null fuori da una sessione).</summary>
    public static ModalHost? Current => _current.Value;

    /// <summary>EventLoop condiviso di questo host.</summary>
    public EventLoop Loop { get; }

    public IInputSource Input => _input;

    /// <summary>
    /// Registra questo host come <see cref="Current"/> per la durata del chiamante:
    /// i loop legacy (GET/FORM/MEMO) e <see cref="AppRunner"/> vi leggono
    /// condividendo il loop, consentendo il nesting.
    /// </summary>
    public IDisposable Enter()
    {
        var previous = _current.Value;
        _current.Value = this;
        return new EnterScope(previous);
    }

    /// <summary>
    /// Sessione generica (es. il <c>Run</c> di una form legacy): legge dal loop
    /// condiviso e invia ogni evento alla funzione finché non restituisce false.
    /// </summary>
    public void RunModal(Func<UiEvent, bool> modal)
    {
        while (true)
        {
            var ev = Loop.Next(50);
            if (ev is UiNone && !Loop.BlinkTick())
                continue;
            if (!modal(ev))
                return;
        }
    }

    /// <summary>
    /// Esegue un <see cref="IApp"/> su questo host (la direzione "dal codice legacy
    /// all'app framework"): ridisegna su un <see cref="ScreenBuffer"/> condiviso e
    /// ritorna quando l'app richiede la chiusura.
    /// </summary>
    public void RunApp(IApp app, Action<ScreenBuffer> render, int width = 80, int height = 24)
    {
        var sb = new ScreenBuffer(width, height);
        app.Resize(width, height);
        app.Draw(sb);
        render(sb);
        ConsoleRenderer.PositionCursor(app.Caret.x, app.Caret.y, app.CursorVisible);

        RunModal(ev =>
        {
            if (ev is not UiNone)
            {
                if (ev is UiResize { Width: var w, Height: var h })
                {
                    sb.Resize(w, h);
                    app.Resize(w, h);
                }
                else
                {
                    app.HandleEvent(ev);
                }
            }
            app.Draw(sb);
            render(sb);
            ConsoleRenderer.PositionCursor(app.Caret.x, app.Caret.y, app.CursorVisible);
            return !app.CloseRequested;
        });
    }

    /// <summary>Loop dell'host corrente, o un nuovo loop sull'input di fallback.</summary>
    public static EventLoop CurrentLoopOrDefault(IInputSource fallback) =>
        Current?.Loop ?? new EventLoop(fallback);

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
    }

    private sealed class EnterScope(ModalHost? previous) : IDisposable
    {
        public void Dispose()
        {
            _current.Value = previous;
        }
    }
}
