using CommonLib.Consoles.Tui;
using CommonLib.Screens;
using System.Text;
using ClipperLib.Clipper;

namespace ClipperLib.Controls;

public class Block : List<string[]>
{
    public Block() : base(7) { }
    public Block(int capacity) : base(capacity) { }
    public Block(IEnumerable<string[]> collection) : base(collection) { }
}

internal static class BigTextFont
{
    private static readonly Dictionary<char, string[]> Font = BuildFont();

    private static Dictionary<char, string[]> BuildFont()
    {
        var f = new Dictionary<char, string[]>();

        void Add(char ch, string[] rows)
        {
            var w = rows.Max(r => r.Length);
            var norm = new string[7];
            for (var i = 0; i < 7; i++)
                norm[i] = rows[i].PadRight(w);
            f[ch] = norm;
        }

        Add(' ', ["     ", "     ", "     ", "     ", "     ", "     ", "     "]);
        Add('A', ["  █  ", " █ █ ", "█   █", "█████", "█   █", "█   █", "█   █"]);
        Add('B', ["████ ", "█   █", "█   █", "████ ", "█   █", "█   █", "████ "]);
        Add('C', [" ███ ", "█   █", "█    ", "█    ", "█    ", "█   █", " ███ "]);
        Add('D', ["████ ", "█   █", "█   █", "█   █", "█   █", "█   █", "████ "]);
        Add('E', ["█████", "█    ", "█    ", "████ ", "█    ", "█    ", "█████"]);
        Add('F', ["█████", "█    ", "█    ", "████ ", "█    ", "█    ", "█    "]);
        Add('G', [" ███ ", "█   █", "█    ", "█ ███", "█   █", "█   █", " ███ "]);
        Add('H', ["█   █", "█   █", "█   █", "█████", "█   █", "█   █", "█   █"]);
        Add('I', ["█████", "  █  ", "  █  ", "  █  ", "  █  ", "  █  ", "█████"]);
        Add('J', [" ███ ", "   █ ", "   █ ", "   █ ", "   █ ", "█  █ ", " ██  "]);
        Add('K', ["█   █", "█  █ ", "█ █  ", "██   ", "█ █  ", "█  █ ", "█   █"]);
        Add('L', ["█    ", "█    ", "█    ", "█    ", "█    ", "█    ", "█████"]);
        Add('M', ["█   █", "██ ██", "█ █ █", "█   █", "█   █", "█   █", "█   █"]);
        Add('N', ["█   █", "██  █", "█ █ █", "█  ██", "█   █", "█   █", "█   █"]);
        Add('O', [" ███ ", "█   █", "█   █", "█   █", "█   █", "█   █", " ███ "]);
        Add('P', ["████ ", "█   █", "█   █", "████ ", "█    ", "█    ", "█    "]);
        Add('Q', [" ███ ", "█   █", "█   █", "█   █", "█   █", "█  █ ", " ██ █"]);
        Add('R', ["████ ", "█   █", "█   █", "████ ", "█ █  ", "█  █ ", "█   █"]);
        Add('S', [" ███ ", "█   █", "█    ", " ███ ", "    █", "█   █", " ███ "]);
        Add('T', ["█████", "  █  ", "  █  ", "  █  ", "  █  ", "  █  ", "  █  "]);
        Add('U', ["█   █", "█   █", "█   █", "█   █", "█   █", "█   █", " ███ "]);
        Add('V', ["█   █", "█   █", "█   █", "█   █", "█   █", " █ █ ", "  █  "]);
        Add('W', ["█   █", "█   █", "█   █", "█   █", "█ █ █", "██ ██", "█   █"]);
        Add('X', ["█   █", "█   █", " █ █ ", "  █  ", " █ █ ", "█   █", "█   █"]);
        Add('Y', ["█   █", "█   █", " █ █ ", "  █  ", "  █  ", "  █  ", "  █  "]);
        Add('Z', ["█████", "    █", "   █ ", "  █  ", " █   ", "█    ", "█████"]);

        Add('0', [" ███ ", "█   █", "█  ██", "█ █ █", "██  █", "█   █", " ███ "]);
        Add('1', ["  █  ", " ██  ", "  █  ", "  █  ", "  █  ", "  █  ", " ███ "]);
        Add('2', [" ███ ", "█   █", "    █", "   █ ", "  █  ", " █   ", "█████"]);
        Add('3', [" ███ ", "█   █", "    █", "  ██ ", "    █", "█   █", " ███ "]);
        Add('4', ["    █", "   ██", "  █ █", " █  █", "█████", "    █", "    █"]);
        Add('5', ["█████", "█    ", "████ ", "    █", "    █", "█   █", " ███ "]);
        Add('6', [" ███ ", "█   █", "█    ", "████ ", "█   █", "█   █", " ███ "]);
        Add('7', ["█████", "    █", "   █ ", "  █  ", "  █  ", "  █  ", "  █  "]);
        Add('8', [" ███ ", "█   █", "█   █", " ███ ", "█   █", "█   █", " ███ "]);
        Add('9', [" ███ ", "█   █", "█   █", " ████", "    █", "█   █", " ███ "]);

        Add('.', ["     ", "     ", "     ", "     ", "     ", "     ", "  █  "]);
        Add(',', ["     ", "     ", "     ", "     ", "     ", "  █  ", " █   "]);
        Add('!', ["  █  ", "  █  ", "  █  ", "  █  ", "  █  ", "     ", "  █  "]);
        Add('?', [" ███ ", "█   █", "    █", "   █ ", "   █ ", "     ", "   █ "]);
        Add('-', ["     ", "     ", "     ", "█████", "     ", "     ", "     "]);
        Add('_', ["     ", "     ", "     ", "     ", "     ", "     ", "█████"]);
        Add('+', ["     ", "     ", "  █  ", "█████", "  █  ", "     ", "     "]);
        Add('=', ["     ", "     ", "█████", "     ", "█████", "     ", "     "]);
        Add('/', ["    █", "   █ ", "  █  ", " █   ", "█    ", "     ", "     "]);
        Add('\\', ["█    ", " █   ", "  █  ", "   █ ", "    █", "     ", "     "]);
        Add(':', ["     ", "     ", "  █  ", "     ", "     ", "  █  ", "     "]);
        Add(';', ["     ", "     ", "  █  ", "     ", "     ", "  █  ", " █   "]);
        Add('(', ["   █ ", "  █  ", " █   ", " █   ", " █   ", "  █  ", "   █ "]);
        Add(')', [" █   ", "  █  ", "   █ ", "   █ ", "   █ ", "  █  ", " █   "]);
        Add('[', [" ███ ", " █   ", " █   ", " █   ", " █   ", " █   ", " ███ "]);
        Add(']', [" ███ ", "   █ ", "   █ ", "   █ ", "   █ ", "   █ ", " ███ "]);
        Add('{', ["  ██ ", " █   ", " █   ", "█    ", " █   ", " █   ", "  ██ "]);
        Add('}', [" ██  ", "   █ ", "   █ ", "    █", "   █ ", "   █ ", " ██  "]);
        Add('<', ["    █", "   █ ", "  █  ", " █   ", "  █  ", "   █ ", "    █"]);
        Add('>', ["█    ", " █   ", "  █  ", "   █ ", "  █  ", " █   ", "█    "]);
        Add('*', ["     ", "  █  ", "█ █ █", " ███ ", "█ █ █", "  █  ", "     "]);
        Add('&', [" █   ", "█ █  ", " █   ", "█ █  ", "█  █ ", "█  █ ", " ██  "]);
        Add('%', ["█  █ ", " █ █ ", "   █ ", "  █  ", " █   ", "█ █  ", "█    "]);
        Add('#', [" █ █ ", "█████", " █ █ ", "█████", " █ █ ", "█████", " █ █ "]);
        Add('@', [" ███ ", "█   █", "█ █ █", "█ █ █", "█ ███", "█    ", " ███ "]);
        Add('$', ["  █  ", " ███ ", "█ █  ", " ███ ", "  ██ ", "████ ", "  █  "]);
        Add('\u00a3', [" ███ ", "█    ", "███  ", "█    ", "█    ", "█    ", " ███ "]); // £
        Add('\u20ac', ["  ███", " █   ", "████ ", " █   ", "████ ", " █   ", "  ███"]); // €
        Add('~', ["     ", "     ", "     ", " █ █ ", "█   █", "     ", "     "]);
        Add('`', ["█    ", " █   ", "  █  ", "     ", "     ", "     ", "     "]);
        Add('\u00b4', ["  █  ", " █   ", "█    ", "     ", "     ", "     ", "     "]); // ´
        Add('^', ["  █  ", " █ █ ", "█   █", "     ", "     ", "     ", "     "]);
        Add('|', ["  █  ", "  █  ", "  █  ", "  █  ", "  █  ", "  █  ", "  █  "]);
        Add('\u2080', ["     ", "     ", "     ", "     ", " ███ ", "█   █", " ███ "]); // ₀
        Add('\u00b9', ["     ", "     ", "     ", "  █  ", " ██  ", "  █  ", " ███ "]); // ¹
        Add('\u00b2', ["     ", "     ", "     ", " ███ ", "    █", "  █  ", "████ "]); // ²
        Add('\u00b3', ["     ", "     ", "     ", " ███ ", "    █", "  ██ ", " ███ "]); // ³
        Add('\u00a7', [" ██  ", "█    ", " ██  ", "█  █ ", " ██  ", "   █ ", " ██  "]); // §
        Add('\u00b5', ["     ", "     ", "█   █", "█   █", "█   █", "█   █", " ███ "]); // µ
        Add('\u00df', ["██   ", "█ █  ", "█ █  ", "██   ", "█ █  ", "█ █  ", "██   "]); // ß
        Add('\u00c4', [" █ █ ", "     ", " ███ ", "█   █", "█████", "█   █", "█   █"]); // Ä
        Add('\u00d6', [" █ █ ", "     ", " ███ ", "█   █", "█   █", "█   █", " ███ "]); // Ö
        Add('\u00dc', [" █ █ ", "     ", "█   █", "█   █", "█   █", "█   █", " ███ "]); // Ü
        Add('\u00e1', ["   █ ", "  █  ", " ██  ", "   █ ", " ███ ", "█   █", " ███ "]); // á
        Add('\u00e0', ["█    ", " █   ", " ██  ", "   █ ", " ███ ", "█   █", " ███ "]); // à
        Add('\u00e9', ["   █ ", "  █  ", " ██  ", "█    ", "████ ", "█    ", " ██  "]); // é
        Add('\u00e8', ["█    ", " █   ", " ██  ", "█    ", "████ ", "█    ", " ██  "]); // è
        Add('\u00ed', ["   █ ", "  █  ", "  █  ", "  █  ", "  █  ", "  █  ", " ███ "]); // í
        Add('\u00ec', ["█    ", " █   ", "  █  ", "  █  ", "  █  ", "  █  ", " ███ "]); // ì
        Add('\u00f3', ["   █ ", "  █  ", " ██  ", "█   █", "█   █", "█   █", " ██  "]); // ó
        Add('\u00f2', ["█    ", " █   ", " ██  ", "█   █", "█   █", "█   █", " ██  "]); // ò
        Add('\u00f1', ["     ", "█  █ ", " ██  ", "█    ", "██   ", "█ █  ", "██   "]); // ñ

        return f;
    }

