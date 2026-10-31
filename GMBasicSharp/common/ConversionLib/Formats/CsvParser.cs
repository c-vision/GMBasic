// ConversionLib/Formats/CsvParser.cs
using System.Collections.Immutable;
using Basic.ConversionLib.Model;

namespace Basic.ConversionLib.Formats;

public static class CsvParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        var text = System.Text.Encoding.UTF8.GetString(bytes);
        // Auto-detect encoding BOM
        if (bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
            text = System.Text.Encoding.UTF8.GetString(bytes, 3, bytes.Length - 3);

        var rows = new List<ImmutableArray<Model.Cell>>();

        using var reader = new StringReader(text);
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            var fields = ParseCsvLine(line);
            var cells = fields.Select(f => new Model.Cell(
                [new Model.Paragraph([new Model.TextInline(f.Trim(), Model.InlineStyle.None)])]
            )).ToArray();
            rows.Add([.. cells]);
        }

        if (rows.Count == 0)
            return Model.Document.Empty;

        var table = new Model.TableBlock([.. rows], 1);
        return new Model.Document([table], []);
    }

    private static List<string> ParseCsvLine(string line)
    {
        var fields = new List<string>();
        var current = new System.Text.StringBuilder();
        bool inQuotes = false;

        for (int i = 0; i < line.Length; i++)
        {
            var c = line[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < line.Length && line[i + 1] == '"')
                    {
                        current.Append('"');
                        i++;
                    }
                    else
                    {
                        inQuotes = false;
                    }
                }
                else
                {
                    current.Append(c);
                }
            }
            else
            {
                if (c == '"')
                {
                    inQuotes = true;
                }
                else if (c == ',')
                {
                    fields.Add(current.ToString());
                    current.Clear();
                }
                else
                {
                    current.Append(c);
                }
            }
        }
        fields.Add(current.ToString());
        return fields;
    }
}
