using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Controls;

/// <summary>Type of a line-breaking item (Box, Glue, Penalty).</summary>
public enum ItemType
{
    Box,
    Glue,
    Penalty
}

/// <summary>An element in a paragraph for Knuth-Plass line breaking.</summary>
public class Item
{
    public ItemType Type { get; }
    public float Width { get; }
    public float Stretch { get; }
    public float Shrink { get; }
    public float Cost { get; }
    public bool Flagged { get; }

    private Item(ItemType type, float width = 0, float stretch = 0, float shrink = 0, float cost = 0, bool flagged = false)
    {
        Type = type;
        Width = width;
        Stretch = stretch;
        Shrink = shrink;
        Cost = cost;
        Flagged = flagged;
    }

    public static Item Box(float width) => new(ItemType.Box, width: width);
    public static Item Glue(float width, float stretch, float shrink) => new(ItemType.Glue, width, stretch, shrink);
    public static Item Penalty(float width, float cost, bool flagged) => new(ItemType.Penalty, width, cost: cost, flagged: flagged);
    public static Item ForcedBreak() => new(ItemType.Penalty, cost: float.NegativeInfinity);

    public float PenaltyCost() => Type == ItemType.Penalty ? Cost : 0.0f;

    public float PenaltyFlag() => Type == ItemType.Penalty && Flagged ? 1.0f : 0.0f;

    public bool IsMandatoryBreak() => Type == ItemType.Penalty && float.IsNegativeInfinity(Cost);

    public (float width, float stretch, float shrink, bool isLegal) IsLegalBreakpoint(Item? pred)
    {
        return Type switch
        {
            ItemType.Box => (Width, 0, 0, false),
            ItemType.Glue => (Width, Stretch, Shrink, pred?.Type == ItemType.Box),
            ItemType.Penalty => (Width, 0, 0, !float.IsPositiveInfinity(Cost)),
            _ => (0, 0, 0, false)
        };
    }

    public float AdjustmentRatio(float width, float stretch, float shrink, float lineWidth)
    {
        var w = Type == ItemType.Penalty ? width + Width : width;
        if (w < lineWidth)
            return stretch > 0 ? (lineWidth - w) / stretch : float.PositiveInfinity;
        if (w > lineWidth)
            return shrink > 0 ? (lineWidth - w) / shrink : float.NegativeInfinity;
        return 0;
    }
}

/// <summary>Describes a line break: the item index where it breaks and the glue adjustment ratio.</summary>
public struct Line
{
    public int BreakAt { get; set; }
    public float AdjustmentRatio { get; set; }

    public float GlueWidth(float width, float stretch, float shrink)
    {
        if (AdjustmentRatio < 0)
            return width + shrink * AdjustmentRatio;
        if (AdjustmentRatio > 0)
            return width + stretch * AdjustmentRatio;
        return width;
    }
}

/// <summary>Algorithm for laying out a paragraph as a sequence of lines.</summary>
public interface IParagraphLayout
{
    List<Line> LayoutParagraph(List<Item> items, float lineWidth);
}

// ---------------------------------------------------------------------------
// First-Fit (greedy)
// ---------------------------------------------------------------------------

/// <summary>Greedy First-Fit line breaker: breaks as soon as the current line overflows.</summary>
public class FirstFit : IParagraphLayout
{
    private bool _allowOverflow;

    public FirstFit() { }

    public FirstFit AllowOverflow(bool allow)
    {
        _allowOverflow = allow;
        return this;
    }

    private struct FfBreak
    {
        public int At;
        public float LineWidth;
        public float LineStretch;
        public float LineShrink;
    }

    private static float FiniteRatio(float width, float stretch, float shrink, float lineWidth)
    {
        float r;
        if (width < lineWidth)
            r = stretch > 0 ? (lineWidth - width) / stretch : 0;
        else if (width > lineWidth)
            r = shrink > 0 ? (lineWidth - width) / shrink : 0;
        else
            r = 0;
        return float.IsFinite(r) ? r : 0;
    }

