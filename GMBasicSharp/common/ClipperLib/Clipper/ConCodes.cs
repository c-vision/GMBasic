using CommonLib.Consoles.Tui;
using CommonLib.Screens;
// <copyright file="ConCodes.cs" company="ClipperLib">
// Port of the Rust `concodes` crate (ANSI escape encoder + DEC state-machine decoder).
// MIT/Apache-2.0 origin (anes-rs for the parser). TTY-free: no console writes.
// Skipped on purpose: codepage437.rs (CP437 tables) and strip.rs (ANSI string stripping
// — duplicate of functionality that lives elsewhere in the library).
// </copyright>

using System.Text;
// ReSharper disable UnusedMember.Global
// ReSharper disable InconsistentNaming

namespace ClipperLib.Clipper;

// =================================================================================================
// ENCODER
// =================================================================================================

/// <summary>Simple SGR attribute codes (no data-carrying colors).</summary>
public enum SgrCode
{
    Reset = 0,
    Bold = 1,
    HalfBright = 2,
    Underscore = 4,
    Blink = 5,
    Reverse = 7,
    Underline = 21,
    NormalIntensity = 22,
    UnderlineOff = 24,
    BlinkOff = 25,
    ReverseOff = 27,
    Strikethrough = 9,
    Overline = 53,
    DefaultFG = 39,
    DefaultBG = 49,
}

/// <summary>ANSI terminal mode (CSI n h/l).</summary>
public enum Mode : ushort
{
    DECCRM = 3,
    DECIM = 4,
    LFNL = 20,
}

/// <summary>DEC private mode (CSI ? n h/l).</summary>
public enum PrivateMode : ushort
{
    DECCKM = 1,
    DECCOLM = 3,
    DECSCNM = 5,
    DECOM = 6,
    DECAWM = 7,
    DECARM = 8,
    X10MR1 = 9,
    DECTECM = 25,
    X11MR2 = 1000,
}

/// <summary>Mouse reporting mode (DEC private modes).</summary>
public enum MouseMode : ushort
{
    X10 = 9,
    X11 = 1000,
    ButtonEvent = 1002,
    AnyEvent = 1003,
    Utf8 = 1005,
    Sgr = 1006,
    PixelSgr = 1016,
    AlternateScroll = 1007,
}

/// <summary>Complete ANSI ENCODER. Every method returns a string (never writes to the console).</summary>
public static class ConCodes
{
    // ---- low level CSI helpers ----------------------------------------------------------------
    private static string Csi(IEnumerable<uint> codes, char final)
    {
        var sb = new StringBuilder();
        sb.Append(Ansi.CSI);
        sb.Append(string.Join(";", codes));
        sb.Append(final);
        return sb.ToString();
    }

    private static string Csi(uint code, char final) => Csi([code], final);
    private static string DecSet(uint n) => $"\x1b[?{n}h";
    private static string DecReset(uint n) => $"\x1b[?{n}l";
    private static string Osc(string body) => "\x1b]" + body + "\x07";

    // ---- cursor / erase ------------------------------------------------------------------------
    public static string Cuu(ushort n = 1) => Csi(n, 'A');
    public static string Cud(ushort n = 1) => Csi(n, 'B');
    public static string Cuf(ushort n = 1) => Csi(n, 'C');
    public static string Cub(ushort n = 1) => Csi(n, 'D');
    public static string Cnl(ushort n = 1) => Csi(n, 'E');
    public static string Cpl(ushort n = 1) => Csi(n, 'F');
    public static string Cha(ushort n = 1) => Csi(n, 'G');
    public static string Cup(ushort row, ushort col) => Csi([row, col], 'H');
    public static string Ed(ushort mode) => Csi(mode, 'J');
    public static string El(ushort mode) => Csi(mode, 'K');

    // ---- modes ---------------------------------------------------------------------------------
    public static string Sm(Mode m) => Csi((uint)m, 'h');
    public static string Rm(Mode m) => Csi((uint)m, 'l');
    public static string Psm(PrivateMode m) => DecSet((uint)m);
    public static string Prm(PrivateMode m) => DecReset((uint)m);

    // ---- SGR -----------------------------------------------------------------------------------
    private static string SgrParamString(SgrCode a) => a switch
    {
        SgrCode.Reset => "0",
        SgrCode.Bold => "1",
        SgrCode.HalfBright => "2",
        SgrCode.Underscore => "4",
        SgrCode.Blink => "5",
        SgrCode.Reverse => "7",
        SgrCode.Underline => "21",
        SgrCode.NormalIntensity => "22",
        SgrCode.UnderlineOff => "24",
        SgrCode.BlinkOff => "25",
        SgrCode.ReverseOff => "27",
        SgrCode.Strikethrough => "9",
        SgrCode.Overline => "53",
        SgrCode.DefaultFG => "39",
        SgrCode.DefaultBG => "49",
        _ => "0",
    };

    /// <summary>Build one SGR sequence from already-formatted numeric segments (e.g. "38;2;r;g;b").</summary>
    public static string Sgr(params string[] segments) => Ansi.CSI + string.Join(";", segments) + "m";
    public static string Sgr(params SgrCode[] attrs) => Sgr(attrs.Select(SgrParamString).ToArray());
    public static string SgrRaw(params uint[] codes) => Csi(codes, 'm');
    public static string SgrUnderlineStyle(byte n) => Sgr($"4;{Math.Min(n, (byte)5)}");

    private static int? StandardIndex(CellColor c)
    {
        if (c.IsDefault) return null;
        return (c.R, c.G, c.B) switch
        {
            (0, 0, 0) => 0,
            (255, 0, 0) => 1,
            (0, 255, 0) => 2,
            (255, 255, 0) => 3,
            (0, 0, 255) => 4,
            (255, 0, 255) => 5,
            (0, 255, 255) => 6,
            (255, 255, 255) => 7,
            _ => null,
        };
    }

    private static string FgSegment(CellColor c, bool bright)
    {
        if (c.IsDefault) return "39";
        var idx = StandardIndex(c);
        if (idx.HasValue) return (bright ? 90 : 30) + idx.Value + "";
        return $"38;2;{c.R};{c.G};{c.B}";
    }

    private static string BgSegment(CellColor c, bool bright)
    {
        if (c.IsDefault) return "49";
        var idx = StandardIndex(c);
        if (idx.HasValue) return (bright ? 100 : 40) + idx.Value + "";
        return $"48;2;{c.R};{c.G};{c.B}";
    }

    public static string SgrFg(CellColor c) => Sgr(FgSegment(c, false));
    public static string SgrBrightFg(CellColor c) => Sgr(FgSegment(c, true));
    public static string SgrBg(CellColor c) => Sgr(BgSegment(c, false));
    public static string SgrBrightBg(CellColor c) => Sgr(BgSegment(c, true));
    public static string SgrFg256(byte idx) => Sgr($"38;5;{idx}");
    public static string SgrBg256(byte idx) => Sgr($"48;5;{idx}");
    public static string SgrFg24(byte r, byte g, byte b) => Sgr($"38;2;{r};{g};{b}");
    public static string SgrBg24(byte r, byte g, byte b) => Sgr($"48;2;{r};{g};{b}");

