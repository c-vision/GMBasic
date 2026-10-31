' MiniTest: LLM TENSOR MOEDENSE — fondi gli esperti in UN FFN denso (media), elimina il router.
' ffn_gate/up/down_exps -> ffn_gate/up/down.weight (denso); ffn_gate_inp eliminato. F16.
' EXPECT: # LLM TENSOR IR 'bin/dense.gguf' GGUF arch=qwen3_moe tensori=21|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	f16	2	256x256	131072|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.0.ffn_gate.weight	FFN	f16	2	256x256	131072|blk.0.ffn_up.weight	FFN	f16	2	256x256	131072|blk.0.ffn_down.weight	FFN	f16	2	256x128	65536|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.1.ffn_gate.weight	FFN	f16	2	256x256	131072|blk.1.ffn_up.weight	FFN	f16	2	256x256	131072|blk.1.ffn_down.weight	FFN	f16	2	256x128	65536
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM TENSOR MOEDENSE "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/dense.gguf"
  LLM TENSOR IR "bin/dense.gguf"
END SUB
