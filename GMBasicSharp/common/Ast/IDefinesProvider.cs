// FILE: Ast.IDefinesProvider.cs

namespace Basic;

/// <summary>Fornisce accesso ai simboli di compilazione (define) usati dal preprocessore del Lexer.</summary>
public interface IDefinesProvider
{
    bool IsDefined(string name);
    void AddDefine(string name, string? value = null);
    void RemoveDefine(string name);
    string? GetDefineValue(string name);
    IReadOnlyDictionary<string, string?> Defines { get; }
}
