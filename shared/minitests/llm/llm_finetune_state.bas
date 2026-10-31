' MiniTest: TRAIN FINETUNE — stato iniziale delle 11 funzioni di lettura (nessuna run).
' Copre tutte le funzioni di stato della superficie LoRA (DONE/LOSS/HOLD/BASETRAIN/
' BASEHOLD/GREEDY/GREEDYAFTER/DEMOCHANGED/CKPT/STATUS) senza modello (non-heavy).
' EXPECT: done= 0|loss=-1|hold=-1|basetrain=-1|basehold=-1|greedy=-1|greedyafter=-1|changed= 0|ckpt_empty=-1|status_idle=-1
IMPORT LLM
SUB Main()
  PRINT "done="; LLM.FINETUNEDONE()
  PRINT "loss="; LLM.FINETUNELOSS()
  PRINT "hold="; LLM.FINETUNEHOLD()
  PRINT "basetrain="; LLM.FINETUNEBASETRAIN()
  PRINT "basehold="; LLM.FINETUNEBASEHOLD()
  PRINT "greedy="; LLM.FINETUNEGREEDY()
  PRINT "greedyafter="; LLM.FINETUNEGREEDYAFTER()
  PRINT "changed="; LLM.FINETUNEDEMOCHANGED()
  PRINT "ckpt_empty="; (LEN(LLM.FINETUNECKPT()) = 0)
  PRINT "status_idle="; (LLM.FINETUNESTATUS() = "idle")
END SUB
