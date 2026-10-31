// FILE: Ast.DataTypes.Strings.cs



// ReSharper disable InconsistentNaming

namespace Basic;

// ========================================================================
// --- TIPI DI DATO PER STRINGHE NATIVE (STRINGBUILDER, REGEX) ---
// ========================================================================

/// <summary>
/// Rappresenta il tipo di dato STRINGBUILDER, un builder efficiente per la concatenazione di stringhe.
/// </summary>
[JsonDerivedType(typeof(StringBuilderType), nameof(StringBuilderType))]
public sealed record StringBuilderType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "STRINGBUILDER";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato REGEX, per le espressioni regolari native.
/// </summary>
[JsonDerivedType(typeof(RegexType), nameof(RegexType))]
public sealed record RegexType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "REGEX";
    public override AstNode DeepClone() => this with { };
}

/// <summary>
/// Rappresenta il tipo di dato restituito da REGEX.Match(), contenente il risultato di un singolo match.
/// </summary>
[JsonDerivedType(typeof(RegexMatchType), nameof(RegexMatchType))]
public sealed record RegexMatchType(Token? SourceToken = null)
    : DataType(SourceToken?.Position ?? SourcePosition.Unknown)
{
    public override string ToString() => "REGEXMATCH";
    public override AstNode DeepClone() => this with { };
}
