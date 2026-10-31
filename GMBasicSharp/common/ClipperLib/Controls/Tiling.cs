using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

// Clean-room port di `pop-tiler` (System76, MPL-2.0).
//
// Ritetuto il cuore dell'engine di tiling: un albero binario di `ForkNode`
// (split con orientamento + ratio) le cui foglie sono `Stack` di finestre
// (tabbed), navigazione direzionale geometrica (`WindowInDirection`),
// swap/move direzionale, toggle dell'orientamento mantenendo il ratio, e
// resize del fork adiacente. Riscritto in C# puro: al posto di
// `Rc<RefCell<..>>` si usano riferimenti diretti a oggetti (`ForkNode`/`Stack`).

/// <summary>Identificatore di finestra.</summary>
public readonly struct WindowId : IEquatable<WindowId>
{
    public ulong Id { get; }
    public WindowId(ulong id) => Id = id;
    public bool Equals(WindowId other) => Id == other.Id;
    public override bool Equals(object? obj) => obj is WindowId w && Equals(w);
    public override int GetHashCode() => Id.GetHashCode();
    public static bool operator ==(WindowId a, WindowId b) => a.Equals(b);
    public static bool operator !=(WindowId a, WindowId b) => !a.Equals(b);
    public override string ToString() => $"W{Id}";
}

/// <summary>Orientamento di uno split.</summary>
public enum TilingOrientation
{
    /// <summary>Split orizzontale: i due lati sono affiancati (sinistra/destra).</summary>
    H,
    /// <summary>Split verticale: i due lati sono sovrapposti (sopra/sotto).</summary>
    V
}

public static class TilingOrientationExt
{
    public static TilingOrientation Flip(this TilingOrientation o) =>
        o == TilingOrientation.H ? TilingOrientation.V : TilingOrientation.H;
}

/// <summary>Direzione per il focus/move direzionale.</summary>
public enum Direction
{
    Left,
    Right,
    Above,
    Below
}

public static class DirectionExt
{
    public static Direction Opposite(this Direction d) => d switch
    {
        Direction.Left => Direction.Right,
        Direction.Right => Direction.Left,
        Direction.Above => Direction.Below,
        Direction.Below => Direction.Above,
        _ => d
    };
}

/// <summary>Un insieme tabbed di finestre (foglia dell'albero di tiling).</summary>
public class Stack
{
    public List<WindowId> Windows;
    public int? Focused;

    public Stack(List<WindowId> windows, int? focused)
    {
        Windows = new List<WindowId>(windows);
        Focused = focused.HasValue && focused.Value < Windows.Count ? focused : null;
    }

    public IReadOnlyList<WindowId> GetWindows() => Windows;
    public int? FocusedIndex() => Focused;
    public WindowId? FocusedWindow() => Focused.HasValue ? Windows[Focused.Value] : null;

    /// <summary>Aggiunge `w`; se `focus` porta il focus sulla nuova finestra. Ritorna l'indice.</summary>
    public int AddWindow(WindowId w, bool focus)
    {
        Windows.Add(w);
        var idx = Windows.Count - 1;
        if (focus || !Focused.HasValue)
            Focused = idx;
        return idx;
    }

    /// <summary>Rimuove `w`. Ritorna la finestra su cui spostare il focus, se ce n'e' una.</summary>
    public WindowId? RemoveWindow(WindowId w)
    {
        var pos = Windows.IndexOf(w);
        if (pos < 0) return null;
        Windows.RemoveAt(pos);
        if (Windows.Count == 0)
        {
            Focused = null;
            return null;
        }
        var newFocus = Math.Min(pos, Windows.Count - 1);
        Focused = newFocus;
        return Windows[newFocus];
    }

    /// <summary>Cicla il focus di `dir` posizioni (modulo lunghezza).</summary>
    public WindowId? CycleFocus(int dir)
    {
        if (Windows.Count == 0) return null;
        var len = Windows.Count;
        var cur = Focused ?? 0;
        var next = ((cur + dir) % len + len) % len;
        Focused = next;
        return Windows[next];
    }

    /// <summary>Inserisce `w` subito dopo la finestra `after` (o in testa se assente).</summary>
    public void InsertAfter(WindowId w, WindowId after, bool focus)
    {
        var pos = Windows.IndexOf(after);
        pos = pos < 0 ? 0 : pos + 1;
        Windows.Insert(pos, w);
        if (focus) Focused = pos;
    }
}

