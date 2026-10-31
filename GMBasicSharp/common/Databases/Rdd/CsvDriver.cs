// FILE: Basic.Databases.Rdd.CsvDriver.cs
// Driver CSV (comma-separated, RFC 4180 quoting). Header obbligatorio.
// Modello: riga = record, indice virtuale = numero di riga, manipolazione con write-back.

using System.IO;

namespace Basic.Databases.Rdd;

[DatabaseDriver("CSV", Extensions = new[] { ".csv", ".tsv" })]
public class CsvDriver : DelimitedTextDriver
{
    public override string DriverName => "CSV";

    public CsvDriver()
    {
        _separator = ",";
        _hasHeader = true;
    }
    public CsvDriver(char separator)
    {
        _separator = separator.ToString();
        _hasHeader = true;
    }

    public override void Open(DbOpenInfo info)
    {
        // Auto-detect separatore da estensione (.tsv → tabulatore).
        if (info.FileName.EndsWith(".tsv", System.StringComparison.OrdinalIgnoreCase))
            _separator = "\t";
        base.Open(info);
    }
}
