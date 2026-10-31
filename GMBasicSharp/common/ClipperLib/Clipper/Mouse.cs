using CommonLib.Screens;
namespace ClipperLib.Clipper;

// Note: MouseButton is already defined in ConCodes.cs (Left, Right, Middle, Any).
// ANSI mouse encoding uses 0=left, 1=middle, 2=right, so map accordingly.

/// <summary>Wheel scroll direction.</summary>
public enum ScrollDirection
{
    Up,
    Down,
    Left,
    Right,
}

/// <summary>Recognized mouse action kinds (coordinate-carrying).</summary>
public enum MouseActionKind
{
    Click,
    DoubleClick,
    RightClick,
    MiddleClick,
    Drag,
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight,
    Release,
    None,
    Move,
    /// <summary>Tercero click rapido sulla stessa cella (seleziona la riga).</summary>
    TripleClick,
}

/// <summary>
/// A recognized mouse action with 0-based terminal coordinates and an INKEY sentinel.
/// Faithful port of the Rust <c>MouseAction</c> enum (struct form, since C# enums
/// cannot carry data).
/// </summary>
public sealed class MouseAction
{
    public MouseActionKind Kind { get; }
    public int X { get; }
    public int Y { get; }

    /// <summary>Modificatori attivi al momento del click (Shift/Ctrl/Alt) — Fase 7.</summary>
    public KeyModifiers Modifiers { get; }

    public MouseAction(MouseActionKind kind, int x, int y, KeyModifiers modifiers = KeyModifiers.None)
    {
        Kind = kind;
        X = x;
        Y = y;
        Modifiers = modifiers;
    }

    public static MouseAction Click(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.Click, x, y, mods);
    public static MouseAction DoubleClick(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.DoubleClick, x, y, mods);
    public static MouseAction TripleClick(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.TripleClick, x, y, mods);
    public static MouseAction RightClick(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.RightClick, x, y, mods);
    public static MouseAction MiddleClick(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.MiddleClick, x, y, mods);
    public static MouseAction Drag(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.Drag, x, y, mods);
    public static MouseAction ScrollUp(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.ScrollUp, x, y, mods);
    public static MouseAction ScrollDown(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.ScrollDown, x, y, mods);
    public static MouseAction ScrollLeft(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.ScrollLeft, x, y, mods);
    public static MouseAction ScrollRight(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.ScrollRight, x, y, mods);
    public static MouseAction Release(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.Release, x, y, mods);
    public static MouseAction None(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.None, x, y, mods);
    public static MouseAction Move(int x, int y, KeyModifiers mods = KeyModifiers.None) => new(MouseActionKind.Move, x, y, mods);

    /// <summary>
    /// INKEY sentinel consistent with <see cref="KeyConstants.K_MOUSE_BASE"/>:
    /// negative, unique per action kind.
    /// </summary>
    public int InkeyCode => KeyConstants.K_MOUSE_BASE - (int)Kind;

    /// <summary>Reconstruct a MouseAction from an INKEY code, reading coordinates from global state.</summary>
    public static MouseAction? FromInkeyCode(int inkey)
    {
        var raw = KeyConstants.K_MOUSE_BASE - inkey;
        if (Enum.IsDefined(typeof(MouseActionKind), raw))
        {
            var st = ClipperGlobal.State;
            var mods = KeyModifiers.None;
            if (st.MouseShift[0] || st.MouseShift[1] || st.MouseShift[2]) mods |= KeyModifiers.Shift;
            if (st.MouseCtrl[0] || st.MouseCtrl[1] || st.MouseCtrl[2]) mods |= KeyModifiers.Control;
            return new MouseAction((MouseActionKind)raw, st.MouseX, st.MouseY, mods);
        }
        return null;
    }

    public override string ToString() => $"{Kind}({X},{Y})";
}

