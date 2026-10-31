// FILE: Interpreter.Consoles.SdlConsole.Writer.cs

using System.Text;

namespace CommonGraphics.Consoles;

/// <summary>
/// Contiene i metodi di input per la classe parziale SdlConsole.
/// Questa implementazione delega le operazioni di lettura bloccanti al KeyboardService
/// per consentire l'intercettazione centralizzata degli hook di tastiera.
/// </summary>
public sealed partial class SdlConsole
{
    /// <summary>
    /// Implementazione interna di TextWriter che reindirizza al buffer della console.
    /// </summary>
    /// <summary>
    /// Implementazione interna di TextWriter che reindirizza al buffer della console.
    /// </summary>
    private sealed class SdlConsoleWriter(SdlConsole console) : TextWriter
    {
        /// <summary>
        /// Ottiene l'encoding dell'output.
        /// </summary>
        public override Encoding Encoding => Encoding.UTF8;

        /// <summary>
        /// Scrive un singolo carattere chiamando il metodo di scrittura interno del proprietario.
        /// </summary>
        /// <param name="value">Il carattere da scrivere.</param>
        public override void Write(char value) => console.WriteInternal(value);

        /// <summary>
        /// Scrive una stringa chiamando il metodo di scrittura interno del proprietario.
        /// </summary>
        /// <param name="value">La stringa da scrivere.</param>
        public override void Write(string? value) => console.WriteInternal(value);
    }

    /// <summary>
    /// Metodo di scrittura interno chiamato da SdlConsoleWriter.
    /// Processa ogni carattere della stringa attraverso il gestore di sequenze di escape.
    /// </summary>
    /// <param name="value">La stringa da scrivere nel buffer.</param>
    private void WriteInternal(string? value)
    {
        if (string.IsNullOrEmpty(value)) return;
        _isContentDirty = true;
        foreach (var c in value)
        {
            _escapeProcessor.ProcessChar(c);
        }
    }

    /// <summary>
    /// Metodo di scrittura interno per un singolo carattere.
    /// </summary>
    /// <param name="value">Il carattere da scrivere nel buffer.</param>
    private void WriteInternal(char value)
    {
        _isContentDirty = true;
        _escapeProcessor.ProcessChar(value);
    }
    
}