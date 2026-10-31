' LLM MAKEPATCH/APPLYPATCH — caso RIMOZIONE (gap b): il delta registra nel KV
' `gmbasic.patch.removed` i tensori presenti nella BASE ma ASSENTI nel TARGET (qui layer 0 rimosso
' via LLM MODEL REMOVE) e APPLYPATCH li elimina. Prova: rebuilt == target (QUANTERROR rel_l2=0);
' senza la rimozione rebuilt avrebbe tensori in piu' (only>0).
' EXPECT: LLM MAKEPATCH: base=../../samples/Fixtures/models/tiny_moe.gguf target=bin/tgt.gguf patch=bin/delta.gmbpatch tensori_delta=2/13 rimossi=12|LLM APPLYPATCH: base=../../samples/Fixtures/models/tiny_moe.gguf patch=bin/delta.gmbpatch out=bin/rebuilt.gguf|LLM TENSOR QUANTERROR 'bin/rebuilt.gguf' vs 'bin/tgt.gguf': numeric=13 quant=0 only=0 rel_l2=0 max_abs=0
IMPORT LLM
SUB Main()
  LLM MODEL REMOVE "../../samples/Fixtures/models/tiny_moe.gguf", "bin/tgt.gguf", 0
  LLM MAKEPATCH "../../samples/Fixtures/models/tiny_moe.gguf", "bin/tgt.gguf", "bin/delta.gmbpatch"
  LLM APPLYPATCH "../../samples/Fixtures/models/tiny_moe.gguf", "bin/delta.gmbpatch", "bin/rebuilt.gguf"
  LLM TENSOR QUANTERROR "bin/rebuilt.gguf", "bin/tgt.gguf"
END SUB
