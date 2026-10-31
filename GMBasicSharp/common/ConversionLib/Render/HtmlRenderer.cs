using System.Collections.Immutable;
// ConversionLib/Render/HtmlRenderer.cs
using System.Text;
using Basic.ConversionLib.Model;

namespace Basic.ConversionLib.Render;

public static class HtmlRenderer
{
    public static string Render(Document doc)
    {
        var sb = new StringBuilder();
        sb.AppendLine("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>");
        sb.AppendLine("body{font-family:system-ui,sans-serif;max-width:900px;margin:2em auto;line-height:1.6;color:#1a1a1a;}");
        sb.AppendLine("table{border-collapse:collapse;width:100%;} td,th{border:1px solid #ddd;padding:8px;text-align:left;}");
        sb.AppendLine("th{background:#f5f5f5;} blockquote{border-left:3px solid #ddd;margin:0;padding:0 1em;color:#666;}");
        sb.AppendLine("code{background:#f0f0f0;padding:2px 6px;border-radius:3px;font-size:0.9em;} pre{background:#f0f0f0;padding:1em;overflow-x:auto;}");
        sb.AppendLine("img{max-width:100%;} hr{border:none;border-top:1px solid #ddd;}</style></head><body>");

        foreach (var block in doc.Blocks)
            RenderBlock(block, sb);

        sb.AppendLine("</body></html>");
        return sb.ToString();
    }

    private static void RenderBlock(Block block, StringBuilder sb)
    {
        switch (block)
        {
            case Heading h: sb.Append($"<h{Math.Clamp(h.Level,1,6)}>"); RenderInlines(h.Content, sb); sb.AppendLine($"</h{Math.Clamp(h.Level,1,6)}>"); break;
            case Paragraph p: sb.Append("<p>"); RenderInlines(p.Content, sb); sb.AppendLine("</p>"); break;
            case CodeBlock c: sb.Append($"<pre><code>").Append(System.Net.WebUtility.HtmlEncode(c.Text)).AppendLine("</code></pre>"); break;
            case BlockQuote q: sb.AppendLine("<blockquote>"); foreach (var b in q.Blocks) RenderBlock(b, sb); sb.AppendLine("</blockquote>"); break;
            case Rule: sb.AppendLine("<hr>"); break;
            case ListBlock l: sb.AppendLine(l.Marker == MarkerKind.Decimal ? "<ol>" : "<ul>");
                foreach (var item in l.Items) { sb.Append("<li>"); foreach (var b in item.Blocks) RenderBlock(b, sb); sb.AppendLine("</li>"); }
                sb.AppendLine(l.Marker == MarkerKind.Decimal ? "</ol>" : "</ul>"); break;
            case TableBlock t: RenderTable(t, sb); break;
        }
    }

    private static void RenderInlines(ImmutableArray<Inline> inlines, StringBuilder sb)
    {
        foreach (var i in inlines)
        {
            switch (i)
            {
                case TextInline t: sb.Append(t.Style.Bold ? "<b>" : t.Style.Italic ? "<i>" : t.Style.Code ? "<code>" : "").Append(System.Net.WebUtility.HtmlEncode(t.Text)).Append(t.Style.Bold ? "</b>" : t.Style.Italic ? "</i>" : t.Style.Code ? "</code>" : ""); break;
                case Link l: sb.Append("<a href=\"").Append(System.Net.WebUtility.HtmlEncode(l.Url)).Append("\">"); RenderInlines(l.Content, sb); sb.Append("</a>"); break;
                case Image img: sb.Append("<img alt=\"").Append(System.Net.WebUtility.HtmlEncode(img.Alt)).Append(img.Source != null ? $"\" src=\"{System.Net.WebUtility.HtmlEncode(img.Source)}\"" : "\"").Append(">"); break;
                case LineBreak: sb.Append("<br>"); break;
                default: break;
            }
        }
    }

    private static void RenderTable(TableBlock table, StringBuilder sb)
    {
        sb.AppendLine("<table>");
        for (int r = 0; r < table.Rows.Length; r++)
        {
            sb.AppendLine("<tr>");
            foreach (var cell in table.Rows[r])
            {
                var tag = r < table.HeaderRows ? "th" : "td";
                var cs = cell.ColSpan > 1 ? $" colspan=\"{cell.ColSpan}\"" : "";
                sb.Append($"<{tag}{cs}>");
                foreach (var b in cell.Blocks) RenderBlock(b, sb);
                sb.AppendLine($"</{tag}>");
            }
            sb.AppendLine("</tr>");
        }
        sb.AppendLine("</table>");
    }
}
