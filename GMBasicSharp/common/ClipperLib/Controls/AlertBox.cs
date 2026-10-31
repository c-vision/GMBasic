using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.App;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

// ALERT = solo OK. CONFIRM = pulsanti configurabili (Buttons).
// Ritorno di Show: 1..N = pulsante premuto (1-based, allineato a %OK=1/%CANCEL=2/...),
// 0 = finestra chiusa con X o ESC (NON confondibile con nessun pulsante).
public enum AlertResult { OK = 1, Cancelled = 0 }
public enum AlertAlign { Left, Center, Right }

public class AlertBox : Control
{
    public static bool WasDismissedByKey { get; private set; }

    public string Message { get; set; } = "";
    public Button OKButton { get; set; } = new() { Label = "OK", Width = 10 };
    /// <summary>Pulsanti del CONFIRM (ALERT ne ha solo uno: OKButton).</summary>
    public List<Button> Buttons { get; set; } = new();
    public PanelBorder Border { get; set; } = PanelBorder.Single;
    public string? Title { get; set; }
    public bool Draggable { get; set; } = true;
    public AlertAlign Alignment { get; set; } = AlertAlign.Left;
    public int? BoxWidth { get; set; }

    public bool IsDragging { get; set; }
    public int DragOffsetX { get; set; }
    public int DragOffsetY { get; set; }
    private int _btnOffX, _btnOffY;

    public AlertBox WithTitle(string? title) { Title = title; return this; }
    public AlertBox WithMessage(string message) { Message = message; return this; }
    public AlertBox WithBorder(PanelBorder border) { Border = border; return this; }
    public AlertBox WithAlignment(AlertAlign a) { Alignment = a; return this; }
    public AlertBox WithWidth(int w) { BoxWidth = w; return this; }

    public static AlertBox Centered(int termW, int termH, string title, string message, int? width = null, AlertAlign align = AlertAlign.Left)
    {
        var lines = WrapText(message, width ?? (termW - 12));
        var maxLine = lines.Max(l => l.Length);
        var w = width ?? Math.Min(Math.Max(title.Length + 4, maxLine + 6), termW - 4);
        var h = 6 + lines.Length;
        var x = (termW - w) / 2;
        var y = (termH - h) / 2;
        var btnStyle = new ControlStyle { Fg = new CellColor { R = 192, G = 192, B = 192 }, Bg = new CellColor { R = 0, G = 0, B = 0 } };
        return new AlertBox
        {
            X = x, Y = y, Width = w, Height = h, Alignment = align, BoxWidth = width,
            Title = title, Message = string.Join('\n', lines),
            OKButton = new Button { X = x + w / 2 - 5, Y = y + h - 2, Width = 10, Label = "OK", Style = btnStyle }
        };
    }

    public static string[] WrapText(string text, int width)
    {
        var result = new List<string>();
        foreach (var para in text.Split('\n'))
        {
            if (para.Length <= width) { result.Add(para); continue; }
            var words = para.Split(' ');
            var line = "";
            foreach (var w in words)
            {
                if (line.Length + w.Length + 1 <= width) line += (line.Length > 0 ? " " : "") + w;
                else { if (line.Length > 0) result.Add(line); line = w; }
            }
            if (line.Length > 0) result.Add(line);
        }
        return result.ToArray();
    }

    private static (char tl, char th, char tr, char vl, char br, char bh, char bl, char vr) BorderChars(PanelBorder b) => b switch
    {
        PanelBorder.Double => ('╔', '═', '╗', '║', '╝', '═', '╚', '║'),
        PanelBorder.Rounded => ('╭', '─', '╮', '│', '╯', '─', '╰', '│'),
        _ => ('┌', '─', '┐', '│', '┘', '─', '└', '│'),
    };

    public override void Draw(ScreenBuffer buffer)
    {
        var (fg, bg) = Style.Effective(Focused);
        var (tl, th, tr, vl, br, bh, bl, vr) = BorderChars(Border);

        // Fill inner area
        for (var y = Y + 1; y < Y + Height - 1; y++)
            for (var x = X + 1; x < X + Width - 1; x++)
                buffer.SetChar(x, y, ' ', fg, bg);

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
            buffer.SetChar(X + Width - 1, y, vr, fg, bg);
        }