    // ---- OSC -----------------------------------------------------------------------------------
    public static string SetTitle(string s) => Osc("2;" + s);
    public static string SetIconName(string s) => Osc("1;" + s);
    public static string SetBoth(string s) => Osc("0;" + s);
    public static string SetWorkingDir(string s) => Osc("7;" + s);
    public static string Notification(string s) => Osc("9;" + s);
    public static string Clipboard(char selector, string data) => Osc($"52;{selector};{data}");
    public static string Hyperlink(string uri, string? parameters = null) => Osc($"8;{parameters ?? ""};{uri}");
    public static string HyperlinkEnd() => Osc("8;;");
    public static string SetColorIndex(byte index, string spec) => Osc($"4;{index};{spec}");
    public static string SetFgColor(string spec) => Osc("10;" + spec);
    public static string SetBgColor(string spec) => Osc("11;" + spec);
    public static string SetCursorColor(string spec) => Osc("12;" + spec);

    // ---- window manipulation (CSI t) -----------------------------------------------------------
    public static string Window(WindowOp op) => Ansi.CSI + WindowBody(op) + "t";

    private static string WindowBody(WindowOp op) => op switch
    {
        WindowDeiconify => "1",
        WindowIconify => "2",
        WindowMove m => $"3;{m.X};{m.Y}",
        WindowResizePixels p => $"4;{p.H};{p.W}",
        WindowRaise => "5",
        WindowLower => "6",
        WindowRefresh => "7",
        WindowResizeChars c => $"8;{c.H};{c.W}",
        WindowMaximizeVert => "9",
        WindowMaximizeHorz => "10",
        WindowMaximize => "11",
        WindowRestore => "13",
        _ => "13",
    };

    // ---- mouse / focus / sync / cursor save-restore / scroll-region -----------------------------
    public static string EnableMouse(MouseMode m) => DecSet((ushort)m);
    public static string DisableMouse(MouseMode m) => DecReset((ushort)m);
    public static string EnableBracketedPaste() => DecSet(2004);
    public static string DisableBracketedPaste() => DecReset(2004);
    public static string EnableFocusReport() => DecSet(1004);
    public static string DisableFocusReport() => DecReset(1004);
    public static string EnableSynchronizedUpdate() => DecSet(2026);
    public static string DisableSynchronizedUpdate() => DecReset(2026);
    public static string SaveCursor() => "\x1b7";
    public static string RestoreCursor() => "\x1b8";
    public static string ScrollRegion(ushort top, ushort bottom) => $"\x1b[{top};{bottom}r";
    public static string LeftRightMargin(ushort left, ushort right) => $"\x1b[{left};{right}s";
    public static string InsertLines(ushort n = 1) => $"\x1b[{n}L";
    public static string DeleteLines(ushort n = 1) => $"\x1b[{n}M";
    public static string InsertChars(ushort n = 1) => $"\x1b[{n}@";
    public static string DeleteChars(ushort n = 1) => $"\x1b[{n}P";
    public static string EraseChars(ushort n = 1) => $"\x1b[{n}X";

    // ---- Kitty keyboard protocol (CSI u) -------------------------------------------------------
    public static class KittyKeyboard
    {
        public const uint Disambiguate = 1;
        public const uint EventTypes = 2;
        public const uint AlternateKeys = 4;
        public const uint AllKeysAsAnsi = 8;
        public const uint AssociatedText = 16;
        public const uint ComposedState = 32;
        public const uint UnicodePoint = 64;
        public const uint StateModifiers = 128;
        public const uint Full = Disambiguate | EventTypes | AlternateKeys | AllKeysAsAnsi | AssociatedText | UnicodePoint;

        public static string Enable(uint flags) => $"\x1b[={flags}u";
        public static string Disable(uint flags) => $"\x1b[>{flags}u";
        public static string Query() => "\x1b[?u";
        public static string EncodeKey(uint codepoint, byte mods) => $"\x1b[{codepoint};{mods}u";

        /// <summary>Map Kitty protocol modifiers (shift=1, alt=2, control=4, super=8, hyper=16, meta=32)
        /// from our <see cref="KeyModifiers"/> bit layout.</summary>
        public static byte ModifiersToKitty(KeyModifiers m)
        {
            byte v = 0;
            if (m.HasFlag(KeyModifiers.Shift)) v |= 1;
            if (m.HasFlag(KeyModifiers.Alt)) v |= 2;
            if (m.HasFlag(KeyModifiers.Control)) v |= 4;
            if (m.HasFlag(KeyModifiers.Meta)) v |= 32;
            if (m.HasFlag(KeyModifiers.Super)) v |= 8;
            if (m.HasFlag(KeyModifiers.Hyper)) v |= 16;
            return v;
        }

        /// <summary>Encode a structured key event as <c>CSI codepoint ; mods u</c>.
        /// Returns null for keys the protocol cannot represent as a codepoint.</summary>
        public static string? EncodeKey(KeyCode key, KeyModifiers mods)
        {
            uint cp = key switch
            {
                KeyChar k => k.Ch,
                KeyEnter => 13,
                KeyTab => 9,
                KeyBackspace => 8,
                KeyEsc => 27,
                KeySpace => 32,
                KeyF f => f.N <= 5 ? 57359u + f.N : 57369u + f.N,
                _ => 0,
            };
            if (cp == 0) return null;
            return EncodeKey(cp, ModifiersToKitty(mods));
        }
    }

    /// <summary>Mouse report ENCODERS for the SGR (1006), X10 (9) and UTF-8 (1005) protocols.
    /// These produce the raw byte sequences a terminal would emit for a <see cref="Mouse"/> event;
    /// the parser decodes them back into <see cref="SeqMouse"/>.</summary>
    public static class MouseEncoder
    {
        private static byte ModsToCb(KeyModifiers m)
        {
            byte b = 0;
            if (m.HasFlag(KeyModifiers.Shift)) b |= 4;
            if (m.HasFlag(KeyModifiers.Alt)) b |= 8;
            if (m.HasFlag(KeyModifiers.Control)) b |= 16;
            return b;
        }

        private static byte ButtonBase(MouseButton btn) => btn switch
        {
            MouseButton.Middle => 1,
            MouseButton.Right => 2,
            _ => 0,
        };

        // SGR button+modifier code (NO 32 offset). Coordinates are 1-based (x+1, y+1) in SGR.
        private static byte SgrCb(Mouse m, KeyModifiers mods, out bool release)
        {
            release = false;
            var cb = ModsToCb(mods);
            switch (m)
            {
                case MouseDown d: cb |= ButtonBase(d.Button); break;
                case MouseUp u: cb |= ButtonBase(u.Button); release = true; break;
                case MouseDrag d: cb |= ButtonBase(d.Button); break;
                case MouseMove: cb |= 32; break;
                case MouseScrollUp: cb |= 64; break;
                case MouseScrollDown: cb |= 65; break;
            }
            return cb;
        }

