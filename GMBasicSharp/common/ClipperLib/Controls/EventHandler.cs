using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

/// <summary>Generic navigation action derived from a UI event.</summary>
public enum NavigationAction
{
    None,
    Next,
    Previous,
    First,
    Last,
    PageUp,
    PageDown,
    Accept,
    Cancel,
    ClickAt,
    ScrollUp,
    ScrollDown,
}

/// <summary>
/// Trait-like interface for widgets that react to mouse events.
/// Faithful port of the Rust <c>MouseTarget</c> trait.
/// </summary>
public interface IMouseTarget
{
    bool Contains(int x, int y);
    bool OnClick(int x, int y);
    bool OnDoubleClick(int x, int y) => false;
    bool OnScroll(ScrollDirection direction);
    bool OnDrag(int x, int y);
    bool OnRightClick(int x, int y) => false;
}

/// <summary>
/// Central event → action dispatcher that routes UI events to the focused control
/// through a <see cref="FocusManager"/>. TTY-free: it never reads from or writes to
/// the terminal; the caller feeds it <see cref="UiEvent"/> values (e.g. decoded by
/// <see cref="Clipper.Mouse"/>).
/// </summary>
public class EventHandler
{
    private readonly FocusManager _focus;
    private readonly Dictionary<string, Control> _controls = new();

    public EventHandler(FocusManager focus) => _focus = focus;

    /// <summary>Sets focus to the control with the given id.</summary>
    public void SetFocusById(string id)
    {
        if (_focus.SetFocusById(id))
            SyncFocus();
    }

    /// <summary>Register a control by id (also registers its focus area).</summary>
    public void Register(string id, Control ctrl)
    {
        _controls[id] = ctrl;
        _focus.Register(id, ctrl);
    }

    /// <summary>Currently focused control, or null.</summary>
    public Control? FocusedControl =>
        _focus.CurrentId != null && _controls.TryGetValue(_focus.CurrentId, out var c) ? c : null;

    /// <summary>Route a UI event to the focused control. Returns true if handled.</summary>
    public bool Dispatch(UiEvent e)
    {
        switch (e)
        {
            case UiKey(var key, _):
                {
                    var fc = FocusedControl;
                    if (fc != null)
                    {
                        fc.Focused = true;
                        if (key is KeyConstants.K_TAB or KeyConstants.K_BTAB)
                        {
                            _focus.HandleKey(key);
                            SyncFocus();
                            return true;
                        }
                        if (fc.HandleKey(key)) return true;
                    }
                }
                return false;

            case UiMouse(var action):
                if (action.Kind is MouseActionKind.Click or MouseActionKind.DoubleClick)
                {
                    if (_focus.SetFocusByPoint(action.X, action.Y)) SyncFocus();
                }
                // Dispatch to the focused control first; if unhandled, try the
                // topmost (last-registered) control at the mouse coordinates.
                {
                    var fc = FocusedControl;
                    if (fc != null && fc.HandleMouse(action)) return true;
                    if (_controls.Values.Reverse().FirstOrDefault(c =>
                        c != fc && c.Visible && c.Rect.Contains(action.X, action.Y) && c.Enabled) is Control hit &&
                        hit.HandleMouse(action))
                        return true;
                }
                return false;

            default:
                return false;
        }
    }

    private void SyncFocus()
    {
        foreach (var kv in _controls)
            kv.Value.Focused = kv.Key == _focus.CurrentId;
    }

    /// <summary>Convert a UI event into a generic navigation action.</summary>
    public static NavigationAction EventToNavigation(UiEvent e) => e switch
    {
        UiKey(var key, var mods) => key switch
        {
            KeyConstants.K_UP => NavigationAction.Previous,
            KeyConstants.K_DOWN => NavigationAction.Next,
            KeyConstants.K_TAB => NavigationAction.Next,
            KeyConstants.K_BTAB => NavigationAction.Previous,
            KeyConstants.K_HOME => NavigationAction.First,
            KeyConstants.K_END => NavigationAction.Last,
            KeyConstants.K_PGUP => NavigationAction.PageUp,
            KeyConstants.K_PGDN => NavigationAction.PageDown,
            KeyConstants.K_ENTER => NavigationAction.Accept,
            KeyConstants.K_ESC => NavigationAction.Cancel,
            _ => NavigationAction.None,
        },
        UiMouse(var a) => a.Kind switch
        {
            MouseActionKind.Click or MouseActionKind.DoubleClick => NavigationAction.ClickAt,
            MouseActionKind.ScrollUp => NavigationAction.ScrollUp,
            MouseActionKind.ScrollDown => NavigationAction.ScrollDown,
            _ => NavigationAction.None,
        },
        _ => NavigationAction.None,
    };

    /// <summary>True if the event is a quit request (Esc, q/Q or Ctrl+Q).</summary>
    public static bool IsQuitEvent(UiEvent e) => e is UiKey(var key, var mods) &&
        (key == KeyConstants.K_ESC || key == 'q' || key == 'Q' ||
         (key == 'q' && (mods & KeyModifiers.Control) != 0));

    /// <summary>Reads one event from the terminal (keyboard + mouse).</summary>
    public static UiEvent ReadEvent()
    {
        if (Clipper.ClipperKeyInput.Current.InputRedirected) return new UiNone();
        return Clipper.TerminalReader.ReadEventCore();
    }

    /// <summary>Reads an event with timeout. Returns <see cref="UiNone"/> on timeout.</summary>
    public static UiEvent ReadEventTimeout(int timeoutMs)
    {
        if (Clipper.ClipperKeyInput.Current.InputRedirected) return new UiNone();
        if (!Clipper.TerminalReader.IsEventAvailable(timeoutMs)) return new UiNone();
        return Clipper.TerminalReader.ReadEventCore();
    }
}
