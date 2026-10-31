using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

public sealed class ConsoleInputSource : IInputSource
{
    public UiEvent Read(int timeoutMs)
    {
        if (!TerminalReader.IsEventAvailable(timeoutMs))
            return new UiNone();
        return TerminalReader.ReadEventCore();
    }

    public void Dispose() { }
}