        // X10 coordinate parts: cb = 32 + button + mods, then coords sent as (x+32, y+32),
        // clamped to 222 so the +32 never exceeds 254.
        private static (byte cb, ushort x, ushort y) X10Parts(Mouse m, KeyModifiers mods)
        {
            var cb = (byte)(ModsToCb(mods) | 32);
            ushort x = 0, y = 0;
            switch (m)
            {
                case MouseDown d: cb |= ButtonBase(d.Button); x = d.X; y = d.Y; break;
                case MouseUp u: cb |= (byte)(ButtonBase(u.Button) | 3); x = u.X; y = u.Y; break;
                case MouseDrag d: cb |= ButtonBase(d.Button); x = d.X; y = d.Y; break;
                case MouseMove mm: x = mm.X; y = mm.Y; break;
                case MouseScrollUp s: cb |= 64; x = s.X; y = s.Y; break;
                case MouseScrollDown s: cb |= 65; x = s.X; y = s.Y; break;
            }
            x = Math.Min(x, (ushort)222);
            y = Math.Min(y, (ushort)222);
            return (cb, x, y);
        }

        /// <summary>SGR (1006): <c>CSI &lt; cb ; x+1 ; y+1 M/m</c>.</summary>
        public static string Sgr(Mouse m, KeyModifiers mods = default)
        {
            var cb = SgrCb(m, mods, out var release);
            var fin = release ? 'm' : 'M';
            (var x, var y) = m switch
            {
                MouseDown d => (d.X, d.Y),
                MouseUp u => (u.X, u.Y),
                MouseDrag d => (d.X, d.Y),
                MouseMove mm => (mm.X, mm.Y),
                MouseScrollUp s => (s.X, s.Y),
                MouseScrollDown s => (s.X, s.Y),
                _ => ((ushort)0, (ushort)0),
            };
            return $"\x1b[<{cb};{x + 1};{y + 1}{fin}";
        }

        /// <summary>X10 (9): <c>CSI M</c> + 3 raw bytes (32+cb, x+32, y+32).</summary>
        public static string X10(Mouse m, KeyModifiers mods = default)
        {
            var (cb, x, y) = X10Parts(m, mods);
            return "\x1b[M" + (char)cb + (char)(x + 32) + (char)(y + 32);
        }

        /// <summary>UTF-8 (1005): like X10 but the two coordinates are UTF-8 encoded.</summary>
        public static string Utf8(Mouse m, KeyModifiers mods = default)
        {
            var (cb, x, y) = X10Parts(m, mods);
            return "\x1b[M" + (char)cb + Utf8Coord(x) + Utf8Coord(y);
        }

        private static string Utf8Coord(ushort v)
        {
            var c = Math.Min(v, (ushort)222) + 32;
            return c < 0x80
                ? ((char)c).ToString()
                : ((char)(0xC0 | (c >> 6))).ToString() + (char)(0x80 | (c & 0x3F));
        }
    }
}

// =================================================================================================
// DECODER TYPES
// =================================================================================================

[Flags]
public enum KeyModifiers : byte
{
    None = 0,
    Shift = 1,
    Control = 2,
    Alt = 4,
    Meta = 8,
    Super = 16,
    Hyper = 32,
}

public abstract record KeyCode;
public sealed record KeyBackspace : KeyCode;
public sealed record KeyEnter : KeyCode;
public sealed record KeyLeft : KeyCode;
public sealed record KeyRight : KeyCode;
public sealed record KeyUp : KeyCode;
public sealed record KeyDown : KeyCode;
public sealed record KeyHome : KeyCode;
public sealed record KeyEnd : KeyCode;
public sealed record KeyPageUp : KeyCode;
public sealed record KeyPageDown : KeyCode;
public sealed record KeyTab : KeyCode;
public sealed record KeyBackTab : KeyCode;
public sealed record KeyDelete : KeyCode;
public sealed record KeyInsert : KeyCode;
public sealed record KeyF(byte N) : KeyCode;
public sealed record KeyChar(char Ch) : KeyCode;
public sealed record KeyNull : KeyCode;
public sealed record KeyEsc : KeyCode;
public sealed record KeyCapsLock : KeyCode;
public sealed record KeyScrollLock : KeyCode;
public sealed record KeyNumLock : KeyCode;
public sealed record KeyPrintScreen : KeyCode;
public sealed record KeyPause : KeyCode;
public sealed record KeyMenu : KeyCode;
public sealed record KeySpace : KeyCode;

public enum MouseButton
{
    Left,
    Right,
    Middle,
    Any,
}

public abstract record Mouse;
public sealed record MouseDown(MouseButton Button, ushort X, ushort Y) : Mouse;
public sealed record MouseUp(MouseButton Button, ushort X, ushort Y) : Mouse;
public sealed record MouseDrag(MouseButton Button, ushort X, ushort Y) : Mouse;
public sealed record MouseScrollUp(ushort X, ushort Y) : Mouse;
public sealed record MouseScrollDown(ushort X, ushort Y) : Mouse;
public sealed record MouseMove(ushort X, ushort Y) : Mouse;

public abstract record SgrColor;
public sealed record SgrColorDefault : SgrColor;
public sealed record SgrColorIndexed(byte Index) : SgrColor;
public sealed record SgrColorRgb(byte R, byte G, byte B) : SgrColor;

public abstract record SgrAttr;
public sealed record SgrReset : SgrAttr;
public sealed record SgrBold : SgrAttr;
public sealed record SgrDim : SgrAttr;
public sealed record SgrItalic : SgrAttr;
public sealed record SgrUnderline : SgrAttr;
public sealed record SgrBlink : SgrAttr;
public sealed record SgrRapidBlink : SgrAttr;
public sealed record SgrReverse : SgrAttr;
public sealed record SgrHidden : SgrAttr;
public sealed record SgrStrike : SgrAttr;
public sealed record SgrPrimaryFont : SgrAttr;
public sealed record SgrAltFont(byte N) : SgrAttr;
public sealed record SgrFraktur : SgrAttr;
public sealed record SgrNotBoldOrDim : SgrAttr;
public sealed record SgrNotItalic : SgrAttr;
public sealed record SgrNotUnderline : SgrAttr;
public sealed record SgrNotBlink : SgrAttr;
public sealed record SgrNotReverse : SgrAttr;
public sealed record SgrNotHidden : SgrAttr;
public sealed record SgrNotStrike : SgrAttr;
public sealed record SgrFg(SgrColor Color) : SgrAttr;
public sealed record SgrFgExtended(SgrColor Color) : SgrAttr;
public sealed record SgrDefaultFg : SgrAttr;
public sealed record SgrBg(SgrColor Color) : SgrAttr;
public sealed record SgrBgExtended(SgrColor Color) : SgrAttr;
public sealed record SgrDefaultBg : SgrAttr;
public sealed record SgrFramed : SgrAttr;
public sealed record SgrEncircled : SgrAttr;
public sealed record SgrOverlined : SgrAttr;
public sealed record SgrNotFramedOrEncircled : SgrAttr;
public sealed record SgrNotOverlined : SgrAttr;

