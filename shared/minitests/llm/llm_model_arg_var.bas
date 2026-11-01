' MiniTest: LLM MODEL <verbo> con argomento PATH in VARIABILE stringa (B$) — deve passare il
' VALORE a runtime, non il letterale "B$" (fix parser C: gmb_p_aimodel_term; pari al C#).
' EXPECT: .gmbarch: (generico)|arch=tiny_f16|structures: attention=mha, norm=rmsnorm|recipe=- boost_fmt=-|roles=0 quant=0 codecs=0 modality=text
IMPORT LLM
SUB Main()
  B$ = "../../samples/Fixtures/models/tiny_f16"
  LLM MODEL INSPECT B$
END SUB
