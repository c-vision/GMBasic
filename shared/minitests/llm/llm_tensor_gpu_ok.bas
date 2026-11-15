' MiniTest: LLM TENSOR GPU positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR GPU '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe|kernel_specialization: attention=gqa moe=moe_sorted_gather matmul=affine_qmm norm=rmsnorm|routing: experts=2 activated=1 spread=0.500 -> bassa divergenza|gpu_cost_model: layers=2 weights=656 bytes (0.00 GiB)
IMPORT LLM
SUB Main()
  LLM TENSOR GPU "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