public enum TerminalMode
{
    KeyboardAction = 2,
    InsertReplacement = 4,
    SendReceive = 12,
    KeyPad = 1,
    Insert = 4,
    Origin = 6,
    AutoWrap = 7,
    CursorVisible = 25,
    AltScreen = 47,
    MouseX10 = 9,
    MouseX11 = 1000,
    MouseButtonEvent = 1002,
    MouseAnyEvent = 1003,
    FocusEvent = 1004,
    MouseUtf8 = 1005,
    MouseSgr = 1006,
    MouseUrxvt = 1015,
    MouseSgrPixels = 1016,
    BracketedPaste = 2004,
    Unknown = -1,
}

public abstract record WindowOp;
public sealed record WindowDeiconify : WindowOp;
public sealed record WindowIconify : WindowOp;
public sealed record WindowMove(int X, int Y) : WindowOp;
public sealed record WindowResizePixels(ushort W, ushort H) : WindowOp;
public sealed record WindowRaise : WindowOp;
public sealed record WindowLower : WindowOp;
public sealed record WindowRefresh : WindowOp;
public sealed record WindowResizeChars(ushort W, ushort H) : WindowOp;
public sealed record WindowMaximizeVert : WindowOp;
public sealed record WindowMaximizeHorz : WindowOp;
public sealed record WindowMaximize : WindowOp;
public sealed record WindowRestore : WindowOp;
public sealed record WindowUnknown(ulong Code, IReadOnlyList<ulong> Params) : WindowOp;

public abstract record Osc;
public sealed record OscSetBoth(string Text) : Osc;
public sealed record OscSetIconName(string Text) : Osc;
public sealed record OscSetTitle(string Text) : Osc;
public sealed record OscSetWorkingDir(string Text) : Osc;
public sealed record OscHyperlink(string Params, string Uri) : Osc;
public sealed record OscNotification(string Text) : Osc;
public sealed record OscClipboard(char Selector, string Data) : Osc;
public sealed record OscSetColorIndex(byte Index, string Spec) : Osc;
public sealed record OscSetFgColor(string Spec) : Osc;
public sealed record OscSetBgColor(string Spec) : Osc;
public sealed record OscSetCursorColor(string Spec) : Osc;
public sealed record OscUnknown(byte Code, string Body) : Osc;

public abstract record Sequence;
public sealed record SeqKey(KeyCode Key, KeyModifiers Mods) : Sequence;
public sealed record SeqMouse(Mouse Mouse, KeyModifiers Mods) : Sequence;
public sealed record SeqCursorPosition(ushort X, ushort Y) : Sequence;
public sealed record SeqFocusIn : Sequence;
public sealed record SeqFocusOut : Sequence;
public sealed record SeqPaste(string Text) : Sequence;
public sealed record SeqOsc(Osc Osc) : Sequence;
public sealed record SeqWindowOp(WindowOp Op) : Sequence;
public sealed record SeqPrint(char Ch) : Sequence;
public sealed record SeqExecute(byte Control) : Sequence;
public sealed record SeqSgr(IReadOnlyList<SgrAttr> Attributes) : Sequence;
public sealed record SeqCursorUp(ushort N) : Sequence;
public sealed record SeqCursorDown(ushort N) : Sequence;
public sealed record SeqCursorForward(ushort N) : Sequence;
public sealed record SeqCursorBackward(ushort N) : Sequence;
public sealed record SeqCursorNextLine(ushort N) : Sequence;
public sealed record SeqCursorPrevLine(ushort N) : Sequence;
public sealed record SeqCursorColumn(ushort N) : Sequence;
public sealed record SeqCursorLine(ushort N) : Sequence;
public sealed record SeqEraseInDisplay(byte Mode) : Sequence;
public sealed record SeqEraseInLine(byte Mode) : Sequence;
public sealed record SeqScrollUp(ushort N) : Sequence;
public sealed record SeqScrollDown(ushort N) : Sequence;
public sealed record SeqInsertLines(ushort N) : Sequence;
public sealed record SeqDeleteLines(ushort N) : Sequence;
public sealed record SeqInsertChars(ushort N) : Sequence;
public sealed record SeqDeleteChars(ushort N) : Sequence;
public sealed record SeqSetScrollRegion(ushort Top, ushort Bottom) : Sequence;
public sealed record SeqSaveCursor : Sequence;
public sealed record SeqRestoreCursor : Sequence;
public sealed record SeqSetMode(TerminalMode Mode) : Sequence;
public sealed record SeqResetMode(TerminalMode Mode) : Sequence;
public sealed record SeqSoftReset : Sequence;

// =================================================================================================
// DECODER — DEC state-machine parser (ported from anes-rs)
// =================================================================================================

internal interface IProvider
{
    void ProvideChar(char ch);
    void ProvideExecute(byte b);
    void ProvideEscSequence(char ch);
    void ProvideCsiSequence(ulong[] parameters, int ignoredCount, char ch, bool priv, bool sgr);
    void ProvideOsc(string body);
}

internal sealed class Engine
{
    private const int MaxParameters = 30;
    private const ulong DefaultParameterValue = 0;
    private const int MaxUtf8 = 4;

    private enum State
    {
        Ground, Escape, EscapeIntermediate, CsiEntry, CsiIgnore,
        CsiParameter, CsiIntermediate, Utf8, OscString,
    }

    private readonly ulong[] _parameters = new ulong[MaxParameters];
    private int _parametersCount;
    private ulong _parameter;
    private int _ignoredParametersCount;
    private State _state = State.Ground;
    private bool _private;
    private bool _sgr;
    private readonly byte[] _utf8Points = new byte[MaxUtf8];
    private int _utf8PointsCount;
    private int _utf8PointsExpected;
    private readonly List<byte> _osc = [];

    private static bool IsC0(byte b) => b is <= 0x17 or 0x19 or >= 0x1C and <= 0x1F;

    private void SetState(State s)
    {
        if (s == State.Ground)
        {
            _parametersCount = 0;
            _parameter = DefaultParameterValue;
            _ignoredParametersCount = 0;
            _private = false;
            _sgr = false;
            _utf8PointsCount = 0;
            _utf8PointsExpected = 0;
            _osc.Clear();
        }
        _state = s;
    }

    private void StoreParameter()
    {
        if (_parametersCount < MaxParameters)
        {
            _parameters[_parametersCount] = _parameter;
            _parametersCount++;
        }
        else
        {
            _ignoredParametersCount++;
        }
        _parameter = DefaultParameterValue;
    }

    private bool HandlePossibleEsc(IProvider p, byte b, bool more)
    {
        if (b != 0x1B) return false;
        switch ((_state, more))
        {
            case (State.Ground, true): SetState(State.Escape); break;
            case (State.Ground, false): p.ProvideChar('\x1B'); break;
            case (State.Escape, true): p.ProvideChar('\x1B'); break;
            case (State.Escape, false):
                p.ProvideChar('\x1B');
                p.ProvideChar('\x1B');
                SetState(State.Ground);
                break;
            case (_, true): SetState(State.Escape); break;
            case (_, false):
                p.ProvideChar('\x1B');
                SetState(State.Ground);
                break;
        }
        return true;
    }

