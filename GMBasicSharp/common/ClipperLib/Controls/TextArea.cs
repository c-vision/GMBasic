using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class TextArea : Control
{
    public List<string> Lines { get; set; } = [""];
    public int CursorLine { get; set; }
    public int CursorCol { get; set; }
    public int ScrollOffset { get; set; }
    public bool Modified { get; set; }
    public void ScrollUp(int lines = 1) => ScrollOffset = Math.Max(0, ScrollOffset - lines);
    public void ScrollDown(int lines = 1) => ScrollOffset = Math.Min(Math.Max(0, Lines.Count - Height), ScrollOffset + lines);

    public TextArea WithLines(List<string> lines)
    {
        Lines = lines;
        if (Lines.Count == 0) Lines.Add("");
        CursorLine = CursorCol = 0;
        return this;
    }

    public string RenderLine(int idx)
    {
        var i = ScrollOffset + idx;
        if (i < 0 || i >= Lines.Count) return new string(' ', Width);
        var line = Lines[i].Length > Width ? Lines[i][..Width] : Lines[i];
        return line.Length < Width ? line.PadRight(Width) : line;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        NativeDraw.Show(buffer, NativeDraw.TextArea, this,
                        text: string.Join("\n", Lines),
                        sel: ScrollOffset, min: CursorLine, max: CursorCol);
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;
        switch (key)
        {
            case KeyConstants.K_UP:
                if (CursorLine > 0) CursorLine--;
                CursorCol = Math.Min(CursorCol, Lines[CursorLine].Length);
                if (CursorLine < ScrollOffset) ScrollOffset--;
                return true;

            case KeyConstants.K_DOWN:
                if (CursorLine < Lines.Count - 1) CursorLine++;
                CursorCol = Math.Min(CursorCol, Lines[CursorLine].Length);
                if (CursorLine >= ScrollOffset + Height) ScrollOffset++;
                return true;

            case KeyConstants.K_LEFT:
                if (CursorCol > 0) CursorCol--;
                return true;

            case KeyConstants.K_RIGHT:
                if (CursorCol < Lines[CursorLine].Length) CursorCol++;
                return true;

            case KeyConstants.K_ENTER:
                Modified = true;
                var rest = Lines[CursorLine][CursorCol..];
                Lines[CursorLine] = Lines[CursorLine][..CursorCol];
                Lines.Insert(CursorLine + 1, rest);
                CursorLine++;
                CursorCol = 0;
                return true;

            case KeyConstants.K_BS:
                if (CursorCol > 0)
                {
                    Modified = true;
                    Lines[CursorLine] = Lines[CursorLine].Remove(CursorCol - 1, 1);
                    CursorCol--;
                }
                else if (CursorLine > 0)
                {
                    Modified = true;
                    var prevLen = Lines[CursorLine - 1].Length;
                    Lines[CursorLine - 1] += Lines[CursorLine];
                    Lines.RemoveAt(CursorLine);
                    CursorLine--;
                    CursorCol = prevLen;
                }
                return true;

            case KeyConstants.K_HOME:
                CursorCol = 0;
                return true;

            case KeyConstants.K_END:
                CursorCol = Lines[CursorLine].Length;
                return true;

            default:
                if (key is >= 32 and <= 126)
                {
                    Modified = true;
                    Lines[CursorLine] = Lines[CursorLine].Insert(CursorCol, ((char)key).ToString());
                    CursorCol++;
                    return true;
                }
                break;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        var idx = y - Y + ScrollOffset;
        if (idx >= 0 && idx < Lines.Count)
        {
            CursorLine = idx;
            CursorCol = Math.Clamp(x - X, 0, Lines[idx].Length);
        }
        return true;
    }
}