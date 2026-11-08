// FILE: Basic.SQLite.Vector.LlmCompleterResolver
namespace Basic.AI.Rag;

/// <summary>Resolves the active LLM completer.</summary>
public static class LlmCompleterResolver
{
    /// <summary>Active completion provider (set by runtime/Interpreter).</summary>
    public static ILlmCompleter? Default { get; set; }

    /// <summary>Test hook — reset to defaults.</summary>
    internal static void ResetForTests() => Default = null;
}
