' MiniTest: LLM MODEL <verbo> con argomento PATH in VARIABILE stringa (B$) — deve passare il
' VALORE a runtime, non il letterale "B$" (fix parser C: gmb_p_aimodel_term; pari al C#).
' EXPECT: .gmbarch: /Users/gaetano/Documents/Projects/LLMEnv/ABI/llama.gmbarch|arch=llama|structures: norm=rmsnorm, attention=gqa, layer=transformer_layer|recipe=_M boost_fmt=q6_k|roles=12 quant=3 codecs=5 modality=text
IMPORT LLM
SUB Main()
  B$ = "../../samples/Fixtures/models/tiny_llama"
  LLM MODEL INSPECT B$
END SUB
