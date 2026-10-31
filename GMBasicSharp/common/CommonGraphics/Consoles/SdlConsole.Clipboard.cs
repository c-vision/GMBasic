using CommonLib.Consoles;
// FILE: Interpreter.Consoles.SdlConsole.Clipboard.cs

using System.Text;

namespace CommonGraphics.Consoles;

/// <summary>
/// Contiene l'implementazione dei metodi di interazione con gli appunti
/// per la classe parziale SdlConsole.
/// </summary>
public sealed partial class SdlConsole
{
    /// <summary>
    /// Copia l'intero contenuto del buffer di testo della console negli appunti di sistema
    /// utilizzando il servizio di clipboard iniettato.
    /// Gli spazi vuoti alla fine di ogni riga vengono rimossi.
    /// </summary>
    public void CopyBufferToClipboard()
    {
        var sb = new StringBuilder();
        var lineSb = new StringBuilder(_width);

        for (var r = 0; r < _height; r++)
        {
            lineSb.Clear();
            for (var c = 0; c < _width; c++)
            {
                // Usa il carattere ' ' per le celle nulle o con carattere nullo.
                lineSb.Append(_screenBuffer[r, c].Character == '\0' ? ' ' : _screenBuffer[r, c].Character);
            }
            // Rimuove gli spazi finali da ogni riga prima di aggiungerla.
            sb.AppendLine(lineSb.ToString().TrimEnd());
        }

        // Rimuove le righe vuote alla fine dell'intero testo.
        _clipboardService.SetText(sb.ToString().TrimEnd());
    }

    /// <summary>
    /// Innesca un'operazione di incolla dagli appunti di sistema.
    /// L'hook di tastiera (es. Ctrl+V) è gestito direttamente dal KeyboardService,
    /// che ha accesso sia al clipboard service che al proprio buffer di input.
    /// La console non ha la responsabilità di avviare questa operazione.
    /// </summary>
    public void PasteFromClipboard()
    {
        // Questo metodo rimane vuoto come da design. L'operazione di incolla
        // è un evento di input e viene gestita a un livello superiore.
    }
}