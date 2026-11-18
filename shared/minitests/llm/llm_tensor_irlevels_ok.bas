' MiniTest: LLM TENSOR IRLEVELS positivo (gerarchia L0-L5 model-aware) -> parità 4 backend.
' Il report è deterministico e indipendente dalla piattaforma (la riga L5 elenca i lowering
' supportati, non quelli compilati): l'oracolo è l'output del report.
' EXPECT: # LLM TENSOR IRLEVELS '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe (gerarchia IR L0-L5, model-aware)|L0 source   : HF/MLX/safetensors (pre-conversione)|L1 container: GGUF v3, 25 tensori, 10 KV, 0.00 MB|L2 model IR : arch=qwen3_moe (ruoli=13) layers=2 modality=text speculation=NONE|L3 primitive: attention=0 moe=8 ffn=0 norm=7 other=10|L4 execution: fasi PREFILL | PREFILL_KV | DECODE; layer_ops=2|L5 lowering : MLX(metal) | C(c99) | VM(C)
IMPORT LLM
SUB Main()
  LLM TENSOR IRLEVELS "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
