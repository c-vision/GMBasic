' LLM COMPARE — confronto numerico PER-TENSORE tra due GGUF (lineage/frozen-layer, B6).
' Dequantizza i tensori comuni e ne misura la differenza: rel_l2 + avg_diff% normalizzato
' (stile antirez/gguf-tools: avg|Δ|/avg_mag*75). Qui A == B -> tutte le differenze a 0.
' Il 3° argomento è un filtro GLOB opzionale sui nomi tensore ("output.weight" = un solo tensore).
' EXPECT: [output.weight] f16/f16 n=16 rel_l2=0 avg_diff=0.0000% max_abs=0|LLM TENSOR COMPARE '../../samples/Fixtures/models/tiny_moe.gguf' vs '../../samples/Fixtures/models/tiny_moe.gguf': n_a=25 n_b=25 matched=1 only_a=0 only_b=0 mismatch=0 skipped=0 rel_l2=0 avg_diff=0.0000% max_diff=0.0000% @-
IMPORT LLM

SUB Main()
  LLM TENSOR COMPARE "../../samples/Fixtures/models/tiny_moe.gguf", "../../samples/Fixtures/models/tiny_moe.gguf", "output.weight"
END SUB
