' MiniTest: LLM TENSOR NUMGRAPH positivo su fixture -> parità 4 backend (RISULTATI identici).
' EXPECT: # LLM TENSOR NUMGRAPH '../../samples/Fixtures/models/tiny_moe.gguf' arch=qwen3_moe layers=2 (numeric graph IR §4.9)|# layer	op-chain (weights=f32, activation=f16, accum=f32, out=f16)|global	EMBED(table lookup; memory-bound) -> RMSNORM(f32) -> [per layer] -> FINAL RMSNORM -> LMHEAD(gemm; weights=f32)|L0	RMSNORM(f32) -> QKV/gemm(attn; memory-bound) -> ROPE(f32) -> SOFTMAX(f32) -> ATTN_O(gemm) -> RMSNORM(f32) -> ROUTER(gemm+TOPK+softmax f32) -> MoE(gather experts) -> SWIGLU(geglu) -> DOWN(gemm) -> RESIDUAL(add f32)|L1	RMSNORM(f32) -> QKV/gemm(attn; memory-bound) -> ROPE(f32) -> SOFTMAX(f32) -> ATTN_O(gemm) -> RMSNORM(f32) -> ROUTER(gemm+TOPK+softmax f32) -> MoE(gather experts) -> SWIGLU(geglu) -> DOWN(gemm) -> RESIDUAL(add f32)|# roofline: DECODE memory-bound (GEMV), PREFILL compute-bound (GEMM); attn O(ctx) -> softmax.
IMPORT LLM
SUB Main()
  LLM TENSOR NUMGRAPH "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