    private bool HandlePossibleUtf8(IProvider p, byte b)
    {
        if ((b & 0x80) == 0x00)
        {
            if (b is >= 0x20 and <= 0x7E) p.ProvideChar((char)b);
            else p.ProvideExecute(b);
            return true;
        }
        if ((b & 0xE0) == 0xC0)
        {
            _utf8PointsCount = 1; _utf8Points[0] = b; _utf8PointsExpected = 2; SetState(State.Utf8); return true;
        }
        if ((b & 0xF0) == 0xE0)
        {
            _utf8PointsCount = 1; _utf8Points[0] = b; _utf8PointsExpected = 3; SetState(State.Utf8); return true;
        }
        if ((b & 0xF8) == 0xF0)
        {
            _utf8PointsCount = 1; _utf8Points[0] = b; _utf8PointsExpected = 4; SetState(State.Utf8); return true;
        }
        return false;
    }

    private void DispatchCsi(IProvider p, byte b)
    {
        var arr = new ulong[_parametersCount];
        Array.Copy(_parameters, arr, _parametersCount);
        p.ProvideCsiSequence(arr, _ignoredParametersCount, (char)b, _private, _sgr);
        SetState(State.Ground);
    }

    private void AdvanceGround(IProvider p, byte b)
    {
        if (HandlePossibleUtf8(p, b)) return;
        if (IsC0(b)) p.ProvideExecute(b);
        else if (b is >= 0x20 and <= 0x7F) p.ProvideChar((char)b);
    }

    private void AdvanceEscape(IProvider p, byte b)
    {
        if (b is >= 0x20 and <= 0x2F) SetState(State.EscapeIntermediate);
        else if (b == 0x5B) SetState(State.CsiEntry);
        else if (b == 0x5D) { _osc.Clear(); SetState(State.OscString); }
        else if (b is >= 0x30 and <= 0x4F or >= 0x51 and <= 0x57 or 0x59 or 0x5A or 0x5C or >= 0x60 and <= 0x7E)
        {
            p.ProvideEscSequence((char)b);
            SetState(State.Ground);
        }
        else if (IsC0(b)) p.ProvideExecute(b);
        else if (b != 0x7F) SetState(State.Ground);
    }

    private void AdvanceEscapeIntermediate(IProvider p, byte b)
    {
        if (b is >= 0x20 and <= 0x2F) { /* collect */ }
        else if (b == 0x5B) SetState(State.CsiEntry);
        else if (b is >= 0x30 and <= 0x5A or >= 0x5C and <= 0x7E)
        {
            p.ProvideEscSequence((char)b);
            SetState(State.Ground);
        }
        else if (IsC0(b)) p.ProvideExecute(b);
        else if (b != 0x7F) SetState(State.Ground);
    }

    private void AdvanceCsiEntry(IProvider p, byte b)
    {
        if (b == 0x3B) { StoreParameter(); SetState(State.CsiParameter); }
        else if (b is >= 0x30 and <= 0x39) { _parameter = (ulong)(b - 0x30); SetState(State.CsiParameter); }
        else if (b == 0x3A) SetState(State.CsiIgnore);
        else if (b is >= 0x3C and <= 0x3F) { if (b == 0x3F) _private = true; if (b == 0x3C) _sgr = true; SetState(State.CsiParameter); }
        else if (b is >= 0x40 and <= 0x7E) DispatchCsi(p, b);
        else if (IsC0(b)) p.ProvideExecute(b);
        else if (b != 0x7F) { _parameter = b; StoreParameter(); }
    }

    private void AdvanceCsiIgnore(IProvider p, byte b)
    {
        if (IsC0(b)) p.ProvideExecute(b);
        else if (b is >= 0x20 and <= 0x3F or 0x7F) { /* ignore */ }
        else if (b is >= 0x40 and <= 0x7E) SetState(State.Ground);
        else SetState(State.Ground);
    }

    private void AdvanceCsiParameter(IProvider p, byte b)
    {
        if (b is >= 0x30 and <= 0x39) { _parameter = _parameter * 10 + (ulong)(b - 0x30); }
        else if (b == 0x3B) StoreParameter();
        else if (b is >= 0x40 and <= 0x7E) { StoreParameter(); DispatchCsi(p, b); }
        else if (b is >= 0x20 and <= 0x2F) { StoreParameter(); SetState(State.CsiIntermediate); }
        else if (b == 0x3A) SetState(State.CsiIgnore);
        else if (b is >= 0x3C and <= 0x3F) { _private = true; }
        else if (IsC0(b)) p.ProvideExecute(b);
        else if (b != 0x7F) SetState(State.Ground);
    }

    private void AdvanceCsiIntermediate(IProvider p, byte b)
    {
        if (b is >= 0x20 and <= 0x2F) { /* collect */ }
        else if (b is >= 0x40 and <= 0x7E) DispatchCsi(p, b);
        else if (IsC0(b)) p.ProvideExecute(b);
        else if (b != 0x7F) SetState(State.Ground);
    }

    private void AdvanceUtf8(IProvider p, byte b)
    {
        if ((b & 0xC0) != 0x80) { SetState(State.Ground); return; }
        _utf8Points[_utf8PointsCount] = b;
        _utf8PointsCount++;
        if (_utf8PointsCount == _utf8PointsExpected)
        {
            var s = Encoding.UTF8.GetString(_utf8Points, 0, _utf8PointsCount);
            if (!string.IsNullOrEmpty(s) && s.Length > 0) p.ProvideChar(s[0]);
            SetState(State.Ground);
        }
    }

    private void FinishOsc(IProvider p)
    {
        p.ProvideOsc(Encoding.UTF8.GetString(_osc.ToArray()));
        _osc.Clear();
    }

    private void AdvanceOsc(IProvider p, byte b)
    {
        if (b == 0x07) { FinishOsc(p); SetState(State.Ground); }
        else if (b == 0x1B) { FinishOsc(p); SetState(State.Escape); }
        else _osc.Add(b);
    }

    public void Advance(IProvider p, byte b, bool more)
    {
        if (HandlePossibleEsc(p, b, more)) return;
        switch (_state)
        {
            case State.Ground: AdvanceGround(p, b); break;
            case State.Escape: AdvanceEscape(p, b); break;
            case State.EscapeIntermediate: AdvanceEscapeIntermediate(p, b); break;
            case State.CsiEntry: AdvanceCsiEntry(p, b); break;
            case State.CsiIgnore: AdvanceCsiIgnore(p, b); break;
            case State.CsiParameter: AdvanceCsiParameter(p, b); break;
            case State.CsiIntermediate: AdvanceCsiIntermediate(p, b); break;
            case State.Utf8: AdvanceUtf8(p, b); break;
            case State.OscString: AdvanceOsc(p, b); break;
        }
    }
}

internal static class Parsers
{
    public static Sequence? ParseCharEscO(char ch)
    {
        if (ch is >= 'P' and <= 'S')
            return new SeqKey(new KeyF((byte)(ch - 'P' + 1)), KeyModifiers.None);
        return null;
    }

    public static Sequence? ParseChar(char ch, bool emulator)
    {
        if (emulator) return new SeqPrint(ch);
        KeyCode code = ch switch
        {
            '\r' or '\n' => new KeyEnter(),
            '\t' => new KeyTab(),
            ' ' => new KeySpace(),
            '\x7F' => new KeyBackTab(),
            '\x1B' => new KeyEsc(),
            '\0' => new KeyNull(),
            _ => new KeyChar(ch),
        };
        return new SeqKey(code, KeyModifiers.None);
    }

