' MiniTest: AI EVAL VALIDATE (T4.1) — UAT via vault (oracolo a termini).
' EXPECT: ok:{"ok":true,"passed":["- Il sistema deve supportare il login utente","- Deve calcolare il totale degli ordini"],"failed":[],"coverage":1}|bad:{"ok":false,"passed":["- Deve calcolare il totale degli ordini"],"failed":["- Il sistema deve supportare il login utente"],"coverage":0.5}|done
IMPORT AI

SUB Main
  DIM j AS STRING
  AI EVAL VALIDATE "il sistema supportare il login utente e deve calcolare il totale degli ordini" AGAINST VAULT "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/UatVault" INTO j
  PRINT "ok:"; j
  AI EVAL VALIDATE "il componente calcola il totale degli ordini ma manca il flusso login" AGAINST VAULT "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/UatVault" INTO j
  PRINT "bad:"; j
  PRINT "done"
END SUB