    public List<Line> LayoutParagraph(List<Item> items, float lineWidth)
    {
        var lines = new List<Line>();
        float width = 0, stretch = 0, shrink = 0;
        FfBreak? lastBreak = null;

        for (var b = 0; b < items.Count; b++)
        {
            var pred = b > 0 ? items[b - 1] : null;
            var (w, st, sh, isLegal) = items[b].IsLegalBreakpoint(pred);

            if (isLegal)
            {
                var ratio = items[b].AdjustmentRatio(width, stretch, shrink, lineWidth);
                var isMandatory = items[b].IsMandatoryBreak();

                if (ratio < -1.0f)
                {
                    if (lastBreak.HasValue)
                    {
                        var prev = lastBreak.Value;
                        lines.Add(new Line
                        {
                            BreakAt = prev.At,
                            AdjustmentRatio = FiniteRatio(prev.LineWidth, prev.LineStretch, prev.LineShrink, lineWidth)
                        });
                        width -= prev.LineWidth;
                        stretch -= prev.LineStretch;
                        shrink -= prev.LineShrink;
                        lastBreak = null;
                    }
                    else if (!_allowOverflow)
                    {
                        return [];
                    }
                }

                if (isMandatory)
                {
                    lines.Add(new Line
                    {
                        BreakAt = b,
                        AdjustmentRatio = FiniteRatio(width, stretch, shrink, lineWidth)
                    });
                    width = 0;
                    stretch = 0;
                    shrink = 0;
                    lastBreak = null;
                    continue;
                }

                lastBreak = new FfBreak
                {
                    At = b,
                    LineWidth = width,
                    LineStretch = stretch,
                    LineShrink = shrink
                };
            }

            width += w;
            stretch += st;
            shrink += sh;
        }

        if (lastBreak.HasValue)
        {
            var prev = lastBreak.Value;
            lines.Add(new Line
            {
                BreakAt = prev.At,
                AdjustmentRatio = FiniteRatio(prev.LineWidth, prev.LineStretch, prev.LineShrink, lineWidth)
            });
        }

        return lines;
    }
}

// ---------------------------------------------------------------------------
// Knuth-Plass (optimal line breaking)
// ---------------------------------------------------------------------------

internal enum FitnessClass
{
    Zero = 0,
    One = 1,
    Two = 2,
    Three = 3
}

internal static class FitnessClassExtensions
{
    public static int Distance(this FitnessClass self, FitnessClass other) =>
        Math.Abs((int)self - (int)other);
}

internal class KpNode
{
    public int Position { get; set; }
    public int Line { get; set; }
    public FitnessClass Fitness { get; set; }
    public float TotalWidth { get; set; }
    public float TotalStretch { get; set; }
    public float TotalShrink { get; set; }
    public float TotalDemerits { get; set; }
    public int? Previous { get; set; }
    public int? Link { get; set; }
}

/// <summary>Optimal Knuth-Plass line breaker (Knuth &amp; Plass '81).</summary>
public class KnuthPlass : IParagraphLayout
{
    private float _flaggedDemerit = 100f;
    private float _fitnessDemerit = 100f;
    private float _threshold = 1f;
    private int _looseness;

    public KnuthPlass() { }

    public KnuthPlass WithFlaggedDemerit(float v) { _flaggedDemerit = v; return this; }
    public KnuthPlass WithFitnessDemerit(float v) { _fitnessDemerit = v; return this; }
    public KnuthPlass WithThreshold(float v) { _threshold = v; return this; }
    public KnuthPlass WithLooseness(int v) { _looseness = v; return this; }

    public List<Line> LayoutParagraph(List<Item> items, float lineWidth)
    {
        var ctx = new KpContext
        {
            Items = items,
            LineWidth = lineWidth,
            FlaggedDemerit = _flaggedDemerit,
            FitnessDemerit = _fitnessDemerit,
            Threshold = _threshold,
            Looseness = _looseness
        };
        return ctx.Run();
    }

    private class KpContext
    {
        public List<KpNode> Arena { get; } = [];
        public List<Item> Items { get; init; } = null!;
        public float LineWidth { get; init; }
        public float FlaggedDemerit { get; init; }
        public float FitnessDemerit { get; init; }
        public float Threshold { get; init; }
        public int Looseness { get; init; }
        public float TotalWidth { get; set; }
        public float TotalStretch { get; set; }
        public float TotalShrink { get; set; }
        public int? Active { get; set; }

        private int NewNode(KpNode node)
        {
            Arena.Add(node);
            return Arena.Count - 1;
        }

        private (float width, float stretch, float shrink, bool isLegal) IsLegalBreakpoint(int b)
        {
            var pred = b > 0 ? Items[b - 1] : null;
            return Items[b].IsLegalBreakpoint(pred);
        }

        private (int line, float ratio) AdjustmentRatio(int aNode, int b)
        {
            var node = Arena[aNode];
            var j = node.Line + 1;
            var r = Items[b].AdjustmentRatio(
                TotalWidth - node.TotalWidth,
                TotalStretch - node.TotalStretch,
                TotalShrink - node.TotalShrink,
                LineWidth);
            return (j, r);
        }

