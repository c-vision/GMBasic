# Open-Jev sample (MIT)

`test_sample.jsonl` = prime 40 righe di `data/synthetic/test.jsonl` dal repository
[daseinlabs/open-jev](https://github.com/daseinlabs/open-jev) (licenza MIT), usate come
fixture per il runner `tools/runners/test_decide_openjev`.

Formato: `{"context":"…","options":["…"],"label":N}`. Le opzioni sono multi-parola: il runner
usa il **verbalizer A..P** (`DecisionSlots.CreateVerbalizedChoice`), così il readout resta su
token singoli e il valore restituito è il testo dell'opzione scelta.

Esecuzione (modello MLX locale):

```
tools/runners/test_decide_openjev/bin/Debug/net10.0/test_decide_openjev \
  ~/ai/Models/Llama-3.2-1B-Instruct-4bit \
  shared/samples/AiLLM/OpenJev/test_sample.jsonl 20
```

Il dataset completo (400 righe) è scaricabile dal repo upstream; il runner accetta qualsiasi
path JSONL con lo stesso schema.

## Corpus duro (nostro) — `hard_synth_400.jsonl`

Generato da `tools/c/gen_decide_hard_corpus.c` (C99, deterministico: `gen_decide_hard_corpus 400 7`),
nessun dato esterno, licenza nostra. Domande con un minimo di ragionamento (somma/differenza/prodotto,
numero maggiore, ordinamento transitivo, conteggio di lettere, sequenze, giorni della settimana) e
distrattori vicini. Serve a esercitare la calibrazione dove il corpus Open-Jev sintetico (task di copia)
dava acc ~ 1 ed ECE ~ 0. Stesso schema e stesso runner. Misure (verbalizer A..P, n=400):

| Modello | acc | ECE grezza | ECE dopo T | Brier grezzo → calibrato (OOF 2-fold) |
|---|---|---|---|---|
| Llama-3.2-1B-Instruct-4bit | 0,285 | 0,707 | 0,013 | 1,412 → 0,738 |
| Qwen3-8B-4bit | 0,715 | 0,259 | 0,068 | 0,529 → 0,366 |
| Qwen3.6-27B-4bit | 0,840 | 0,096 | 0,045 | 0,233 → 0,202 |
