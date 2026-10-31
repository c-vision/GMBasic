using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public abstract class Control
{
    public static bool BlinkOn { get; set; } = true;

    public string Id { get; set; } = "";
    public int X { get; set; }
    public int Y { get; set; }
    public int Width { get; set; }
    public int Height { get; set; }
    public virtual bool Focused { get; set; }
    public bool Visible { get; set; } = true;
    public bool Enabled { get; set; } = true;
    public bool TabStop { get; set; } = true;
    public int TabIndex { get; set; }
    public ControlStyle Style { get; set; } = ControlStyle.Default();

    /// <summary>Trasforma il testo in maiuscolo quando il controllo lo gestisce.</summary>
    public bool Uppercase { get; set; }

    /// <summary>Stato di errore (es. validazione fallita): attiva i colori di errore.</summary>
    public bool HasError { get; set; }

    public abstract void Draw(ScreenBuffer buffer);

    public abstract bool HandleKey(int key);

    /// <summary>Legacy mouse handler. Sovrascrivi HandleMouse per tutti i tipi di azione.</summary>
    public virtual bool HandleClick(int x, int y) => false;

    /// <summary>
    /// Gestisce un evento mouse completo con tipo di azione.
    /// Default: reindirizza Click/DoubleClick a HandleClick(x,y).
    /// Sovrascrivi per gestire Scroll, Drag, RightClick, Release.
    /// </summary>
    public virtual bool HandleMouse(MouseAction action)
    {
        if (action.Kind is MouseActionKind.Click or MouseActionKind.DoubleClick)
            return HandleClick(action.X, action.Y);
        return false;
    }

    /// <summary>
    /// Punto di ingresso unificato: instrada codici INKEY mouse a HandleMouse,
    /// tasti normali a HandleKey. Usalo nei loop interattivi al posto di HandleKey.
    /// </summary>
    public bool HandleInput(int key)
    {
        if (key < 0 && MouseAction.FromInkeyCode(key) is MouseAction ma)
            return HandleMouse(ma);
        return HandleKey(key);
    }

    /// <summary>
    /// Gestisce un UiEvent unificato (crossterm-style). Sovrascrivi per comportamento personalizzato.
    /// Default: istruisce HandleKey/HandleMouse in base al tipo di evento.
    /// </summary>
    public virtual bool Navigate(UiEvent evt)
    {
        return evt switch
        {
            UiKey(var key, _) => HandleKey(key),
            UiMouse(var action) => HandleMouse(action),
            _ => false,
        };
    }

    public virtual ScreenRect Rect => new(X, Y, Width, Math.Max(1, Height));

    /// <summary>Applica la trasformazione maiuscolo se richiesta.</summary>
    protected string ApplyCase(string s) => Uppercase ? s.ToUpperInvariant() : s;

    public Control WithStyle(ControlStyle style) { Style = style; return this; }
    public Control WithColors(CellColor fg, CellColor bg) { Style = Style.WithColors(fg, bg); return this; }
    public Control WithFocusColors(CellColor fg, CellColor bg) { Style = Style.WithFocusColors(fg, bg); return this; }
    public Control WithErrorColors(CellColor fg, CellColor bg) { Style = Style.WithErrorColors(fg, bg); return this; }
    public Control WithSelectedColors(CellColor fg, CellColor bg) { Style = Style.WithSelectedColors(fg, bg); return this; }
    public Control WithFillChar(char fill) { Style = Style.WithFill(fill); return this; }
    public Control WithUppercase(bool up = true) { Uppercase = up; return this; }
}
