' LLM DERIVE — pipeline di derivazione riproducibile da una ricetta .gmbderive (B5).
' BASE/OUT: base relativa alla ricetta, out relativo alla CWD. Verifica l'artefatto derivato.
' EXPECT: LLM DERIVE '../../samples/Fixtures/models/tiny.gmbderive': base=../../samples/Fixtures/models/tiny_moe.gguf ops=1 out=work/derived.gguf|LLM REGISTRY ADD: tag=derived path=work/derived.gguf|LLM REGISTRY TAG 'derived': metrics=size=2784;recipe=tiny.gmbderive;ops=1;base=tiny_moe.gguf|LLM TENSOR QUANTERROR 'work/derived.gguf' vs 'work/derived.gguf': numeric=25 quant=0 only=0 rel_l2=0 max_abs=0
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  LLM DERIVE "../../samples/Fixtures/models/tiny.gmbderive"
  LLM TENSOR QUANTERROR "work/derived.gguf", "work/derived.gguf"
END SUB
