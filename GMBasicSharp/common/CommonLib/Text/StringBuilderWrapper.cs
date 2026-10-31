// FILE: CommonLib.Text.StringBuilderWrapper.cs

/* 
 * StringBuilderWrapper.cs: wrapper runtime per StringBuilder per concatenazione efficiente di stringhe
 * 
 * FUNZIONI:
 * - Length(): lunghezza corrente del buffer
 * - Capacity(): capacità corrente del buffer
 * - Append(value): aggiunge un valore in coda
 * - AppendLine(value): aggiunge una riga in coda
 * - AppendFormat(format, args): aggiunge testo formattato
 * - Insert(index, value): inserisce un valore a una posizione
 * - Replace(oldStr, newStr): sostituisce testo
 * - Remove(startIndex, length): rimuove un intervallo
 * - Clear(): svuota il buffer
 * - ToString(): converte in stringa
 */

namespace CommonLib.Text;

/// <summary>Wrapper runtime per StringBuilder, per concatenazione efficiente di stringhe.</summary>
public sealed class StringBuilderWrapper
{
    internal System.Text.StringBuilder Inner { get; } = new();

    /// <summary>Restituisce la lunghezza corrente del buffer.</summary>
    /// <returns>Numero di caratteri nel buffer.</returns>
    public long Length() => Inner.Length;

    /// <summary>Restituisce la capacità corrente del buffer.</summary>
    /// <returns>Numero di caratteri che il buffer può contenere senza riallocazione.</returns>
    public long Capacity() => Inner.Capacity;

    /// <summary>Aggiunge la rappresentazione testuale di un valore in coda al buffer.</summary>
    /// <param name="value">Il valore da aggiungere (può essere null).</param>
    public void Append(object? value) => Inner.Append(value);

    /// <summary>Aggiunge una riga di testo in coda al buffer.</summary>
    /// <param name="value">Il valore da aggiungere come riga (può essere null).</param>
    public void AppendLine(object? value) => Inner.AppendLine(value?.ToString());

    /// <summary>Aggiunge testo formattato in coda al buffer.</summary>
    /// <param name="format">Stringa di formato composito.</param>
    /// <param name="args">Array di oggetti da formattare.</param>
    public void AppendFormat(string format, params object?[] args) => Inner.AppendFormat(format, args);

    /// <summary>Inserisce un valore a una posizione specifica nel buffer.</summary>
    /// <param name="index">Posizione di inserimento (0-based).</param>
    /// <param name="value">Il valore da inserire.</param>
    public void Insert(int index, object? value) => Inner.Insert(index, value?.ToString());

    /// <summary>Sostituisce tutte le occorrenze di una stringa con un'altra nel buffer.</summary>
    /// <param name="oldStr">Stringa da sostituire.</param>
    /// <param name="newStr">Stringa di sostituzione.</param>
    public void Replace(string oldStr, string newStr) => Inner.Replace(oldStr, newStr);

    /// <summary>Rimuove un intervallo di caratteri dal buffer.</summary>
    /// <param name="startIndex">Posizione di inizio (0-based).</param>
    /// <param name="length">Numero di caratteri da rimuovere.</param>
    public void Remove(int startIndex, int length) => Inner.Remove(startIndex, length);

    /// <summary>Rimuove tutti i caratteri dal buffer.</summary>
    public void Clear() => Inner.Clear();

    /// <summary>Converte il contenuto del buffer in stringa.</summary>
    /// <returns>La stringa completa del buffer.</returns>
    public override string ToString() => Inner.ToString();
}
