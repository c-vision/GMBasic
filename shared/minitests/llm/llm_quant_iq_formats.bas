' MiniTest: LLM QUANT con formati IQ a basso bpw (iq1_s/iq3_s) — cablaggio policy -> encoder IQ.
' Verifica che il formato richiesto via POLICY ROUTED_EXPERT sia APPLICATO agli esperti (tipo in IR).
' EXPECT: # LLM TENSOR IR 'work/iq1s.gguf' GGUF arch=qwen3_moe tensori=23|# name	role	type	ndim	dims	bytes|token_embd.weight	EMBEDDING	f16	2	256x256	131072|output.weight	OUTPUT	q8_0	2	256x256	69632|output_norm.weight	NORM	f32	1	256	1024|blk.0.attn_norm.weight	NORM	f32	1	256	1024|blk.0.ffn_norm.weight	NORM	f32	1	256	1024|blk.0.attn_q.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.attn_k.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.attn_v.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.attn_output.weight	ATTENTION	q8_0	2	256x256	69632|blk.0.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.0.ffn_gate_exps.weight	ROUTED_EXPERT	iq1_s	3	256x256x4	51200|blk.0.ffn_up_exps.weight	ROUTED_EXPERT	iq1_s	3	256x256x4	51200|blk.0.ffn_down_exps.weight	ROUTED_EXPERT	iq1_s	3	256x128x4	25600|blk.1.attn_norm.weight	NORM	f32	1	256	1024|blk.1.ffn_norm.weight	NORM	f32	1	256	1024|blk.1.attn_q.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.attn_k.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.attn_v.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.attn_output.weight	ATTENTION	q8_0	2	256x256	69632|blk.1.ffn_gate_inp.weight	ROUTER	f16	2	4x256	2048|blk.1.ffn_gate_exps.weight	ROUTED_EXPERT	iq1_s	3	256x256x4	51200|blk.1.ffn_up_exps.weight	ROUTED_EXPERT	iq1_s	3	256x256x4	51200|blk.1.ffn_down_exps.weight	ROUTED_EXPERT	iq1_s	3	256x128x4	25600
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  LLM POLICY SET "ROUTED_EXPERT", "iq1s"
  LLM QUANT "../../samples/Fixtures/models/tiny_big_moe.gguf", "work/iq1s.gguf"
  LLM TENSOR IR "work/iq1s.gguf"
  LLM POLICY CLEARITEM
END SUB
