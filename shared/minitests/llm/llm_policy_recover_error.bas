' EXPECT: === recover:error ===|{"verdict":"ERROR","reason":"gate(init)","gate":{"verdict":"ERROR","reason":"ref model not loadable"}}
' MiniTest: LLM POLICY RECOVER <model>, <dataset>, <ref> — composizione gate->retrain->re-eval (§9.B).
' Error path deterministico: il gate iniziale non trova i modelli -> ERROR, nessun training.
IMPORT LLM
SUB Main()
  PRINT "=== recover:error ==="
  LLM POLICY RECOVER "nope_model", "nope.train.bin", "nope_ref"
END SUB
