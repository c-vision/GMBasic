' MiniTest: QuantIR I11 — la policy a 4 assi (w/a/acc/o) è risolta per-tensore sul CALCOLO.
' Parità 4 backend (RISULTATI identici); oracolo: le righe ATTENTION mostrano a=fp8 acc=f16 *.
IMPORT LLM
SUB Main()
  LLM POLICY DEFINE "i11", "ROLE=ATTENTION:w=q4_k,a=fp8,acc=f16,o=f16"
  LLM POLICY APPLY "i11"
  LLM TENSOR QUANTIR "../../samples/Fixtures/models/tiny_moe.gguf"
END SUB