/// <summary>Nodo dell'albero di tiling.</summary>
public abstract class TilerNode<T>
{
    public abstract List<KeyValuePair<WindowId, ScreenRect>> Layout(ScreenRect rect);
    public abstract Stack? FindStack(WindowId w);
    public abstract WindowId? RemoveWindow(WindowId w);
    public abstract ForkNode<T>? ParentForkOf(WindowId w);
    public abstract bool ContainsWindow(WindowId w);
}

/// <summary>Split binario tra due sotto-nodi.</summary>
public class ForkNode<T> : TilerNode<T>
{
    public TilingOrientation SplitOrientation;
    public float Ratio;
    public TilerNode<T> First;
    public TilerNode<T> Second;

    public ForkNode(TilingOrientation orientation, float ratio, TilerNode<T> first, TilerNode<T> second)
    {
        SplitOrientation = orientation;
        Ratio = TilingMath.Clamp(ratio, 0.05f, 0.95f);
        First = first;
        Second = second;
    }

    public override List<KeyValuePair<WindowId, ScreenRect>> Layout(ScreenRect rect)
    {
        var (fr, sr) = TilingMath.SplitRect(rect, SplitOrientation, Ratio);
        var result = First.Layout(fr);
        result.AddRange(Second.Layout(sr));
        return result;
    }

    public override Stack? FindStack(WindowId w)
    {
        if (First.ContainsWindow(w)) return First.FindStack(w);
        if (Second.ContainsWindow(w)) return Second.FindStack(w);
        return null;
    }

    public override WindowId? RemoveWindow(WindowId w)
    {
        if (First.ContainsWindow(w)) return First.RemoveWindow(w);
        return Second.RemoveWindow(w);
    }

    public override ForkNode<T>? ParentForkOf(WindowId w)
    {
        var inFirst = First.ContainsWindow(w);
        var inSecond = Second.ContainsWindow(w);
        if (inFirst || inSecond) return this;
        return First.ParentForkOf(w) ?? Second.ParentForkOf(w);
    }

    public override bool ContainsWindow(WindowId w) =>
        First.ContainsWindow(w) || Second.ContainsWindow(w);
}

/// <summary>Foglia: uno stack tabbed di finestre.</summary>
public class StackNode<T> : TilerNode<T>
{
    public Stack Stack;

    public StackNode(Stack stack) => Stack = stack;

    public override List<KeyValuePair<WindowId, ScreenRect>> Layout(ScreenRect rect)
    {
        var result = new List<KeyValuePair<WindowId, ScreenRect>>();
        var focused = Stack.FocusedWindow();
        foreach (var w in Stack.Windows)
        {
            if (focused.HasValue && w.Equals(focused.Value))
                result.Add(new KeyValuePair<WindowId, ScreenRect>(w, rect));
            else
                result.Add(new KeyValuePair<WindowId, ScreenRect>(w, new ScreenRect(rect.X, rect.Y, 0, 0)));
        }
        return result;
    }

    public override Stack? FindStack(WindowId w) =>
        Stack.Windows.Contains(w) ? Stack : null;

    public override WindowId? RemoveWindow(WindowId w) => Stack.RemoveWindow(w);

    public override ForkNode<T>? ParentForkOf(WindowId w) => null;

    public override bool ContainsWindow(WindowId w) => Stack.Windows.Contains(w);
}

/// <summary>Engine di tiling (stile pop-tiler).</summary>
public class Tiler<T>
{
    private readonly Dictionary<WindowId, T> _windows = new();
    private TilerNode<T>? _root;
    private WindowId? _focused;
    private ulong _nextId;
    public ScreenRect WorkArea = new(0, 0, 80, 24);

    public Tiler() { }

    public void SetWorkArea(ScreenRect rect) => WorkArea = rect;
    public int Count => _windows.Count;
    public bool IsEmpty => _windows.Count == 0;
    public List<WindowId> WindowIds()
    {
        var l = new List<WindowId>(_windows.Keys);
        return l;
    }
    public bool Contains(WindowId w) => _windows.ContainsKey(w);
    public WindowId? FocusedWindow() => _focused;

    /// <summary>Aggiunge una finestra (con payload `data`) e la focalizza.</summary>
    public WindowId AddWindow(T data)
    {
        var id = new WindowId(_nextId++);
        _windows[id] = data;
        if (_root == null)
        {
            _root = new StackNode<T>(new Stack([id], 0));
        }
        else
        {
            var newStack = new StackNode<T>(new Stack([id], 0));
            _root = new ForkNode<T>(TilingOrientation.H, 0.5f, _root, newStack);
        }
        _focused = id;
        return id;
    }

