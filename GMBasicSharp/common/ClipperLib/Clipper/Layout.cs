using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Direction along which a layout area is subdivided.
/// </summary>
public enum Direction
{
    Horizontal,
    Vertical,
}

/// <summary>
/// Residual space distribution policy when segments do not fill the area
/// (matches Rust Flex: Legacy expands flexible segments; Start/End/Center pack;
/// SpaceBetween/SpaceAround distribute the gap).
/// </summary>
public enum Flex
{
    Legacy,
    Start,
    End,
    Center,
    SpaceBetween,
    SpaceAround,
}

/// <summary>
/// Size constraint for a layout segment (clean-room port of Rust Constraint).
/// Adds MinMax and Proportion (the proportional/Layout::default style, named
/// after the Rust Fill variant).
/// </summary>
public sealed class Constraint
{
    public enum Kind { Length, Min, Max, Percentage, Ratio, MinMax, Proportion }

    public Kind Tag { get; }
    public int A { get; }
    public int B { get; }

    private Constraint(Kind tag, int a, int b)
    {
        Tag = tag;
        A = a;
        B = b;
    }

    public static Constraint Length(int n) => new Constraint(Kind.Length, n, 0);
    public static Constraint Min(int n) => new Constraint(Kind.Min, n, 0);
    public static Constraint Max(int n) => new Constraint(Kind.Max, n, 0);
    public static Constraint Percentage(int p) => new Constraint(Kind.Percentage, p, 0);
    public static Constraint Ratio(int n, int d) => new Constraint(Kind.Ratio, n, d);
    public static Constraint MinMax(int min, int max) => new Constraint(Kind.MinMax, min, max);
    public static Constraint Proportion(int w) => new Constraint(Kind.Proportion, w, 0);

    /// <summary>
    /// Minimum size contributed by this constraint for a given total span.
    /// </summary>
    internal int Base(int total)
    {
        switch (Tag)
        {
            case Kind.Length: return A;
            case Kind.Min: return A;
            case Kind.Max: return Math.Min(A, total);
            case Kind.Percentage: return (int)long.Clamp((long)A * total / 100, 0, total);
            case Kind.Ratio: return B == 0 ? 0 : (int)long.Clamp((long)A * total / B, 0, total);
            case Kind.MinMax: return A;
            case Kind.Proportion: return 0;
            default: return 0;
        }
    }

    /// <summary>
    /// Lower bound that a segment must keep when the area is over-subscribed.
    /// </summary>
    internal int MinSize => Tag == Kind.MinMax ? A : Base(int.MaxValue);

    public override string ToString() => Tag switch
    {
        Kind.Length => $"Length({A})",
        Kind.Min => $"Min({A})",
        Kind.Max => $"Max({A})",
        Kind.Percentage => $"Percentage({A})",
        Kind.Ratio => $"Ratio({A},{B})",
        Kind.MinMax => $"MinMax({A},{B})",
        Kind.Proportion => $"Proportion({A})",
        _ => Tag.ToString(),
    };
}

/// <summary>
/// Margin applied around a layout area (TTY-free).
/// </summary>
public struct Margin
{
    public int Horizontal;
    public int Vertical;

    public static readonly Margin None = new Margin { Horizontal = 0, Vertical = 0 };

    public Margin(int horizontal, int vertical)
    {
        Horizontal = horizontal;
        Vertical = vertical;
    }

    public static Margin New(int horizontal, int vertical) => new Margin(horizontal, vertical);
}

/// <summary>
/// Minimal layout solver (clean-room port of Rust Layout). Subdivides a Rect
/// into one child Rect per constraint along the chosen direction, honoring
/// margins, spacing and the residual-space distribution policy.
/// </summary>
public class Layout
{
    public Direction Dir { get; set; } = Direction.Vertical;
    public Margin Margin { get; set; } = Margin.None;
    public List<Constraint> Constraints { get; set; } = [];
    public int Spacing { get; set; } = 0;
    public Flex FlexMode { get; set; } = Flex.Legacy;

    public static Layout New(Direction dir) => new Layout { Dir = dir };

    public Layout WithMargin(Margin m) { Margin = m; return this; }
    public Layout WithConstraints(List<Constraint> cs) { Constraints = cs; return this; }
    public Layout WithConstraint(Constraint c)
    {
        if (Constraints == null) Constraints = [];
        Constraints.Add(c);
        return this;
    }
    public Layout WithSpacing(int s) { Spacing = s; return this; }
    public Layout WithFlex(Flex f) { FlexMode = f; return this; }
    public Layout WithDirection(Direction d) { Dir = d; return this; }

    /// <summary>Split area into one Rect per constraint (varargs helper).</summary>
    public Rect[] SplitN(Rect area, params Constraint[] cs)
    {
        Constraints = new List<Constraint>(cs);
        return Split(area).ToArray();
    }

