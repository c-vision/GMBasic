// FILE: Basic.DBF.DbfFieldsList

namespace Basic.DBF;

/// <summary>
/// Parses comma-separated DBF field name lists (e.g. from Clipper <c>FIELDS &amp;macro</c> expansion).
/// </summary>
public static class DbfFieldsList
{
    /// <summary>
    /// Splits a comma-separated field list into trimmed, non-empty names.
    /// </summary>
    /// <param name="fieldList">Raw list such as <c>"Type, Shape"</c>.</param>
    /// <returns>Individual field names in source order.</returns>
    public static IReadOnlyList<string> SplitCommaSeparatedNames(string? fieldList)
    {
        if (string.IsNullOrWhiteSpace(fieldList))
        {
            return [];
        }

        var names = new List<string>();
        var start = 0;
        for (var i = 0; i <= fieldList.Length; i++)
        {
            if (i < fieldList.Length && fieldList[i] != ',')
            {
                continue;
            }

            var segment = fieldList[start..i].Trim();
            if (segment.Length > 0)
            {
                names.Add(segment);
            }

            start = i + 1;
        }

        return names;
    }
}
