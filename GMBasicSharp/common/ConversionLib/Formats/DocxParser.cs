// ConversionLib/Formats/DocxParser.cs
// Parses .docx (Office Open XML) to Document 
// Zero external dependencies — uses System.IO.Compression + System.Xml.Linq.

using System.Collections.Immutable;
using System.IO.Compression;
using System.Xml.Linq;
using Basic.ConversionLib.Model;

namespace Basic.ConversionLib.Formats;

public static class DocxParser
{
    private static readonly XNamespace W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
    private static readonly XNamespace R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
    private static readonly XNamespace WP = "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing";
    private static readonly XNamespace A = "http://schemas.openxmlformats.org/drawingml/2006/main";
    private static readonly XNamespace PIC = "http://schemas.openxmlformats.org/drawingml/2006/picture";
    private static readonly XNamespace MC = "http://schemas.openxmlformats.org/markup-compatibility/2006";
    private static readonly XNamespace VML = "urn:schemas-microsoft-com:vml";

    public static Document Parse(byte[] bytes)
    {
        using var zip = new ZipArchive(new MemoryStream(bytes), ZipArchiveMode.Read);
        var assets = new List<Asset>();

        // Read relationships
        var rels = ReadRels(zip, "_rels/.rels");
        var docPart = FindPartByType(rels, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument")
            ?? "word/document.xml";

        // Read styles
        var stylesPart = FindPartByType(rels, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles")
            ?? "word/styles.xml";
        var stylesCtx = ParseStyles(ReadXmlPart(zip, stylesPart));

        // Read main document
        var docXml = ReadXmlPart(zip, docPart);
        var docRoot = StripMc(docXml);
        var body = docRoot?.Element(W + "body");
        if (body == null) return Document.Empty;

        // Read document-level relationships (for images, hyperlinks)
        var docRelsPath = docPart.Replace("/document.xml", "/_rels/document.xml.rels");
        if (docRelsPath == docPart) docRelsPath = "word/_rels/document.xml.rels";
        var docRels = ReadRels(zip, docRelsPath);
        var docRelsDict = docRels.ToDictionary(r => r.Id, r => r.Target);

        var ctx = new ParseContext(zip, docRelsDict, stylesCtx, assets);
        var blocks = ParseBlocks(body, ctx);

        return new Document([.. blocks], [.. assets]);
    }

    private static List<(string Id, string Type, string Target)> ReadRels(ZipArchive zip, string path)
    {
        var entry = zip.GetEntry(path);
        if (entry == null) return [];
        using var stream = entry.Open();
        var xml = XElement.Load(stream);
        XNamespace ns = "http://schemas.openxmlformats.org/package/2006/relationships";
        return xml.Elements(ns + "Relationship").Select(e => (
            e.Attribute("Id")!.Value,
            e.Attribute("Type")!.Value,
            e.Attribute("Target")!.Value
        )).ToList();
    }

    private static string? FindPartByType(List<(string Id, string Type, string Target)> rels, string type)
    {
        foreach (var (_, rt, target) in rels)
            if (rt == type) return target;
        return null;
    }

    private static XElement? ReadXmlPart(ZipArchive zip, string path)
    {
        path = path.TrimStart('/');
        var entry = zip.GetEntry(path);
        if (entry == null) return null;
        using var stream = entry.Open();
        return XElement.Load(stream);
    }

    private static XElement? StripMc(XElement? root)
    {
        if (root == null) return null;
        // Remove mc:AlternateContent, keep first mc:Choice
        foreach (var ac in root.Descendants(MC + "AlternateContent").ToList())
        {
            var choice = ac.Element(MC + "Choice");
            if (choice != null) ac.ReplaceWith(choice.Elements());
            else ac.Remove();
        }
        return root;
    }

    private static InlineStyle ParseStyles(XElement? stylesXml)
    {
        if (stylesXml == null) return InlineStyle.None;
        var docDefaults = stylesXml.Element(W + "docDefaults");
        var rPrDefault = docDefaults?.Element(W + "rPrDefault")?.Element(W + "rPr");
        var baseStyle = ParseRunStyle(rPrDefault);
        return baseStyle;
    }

    private static InlineStyle ParseRunStyle(XElement? rPr)
    {
        if (rPr == null) return InlineStyle.None;
        return new InlineStyle(
            Bold: ParseOnOff(rPr.Element(W + "b")),
            Italic: ParseOnOff(rPr.Element(W + "i")),
            Strike: ParseOnOff(rPr.Element(W + "strike")) || ParseOnOff(rPr.Element(W + "dstrike")),
            Code: false
        );
    }

    private static bool ParseOnOff(XElement? el)
    {
        if (el == null) return false;
        var val = el.Attribute(W + "val")?.Value;
        if (val == null) return true; // bare <w:b/> means true
        return val is "1" or "true" or "on";
    }

    private static List<Block> ParseBlocks(XElement parent, ParseContext ctx)
    {
        var blocks = new List<Block>();
        foreach (var el in parent.Elements())
        {
            if (el.Name.Namespace != W) continue;
            switch (el.Name.LocalName)
            {
                case "p":
                    var (block, _) = ParseParagraph(el, ctx);
                    if (block != null) blocks.Add(block);
                    break;
                case "tbl":
                    blocks.Add(ParseTable(el, ctx));
                    break;
                case "sdt":
                    var sdtContent = el.Element(W + "sdtContent");
                    if (sdtContent != null) blocks.AddRange(ParseBlocks(sdtContent, ctx));
                    break;
            }
        }
        return blocks;
    }

    private static (Block?, Block?) ParseParagraph(XElement p, ParseContext ctx)
    {
        var pPr = p.Element(W + "pPr");

        // Heading detection
        var pStyle = pPr?.Element(W + "pStyle")?.Attribute(W + "val")?.Value;
        var outlineLvl = pPr?.Element(W + "outlineLvl")?.Attribute(W + "val")?.Value;
        int? headingLevel = null;
        if (outlineLvl != null && int.TryParse(outlineLvl, out var ol) && ol >= 0)
            headingLevel = ol + 1;
        else if (pStyle != null && pStyle.StartsWith("Heading", StringComparison.OrdinalIgnoreCase))
            headingLevel = int.TryParse(pStyle.AsSpan(7), out var hl) ? hl : 1;
        else if (pStyle != null && pStyle.Equals("Title", StringComparison.OrdinalIgnoreCase))
            headingLevel = 1;

        // Resolve paragraph-level style toggles
        var baseStyle = ctx.StyleBase;

        // Parse runs
        var inlines = new List<Inline>();
        ParseRuns(p, ctx, baseStyle, inlines);

        if (headingLevel.HasValue)
        {
            var level = Math.Clamp(headingLevel.Value, 1, 6);
            return (new Heading(level, [.. inlines]), null);
        }

        return (inlines.Count > 0 ? new Paragraph([.. inlines]) : null, null);
    }

    private static void ParseRuns(XElement parent, ParseContext ctx, InlineStyle baseStyle, List<Inline> inlines)
    {
        foreach (var el in parent.Elements())
        {
            if (el.Name.Namespace == MC) continue;

            switch (el.Name.LocalName)
            {
                case "r" when el.Name.Namespace == W:
                    ParseRun(el, ctx, baseStyle, inlines);
                    break;
                case "hyperlink" when el.Name.Namespace == W:
                    var url = ResolveHyperlink(el, ctx);
                    var linkInlines = new List<Inline>();
                    ParseRuns(el, ctx, baseStyle, linkInlines);
                    if (linkInlines.Count > 0)
                        inlines.Add(new Link([.. linkInlines], url ?? ""));
                    break;
                case "bookmarkStart" when el.Name.Namespace == W:
                    // skip for now
                    break;
                case "sdt" when el.Name.Namespace == W:
                    ParseRuns(el.Element(W + "sdtContent") ?? el, ctx, baseStyle, inlines);
                    break;
                case "drawing" when el.Name.Namespace == W:
                    ParseDrawing(el, ctx, inlines);
                    break;
                case "pict" when el.Name.Namespace == W:
                    ParsePict(el, ctx, inlines);
                    break;
                case "br" when el.Name.Namespace == W:
                case "cr" when el.Name.Namespace == W:
                    inlines.Add(new LineBreak());
                    break;
                case "fldSimple" when el.Name.Namespace == W:
                    ParseRuns(el, ctx, baseStyle, inlines);
                    break;
            }
        }
    }

    private static void ParseRun(XElement r, ParseContext ctx, InlineStyle baseStyle, List<Inline> inlines)
    {
        var rPr = r.Element(W + "rPr");
        var style = baseStyle;

        // Direct run formatting (absolute)
        if (rPr != null)
        {
            style = new InlineStyle(
                Bold: ParseOnOff(rPr.Element(W + "b")) || style.Bold,
                Italic: ParseOnOff(rPr.Element(W + "i")) || style.Italic,
                Strike: ParseOnOff(rPr.Element(W + "strike")) || ParseOnOff(rPr.Element(W + "dstrike")) || style.Strike,
                Code: false
            );
        }

        foreach (var el in r.Elements())
        {
            if (el.Name.Namespace != W) continue;
            switch (el.Name.LocalName)
            {
                case "t":
                    var text = el.Value;
                    var preserve = (string?)el.Attribute(XNamespace.Xml + "space");
                    if (preserve != "preserve")
                        text = text.Trim();
                    if (text.Length > 0)
                        inlines.Add(new TextInline(text, style));
                    break;
                case "tab":
                case "ptab":
                    inlines.Add(new TextInline(" ", InlineStyle.None));
                    break;
                case "br":
                case "cr":
                    inlines.Add(new LineBreak());
                    break;
                case "drawing":
                    ParseDrawing(el, ctx, inlines);
                    break;
                case "pict":
                    ParsePict(el, ctx, inlines);
                    break;
            }
        }
    }

    private static string? ResolveHyperlink(XElement hl, ParseContext ctx)
    {
        var rId = hl.Attribute(R + "id")?.Value;
        if (rId != null && ctx.Rels.TryGetValue(rId, out var target))
            return target;
        return hl.Attribute(W + "anchor")?.Value;
    }

    private static void ParseDrawing(XElement drawing, ParseContext ctx, List<Inline> inlines)
    {
        var blip = drawing.Descendants(A + "blip").FirstOrDefault();
        if (blip == null) return;
        var embed = blip.Attribute(R + "embed")?.Value;
        if (embed != null && ctx.Rels.TryGetValue(embed, out var target))
        {
            var bytes = ReadPartBytes(ctx.Zip, target);
            var ext = Path.GetExtension(target).ToLowerInvariant();
            var mime = ext switch
            {
                ".png" => "image/png",
                ".jpg" or ".jpeg" => "image/jpeg",
                ".gif" => "image/gif",
                ".bmp" => "image/bmp",
                _ => "application/octet-stream"
            };
            var id = ctx.Assets.Count;
            ctx.Assets.Add(new Asset(id, mime, target, bytes ?? []));
            // For now: use alt text as image, don't embed asset data in markdown
            var docPr = drawing.Descendants(WP + "docPr").FirstOrDefault();
            var alt = docPr?.Attribute("descr")?.Value ?? docPr?.Attribute("name")?.Value ?? "image";
            inlines.Add(new Image(alt, null)); // asset not serializable in Github markdown text
        }
    }

    private static void ParsePict(XElement pict, ParseContext ctx, List<Inline> inlines)
    {
        var imagedata = pict.Descendants(VML + "imagedata").FirstOrDefault()
            ?? pict.Descendants().FirstOrDefault(e => e.Name.LocalName == "imagedata");
        if (imagedata == null) return;
        var src = imagedata.Attribute(R + "id")?.Value
            ?? imagedata.Attribute("src")?.Value;
        if (src != null)
            inlines.Add(new Image("image", src));
    }

    private static byte[]? ReadPartBytes(ZipArchive zip, string path)
    {
        path = path.TrimStart('/');
        var entry = zip.GetEntry(path);
        if (entry == null) return null;
        using var stream = entry.Open();
        using var ms = new MemoryStream();
        stream.CopyTo(ms);
        return ms.ToArray();
    }

    private static Block ParseTable(XElement tbl, ParseContext ctx)
    {
        var rows = new List<ImmutableArray<Cell>>();
        var headerRows = 0;

        foreach (var tr in tbl.Elements(W + "tr"))
        {
            var trPr = tr.Element(W + "trPr");
            var isHeader = ParseOnOff(trPr?.Element(W + "tblHeader"));
            var cells = ImmutableArray.CreateBuilder<Cell>();

            foreach (var tc in tr.Elements(W + "tc"))
            {
                var tcPr = tc.Element(W + "tcPr");
                var gridSpan = tcPr?.Element(W + "gridSpan");
                var colSpan = gridSpan != null ? Math.Max(1, int.Parse(gridSpan.Attribute(W + "val")!.Value)) : 1;
                var vMerge = tcPr?.Element(W + "vMerge");
                var rowSpan = 1; // simplified — full vMerge needs tracking

                var cellBlocks = ParseBlocks(tc, ctx);
                cells.Add(new Cell([.. cellBlocks], colSpan, rowSpan));
            }
            if (isHeader && headerRows == 0) headerRows = rows.Count + 1;
            rows.Add(cells.ToImmutable());
        }

        return new TableBlock([.. rows], headerRows);
    }

    private sealed class ParseContext
    {
        public ZipArchive Zip;
        public Dictionary<string, string> Rels;
        public InlineStyle StyleBase;
        public List<Asset> Assets;
        public ParseContext(ZipArchive zip, Dictionary<string, string> rels, InlineStyle styleBase, List<Asset> assets)
        {
            Zip = zip; Rels = rels; StyleBase = styleBase; Assets = assets;
        }
    }
}
