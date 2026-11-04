' MiniTest: TRAIN LORA APPLY su checkpoint inesistente — stato dopo l'errore (TRY/CATCH).
' Verifica parità post-errore: CKPT preservato ("" se nessuna run), DONE=0, STATUS in errore.
' Non-heavy (nessun modello reale).
' EXPECT: la_caught=1|ckpt_empty=-1|done= 0|status_err=-1
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  TRY
    TRAIN LORA APPLY GMBWORK$ + "/no_ckpt"
    PRINT "la_caught=0"
  CATCH
    PRINT "la_caught=1"
  END TRY
  PRINT "ckpt_empty="; (LEN(LLM.FINETUNECKPT()) = 0)
  PRINT "done="; LLM.FINETUNEDONE()
  PRINT "status_err="; (LEFT$(LLM.FINETUNESTATUS(), 5) = "error")
END SUB
