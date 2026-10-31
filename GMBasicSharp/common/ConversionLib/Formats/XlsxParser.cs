// ConversionLib/Formats/XlsxParser.cs — via ClosedXML
using System.Collections.Immutable;
using System.Text;
using ClosedXML.Excel;

namespace Basic.ConversionLib.Formats;

public static class XlsxParser
{
    public static Model.Document Parse(byte[] bytes)
    {
        using var ms = new MemoryStream(bytes);
        using var wb = new XLWorkbook(ms);

        // Use first worksheet
        var ws = wb.Worksheets.FirstOrDefault();
        if (ws == null) return Model.Document.Empty;

        var rows = new List<ImmutableArray<Model.Cell>>();
        var usedRange = ws.RangeUsed();
        if (usedRange == null) return Model.Document.Empty;

        foreach (var row in usedRange.RowsUsed())
        {
            var cells = new List<Model.Cell>();
            foreach (var cell in row.CellsUsed())
            {
                var text = cell.GetFormattedString();
                cells.Add(new Model.Cell([new Model.Paragraph(
                    [new Model.TextInline(text, Model.InlineStyle.None)])]));
            }
            if (cells.Count > 0) rows.Add(cells.ToImmutableArray());
        }

        return rows.Count > 0
            ? new Model.Document([new Model.TableBlock([.. rows], 1)], [])
            : Model.Document.Empty;
    }
}
