// ConversionLib/Formats/EpubParser.cs — via VersOne.Epub
using System.Collections.Immutable;
using System.Xml.Linq;
using System.Text;
using VersOne.Epub;

namespace Basic.ConversionLib.Formats;

public static class EpubParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        using var ms = new MemoryStream(bytes);
        var book = EpubReader.ReadBook(ms);
        var allBlocks = new List<Model.Block>();

        foreach (var chapter in book.ReadingOrder)
        {
            allBlocks.AddRange(ParseHtmlContent(chapter.Content));
        }

        return allBlocks.Count > 0 ? new Model.Document([.. allBlocks], []) : Model.Document.Empty;
    }

    // Shared HTML parsing — also used by HtmlConverter
    internal static List<Model.Block> ParseHtmlBlocks(XElement el)
    {
        var blocks = new List<Model.Block>();
        WalkHtmlChildren(el, blocks);
        return blocks;
    }

    private static List<Model.Block> ParseHtmlContent(string html)
    {
        // Strip DOCTYPE, wrap if needed
        var cleaned = System.Text.RegularExpressions.Regex.Replace(html, "<!DOCTYPE[^>]*>", "",
            System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        XElement body;
        try { body = XElement.Parse("<root>" + cleaned + "</root>"); }
        catch { body = XElement.Parse("<root><p>Error parsing HTML</p></root>"); }
        return ParseHtmlBlocks(body);
    }

    internal static void WalkHtmlChildren(XElement parent, List<Model.Block> blocks)
    {
        foreach (var child in parent.Elements())
        {
            var name = child.Name.LocalName.ToLowerInvariant();
            if (name is "div" or "section" or "article" or "body" or "html" or "root")
            { WalkHtmlChildren(child, blocks); continue; }

            if (name is "h1" or "h2" or "h3" or "h4" or "h5" or "h6")
                blocks.Add(new Model.Heading(name[1] - '0', [.. ParseHtmlInlines(child)]));
            else if (name == "p")
            { var inlines = ParseHtmlInlines(child); if (inlines.Count > 0) blocks.Add(new Model.Paragraph([.. inlines])); }
            else if (name == "table")
                blocks.Add(ParseHtmlTable(child));
            else if (name is "ul" or "ol")
            { var items = new List<Model.ListItem>(); foreach (var li in child.Elements().Where(e => e.Name.LocalName.ToLowerInvariant() == "li")) { var ib = new List<Model.Block>(); WalkHtmlChildren(li, ib); items.Add(new Model.ListItem([.. ib])); }
              blocks.Add(new Model.ListBlock(name == "ol" ? Model.MarkerKind.Decimal : Model.MarkerKind.Bullet, 1, [.. items])); }
            else if (name == "blockquote")
            { var ib = new List<Model.Block>(); WalkHtmlChildren(child, ib); blocks.Add(new Model.BlockQuote([.. ib])); }
            else if (name == "pre")
                blocks.Add(new Model.CodeBlock(null, child.Value));
            else if (name == "hr")
                blocks.Add(new Model.Rule());
        }
    }

    private static List<Model.Inline> ParseHtmlInlines(XElement parent)
    {
        var inlines = new List<Model.Inline>();
        WalkNodes(parent.Nodes(), inlines, Model.InlineStyle.None);
        return inlines;
    }

    private static void WalkNodes(IEnumerable<XNode> nodes, List<Model.Inline> inlines, Model.InlineStyle inherited)
    {
        foreach (var node in nodes)
        {
            if (node is XText xt && xt.Value.Length > 0)
                inlines.Add(new Model.TextInline(xt.Value, inherited));
            else if (node is XElement el)
            {
                var name = el.Name.LocalName.ToLowerInvariant();
                var style = name switch { "b" or "strong" => inherited with { Bold = true }, "i" or "em" => inherited with { Italic = true }, "strike" or "del" or "s" => inherited with { Strike = true }, "code" => inherited with { Code = true }, _ => inherited };
                if (name == "br") inlines.Add(new Model.LineBreak());
                else if (name == "img") inlines.Add(new Model.Image(el.Attribute("alt")?.Value ?? "", el.Attribute("src")?.Value));
                else if (name == "a") { var href = el.Attribute("href")?.Value ?? ""; var li = new List<Model.Inline>(); WalkNodes(el.Nodes(), li, style); if (li.Count > 0) inlines.Add(new Model.Link([.. li], href)); }
                else WalkNodes(el.Nodes(), inlines, style);
            }
        }
    }

    private static Model.Block ParseHtmlTable(XElement tableEl)
    {
        var rows = new List<ImmutableArray<Model.Cell>>();
        foreach (var child in tableEl.Elements())
        {
            if (child.Name.LocalName.ToLowerInvariant() is "thead" or "tbody" or "tfoot")
            { foreach (var tr in child.Elements()) { if (tr.Name.LocalName.ToLowerInvariant() == "tr") AddRow(tr, rows); } continue; }
            if (child.Name.LocalName.ToLowerInvariant() == "tr") AddRow(child, rows);
        }
        return new Model.TableBlock([.. rows]);
    }

    private static void AddRow(XElement tr, List<ImmutableArray<Model.Cell>> rows)
    {
        var cells = new List<Model.Cell>();
        foreach (var tc in tr.Elements())
        {
            if (tc.Name.LocalName.ToLowerInvariant() is "td" or "th")
            { var bl = new List<Model.Block>(); WalkHtmlChildren(tc, bl); cells.Add(new Model.Cell([.. bl])); }
        }
        if (cells.Count > 0) rows.Add([.. cells]);
    }
}
