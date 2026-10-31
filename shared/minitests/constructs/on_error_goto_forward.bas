IMPORT AI WITH ALIAS
IMPORT LLM
' ON ERROR GOTO verso un'etichetta in AVANTI (opcode a indirizzo assoluto OP_SET_ERR_HANDLER_GOTO): l'handler deve essere raggiunto.
' EXPECT: caught
SUB Main
  ON ERROR GOTO H
  LLM MODEL CONVERT "/non/esiste/checkpoint", "/tmp/gmbasic-noconv.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
