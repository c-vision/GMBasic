' MiniTest: LLM TENSOR INSPECT positivo su fixture .gmbllm -> parità 4 backend (RISULTATI identici).
' Read-only (nessuna scrittura in work/). Fixture: samples/Fixtures/models/tiny_moe.gmbllm.
' EXPECT: # LLM TENSOR INSPECT '../../samples/Fixtures/models/tiny_moe.gmbllm' (artefatto .gmbllm)|artifact=gmbllm arch=- layers=2|attention=- moe=- modality=- speculation=NONE|memory_plan: weights_bytes=656 kv_per_token_bytes=32|exact_capacity: kv_bound=3154116587|kernels: matmul=affine_qmm (+1 definizioni)|layer_ops: 2 layer con ops
IMPORT LLM
SUB Main()
  LLM TENSOR INSPECT "../../samples/Fixtures/models/tiny_moe.gmbllm"
END SUB
