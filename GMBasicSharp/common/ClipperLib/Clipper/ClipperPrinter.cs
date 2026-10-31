using CommonLib.Consoles.Tui;
using CommonLib.Screens;
namespace ClipperLib.Clipper;

/// <summary>
/// Funzioni stampante di Clipper 5.3 (SET PRINTER, SET DEVICE, PROW, PCOL, EJECT).
/// La stampante è virtuale e in memoria: QOUT/QQOUT/DEVOUT instradano il testo su un
/// buffer quando SET PRINTER è ON oppure SET DEVICE è PRINTER (in quel caso solo stampante).
/// </summary>
public static class ClipperPrinter
{
    private static ClipperState S() => ClipperGlobal.State;

    /// <summary>SET PRINTER ON/OFF — l'output va anche alla stampante.</summary>
    public static bool SetPrinter(bool? on = null) => ClipperSet.SetPrinter(on);

    /// <summary>SET DEVICE TO &lt;device&gt; — "PRINTER" instrada solo alla stampante.</summary>
    public static string SetDevice(string? device = null) => ClipperSet.SetDevice(device);

    /// <summary>PROW() — riga corrente della stampante.</summary>
    public static int Prow() => S().PrinterRow;

    /// <summary>PCOL() — colonna corrente della stampante.</summary>
    public static int Pcol() => S().PrinterCol;

    /// <summary>EJECT() — invia un salto pagina (azzera il buffer e le coordinate) e spegne SET PRINTER.</summary>
    public static void Eject()
    {
        var st = S();
        st.PrinterBuffer.Clear();
        st.PrinterRow = 0;
        st.PrinterCol = 0;
        st.PrinterOn = false;
    }

    /// <summary>Testo accumulato nel buffer stampante (utilità di lettura, non Clipper).</summary>
    public static string PrinterText() => S().PrinterBuffer.ToString();

    /// <summary>True se l'output deve andare alla stampante.</summary>
    public static bool Routing() =>
        S().PrinterOn || S().PrinterDevice == "PRINTER";

    /// <summary>True se l'output deve andare SOLO alla stampante (nessun video).</summary>
    public static bool PrinterOnly() =>
        S().PrinterDevice == "PRINTER";

    /// <summary>Accoda testo alla stampante e aggiorna PROW/PCOL.</summary>
    public static void Write(string s, bool newLine)
    {
        var st = S();
        if (!Routing()) return;
        if (newLine)
        {
            st.PrinterBuffer.AppendLine(s);
            st.PrinterRow++;
            st.PrinterCol = 0;
        }
        else
        {
            st.PrinterBuffer.Append(s);
            st.PrinterCol += s.Length;
        }
    }
}
