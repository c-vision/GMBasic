' ModelQuantize.bas — Quantizzatore RTN self-serve (E1): `LLM MODEL QUANTIZE`.
'
' Quantizza un modello HF/MLX (F32/F16/BF16) in **formato MLX affine** (weight U32 packed +
' scales/biases per-gruppo) con allocazione RTN (round-to-nearest), senza calibrazione. È lo
' stesso formato che i motori già leggono — quindi il risultato è direttamente caricabile con
' `LLM MODEL LOAD` (o con mlx_lm). bits 2..8 (gruppo di default 64).
'
' Sintassi (comando legacy a spazio, §2.0):
'   LLM MODEL QUANTIZE <src_dir>, <dst_dir>, <bits>[, <group>]
'
' Output: una dir con `model.safetensors` (i pesi 2D `.weight` diventano `.weight` U32 +
' `.scales`/`.biases` F16; norm/bias 1D copiati) + config/tokenizer, con `quantization`
' iniettato in config.json.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main
  LET src$ = "../Fixtures/models/tiny_f16"

  ' 1) Ispezione read-only del modello (LLM MODEL INSPECT): dump del descriptor.
  PRINT "== Ispezione del modello (LLM MODEL INSPECT) =="
  LLM MODEL INSPECT src$
  PRINT ""

  ' 2) Quantizzazione RTN self-serve (LLM MODEL QUANTIZE): 4 varianti.
  PRINT "== Quantizzatore RTN self-serve (LLM MODEL QUANTIZE) =="
  PRINT "Sorgente F16: "; src$
  PRINT ""
  PRINT " bits  group  destinazione                 "
  PRINT " ----  -----  -----------------------------"

  ' int8 (gruppo 64 di default)
  LLM MODEL QUANTIZE src$, "work/quant_int8", 8
  PRINT "    8     64  work/quant_int8"

  ' int4, gruppo 64 (default)
  LLM MODEL QUANTIZE src$, "work/quant_int4_g64", 4, 64
  PRINT "    4     64  work/quant_int4_g64"

  ' int4, gruppo 32 (piu' gruppi = piu' precisione, piu' memoria)
  LLM MODEL QUANTIZE src$, "work/quant_int4_g32", 4, 32
  PRINT "    4     32  work/quant_int4_g32"

  ' int2, gruppo 64 (massima compressione, perdita maggiore)
  LLM MODEL QUANTIZE src$, "work/quant_int2_g64", 2, 64
  PRINT "    2     64  work/quant_int2_g64"

  PRINT ""
  PRINT "OK: 4 varianti prodotte (int8, int4/g64, int4/g32, int2/g64)."
END SUB
