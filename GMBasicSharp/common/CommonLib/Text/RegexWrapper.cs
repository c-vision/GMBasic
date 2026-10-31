// FILE: CommonLib.Text.RegexWrapper.cs

using System.Text.RegularExpressions;

/* 
 * RegexWrapper.cs: wrapper runtime per REGEX, incapsula System.Text.RegularExpressions.Regex
 * 
 * CLASSI:
 * - RegexWrapper: wrapper per Regex con pattern, IsMatch, Match, Matches, Replace, Split
 * - RegexMatchWrapper: wrapper per il risultato di un match (Value, Index, Length, Success, Group)
 */

namespace CommonLib.Text;

/// <summary>Wrapper runtime per REGEX. Incapsula System.Text.RegularExpressions.Regex.</summary>
public sealed class RegexWrapper
{
    private string _pattern = "";
    private Regex? _compiled;

    /// <summary>Pattern regex corrente. L'impostazione ricompila la regex.</summary>
    public string Pattern
    {
        get => _pattern;
        set
        {
            _pattern = value;
            _compiled = new Regex(value, RegexOptions.Compiled, TimeSpan.FromSeconds(2));
        }
    }

    private Regex Regex => _compiled ??= new Regex(_pattern, RegexOptions.Compiled, TimeSpan.FromSeconds(2));

    /// <summary>Verifica se l'input corrisponde al pattern regex.</summary>
    /// <param name="input">Stringa da testare.</param>
    /// <returns>True se il pattern trova corrispondenza.</returns>
    public bool IsMatch(string input) => Regex.IsMatch(input);

    /// <summary>Cerca la prima occorrenza del pattern nell'input.</summary>
    /// <param name="input">Stringa in cui cercare.</param>
    /// <returns>Wrapper per il risultato del match.</returns>
    public RegexMatchWrapper Match(string input)
    {
        var m = Regex.Match(input);
        return new RegexMatchWrapper(m);
    }

    /// <summary>Cerca tutte le occorrenze del pattern nell'input.</summary>
    /// <param name="input">Stringa in cui cercare.</param>
    /// <returns>Lista di wrapper per i match trovati.</returns>
    public List<RegexMatchWrapper> Matches(string input)
    {
        var results = new List<RegexMatchWrapper>();
        foreach (Match m in Regex.Matches(input))
            results.Add(new RegexMatchWrapper(m));
        return results;
    }

    /// <summary>Sostituisce le occorrenze del pattern con una stringa di sostituzione.</summary>
    /// <param name="input">Stringa di input.</param>
    /// <param name="replacement">Stringa di sostituzione.</param>
    /// <returns>Stringa con le sostituzioni effettuate.</returns>
    public string Replace(string input, string replacement) => Regex.Replace(input, replacement);

    /// <summary>Suddivide l'input in base al pattern regex.</summary>
    /// <param name="input">Stringa da suddividere.</param>
    /// <returns>Array di stringhe risultanti dalla suddivisione.</returns>
    public string[] Split(string input) => Regex.Split(input);
}

/// <summary>Wrapper runtime per il risultato di REGEX.Match().</summary>
public sealed class RegexMatchWrapper
{
    private readonly Match _match;

    /// <summary>Crea un nuovo wrapper per un Match.</summary>
    /// <param name="match">Il match Regex sottostante.</param>
    public RegexMatchWrapper(Match match) => _match = match;

    /// <summary>Valore del match trovato, o stringa vuota se fallito.</summary>
    public string Value => _match.Success ? _match.Value : "";

    /// <summary>Indice di inizio del match nell'input originale.</summary>
    public long Index => _match.Index;

    /// <summary>Lunghezza del match trovato.</summary>
    public long Length => _match.Length;

    /// <summary>Indica se il match ha avuto successo.</summary>
    public bool Success => _match.Success;

    /// <summary>Restituisce il valore del gruppo catturante per indice.</summary>
    /// <param name="index">Indice del gruppo (0-based).</param>
    /// <returns>Valore del gruppo, o stringa vuota se non presente.</returns>
    public string Group(int index) =>
        index < _match.Groups.Count ? _match.Groups[index].Value : string.Empty;

    /// <summary>Restituisce il valore del gruppo catturante per nome.</summary>
    /// <param name="name">Nome del gruppo.</param>
    /// <returns>Valore del gruppo, o stringa vuota se non presente.</returns>
    public string Group(string name) =>
        _match.Groups[name].Value;
}
