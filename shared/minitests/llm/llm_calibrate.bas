' LLM CALIBRATE — imatrix (importanza attivazioni) da token (B4). Alias → LLM TENSOR CALIBRATE.
' Dataset = token int32 grezzi (.bin). Modello = fixture MoE F16 (tiny_moe). Parità 4 backend.
' (I warning diagnostici "LLM PROFILE:" restano su stderr — non fanno parte dell'output del canale.)
' EXPECT: LLM CALIBRATE: model=../../samples/Fixtures/models/tiny_moe.gguf dataset=../../samples/Fixtures/models/tiny_tokens.bin imatrix=work/calib.imatrix.gguf tokens=16
IMPORT LLM
SUB Main()
  LLM CALIBRATE "../../samples/Fixtures/models/tiny_moe.gguf", "../../samples/Fixtures/models/tiny_tokens.bin", "work/calib.imatrix.gguf"
END SUB
