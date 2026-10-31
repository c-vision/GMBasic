// FILE: Ast.IParserRegistry.cs

namespace Basic;

/// <summary>Fornisce al Parser l'accesso ai registri delle funzioni built-in (popolati da StdLib).</summary>
public interface IParserRegistry
{
    HashSet<string> AllBuiltinFunctionNames { get; }
    Dictionary<TokenKind, List<string>> BuiltinFunctionProvidersMap { get; }
    Dictionary<string, TokenKind> CommandNameToTokenKindMap { get; }
    Dictionary<string, List<string>> FunctionNameToNamespacesMap { get; }
    IReadOnlyDictionary<string, IEnumerable<ProcedureDefinition>> BuiltinProcedures { get; }
}
