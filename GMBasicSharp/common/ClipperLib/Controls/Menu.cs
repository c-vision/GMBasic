using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class MenuItem
{
    public string Label { get; set; } = "";
    public List<MenuItem> SubItems { get; set; } = [];
    public bool Separator { get; set; }
    public Action? Action { get; set; }
    public string? Hotkey { get; set; }
}

public class Menu : Control
{
    public List<MenuItem> Items { get; set; } = [];
    public int SelectedIndex { get; set; }
    public bool IsOpen { get; set; }
    public int SubSelectedIndex { get; set; }

    public Menu() { Height = 3; }

    private static readonly CellColor BorderFg = new() { R = 0, G = 0, B = 0 };
    private static readonly CellColor BorderBg = new() { R = 192, G = 192, B = 192 };
    /// <summary>Rosso Borland per le lettere hotkey (~X~) nel menu/status.</summary>
    private static readonly CellColor HotkeyRed = new() { R = 170, G = 0, B = 0 };

    /// <summary>Disegna una label con marker ~X~ : X in rosso (o hotkeyColor), il resto fg.
    /// Ritorna la x dopo il testo.</summary>
    private static int DrawHotkeyLabel(ScreenBuffer buffer, int x, int y, string label, CellColor fg, CellColor bg, CellColor hotkey)
    {
        var cx = x; var inHk = false;
        foreach (var ch in label)
        {
            if (ch == '~') { inHk = !inHk; continue; }
            buffer.SetChar(cx++, y, ch, inHk ? hotkey : fg, bg);
        }
        return cx;
    }
    private static string StripTilde(string s) => s.Replace("~", "");

    private int ComputeWidth()
    {
        if (Items.Count == 0) return Width;
        var maxLen = Items.Max(m => m.Label.Length);
        var subs = Items.SelectMany(m => m.SubItems).ToList();
        if (subs.Count == 0) return Math.Max(Width, maxLen + 4);
        var maxLabel = subs.Max(s => s.Label.Length);
        var maxHotkey = subs.Max(s => s.Hotkey?.Length ?? 0);
        var inner = maxHotkey > 0 ? maxLabel + maxHotkey + 6 : maxLabel + 4;
        return Math.Max(Width, maxLen + inner + 4);
    }

    private int SubmenuWidth()
    {
        if (SelectedIndex < 0 || SelectedIndex >= Items.Count) return 0;
        var subs = Items[SelectedIndex].SubItems;
        if (subs.Count == 0) return 0;
        var maxLabel = subs.Max(s => s.Label.Length);
        var maxHotkey = subs.Max(s => s.Hotkey?.Length ?? 0);
        return maxLabel + (maxHotkey > 0 ? maxHotkey + 4 : 2) + 2;
    }

    public override ScreenRect Rect
    {
        get
        {
            if (!IsOpen) return new(X, Y, ComputeWidth(), 1); // barra chiusa = solo la riga barra
            var subH = IsOpen && SelectedIndex >= 0 && SelectedIndex < Items.Count ? Items[SelectedIndex].SubItems.Count : 0;
            if (subH > 0) return new(X, Y, ComputeWidth(), 1 + 2 + subH + 2); // menu bar + top border + items + bottom border
            return new(X, Y, ComputeWidth(), 1);
        }
    }

    public Menu WithItems(List<MenuItem> items) { Items = items; return this; }

    public override void Draw(ScreenBuffer buffer)
    {
        var (fg, bg) = Style.Effective(false);
        var (selFg, selBg) = Style.Effective(selected: true);

        // Top-level menu bar — fill entire row with bg
        buffer.SetString(0, Y, new string(' ', buffer.Width), fg, bg);
        var cx = X;
        for (var i = 0; i < Items.Count; i++)
        {
            var sel = i == SelectedIndex && IsOpen;
            var (f, b) = sel ? (selFg, selBg) : (fg, bg);
            var hk = sel ? selFg : HotkeyRed;
            cx = DrawHotkeyLabel(buffer, cx, Y, " " + ApplyCase(Items[i].Label) + " ", f, b, hk);
        }

        // Submenu
        if (!IsOpen || SelectedIndex < 0 || SelectedIndex >= Items.Count) return;
        var menu = Items[SelectedIndex];
        if (menu.SubItems.Count == 0) return;

        var px = X;
        for (var i = 0; i < SelectedIndex; i++)
            px += Items[i].Label.Length + 2;

        var subs = menu.SubItems;
        var maxLabel = subs.Max(s => s.Label.Length);
        var maxHotkey = subs.Max(s => s.Hotkey?.Length ?? 0);
        var innerW = maxLabel + (maxHotkey > 0 ? maxHotkey + 4 : 3);
        var sy = Y + 1;

        // Top border
        buffer.SetString(px, sy, "┌" + new string('─', innerW) + "┐", BorderFg, BorderBg);

        for (var i = 0; i < subs.Count; i++)
        {
            var iy = sy + 1 + i;
            if (subs[i].Separator)
            {
                buffer.SetString(px, iy, "├" + new string('─', innerW) + "┤", BorderFg, BorderBg);
            }
            else
            {
                var isSel = i == SubSelectedIndex;
                var (selFgCol, selBgCol) = isSel ? (BorderBg, BorderFg) : (BorderFg, BorderBg);
                buffer.SetChar(px, iy, '│', BorderFg, BorderBg);
                var vis = StripTilde(subs[i].Label);
                var pad = " " + vis.PadRight(maxLabel);
                DrawHotkeyLabel(buffer, px + 1, iy, " " + subs[i].Label + new string(' ', Math.Max(0, maxLabel - vis.Length)), selFgCol, selBgCol, isSel ? selFgCol : HotkeyRed);
                var accel = subs[i].Hotkey != null ? "  " + subs[i].Hotkey!.PadLeft(maxHotkey) + " " : "  ";
                buffer.SetString(px + 1 + pad.Length, iy, accel.PadRight(innerW - pad.Length), selFgCol, selBgCol);
                buffer.SetChar(px + innerW + 1, iy, '│', BorderFg, BorderBg);
            }
        }

        // Bottom border
        buffer.SetString(px, sy + 1 + subs.Count, "└" + new string('─', innerW) + "┘", BorderFg, BorderBg);
    }