    public static Sequence? ParseExecute(byte b, bool emulator)
    {
        if (emulator) return new SeqExecute(b);
        return ParseChar((char)b, false);
    }

    public static Sequence? ParseEscSequence(char ch, bool emulator)
    {
        if (emulator)
        {
            return ch switch
            {
                '7' => new SeqSaveCursor(),
                '8' => new SeqRestoreCursor(),
                'c' => new SeqSoftReset(),
                _ => null,
            };
        }
        return new SeqKey(new KeyChar(ch), KeyModifiers.Alt);
    }

    private static ulong P(ulong[] p, int i, ulong def)
    {
        var v = i < p.Length ? p[i] : 0;
        return v == 0 ? def : v;
    }

    private static ushort Pn(ulong[] p, int i)
    {
        var v = P(p, i, 1);
        if (v < 1) v = 1;
        if (v > ushort.MaxValue) v = ushort.MaxValue;
        return (ushort)v;
    }

    private static KeyModifiers DecodeModifierParam(ulong modifier)
    {
        var bits = modifier == 0 ? 0 : modifier - 1;
        var m = KeyModifiers.None;
        if ((bits & 1) != 0) m |= KeyModifiers.Shift;
        if ((bits & 2) != 0) m |= KeyModifiers.Alt;
        if ((bits & 4) != 0) m |= KeyModifiers.Control;
        if ((bits & 8) != 0) m |= KeyModifiers.Meta;
        if ((bits & 16) != 0) m |= KeyModifiers.Super;
        if ((bits & 32) != 0) m |= KeyModifiers.Hyper;
        return m;
    }

    private static KeyModifiers ParseKittyModifiers(ulong? mods)
    {
        var m = KeyModifiers.None;
        if (mods.HasValue)
        {
            var v = mods.Value;
            if ((v & 1) != 0) m |= KeyModifiers.Shift;
            if ((v & 2) != 0) m |= KeyModifiers.Alt;
            if ((v & 4) != 0) m |= KeyModifiers.Control;
            if ((v & 8) != 0) m |= KeyModifiers.Super;
            if ((v & 16) != 0) m |= KeyModifiers.Hyper;
            if ((v & 32) != 0) m |= KeyModifiers.Meta;
        }
        return m;
    }

    private static Sequence? ParseCsiKeyboard(ulong[] p)
    {
        if (p.Length == 0) return null;
        var cp = (uint)p[0];
        if (cp > 0x10FFFF) return null;
        var ch = (char)cp;
        var mods = ParseKittyModifiers(p.Length > 1 ? p[1] : null);
        return new SeqKey(new KeyChar(ch), mods);
    }

    private static Sequence? ParseCsiTilde(ulong[] p)
    {
        if (p.Length == 0) return null;
        var mods = DecodeModifierParam(p.Length > 1 ? p[1] : 1);
        KeyCode code = p[0] switch
        {
            1 or 7 => new KeyHome(),
            2 => new KeyInsert(),
            3 => new KeyDelete(),
            4 or 8 => new KeyEnd(),
            5 => new KeyPageUp(),
            6 => new KeyPageDown(),
            >= 11 and <= 15 => new KeyF((byte)(p[0] - 10)),
            >= 17 and <= 21 => new KeyF((byte)(p[0] - 11)),
            >= 23 and <= 24 => new KeyF((byte)(p[0] - 12)),
            _ => null!,
        };
        if (code is null) return null;
        return new SeqKey(code, mods);
    }

    private static Sequence? ParseCsiCursorPosition(ulong[] p)
    {
        if (p.Length < 2) return null;
        return new SeqCursorPosition((ushort)p[1], (ushort)p[0]);
    }

    private static Sequence? ParseCsiWindowOp(ulong[] p)
    {
        if (p.Length == 0) return null;
        var code = p[0];
        ulong g(int i) => i < p.Length ? p[i] : 0;
        WindowOp op = code switch
        {
            1 => new WindowDeiconify(),
            2 => new WindowIconify(),
            3 => new WindowMove((int)g(1), (int)g(2)),
            4 => new WindowResizePixels((ushort)g(2), (ushort)g(1)),
            5 => new WindowRaise(),
            6 => new WindowLower(),
            7 => new WindowRefresh(),
            8 => new WindowResizeChars((ushort)g(2), (ushort)g(1)),
            9 => new WindowMaximizeVert(),
            10 => new WindowMaximizeHorz(),
            11 => new WindowMaximize(),
            13 => new WindowRestore(),
            _ => new WindowUnknown(code, p.ToArray()),
        };
        return new SeqWindowOp(op);
    }

    private static Sequence? ParseCsiXtermMouse(ulong[] p, char ch)
    {
        if (p.Length < 4) return null;
        var cb = (byte)p[1];
        var cx = (ushort)p[2];
        var cy = (ushort)p[3];
        var up = ch == 'm';

        var mods = KeyModifiers.None;
        if ((cb & 0b0000_0100) != 0) mods |= KeyModifiers.Shift;
        if ((cb & 0b0000_1000) != 0) mods |= KeyModifiers.Alt;
        if ((cb & 0b0001_0000) != 0) mods |= KeyModifiers.Control;

        Mouse mouse;
        if ((cb & 0b0100_0000) != 0)
        {
            mouse = (cb & 0b0000_0001) != 0 ? new MouseScrollDown(cx, cy) : new MouseScrollUp(cx, cy);
        }
        else if ((cb & 0b0010_0000) != 0)
        {
            mouse = new MouseMove(cx, cy);
        }
        else
        {
            var drag = (cb & 0b0010_0000) != 0;
            (MouseButton btn, bool isUp, bool isDrag) key = ((MouseButton)(cb & 0b0000_0011), up, drag);
            mouse = key switch
            {
                (MouseButton.Left, true, _) => new MouseUp(MouseButton.Left, cx, cy),
                (MouseButton.Left, false, false) => new MouseDown(MouseButton.Left, cx, cy),
                (MouseButton.Left, false, true) => new MouseDrag(MouseButton.Left, cx, cy),
                (MouseButton.Right, true, _) => new MouseUp(MouseButton.Right, cx, cy),
                (MouseButton.Right, false, false) => new MouseDown(MouseButton.Right, cx, cy),
                (MouseButton.Right, false, true) => new MouseDrag(MouseButton.Right, cx, cy),
                (MouseButton.Middle, true, _) => new MouseUp(MouseButton.Middle, cx, cy),
                (MouseButton.Middle, false, false) => new MouseDown(MouseButton.Middle, cx, cy),
                (MouseButton.Middle, false, true) => new MouseDrag(MouseButton.Middle, cx, cy),
                _ => null!,
            };
        }
        if (mouse is null) return null;
        return new SeqMouse(mouse, mods);
    }

