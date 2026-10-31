' EXPECT: caught
' MiniTest: LLM MODEL CONVERT con una sorgente inesistente deve fallire in modo CATTURABILE su
' tutti e 5 i backend (verifica del dispatch del comando, senza eseguire una conversione reale).
IMPORT AI WITH ALIAS
IMPORT LLM
SUB Main
  ON ERROR GOTO H
  LLM MODEL CONVERT "/non/esiste/checkpoint", "/tmp/gmbasic-noconv.gguf"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
