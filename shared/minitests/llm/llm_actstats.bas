' LLM ACTIVATIONSTATS — statistiche di attivazione (routing) dal dataset (B4). Alias → LLM TENSOR.
' Modello = fixture MoE F16 (tiny_moe); dataset = token int32 (.bin). Parità 4 backend.
' EXPECT: LLM TENSOR ACTIVATIONSTATS '../../samples/Fixtures/models/tiny_moe.gguf': layers=2 experts=2 tokens=16 active=1
IMPORT LLM
SUB Main()
  LLM ACTIVATIONSTATS "../../samples/Fixtures/models/tiny_moe.gguf", "../../samples/Fixtures/models/tiny_tokens.bin"
END SUB
