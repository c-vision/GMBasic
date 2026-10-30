// FILE: AI.Eval.Program
// Quality gates per l'implementazione AI di GMBASIC: esegue scenari reali contro il
// provider attivo e produce un report pass/fail.
//
// USO (4 modalita'):
//   1. DOTNET RUN SENZA MODELLO REALE (default, safe):   dotnet run --project tests/AI.Eval
//      -> usa il fixture deterministico (nessuna rete, nessun errore). I casi che
//      richiedono un modello reale verificano solo la pipeline (retrieval/dispatch),
//      non la qualita' della risposta.
//
//   2. OPENROUTER (REALE, richiede OPENROUTER_API_KEY in env):
//        dotnet run --project tests/AI.Eval -- --openrouter <modello>
//      esempi free verificati verdi (6/6 PASS):
//        dotnet run --project tests/AI.Eval -- --openrouter openai/gpt-oss-20b:free
//        dotnet run --project tests/AI.Eval -- --openrouter nvidia/nemotron-3-super-120b-a12b:free
//      Nota: i modelli :free cambiano disponibilita' (alcuni diventano a pagamento o
//      rate-limited upstream con 429). Se OPENROUTER_API_KEY manca -> fallback al fixture.
//
//   3. MODALITA' OLLAMA (REALE) — DA ATTIVARE MANUALMENTE: questo file e' consegnato
//      con il path ollama COMMENTATO perche' senza ollama in esecuzione i casi
//      fallirebbero (provider Unavailable -> fallback fixture -> attese violate).
//      Per attivarla: avviare ollama (`ollama serve`) e rimuovere i commenti dal blocco
//      `--ollama` in SetupProvider qui sotto. Poi:
//        dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 <modello>
//      Esempi provati e verdi:
//        dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 gemma4:31b-mlx
//        dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 qwen2.5-coder:32b
//
//   4. VERIFICA HARNESS:   dotnet run --project tests/AI.Eval -- --fixture
//      (equivalente alla 1 ma esplicito).
using Basic.Agents;
using Basic.Mcp;
using Basic.Providers;
using Basic.AI.Rag;
using AI.Eval;

var dispatcher = new EvalToolDispatcher();
McpRuntimeBridge.Dispatcher = dispatcher;

SetupProvider(args);

// ── M6: Eval Harness unificato — --report [path] → eval_report.json
//     metriche {llm.tokps, rag.ndcg@2, team.acc, credit.blocked}. fixture=true se
//     --fixture o nessun provider reale attivo (mai rete implicita).
var reportIdx = Array.IndexOf(args, "--report");
if (reportIdx >= 0 && reportIdx + 1 < args.Length)
{
    var reportPath = args[reportIdx + 1];
    var fixture = args.Contains("--fixture", StringComparer.OrdinalIgnoreCase)
        || LlmCompleterResolver.Default is null;
    Console.WriteLine($"=== AI EVAL RUN (Eval Harness) → {reportPath} (fixture={fixture}) ===");
    var json = Basic.AI.Eval.EvalHarness.RunAll(reportPath, fixture: fixture);
    Console.WriteLine(json);
    return;
}

Console.WriteLine("=== AI EVAL ===");
Console.WriteLine($"Completer attivo: {ProviderServer.Instance.GetActiveCompletion().ModelName}");
Console.WriteLine();

var cases = EvalCases.All(dispatcher);
int passed = 0, failed = 0;
foreach (var c in cases)
{
    try
    {
        var r = c.Run();
        if (r.Passed) passed++; else failed++;
        Console.WriteLine($"[{(r.Passed ? "PASS" : "FAIL")}] {c.Id}: {c.Description}");
        if (!r.Passed) Console.WriteLine($"      {r.Detail}");
    }
    catch (Exception ex)
    {
        failed++;
        Console.WriteLine($"[FAIL] {c.Id}: exception {ex.Message}");
    }
}

Console.WriteLine();
Console.WriteLine($"Eval: {passed + failed} casi, {passed} passati, {failed} falliti.");
Environment.ExitCode = failed == 0 ? 0 : 1;

static void SetupProvider(string[] args)
{
    if (args.Contains("--fixture", StringComparer.OrdinalIgnoreCase))
    {
        ProviderServer.Instance.Register("completion", "eval-fixture", "fixture");
        return;
    }

    // ── OPENROUTER (REALE) ───────────────────────────────────────────────────
    // Provider OpenAI-compatibile con API key. Legge OPENROUTER_API_KEY dall'env
    // (o dalla config `~/.gmbasic/providers.json`, kind openai). Se la key manca,
    // si cade sul default/fixture: l'eval non deve mai fallire per configurazione.
    var openrouter = Array.IndexOf(args, "--openrouter");
    if (openrouter >= 0 && openrouter + 1 < args.Length)
    {
        var key = Environment.GetEnvironmentVariable("OPENROUTER_API_KEY");
        if (string.IsNullOrWhiteSpace(key))
        {
            Console.WriteLine("Avviso: OPENROUTER_API_KEY non impostata, uso il fixture.");
        }
        else
        {
            var model = args[openrouter + 1];
            ProviderServer.Instance.Register("completion", "eval-openrouter", "openai",
                new Dictionary<string, string>
                {
                    ["baseUrl"] = "https://openrouter.ai/api/v1",
                    ["model"] = model,
                    ["apiKey"] = key,
                    ["timeout"] = "300000",
                    ["httpReferer"] = "https://github.com/GMBASIC",
                    ["xTitle"] = "GMBASIC AI.Eval",
                });
            ProviderServer.Instance.SetActive("completion", "eval-openrouter");
            return;
        }
    }

    // ── MODALITA' OLLAMA (REALE) ──────────────────────────────────────────────
    // ATTIVAZIONE: ollama deve essere in esecuzione (`ollama serve`). Rimuovere i
    // commenti dal blocco qui sotto. Il provider viene registrato con timeout 300s
    // (modelli 32B non-MLX possono superare i 60s di default sul team eval).
    //
    // Ollama espone /v1/chat/completions (formato OpenAI, con tool-calling nativo);
    // il kind "openai" lo usa. Per il protocollo nativo Ollama (/api/chat) usare
    // baseUrl senza /v1 con kind "ollama".
    //
    // var ollama = Array.IndexOf(args, "--ollama");
    // if (ollama >= 0 && ollama + 2 < args.Length)
    // {
    //     var url = args[ollama + 1];
    //     var model = args[ollama + 2];
    //     ProviderServer.Instance.Register("completion", "eval-ollama", "openai",
    //         new Dictionary<string, string> { ["baseUrl"] = url, ["model"] = model, ["timeout"] = "300000" });
    //     ProviderServer.Instance.SetActive("completion", "eval-ollama");
    //     return;
    // }
    //
    // ── FINE MODALITA' OLLAMA ──────────────────────────────────────────────────

    // Default: provider attivo della config. Se nessuno, si va sul fixture per sicurezza.
    try
    {
        _ = ProviderServer.Instance.GetActiveCompletion();
    }
    catch
    {
        ProviderServer.Instance.Register("completion", "eval-fixture", "fixture");
    }
}
