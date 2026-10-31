using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

public interface IInputSource : IDisposable
{
    UiEvent Read(int timeoutMs);
}
