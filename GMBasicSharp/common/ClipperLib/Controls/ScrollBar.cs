using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public enum Orientation { Vertical, Horizontal }

public class ScrollBar : Control
{
    private Orientation _orientation = Orientation.Vertical;
    public Orientation Orientation
    {
        get => _orientation;
        set
        {
            _orientation = value;
            if (value == Orientation.Vertical && Width == 0) Width = 1;
            if (value == Orientation.Horizontal && Height == 0) Height = 1;
        }
    }
    public int Min { get; set; }
    public int Max { get; set; } = 100;
    public int Position { get; set; }
    public int Page { get; set; } = 10;
    public CellColor? TrackColor { get; set; }

    /// <summary>Alias coerente a <see cref="Min"/>.</summary>
    public int MinValue { get => Min; set => Min = value; }
    /// <summary>Alias coerente a <see cref="Max"/>.</summary>
    public int MaxValue { get => Max; set => Max = value; }


    public ScrollBar WithRange(int min, int max) { Min = min; Max = Math.Max(min, max); return this; }
    public ScrollBar WithPage(int page) { Page = page; return this; }
    public ScrollBar WithPosition(int pos) { Position = Math.Clamp(pos, Min, Max); return this; }

    public string RenderLine()
    {
        var range = Math.Max(1, Max - Min);
        var thumbW = Math.Max(1, Width * Page / range);
        var thumbX = Width * Position / range;
        thumbX = Math.Min(thumbX, Width - thumbW);
        return new string('─', Math.Max(0, thumbX)) +
               new string('█', thumbW) +
               new string('─', Math.Max(0, Width - thumbX - thumbW));
    }

    public List<string> RenderRows()
    {
        var range = Math.Max(1, Max - Min);
        var thumbH = Math.Max(1, Height * Page / range);
        var thumbY = Height * Position / range;
        thumbY = Math.Min(thumbY, Height - thumbH);
        var rows = new List<string>();
        for (var i = 0; i < Height; i++)
            rows.Add(i >= thumbY && i < thumbY + thumbH ? "█" : "│");
        return rows;
    }

    public override void Draw(ScreenBuffer buffer)
    {
        var range = Math.Max(1, Max - Min);
        if (Page >= range) return;
        var (fg, bg) = Style.Effective(Focused, HasError);
        if (Orientation == Orientation.Vertical)
        {
            var rows = RenderRows();
            for (var i = 0; i < rows.Count && i < Height; i++)
            {
                var color = rows[i] == "│" && TrackColor.HasValue ? TrackColor.Value : fg;
                buffer.SetString(X, Y + i, rows[i], color, bg);
            }
        }
        else
        {
            buffer.SetString(X, Y, RenderLine(), fg, bg);
        }
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;
        switch (key)
        {
            case KeyConstants.K_UP:
            case KeyConstants.K_LEFT:
                Position = Math.Max(Min, Position - 1);
                return true;
            case KeyConstants.K_DOWN:
            case KeyConstants.K_RIGHT:
                Position = Math.Min(Max, Position + 1);
                return true;
            case KeyConstants.K_PGUP:
                Position = Math.Max(Min, Position - Page);
                return true;
            case KeyConstants.K_PGDN:
                Position = Math.Min(Max, Position + Page);
                return true;
            case KeyConstants.K_HOME: Position = Min; return true;
            case KeyConstants.K_END: Position = Max; return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Enabled) return false;
        if (!Rect.Contains(x, y)) return false;

        var progress = Orientation == Orientation.Horizontal ? (double)(x - X) / Width : (double)(y - Y) / Height;
        Position = Math.Clamp((int)(Min + progress * (Max - Min)), Min, Max);
        return true;
    }

    private bool _dragging;

    public override bool HandleMouse(MouseAction action)
    {
        if (!Enabled) return false;
        switch (action.Kind)
        {
            case MouseActionKind.Click:
                if (!Rect.Contains(action.X, action.Y)) return false;
                _dragging = true;
                Focused = true;
                UpdatePositionFromMouse(action.X, action.Y);
                return true;
            case MouseActionKind.Drag:
                if (!_dragging) return false;
                UpdatePositionFromMouse(action.X, action.Y);
                return true;
            case MouseActionKind.Release:
                _dragging = false;
                return true;
        }
        return false;
    }

    private void UpdatePositionFromMouse(int x, int y)
    {
        var progress = Orientation == Orientation.Horizontal
            ? (double)(x - X) / Width
            : (double)(y - Y) / Height;
        Position = Math.Clamp((int)(Min + progress * (Max - Min)), Min, Max);
    }
}