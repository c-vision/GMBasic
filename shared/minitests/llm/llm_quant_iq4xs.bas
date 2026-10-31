' MiniTest: LLM QUANT con formato IQ4_XS — cablaggio policy -> encoder IQ4_XS (PUL). Tipo in IR.
' EXPECT: # LLM TENSOR IR 'bin/iq4xs.gguf' GGUF arch=qwen3_moe tensori=23|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	q8_0	2	256x256	69632|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.attn_k.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.attn_v.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.attn_output.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.0.ffn_gate_exps.weight	ROUTED_EXPERT	iq4_xs	3	256x256x4	139264|blk.0.ffn_up_exps.weight	ROUTED_EXPERT	iq4_xs	3	256x256x4	139264|blk.0.ffn_down_exps.weight	ROUTED_EXPERT	iq4_xs	3	256x128x4	69632|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.attn_k.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.attn_v.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.attn_output.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.1.ffn_gate_exps.weight	ROUTED_EXPERT	iq4_xs	3	256x256x4	139264|blk.1.ffn_up_exps.weight	ROUTED_EXPERT	iq4_xs	3	256x256x4	139264|blk.1.ffn_down_exps.weight	ROUTED_EXPERT	iq4_xs	3	256x128x4	69632
IMPORT LLM
SUB Main()
  MKDIR "bin"
  LLM POLICY SET "ROUTED_EXPERT", "iq4xs"
  LLM QUANT "../../samples/Fixtures/models/tiny_big_moe.gguf", "bin/iq4xs.gguf"
  LLM TENSOR IR "bin/iq4xs.gguf"
  LLM POLICY CLEARITEM
END SUB