    private static Sequence? ParseCsiRxvtMouse(ulong[] p)
    {
        if (p.Length < 3) return null;
        var cb = p[0];
        var cx = (ushort)p[1];
        var cy = (ushort)p[2];

        var mods = KeyModifiers.None;
        if ((cb & 0b0000_0100) != 0) mods |= KeyModifiers.Shift;
        if ((cb & 0b0000_1000) != 0) mods |= KeyModifiers.Alt;
        if ((cb & 0b0001_0000) != 0) mods |= KeyModifiers.Control;

        Mouse mouse;
        if ((cb & 0b0110_0000) == 0b0110_0000)
        {
            mouse = (cb & 0b0000_0001) != 0 ? new MouseScrollDown(cx, cy) : new MouseScrollUp(cx, cy);
        }
        else
        {
            var drag = (cb & 0b0100_0000) != 0;
            (byte btn, bool d) key = ((byte)(cb & 0b0000_0011), drag);
            mouse = key switch
            {
                (0b0000_0000, false) => new MouseDown(MouseButton.Left, cx, cy),
                (0b0000_0010, false) => new MouseDown(MouseButton.Right, cx, cy),
                (0b0000_0001, false) => new MouseDown(MouseButton.Middle, cx, cy),
                (0b0000_0000, true) => new MouseDrag(MouseButton.Left, cx, cy),
                (0b0000_0010, true) => new MouseDrag(MouseButton.Right, cx, cy),
                (0b0000_0001, true) => new MouseDrag(MouseButton.Middle, cx, cy),
                (0b0000_0011, false) => new MouseUp(MouseButton.Any, cx, cy),
                _ => null!,
            };
        }
        if (mouse is null) return null;
        return new SeqMouse(mouse, mods);
    }

    private static TerminalMode DecodeMode(ulong code) => code switch
    {
        1 => TerminalMode.KeyPad,
        2 => TerminalMode.KeyboardAction,
        4 => TerminalMode.InsertReplacement,
        6 => TerminalMode.Origin,
        7 => TerminalMode.AutoWrap,
        12 => TerminalMode.SendReceive,
        25 => TerminalMode.CursorVisible,
        47 or 1047 or 1049 => TerminalMode.AltScreen,
        9 => TerminalMode.MouseX10,
        1000 => TerminalMode.MouseX11,
        1002 => TerminalMode.MouseButtonEvent,
        1003 => TerminalMode.MouseAnyEvent,
        1004 => TerminalMode.FocusEvent,
        1005 => TerminalMode.MouseUtf8,
        1006 => TerminalMode.MouseSgr,
        1015 => TerminalMode.MouseUrxvt,
        1016 => TerminalMode.MouseSgrPixels,
        2004 => TerminalMode.BracketedPaste,
        _ => TerminalMode.Unknown,
    };

    public static List<SgrAttr> DecodeSgr(ulong[] ps)
    {
        var outp = new List<SgrAttr>();
        var p = ps.Length == 0 ? [0UL] : ps;
        var i = 0;
        while (i < p.Length)
        {
            var code = p[i];
            switch (code)
            {
                case 0: outp.Add(new SgrReset()); break;
                case 1: outp.Add(new SgrBold()); break;
                case 2: outp.Add(new SgrDim()); break;
                case 3: outp.Add(new SgrItalic()); break;
                case 4: outp.Add(new SgrUnderline()); break;
                case 5: outp.Add(new SgrBlink()); break;
                case 6: outp.Add(new SgrRapidBlink()); break;
                case 7: outp.Add(new SgrReverse()); break;
                case 8: outp.Add(new SgrHidden()); break;
                case 9: outp.Add(new SgrStrike()); break;
                case 10: outp.Add(new SgrPrimaryFont()); break;
                case >= 11 and <= 19: outp.Add(new SgrAltFont((byte)(code - 10))); break;
                case 20: outp.Add(new SgrFraktur()); break;
                case 22: outp.Add(new SgrNotBoldOrDim()); break;
                case 23: outp.Add(new SgrNotItalic()); break;
                case 24: outp.Add(new SgrNotUnderline()); break;
                case 25: outp.Add(new SgrNotBlink()); break;
                case 27: outp.Add(new SgrNotReverse()); break;
                case 28: outp.Add(new SgrNotHidden()); break;
                case 29: outp.Add(new SgrNotStrike()); break;
                case >= 30 and <= 37: outp.Add(new SgrFg(new SgrColorIndexed((byte)(code - 30)))); break;
                case 38:
                    var (fg, fni) = ReadExtendedColor(p, i);
                    outp.Add(new SgrFgExtended(fg));
                    i = fni; break;
                case 39: outp.Add(new SgrDefaultFg()); break;
                case >= 40 and <= 47: outp.Add(new SgrBg(new SgrColorIndexed((byte)(code - 40)))); break;
                case 48:
                    var (bg, bni) = ReadExtendedColor(p, i);
                    outp.Add(new SgrBgExtended(bg));
                    i = bni; break;
                case 49: outp.Add(new SgrDefaultBg()); break;
                case >= 90 and <= 97: outp.Add(new SgrFg(new SgrColorIndexed((byte)(code - 90 + 8)))); break;
                case >= 100 and <= 107: outp.Add(new SgrBg(new SgrColorIndexed((byte)(code - 100 + 8)))); break;
                case 51: outp.Add(new SgrFramed()); break;
                case 52: outp.Add(new SgrEncircled()); break;
                case 53: outp.Add(new SgrOverlined()); break;
                case 54: outp.Add(new SgrNotFramedOrEncircled()); break;
                case 55: outp.Add(new SgrNotOverlined()); break;
            }
            i++;
        }
        return outp;
    }

    private static (SgrColor, int) ReadExtendedColor(ulong[] p, int i)
    {
        var next = i + 1;
        if (next >= p.Length) return (new SgrColorDefault(), next);
        return p[next] switch
        {
            2 => (new SgrColorRgb(
                    (byte)(p.Length > next + 1 ? p[next + 1] : 0),
                    (byte)(p.Length > next + 2 ? p[next + 2] : 0),
                    (byte)(p.Length > next + 3 ? p[next + 3] : 0)), next + 3),
            5 => (new SgrColorIndexed((byte)(p.Length > next + 1 ? p[next + 1] : 0)), next + 1),
            _ => (new SgrColorDefault(), next),
        };
    }

