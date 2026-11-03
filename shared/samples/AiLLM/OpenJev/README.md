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
