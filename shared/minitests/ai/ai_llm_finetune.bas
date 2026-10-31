' MiniTest: LLM.Finetune* (stato idle: nessun training in corso) -> sentinelle.
' FinetuneBaseHold/BaseTrain/GreedyAfter = -1 (mai), DemoChanged = 0 (false).
' (Con un training attivo i valori cambierebbero: oracolo valido a stato idle.)
' EXPECT: fbh=-1|fbt=-1|fdc= 0|fga=-1
IMPORT LLM
SUB Main()
  PRINT "fbh="; LLM.FinetuneBaseHold()
  PRINT "fbt="; LLM.FinetuneBaseTrain()
  PRINT "fdc="; LLM.FinetuneDemoChanged()
  PRINT "fga="; LLM.FinetuneGreedyAfter()
END SUB