        private (float demerits, FitnessClass fitness) DemeritsAndFitness(float r, int aNode, int b)
        {
            var cost = Items[b].PenaltyCost();
            var ra3 = Math.Abs(r) * Math.Abs(r) * Math.Abs(r);
            var baseVal = 1.0f + 100.0f * ra3;
            float d;
            if (cost >= 0)
                d = (baseVal + cost) * (baseVal + cost);
            else if (!float.IsNegativeInfinity(cost))
                d = baseVal * baseVal - cost * cost;
            else
                d = baseVal * baseVal;

            var nodeA = Arena[aNode];
            d += FlaggedDemerit * Items[b].PenaltyFlag() * Items[nodeA.Position].PenaltyFlag();

            var c = r < -0.5f ? FitnessClass.Zero
                : r <= 0.5f ? FitnessClass.One
                : r <= 1.0f ? FitnessClass.Two
                : FitnessClass.Three;

            if (c.Distance(nodeA.Fitness) > 1)
                d += FitnessDemerit;

            return (d + nodeA.TotalDemerits, c);
        }

        private (float w, float st, float sh) TotalAfter(int b)
        {
            float w = TotalWidth, st = TotalStretch, sh = TotalShrink;
            for (var i = b; i < Items.Count; i++)
            {
                switch (Items[i].Type)
                {
                    case ItemType.Box:
                        return (w, st, sh);
                    case ItemType.Glue:
                        w += Items[i].Width;
                        st += Items[i].Stretch;
                        sh += Items[i].Shrink;
                        break;
                    case ItemType.Penalty:
                        if (float.IsNegativeInfinity(Items[i].Cost) && i > b)
                            return (w, st, sh);
                        break;
                }
            }
            return (w, st, sh);
        }

        private bool LayoutBreakpoint(int b)
        {
            var a = Active;
            int? prevA = null;

            while (a.HasValue)
            {
                var classA = new int?[4];
                var classDemerits = new[] { float.PositiveInfinity, float.PositiveInfinity, float.PositiveInfinity, float.PositiveInfinity };
                var minDemerits = float.PositiveInfinity;

                while (true)
                {
                    var cur = a!.Value;
                    var nextA = Arena[cur].Link;

                    var (j, r) = AdjustmentRatio(cur, b);
                    if (r < -1.0f || Items[b].IsMandatoryBreak())
                    {
                        if (prevA.HasValue)
                            Arena[prevA.Value].Link = nextA;
                        else
                            Active = nextA;
                    }
                    else
                    {
                        prevA = cur;
                    }

                    if (r >= -1.0f && r <= Threshold)
                    {
                        var (demerits, fitness) = DemeritsAndFitness(r, cur, b);
                        var fi = (int)fitness;
                        if (demerits < classDemerits[fi])
                        {
                            classDemerits[fi] = demerits;
                            classA[fi] = cur;
                            if (demerits < minDemerits)
                                minDemerits = demerits;
                        }
                    }

                    a = nextA;
                    if (!a.HasValue)
                        break;
                    if (Arena[a.Value].Line >= j)
                        break;
                }

                if (float.IsFinite(minDemerits))
                {
                    var (totalWidth, totalStretch, totalShrink) = TotalAfter(b);
                    var minD = minDemerits + FitnessDemerit;
                    var classes = new[] { FitnessClass.Zero, FitnessClass.One, FitnessClass.Two, FitnessClass.Three };
                    foreach (var fc in classes)
                    {
                        var ci = (int)fc;
                        var demerits = classDemerits[ci];
                        if (demerits <= minD && classA[ci].HasValue)
                        {
                            var ca = classA[ci]!.Value;
                            var line = Arena[ca].Line + 1;
                            var s = NewNode(new KpNode
                            {
                                Position = b,
                                Line = line,
                                Fitness = fc,
                                TotalWidth = totalWidth,
                                TotalStretch = totalStretch,
                                TotalShrink = totalShrink,
                                TotalDemerits = demerits,
                                Previous = ca,
                                Link = a
                            });
                            if (prevA.HasValue)
                                Arena[prevA.Value].Link = s;
                            else
                                Active = s;
                            prevA = s;
                        }
                    }
                }
            }

            return Active.HasValue;
        }

