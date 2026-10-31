// ConversionLib/Formats/OdtParser.cs
using System.Collections.Immutable;
using Basic.ConversionLib.Model;
using System.IO.Compression;
using System.Xml.Linq;

namespace Basic.ConversionLib.Formats;

public static class OdtParser
{
    private static readonly XNamespace TEXT = "urn:oasis:names:tc:opendocument:xmlns:text:1.0";
    private static readonly XNamespace OFFICE = "urn:oasis:names:tc:opendocument:xmlns:office:1.0";

    public static Model.Document Parse(byte[] bytes)
    {
        using var zip = new ZipArchive(new MemoryStream(bytes), ZipArchiveMode.Read);
        var entry = zip.GetEntry("content.xml");
        if (entry == null) return Model.Document.Empty;

        using var stream = entry.Open();
        var xml = XElement.Load(stream);
        var body = xml.Element(OFFICE + "body")?.Element(OFFICE + "text");
        if (body == null) return Model.Document.Empty;

        var blocks = ParseTextElements(body);
        return blocks.Count > 0 ? new Model.Document([.. blocks], []) : Model.Document.Empty;
    }

    private static List<Model.Block> ParseTextElements(XElement parent)
    {
        var blocks = new List<Model.Block>();

        foreach (var el in parent.Elements())
        {
            if (el.Name == TEXT + "h")
            {
                var levelStr = el.Attribute(TEXT + "outline-level")?.Value ?? "1";
                int level = int.TryParse(levelStr, out var l) ? l : 1;
                var inlines = ParseTextSpans(el);
                blocks.Add(new Model.Heading(level, [.. inlines]));
            }
            else if (el.Name == TEXT + "p")
            {
                var inlines = ParseTextSpans(el);
                if (inlines.Count > 0)
                    blocks.Add(new Model.Paragraph([.. inlines]));
            }
            else if (el.Name == TEXT + "list")
            {
                var items = ParseListItems(el);
                if (items.Count > 0)
                    blocks.Add(new Model.ListBlock(Model.MarkerKind.Bullet, 1, [.. items]));
            }
            else if (el.Name == TABLE + "table")
            {
                blocks.Add(ParseOdtTable(el));
            }
        }

        return blocks;
    }

    private static List<Model.Inline> ParseTextSpans(XElement parent)
    {
        var inlines = new List<Model.Inline>();

        foreach (var el in parent.Elements())
        {
            if (el.Name == TEXT + "span" || el.Name == TEXT + "a")
            {
                var style = ParseSpanStyle(el);
                var text = string.Concat(el.Nodes().OfType<XText>().Select(t => t.Value));

                if (el.Name == TEXT + "a")
                {
                    var href = el.Attribute(XNamespace.Get("http://www.w3.org/1999/xlink") + "href")?.Value;
                    if (!string.IsNullOrEmpty(text))
                        inlines.Add(new Model.Link(
                            [new Model.TextInline(text, style)], href ?? ""));
                }
                else if (text.Length > 0)
                {
                    inlines.Add(new Model.TextInline(text, style));
                }
            }
            else if (el.Name == TEXT + "s")
            {
                inlines.Add(new Model.TextInline(" ", Model.InlineStyle.None));
            }
            else if (el.Name == TEXT + "tab")
            {
                inlines.Add(new Model.TextInline("\t", Model.InlineStyle.None));
            }
            else if (el.Name == TEXT + "line-break")
            {
                inlines.Add(new Model.LineBreak());
            }
        }

        return inlines;
    }

    private static Model.InlineStyle ParseSpanStyle(XElement el)
    {
        // Simplified: ODT uses style names, full resolution needs content.xml auto-styles
        // Common patterns: "Bold", "Italic", "Bold_Italic"
        var name = el.Attribute(TEXT + "style-name")?.Value ?? "";
        return new Model.InlineStyle(
            Bold: name.Contains("Bold", StringComparison.OrdinalIgnoreCase) || name.Contains("Strong"),
            Italic: name.Contains("Italic", StringComparison.OrdinalIgnoreCase) || name.Contains("Emphasis"),
            Strike: false,
            Code: false
        );
    }

    private static List<Model.ListItem> ParseListItems(XElement list)
    {
        var items = new List<Model.ListItem>();
        foreach (var item in list.Elements(TEXT + "list-item"))
        {
            var blocks = ParseTextElements(item);
            items.Add(new Model.ListItem([.. blocks]));
        }
        return items;
    }

    // Table support — basic
    private static readonly XNamespace TABLE = "urn:oasis:names:tc:opendocument:xmlns:table:1.0";

    private static Model.Block ParseOdtTable(XElement tableEl)
    {
        var rows = new List<ImmutableArray<Model.Cell>>();
        foreach (var tr in tableEl.Elements(TABLE + "table-row"))
        {
            var cells = new List<Model.Cell>();
            foreach (var tc in tr.Elements(TABLE + "table-cell"))
            {
                var blocks = ParseTextElements(tc);
                cells.Add(new Model.Cell(blocks.Count > 0 ? [.. blocks] : []));
            }
            if (cells.Count > 0) rows.Add([.. cells]);
        }
        return rows.Count > 0 ? new Model.TableBlock([.. rows]) : (Model.Block)new Model.Paragraph([]);
    }
}
