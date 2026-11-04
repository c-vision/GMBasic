' MiniTest: LLM PRUNE con criterio quality-guided "imp" (uso x norma-router-centrata). Esperti 4->2.
' EXPECT: # LLM TENSOR IR 'work/pruned_imp.gguf' GGUF arch=qwen3_moe tensori=23|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	f16	2	256x256	131072|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.0.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.0.ffn_gate_inp.weight	ROUTER	f16	2	4x2	16|blk.0.ffn_gate_exps.weight	ROUTED_EXPERT	f16	3	2x256x4	4096|blk.0.ffn_up_exps.weight	ROUTED_EXPERT	f16	3	2x256x4	4096|blk.0.ffn_down_exps.weight	ROUTED_EXPERT	f16	3	2x128x4	2048|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_k.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_v.weight	ATTENTION	f16	2	256x256	131072|blk.1.attn_output.weight	ATTENTION	f16	2	256x256	131072|blk.1.ffn_gate_inp.weight	ROUTER	f16	2	4x2	16|blk.1.ffn_gate_exps.weight	ROUTED_EXPERT	f16	3	2x256x4	4096|blk.1.ffn_up_exps.weight	ROUTED_EXPERT	f16	3	2x256x4	4096|blk.1.ffn_down_exps.weight	ROUTED_EXPERT	f16	3	2x128x4	2048
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  LLM POLICY CLEARITEM
  LLM POLICY SET "PRUNE_EXPERT", "imp"
  LLM PRUNE "../../samples/Fixtures/models/tiny_big_moe.gguf", "work/pruned_imp.gguf", 2
  LLM TENSOR IR "work/pruned_imp.gguf"
END SUB
