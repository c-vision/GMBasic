' MiniTest: LLM MERGE con MERGE_METHOD=dare (model soup: drop+rescale del task-vector b-a).
' EXPECT: # LLM TENSOR IR 'work/m_dare.gguf' GGUF arch=qwen3_moe tensori=23|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	f16	2	256x256	131072|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.0.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.0.ffn_gate_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.0.ffn_up_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.0.ffn_down_exps.weight	ROUTED_EXPERT	f16	3	256x128x4	262144|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.1.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.1.ffn_gate_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.1.ffn_up_exps.weight	ROUTED_EXPERT	f16	3	256x256x4	524288|blk.1.ffn_down_exps.weight	ROUTED_EXPERT	f16	3	256x128x4	262144
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  LLM POLICY SET "MERGE_METHOD", "dare"
  LLM MERGE "../../samples/Fixtures/models/tiny_big_moe.gguf", "../../samples/Fixtures/models/tiny_big_moe.gguf", "work/m_dare.gguf"
  LLM TENSOR IR "work/m_dare.gguf"
  LLM POLICY CLEARITEM
END SUB
