' MiniTest: LLM MODEL CACHE/PREFETCH/DEVICE/PROMOTE/DEMOTE/UNLOAD su modello non caricato
' → errore CATTURABILE su tutti i 4 backend (nessuno stub silenzioso).
' EXPECT: cache_err|prefetch_err|device_err|promote_err|demote_err|unload_err|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL CACHE "m", 8
  GOTO N1
E1:
  PRINT "cache_err"
N1:
  ON ERROR GOTO E2
  LLM MODEL PREFETCH "m", 4
  GOTO N2
E2:
  PRINT "prefetch_err"
N2:
  ON ERROR GOTO E3
  LLM MODEL DEVICE "m", "AUTO"
  GOTO N3
E3:
  PRINT "device_err"
N3:
  ON ERROR GOTO E4
  LLM MODEL PROMOTE "m"
  GOTO N4
E4:
  PRINT "promote_err"
N4:
  ON ERROR GOTO E5
  LLM MODEL DEMOTE "m"
  GOTO N5
E5:
  PRINT "demote_err"
N5:
  ON ERROR GOTO E6
  LLM MODEL UNLOAD "m"
  GOTO N6
E6:
  PRINT "unload_err"
N6:
  PRINT "done"
END SUB
