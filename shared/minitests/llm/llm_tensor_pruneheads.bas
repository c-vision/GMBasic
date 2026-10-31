' MiniTest: LLM TENSOR PRUNEHEADS — pruning strutturato delle teste di attenzione (4->2).
' q/k/v dimezzate, attn_output dimezzata; head_count aggiornato. F16.
' EXPECT: # LLM TENSOR IR 'bin/heads2.gguf' GGUF arch=qwen3_moe tensori=23|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	f16	2	256x256	131072|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	f16	2	256x128	65536|blk.0.attn_k.weight	ATTENTION	f16	2	256x128	65536|blk.0.attn_v.weight	ATTENTION	f16	2	256x128	65536|blk.0.attn_output.weight	ATTENTION	f16	2	128x256	65536|blk.0.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.0.ffn_gate_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.0.ffn_up_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.0.ffn_down_exps.weight	ROUTED_EXPERT	f16	3	256x128x4	262144|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	f16	2	256x128	65536|blk.1.attn_k.weight	ATTENTION	f16	2	256x128	65536|blk.1.attn_v.weight	ATTENTION	f16	2	256x128	65536|blk.1.attn_output.weight	ATTENTION	f16	2	128x256	65536|blk.1.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.1.ffn_gate_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.1.ffn_up_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.1.ffn_down_exps.weight	ROUTED_EXPERT	f16	3	256x128x4	262144
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM POLICY SET "PRUNE_HEADS", "2"
  LLM TENSOR PRUNEHEADS "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/heads2.gguf"
  LLM TENSOR IR "bin/heads2.gguf"
  LLM POLICY CLEARITEM
END SUB