    public override bool HandleKey(int key)
    {
        if (!Enabled) return false;

        if (IsOpen && SelectedIndex >= 0 && SelectedIndex < Items.Count && Items[SelectedIndex].SubItems.Count > 0)
        {
            var subs = Items[SelectedIndex].SubItems;
            var subCount = subs.Count;
            switch (key)
            {
                case KeyConstants.K_UP:
                    do { SubSelectedIndex = (SubSelectedIndex - 1 + subCount) % subCount; }
                    while (subs[SubSelectedIndex].Separator);
                    return true;
                case KeyConstants.K_DOWN:
                    do { SubSelectedIndex = (SubSelectedIndex + 1) % subCount; }
                    while (subs[SubSelectedIndex].Separator);
                    return true;
                case KeyConstants.K_LEFT:
                    if (SelectedIndex > 0) { SelectedIndex--; SubSelectedIndex = 0; }
                    return true;
                case KeyConstants.K_RIGHT:
                    if (SelectedIndex < Items.Count - 1) { SelectedIndex++; SubSelectedIndex = 0; }
                    return true;
                case KeyConstants.K_ESC:
                    IsOpen = false; SubSelectedIndex = 0; return true;
                case KeyConstants.K_ENTER:
                    return true;
            }
            return false;
        }

        if (IsOpen)
        {
            switch (key)
            {
                case KeyConstants.K_LEFT: if (SelectedIndex > 0) SelectedIndex--; return true;
                case KeyConstants.K_RIGHT: if (SelectedIndex < Items.Count - 1) SelectedIndex++; return true;
                case KeyConstants.K_ESC: IsOpen = false; return true;
                case KeyConstants.K_ENTER: case KeyConstants.K_DOWN: return true;
            }
            return false;
        }

        switch (key)
        {
            case KeyConstants.K_ENTER: case KeyConstants.K_DOWN:
                IsOpen = true; SubSelectedIndex = 0; return true;
        }
        return false;
    }

    public override bool HandleClick(int x, int y)
    {
        if (!Rect.Contains(x, y)) return false;
        Focused = true;

        // Menu bar items
        var cx = X;
        for (var i = 0; i < Items.Count; i++)
        {
            var label = " " + Items[i].Label + " ";
            if (x >= cx && x < cx + label.Length)
            {
                if (SelectedIndex == i && IsOpen) IsOpen = false;
                else { SelectedIndex = i; IsOpen = true; SubSelectedIndex = 0; }
                return true;
            }
            cx += label.Length;
        }

        // Submenu items
        if (IsOpen && SelectedIndex >= 0 && SelectedIndex < Items.Count)
        {
            var menu = Items[SelectedIndex];
            var subs = menu.SubItems;
            if (subs.Count == 0) return true;

            var px = X;
            for (var i = 0; i < SelectedIndex; i++)
                px += Items[i].Label.Length + 2;

            var maxLabel = subs.Max(s => s.Label.Length);
            var maxHotkey = subs.Max(s => s.Hotkey?.Length ?? 0);
            var innerW = maxLabel + (maxHotkey > 0 ? maxHotkey + 4 : 2);
            var sy = Y + 1; // top border row
            var contentW = innerW + 2; // border chars

            for (var i = 0; i < subs.Count; i++)
            {
                var iy = sy + 1 + i; // item row (after top border)
                if (y == iy && x >= px && x < px + contentW + 2)
                {
                    if (!subs[i].Separator) { SubSelectedIndex = i; subs[i].Action?.Invoke(); }
                    IsOpen = false;
                    return true;
                }
            }
        }
        return true;
    }

    public string? GetSelectedLabel()
    {
        if (SelectedIndex < 0 || SelectedIndex >= Items.Count) return null;
        var top = Items[SelectedIndex];
        if (top.SubItems.Count == 0) return top.Label;
        if (SubSelectedIndex < 0 || SubSelectedIndex >= top.SubItems.Count) return null;
        return top.SubItems[SubSelectedIndex].Separator ? null : top.SubItems[SubSelectedIndex].Label;
    }
}