    /// <summary>Rimuove una finestra; se era focalizzata, sposta il focus altrove.</summary>
    public T RemoveWindow(WindowId w)
    {
        if (!_windows.ContainsKey(w)) return default!;
        var replacement = _root?.RemoveWindow(w);
        _windows.Remove(w);
        if (_root is StackNode<T> { Stack.Windows.Count: 0 })
            _root = null;
        if (_root != null)
            _root = Collapse(_root)!;
        if (_focused.HasValue && _focused.Value.Equals(w))
        {
            _focused = replacement.HasValue
                ? replacement
                : FirstKey();
        }
        return default!;
    }

    /// <summary>Sposta il focus su `w`.</summary>
    public bool FocusWindow(WindowId w)
    {
        if (_windows.ContainsKey(w))
        {
            _focused = w;
            return true;
        }
        return false;
    }

    /// <summary>Cicla il focus tra le finestre dello stack corrente.</summary>
    public WindowId? CycleFocus(int dir)
    {
        if (!_focused.HasValue) return null;
        var st = _root?.FindStack(_focused.Value);
        if (st == null) return null;
        var next = st.CycleFocus(dir);
        if (next.HasValue) _focused = next;
        return next;
    }

    /// <summary>Calcola la disposizione delle finestre focalizzate sull'area di lavoro.</summary>
    public Dictionary<WindowId, ScreenRect> Layout()
    {
        var map = new Dictionary<WindowId, ScreenRect>();
        if (_root != null)
        {
            foreach (var (id, r) in _root.Layout(WorkArea))
            {
                if (r is { W: > 0, H: > 0 })
                    map[id] = r;
            }
        }
        return map;
    }

    /// <summary>Calcola la disposizione in un'area esplicita.</summary>
    public Dictionary<WindowId, ScreenRect> Layout(int x, int y, int w, int h) =>
        Layout(new ScreenRect(x, y, w, h));

    public Dictionary<WindowId, ScreenRect> Layout(ScreenRect area)
    {
        var prev = WorkArea;
        WorkArea = area;
        var m = Layout();
        WorkArea = prev;
        return m;
    }

    /// <summary>Ritorna la finestra piu' vicina a `w` nella direzione `dir`.</summary>
    public WindowId? WindowInDirection(WindowId w, Direction dir)
    {
        var layout = Layout();
        if (!layout.TryGetValue(w, out var target)) return null;
        var candidates = new List<KeyValuePair<WindowId, ScreenRect>>();
        foreach (var kv in layout)
            if (!kv.Key.Equals(w)) candidates.Add(kv);

        WindowId? best = null;
        var bestScore = float.MaxValue;
        foreach (var (id, r) in candidates)
        {
            var ok = dir switch
            {
                Direction.Left => TilingMath.Right(r) <= TilingMath.Left(target) && TilingMath.OverlapV(r, target),
                Direction.Right => TilingMath.Left(r) >= TilingMath.Right(target) && TilingMath.OverlapV(r, target),
                Direction.Above => TilingMath.Bottom(r) <= TilingMath.Top(target) && TilingMath.OverlapH(r, target),
                Direction.Below => TilingMath.Top(r) >= TilingMath.Bottom(target) && TilingMath.OverlapH(r, target),
                _ => false
            };
            if (!ok) continue;
            var score = TilingMath.DirectionalScore(target, r, dir);
            if (score < bestScore) { bestScore = score; best = id; }
        }
        if (best == null)
        {
            foreach (var (id, r) in candidates)
            {
                var score = TilingMath.CenterDistance(target, r);
                if (score < bestScore) { bestScore = score; best = id; }
            }
        }
        return best;
    }

    /// <summary>Sposta `w` nella direzione `dir` (reattacca lo stack bersaglio).</summary>
    public bool MoveInDirection(WindowId w, Direction dir)
    {
        var target = WindowInDirection(w, dir);
        if (target == null) return false;
        var targetStack = _root?.FindStack(target.Value);
        if (targetStack == null) return false;
        var src = _root?.FindStack(w);
        src?.RemoveWindow(w);
        targetStack.InsertAfter(w, target.Value, true);
        _focused = w;
        return true;
    }

    /// <summary>Inverte l'orientamento del fork che contiene `w`.</summary>
    public void ToggleTilingOrientation(WindowId w)
    {
        var fork = _root?.ParentForkOf(w);
        if (fork != null) fork.SplitOrientation = fork.SplitOrientation.Flip();
    }

    /// <summary>Ritorna l'orientamento del fork contenente `w` (se presente).</summary>
    public TilingOrientation? TilingOrientationOf(WindowId w)
    {
        var fork = _root?.ParentForkOf(w);
        return fork?.SplitOrientation;
    }

