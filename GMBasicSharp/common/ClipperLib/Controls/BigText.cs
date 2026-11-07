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

public static class BigTextBuilder
{
    public static string ToBlockString(string text)
    {
        return ToString(ToBlock(text));
    }

    public static Block ToBlock(string text)
    {
        // Composizione del blocco NEL runtime C (gmb_tui_bigfont.c + gmb_bigtext_to_block):
        // il client non ha più né la tabella dei glifi né la logica di concatenazione.
        var raw = new byte[7 * 512];
        NativeControlDraw.gmb_bigtext_to_block(text ?? "", raw);

        var block = new Block(7);
        for (var r = 0; r < 7; r++)
        {
            var cells = new List<string>();
            var i = r * 512;
            var end = i;
            while (end < i + 512 && raw[end] != 0) end++;
            while (i < end)
            {
                var len = raw[i] < 0x80 ? 1
                        : (raw[i] & 0xE0) == 0xC0 ? 2
                        : (raw[i] & 0xF0) == 0xE0 ? 3 : 4;
                if (i + len > end) len = 1;
                cells.Add(Encoding.UTF8.GetString(raw, i, len));
                i += len;
            }
            block.Add(cells.ToArray());
        }
        return block;
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
        // Deprecato: il disegno del BigText vive nel runtime C (gmb_tui_draw_control,
        // kind GMB_DK_BIGTEXT). La metrica viene dalla stessa tabella C.
        var n = NativeControlDraw.gmb_bigtext_measure(text, out var w, out var h);
        if (n <= 0) return;
        NativeDraw.Show(buf, NativeDraw.BigText, x, y, w, h, text: text);
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
            // Metrica nel runtime C (stessa tabella del render): nessuna copia in C#.
            var n = NativeControlDraw.gmb_bigtext_measure(Text, out var w, out var h);
            return n > 0 ? (w, h) : (0, 0);
        }
    }

    public override void Draw(ScreenBuffer buffer)
    {
        NativeDraw.Show(buffer, NativeDraw.BigText, this, text: Text, h: Height, w: Width);
    }

    public void Render(ScreenBuffer buffer, int x, int y)
    {
        // Render nativo (runtime C): nessun disegno a celle nel client.
        NativeDraw.Show(buffer, NativeDraw.BigText, x, y, Width, Height, text: Text);
    }

    public override bool HandleKey(int key) => false;
}
