' EXPECT: a|b
' MiniTest: LLM SERVER STOP idempotente (nessun server avviato) — deve essere un no-op
' silenzioso e non bloccare il programma. Copre il comando LLM SERVER <STOP> su tutti i
' backend senza richiedere un modello/server. Vedi samples/Ai/AiServerDemo.bas
' per il ciclo completo (START/STATUS/MODEL/UNLOAD/STOP) con un modello reale.
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main
  PRINT "a"
  LLM SERVER STOP
  LLM SERVER STOP
  PRINT "b"
END SUB
