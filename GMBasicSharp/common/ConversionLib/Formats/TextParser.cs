// ConversionLib/Formats/TextParser.cs
using System.Collections.Immutable;

namespace Basic.ConversionLib.Formats;

public static class TextParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        var text = System.Text.Encoding.UTF8.GetString(bytes);
        var lines = text.Replace("\r\n", "\n").Replace('\r', '\n').Split('\n');
        var blocks = new List<Model.Block>();

        foreach (var line in lines)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                blocks.Add(new Model.Paragraph([]));
                continue;
            }
            blocks.Add(new Model.Paragraph([new Model.TextInline(line, Model.InlineStyle.None)]));
        }

        if (blocks.Count == 0)
            return Model.Document.Empty;

        return new Model.Document([.. blocks], []);
    }
}
