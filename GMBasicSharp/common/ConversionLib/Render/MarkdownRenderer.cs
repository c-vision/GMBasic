// ConversionLib/Render/MarkdownRenderer.cs

using System.Collections.Immutable;
using System.Text;
using Basic.ConversionLib.Model;

namespace Basic.ConversionLib.Render;

public static class MarkdownRenderer
{
    public static string Render(Document doc)
    {
        var sb = new StringBuilder();
        for (int i = 0; i < doc.Blocks.Length; i++)
        {
            if (i > 0) sb.Append("\n\n");
            RenderBlock(doc.Blocks[i], sb);
        }
        if (sb.Length > 0 && sb[^1] != '\n') sb.Append('\n');
        return sb.ToString();
    }

    private static void RenderBlock(Block block, StringBuilder sb)
    {
        switch (block)
        {
            case Heading h: RenderHeading(h, sb); break;
            case Paragraph p: RenderParagraph(p, sb); break;
            case CodeBlock c: RenderCodeBlock(c, sb); break;
            case BlockQuote q: RenderBlockQuote(q, sb); break;
            case Rule: sb.Append("---"); break;
            case ListBlock l: RenderList(l, sb); break;
            case TableBlock t: RenderTable(t, sb); break;
        }
    }

    private static void RenderHeading(Heading h, StringBuilder sb)
    {
        var level = Math.Clamp(h.Level, 1, 6);
        sb.Append(new string('#', level)).Append(' ');
        RenderInlines(h.Content, sb);
    }

    private static void RenderParagraph(Paragraph p, StringBuilder sb)
    {
        RenderInlines(p.Content, sb);
    }

    private static void RenderCodeBlock(CodeBlock c, StringBuilder sb)
    {
        sb.Append("```");
        if (c.Language != null) sb.Append(c.Language);
        sb.Append('\n').Append(c.Text).Append("\n```");
    }

    private static void RenderBlockQuote(BlockQuote q, StringBuilder sb)
    {
        var inner = new StringBuilder();
        for (int i = 0; i < q.Blocks.Length; i++)
        {
            if (i > 0) inner.Append("\n\n");
            RenderBlock(q.Blocks[i], inner);
        }
        foreach (var line in inner.ToString().Split('\n'))
            sb.Append("> ").AppendLine(line);
        sb.Length -= 1; // trim trailing newline
    }

    private static void RenderList(ListBlock list, StringBuilder sb)
    {
        for (int i = 0; i < list.Items.Length; i++)
        {
            if (i > 0) sb.Append('\n');
            RenderListItem(list, i, list.Items[i], sb);
        }
    }

    private static void RenderListItem(ListBlock list, int index, ListItem item, StringBuilder sb)
    {
        var marker = GetMarker(list, index);
        sb.Append(marker).Append(' ');

        if (item.Checked is { } chk)
            sb.Append(chk == "true" ? "[x] " : "[ ] ");

        for (int j = 0; j < item.Blocks.Length; j++)
        {
            if (j > 0) sb.Append('\n').Append(new string(' ', marker.Length + 1));
            RenderBlock(item.Blocks[j], sb);
        }
    }

    private static string GetMarker(ListBlock list, int index)
    {
        return list.Marker switch
        {
            MarkerKind.Decimal => $"{list.Start + index}.",
            MarkerKind.Bullet => "-",
            _ => $"{list.Start + index}."
        };
    }

    private static void RenderTable(TableBlock table, StringBuilder sb)
    {
        if (table.Rows.Length == 0) return;
        var colCount = table.Rows.Max(r => r.Length);
        var widths = new int[colCount];
        var cells = new string[table.Rows.Length][];

        for (int r = 0; r < table.Rows.Length; r++)
        {
            cells[r] = new string[colCount];
            for (int c = 0; c < table.Rows[r].Length; c++)
            {
                var cellSb = new StringBuilder();
                foreach (var b in table.Rows[r][c].Blocks)
                    RenderBlock(b, cellSb);
                cells[r][c] = cellSb.ToString().Replace("\n", "<br>");
                widths[c] = Math.Max(widths[c], cells[r][c].Length);
            }
        }

        for (int r = 0; r < table.Rows.Length; r++)
        {
            sb.Append('|');
            for (int c = 0; c < colCount; c++)
                sb.Append(' ').Append((cells[r][c] ?? "").PadRight(widths[c])).Append(" |");
            if (r == table.HeaderRows - 1)
                sb.Append('\n').Append(RenderHeaderSeparator(widths));
            if (r < table.Rows.Length - 1)
                sb.Append('\n');
        }
    }