        public List<Line> Run()
        {
            Active = NewNode(new KpNode());

            for (var b = 0; b < Items.Count; b++)
            {
                var (width, stretch, shrink, isLegal) = IsLegalBreakpoint(b);
                if (isLegal && !LayoutBreakpoint(b))
                    return [];
                TotalWidth += width;
                TotalStretch += stretch;
                TotalShrink += shrink;
            }

            if (!Active.HasValue)
                return [];

            var best = Active!.Value;
            {
                var a = Active;
                while (a.HasValue)
                {
                    if (Arena[a.Value].TotalDemerits < Arena[best].TotalDemerits)
                        best = a.Value;
                    a = Arena[a.Value].Link;
                }
            }

            if (Looseness != 0)
            {
                var k = Arena[best].Line;
                var chosen = Active!.Value;
                var s = 0;
                var a = Active;
                while (a.HasValue)
                {
                    var n = a.Value;
                    var delta = Arena[n].Line - k;
                    if ((Looseness <= delta && delta < s) || (s < delta && delta <= Looseness))
                    {
                        s = delta;
                        chosen = n;
                    }
                    else if (delta == s && Arena[n].TotalDemerits < Arena[chosen].TotalDemerits)
                    {
                        chosen = n;
                    }
                    a = Arena[n].Link;
                }
                best = chosen;
            }

            var totalLines = Arena[best].Line;
            var lines = new List<Line>(totalLines);
            for (var i = 0; i < totalLines; i++)
                lines.Add(new Line());

            var cur = best;
            var j = totalLines;
            while (j > 0)
            {
                var prev = Arena[cur].Previous!.Value;
                var prevPos = j == 1 ? 0 : Arena[prev].Position + 1;
                var curPos = Arena[cur].Position;

                float width = 0, stretch = 0, shrink = 0;
                for (var i = prevPos; i < curPos; i++)
                {
                    var item = Items[i];
                    switch (item.Type)
                    {
                        case ItemType.Box:
                            width += item.Width;
                            break;
                        case ItemType.Glue:
                            width += item.Width;
                            stretch += item.Stretch;
                            shrink += item.Shrink;
                            break;
                        case ItemType.Penalty:
                            width += item.Width;
                            break;
                    }
                }

                var adjustmentRatio = Items[curPos].AdjustmentRatio(width, stretch, shrink, LineWidth);
                lines[j - 1] = new Line
                {
                    BreakAt = curPos,
                    AdjustmentRatio = adjustmentRatio
                };

                cur = prev;
                j--;
            }

            return lines;
        }
    }
}

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

public static class TextLayout
{
    /// <summary>
    /// Simple greedy word wrap. Splits by spaces, no hyphenation.
    /// This is the primary public API for most use cases.
    /// </summary>
    public static string[] WordWrap(string text, int maxWidth)
    {
        if (string.IsNullOrEmpty(text))
            return [];

        var words = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (words.Length == 0)
            return [];

        var result = new List<string>();
        var currentLine = new System.Text.StringBuilder();

        foreach (var word in words)
        {
            if (currentLine.Length == 0)
            {
                currentLine.Append(word);
            }
            else if (currentLine.Length + 1 + word.Length <= maxWidth)
            {
                currentLine.Append(' ');
                currentLine.Append(word);
            }
            else
            {
                result.Add(currentLine.ToString());
                currentLine.Clear();
                currentLine.Append(word);
            }
        }

        if (currentLine.Length > 0)
            result.Add(currentLine.ToString());

        return result.ToArray();
    }

    /// <summary>Builds a list of Items from text, measuring words in character count.</summary>
    public static List<Item> ItemsFromWords(string text, float spaceStretch, float spaceShrink)
    {
        var items = new List<Item>();
        var words = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        for (var i = 0; i < words.Length; i++)
        {
            if (i > 0)
                items.Add(Item.Glue(1.0f, spaceStretch, spaceShrink));
            items.Add(Item.Box(words[i].Length));
        }
        items.Add(Item.Glue(0.0f, float.PositiveInfinity, 0.0f));
        items.Add(Item.ForcedBreak());
        return items;
    }

    /// <summary>
    /// Wraps words into lines using the given layout algorithm (e.g. KnuthPlass, FirstFit).
    /// Returns the reconstructed text lines.
    /// </summary>
    public static string[] WrapWords(IParagraphLayout algorithm, string text, float lineWidth)
    {
        var words = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (words.Length == 0)
            return [];

        var items = ItemsFromWords(text, 1.0f, 0.0f);
        var lines = algorithm.LayoutParagraph(items, lineWidth);

        var wordAtItem = new int?[items.Count];
        var wordIdx = 0;
        for (var i = 0; i < items.Count; i++)
        {
            if (items[i].Type == ItemType.Box)
            {
                wordAtItem[i] = wordIdx;
                wordIdx++;
            }
        }

        var result = new List<string>();
        var startWord = 0;

        foreach (var line in lines)
        {
            var endWord = startWord;
            for (var idx = 0; idx < line.BreakAt; idx++)
            {
                if (wordAtItem[idx]!.HasValue && wordAtItem[idx]!.Value >= startWord)
                    endWord = wordAtItem[idx]!.Value;
            }

            if (endWord >= startWord && startWord < words.Length)
            {
                var count = Math.Min(endWord - startWord + 1, words.Length - startWord);
                if (count > 0)
                {
                    result.Add(string.Join(" ", words, startWord, count));
                    startWord = endWord + 1;
                }
            }
        }

        if (startWord < words.Length)
            result.Add(string.Join(" ", words, startWord, words.Length - startWord));

        return result.ToArray();
    }
}
