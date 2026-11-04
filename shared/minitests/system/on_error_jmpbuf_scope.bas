' MiniTest (regressione): scope del jmp_buf degli handler ON ERROR su C99Native.
' Prima del fix il C generato dichiarava `jmp_buf __errN` in un blocco che chiudeva subito:
' il puntatore registrato in gmb_err_set_handler dangleava → longjmp su slot riusato
' (SIGSEGV/SIGBUS) o loop di longjmp. Qui: 4 handler ON ERROR GOTO distinti nello stesso
' SUB, con errori che attraversano anche chiamate native profonde (TRAIN LORA APPLY).
' Vedi Bugs/(solved) - C99-ON-ERROR-jmpbuf-Fuori-Scope.md.
' EXPECT: a= 1|b= 2|c= 3|deep= 4
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  step = 0
  ON ERROR GOTO H1
  TRAIN FINETUNE "work/no_model"
  PRINT "no1"
H1:
  step = step + 1
  PRINT "a="; step
  ON ERROR GOTO H2
  TRAIN LORA APPLY "work/no_ckpt"
  PRINT "no2"
H2:
  step = step + 1
  PRINT "b="; step
  ON ERROR GOTO H3
  TRAIN LORA APPLY "work/no_ckpt", "{""model"":""work/no_model"",""dataset"":""work/no_ds.bin""}"
  PRINT "no3"
H3:
  step = step + 1
  PRINT "c="; step
  ON ERROR GOTO H4
  TRAIN FINETUNE "work/no_model", "{""dataset"":""work/no_ds.bin"", ""holdout"":""work/no_hold.bin""}"
  PRINT "no4"
H4:
  step = step + 1
  PRINT "deep="; step
END SUB