/// <summary>
/// Decoded (neutral) mouse event from an escape sequence. 0-based coordinates.
/// Faithful port of the crossterm <c>MouseEvent</c> fields used by the Rust
/// <c>process_mouse_event</c>.
/// </summary>
public sealed class MouseEvent
{
    public MouseButton Button;
    public int X;
    public int Y;
    public KeyModifiers Modifiers;
    public bool Released;
    public bool Drag;
    public ScrollDirection? Scroll;
}

/// <summary>
/// Mouse support for the terminal TUI. Decodes SGR (1006) and X10/rxvt (1000/1002)
/// mouse escape sequences into <see cref="MouseEvent"/>, then classifies them into a
/// <see cref="MouseAction"/> (with double-click detection within 400ms / 2 cells).
/// TTY-free: it only parses strings, it never enables capture or writes to the console.
/// </summary>
public static class MouseDecoder
{
    private const int DoubleClickMs = 400;
    private const int DoubleClickDistance = 2;

    private static DateTime _lastClick = DateTime.MinValue;
    private static int _lastClickX;
    private static int _lastClickY;

    // Stato per rilevare il triplo click (terzo clic ravvicinato dopo un doppio clic).
    private static DateTime _lastDouble = DateTime.MinValue;
    private static int _lastDoubleX;
    private static int _lastDoubleY;

    /// <summary>
    /// Decode an escaped mouse sequence (SGR <c>ESC[&lt;...</c> or X10 <c>ESC[M...</c>)
    /// into a <see cref="MouseEvent"/>, or null if it is not a mouse sequence.
    /// Coordinates are returned 0-based to match crossterm semantics.
    /// </summary>
    public static MouseEvent? DecodeMouseSequence(string seq)
    {
        if (string.IsNullOrEmpty(seq) || seq.Length < 3) return null;

        if (seq.StartsWith("\x1b[<"))
        {
            var end = seq.IndexOfAny(['M', 'm']);
            if (end < 0) return null;
            var body = seq[3..end];
            var parts = body.Split(';');
            if (parts.Length < 3) return null;
            if (!int.TryParse(parts[0], out var cb) ||
                !int.TryParse(parts[1], out var x) ||
                !int.TryParse(parts[2], out var y)) return null;
            var released = seq[end] == 'm';
            return FromEncoded(cb, x, y, released);
        }

        if (seq.StartsWith("\x1b[M"))
        {
            if (seq.Length < 6) return null;
            var cb = (byte)seq[3] - 32;
            var x = (byte)seq[4] - 32;
            var y = (byte)seq[5] - 32;
            return FromEncoded(cb, x, y, false);
        }

        return null;
    }

    private static MouseEvent FromEncoded(int cb, int x, int y, bool released)
    {
        // ANSI button encoding: 0=left, 1=middle, 2=right → existing MouseButton enum.
        static MouseButton AnsiButton(int b) => b switch
        {
            0 => MouseButton.Left,
            1 => MouseButton.Middle,
            2 => MouseButton.Right,
            _ => MouseButton.Any,
        };

        var mods = KeyModifiers.None;
        if ((cb & 0b0000_0100) != 0) mods |= KeyModifiers.Shift;
        if ((cb & 0b0000_1000) != 0) mods |= KeyModifiers.Alt;
        if ((cb & 0b0001_0000) != 0) mods |= KeyModifiers.Control;

        // Modifier bits (shift/alt/control) live in 0x1C and must be stripped before
        // classifying button vs wheel.
        var code = cb & ~0b0001_1100;

        var btn = MouseButton.Left;
        var drag = false;
        ScrollDirection? scroll = null;

        if (code is >= 64 and <= 67)
        {
            // wheel: 64 = up, 65 = down, 66 = left, 67 = right
            scroll = code switch
            {
                64 => ScrollDirection.Up,
                65 => ScrollDirection.Down,
                66 => ScrollDirection.Left,
                _ => ScrollDirection.Right,
            };
        }
        else if ((cb & 0b0010_0000) != 0)
        {
            drag = true;
            btn = AnsiButton(cb & 0b0000_0011);
        }
        else
        {
            btn = AnsiButton(cb & 0b0000_0011);
        }

        return new MouseEvent
        {
            Button = btn,
            X = x - 1,
            Y = y - 1,
            Modifiers = mods,
            Released = released,
            Drag = drag,
            Scroll = scroll,
        };
    }

