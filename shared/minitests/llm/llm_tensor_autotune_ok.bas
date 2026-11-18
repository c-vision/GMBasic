' MiniTest: LLM TENSOR AUTOTUNE (§4.1) — benchmark proiettato O0..O4 + livello raccomandato.
' TAG: heavy — richiede engine MLX/PPL e plugin primmlx; escluso dal default.
' EXPECT: # LLM TENSOR AUTOTUNE 'qwen3_moe' arch= (benchmark proiettato O0..O6)|# livello	budget(bit/peso)	proiettato	compressione	fit_RAM|O0	16.0	0.00 GiB	1.1x	SI|O1	8.5	0.00 GiB	1.9x	SI|O2	4.5	0.00 GiB	3.1x	SI|O3	2.6	0.00 GiB	4.5x	SI|O4	2.1	0.00 GiB	5.2x	SI|O5	1.8	0.00 GiB	6.0x	SI|O6	1.6	0.00 GiB	6.0x	SI|# RACCOMANDATO: O6 (massima compressione entro il budget RAM). APPLY: `LLM POLICY APPLY "autopolicy_O6"`
IMPORT LLM
SUB Main()
  LLM TENSOR AUTOTUNE "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