    private static string RenderHeaderSeparator(int[] widths)
    {
        var sb = new StringBuilder();
        sb.Append('|');
        foreach (var w in widths)
            sb.Append(' ').Append(new string('-', w)).Append(" |");
        return sb.ToString();
    }

    internal static void RenderInlines(ImmutableArray<Inline> inlines, StringBuilder sb)
    {
        var normalized = NormalizeInlines(inlines);
        var prevEndsSpace = false;
        foreach (var inline in normalized)
        {
            switch (inline)
            {
                case TextInline txt:
                    sb.Append(RenderText(txt, prevEndsSpace));
                    var text = txt.Text;
                    prevEndsSpace = text.Length > 0 && char.IsWhiteSpace(text[^1]);
                    break;
                case Link link:
                    sb.Append('[');
                    RenderInlines(link.Content, sb);
                    sb.Append("](").Append(EscapeUrl(link.Url)).Append(')');
                    prevEndsSpace = false;
                    break;
                case Image img:
                    sb.Append("![");
                    sb.Append(EscapeText(img.Alt));
                    if (img.Source != null)
                        sb.Append("](").Append(EscapeUrl(img.Source)).Append(')');
                    else
                        sb.Append(']');
                    prevEndsSpace = false;
                    break;
                case NoteRef nr:
                    sb.Append("[^").Append(nr.Id).Append(']');
                    prevEndsSpace = false;
                    break;
                case LineBreak:
                    sb.Append("\\\n");
                    prevEndsSpace = false;
                    break;
            }
        }
    }

    // Normalize: merge adjacent same-style runs, drop empty runs, strip style from whitespace-only runs
    private static List<Inline> NormalizeInlines(ImmutableArray<Inline> inlines)
    {
        var result = new List<Inline>();
        TextInline? pending = null;

        void Flush()
        {
            if (pending != null && pending.Text.Length > 0)
            {
                result.Add(pending);
                pending = null;
            }
        }

        foreach (var inline in inlines)
        {
            if (inline is TextInline t)
            {
                if (t.Text.Length == 0) continue;
                var normalized = t.Text.Replace("\r\n", "\n").Replace('\r', '\n');
                // Strip style from whitespace-only runs
                var style = string.IsNullOrWhiteSpace(normalized) ? InlineStyle.None : t.Style;
                if (pending != null && pending.Style == style && !pending.Text.EndsWith('\n'))
                {
                    pending = new TextInline(pending.Text + normalized, style);
                }
                else
                {
                    Flush();
                    pending = new TextInline(normalized, style);
                }
            }
            else
            {
                Flush();
                result.Add(inline);
            }
        }
        Flush();
        return result;
    }

    private static string RenderText(TextInline t, bool prevEndsSpace)
    {
        var text = EscapeText(t.Text);

        if (!t.Style.Any) return text;

        // Leading/trailing whitespace goes outside delimiters
        var leading = text.Length - text.TrimStart().Length;
        var trailing = text.Length - text.TrimEnd().Length;
        var prefix = text[..leading];
        var suffix = trailing > 0 ? text[^trailing..] : "";
        text = leading < text.Length - trailing ? text[leading..(text.Length - trailing)] : "";

        if (t.Style.Code) return $"{prefix}`{text}`{suffix}";

        var styled = text;
        if (t.Style.Bold && t.Style.Italic) styled = $"***{styled}***";
        else if (t.Style.Bold) styled = $"**{styled}**";
        else if (t.Style.Italic) styled = $"*{styled}*";
        if (t.Style.Strike) styled = $"~~{styled}~~";

        return $"{prefix}{styled}{suffix}";
    }

    private static string EscapeText(string text)
    {
        var sb = new StringBuilder();
        foreach (var c in text)
        {
            switch (c)
            {
                case '\\': sb.Append("\\\\"); break;
                case '`': sb.Append("\\`"); break;
                case '*': sb.Append("\\*"); break;
                case '_': sb.Append("\\_"); break;
                case '[': sb.Append("\\["); break;
                case ']': sb.Append("\\]"); break;
                case '<': sb.Append("\\<"); break;
                case '>': sb.Append("\\>"); break;
                case '|': sb.Append("\\|"); break;
                default: sb.Append(c); break;
            }
        }
        return sb.ToString();
    }

    private static string EscapeUrl(string url)
    {
        var sb = new StringBuilder();
        foreach (var c in url)
        {
            if (c is ' ' or '(' or ')' or '<' or '>')
                sb.Append('%').Append(((int)c).ToString("X2"));
            else
                sb.Append(c);
        }
        return sb.ToString();
    }
}
