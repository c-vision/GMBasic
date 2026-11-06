# libgmbwsp (FFI) — Weight-Space Programming

Namespace BASIC `WSP` (`shared/include/wsp.inc`): aprire un modello (safetensors), sezionarlo (`gmb.wsp@1`), analizzare i
circuiti OV/QK, modificare i pesi su un **overlay** con journal/undo/recipe/patch, derivare modelli (layer, gruppi kv, neuroni),
verificare e tracciare la provenienza, pilotare una sessione agente a verbi con budget e replay.

Sorgenti venduti da `GMBasic/gmbllm/{src,include}/llmwsp_*` (sede canonica lì; `build.sh` li ricopia). Binding: `src/wsp_ffi.c`.

```
bash shared/libs/wsp/build.sh
```

Handle: massimo 8 modelli vivi, uso sequenziale, non thread-safe. Errori: ritorno -1 e `WSP.LASTERROR(h)`. Dettagli del formato e
dei limiti: `GMBasic/gmbllm/docs/WSP.md`.
