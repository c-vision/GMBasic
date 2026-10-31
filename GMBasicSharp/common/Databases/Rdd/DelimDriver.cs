// FILE: Basic.Databases.Rdd.DelimDriver.cs
// Driver DELIM (delimited text, XSharp-compatible).
// Separatore configurabile (default: virgola). Header opzionale.
// Modello: riga = record, indice virtuale = numero di riga, manipolazione con write-back.

namespace Basic.Databases.Rdd;

/// <summary>Driver per file di testo delimitati (DELIM). Compatibile XSharp.</summary>
[DatabaseDriver("DELIM", Extensions = new[] { ".delim", ".txt", ".csv" })]
public class DelimDriver : DelimitedTextDriver
{
    public override string DriverName => "DELIM";

    public DelimDriver() { _separator = ","; _hasHeader = true; }
    public DelimDriver(string separator, bool hasHeader = true)
    {
        _separator = separator;
        _hasHeader = hasHeader;
    }
}