    public static string[] Get(char c)
    {
        var key = char.IsLower(c) ? char.ToUpperInvariant(c) : c;
        if (Font.TryGetValue(key, out var rows))
            return rows;
        return Font['?'];
    }

    public static int GetWidth(char c)
    {
        var rows = Get(c);
        return rows.Length > 0 ? rows[0].Length : 0;
    }
}

public static class BigTextBuilder
{
    private static readonly Block EmptyBlock =
        [[], [], [], [], [], [], []];

    public static string ToBlockString(string text)
    {
        return ToString(ToBlock(text));
    }

    public static Block ToBlock(string text)
    {
        var result = new Block(7);
        for (var i = 0; i < 7; i++)
            result.Add([]);

        foreach (var ch in text)
        {
            var charRows = BigTextFont.Get(ch);
            var charBlock = new Block(7);
            for (var i = 0; i < 7; i++)
                charBlock.Add([charRows[i]]);

            result = ConcatBlock(result, charBlock);
        }

        return result;
    }

    public static Block ConcatBlock(Block a, Block b)
    {
        if (a.Count == 0 || a.All(row => row.Length == 0))
            return new Block(b.Select(row => row.ToArray()));
        if (b.Count == 0 || b.All(row => row.Length == 0))
            return new Block(a.Select(row => row.ToArray()));

        var rows = Math.Max(a.Count, b.Count);
        var result = new Block(rows);

        for (var i = 0; i < rows; i++)
        {
            var rowA = i < a.Count ? a[i] : [];
            var rowB = i < b.Count ? b[i] : [];
            var combined = new string[rowA.Length + 1 + rowB.Length];
            var idx = 0;
            foreach (var entry in rowA)
                combined[idx++] = entry;
            combined[idx++] = " ";
            foreach (var entry in rowB)
                combined[idx++] = entry;
            result.Add(combined);
        }

        return result;
    }

