' EXPECT: status_caught|model_caught|unload_caught
' MiniTest: i verbi LLM SERVER che FALLISCONO (nessun server avviato) devono essere CATTURABILI
' via ON ERROR su TUTTI i backend (IL/PCode/VM-C/C99Native). Copre STATUS/MODEL/UNLOAD senza
' server. Prima del fix PCodeVM lasciava sfuggire l'eccezione (rc=0) e VM/C la rendeva fatale
' (ON ERROR ignorato). Vedi Bugs/(solved) - Semantica-Errore-Builtin-Tra-Backend.md.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main
  ON ERROR GOTO H1
  LLM SERVER STATUS
  PRINT "status_noerr"
  GOTO A2
H1:
  PRINT "status_caught"
A2:
  ON ERROR GOTO H2
  LLM SERVER MODEL "/"
  PRINT "model_noerr"
  GOTO A3
H2:
  PRINT "model_caught"
A3:
  ON ERROR GOTO H3
  LLM SERVER UNLOAD
  PRINT "unload_noerr"
  GOTO D
H3:
  PRINT "unload_caught"
D:
END SUB