    /// <summary>Subdivide area according to the constraints, returning one Rect per constraint.</summary>
    public List<Rect> Split(Rect area)
    {
        var innerX = area.X + Margin.Horizontal;
        var innerY = area.Y + Margin.Vertical;
        var innerW = Math.Max(0, area.W - Margin.Horizontal * 2);
        var innerH = Math.Max(0, area.H - Margin.Vertical * 2);

        var inner = new Rect(innerX, innerY, innerW, innerH);
        var n = Constraints.Count;
        if (n == 0) return [];

        var totalSpan = Dir == Direction.Horizontal ? inner.W : inner.H;
        var spacingTotal = Spacing * Math.Max(0, n - 1);
        var available = Math.Max(0, totalSpan - spacingTotal);
        var baseStart = Dir == Direction.Horizontal ? inner.X : inner.Y;

        List<int> sizes;
        int lead;
        int gapExtra;

        if (FlexMode == Flex.Legacy)
        {
            sizes = Solve(available);
            lead = 0;
            gapExtra = 0;
        }
        else
        {
            sizes = Constraints.Select(c => c.Base(available)).ToList();
            var used = sizes.Sum();
            var leftover = Math.Max(0, available - used);
            switch (FlexMode)
            {
                case Flex.Start: lead = 0; gapExtra = 0; break;
                case Flex.End: lead = leftover; gapExtra = 0; break;
                case Flex.Center: lead = leftover / 2; gapExtra = 0; break;
                case Flex.SpaceBetween:
                    lead = 0;
                    gapExtra = n > 1 ? leftover / (n - 1) : leftover / 2;
                    break;
                case Flex.SpaceAround:
                    lead = leftover / (2 * n);
                    gapExtra = leftover / n;
                    break;
                default: lead = 0; gapExtra = 0; break;
            }
        }

        var rects = new List<Rect>(n);
        var offset = baseStart + lead;
        for (var i = 0; i < n; i++)
        {
            var size = sizes[i];
            var r = Dir == Direction.Horizontal
                ? new Rect(offset, inner.Y, size, inner.H)
                : new Rect(inner.X, offset, inner.W, size);
            rects.Add(r);
            offset += size;
            if (i + 1 < n) offset += Spacing + gapExtra;
        }
        return rects;
    }

    private List<int> Solve(int available)
    {
        var sizes = Constraints.Select(c => c.Base(available)).ToList();
        var used = sizes.Sum(s => (long)s);
        long avail = available;

        if (used < avail)
        {
            var remaining = (int)(avail - used);

            // 1) Distribute to Proportion constraints (weighted).
            var propWeight = Constraints
                .Where(c => c.Tag == Constraint.Kind.Proportion)
                .Sum(c => (long)c.A);
            if (propWeight > 0)
            {
                long totalExtra = remaining;
                long distributed = 0;
                var lastProp = -1;
                for (var i = 0; i < Constraints.Count; i++)
                {
                    if (Constraints[i].Tag == Constraint.Kind.Proportion)
                    {
                        var share = totalExtra * Constraints[i].A / propWeight;
                        sizes[i] += (int)share;
                        distributed += share;
                        lastProp = i;
                    }
                }
                if (lastProp >= 0)
                {
                    var leftover = (int)(totalExtra - distributed);
                    sizes[lastProp] += leftover;
                }
                remaining = 0;
            }

            // 2) Otherwise to Min / MinMax constraints (equal parts, remainder to first).
            if (remaining > 0)
            {
                var grow = Constraints
                    .Select((c, i) => (c, i))
                    .Where(t => t.c.Tag is Constraint.Kind.Min or Constraint.Kind.MinMax)
                    .ToList();
                if (grow.Count > 0)
                {
                    var per = remaining / grow.Count;
                    var extra = remaining % grow.Count;
                    foreach (var (c, i) in grow)
                    {
                        var add = per + (extra > 0 ? 1 : 0);
                        if (extra > 0) extra--;
                        var cap = c.Tag == Constraint.Kind.MinMax ? c.B : int.MaxValue;
                        sizes[i] = Math.Min(sizes[i] + add, cap);
                    }
                    remaining = 0;
                }
            }

            // 3) Finally to the last segment.
            if (remaining > 0)
            {
                var last = sizes.Count - 1;
                sizes[last] += remaining;
            }
        }
        else if (used > avail)
        {
            // Over-subscribed: shrink from the last segments, never below their minimum.
            var overflow = (int)(used - avail);
            for (var i = sizes.Count - 1; i >= 0 && overflow > 0; i--)
            {
                var min = Constraints[i].MinSize;
                var reduce = Math.Min(sizes[i] - min, overflow);
                if (reduce < 0) reduce = 0;
                sizes[i] -= reduce;
                overflow -= reduce;
            }
        }

        return sizes;
    }
}
