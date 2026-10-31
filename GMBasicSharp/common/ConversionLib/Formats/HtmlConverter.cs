// ConversionLib/Formats/HtmlConverter.cs
using System.Collections.Immutable;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace Basic.ConversionLib.Formats;

public static class HtmlConverter
{
    public static Model.Document Parse(byte[] bytes)
    {
        var html = System.Text.Encoding.UTF8.GetString(bytes);
        // Try to wrap in a root element for XElement parsing
        XElement root;
        try
        {
            root = XElement.Parse("<root>" + html + "</root>");
        }
        catch
        {
            // Try stripping <!DOCTYPE> and <html> tags, parse as-is
            var cleaned = Regex.Replace(html, "<!DOCTYPE[^>]*>", "", RegexOptions.IgnoreCase);
            try { root = XElement.Parse(cleaned); }
            catch { root = XElement.Parse("<root>" + cleaned + "</root>"); }
        }

        // Strip script/style tags
        foreach (var el in root.Descendants().ToList())
        {
            var name = el.Name.LocalName.ToLowerInvariant();
            if (name is "script" or "style" or "nav" or "header" or "footer")
                el.Remove();
        }

        var body = FindBody(root);
        var blocks = EpubParser.ParseHtmlBlocks(body ?? root);
        return blocks.Count > 0 ? new Model.Document([.. blocks], []) : Model.Document.Empty;
    }

    private static XElement? FindBody(XElement root)
    {
        foreach (var el in root.Descendants())
        {
            var name = el.Name.LocalName.ToLowerInvariant();
            if (name == "body") return el;
        }
        return null;
    }
}
