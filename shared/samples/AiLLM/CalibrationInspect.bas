' CalibrationInspect.bas — calibrazione / quant-error (B4).
' Esercita: LLM QUANTERROR (errore di ricostruzione ref vs cand), chiave policy ACTIVATION (asse
' activation del QuantIR). LLM CALIBRATE / LLM ACTIVATIONSTATS richiedono un MoE reale (heavy).
IMPORT LLM
SUB Main()
  PRINT "=== 1. QUANTERROR — ref vs se stesso (errore 0) ==="
  LLM QUANTERROR "../Fixtures/models/tiny_moe.gguf", "../Fixtures/models/tiny_moe.gguf"

  PRINT "=== 2. POLICY SET ACTIVATION (chiave di 1a classe -> asse a=QuantIR) ==="
  LLM POLICY SET "ACTIVATION", "fp8"
  LLM POLICY SET "ATTENTION", "q8_0"

  PRINT "=== 3. QUANTIR — mostra l'asse activation risolto (a=fp8) ==="
  LLM TENSOR QUANTIR "../Fixtures/models/tiny_moe.gguf"
END SUB
