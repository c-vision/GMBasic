using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

public static class KeyConstants
{
    public const int K_LEFT = 19;
    public const int K_RIGHT = 4;
    public const int K_UP = 5;
    public const int K_DOWN = 24;
    public const int K_ENTER = 13;
    public const int K_RETURN = 13;
    public const int K_ESC = 27;
    public const int K_TAB = 9;
    public const int K_BTAB = 15;
    public const int K_BS = 127;
    public const int K_INS = 22;
    public const int K_DEL = 7;
    public const int K_HOME = 1;
    public const int K_END = 6;
    public const int K_PGUP = 18;
    public const int K_PGDN = 3;
    public const int K_MOUSE_BASE = -1000;

    /// <summary>Decodifica una key code di mouse (x, y, button, pressed).</summary>
    public static (int x, int y, int button, bool pressed) DecodeMouseKey(int key)
    {
        var raw = K_MOUSE_BASE - key;
        var p = raw % 10;
        var btn = (raw / 10) % 100;
        var yy = (raw / 1000) % 1000;
        var xx = raw / 1000000;
        return (xx, yy, btn, p != 0);
    }

    // I codici F-Key (300..309) stanno SOPRA l'intervallo caratteri stampabili
    // (32..126): prima K_F1..K_F10 erano 28..37 e collidevano con spazio (32),
    // '!' (33), '"' (34), '#' (35), '$' (36), '%' (37) — digitare spazio
    // attivava F5. Gli offset relativi restano validi (key - K_F1 = 0..9).
    // (Allineato a GMBasicAI.)
    public const int K_F1 = 300;
    public const int K_F2 = 301;
    public const int K_F3 = 302;
    public const int K_F4 = 303;
    public const int K_F5 = 304;
    public const int K_F6 = 305;
    public const int K_F7 = 306;
    public const int K_F8 = 307;
    public const int K_F9 = 308;
    public const int K_F10 = 309;

    public const int BLINK_MS = 400;

    public static string BORDER_SINGLE = "┌─┐│┘─└│ ";
    public static string BORDER_DOUBLE = "╔═╗║╝═╚║ ";
}