    /// <summary>
    /// Convert a decoded <see cref="MouseEvent"/> into a <see cref="MouseAction"/>,
    /// detecting double-clicks (two left presses within 400ms and 2 cells).
    /// Faithful port of the Rust <c>process_mouse_event</c>.
    /// </summary>
    public static MouseAction ProcessMouseEvent(MouseEvent e)
    {
        if (e.Scroll.HasValue)
        {
            return e.Scroll.Value switch
            {
                ScrollDirection.Up => MouseAction.ScrollUp(e.X, e.Y),
                ScrollDirection.Down => MouseAction.ScrollDown(e.X, e.Y),
                ScrollDirection.Left => MouseAction.ScrollLeft(e.X, e.Y),
                ScrollDirection.Right => MouseAction.ScrollRight(e.X, e.Y),
                _ => MouseAction.None(e.X, e.Y),
            };
        }

        // Motion without any button pressed is hover; with a button it is a drag.
        if (e.Drag) return e.Button == MouseButton.Any ? MouseAction.Move(e.X, e.Y) : MouseAction.Drag(e.X, e.Y);
        if (e.Released) return MouseAction.Release(e.X, e.Y);

        switch (e.Button)
        {
            case MouseButton.Right:
                return MouseAction.RightClick(e.X, e.Y, e.Modifiers);
            case MouseButton.Middle:
                return MouseAction.MiddleClick(e.X, e.Y, e.Modifiers);
            default:
                // Left press: detect triple (third quick click after a double), then double.
                var now = DateTime.Now;

                // Terzo clic: rapido e vicino all'ultimo doppio clic → TripleClick.
                if ((now - _lastDouble).TotalMilliseconds <= DoubleClickMs &&
                    Math.Abs(e.X - _lastDoubleX) <= DoubleClickDistance &&
                    Math.Abs(e.Y - _lastDoubleY) <= DoubleClickDistance)
                {
                    _lastDouble = DateTime.MinValue;
                    return MouseAction.TripleClick(e.X, e.Y, e.Modifiers);
                }

                if ((now - _lastClick).TotalMilliseconds <= DoubleClickMs &&
                    Math.Abs(e.X - _lastClickX) <= DoubleClickDistance &&
                    Math.Abs(e.Y - _lastClickY) <= DoubleClickDistance)
                {
                    _lastClick = DateTime.MinValue;
                    _lastDouble = now;
                    _lastDoubleX = e.X;
                    _lastDoubleY = e.Y;
                    return MouseAction.DoubleClick(e.X, e.Y, e.Modifiers);
                }

                _lastClick = now;
                _lastClickX = e.X;
                _lastClickY = e.Y;
                return MouseAction.Click(e.X, e.Y, e.Modifiers);
        }
    }

    /// <summary>Resetta lo stato interno di rilevamento click (utile nei test).</summary>
    public static void ResetClickState()
    {
        _lastClick = DateTime.MinValue;
        _lastDouble = DateTime.MinValue;
    }

    /// <summary>Decode a mouse sequence and classify it in one step.</summary>
    public static MouseAction? DecodeAndProcess(string seq)
    {
        var e = DecodeMouseSequence(seq);
        return e == null ? null : ProcessMouseEvent(e);
    }

    /// <summary>
    /// Create a MouseEvent from X10-style raw bytes (cb, x, y are already decoded
    /// from the escape sequence). Used when reading mouse events character-by-character.
    /// </summary>
    public static MouseEvent FromEncodedStyle(int cb, int x, int y, bool released)
    {
        return FromEncoded(cb, x, y, released);
    }
}
