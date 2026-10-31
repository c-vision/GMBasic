using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Replica delle funzioni mouse di Clipper 5.3 (MROW(), MLEFT(), MOUSTAT(), ...).
/// Lo stato viene aggiornato automaticamente da ogni <see cref="MouseAction"/> che
/// attraversa il canale input legacy (<see cref="ClipperInputSource"/>), quindi le
/// funzioni leggono gli ultimi eventi ricevuti.
/// </summary>
public static class ClipperMouse
{
    private static ClipperState S() => ClipperGlobal.State;

    /// <summary>ISMOUSE() — true se è presente un mouse.</summary>
    public static bool IsMouse() => S().MousePresent;

    /// <summary>MROW() — riga corrente del puntatore mouse.</summary>
    public static int MRaw() => S().MouseRow;

    /// <summary>MCOL() — colonna corrente del puntatore mouse.</summary>
    public static int MCol() => S().MouseCol;

    /// <summary>MLEFT() — true se il tasto sinistro è premuto.</summary>
    public static bool MLeft() => S().MouseDown[0];

    /// <summary>MRIGHT() — true se il tasto destro è premuto.</summary>
    public static bool MRight() => S().MouseDown[2];

    /// <summary>MMIDDLE() — true se il tasto centrale è premuto.</summary>
    public static bool MMiddle() => S().MouseDown[1];

    /// <summary>MLEFTPRESS() — true se il tasto sinistro è stato appena premuto.</summary>
    public static bool MLeftPress() => S().MousePressed[0];

    /// <summary>MRIGHTPRESS() — true se il tasto destro è stato appena premuto.</summary>
    public static bool MRightPress() => S().MousePressed[2];

    /// <summary>MMIDDLEPRESS() — true se il tasto centrale è stato appena premuto.</summary>
    public static bool MMiddlePress() => S().MousePressed[1];

    /// <summary>MLEFTPLESS() — true se il tasto sinistro è stato appena rilasciato.</summary>
    public static bool MLeftLess() => S().MouseReleased[0];

    /// <summary>MRIGHTPLESS() — true se il tasto destro è stato appena rilasciato.</summary>
    public static bool MRightLess() => S().MouseReleased[2];

    /// <summary>MMIDDLEPLESS() — true se il tasto centrale è stato appena rilasciato.</summary>
    public static bool MMiddleLess() => S().MouseReleased[1];

    /// <summary>MLEFTSHIFT() — true se il tasto sinistro è premuto con Shift.</summary>
    public static bool MLeftShift() => S().MouseShift[0];

    /// <summary>MRIGHTSHIFT() — true se il tasto destro è premuto con Shift.</summary>
    public static bool MRightShift() => S().MouseShift[2];

    /// <summary>MMIDDLESHIFT() — true se il tasto centrale è premuto con Shift.</summary>
    public static bool MMiddleShift() => S().MouseShift[1];

    /// <summary>MLEFTCONTROL() — true se il tasto sinistro è premuto con Ctrl.</summary>
    public static bool MLeftControl() => S().MouseCtrl[0];

    /// <summary>MRIGHTCONTROL() — true se il tasto destro è premuto con Ctrl.</summary>
    public static bool MRightControl() => S().MouseCtrl[2];

    /// <summary>MMIDDLECONTROL() — true se il tasto centrale è premuto con Ctrl.</summary>
    public static bool MMiddleControl() => S().MouseCtrl[1];

    /// <summary>MLEFTM() — true se il tasto sinistro è premuto e il mouse si è mosso.</summary>
    public static bool MLeftM() => S().MouseDown[0] && S().MouseMoved;

    /// <summary>MRIGHTM() — true se il tasto destro è premuto e il mouse si è mosso.</summary>
    public static bool MRightM() => S().MouseDown[2] && S().MouseMoved;

    /// <summary>MMIDDLEM() — true se il tasto centrale è premuto e il mouse si è mosso.</summary>
    public static bool MMiddleM() => S().MouseDown[1] && S().MouseMoved;

    /// <summary>
    /// MOUSTAT(nButton[, @nRow, @nCol, @nModifiers]) — true se il tasto indicato è
    /// premuto (1=sinistro, 2=destro, 3=centro); scrive posizione e modificatori.
    /// </summary>
    public static bool MouStat(int button, out int row, out int col, out int mods)
    {
        row = S().MouseRow;
        col = S().MouseCol;
        mods = (MLeftShift() ? 1 : 0) | (MLeftControl() ? 2 : 0);
        return button switch
        {
            1 => MLeft(),
            2 => MRight(),
            3 => MMiddle(),
            _ => S().MouseDown.Any(d => d),
        };
    }

    /// <summary>
    /// MPOSITION() — restituisce (riga, colonna) del puntatore; se si passano valori
    /// sposta il puntatore. In Clipper senza argomenti restituisce un array {row, col}.
    /// </summary>
    public static (int row, int col) MPosition(int? row = null, int? col = null)
    {
        var st = S();
        if (row.HasValue) st.MouseRow = row.Value;
        if (col.HasValue) st.MouseCol = col.Value;
        return (st.MouseRow, st.MouseCol);
    }

    /// <summary>
    /// MOUSECURSOR([row[, col[, type]]]) — sposta (e con row/col null mostra/nasconde)
    /// il cursore mouse. type: "arrow" | "text". Restituisce true se visibile.
    /// </summary>
    public static bool MouseCursor(int? row = null, int? col = null, string? type = null)
    {
        var st = S();
        if (row.HasValue) st.MouseRow = row.Value;
        if (col.HasValue) st.MouseCol = col.Value;
        if (type != null)
        {
            var t = type.ToLowerInvariant();
            if (t is "off" or "hide" or "hidden") st.MouseCursorVisible = false;
            else if (t is "on" or "show" or "visible") st.MouseCursorVisible = true;
            else st.MouseCursorShape = t;
        }
        return st.MouseCursorVisible;
    }

