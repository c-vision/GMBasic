// FILE: Basic.DBF.DbfListFormatter

using System.Globalization;
using System.Text;

namespace Basic.DBF;

/// <summary>
/// Formats DBF records for LIST output (xBase-style: record number + space-separated values).
/// </summary>
public static class DbfListFormatter
{
    /// <summary>
    /// Formats a single field value for LIST display.
    /// </summary>
    public static string FormatValue(object? value) =>
        value switch
        {
            null => string.Empty,
            bool b => b ? ".T." : ".F.",
            DateTime dt => dt.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture),
            IFormattable f => f.ToString(null, CultureInfo.InvariantCulture) ?? string.Empty,
            _ => value.ToString()?.Trim() ?? string.Empty
        };

    /// <summary>
    /// Formats one LIST line: padded record number followed by column values.
    /// </summary>
    public static string FormatLine(long recordNumber, IReadOnlyList<object?> values)
    {
        var sb = new StringBuilder();
        sb.Append(recordNumber.ToString(CultureInfo.InvariantCulture).PadLeft(8));
        foreach (var value in values)
        {
            sb.Append(' ');
            sb.Append(FormatValue(value));
        }

        return sb.ToString();
    }
}
