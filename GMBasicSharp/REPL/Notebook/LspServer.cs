// FILE: REPL/Notebook/LspServer.cs
// [MIGRATO 2026-10-01, §4.10] La logica LSP e le feature semantiche vivono ora nel progetto
// `common/SemanticGraph` (GMBasic.SemanticGraph.Lsp.LspServer), cablate al grafo semantico
// (completion + definition + references + hover + documentSymbol + workspace/symbol).
// Qui resta solo un FACCIATA per compatibilità con i consumatori del REPL/Notebook.

namespace GMBasic.Notebook;

/// <summary>Facade LSP del Notebook: delega a <see cref="SemanticGraph.Lsp.LspServer"/>.
/// Mantiene l'API storica (Handle/ReadMessage/WriteMessage) usata dal Notebook.</summary>
public sealed class LspServer
{
    private readonly SemanticGraph.Lsp.LspServer _inner = new();

    public string? Handle(string rawRequest) => _inner.Handle(rawRequest);

    public static string ReadMessage(Stream input) => SemanticGraph.Lsp.LspServer.ReadMessage(input);

    public static void WriteMessage(Stream output, string json) => SemanticGraph.Lsp.LspServer.WriteMessage(output, json);
}
