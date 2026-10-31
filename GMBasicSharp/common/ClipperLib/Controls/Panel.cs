using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public enum PanelBorder { None, Single, Double, Rounded }

public class Panel : Control
{
    public string? Title { get; set; }
    public PanelBorder Border { get; set; } = PanelBorder.Single;
    public bool Draggable { get; set; } = true;

    public bool IsDragging { get; set; }
    public int DragOffsetX { get; set; }
    public int DragOffsetY { get; set; }

    public Panel WithTitle(string? title) { Title = title; return this; }
    public Panel WithBorder(PanelBorder border) { Border = border; return this; }

    private static (char tl, char th, char tr, char vl, char br, char bh, char bl, char vr) BorderChars(PanelBorder b) => b switch
    {
        PanelBorder.Double => ('╔', '═', '╗', '║', '╝', '═', '╚', '║'),
        PanelBorder.Rounded => ('╭', '─', '╮', '│', '╯', '─', '╰', '│'),
        PanelBorder.None => (' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '),
        _ => ('┌', '─', '┐', '│', '┘', '─', '└', '│'),
    };

    public ScreenRect InnerArea
    {
        get
        {
            if (Border == PanelBorder.None) return new ScreenRect(X, Y, Width, Height);
            return new ScreenRect(X + 1, Y + 1, Width - 2, Height - 2);
        }
    }

    public override void Draw(ScreenBuffer buffer)
    {
        var (fg, bg) = Style.Effective(Focused);
        if (Border == PanelBorder.None) return;

        var (tl, th, tr, vl, br, bh, bl, vr) = BorderChars(Border);

        buffer.SetChar(X, Y, tl, fg, bg);
        buffer.SetChar(X + Width - 1, Y, tr, fg, bg);
        buffer.SetChar(X, Y + Height - 1, bl, fg, bg);
        buffer.SetChar(X + Width - 1, Y + Height - 1, br, fg, bg);

        for (var x = X + 1; x < X + Width - 1; x++)
        {
            buffer.SetChar(x, Y, th, fg, bg);
            buffer.SetChar(x, Y + Height - 1, bh, fg, bg);
        }
        for (var y = Y + 1; y < Y + Height - 1; y++)
        {
            buffer.SetChar(X, y, vl, fg, bg);
            for (var x = X + 1; x < X + Width - 1; x++)
                buffer.SetChar(x, y, ' ', fg, bg);
            buffer.SetChar(X + Width - 1, y, vr, fg, bg);
        }

        if (!string.IsNullOrEmpty(Title))
        {
            var title = $" {Title} ";
            if (title.Length < Width - 2)
                buffer.SetString(X + 1, Y, title, fg, bg);
        }
    }

    public override bool HandleKey(int key) => false;

    public override bool HandleMouse(MouseAction action)
    {
        switch (action.Kind)
        {
            case MouseActionKind.Click:
                if (Draggable && action.Y == Y && action.X >= X && action.X < X + Width)
                {
                    Focused = true;
                    IsDragging = true;
                    DragOffsetX = action.X - X;
                    DragOffsetY = action.Y - Y;
                    return true;
                }
                return false;
            case MouseActionKind.Drag:
                if (IsDragging && Draggable)
                {
                    X = action.X - DragOffsetX;
                    Y = action.Y - DragOffsetY;
                    return true;
                }
                return false;
            case MouseActionKind.Release:
                IsDragging = false;
                return true;
        }
        return false;
    }
}

public class Window : Control
{
    public string Title { get; set; } = "";
    public PanelBorder Border { get; set; } = PanelBorder.Single;
    public bool Closable { get; set; } = true;
    public bool Draggable { get; set; } = true;
    public bool IsDragging { get; set; }
    public int DragOffsetX { get; set; }
    public int DragOffsetY { get; set; }

    private static (char tl, char th, char tr, char vl, char br, char bh, char bl, char vr) BorderChars(PanelBorder b) => b switch
    {
        PanelBorder.Double => ('╔', '═', '╗', '║', '╝', '═', '╚', '║'),
        PanelBorder.Rounded => ('╭', '─', '╮', '│', '╯', '─', '╰', '│'),
        _ => ('┌', '─', '┐', '│', '┘', '─', '└', '│'),
    };

    public ScreenRect InnerArea => new(X + 1, Y + 1, Width - 2, Height - 2);

    public bool TitleBarContains(int cx, int cy) =>
        cy == Y && cx >= X && cx < X + Width;

    public bool CloseButtonContains(int cx, int cy) =>
        cy == Y && cx >= X + Width - 3 && cx <= X + Width - 1;

    public Window WithTitle(string title) { Title = title; return this; }
    public Window WithBorder(PanelBorder border) { Border = border; return this; }
    public Window WithClosable(bool closable) { Closable = closable; return this; }

    public override void Draw(ScreenBuffer buffer)
    {
        if (!Visible) return;
        var (fg, bg) = Style.Effective(Focused);
        var (tl, th, tr, vl, br, bh, bl, vr) = BorderChars(Border);

        buffer.SetChar(X, Y, tl, fg, bg);
        buffer.SetChar(X + Width - 1, Y, tr, fg, bg);
        buffer.SetChar(X, Y + Height - 1, bl, fg, bg);
        buffer.SetChar(X + Width - 1, Y + Height - 1, br, fg, bg);

        for (var x = X + 1; x < X + Width - 1; x++)
        {
            buffer.SetChar(x, Y, th, fg, bg);
            buffer.SetChar(x, Y + Height - 1, bh, fg, bg);
        }
        for (var y = Y + 1; y < Y + Height - 1; y++)
        {
            buffer.SetChar(X, y, vl, fg, bg);
            for (var x = X + 1; x < X + Width - 1; x++)
                buffer.SetChar(x, y, ' ', fg, bg);
            buffer.SetChar(X + Width - 1, y, vr, fg, bg);
        }

        var title = $" {ApplyCase(Title)} ";
        if (title.Length < Width - 2)
        {
            buffer.SetString(X + 1, Y, title, fg, bg);
            if (Closable)
                buffer.SetString(X + Width - 3, Y, " ×", Style.FgError ?? CellColor.Red, bg);
        }
    }

    public override bool HandleKey(int key) => false;

    public override bool HandleClick(int x, int y) => false;

    public override bool HandleMouse(MouseAction action)
    {
        switch (action.Kind)
        {
            case MouseActionKind.Click:
                if (Closable && CloseButtonContains(action.X, action.Y))
                {
                    Visible = false;
                    return true;
                }
                if (Draggable && TitleBarContains(action.X, action.Y))
                {
                    Focused = true;
                    IsDragging = true;
                    DragOffsetX = action.X - X;
                    DragOffsetY = action.Y - Y;
                    return true;
                }
                return false;
            case MouseActionKind.Drag:
                if (IsDragging && Draggable)
                {
                    X = action.X - DragOffsetX;
                    Y = action.Y - DragOffsetY;
                    return true;
                }
                return false;
            case MouseActionKind.Release:
                IsDragging = false;
                return true;
        }
        return false;
    }
}