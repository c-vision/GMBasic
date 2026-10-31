' EXPECT: === recover:args ===|{"verdict":"ERROR","reason":"attesi <model>, <dataset>, <ref>"}
' MiniTest: LLM POLICY RECOVER — argomenti mancanti -> JSON deterministico (parità 4 backend).
' Verifica il gate di AMMISSIONE dei parametri del comando (nessuna eccezione backend-specifica).
IMPORT LLM
SUB Main()
  PRINT "=== recover:args ==="
  LLM POLICY RECOVER "solo_model", "solo_dataset"
END SUB
