' LLM MAKEPATCH / APPLYPATCH — delta binario applicabile (B5).
' target = DERIVE con ABLATE (azzera i tensori ffn_down); MAKEPATCH crea il delta; APPLYPATCH
' ricostruisce il target da base + delta. Verifica rebuilt == target (QUANTERROR rel_l2=0).
' EXPECT: LLM DERIVE '../../samples/Fixtures/models/tiny_ablate.gmbderive': base=../../samples/Fixtures/models/tiny_moe.gguf ops=1 out=bin/target.gguf|LLM REGISTRY ADD: tag=target path=bin/target.gguf|LLM REGISTRY TAG 'target': metrics=size=2784;recipe=tiny_ablate.gmbderive;ops=1;base=tiny_moe.gguf|LLM MAKEPATCH: base=../../samples/Fixtures/models/tiny_moe.gguf target=bin/target.gguf patch=bin/delta.gmbpatch tensori_delta=2/25 rimossi=0|LLM APPLYPATCH: base=../../samples/Fixtures/models/tiny_moe.gguf patch=bin/delta.gmbpatch out=bin/rebuilt.gguf|LLM TENSOR QUANTERROR 'bin/rebuilt.gguf' vs 'bin/target.gguf': numeric=25 quant=0 only=0 rel_l2=0 max_abs=0
IMPORT LLM
SUB Main()
  LLM DERIVE "../../samples/Fixtures/models/tiny_ablate.gmbderive"
  LLM MAKEPATCH "../../samples/Fixtures/models/tiny_moe.gguf", "bin/target.gguf", "bin/delta.gmbpatch"
  LLM APPLYPATCH "../../samples/Fixtures/models/tiny_moe.gguf", "bin/delta.gmbpatch", "bin/rebuilt.gguf"
  LLM TENSOR QUANTERROR "bin/rebuilt.gguf", "bin/target.gguf"
END SUB
