' TAG: expected-error
' MiniTest: AI.DISCOVER su endpoint irraggiungibile -> errore esplicito deterministico
' (Connection refused, nessuna richiesta di rete reale verso l'esterno).
' Escluso dal cross-leg (IsParityExcluded, come da storico): il formato del messaggio
' dipende dallo stack HTTP di ciascuna gamba. Copre il simbolo AI.Discover del census.
' EXPECT: ERROR: Connection refused (127.0.0.1:1)
IMPORT AI WITH ALIAS
SUB Main()
  DIM r = AI.DISCOVER("test", "chat", "http://127.0.0.1:1/v1", "k")
  PRINT "n="; LEN(r)
END SUB