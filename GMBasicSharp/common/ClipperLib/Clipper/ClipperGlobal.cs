using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

public static class ClipperGlobal
{
    /* PIGRO: `new ClipperState()` alloca i buffer di schermo in C (`gmb_screen_new` →
     * libgmb_gfx, che a sua volta porta SDL/audio/codec). Il campo si inizializzava al primo
     * tocco della classe (all'avvio) e trascinava ~26 MB anche per un `PRINT`. */
    private static ClipperState? _state;

    public static ClipperState State => _state ??= new();

    public static void SetScreenSize(int rows, int cols)
    {
        State.Buffer.Resize(cols, rows);
    }

    public static (int rows, int cols) ScreenSize() =>
        (State.Buffer.Height, State.Buffer.Width);
}
