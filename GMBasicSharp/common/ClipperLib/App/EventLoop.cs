using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

/// <summary>
/// Ciclo host unico per la lettura degli eventi con blink condiviso.
/// <see cref="Next"/> legge un <see cref="UiEvent"/> dall'input (resi
/// UiResize/UiPaste inclusi); <see cref="BlinkTick"/> avanza il
/// <see cref="BlinkClock"/> e segnala quando serve un redraw di blink.
/// </summary>
public sealed class EventLoop
{
    private readonly IInputSource _input;
    private readonly BlinkClock _clock = new();

    public EventLoop(IInputSource input) => _input = input;

    public BlinkClock Clock => _clock;

    /// <summary>
    /// Legge il prossimo evento (timeout negativo = attesa indefinita).
    /// Un input reale azzera la fase di blink (cursore pieno).
    /// </summary>
    public UiEvent Next(int timeoutMs)
    {
        var ev = _input.Read(timeoutMs);
        if (ev is not UiNone)
        {
            _clock.Reset();
            return ev;
        }
        return ev;
    }

    /// <summary>True se la fase di blink è cambiata (redraw richiesto).</summary>
    public bool BlinkTick() => _clock.Tick();
}
