# AI.Eval — Quality gates for the AI implementation

The harness runs real scenarios against the active provider and produces a `PASS`/`FAIL` report.
It is the end-to-end quality gate of the second brain: it does not test the single units (that is what `AI.Tests` does),
but the **whole chain** (model → agent → tool → memory) against a real LLM.

## Covered cases

| Id | What it verifies | With fixture (without ollama) | With a real model |
|----|---------------|----------------------------|-------------------|
| `agent-tool-calling` | the agent picks the `add_numbers` tool and uses the result | tool dispatch happened | dispatch + answer contains the result |
| `agent-facts` | the agent answers in a non-empty way | non-empty answer | idem |
| `agent-streaming` | streaming concatenates the chunks | non-empty concatenation | idem |
| `team-orchestration` | the team runs the members and records the conversation | execution + conversation | idem |
| `agent-memory-recall` | recall: the answer contains a fact saved in the brain | retrieval of the fact found | retrieval + answer contains the fact |
| `agent-no-loop` | loop detection: the agent terminates within the budget without repeating | termination within budget | idem (loop detection active) |

In fixture mode (default, without ollama) the cases verify **only the deterministic pipeline**
(dispatcher called / retrieval that finds the fact). The end-to-end quality of the answer
is verified only with a real model.

## Modes

### 1. Fixture (default, safe — no network)

```bash
dotnet run --project tests/AI.Eval
# or explicitly:
dotnet run --project tests/AI.Eval -- --fixture
```

Uses `FixtureLlmCompleter`/`FixtureToolCallingCompleter`: deterministic, no errors,
no download. The default command does not depend on ollama.

### 2. Real model via OpenRouter (API key in env)

Active by default (no comment to remove). It requires the key in the environment:

```bash
export OPENROUTER_API_KEY="sk-or-v1-..."
dotnet run --project tests/AI.Eval -- --openrouter <modello>
```

Free models verified green (6/6 PASS):

```bash
dotnet run --project tests/AI.Eval -- --openrouter openai/gpt-oss-20b:free
dotnet run --project tests/AI.Eval -- --openrouter nvidia/nemotron-3-super-120b-a12b:free
```

Note: the `:free` models of OpenRouter change availability over time — some become
paid (HTTP 404 with a suggested slug) or rate-limited upstream (HTTP 429). If
`OPENROUTER_API_KEY` is not set, the eval falls back to the fixture with no errors.

### 3. Real model via Ollama (to be enabled manually)

The `--ollama` path is **commented out** in `Program.cs`: without ollama running the cases
would fail (provider `Unavailable` → fixture fallback → violated expectations).

To enable it:

1. Start ollama: `ollama serve`
2. In `tests/AI.Eval/Program.cs`, remove the comments from the `── MODALITA' OLLAMA (REALE) ──` block
3. Run:

```bash
dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 <modello>
```

Examples verified green (6/6 PASS):

```bash
dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 gemma4:31b-mlx
dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 qwen2.5-coder:32b
```

Notes on the provider:
- Ollama exposes `/v1/chat/completions` (OpenAI format, native tool-calling) → kind `openai`.
  For the native `/api/chat` protocol use baseUrl **without** `/v1` with kind `ollama`.
- The provider is registered with `timeout = 300000` (5 min): non-MLX 32B models
  can exceed the 60 second default on the `team-orchestration` case.

## How to enable the second brain (for the memory cases)

- `agent-memory-recall` saves a fact in the brain (`~/.gmbasic/brain.db`) and queries
  an agent with `memory ON`. The retrieval injects the fact into the system prompt.
- Without an active embedding provider the retrieval uses the lexical fallback
  (`Text.Contains(query)`); with an embedding provider it uses cosine similarity.
- To enable memory on an agent: `AI AGENT SET <nome>, memory, ON`.

## Exit code

- `0` = all cases pass
- `1` = at least one case fails (or throws an exception)

## Structure

- `Program.cs` — runner: provider setup (fixture/ollama/config), runs the cases, report.
- `EvalCases.cs` — case definition (setup + prompt + expectations).
- `EvalCase.cs` — `EvalCase`/`EvalResult` type.
- `EvalToolDispatcher.cs` — deterministic dispatcher (records the calls, known results).

### M6 — Unified Eval Harness (metrics)

Writes `eval_report.json` with the 4 families of metrics comparable across backends/sessions:

```bash
dotnet run --project tests/AI.Eval -- --report tools/out/eval_report.json
# deterministic fixture (never network) by default; with a real model it uses the active provider
# → eval_report.json { total:{cases,passed,failed}, suites:[
#     llm.tokps/rag.ndcg@2/team.acc/credit.blocked ] }
```

- `llm.tokps`: throughput (estimate ~1 tok/4 char, timed)
- `rag.ndcg@2`: retrieval ranking (query → ideal docs, binary NDCG)
- `team.acc`: team accuracy (answer contains the expected token)
- `credit.blocked`: the credit tracker brings the provider to 0 credits → Blocked

Implementation: `common/AI/Eval/EvalHarness.cs` + `tests/AI.Tests/Eval/EvalHarnessTests.cs` (2/2).
