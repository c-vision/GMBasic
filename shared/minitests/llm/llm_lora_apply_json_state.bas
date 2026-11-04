' MiniTest: TRAIN LORA APPLY con opts JSON-STRINGA — parità parametri 4 backend (TRY/CATCH).
' `"{""model"":…,""dataset"":…}"` deve essere accettata come il dict: l'errore atteso è
' quello nativo (checkpoint/modello inesistente), MAI "opzione 'model' … mancante"
' (che indicherebbe che la stringa JSON non è stata parsata — divergenza storica IL/PCode).
' EXPECT: la2_caught=1|la2_no_opt_err=-1
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  TRY
    TRAIN LORA APPLY "work/no_ckpt", "{""model"":""work/no_model"",""dataset"":""work/no_ds.bin""}"
    PRINT "la2_caught=0"
  CATCH
    PRINT "la2_caught=1"
  END TRY
  PRINT "la2_no_opt_err="; (INSTR(LLM.FINETUNESTATUS(), "opzione 'model'") = 0)
END SUB
