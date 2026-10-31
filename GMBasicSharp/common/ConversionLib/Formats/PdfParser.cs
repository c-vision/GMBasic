// ConversionLib/Formats/PdfParser.cs
using System.Collections.Immutable;
using System.Text;
using UglyToad.PdfPig;
using UglyToad.PdfPig.Content;
using UglyToad.PdfPig.Graphics;

namespace Basic.ConversionLib.Formats;

public static class PdfParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        var blocks = new List<Model.Block>();
        var assets = new List<Model.Asset>();

        using var pdf = PdfDocument.Open(bytes);

        foreach (var page in pdf.GetPages())
        {
            var words = page.GetWords().ToList();
            if (words.Count == 0) continue;

            // Group words by line (Y position)
            var lines = GroupWordsByLine(words);
            // Compute average font size for body text
            var avgSize = lines.Select(l => l.FontSize).DefaultIfEmpty(12).Average();
            
            var pageBlocks = new List<Model.Block>();
            
            foreach (var line in lines)
            {
                var text = line.Text.Trim();
                if (string.IsNullOrWhiteSpace(text)) continue;

                // Determine heading level from font size ratio
                int? headingLevel = null;
                if (avgSize > 0 && line.FontSize >= avgSize * 1.35)
                    headingLevel = line.FontSize >= avgSize * 2.0 ? 1
                                 : line.FontSize >= avgSize * 1.7 ? 2
                                 : line.FontSize >= avgSize * 1.5 ? 3 : 4;

                // Detect list markers
                var (isList, listText) = DetectList(text);
                
                // Build inlines with style from word-level font data
                var inlines = BuildInlines(line.Words);

                if (headingLevel.HasValue)
                    pageBlocks.Add(new Model.Heading(headingLevel.Value, [.. inlines]));
                else if (isList)
                    pageBlocks.Add(new Model.Paragraph([.. inlines])); // flat list for now
                else
                    pageBlocks.Add(new Model.Paragraph([.. inlines]));
            }

            if (pageBlocks.Count > 0)
            {
                blocks.Add(new Model.Heading(2, [new Model.TextInline($"Page {page.Number}", Model.InlineStyle.None)]));
                blocks.AddRange(pageBlocks);
            }

            // Extract images
            foreach (var img in page.GetImages())
            {
                if (img.TryGetPng(out var pngBytes))
                {
                    assets.Add(new Model.Asset(assets.Count, "image/png", $"page{page.Number}_img{assets.Count}.png", pngBytes));
                }
            }
        }

        return blocks.Count > 0 ? new Model.Document([.. blocks], [.. assets]) : Model.Document.Empty;
    }

    private static List<LineInfo> GroupWordsByLine(List<Word> words)
    {
        if (words.Count == 0) return [];

        var lines = new List<LineInfo>();
        var currentLine = new List<Word>();
        double currentY = words[0].BoundingBox.Bottom; // use bottom for Y grouping
        const double yTolerance = 3; // pixels tolerance

        foreach (var word in words)
        {
            var wordY = word.BoundingBox.Bottom;
            if (Math.Abs(wordY - currentY) > yTolerance)
            {
                if (currentLine.Count > 0)
                    lines.Add(CreateLineInfo(currentLine));
                currentLine = [word];
                currentY = wordY;
            }
            else
            {
                currentLine.Add(word);
            }
        }
        if (currentLine.Count > 0)
            lines.Add(CreateLineInfo(currentLine));

        return lines;
    }

    private static LineInfo CreateLineInfo(List<Word> lineWords)
    {
        var sorted = lineWords.OrderBy(w => w.BoundingBox.Left).ToList();
        var text = string.Join(" ", sorted.Select(w => w.Text));
        var fontSize = sorted.Select(w => w.Letters.FirstOrDefault()?.FontSize ?? 12).DefaultIfEmpty(12).Average();
        return new LineInfo(text, fontSize, sorted);
    }

    private sealed record LineInfo(string Text, double FontSize, List<Word> Words);

    private static List<Model.Inline> BuildInlines(List<Word> words)
    {
        var inlines = new List<Model.Inline>();
        Model.InlineStyle? currentStyle = null;
        var sb = new StringBuilder();

        foreach (var word in words)
        {
            foreach (var letter in word.Letters)
            {
                var style = GetLetterStyle(letter);
                if (currentStyle != null && style != currentStyle)
                {
                    if (sb.Length > 0)
                    {
                        inlines.Add(new Model.TextInline(sb.ToString(), currentStyle.Value));
                        sb.Clear();
                    }
                }
                currentStyle = style;
                sb.Append(letter.Value);
            }
        }
        if (sb.Length > 0 && currentStyle.HasValue)
            inlines.Add(new Model.TextInline(sb.ToString(), currentStyle.Value));

        return inlines;
    }

    private static Model.InlineStyle GetLetterStyle(Letter letter)
    {
        var fontName = letter.FontName?.ToLowerInvariant() ?? "";
        return new Model.InlineStyle(
            Bold: letter.FontName?.Contains("Bold", StringComparison.OrdinalIgnoreCase) == true
                || letter.FontName?.Contains("Black", StringComparison.OrdinalIgnoreCase) == true
                || letter.FontName?.Contains("Heavy", StringComparison.OrdinalIgnoreCase) == true,
            Italic: letter.FontName?.Contains("Italic", StringComparison.OrdinalIgnoreCase) == true
                || letter.FontName?.Contains("Oblique", StringComparison.OrdinalIgnoreCase) == true,
            Strike: false,
            Code: fontName.Contains("Courier", StringComparison.OrdinalIgnoreCase)
                || fontName.Contains("Consolas", StringComparison.OrdinalIgnoreCase)
                || fontName.Contains("Mono", StringComparison.OrdinalIgnoreCase)
        );
    }

    private static (bool isList, string text) DetectList(string text)
    {
        if (text.Length > 2 && text[0] == '-' && text[1] == ' ') return (true, text[2..]);
        if (text.Length > 2 && text[0] == '*' && text[1] == ' ') return (true, text[2..]);
        // Numbered list: "1. " or "1) "
        if (text.Length > 3 && char.IsDigit(text[0]))
        {
            int i = 1;
            while (i < text.Length && char.IsDigit(text[i])) i++;
            if (i < text.Length && (text[i] == '.' || text[i] == ')') && i + 1 < text.Length && text[i + 1] == ' ')
                return (true, text[(i + 2)..]);
        }
        return (false, text);
    }
}
