' MiniTest: LLM MODEL DEVICE — device di calcolo (AUTO|CPU|METAL); su modello non caricato →
' errore CATTURABILE, su modello caricato → ok. Parità 4 backend.
' EXPECT: device_before_err|load_ok|device_auto_ok|device_cpu_ok|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL DEVICE "m", "AUTO"
  PRINT "device_before_ok"
  GOTO N1
E1:
  PRINT "device_before_err"
N1:
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E2
  LLM MODEL DEVICE "m", "AUTO"
  PRINT "device_auto_ok"
  GOTO N2
E2:
  PRINT "device_auto_err"
N2:
  ON ERROR GOTO E3
  LLM MODEL DEVICE "m", "CPU"
  PRINT "device_cpu_ok"
  GOTO N3
E3:
  PRINT "device_cpu_err"
N3:
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
