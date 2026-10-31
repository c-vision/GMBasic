using CommonLib.Consoles;
// FILE: Interpreter.Consoles.SdlConsole.Read.cs

namespace CommonGraphics.Consoles;

/// <summary>
/// Contiene i metodi di input per la classe parziale SdlConsole.
/// Questa implementazione delega le operazioni di lettura bloccanti al KeyboardService
/// per consentire l'intercettazione centralizzata degli hook di tastiera.
/// </summary>
public sealed partial class SdlConsole
{
    /// <summary>
    /// Legge la prossima riga di caratteri. La logica viene delegata al KeyboardService
    /// per consentire l'intercettazione degli hook di tastiera (ON KEY) anche durante l'input.
    /// </summary>
    /// <returns>La riga di testo inserita dall'utente. Non restituisce mai null.</returns>
    public string ReadLine()
    {
        // Delega l'intera operazione. Il KeyboardService gestirà l'eco dei caratteri
        // e il backspace direttamente su questa istanza di console, che viene passata
        // come argomento nel metodo ReadLineAsync.
        return _keyboardService.ReadLineAsync(this, "", null).GetAwaiter().GetResult() ?? "";
    }

    /// <summary>
    /// Ottiene il prossimo carattere o tasto funzione premuto dall'utente.
    /// Questa è un'operazione bloccante che attende l'input.
    /// La logica viene delegata al KeyboardService per consentire l'intercettazione degli hook.
    /// </summary>
    /// <param name="intercept">Se true, il tasto non viene visualizzato; se false, viene visualizzato.</param>
    /// <returns>Un oggetto TerminalKeyInfo che descrive il tasto premuto.</returns>
    public TerminalKeyInfo ReadKey(bool intercept)
    {
        // Delega la lettura al servizio, che gestisce gli hook.
        var keyInfo = _keyboardService.ReadKeyAsync().GetAwaiter().GetResult();
        
        // Se l'input non deve essere nascosto, scrive il carattere sulla console (eco).
        if (!intercept)
        {
            if (!char.IsControl(keyInfo.KeyChar))
            {
                Out.Write(keyInfo.KeyChar.ToString());
            }
        }
        
        return new TerminalKeyInfo(keyInfo.KeyChar, (int)keyInfo.Key, 
            keyInfo.Modifiers.HasFlag(ConsoleModifiers.Shift), 
            keyInfo.Modifiers.HasFlag(ConsoleModifiers.Alt), 
            keyInfo.Modifiers.HasFlag(ConsoleModifiers.Control));
    }

    /// <summary>
    /// Legge il prossimo carattere dalla coda di input in modo bloccante.
    /// </summary>
    /// <returns>Il codice del carattere letto, o -1 se si verifica un errore.</returns>
    public int Read()
    {
        // Chiama ReadKey intercettando l'input per non visualizzarlo.
        var keyInfo = ReadKey(true); 
        return keyInfo.KeyChar == '\0' ? -1 : keyInfo.KeyChar;
    }

    /// <summary>
    /// Ottiene un valore che indica se è disponibile un tasto nella coda di input non bloccante.
    /// </summary>
    public bool KeyAvailable => _keyboardService.TryReadKey(false).HasValue;
}