        if (!string.IsNullOrEmpty(Title))
        {
            var title = $" {Title} ";
            if (title.Length < Width - 2)
                buffer.SetString(X + 1, Y, title, fg, bg);
        }

        var lines = Message.Split('\n');
        for (var i = 0; i < lines.Length; i++)
        {
            var my = Y + 2 + i;
            if (my >= Y + Height - 1) break;
            var line = lines[i];
            if (line.Length > Width - 3) line = line[..(Width - 3)];
            var x = X + 2;
            if (Alignment == AlertAlign.Center) x = X + (Width - line.Length) / 2;
            else if (Alignment == AlertAlign.Right) x = X + Width - line.Length - 2;
            buffer.SetString(x, my, line, fg, bg);
        }

        // CONFIRM multi-pulsante: disegna tutti i pulsanti; ALERT solo OKButton.
        if (Buttons.Count > 0)
            foreach (var b in Buttons) b.Draw(buffer);
        else
            OKButton.Draw(buffer);
    }

    /// <summary>
    /// Mostra la box. <paramref name="buttons"/> = etichette dei pulsanti (CONFIRM); se vuoto
    /// (ALERT) usa solo OKButton. Ritorna l'indice 1-based del pulsante premuto (allineato alle
    /// costanti %OK=1/%CANCEL=2/%YES=6/%NO=7), oppure 0 se la finestra è stata chiusa con X o ESC.
    /// <paramref name="fg"/>/<paramref name="bg"/> = colori CellColor opzionali; <paramref name="border"/>
    /// = PanelBorder (default Single); <paramref name="align"/> = allineamento testo (default Center).
    /// </summary>
    public static AlertResult Show(string title, string message, int? width = null, AlertAlign align = AlertAlign.Center,
        CellColor? fg = null, CellColor? bg = null, PanelBorder border = PanelBorder.Single, string[]? buttons = null)
    {
        var st = ClipperGlobal.State;
        bool ownHook = st.RenderHook == null;
        WasDismissedByKey = false;
        if (ownHook)
            st.RenderHook = buf => ConsoleRenderer.Write(buf);

        var termW = st.Buffer.Width;
        var termH = st.Buffer.Height;
        var box = Centered(termW, termH, title, message, width, align);
        box.Border = border;

        var style = new ControlStyle();
        if (fg != null) style.Fg = fg;
        if (bg != null) style.Bg = bg;
        box.Style = style;

        // CONFIRM: costruisci i pulsanti da buttons[] (etichette). ALERT: solo OKButton.
        if (buttons is { Length: > 0 })
        {
            box.Buttons.Clear();
            int totalW = buttons.Length * 12 + (buttons.Length - 1) * 2;
            int bx = box.X + (box.Width - totalW) / 2;
            for (int i = 0; i < buttons.Length; i++)
            {
                var b = new Button { X = bx, Y = box.Y + box.Height - 2, Width = 12, Label = buttons[i], Style = style };
                b.OnClick = () => { };
                box.Buttons.Add(b);
                bx += 14;
            }
            box.Buttons[0].Focused = true;
        }
        else
        {
            box.OKButton.Focused = true;
        }

        var cells = new ScreenCell[box.Height, box.Width];
        for (var y = 0; y < box.Height && box.Y + y < st.Buffer.Height; y++)
            for (var x = 0; x < box.Width && box.X + x < st.Buffer.Width; x++)
                cells[y, x] = st.Buffer.Cells[box.Y + y, box.X + x];

        box.Draw(st.Buffer);
        st.InvokeRenderHook();

        // Indice del pulsante corrente (0-based nel set; +1 per il ritorno 1-based).
        int current = 0;

        while (true)
        {
            var raw = ClipperInput.InKey();
            if (raw == 0)
            {
                if (Console.IsInputRedirected && st.KeySource == null)
                {
                    RestoreAndExit(st, box, cells, ownHook);
                    return AlertResult.Cancelled; // 0: chiusa
                }
                continue;
            }

            if (raw < 0)
            {
                var ma = MouseAction.FromInkeyCode(raw);
                if (ma != null)
                {
                    var targets = box.Buttons.Count > 0 ? box.Buttons : new List<Button> { box.OKButton };
                    bool dismissed = false;
                    for (int i = 0; i < targets.Count; i++)
                    {
                        if (ma.Kind is MouseActionKind.Click or MouseActionKind.DoubleClick && targets[i].Rect.Contains(ma.X, ma.Y))
                        {
                            dismissed = true;
                            current = i;
                            break;
                        }
                    }
                    box.HandleMouse(ma);
                    if (dismissed)
                    {
                        WasDismissedByKey = true;
                        RestoreAndExit(st, box, cells, ownHook);
                        return (AlertResult)(current + 1);
                    }
                    box.Draw(st.Buffer);
                    st.InvokeRenderHook();
                }
                continue;
            }

            if (raw == KeyConstants.K_ESC)
            {
                // Chiusura con ESC: ritorna 0 (finestra chiusa), NON un pulsante.
                WasDismissedByKey = true;
                RestoreAndExit(st, box, cells, ownHook);
                return AlertResult.Cancelled;
            }

            if (raw is KeyConstants.K_ENTER or ' ')
            {
                WasDismissedByKey = true;
                RestoreAndExit(st, box, cells, ownHook);
                return (AlertResult)(current + 1);
            }

            if (raw == KeyConstants.K_TAB)
            {
                var targets = box.Buttons.Count > 0 ? box.Buttons : new List<Button> { box.OKButton };
                current = (current + 1) % targets.Count;
                for (int i = 0; i < targets.Count; i++) targets[i].Focused = i == current;
                box.Draw(st.Buffer);
                st.InvokeRenderHook();
            }
        }
    }

    private static void RestoreAndExit(ClipperState st, AlertBox box, ScreenCell[,] cells, bool ownHook)
    {
        for (var y = 0; y < box.Height && box.Y + y < st.Buffer.Height; y++)
            for (var x = 0; x < box.Width && box.X + x < st.Buffer.Width; x++)
                st.Buffer.Cells[box.Y + y, box.X + x] = cells[y, x];
        st.InvokeRenderHook();
        if (ownHook) st.RenderHook = null;
    }

    public override bool HandleKey(int key)
    {
        if (key is KeyConstants.K_ENTER or KeyConstants.K_ESC or ' ')
        {
            OKButton.OnClick?.Invoke();
            return true;
        }
        if (key == KeyConstants.K_TAB)
        {
            OKButton.Focused = true;
            return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (OKButton.Rect.Contains(x, y))
        {
            OKButton.Focused = true;
            OKButton.OnClick?.Invoke();
            return true;
        }
        if (Rect.Contains(x, y))
        {
            Focused = true;
            return true;
        }
        return false;
    }

    public override bool HandleMouse(MouseAction action)
    {
        switch (action.Kind)
        {
            case MouseActionKind.Click:
                if (OKButton.Rect.Contains(action.X, action.Y))
                {
                    OKButton.Focused = true;
                    OKButton.OnClick?.Invoke();
                    return true;
                }
                if (Draggable && action.Y == Y && action.X >= X && action.X < X + Width)
                {
                    Focused = true;
                    IsDragging = true;
                    DragOffsetX = action.X - X;
                    DragOffsetY = action.Y - Y;
                    _btnOffX = OKButton.X - X;
                    _btnOffY = OKButton.Y - Y;
                    return true;
                }
                return false;
            case MouseActionKind.Drag:
                if (IsDragging && Draggable)
                {
                    X = action.X - DragOffsetX;
                    Y = action.Y - DragOffsetY;
                    OKButton.X = X + _btnOffX;
                    OKButton.Y = Y + _btnOffY;
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