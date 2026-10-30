// FILE: AI.Eval.EvalCases
// Casi dell'harness eval AI. Con il fixture (default, senza ollama) ogni caso verifica
// solo la pipeline deterministico (dispatcher chiamato / retrieval trova il fatto).
// Con un modello REALE (vedi Program.cs: modalita' --ollama da attivare) i casi
// verificano anche la qualita' end-to-end:
//   - agent-memory-recall: la risposta contiene il fatto salvato nel brain (recall).
//   - agent-no-loop: l'agente termina entro il budget senza girare all'infinito.
using Basic.Agents;
using Basic.Mcp;
using Basic.AI;
using Basic.Providers;

namespace AI.Eval;

public static class EvalCases
{
    public static IReadOnlyList<EvalCase> All(EvalToolDispatcher dispatcher)
    {
        return
        [
            new("agent-tool-calling",
                "l'agente sceglie il tool add_numbers e usa il risultato",
                () =>
                {
                    dispatcher.Calls.Clear();
                    McpToolRegistry.Instance.Register(new McpToolDefinition
                    {
                        Name = "add_numbers",
                        Description = "Somma due numeri interi",
                        QualifiedProcedureName = "add_numbers",
                        InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
                    });
                    AgentServer.Instance.Create("eval-tool");
                    AgentServer.Instance.RegisterTool("eval-tool", "add_numbers");

                    var isFixture = IsFixtureCompleter();
                    var prompt = isFixture
                        ? "tool: add_numbers a=2 b=3. Quanto fa 2+3?"
                        : "Quanto fa 2+3? Rispondi SOLO con il numero, usando il tool add_numbers.";
                    var answer = AgentServer.Instance.Ask("eval-tool", prompt);

                    var toolDispatched = dispatcher.Calls.Contains("add_numbers");
                    return new EvalResult(
                        toolDispatched && (isFixture || answer.Contains("5")),
                        $"answer={answer}, calls=[{string.Join(",", dispatcher.Calls)}]");
                }),

            new("agent-facts",
                "l'agente risponde in modo non vuoto",
                () =>
                {
                    AgentServer.Instance.Create("eval-facts");
                    var answer = AgentServer.Instance.Ask(
                        "eval-facts", "Rispondi in una sola riga: quale pianeta e' il piu' vicino al Sole?");

                    return new EvalResult(!string.IsNullOrWhiteSpace(answer), $"answer={answer}");
                }),

            new("agent-streaming",
                "lo streaming concatena i chunk della risposta",
                () =>
                {
                    AgentServer.Instance.Create("eval-stream");
                    var chunks = new List<string>();
                    AgentServer.Instance.AskStream("eval-stream", "Ripeti la parola CASA.", chunks.Add);
                    Thread.Sleep(2000); // il callback viaggia su Task
                    var joined = string.Concat(chunks);

                    return new EvalResult(joined.Length > 0, $"streamLen={joined.Length}, first='{joined[..Math.Min(40, joined.Length)]}'");
                }),

            new("team-orchestration",
                "il team esegue i membri e registra la conversazione",
                () =>
                {
                    AgentServer.Instance.Create("eval-teama");
                    AgentServer.Instance.Create("eval-teamb");
                    TeamServer.Instance.Create("eval-team");
                    TeamServer.Instance.AddAgent("eval-team", "eval-teama");
                    TeamServer.Instance.AddAgent("eval-team", "eval-teamb");
                    var team = TeamServer.Instance.Get("eval-team")!;

                    var result = TeamOrchestrator.Run(team, "descrivi la coordinazione del team");

                    return new EvalResult(!string.IsNullOrWhiteSpace(result) && team.Conversation.Count == 2,
                        $"resultLen={result.Length}, conv={team.Conversation.Count}");
                }),

            new("agent-memory-recall",
                "l'agente ricorda un fatto salvato nel brain (recall di memoria)",
                () =>
                {
                    MemoryBrain.Add(
                        "L'utente preferisce SQLite per memorizzare i dati. Quale database preferisce l'utente?",
                        MemoryBrain.MemoryKind.Fact, source: "eval");
                    AgentServer.Instance.Create("eval-mem");
                    AgentServer.Instance.Set("eval-mem", "memory", "ON");

                    var answer = AgentServer.Instance.Ask(
                        "eval-mem", "Quale database preferisce l'utente?");

                    var retrieved = MemoryBrain.Search("Quale database preferisce l'utente?")
                        .Contains("SQLite", StringComparison.OrdinalIgnoreCase);
                    var isFixture = IsFixtureCompleter();
                    return new EvalResult(
                        retrieved && (isFixture || answer.Contains("SQLite", StringComparison.OrdinalIgnoreCase)),
                        $"answer={answer}, retrieved={retrieved}");
                }),

            new("agent-no-loop",
                "l'agente termina e non gira in loop su un'azione ripetuta",
                () =>
                {
                    dispatcher.Calls.Clear();
                    McpToolRegistry.Instance.Register(new McpToolDefinition
                    {
                        Name = "add_numbers",
                        Description = "Somma due numeri interi",
                        QualifiedProcedureName = "add_numbers",
                        InputSchema = McpSchemaBuilder.FromParameters([("a", "INTEGER", false), ("b", "INTEGER", false)])
                    });
                    AgentServer.Instance.Create("eval-noloop");
                    AgentServer.Instance.RegisterTool("eval-noloop", "add_numbers");

                    var answer = AgentServer.Instance.Ask(
                        "eval-noloop",
                        "tool: add_numbers a=2 b=3. Ripeti l'azione finche' non ottieni il risultato, poi rispondi SOLO con il numero.");

                    return new EvalResult(
                        !string.IsNullOrWhiteSpace(answer) && dispatcher.Calls.Count < 6,
                        $"answer={answer}, calls=[{string.Join(",", dispatcher.Calls)}]");
                }),
        ];
    }

    private static bool IsFixtureCompleter() =>
        ProviderServer.Instance.GetActiveCompletion().ModelName.Contains("fixture", StringComparison.OrdinalIgnoreCase);
}
