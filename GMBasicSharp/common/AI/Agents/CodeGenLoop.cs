// FILE: Basic.Agents.CodeGenLoop
// B2.3 — CodeGenLoop: PROMPT→LLM→.bas→LEXER/PARSER/COMPILER→diagnostics→fix

using System.IO;
using Basic.AI.Knowledge;
using Basic.AI.Rag;

namespace Basic.Agents;

public sealed class CodeGenLoop
{
    private readonly Basic.AI.Rag.ILlmCompleter? _completer;
    private readonly int _maxFixAttempts;

    public CodeGenLoop(Basic.AI.Rag.ILlmCompleter? completer = null, int maxFixAttempts = 3)
    {
        _completer = completer ?? LlmCompleterResolver.Default;
        _maxFixAttempts = maxFixAttempts;
    }

    /// <summary>Esegue il loop: prompt → LLM → .bas → compile → fix. Ritorna (ok, path, output, diagnostics).</summary>
    public (bool ok, string path, string output, string diagnostics) Run(string prompt, string outputPath, CodeGenSandbox sandbox = CodeGenSandbox.Auto)
    {
        if (string.IsNullOrWhiteSpace(prompt)) return (false, outputPath, "", "Prompt vuoto");
        if (_completer == null) return (false, outputPath, "", "Nessun provider LLM configurato (AI SET ACTIVE)");

        // 1) Retrieval dal brain vault per contesto (se disponibile)
        string context = "";
        try { context = BrainVault.Query(prompt, 5); } catch { }

        string system = "Sei un generatore di codice GMBasic. Produci SOLO codice BASIC valido, "
                      + "usando le firme esatte dal contesto. Niente spiegazioni fuori dal codice. "
                      + "Contesto GMBasic (brain vault):\n" + context;

        string code = "";
        string diagnostics = "";
        for (int attempt = 0; attempt <= _maxFixAttempts; attempt++)
        {
            var userPrompt = attempt == 0 ? prompt : $"Il codice precedente ha dato errore di compilazione:\n{diagnostics}\n\nRichiesta originale: {prompt}\n\nCorreggi il codice BASIC e rispondi SOLO con il codice corretto.";
            try
            {
                code = _completer.Complete(system, userPrompt);
            }
            catch (Exception ex)
            {
                return (false, outputPath, "", $"LLM error: {ex.Message}");
            }

            // Estrai il codice dal markdown se il LLM ha messo ```basic ... ```
            code = ExtractCode(code);

            var writeRes = CodeGenTools.ToolWrite(outputPath, code, sandbox);
            if (writeRes.StartsWith("Error")) return (false, outputPath, "", writeRes);

            var (okC, diagC) = CodeGenTools.ToolCompile(outputPath);
            if (okC)
            {
                var (okR, outR, diagR) = CodeGenTools.ToolRun(outputPath);
                //Anche se il run fallisce, il compile è ok → consideriamo successo di generazione (il run può fallire per logica)
                return (true, outputPath, outR, diagC + (okR ? "" : $"\nRun: {diagR}"));
            }
            diagnostics = diagC;
            // Continua il loop per fix
        }
        return (false, outputPath, code, $"Fix esauriti dopo {_maxFixAttempts} tentativi. Ultimo diagnostics: {diagnostics}");
    }

    private static string ExtractCode(string raw)
    {
        if (string.IsNullOrWhiteSpace(raw)) return raw;
        // Se contiene ```basic ... ``` o ``` ... ```, estrai il contenuto
        var s = raw.Trim();
        var start = s.IndexOf("```", StringComparison.Ordinal);
        if (start >= 0)
        {
            var end = s.IndexOf("```", start + 3, StringComparison.Ordinal);
            if (end > start)
            {
                var code = s.Substring(start + 3, end - start - 3).Trim();
                // Rimuovi la prima riga se è "basic" o "bas"
                var firstLineEnd = code.IndexOf('\n');
                if (firstLineEnd > 0)
                {
                    var first = code.Substring(0, firstLineEnd).Trim().ToLowerInvariant();
                    if (first == "basic" || first == "bas" || first == "gmbasic")
                        code = code.Substring(firstLineEnd + 1).Trim();
                }
                return code;
            }
        }
        return s;
    }
}
