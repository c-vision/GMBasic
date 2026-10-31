using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.App;

public interface IApp
{
    string Name { get; }
    bool CloseRequested { get; }
    (int x, int y) Caret { get; }
    bool CursorVisible { get; }
    void Resize(int cols, int rows);
    void HandleEvent(UiEvent ev);
    void Draw(ScreenBuffer buffer);
}
