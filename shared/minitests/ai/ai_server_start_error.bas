' EXPECT: start_caught
' MiniTest: LLM SERVER START con una model dir INESISTENTE deve fallire in modo catturabile
' (ON ERROR) su TUTTI i backend, senza avviare processi. Il runtime C controlla la dir PRIMA
' del fork (parità col gestore C# AiServerManager.Start). Vedi
' Bugs/(solved) - Semantica-Errore-Builtin-Tra-Backend.md.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main
  ON ERROR GOTO H
  LLM SERVER START "/nonexistent_gmbasic_model_dir_xyz"
  PRINT "start_noerr"
  GOTO D
H:
  PRINT "start_caught"
D:
END SUB
