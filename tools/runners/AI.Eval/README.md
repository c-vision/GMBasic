# AI.Eval — Quality gates per l'implementazione AI

L'harness esegue scenari reali contro il provider attivo e produce un report `PASS`/`FAIL`.
È il quality gate end-to-end del second brain: non testa le singole unità (lo fa `AI.Tests`),
ma la **catena completa** (modello → agente → tool → memoria) contro un LLM vero.

## Casi coperti

| Id | Cosa verifica | Con fixture (senza ollama) | Con modello reale |
|----|---------------|----------------------------|-------------------|
| `agent-tool-calling` | l'agente sceglie il tool `add_numbers` e usa il risultato | dispatch del tool avvenuto | dispatch + risposta contiene il risultato |
| `agent-facts` | l'agente risponde in modo non vuoto | risposta non vuota | idem |
| `agent-streaming` | lo streaming concatena i chunk | concatenazione non vuota | idem |
| `team-orchestration` | il team esegue i membri e registra la conversazione | esecuzione + conversazione | idem |
| `agent-memory-recall` | recall: la risposta contiene un fatto salvato nel brain | retrieval del fatto trovato | retrieval + risposta contiene il fatto |
| `agent-no-loop` | loop detection: l'agente termina entro il budget senza ripetere | termine entro budget | idem (loop detection attivo) |

In modalità fixture (default, senza ollama) i casi verificano **solo la pipeline deterministica**
(dispatcher chiamato / retrieval che trova il fatto). La qualità end-to-end della risposta
viene verificata solo con un modello reale.

## Modalità

### 1. Fixture (default, sicuro — nessuna rete)

```bash
dotnet run --project tests/AI.Eval
# oppure esplicitamente:
dotnet run --project tests/AI.Eval -- --fixture
```

Usa `FixtureLlmCompleter`/`FixtureToolCallingCompleter`: deterministico, nessun errore,
nessun download. Il comando di default non dipende da ollama.

### 2. Modello reale via OpenRouter (API key in env)

Attivo per default (nessun commento da rimuovere). Richiede la key nell'ambiente:

```bash
export OPENROUTER_API_KEY="sk-or-v1-..."
dotnet run --project tests/AI.Eval -- --openrouter <modello>
```

Modelli free verificati verdi (6/6 PASS):

```bash
dotnet run --project tests/AI.Eval -- --openrouter openai/gpt-oss-20b:free
dotnet run --project tests/AI.Eval -- --openrouter nvidia/nemotron-3-super-120b-a12b:free
```

Nota: i modelli `:free` di OpenRouter cambiano disponibilità nel tempo — alcuni diventano
a pagamento (HTTP 404 con slug suggerito) o rate-limited upstream (HTTP 429). Se
`OPENROUTER_API_KEY` non è impostata, l'eval cade sul fixture senza errori.

### 3. Modello reale via Ollama (da attivare manualmente)

Il path `--ollama` è **commentato** in `Program.cs`: senza ollama in esecuzione i casi
fallirebbero (provider `Unavailable` → fallback fixture → attese violate).

Per attivarla:

1. Avviare ollama: `ollama serve`
2. In `tests/AI.Eval/Program.cs`, rimuovere i commenti dal blocco `── MODALITA' OLLAMA (REALE) ──`
3. Lanciare:

```bash
dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 <modello>
```

Esempi verificati verdi (6/6 PASS):

```bash
dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 gemma4:31b-mlx
dotnet run --project tests/AI.Eval -- --ollama http://localhost:11434/v1 qwen2.5-coder:32b
```

Note sul provider:
- Ollama espone `/v1/chat/completions` (formato OpenAI, tool-calling nativo) → kind `openai`.
  Per il protocollo nativo `/api/chat` usare baseUrl **senza** `/v1` con kind `ollama`.
- Il provider viene registrato con `timeout = 300000` (5 min): i modelli 32B non-MLX
  possono superare i 60 secondi di default sul caso `team-orchestration`.

## Come attivare il second brain (per i casi di memoria)

- `agent-memory-recall` salva un fatto nel brain (`~/.gmbasic/brain.db`) e interroga
  un agente con `memory ON`. Il retrieval inietta il fatto nel system prompt.
- Senza un provider di embedding attivo il retrieval usa il fallback lessicale
  (`Text.Contains(query)`); con un embedding provider usa la similarità coseno.
- Per abilitare la memoria su un agente: `AI AGENT SET <nome>, memory, ON`.

## Exit code

- `0` = tutti i casi passano
- `1` = almeno un caso fallisce (o lancia un'eccezione)

## Struttura

- `Program.cs` — runner: setup provider (fixture/ollama/config), esegue i casi, report.
- `EvalCases.cs` — definizione dei casi (setup + prompt + attese).
- `EvalCase.cs` — tipo `EvalCase`/`EvalResult`.
- `EvalToolDispatcher.cs` — dispatcher deterministico (registra le chiamate, risultati noti).

### M6 — Eval Harness unificato (metriche)

Scrive `eval_report.json` con le 4 famiglie di metriche confrontabili tra backend/sessioni:

```bash
dotnet run --project tests/AI.Eval -- --report tools/out/eval_report.json
# fixture deterministico (mai rete) di default; con modello reale usa il provider attivo
# → eval_report.json { total:{cases,passed,failed}, suites:[
#     llm.tokps/rag.ndcg@2/team.acc/credit.blocked ] }
```

- `llm.tokps`: throughput (stima ~1 tok/4 char, cronometrato)
- `rag.ndcg@2`: ranking del retrieval (query → doc ideali, NDCG binario)
- `team.acc`: accuracy del team (risposta contiene il token atteso)
- `credit.blocked`: il credit tracker porta il provider a 0 crediti → Blocked

Implementazione: `common/AI/Eval/EvalHarness.cs` + `tests/AI.Tests/Eval/EvalHarnessTests.cs` (2/2).
