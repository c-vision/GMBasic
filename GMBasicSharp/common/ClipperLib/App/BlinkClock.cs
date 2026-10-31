using CommonLib.Screens;
using ClipperLib.Clipper;
using ClipperLib.Controls;

namespace ClipperLib.App;

/// <summary>
/// Unico timer di blink della libreria: toggla la fase e sincronizza sia
/// <see cref="Control.BlinkOn"/> (Controls) sia <see cref="ClipperState.BlinkOn"/>
/// (legacy Clipper), così entrambi i mondi lampeggiano con lo stesso periodo.
/// </summary>
public sealed class BlinkClock
{
    public long PeriodMs { get; set; } = KeyConstants.BLINK_MS;

    private long _nextFlip = -1;

    public bool BlinkOn { get; private set; } = true;

    /// <summary>Riporta la fase a "cursore pieno" (dopo un input o all'avvio).</summary>
    public void Reset()
    {
        _nextFlip = Environment.TickCount64 + PeriodMs;
        BlinkOn = true;
        Control.BlinkOn = true;
        ClipperGlobal.State.BlinkOn = true;
    }

    /// <summary>Avanza il clock; true se la fase di blink è cambiata (chiedere un redraw).</summary>
    public bool Tick()
    {
        var now = Environment.TickCount64;
        if (_nextFlip < 0) _nextFlip = now + PeriodMs;
        if (now < _nextFlip) return false;
        _nextFlip = now + PeriodMs;

        var st = ClipperGlobal.State;
        if (!st.BlinkEnabled)
        {
            BlinkOn = true;
            Control.BlinkOn = true;
            st.BlinkOn = true;
            return false;
        }

        BlinkOn = !BlinkOn;
        Control.BlinkOn = BlinkOn;
        st.BlinkOn = BlinkOn;
        return true;
    }
}
