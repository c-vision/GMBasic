using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class ComboBox : Control
{
    public List<string> Items { get; set; } = [];
    public int? Selected { get; set; }
    public bool Open { get; set; }
    public int Highlight { get; set; }
    public int DropDownHeight { get; set; } = 5;

    public ComboBox WithItems(List<string> items)
    {
        Items = items;
        if (Items.Count > 0 && Selected == null) { Selected = 0; Highlight = 0; }
        return this;
    }

    public string? SelectedItem => Selected.HasValue && Selected.Value < Items.Count ? Items[Selected.Value] : null;

    public string RenderLine()
    {
        var display = Selected.HasValue ? Items[Selected.Value] : "";
        if (display.Length > Width - 1) display = display[..(Width - 1)];
        return display.PadRight(Width);
    }

    public override void Draw(ScreenBuffer buffer)
    {
        var (fg, bg) = Style.Effective(Focused);
        var display = RenderLine();
        buffer.SetString(X, Y, display, fg, bg);
        buffer.SetChar(X + Width - 1, Y, '▼', fg, bg);

        if (Open && Items.Count > 0)
        {
            var (popFg, popBg) = Style.Effective(false);
            var (selFg, selBg) = Style.Effective(selected: true);
            var maxH = Math.Min(DropDownHeight, Items.Count);
            for (var i = 0; i < maxH; i++)
            {
                var item = ApplyCase(Items[i].Length > Width ? Items[i][..Width] : Items[i].PadRight(Width));
                var (f, b) = i == Highlight ? (selFg, selBg) : (popFg, popBg);
                buffer.SetString(X, Y + 1 + i, item, f, b);
            }
        }
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;

        if (Open)
        {
            switch (key)
            {
                case KeyConstants.K_UP:
                    if (Highlight > 0) Highlight--;
                    return true;
                case KeyConstants.K_DOWN:
                    if (Highlight < Items.Count - 1) Highlight++;
                    return true;
                case KeyConstants.K_ENTER:
                    if (Highlight >= 0 && Highlight < Items.Count)
                    {
                        Selected = Highlight;
                        Open = false;
                    }
                    return true;
                case KeyConstants.K_ESC:
                    Open = false;
                    return true;
            }
        }
        else
        {
            switch (key)
            {
                case KeyConstants.K_ENTER:
                case KeyConstants.K_DOWN:
                    if (Items.Count > 0)
                    {
                        Open = true;
                        Highlight = Selected ?? 0;
                    }
                    return true;
                case KeyConstants.K_ESC:
                    return true;
            }
        }
        return false;
    }

    public override ScreenRect Rect => new(X, Y, Width, Open ? DropDownHeight + 1 : Math.Max(1, Height));

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled || !Rect.Contains(x, y)) return false;
        Focused = true;
        if (Open)
        {
            var itemIdx = y - (Y + 1);
            if (itemIdx >= 0 && itemIdx < Math.Min(DropDownHeight, Items.Count))
            {
                Selected = Highlight = itemIdx;
                Open = false;
            }
            else
            {
                Open = false;
            }
        }
        else
        {
            Open = true;
            Highlight = Selected ?? 0;
        }
        return true;
    }
}