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
    {
        for (int x = X; x < X + Width && x < buffer.Width; x++)
            buffer.SetChar(x, Y, ' ', Fg, Bg);

        int col = X + 1;
        foreach (var label in Keys)
        {
            var numEnd = 0;
            while (numEnd < label.Length && char.IsDigit(label[numEnd])) numEnd++;
            var num = label[..numEnd];
            var text = label[numEnd..];

            foreach (var c in num)
            {
                if (col >= X + Width) break;
                buffer.SetChar(col++, Y, c, KeyFg, KeyBg);
            }
            foreach (var c in text)
            {
                if (col >= X + Width) break;
                buffer.SetChar(col++, Y, c, Fg, Bg);
            }
            if (col < X + Width) { buffer.SetChar(col++, Y, ' ', Fg, Bg); buffer.SetChar(col++, Y, ' ', Fg, Bg); }
        }
    }

    public bool HandleKey(int key)
    {
        var idx = key - KeyConstants.K_F1;
        return idx >= 0 && idx < Keys.Length;
    }

    public int GetCommand(int key) => key - KeyConstants.K_F1;
}
