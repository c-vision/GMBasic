' MiniTest: LLM.STREAMSTATS(<nome>) — funzione che ritorna il JSON delle statistiche di
' streaming/residenza del modello; su modello non caricato → errore CATTURABILE, su modello
' caricato → JSON con ok/mode/residency. Parità 4 backend (il JSON è prodotto dal runtime C).
' EXPECT: ss_before_err|load_ok|ss_ok|ss_mode_swap|ss_res_warm|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  s$ = LLM.STREAMSTATS("m")
  PRINT "ss_before_ok"
  GOTO N1
E1:
  PRINT "ss_before_err"
N1:
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  s$ = LLM.STREAMSTATS("m")
  IF INSTR(s$, "ok") > 0 THEN
    PRINT "ss_ok"
  ELSE
    PRINT "ss_bad"
  END IF
  IF INSTR(s$, "swap") > 0 THEN
    PRINT "ss_mode_swap"
  ELSE
    PRINT "ss_mode_other"
  END IF
  IF INSTR(s$, "warm") > 0 THEN
    PRINT "ss_res_warm"
  ELSE
    PRINT "ss_res_other"
  END IF
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