    /// <summary>Ridimensiona il fork contenente `w` spostandone il ratio di `delta`.</summary>
    public void ResizeWindowInDirection(WindowId w, Direction dir, float delta)
    {
        var fork = _root?.ParentForkOf(w);
        if (fork == null) return;
        var applicable = fork.SplitOrientation switch
        {
            TilingOrientation.H => dir is Direction.Left or Direction.Right,
            TilingOrientation.V => dir is Direction.Above or Direction.Below,
            _ => false
        };
        if (!applicable) return;
        var sign = dir is Direction.Right or Direction.Below ? 1.0f : -1.0f;
        fork.Ratio = TilingMath.Clamp(fork.Ratio + sign * delta, 0.05f, 0.95f);
    }

    /// <summary>Accesso in lettura al payload di una finestra.</summary>
    public T Get(WindowId w) => _windows.TryGetValue(w, out var v) ? v : default!;
    public bool TryGet(WindowId w, out T? value) => _windows.TryGetValue(w, out value);
    public T? this[WindowId w] => Get(w);

    private WindowId? FirstKey()
    {
        foreach (var k in _windows.Keys) return k;
        return null;
    }

    private static TilerNode<T>? Collapse(TilerNode<T> node)
    {
        if (node is ForkNode<T> f)
        {
            var first = Collapse(f.First);
            var second = Collapse(f.Second);
            if (IsEmptyStack(first)) return second;
            if (IsEmptyStack(second)) return first;
            return new ForkNode<T>(f.SplitOrientation, f.Ratio, first!, second!);
        }
        return node;
    }

    private static bool IsEmptyStack(TilerNode<T>? node) =>
        node is StackNode<T> { Stack.Windows.Count: 0 };
}

/// <summary>Helper geometrici puri (Right/Bottom esclusivi, coerenti con Rect Rust).</summary>
internal static class TilingMath
{
    public static int Left(ScreenRect r) => r.X;
    public static int Top(ScreenRect r) => r.Y;
    public static int Right(ScreenRect r) => r.X + r.W;
    public static int Bottom(ScreenRect r) => r.Y + r.H;

    public static bool OverlapV(ScreenRect a, ScreenRect b) =>
        a.Y < Bottom(b) && Bottom(a) > b.Y;
    public static bool OverlapH(ScreenRect a, ScreenRect b) =>
        a.X < Right(b) && Right(a) > b.X;

    public static (float, float) Center(ScreenRect r) =>
        (r.X + r.W / 2.0f, r.Y + r.H / 2.0f);

    public static float DirectionalScore(ScreenRect target, ScreenRect cand, Direction dir)
    {
        var (tx, ty) = Center(target);
        var (cx, cy) = Center(cand);
        var gap = dir switch
        {
            Direction.Left => Math.Max(0f, tx - Right(cand)),
            Direction.Right => Math.Max(0f, Left(cand) - Right(target)),
            Direction.Above => Math.Max(0f, ty - Bottom(cand)),
            Direction.Below => Math.Max(0f, Top(cand) - Bottom(target)),
            _ => 0f
        };
        var perp = dir switch
        {
            Direction.Left or Direction.Right => Math.Abs(cy - ty),
            _ => Math.Abs(cx - tx)
        };
        return gap + perp * 0.5f;
    }

    public static float CenterDistance(ScreenRect a, ScreenRect b)
    {
        var (ax, ay) = Center(a);
        var (bx, by) = Center(b);
        float dx = ax - bx, dy = ay - by;
        return (float)Math.Sqrt(dx * dx + dy * dy);
    }

    public static (ScreenRect, ScreenRect) SplitRect(ScreenRect r, TilingOrientation o, float ratio)
    {
        ratio = Clamp(ratio, 0.05f, 0.95f);
        if (o == TilingOrientation.H)
        {
            var w1 = Math.Max(1, (int)Math.Round(r.W * ratio));
            var w2 = Math.Max(0, r.W - w1);
            return (new ScreenRect(r.X, r.Y, w1, r.H),
                    new ScreenRect(r.X + w1, r.Y, w2, r.H));
        }
        else
        {
            var h1 = Math.Max(1, (int)Math.Round(r.H * ratio));
            var h2 = Math.Max(0, r.H - h1);
            return (new ScreenRect(r.X, r.Y, r.W, h1),
                    new ScreenRect(r.X, r.Y + h1, r.W, h2));
        }
    }

    public static float Clamp(float v, float lo, float hi) =>
        v < lo ? lo : v > hi ? hi : v;
}
