' MiniTest: LLM TENSOR CED positivo (elimination plan + gate) su fixture -> parità 4 backend.
' EXPECT backfillato dal leg IL.
' EXPECT: # LLM TENSOR CED '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe: tensori=25 (scan campionato) all-zero=0|elimination plan (ESATTA): 0 tensori / 0 layer, 0 byte su 656 = 0.00% del modello|GATE/kill-switch: OFF — nessun candidato (default OFF = report-only; ON abilita l'applicazione a compile/prune)|# LLM TENSOR CED '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe: tensori=25 (scan campionato) all-zero=0|elimination plan (ESATTA): 0 tensori / 0 layer, 0 byte su 656 = 0.00% del modello|GATE/kill-switch: ON — eliminazione ATTIVA (applica al .gmbllm/prune) (default OFF = report-only; ON abilita l'applicazione a compile/prune)
IMPORT LLM
SUB Main()
  LLM TENSOR CED "../../samples/Fixtures/models/tiny_moe.gguf"
  LLM TENSOR CED "../../samples/Fixtures/models/tiny_moe.gguf", "on"
END SUB