    public static Sequence? ParseCsiSequence(ulong[] p, int ignoredCount, char ch, bool priv, bool emulator, bool sgr = false)
    {
        if (priv)
        {
            return ch switch
            {
                'h' => new SeqSetMode(DecodeMode(p.Length > 0 ? p[0] : 0)),
                'l' => new SeqResetMode(DecodeMode(p.Length > 0 ? p[0] : 0)),
                _ => null,
            };
        }

        if (emulator)
        {
            return ch switch
            {
                'A' => new SeqCursorUp(Pn(p, 0)),
                'B' => new SeqCursorDown(Pn(p, 0)),
                'C' => new SeqCursorForward(Pn(p, 0)),
                'D' => new SeqCursorBackward(Pn(p, 0)),
                'E' => new SeqCursorNextLine(Pn(p, 0)),
                'F' => new SeqCursorPrevLine(Pn(p, 0)),
                'G' or '`' => new SeqCursorColumn(Pn(p, 0)),
                'd' => new SeqCursorLine(Pn(p, 0)),
                'H' or 'f' => new SeqCursorPosition((ushort)P(p, 1, 1), (ushort)P(p, 0, 1)),
                'J' => new SeqEraseInDisplay((byte)P(p, 0, 0)),
                'K' => new SeqEraseInLine((byte)P(p, 0, 0)),
                'L' => new SeqInsertLines(Pn(p, 0)),
                'M' => new SeqDeleteLines(Pn(p, 0)),
                '@' => new SeqInsertChars(Pn(p, 0)),
                'P' => new SeqDeleteChars(Pn(p, 0)),
                'S' => new SeqScrollUp(Pn(p, 0)),
                'T' => new SeqScrollDown(Pn(p, 0)),
                'r' => new SeqSetScrollRegion((ushort)P(p, 0, 1), (ushort)P(p, 1, 0)),
                'm' => new SeqSgr(DecodeSgr(p)),
                's' => new SeqSaveCursor(),
                'u' => new SeqRestoreCursor(),
                _ => null,
            };
        }

        switch (ch)
        {
            case 'A': return new SeqKey(new KeyUp(), DecodeModifierParam(p.Length > 1 ? p[1] : 1));
            case 'B': return new SeqKey(new KeyDown(), DecodeModifierParam(p.Length > 1 ? p[1] : 1));
            case 'C': return new SeqKey(new KeyRight(), DecodeModifierParam(p.Length > 1 ? p[1] : 1));
            case 'D': return new SeqKey(new KeyLeft(), DecodeModifierParam(p.Length > 1 ? p[1] : 1));
            case 'H': return new SeqKey(new KeyHome(), KeyModifiers.None);
            case 'F': return new SeqKey(new KeyEnd(), KeyModifiers.None);
            case 'Z': return new SeqKey(new KeyBackTab(), KeyModifiers.None);
            case 'I': return new SeqFocusIn();
            case 'O': return new SeqFocusOut();
            case 'R': return ParseCsiCursorPosition(p);
            case 'u': return ParseCsiKeyboard(p);
            case 't': return ParseCsiWindowOp(p);
            case 'm': return ParseCsiXtermMouse(p, ch);
            case 'M' when sgr: return ParseCsiXtermMouse(p, ch);
            case 'M': return ParseCsiRxvtMouse(p);
            case '~': return ParseCsiTilde(p);
            default: return null;
        }
    }

    public static Osc? ParseOsc(string body)
    {
        var semi = body.IndexOf(';');
        var codePart = semi >= 0 ? body[..semi] : body;
        if (!uint.TryParse(codePart.Split(':')[0], out var codeVal)) return null;
        var code = (byte)codeVal;
        var restParts = semi >= 0 ? body[(semi + 1)..].Split(';') : [];
        string join() => string.Join(";", restParts);

        switch (code)
        {
            case 0: return new OscSetBoth(join());
            case 1: return new OscSetIconName(join());
            case 2: return new OscSetTitle(join());
            case 7: return new OscSetWorkingDir(join());
            case 8:
                var prm = restParts.Length > 0 ? restParts[0] : "";
                var uri = restParts.Length > 1 ? restParts[1] : "";
                return new OscHyperlink(prm, uri);
            case 9: return new OscNotification(join());
            case 4:
                var idx = restParts.Length > 0 && byte.TryParse(restParts[0], out var ix) ? ix : (byte)0;
                var spec = restParts.Length > 1 ? restParts[1] : "";
                return new OscSetColorIndex(idx, spec);
            case 10: return new OscSetFgColor(join());
            case 11: return new OscSetBgColor(join());
            case 12: return new OscSetCursorColor(join());
            case 52:
                var sel = restParts.Length > 0 && restParts[0].Length > 0 ? restParts[0][0] : 'c';
                var data = restParts.Length > 1 ? restParts[1] : "";
                return new OscClipboard(sel, data);
            default: return new OscUnknown(code, join());
        }
    }
}

internal sealed class SequenceProvider : IProvider
{
    private readonly List<Sequence> _seqs = [];
    private readonly bool _emulator;
    private bool _escO;
    public Action<Sequence>? Callback;

    public SequenceProvider(bool emulator) => _emulator = emulator;
    public List<Sequence> Seqs => _seqs;
    public void Clear() => _seqs.Clear();

    private void Emit(Sequence s)
    {
        _seqs.Add(s);
        Callback?.Invoke(s);
    }

    public void ProvideChar(char ch)
    {
        if (_escO)
        {
            _escO = false;
            var s = Parsers.ParseCharEscO(ch);
            if (s != null) Emit(s);
            return;
        }
        var s2 = Parsers.ParseChar(ch, _emulator);
        if (s2 != null) Emit(s2);
    }

    public void ProvideExecute(byte b)
    {
        var s = Parsers.ParseExecute(b, _emulator);
        if (s != null) Emit(s);
    }

    public void ProvideEscSequence(char ch)
    {
        if (ch == 'O') { _escO = true; return; }
        _escO = false;
        var s = Parsers.ParseEscSequence(ch, _emulator);
        if (s != null) Emit(s);
    }

    public void ProvideCsiSequence(ulong[] parameters, int ignoredCount, char ch, bool priv, bool sgr)
    {
        var s = Parsers.ParseCsiSequence(parameters, ignoredCount, ch, priv, _emulator, sgr);
        if (s != null) Emit(s);
        _escO = false;
    }

    public void ProvideOsc(string body)
    {
        var s = Parsers.ParseOsc(body);
        if (s != null) Emit(new SeqOsc(s));
        _escO = false;
    }
}

/// <summary>
/// ANSI escape DECODER. Feed it a char/string stream via <see cref="Advance(char,bool)"/> and
/// pull decoded <see cref="Sequence"/> values from the queue (or subscribe to <see cref="SequenceEmitted"/>).
/// </summary>
public sealed class Parser
{
    private readonly Engine _engine = new();
    private readonly SequenceProvider _provider;

    public bool Emulator { get; }
    public event Action<Sequence>? SequenceEmitted
    {
        add => _provider.Callback += value;
        remove => _provider.Callback -= value;
    }

    public Parser(bool emulator = false)
    {
        Emulator = emulator;
        _provider = new SequenceProvider(emulator);
    }

    /// <summary>Create a parser in emulator mode (printable chars / C0 controls become output sequences).</summary>
    public static Parser CreateEmulator() => new(true);

    public void Advance(char c, bool more = false) => Advance(new string(c, 1), more);
    public void Advance(ReadOnlySpan<char> chars, bool more = false) => Advance(chars.ToString(), more);
    public void Advance(string s, bool more = false)
    {
        var bytes = Encoding.UTF8.GetBytes(s);
        for (var i = 0; i < bytes.Length; i++)
            _engine.Advance(_provider, bytes[i], i < bytes.Length - 1 || more);
    }

    public int Pending => _provider.Seqs.Count;
    public IReadOnlyList<Sequence> PeekAll() => _provider.Seqs;
    public bool TryDequeue(out Sequence? seq)
    {
        if (_provider.Seqs.Count > 0)
        {
            seq = _provider.Seqs[0];
            _provider.Seqs.RemoveAt(0);
            return true;
        }
        seq = null;
        return false;
    }
    public List<Sequence> Drain()
    {
        var r = new List<Sequence>(_provider.Seqs);
        _provider.Clear();
        return r;
    }
}