    public static Block ConcatBlocks(Block[] blocks)
    {
        if (blocks == null || blocks.Length == 0)
            return new Block(7) { new string[0], new string[0], new string[0], new string[0], new string[0], new string[0], new string[0] };

        var result = blocks[0];
        for (var i = 1; i < blocks.Length; i++)
            result = ConcatBlock(result, blocks[i]);

        return result;
    }

    public static Block MapBlock(Block block, Func<string, int, int, string> mapper)
    {
        var result = new Block(block.Count);
        for (var row = 0; row < block.Count; row++)
        {
            var src = block[row];
            var dst = new string[src.Length];
            for (var col = 0; col < src.Length; col++)
                dst[col] = mapper(src[col], row, col);
            result.Add(dst);
        }
        return result;
    }

    public static string ToString(Block block)
    {
        var sb = new StringBuilder();
        for (var i = 0; i < block.Count; i++)
        {
            if (i > 0)
                sb.AppendLine();
            foreach (var entry in block[i])
                sb.Append(entry);
        }
        return sb.ToString();
    }

    public static void RenderToBuffer(ScreenBuffer buf, int x, int y, string text, CellColor? fg, CellColor? bg)
    {
        var block = ToBlock(text);
        for (var i = 0; i < block.Count; i++)
        {
            var row = string.Concat(block[i]);
            if (string.IsNullOrEmpty(row))
                continue;
            for (var j = 0; j < row.Length; j++)
            {
                if (row[j] == '█')
                    buf.SetChar(x + j, y + i, '█', fg, bg);
                else
                    buf.SetChar(x + j, y + i, ' ', null, bg);
            }
        }
    }
}

public class BigText : Control
{
    public string Text { get; set; } = "";
    public CellColor? Color { get; set; }
    public CellColor? Background { get; set; }

    public BigText()
    {
        Width = 1;
        Height = 7;
    }

    public BigText WithText(string text)
    {
        Text = text;
        return this;
    }

    public BigText WithColor(CellColor color)
    {
        Color = color;
        return this;
    }

    public BigText WithBackground(CellColor bg)
    {
        Background = bg;
        return this;
    }

    public (int w, int h) Dimensions
    {
        get
        {
            var block = BigTextBuilder.ToBlock(Text);
            if (block.Count == 0 || block.All(r => r.Length == 0))
                return (0, 0);
            var w = block.Max(row => string.Concat(row).Length);
            return (w, block.Count);
        }
    }

    public override void Draw(ScreenBuffer buffer)
    {
        Render(buffer, X, Y);
    }

    public void Render(ScreenBuffer buffer, int x, int y)
    {
        BigTextBuilder.RenderToBuffer(buffer, x, y, Text, Color, Background);
    }

    public override bool HandleKey(int key) => false;
}
