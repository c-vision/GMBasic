using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class FunctionKeyBar
{
    public int X, Y, Width;
    public string[] Keys = ["1Help","2Save","3Load","4Find","5Run","6Edit","7List","8Auto","9Del","10Quit"];
    public CellColor Bg = CellColor.Grey;
    public CellColor Fg = CellColor.Black;
    public CellColor KeyBg = CellColor.DarkBlue;
    public CellColor KeyFg = CellColor.White;

    public void Draw(ScreenBuffer buffer)
        => NativeDraw.Show(buffer, NativeDraw.FunctionKeyBar, X, Y, Width, 1,
                           text: string.Join("|", Keys));

    public bool HandleKey(int key)
    {
        var idx = key - KeyConstants.K_F1;
        return idx >= 0 && idx < Keys.Length;
    }

    public int GetCommand(int key) => key - KeyConstants.K_F1;
}
