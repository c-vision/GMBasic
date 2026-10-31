' MiniTest: LLM TENSOR PRUNEFFN — pruning strutturato della dimensione INTERMEDIA del FFN (256->128).
' Catena: MOEDENSE (MoE->denso) -> PRUNEFFN (taglia l'intermedio). ffn_gate/up ne1, ffn_down ne0.
' EXPECT: # LLM TENSOR IR 'bin/dense_ffn.gguf' GGUF arch=qwen3_moe tensori=21|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	f16	2	256x256	131072|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.0.ffn_gate.weight	FFN	f16	2	256x128	65536|blk.0.ffn_up.weight	FFN	f16	2	256x128	65536|blk.0.ffn_down.weight	FFN	f16	2	128x128	32768|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.1.ffn_gate.weight	FFN	f16	2	256x128	65536|blk.1.ffn_up.weight	FFN	f16	2	256x128	65536|blk.1.ffn_down.weight	FFN	f16	2	128x128	32768
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM TENSOR MOEDENSE "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/dense.gguf"
  LLM POLICY SET "FFN_KEEP", "128"
  LLM TENSOR PRUNEFFN "bin/dense.gguf", "bin/dense_ffn.gguf"
  LLM TENSOR IR "bin/dense_ffn.gguf"
  LLM POLICY CLEARITEM
END SUB
