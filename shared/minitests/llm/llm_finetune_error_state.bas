' MiniTest: TRAIN FINETUNE su modello inesistente — stato dopo l'errore (TRY/CATCH).
' Verifica la parità post-errore: CKPT vuoto (nessun checkpoint salvato), DONE=0,
' misure a -1, STATUS in errore. Non-heavy (nessun modello reale).
' EXPECT: ft_caught=1|ckpt_empty=-1|done= 0|loss=-1|status_err=-1
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  TRY
    TRAIN FINETUNE GMBWORK$ + "/no_model"
    PRINT "ft_caught=0"
  CATCH
    PRINT "ft_caught=1"
  END TRY
  PRINT "ckpt_empty="; (LEN(LLM.FINETUNECKPT()) = 0)
  PRINT "done="; LLM.FINETUNEDONE()
  PRINT "loss="; LLM.FINETUNELOSS()
  PRINT "status_err="; (LEFT$(LLM.FINETUNESTATUS(), 5) = "error")
END SUB
