using System.Collections.Immutable;
// ConversionLib/Render/TextRenderer.cs
using System.Text;
using Basic.ConversionLib.Model;

namespace Basic.ConversionLib.Render;

public static class TextRenderer
{
    public static string Render(Document doc)
    {
        var sb = new StringBuilder();
        foreach (var block in doc.Blocks)
            RenderBlock(block, sb);
        return sb.ToString().Trim();
    }

    private static void RenderBlock(Block block, StringBuilder sb)
    {
        switch (block)
        {
            case Heading h: RenderInlines(h.Content, sb); sb.AppendLine(); break;
            case Paragraph p: RenderInlines(p.Content, sb); sb.AppendLine(); break;
            case CodeBlock c: sb.AppendLine(c.Text); break;
            case BlockQuote q: foreach (var b in q.Blocks) { sb.Append("> "); RenderBlock(b, sb); } break;
            case Rule: sb.AppendLine("---"); break;
            case ListBlock l: foreach (var item in l.Items) { sb.Append("- "); foreach (var b in item.Blocks) RenderBlock(b, sb); } break;
            case TableBlock t: foreach (var row in t.Rows) { foreach (var cell in row) { foreach (var b in cell.Blocks) RenderBlock(b, sb); sb.Append('\t'); } sb.AppendLine(); } break;
        }
    }

    private static void RenderInlines(ImmutableArray<Inline> inlines, StringBuilder sb)
    {
        foreach (var i in inlines)
        {
            if (i is TextInline t) sb.Append(t.Text);
            else if (i is Link l) { RenderInlines(l.Content, sb); sb.Append($" <{l.Url}>"); }
            else if (i is Image img) sb.Append($"[{img.Alt}]");
            else if (i is LineBreak) sb.AppendLine();
        }
    }
}