    /// <summary>
    /// MRELEASE() — rilascia la pressione corrente e restituisce il tasto rilasciato
    /// ("L"/"R"/"M") oppure una stringa vuota.
    /// </summary>
    public static string MRelease()
    {
        var st = S();
        for (var b = 0; b < 3; b++)
        {
            if (st.MouseDown[b] || st.MousePressed[b])
            {
                st.MouseDown[b] = false;
                st.MousePressed[b] = false;
                st.MouseReleased[b] = true;
                st.MouseMoved = false;
                return b switch { 0 => "L", 2 => "R", _ => "M" };
            }
        }
        return "";
    }

    /// <summary>Aggiorna lo stato mouse da un evento. Chiamato dal canale input legacy.</summary>
    public static void UpdateFromEvent(MouseAction ma)
    {
        var st = S();
        st.MousePresent = true;

        // reset dei flag "appena premuto/rilasciato" (valgono per un solo evento)
        Array.Fill(st.MousePressed, false);
        Array.Fill(st.MouseReleased, false);
        st.MouseDoubleClicked = false;
        st.MouseWheelDelta = 0;

        st.MouseRow = ma.Y;
        st.MouseCol = ma.X;
        st.MouseMoved = false;

        switch (ma.Kind)
        {
            case MouseActionKind.Click:
            case MouseActionKind.DoubleClick:
            case MouseActionKind.RightClick:
            case MouseActionKind.MiddleClick:
            {
                var b = ma.Kind switch
                {
                    MouseActionKind.RightClick => 2,
                    MouseActionKind.MiddleClick => 1,
                    _ => 0,
                };
                st.MouseDown[b] = true;
                st.MousePressed[b] = true;
                st.LastClickX = ma.X;
                st.LastClickY = ma.Y;
                st.MouseDoubleClicked = ma.Kind == MouseActionKind.DoubleClick;
                break;
            }
            case MouseActionKind.Drag:
                st.MouseMoved = true;
                break;
            case MouseActionKind.Release:
            {
                for (var b = 0; b < 3; b++)
                {
                    if (st.MouseDown[b])
                    {
                        st.MouseDown[b] = false;
                        st.MouseReleased[b] = true;
                    }
                }
                break;
            }
            case MouseActionKind.ScrollUp:
                st.MouseWheelDelta = 1;
                break;
            case MouseActionKind.ScrollDown:
                st.MouseWheelDelta = -1;
                break;
            case MouseActionKind.ScrollLeft:
            case MouseActionKind.ScrollRight:
            case MouseActionKind.Move:
            case MouseActionKind.None:
                break;
        }
    }

    // ========================================================================
    // Funzioni BASIC MOUSE.* (MOUSEX, MOUSEY, MOUSEBUTTON, ...) — leggono lo
    // stato corrente aggiornato da <see cref="UpdateFromEvent"/>.
    // ========================================================================

    /// <summary>MOUSEX() — colonna corrente del puntatore.</summary>
    public static int MouseX() => S().MouseCol;

    /// <summary>MOUSEY() — riga corrente del puntatore.</summary>
    public static int MouseY() => S().MouseRow;

    /// <summary>MOUSEBUTTON([buttonId]) — senza argomenti bitmask (1=left, 2=middle, 4=right); con id 0..2 stato del tasto.</summary>
    public static int MouseButton(int? buttonId = null)
    {
        var st = S();
        if (buttonId.HasValue)
            return st.MouseDown[Math.Clamp(buttonId.Value, 0, 2)] ? 1 : 0;
        return (st.MouseDown[0] ? 1 : 0) | (st.MouseDown[1] ? 2 : 0) | (st.MouseDown[2] ? 4 : 0);
    }

    /// <summary>MOUSEWHEEL() — delta rotella (1=su, -1=giù, 0=nessuno).</summary>
    public static int MouseWheel() => S().MouseWheelDelta;

    /// <summary>MOUSECLICK() — true se l'ultimo evento è stato un click sinistro.</summary>
    public static bool MouseClick() => S().MousePressed[0];

    /// <summary>MOUSECLICK(x1, y1, x2, y2) — true se l'ultimo click è dentro l'area rettangolare.</summary>
    public static bool MouseClickArea(int x1, int y1, int x2, int y2)
    {
        var st = S();
        var minX = Math.Min(x1, x2);
        var maxX = Math.Max(x1, x2);
        var minY = Math.Min(y1, y2);
        var maxY = Math.Max(y1, y2);
        return st.LastClickX >= minX && st.LastClickX <= maxX &&
               st.LastClickY >= minY && st.LastClickY <= maxY;
    }

    /// <summary>MOUSEDBLCLICK() — true se l'ultimo evento è stato un doppio click.</summary>
    public static bool MouseDblClick() => S().MouseDoubleClicked;

    /// <summary>MOUSEMOVE() — true se il mouse si è mosso con un tasto premuto (drag).</summary>
    public static bool MouseMove() => S().MouseMoved;

    /// <summary>MOUSEPRESENT() — true se è presente un mouse.</summary>
    public static bool MousePresent() => S().MousePresent;

    /// <summary>MOUSELEFT() — true se il tasto sinistro è premuto.</summary>
    public static bool MouseLeft() => S().MouseDown[0];

    /// <summary>MOUSERIGHT() — true se il tasto destro è premuto.</summary>
    public static bool MouseRight() => S().MouseDown[2];

    /// <summary>MOUSEMIDDLE() — true se il tasto centrale è premuto.</summary>
    public static bool MouseMiddle() => S().MouseDown[1];
}
