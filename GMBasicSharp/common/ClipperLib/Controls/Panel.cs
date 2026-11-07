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
        if (Border == PanelBorder.None) return;
        NativeDraw.Show(buffer, NativeDraw.Panel, this,
                        text: Title ?? "",
                        sel: NativeDraw.CgPanelBorder(Border));
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
        NativeDraw.Show(buffer, NativeDraw.Window, this,
                        extra: ApplyCase(Title),
                        sel: NativeDraw.CgPanelBorder(Border),
                        extraState: Closable ? NativeDraw.StOpen : 0);
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