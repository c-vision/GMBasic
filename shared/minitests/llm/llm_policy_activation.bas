' MiniTest: la chiave di policy "ACTIVATION" (B4) è risolta sull'asse activation del QuantIR (I11).
' Parità 4 backend: le righe per-tensore mostrano a=fp8 (RISULTATI identici). nessun EXPECT (oracolo
' cross-leg come llm_quantir_i11).
IMPORT LLM
SUB Main()
  LLM POLICY SET "ACTIVATION", "fp8"
  LLM TENSOR QUANTIR "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
