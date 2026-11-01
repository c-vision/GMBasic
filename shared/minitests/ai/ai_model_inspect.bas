' EXPECT: .gmbarch: (generico)|arch=tiny_f16|structures: attention=mha, norm=rmsnorm|recipe=- boost_fmt=-|roles=0 quant=0 codecs=0 modality=text
' MiniTest POSITIVO: LLM MODEL INSPECT su un modello F16 minimo (fixture tiny_f16) — dump read-only
' del descriptor `.gmbarch` risolto (via llmdesc; GMAD eliminato). Path RELATIVO (deterministico).
IMPORT AI WITH ALIAS
IMPORT LLM
SUB Main
  LLM MODEL INSPECT "../../samples/Fixtures/models/tiny_f16"
END SUB
