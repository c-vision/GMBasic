// ConversionLib/Formats/PptxParser.cs — via DocumentFormat.OpenXml
using System.Collections.Immutable;
using DocumentFormat.OpenXml.Packaging;
using DocumentFormat.OpenXml.Presentation;
using A = DocumentFormat.OpenXml.Drawing;

namespace Basic.ConversionLib.Formats;

public static class PptxParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        using var ms = new MemoryStream(bytes);
        using var doc = PresentationDocument.Open(ms, false);
        var pres = doc.PresentationPart?.Presentation;
        if (pres == null) return Model.Document.Empty;

        var allBlocks = new List<Model.Block>();
        int slideNum = 0;

        foreach (var slidePart in doc.PresentationPart!.SlideParts!)
        {
            slideNum++;
            var blocks = new List<Model.Block>();

            foreach (var shape in slidePart.Slide?.Descendants<Shape>() ?? [])
            {
                var name = shape.NonVisualShapeProperties?
                    .NonVisualDrawingProperties?.Name?.Value ?? "";
                var textBody = shape.TextBody;
                if (textBody == null) continue;

                bool isTitle = name.IndexOf("Title", StringComparison.OrdinalIgnoreCase) >= 0;

                foreach (var p in textBody.Elements<A.Paragraph>())
                {
                    var inlines = new List<Model.Inline>();
                    foreach (var r in p.Elements<A.Run>())
                    {
                        var bold = r.RunProperties?.Bold?.Value ?? false;
                        var italic = r.RunProperties?.Italic?.Value ?? false;
                        var style = new Model.InlineStyle(bold, italic, false, false);
                        var text = r.Text?.Text ?? "";
                        if (text.Length > 0)
                            inlines.Add(new Model.TextInline(text, style));
                    }
                    if (inlines.Count > 0)
                    {
                        if (isTitle) blocks.Add(new Model.Heading(3, [.. inlines]));
                        else blocks.Add(new Model.Paragraph([.. inlines]));
                    }
                }
            }

            if (blocks.Count > 0)
            {
                allBlocks.Add(new Model.Heading(2, [new Model.TextInline($"Slide {slideNum}", Model.InlineStyle.None)]));
                allBlocks.AddRange(blocks);
            }
        }

        return allBlocks.Count > 0 ? new Model.Document([.. allBlocks], []) : Model.Document.Empty;
    }
}
